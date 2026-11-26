// Build/run with tests/Run-UiTests.ps1. All writes stay in a temporary directory.
#include "integration/ui.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

struct CommunityUiTestAccess {
    using Page = CommunityUI::Page;
    using Action = CommunityUI::Action;
    static constexpr int pageCount = static_cast<int>(Page::Repairs) + 1;
    static void frame(CommunityUI& ui) {
        ImGui::NewFrame();
        ui.render();
        ImGui::Render();
        check(ImGui::GetDrawData() != nullptr, "No draw data");
    }
    static void success(CommunityUI& ui) {
        check(!ui.state_.error, ui.state_.message.c_str());
    }
    static void create(CommunityUI& ui, Page page,
                       std::initializer_list<std::string> fields) {
        ui.changePage(page);
        std::copy(fields.begin(), fields.end(), ui.state_.form.begin());
        ui.submit(Action::Create);
        success(ui);
    }
    static void exerciseRepairs(CommunityUI& ui, const std::filesystem::path& directory,
                                const OwnerManagementSystem& owners) {
        const auto loginAs = [&](const char* username, const char* password) {
            ui.logout();
            ui.state_.username = username;
            ui.state_.password = password;
            check(ui.login(), "Repair test login failed");
        };
        const std::string description = "厨房水管漏水，位置在\"水槽下\"。\n请检查接头。";
        const std::string note = "已更换接头。\n试水正常。";

        loginAs("owner1", "pw1");
        check(ui.allowed(Page::Repairs) && !ui.allowed(Page::Tasks), "Owner repair permissions wrong");
        ui.changePage(Page::Repairs);
        ui.state_.form = {"  \t", description};
        ui.submit(Action::Create);
        check(ui.state_.error && ui.rows().empty(), "Blank repair title accepted");
        create(ui, Page::Repairs, {"厨房漏水", description});
        const std::string repairId = ui.state_.selected;
        check(repairId.rfind("R-", 0) == 0, "No automatic repair ID");
        const auto newRepair = ui.rows();
        check(newRepair.size() == 1 && newRepair[0].cells[2] == "未分配" &&
              newRepair[0].cells[3] == "O0001" && !newRepair[0].cells[4].empty(), "Bad new repair data");
        ui.submit(Action::ConfirmRepair);
        check(ui.state_.error, "Unprocessed repair could be confirmed");
        ui.state_.form[5] = "repairs";
        ui.submit(Action::AssignTask);
        check(ui.state_.error, "Owner was able to assign a repair");
        frame(ui);

        loginAs("owner2", "pw2");
        ui.changePage(Page::Repairs);
        check(ui.rows().empty(), "Other owner can see a repair");
        create(ui, Page::Repairs, {"楼道灯损坏", "二楼灯不亮"});
        check(ui.state_.selected != repairId && ui.rows().size() == 1, "Repairs not isolated per owner");

        loginAs("access", "pw");
        check(!ui.allowed(Page::Repairs), "Access staff can view repairs");
        loginAs("repairs", "pw");
        ui.changePage(Page::Repairs);
        check(ui.rows().size() == 2 && !ui.canCreate(), "Property staff repair list or permission is wrong");
        ui.state_.selected = repairId;
        ui.submit(Action::AdvanceTask);
        check(ui.state_.error, "Unassigned repair could be started");
        ui.state_.form[5] = "repairs";
        ui.submit(Action::AssignTask); success(ui);
        ui.submit(Action::AdvanceTask); success(ui);
        frame(ui);
        ui.submit(Action::AdvanceTask);
        check(ui.state_.error, "Repair could bypass submission of a processing result");
        ui.state_.form[6] = " \t\n";
        ui.submit(Action::FinishRepair);
        check(ui.state_.error, "Blank repair result accepted");

        const auto taskFile = directory / "property_tasks.csv";
        const HANDLE lockedFile = CreateFileW(taskFile.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        check(lockedFile != INVALID_HANDLE_VALUE, "Could not lock task CSV for failure test");
        ui.state_.form[6] = note;
        ui.submit(Action::FinishRepair);
        CloseHandle(lockedFile);
        check(ui.state_.error && ui.rows()[0].cells[2] == "进行中", "Failed save changed repair status");

        ui.submit(Action::FinishRepair); success(ui);
        check(ui.rows()[0].cells[2] == "待业主确认" && ui.rows()[0].cells[7] == note,
            "Repair result missing from property view");
        ui.submit(Action::ConfirmRepair);
        check(ui.state_.error, "Property staff confirmed an owner's repair");

        PropertyManagementSystem restarted(owners, directory.string());
        const auto firstOwner = owners.findOwnerById("O0001");
        check(firstOwner.has_value(), "Missing repair fixture owner");
        const auto reopened = restarted.queryRepairs(*firstOwner);
        check(reopened.size() == 1 && reopened[0].status == TaskStatus::AwaitingConfirmation &&
              community_ui::toUtf8(reopened[0].description) == description &&
              community_ui::toUtf8(reopened[0].completionNote) == note, "Pending repair did not survive reload");
        check(!restarted.updateTaskStatus(repairId, TaskStatus::Completed),
            "Generic status API can bypass owner confirmation");
        const Owner reusedId(firstOwner->id(), "", "", "", "", "", "replacement", "pw");
        check(restarted.queryRepairs(reusedId).empty() && !restarted.confirmRepair(repairId, reusedId),
            "Owner number alone granted access to a historical repair");

        loginAs("owner2", "pw2");
        ui.changePage(Page::Repairs);
        ui.state_.selected = repairId;
        ui.submit(Action::ConfirmRepair);
        check(ui.state_.error, "Other owner confirmed a repair by guessing the ID");
        loginAs("owner1", "pw1");
        ui.changePage(Page::Repairs);
        ui.state_.selected = repairId;
        frame(ui);
        ui.submit(Action::ConfirmRepair); success(ui);
        check(ui.rows()[0].cells[2] == "已完成", "Owner confirmation was not displayed");
        restarted.load();
        const auto completed = restarted.queryRepairs(*firstOwner);
        check(completed.size() == 1 && completed[0].status == TaskStatus::Completed,
            "Repair completion did not survive reload");

        const auto legacyDir = directory.parent_path() / "legacy";
        std::filesystem::create_directories(legacyDir);
        std::ofstream legacy(legacyDir / "property_tasks.csv");
        legacy << "id,title,description,type,created,assignee,status,note\n"
                  "T001,old,\"line 1\nline 2\",repair,2026.11.01,property,Completed,done\n";
        legacy.close();
        PropertyManagementSystem upgraded(owners, legacyDir.string());
        check(upgraded.submitRepair(*firstOwner, "test", "test description").has_value(),
            "Could not append a repair to the legacy task file");
        upgraded.load();
        const auto upgradedTasks = upgraded.queryTasks();
        check(upgradedTasks.size() == 2 && !upgradedTasks[0].isRepair() &&
              upgradedTasks[0].status == TaskStatus::Completed && upgradedTasks[0].description == "line 1\nline 2",
              "Legacy internal task was lost or changed during upgrade");
        OwnerManagementSystem replacementOwners((legacyDir / "owners.csv").string());
        const auto replacementId = replacementOwners.addOwner(reusedId);
        check(replacementId == firstOwner->id(), "Replacement owner did not reuse the same ID");
        PropertyManagementSystem replacementProperty(replacementOwners, legacyDir.string());
        check(replacementProperty.queryRepairs(reusedId).empty(), "Replacement owner can see historical repair");
    }
    static void run(const std::filesystem::path& directory) {
        IntegrationSystem system(directory.string());
        CommunityUI ui(system);
        check(!ui.allowed(Page::Owners), "Anonymous user can view owners");
        ui.state_.username = "admin"; ui.state_.password = "wrong";
        check(!ui.login(), "Wrong password accepted");
        check(ui.state_.password.empty(), "Failed-login password retained");
        ui.state_.password = "admin123";
        check(ui.login(), "Admin login failed");
        for (int i = 0; i < pageCount; ++i) check(ui.allowed(static_cast<Page>(i)), "Missing admin page");

        create(ui, Page::Owners, {"测试业主甲", "13800138000", "1", "1", "101", "owner1", "pw1"});
        check(ui.state_.selected == "O0001", "Owner ID is not automatic");
        check(ui.rows()[0].cells[7] == "待审核", "New owner is not pending");
        ui.logout();
        ui.state_.username = "owner1"; ui.state_.password = "pw1";
        check(!ui.login(), "Pending owner can sign in");
        ui.state_.username = "admin"; ui.state_.password = "admin123";
        check(ui.login(), "Admin re-login failed");
        ui.changePage(Page::Owners); ui.state_.selected = "O0001";
        ui.submit(Action::ApproveOwner); success(ui);
        check(ui.rows()[0].cells[7] == "正常入住", "Approved owner is not active");
        ui.submit(Action::ApproveOwner);
        check(ui.state_.error, "Active owner was approved twice");
        create(ui, Page::Owners, {"测试业主乙", "13800138001", "1", "1", "102", "owner2", "pw2"});
        check(ui.state_.selected == "O0002", "Owner sequence failed");
        ui.submit(Action::ApproveOwner); success(ui);
        create(ui, Page::Points, {"小区南门", "CommunityGate", "南侧"});
        check(ui.state_.selected == "AP0001", "Gate ID failed");
        create(ui, Page::Points, {"一栋单元门", "BuildingGate", "一栋"});
        create(ui, Page::Accounts, {"property", "pw", "物业人员", "PropertyStaff"});
        create(ui, Page::Accounts, {"access", "pw", "门禁人员", "AccessStaff"});
        create(ui, Page::Fees, {"O0001", "F001", "2026-11", "120", "80", "2026.11.09"});
        ui.state_.queryOwner = "O0001";
        check(ui.rows().size() == 1 && ui.rows()[0].cells[4] == "40.00", "Payment query failed");
        ui.state_.form = {"O0001", "F001", "2026-11", "nan", "0", "2026.11.09"};
        ui.submit(Action::Create);
        check(ui.state_.error && ui.rows().size() == 1, "NaN payment accepted");

        create(ui, Page::Tasks, {"T001", "水管维修", "检查漏水", "维修", "2026.11.09"});
        check(ui.rows()[0].cells[5] == "未分配", "Unassigned task label missing");
        const auto staff = ui.staffChoices();
        check(std::any_of(staff.begin(), staff.end(), [](const auto& item) {
            return item.first == "property" && item.second == "物业人员";
        }), "Assignee options must show names and retain account keys");
        ui.state_.selected = "T001"; ui.state_.form[5] = "missing-account";
        ui.submit(Action::AssignTask);
        check(ui.state_.error, "Unknown assignee accepted");
        ui.state_.form[5] = "property";
        ui.submit(Action::AssignTask); success(ui);
        check(ui.rows()[0].cells[5] == "物业人员", "Task displays account instead of name");
        ui.state_.search = "物业人员";
        check(ui.rows().size() == 1, "Task cannot be searched by assignee name");
        ui.state_.search.clear();
        check(ui.assigneeName("missing-account") == "未找到工作人员（missing-account）",
            "Historical missing assignee is not identified");
        ui.submit(Action::AdvanceTask); success(ui);
        ui.submit(Action::AdvanceTask); success(ui);
        check(ui.rows()[0].cells[6] == "已完成", "Task lifecycle failed");

        create(ui, Page::People, {"O0001", "Enter", "回家", "AP0001"});
        check(ui.state_.selected.rfind("AR-", 0) == 0, "Person ID missing");
        create(ui, Page::Vehicles, {"京A12345", "O0001", "Enter", "驶入", "AP0001"});
        check(ui.state_.selected.rfind("VR-", 0) == 0, "Vehicle ID missing");
        create(ui, Page::Vehicles, {"京B12345", "O0002", "Enter", "驶入", "AP0001"});
        create(ui, Page::Parking, {"京A12345", "O0001", "2026.11.09"});
        check(ui.state_.selected == "PK-京A12345", "Chinese parking ID failed");
        ui.submit(Action::ReleaseParking); success(ui);
        create(ui, Page::Parking, {"京A12345", "O0001", "2026.11.10"});
        check(ui.state_.selected == "PK-京A12345", "Parking ID not reused");
        create(ui, Page::Visitors, {"来访人员", "13900139000", "O0001", "探亲", "AP0001"});
        const std::string visitorId = ui.state_.selected;
        check(visitorId.find("-visitor") != std::string::npos, "Visitor ID changed");
        ui.state_.form[5] = "AP0001";
        ui.submit(Action::CheckoutVisitor); success(ui);
        check(ui.rows().empty(), "Checked-out visitor remains in visiting list");
        ui.state_.visitingOnly = false;
        check(ui.rows().size() == 1 && ui.rows()[0].cells[7] == "已签离", "Visitor archive lost");
        ui.changePage(Page::People); ui.state_.query = visitorId;
        check(ui.rows().size() == 2, "Visitor enter/leave events missing");

        // Render every admin page with real table/form data, catching ImGui assertions.
        for (int i = 0; i < pageCount; ++i) {
            ui.changePage(static_cast<Page>(i));
            frame(ui); frame(ui);
        }
        auto* actions = ImGui::FindWindowByName("操作面板###Actions");
        check(actions && actions->DockId != 0, "Actions panel is not docked");
        ImGui::SetWindowDock(actions, 0, ImGuiCond_Always);
        frame(ui);
        check(actions->DockId == 0, "Undocked panel was forcibly docked again");
        ui.state_.resetLayout = true;
        frame(ui); frame(ui);
        check(actions->DockId != 0, "Default layout restore failed");
        std::size_t iniSize = 0;
        const std::string ini = ImGui::SaveIniSettingsToMemory(&iniSize);
        check(ini.find("[Docking][Data]") != std::string::npos, "Dock layout not serializable");
        ImGui::LoadIniSettingsFromMemory(ini.data(), iniSize);
        frame(ui);
        ui.changePage(Page::Vehicles); ui.state_.query = "😀";
        frame(ui); // Invalid business encoding must not leave the ImGui stack unbalanced.
        frame(ui);
        ui.changePage(Page::Points); ui.state_.selected = "AP0002";
        ui.submit(Action::DisablePoint); success(ui);

        const auto loginAs = [&](const char* username, const char* password) {
            ui.state_.search = "secret"; ui.state_.selected = "secret";
            ui.state_.form[0] = "secret"; ui.logout();
            check(ui.state_.search.empty() && ui.state_.selected.empty() &&
                ui.state_.form[0].empty() && ui.state_.query.empty(), "Logout leaks state");
            ui.state_.username = username; ui.state_.password = password;
            check(ui.login(), "Role login failed");
        };
        loginAs("property", "pw");
        check(ui.allowed(Page::Fees) && ui.allowed(Page::Tasks) &&
            !ui.allowed(Page::Vehicles) && !ui.allowed(Page::Accounts), "Property permissions wrong");
        for (int i = 0; i < pageCount; ++i) {
            auto page = static_cast<Page>(i);
            if (ui.allowed(page)) { ui.changePage(page); frame(ui); }
        }
        ui.state_.page = Page::Accounts; ui.submit(Action::Create);
        check(ui.state_.error, "Property staff can create staff");
        ui.state_.selected = "access";
        ui.submit(Action::DeleteStaff);
        check(ui.state_.error, "Property staff can delete staff");
        loginAs("access", "pw");
        check(ui.allowed(Page::Vehicles) && !ui.allowed(Page::Fees), "Access permissions wrong");
        for (int i = 0; i < pageCount; ++i) {
            auto page = static_cast<Page>(i);
            if (ui.allowed(page)) { ui.changePage(page); frame(ui); }
        }
        loginAs("owner1", "pw1");
        check(ui.allowed(Page::Owners) && !ui.allowed(Page::Visitors) &&
            !ui.allowed(Page::Accounts), "Owner permissions wrong");
        ui.changePage(Page::Owners);
        check(ui.rows().size() == 1 && ui.rows()[0].key == "O0001", "Owner sees other owner data");
        ui.submit(Action::Create); check(ui.state_.error, "Owner can add records");
        ui.changePage(Page::Vehicles); ui.state_.query = "京B12345";
        check(ui.rows().empty(), "Owner can view other owner vehicles");
        ui.state_.query = "京A12345";
        check(ui.rows().size() == 1, "Owner vehicle query failed");
        for (int i = 0; i < pageCount; ++i) {
            auto page = static_cast<Page>(i);
            if (ui.allowed(page)) { ui.changePage(page); frame(ui); }
        }
        loginAs("admin", "admin123");
        create(ui, Page::Owners, {"临时业主", "", "", "", "", "temporary", "pw"});
        ui.submit(Action::MoveOutOwner);
        check(ui.state_.error, "Pending owner skipped approval and moved out");
        ui.submit(Action::ApproveOwner); success(ui);
        ui.submit(Action::MoveOutOwner); success(ui);
        check(ui.rows().size() == 3 && ui.rows()[2].cells[7] == "已迁出",
              "Moved-out owner archive was not retained");
        // Reject unsupported Unicode rather than silently replacing it with '?'.
        ui.state_.form[0] = "😀"; ui.state_.form[5] = "emoji"; ui.state_.form[6] = "pw";
        ui.submit(Action::Create); check(ui.state_.error, "Lossy encoding accepted");

        OwnerManagementSystem reloaded((directory / "owners.csv").string());
        check(reloaded.all().size() == 3, "Owner persistence failed");
        check(community_ui::toUtf8(reloaded.all()[0].name()) == "测试业主甲", "Chinese name corrupted");
        check(reloaded.all()[0].status() == OwnerStatus::Active &&
              reloaded.all()[2].status() == OwnerStatus::MovedOut,
              "Owner status persistence failed");
        const auto legacyOwnerFile = directory / "legacy_owners.csv";
        std::ofstream legacyOwnersOutput(legacyOwnerFile);
        legacyOwnersOutput << "id,name,phone,building,unit,room,username,password\n"
                              "O0099,Legacy,10086,9,1,901,legacy,pw\n";
        legacyOwnersOutput.close();
        OwnerManagementSystem legacyOwners(legacyOwnerFile.string());
        check(legacyOwners.all().size() == 1 &&
              legacyOwners.all()[0].status() == OwnerStatus::Active,
              "Legacy owner CSV is not treated as active");
        AccountService ownerAccounts((directory / "accounts.csv").string());
        ownerAccounts.load(reloaded);
        std::shared_ptr<User> ownerAccount;
        check(!ownerAccounts.authenticate("temporary", "pw", reloaded, ownerAccount),
              "Moved-out owner can sign in");
        AccessManagementSystem access(reloaded, directory.string());
        AccessRecord movedOutAccess;
        movedOutAccess.personId = "O0003";
        movedOutAccess.personType = "Owner";
        movedOutAccess.direction = "Enter";
        movedOutAccess.gateId = "AP0001";
        check(!access.registerPerson(movedOutAccess).has_value(),
              "Moved-out owner can create new access records");
        const auto vehicles = access.queryVehicle(community_ui::toBusiness("京A12345"));
        check(vehicles.size() == 1 && !vehicles[0].time.empty(), "Automatic time not persisted");
        check(vehicles[0].id.substr(3, 14) ==
            vehicles[0].time.substr(0, 4) + vehicles[0].time.substr(5, 2) +
            vehicles[0].time.substr(8, 2) + vehicles[0].time.substr(11, 2) +
            vehicles[0].time.substr(14, 2) + vehicles[0].time.substr(17, 2), "ID and time disagree");
        check(community_ui::toUtf8(access.parking()[0].spaceId) == "PK-京A12345", "Chinese plate corrupted");
        PropertyManagementSystem property(reloaded, directory.string());
        FeeRecord movedOutFee;
        movedOutFee.ownerId = "O0003";
        movedOutFee.itemId = "F001";
        movedOutFee.month = "2026-12";
        movedOutFee.due = 100;
        check(!property.recordPayment(movedOutFee),
              "Moved-out owner can create new fee records");
        check(property.outstanding("O0001") == 40, "Outstanding persistence failed");
        check(property.queryTasks()[0].status == TaskStatus::Completed, "Task persistence failed");
        check(property.queryTasks()[0].assignee == "property", "Assignee account association changed");
        create(ui, Page::Accounts, {"property2", "pw", "物业人员", "PropertyStaff"});
        const auto duplicateStaff = ui.staffChoices();
        check(std::any_of(duplicateStaff.begin(), duplicateStaff.end(), [](const auto& item) {
            return item.first == "property2" && item.second == "物业人员（property2）";
        }), "Duplicate staff names are ambiguous");

        create(ui, Page::Accounts, {"admin2", "pw", "管理员乙", "Admin"});
        loginAs("admin2", "pw");
        ui.changePage(Page::Accounts);
        ui.state_.selected = "admin2";
        ui.submit(Action::DeleteStaff);
        check(ui.state_.error, "Signed-in administrator deleted their own account");
        ui.state_.selected = "admin";
        ui.submit(Action::DeleteStaff);
        check(ui.state_.error, "Built-in administrator account was deleted");
        ui.state_.selected = "missing-account";
        ui.submit(Action::DeleteStaff);
        check(ui.state_.error, "Deleting a missing staff account succeeded");

        ui.state_.selected = "property";
        frame(ui);
        const auto accountFile = directory / "accounts.csv";
        // Deny file replacement to exercise the save failure path and in-memory rollback.
        const HANDLE lockedFile = CreateFileW(accountFile.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        check(lockedFile != INVALID_HANDLE_VALUE, "Could not lock account CSV for failure test");
        ui.submit(Action::DeleteStaff);
        CloseHandle(lockedFile);
        const auto rowsAfterFailedRemoval = ui.rows();
        check(ui.state_.error && std::any_of(rowsAfterFailedRemoval.begin(), rowsAfterFailedRemoval.end(),
            [](const auto& row) { return row.key == "property"; }), "Save failure removed staff from memory");
        AccountService persistedAccounts(accountFile.string());
        persistedAccounts.load(reloaded);
        std::shared_ptr<User> account;
        check(persistedAccounts.authenticate("property", "pw", reloaded, account),
            "Save failure removed staff from disk");
        check(persistedAccounts.removeStaff("access", *account) == StaffRemovalResult::Forbidden,
            "Account service accepted staff removal by a non-admin");

        ui.state_.form[0] = "😀"; // An unrelated draft entry must not prevent deletion.
        ui.submit(Action::DeleteStaff);
        success(ui);
        const auto remainingStaff = ui.rows();
        check(ui.state_.selected.empty() && std::none_of(remainingStaff.begin(), remainingStaff.end(),
            [](const auto& row) { return row.key == "property"; }), "Deleted staff remains in account list");
        ui.changePage(Page::Tasks);
        check(ui.rows()[0].cells[5] == "未找到工作人员（property）", "Task lost its historical assignee");
        const auto remainingChoices = ui.staffChoices();
        check(std::none_of(remainingChoices.begin(), remainingChoices.end(),
            [](const auto& item) { return item.first == "property"; }), "Deleted staff remains assignable");

        ui.logout();
        ui.state_.username = "property";
        ui.state_.password = "pw";
        check(!ui.login(), "Deleted staff can still sign in");
        persistedAccounts.load(reloaded);
        check(!persistedAccounts.authenticate("property", "pw", reloaded, account),
            "Deleted staff can sign in after reload");
        check(persistedAccounts.authenticate("property2", "pw", reloaded, account),
            "Deleting staff removed a different user with the same name");
        check(persistedAccounts.authenticate("admin", "admin123", reloaded, account),
            "Built-in administrator cannot sign in after reload");
        check(persistedAccounts.authenticate("admin2", "pw", reloaded, account),
            "Protected administrator cannot sign in after reload");
        loginAs("admin", "admin123");
        create(ui, Page::Accounts, {"repairs", "pw", "维修人员", "PropertyStaff"});
        exerciseRepairs(ui, directory, reloaded);
        ui.logout(); frame(ui);
    }
};

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("community-ui-tests-" + std::to_string(GetCurrentProcessId()));
    try {
        std::filesystem::create_directories(root / "data");
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(1440, 900);
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.Fonts->AddFontDefaultBitmap();
        io.Fonts->Build();
        CommunityUiTestAccess::run(root / "data");
        const auto located = community_ui::findDataDirectory(root / "build" / "x64");
        check(located == root / "data", "Executable-relative data resolution failed");
        check(community_ui::findDataDirectory(root / "missing") == root / "data", "Parent data lookup failed");
        check(community_ui::findDataDirectory(root / "data" / "bin") == root / "data", "Nested output lookup failed");
        ImGui::DestroyContext();
        std::filesystem::remove_all(root);
        std::cout << "PASS: UI workflows, owner repairs, four-role permissions, persistence, encoding, and rendering.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\nData: " << root << '\n';
        return 1;
    }
}
