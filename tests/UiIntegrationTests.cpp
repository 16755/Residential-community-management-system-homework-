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
    static void run(const std::filesystem::path& directory) {
        IntegrationSystem system(directory.string());
        CommunityUI ui(system);
        check(!ui.allowed(Page::Owners), "Anonymous user can view owners");
        ui.state_.username = "admin"; ui.state_.password = "wrong";
        check(!ui.login(), "Wrong password accepted");
        check(ui.state_.password.empty(), "Failed-login password retained");
        ui.state_.password = "admin123";
        check(ui.login(), "Admin login failed");
        for (int i = 0; i < 9; ++i) check(ui.allowed(static_cast<Page>(i)), "Missing admin page");

        create(ui, Page::Owners, {"测试业主甲", "13800138000", "1", "1", "101", "owner1", "pw1"});
        check(ui.state_.selected == "O0001", "Owner ID is not automatic");
        create(ui, Page::Owners, {"测试业主乙", "13800138001", "1", "1", "102", "owner2", "pw2"});
        check(ui.state_.selected == "O0002", "Owner sequence failed");
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
        for (int i = 0; i < 9; ++i) {
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
        for (int i = 0; i < 9; ++i) {
            auto page = static_cast<Page>(i);
            if (ui.allowed(page)) { ui.changePage(page); frame(ui); }
        }
        ui.state_.page = Page::Accounts; ui.submit(Action::Create);
        check(ui.state_.error, "Property staff can create staff");
        loginAs("access", "pw");
        check(ui.allowed(Page::Vehicles) && !ui.allowed(Page::Fees), "Access permissions wrong");
        for (int i = 0; i < 9; ++i) {
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
        for (int i = 0; i < 9; ++i) {
            auto page = static_cast<Page>(i);
            if (ui.allowed(page)) { ui.changePage(page); frame(ui); }
        }
        loginAs("admin", "admin123");
        create(ui, Page::Owners, {"临时业主", "", "", "", "", "temporary", "pw"});
        ui.submit(Action::DeleteOwner); success(ui);
        check(ui.rows().size() == 2, "Owner delete failed");
        // Reject unsupported Unicode rather than silently replacing it with '?'.
        ui.state_.form[0] = "😀"; ui.state_.form[5] = "emoji"; ui.state_.form[6] = "pw";
        ui.submit(Action::Create); check(ui.state_.error, "Lossy encoding accepted");

        OwnerManagementSystem reloaded((directory / "owners.csv").string());
        check(reloaded.all().size() == 2, "Owner persistence failed");
        check(community_ui::toUtf8(reloaded.all()[0].name()) == "测试业主甲", "Chinese name corrupted");
        AccessManagementSystem access(reloaded, directory.string());
        const auto vehicles = access.queryVehicle(community_ui::toBusiness("京A12345"));
        check(vehicles.size() == 1 && !vehicles[0].time.empty(), "Automatic time not persisted");
        check(vehicles[0].id.substr(3, 14) ==
            vehicles[0].time.substr(0, 4) + vehicles[0].time.substr(5, 2) +
            vehicles[0].time.substr(8, 2) + vehicles[0].time.substr(11, 2) +
            vehicles[0].time.substr(14, 2) + vehicles[0].time.substr(17, 2), "ID and time disagree");
        check(community_ui::toUtf8(access.parking()[0].spaceId) == "PK-京A12345", "Chinese plate corrupted");
        PropertyManagementSystem property(reloaded, directory.string());
        check(property.outstanding("O0001") == 40, "Outstanding persistence failed");
        check(property.queryTasks()[0].status == TaskStatus::Completed, "Task persistence failed");
        check(property.queryTasks()[0].assignee == "property", "Assignee account association changed");
        create(ui, Page::Accounts, {"property2", "pw", "物业人员", "PropertyStaff"});
        const auto duplicateStaff = ui.staffChoices();
        check(std::any_of(duplicateStaff.begin(), duplicateStaff.end(), [](const auto& item) {
            return item.first == "property2" && item.second == "物业人员（property2）";
        }), "Duplicate staff names are ambiguous");
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
        std::cout << "PASS: UI workflows, four-role permissions, persistence, encoding, and rendering.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\nData: " << root << '\n';
        return 1;
    }
}
