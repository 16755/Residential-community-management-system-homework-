// ============================================================================
// 文件：ui.cpp
// 职责：按窗口组织小区管理系统界面；业务操作统一交给现有子系统处理。
// 阅读顺序：公共控件 → 状态及表格数据 → 操作提交 → 布局及各窗口绘制。
// ============================================================================
#include "integration/ui.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <stdexcept>
#include <utility>

using community_ui::toBusiness;
using community_ui::toUtf8;

namespace {
// 公共控件：自动配对窗口结束调用，即使提前返回或抛异常也执行 End。
struct EndWindow {
    ~EndWindow() {
        ImGui::End();
    }
};
constexpr const char* pageNames[] = {"业主资料",
                                     "物业缴费",
                                     "物业任务",
                                     "人员出入",
                                     "车辆出入",
                                     "车位管理",
                                     "访客管理",
                                     "门禁点",
                                     "工作人员账号"};
constexpr const char* navIds[] = {"01", "02", "03", "04", "05", "06", "07", "08", "09"};
constexpr const char* taskNames[] = {"未分配", "已分配", "进行中", "已完成"};

// 为新增表单预填日期或时间；仅格式化显示，不负责生成业务编号。
std::string today(const char* format = "%Y.%m.%d") {
    const auto now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), format, &local);
    return buffer;
}

const char* roleName(Role role) {
    switch (role) {
        case Role::Owner:
            return "业主";
        case Role::PropertyStaff:
            return "物业人员";
        case Role::AccessStaff:
            return "门禁人员";
        default:
            return "管理员";
    }
}

// 将业务中保存的英文状态转换为表格和详情中展示的中文。
std::string translated(const std::string& value) {
    if (value == "Enter") {
        return "进入";
    }
    if (value == "Leave") {
        return "离开";
    }
    if (value == "Owner") {
        return "业主";
    }
    if (value == "Visitor") {
        return "访客";
    }
    if (value == "Occupied") {
        return "占用中";
    }
    if (value == "Released") {
        return "已释放";
    }
    if (value == "Visiting") {
        return "在访";
    }
    if (value == "Left") {
        return "已签离";
    }
    if (value == "Enabled") {
        return "启用";
    }
    if (value == "Disabled") {
        return "停用";
    }
    if (value == "CommunityGate") {
        return "小区大门";
    }
    if (value == "GarageGate") {
        return "车库大门";
    }
    if (value == "BuildingGate") {
        return "楼栋门";
    }
    return toUtf8(value);
}

std::string money(double value) {
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    return buffer;
}

// 控件尺寸跟随当前窗口 DPI，按钮高度至少容纳一行完整文字。
float scaled(float value) {
    return value * ImGui::GetStyle().FontScaleDpi;
}

bool actionButton(const char* label, float height = 34, float width = -1) {
    float buttonWidth = width;
    if (width >= 0) {
        buttonWidth = scaled(width);
    }

    const float buttonHeight = std::max(scaled(height), ImGui::GetFrameHeight());
    return ImGui::Button(label, ImVec2(buttonWidth, buttonHeight));
}

// 通用表单输入：标签单独占一行，输入框铺满宽度，可切换密码或多行模式。
void textField(const char* label,
               std::string& value,
               bool password = false,
               bool multiline = false) {
    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1);
    const std::string id = std::string("##") + label;
    if (multiline) {
        ImGui::InputTextMultiline(id.c_str(), &value, ImVec2(-1, scaled(80)));
    } else {
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
        if (password) {
            flags |= ImGuiInputTextFlags_Password;
        }
        ImGui::InputText(id.c_str(), &value, flags);
    }
}

// 通用下拉框：选项 first 是提交值，second 是显示名称，二者互不混用。
// 无选项时清空选中值，返回 false，供调用方禁用依赖该选项的按钮。
bool choices(const char* label,
             std::string& selected,
             const std::vector<std::pair<std::string, std::string>>& items) {
    // 找到当前选中值的显示名称；旧值失效时默认选择第一项。
    std::string selectedLabel;
    bool selectionFound = false;
    for (const auto& item : items) {
        if (item.first == selected) {
            selectedLabel = item.second;
            selectionFound = true;
            break;
        }
    }

    if (items.empty()) {
        selected.clear();
        selectedLabel = "暂无可选数据";
    } else if (!selectionFound) {
        selected = items.front().first;
        selectedLabel = items.front().second;
    }

    ImGui::TextUnformatted(label);
    ImGui::SetNextItemWidth(-1);
    const std::string id = std::string("##") + label;
    if (ImGui::BeginCombo(id.c_str(), selectedLabel.c_str())) {
        for (const auto& item : items) {
            const bool active = selected == item.first;
            const std::string optionLabel = item.second + "###" + item.first;
            if (ImGui::Selectable(optionLabel.c_str(), active)) {
                selected = item.first;
            }
            if (active) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return !items.empty();
}

} // namespace

// 数据工作区的表头定义：直接按页面名称配置列，与 rows() 中单元格顺序一致。
std::vector<const char*> CommunityUI::tableColumns() const {
    switch (state_.page) {
        case Page::Owners:
            return {"编号", "姓名", "电话", "楼栋", "单元", "房号", "账号"};
        case Page::Fees:
            return {"月份", "收费项目", "应缴", "实缴", "欠费", "缴费日期"};
        case Page::Tasks:
            return {"编号", "标题", "描述", "类型", "创建时间", "负责人", "状态"};
        case Page::People:
            return {"流水编号", "人员编号", "类型", "方向", "时间", "门禁点", "备注"};
        case Page::Vehicles:
            return {"流水编号", "车牌", "业主编号", "方向", "时间", "门禁点", "备注"};
        case Page::Parking:
            return {"车位编号", "车牌", "业主编号", "状态", "登记时间"};
        case Page::Visitors:
            return {"访客编号",
                    "姓名",
                    "电话",
                    "被访业主",
                    "事由",
                    "进入时间",
                    "离开时间",
                    "状态",
                    "进入门禁点"};
        case Page::Points:
            return {"编号", "名称", "类型", "位置", "状态"};
        case Page::Accounts:
            return {"姓名", "账号", "角色"};
        default:
            return {};
    }
}

CommunityUI::CommunityUI(IntegrationSystem& system) : system_(system) {
    initializeForm();
}

std::string CommunityUI::ownId() const {
    const auto owner = std::dynamic_pointer_cast<Owner>(system_.currentUser_);
    if (owner) {
        return owner->id();
    }
    return "";
}

// 页面访问权限：导航显示及实际操作都使用此规则；业主仅开放本人查询页。
bool CommunityUI::allowed(Page page) const {
    const auto& user = system_.currentUser_;
    if (!user) {
        return false;
    }
    if (user->role() == Role::Owner) {
        return page == Page::Owners || page == Page::Fees || page == Page::People ||
               page == Page::Vehicles || page == Page::Parking;
    }
    switch (page) {
        case Page::Owners:
            return user->hasPermission(Permission::QueryOwner);
        case Page::Fees:
            return user->hasPermission(Permission::ManageFees);
        case Page::Tasks:
            return user->hasPermission(Permission::ManageTasks);
        case Page::Accounts:
            return user->hasPermission(Permission::ManageAccounts);
        default:
            return user->hasPermission(Permission::ManageAccess);
    }
}

bool CommunityUI::canCreate() const {
    if (!allowed(state_.page)) {
        return false;
    }
    if (state_.page == Page::Owners) {
        return system_.currentUser_->hasPermission(Permission::ManageOwner);
    }
    return system_.currentUser_->role() != Role::Owner;
}

void CommunityUI::notify(bool success, const std::string& text) {
    state_.error = !success;
    state_.message = text;
}

// 登录窗口提交：转换输入编码并认证，成功后清空输入与旧页面状态。
bool CommunityUI::login() {
    try {
        if (!system_.accounts_.authenticate(toBusiness(state_.username),
                                            toBusiness(state_.password),
                                            system_.owners_,
                                            system_.currentUser_)) {
            notify(false, "账号或密码不正确。");
            state_.password.clear();
            return false;
        }
        state_ = State{};
        initializeForm();
        return true;
    } catch (const std::exception& error) {
        state_.password.clear();
        notify(false, error.what());
        return false;
    }
}

// 导航窗口退出登录：清除用户身份、表单和筛选条件，避免跨用户残留。
void CommunityUI::logout() {
    system_.currentUser_.reset();
    state_ = State{};
    initializeForm();
}

// 操作面板表单初始化：清空上一条输入，并为日期、方向、类型设置默认值。
void CommunityUI::initializeForm() {
    state_.form = {};
    switch (state_.page) {
        case Page::Fees:
            state_.form[2] = today("%Y-%m"); // 计费月份。
            state_.form[3] = "0";            // 应缴金额。
            state_.form[4] = "0";            // 实缴金额。
            state_.form[5] = today();        // 缴费日期。
            break;
        case Page::Tasks:
            state_.form[4] = today("%Y.%m.%d %H:%M:%S"); // 创建时间。
            break;
        case Page::People:
            state_.form[1] = "Enter";
            break;
        case Page::Vehicles:
            state_.form[2] = "Enter";
            break;
        case Page::Parking:
            state_.form[2] = today();
            break;
        case Page::Points:
            state_.form[1] = "CommunityGate";
            break;
        case Page::Accounts:
            state_.form[3] = "PropertyStaff";
            break;
        default:
            break;
    }
}

// 导航切页：同步重置中央查询条件、所选记录及右侧新增表单。
void CommunityUI::changePage(Page page) {
    if (!allowed(page)) {
        return;
    }
    state_.page = page;
    state_.search.clear();
    state_.selected.clear();
    state_.queryOwner.clear();
    state_.query.clear();
    state_.message.clear();
    initializeForm();
}

// 业主下拉框：显示“编号 + 姓名”，保存并提交业主编号。
bool CommunityUI::ownerCombo(const char* label, std::string& selected) {
    std::vector<std::pair<std::string, std::string>> items;
    for (const Owner& owner : system_.owners_.all()) {
        items.emplace_back(toUtf8(owner.id()), toUtf8(owner.id() + "  " + owner.name()));
    }
    return choices(label, selected, items);
}

// 门禁点下拉框：仅列出启用项，车辆登记进一步排除不允许车辆通行的门禁点。
bool CommunityUI::gateCombo(const char* label, std::string& selected, bool vehicle) {
    std::vector<std::pair<std::string, std::string>> items;
    for (const auto& point : system_.access_.accessPoints()) {
        if (point.status != "Enabled") {
            continue;
        }

        const bool allowsVehicles = point.type == "CommunityGate" || point.type == "GarageGate";
        if (vehicle && !allowsVehicles) {
            continue;
        }
        items.emplace_back(toUtf8(point.pointId), toUtf8(point.pointId + "  " + point.name));
    }
    return choices(label, selected, items);
}

// 任务负责人下拉选项：优先显示姓名，同名时附账号区分，提交值仍为账号。
std::vector<std::pair<std::string, std::string>> CommunityUI::staffChoices() const {
    const auto profiles = system_.accounts_.staffProfiles();
    std::vector<std::pair<std::string, std::string>> items;
    for (const auto& profile : profiles) {
        std::string label = toUtf8(profile.name);
        if (label.empty()) {
            label = "未填写姓名（" + toUtf8(profile.username) + "）";
        } else {
            int sameNameCount = 0;
            for (const auto& other : profiles) {
                if (other.name == profile.name) {
                    ++sameNameCount;
                }
            }
            if (sameNameCount > 1) {
                label += "（" + toUtf8(profile.username) + "）";
            }
        }
        items.emplace_back(toUtf8(profile.username), std::move(label));
    }
    return items;
}

// 任务表格及详情的负责人名称：依据已保存账号查询姓名
std::string CommunityUI::assigneeName(const std::string& username) const {
    if (username.empty()) {
        return "未分配";
    }
    for (const auto& profile : system_.accounts_.staffProfiles()) {
        if (profile.username == username) {
            if (profile.name.empty()) {
                return "未填写姓名（" + toUtf8(username) + "）";
            }
            return toUtf8(profile.name);
        }
    }
    // 历史负责人账号已不存在时保留可识别的提示，避免误显示为未分配。
    return "未找到工作人员（" + toUtf8(username) + "）";
}

// 中央表格的数据准备：读取当前页业务数据，处理权限隔离、中文及金额展示。
// 这里只生成显示用行数据，选中后的操作仍使用行标识调用原业务接口。
std::vector<CommunityUI::Row> CommunityUI::rows() const {
    std::vector<Row> result;
    if (!allowed(state_.page)) {
        return result;
    }
    const bool ownerOnly = system_.currentUser_->role() == Role::Owner;
    std::string ownerId;
    if (ownerOnly) {
        ownerId = ownId();
    } else {
        ownerId = toBusiness(state_.queryOwner);
    }
    switch (state_.page) {
        case Page::Owners:
            // 业主资料表：业主本人只见自己的记录，工作人员按权限查看列表。
            for (const auto& owner : system_.owners_.all()) {
                if (ownerOnly && owner.id() != ownerId) {
                    continue;
                }
                Row row;
                row.key = toUtf8(owner.id());
                row.cells = {toUtf8(owner.id()),
                             toUtf8(owner.name()),
                             toUtf8(owner.phone()),
                             toUtf8(owner.building()),
                             toUtf8(owner.unit()),
                             toUtf8(owner.room()),
                             toUtf8(owner.username())};
                result.push_back(std::move(row));
            }
            break;
        case Page::Fees: {
            // 缴费明细表：按所选业主查询，并展示项目名称及每笔欠费金额。
            const auto records = system_.property_.queryFees(ownerId);
            for (std::size_t recordIndex = 0; recordIndex < records.size(); ++recordIndex) {
                const auto& record = records[recordIndex];
                std::string itemLabel = toUtf8(record.itemId);
                for (const auto& feeItem : system_.property_.feeItems()) {
                    if (feeItem.id == record.itemId) {
                        itemLabel += " · " + toUtf8(feeItem.name);
                    }
                }
                Row row;
                row.key = std::to_string(recordIndex);
                row.cells = {toUtf8(record.month),
                             itemLabel,
                             money(record.due),
                             money(record.paid),
                             money(record.due - record.paid),
                             toUtf8(record.paymentDate)};
                result.push_back(std::move(row));
            }
            break;
        }
        case Page::Tasks:
            // 物业任务表：汇总工单信息，负责人展示姓名，状态展示中文。
            for (const auto& task : system_.property_.queryTasks()) {
                Row row;
                row.key = toUtf8(task.id);
                row.cells = {toUtf8(task.id),
                             toUtf8(task.title),
                             toUtf8(task.description),
                             toUtf8(task.type),
                             toUtf8(task.createdAt),
                             assigneeName(task.assignee),
                             taskNames[static_cast<int>(task.status)]};
                result.push_back(std::move(row));
            }
            break;
        case Page::People: {
            // 人员出入表：工作人员可查业主或访客，业主仅查询本人流水。
            std::string person = ownerId;
            if (!ownerOnly && !state_.query.empty()) {
                person = toBusiness(state_.query);
            }
            for (const auto& record : system_.access_.queryPerson(person)) {
                Row row;
                row.key = toUtf8(record.id);
                row.cells = {toUtf8(record.id),
                             toUtf8(record.personId),
                             translated(record.personType),
                             translated(record.direction),
                             toUtf8(record.time),
                             toUtf8(record.gateId),
                             toUtf8(record.note)};
                result.push_back(std::move(row));
            }
            break;
        }
        case Page::Vehicles:
            // 车辆出入表：按完整车牌查询，业主查询结果再次按所属业主过滤。
            for (const auto& record : system_.access_.queryVehicle(toBusiness(state_.query))) {
                if (ownerOnly && record.ownerId != ownerId) {
                    continue;
                }
                Row row;
                row.key = toUtf8(record.id);
                row.cells = {toUtf8(record.id),
                             toUtf8(record.plateNumber),
                             toUtf8(record.ownerId),
                             translated(record.direction),
                             toUtf8(record.time),
                             toUtf8(record.gateId),
                             toUtf8(record.note)};
                result.push_back(std::move(row));
            }
            break;
        case Page::Parking:
            // 车位占用表：展示绑定及释放状态，业主只见本人当前关联车位。
            for (const auto& record : system_.access_.parking()) {
                if (ownerOnly && record.ownerId != ownerId) {
                    continue;
                }
                Row row;
                row.key = toUtf8(record.spaceId);
                row.cells = {toUtf8(record.spaceId),
                             toUtf8(record.plateNumber),
                             toUtf8(record.ownerId),
                             translated(record.status),
                             toUtf8(record.registeredAt)};
                result.push_back(std::move(row));
            }
            break;
        case Page::Visitors:
            // 访客档案表：可切换仅在访或全部档案，签离记录仍保留。
            for (const auto& visitor : system_.access_.visitors()) {
                if (state_.visitingOnly && visitor.status != "Visiting") {
                    continue;
                }
                Row row;
                row.key = toUtf8(visitor.visitorId);
                row.cells = {toUtf8(visitor.visitorId),
                             toUtf8(visitor.name),
                             toUtf8(visitor.phone),
                             toUtf8(visitor.hostOwnerId),
                             toUtf8(visitor.reason),
                             toUtf8(visitor.enterTime),
                             toUtf8(visitor.leaveTime),
                             translated(visitor.status),
                             toUtf8(visitor.gateId)};
                result.push_back(std::move(row));
            }
            break;
        case Page::Points:
            // 门禁点表：保留已停用项，便于查看历史门禁点资料。
            for (const auto& point : system_.access_.accessPoints()) {
                Row row;
                row.key = toUtf8(point.pointId);
                row.cells = {toUtf8(point.pointId),
                             toUtf8(point.name),
                             translated(point.type),
                             toUtf8(point.location),
                             translated(point.status)};
                result.push_back(std::move(row));
            }
            break;
        case Page::Accounts:
            // 工作人员表：管理员浏览已注册人员的姓名、账号和中文角色，不读取密码。
            // 直接读取共用账号服务，新增成功后下一帧即可看到最新列表。
            for (const auto& profile : system_.accounts_.staffProfiles()) {
                Row row;
                row.key = toUtf8(profile.username);
                row.cells = {
                    toUtf8(profile.name), toUtf8(profile.username), roleName(profile.role)};
                result.push_back(std::move(row));
            }
            break;
        default:
            break;
    }
    if (state_.search.empty()) {
        return result;
    }

    // 列表内搜索：逐行检查各列，任意一列包含搜索词就保留该行。
    std::vector<Row> matchingRows;
    for (const auto& row : result) {
        for (const auto& cell : row.cells) {
            if (cell.find(state_.search) != std::string::npos) {
                matchingRows.push_back(row);
                break;
            }
        }
    }
    return matchingRows;
}

// 新增校验：先按页面列出必填字段下标，再统一检查，避免遗漏空输入。
void CommunityUI::validateCreateForm(const std::array<std::string, 8>& fields) const {
    std::vector<int> requiredFields;

    switch (state_.page) {
        case Page::Owners:
            requiredFields = {0, 5, 6};
            break;
        case Page::Fees:
            requiredFields = {0, 1, 2, 3, 4, 5};
            break;
        case Page::Tasks:
            requiredFields = {0, 1, 4};
            break;
        case Page::People:
            requiredFields = {0, 1, 3};
            break;
        case Page::Vehicles:
            requiredFields = {0, 1, 2, 4};
            break;
        case Page::Parking:
            requiredFields = {0, 1, 2};
            break;
        case Page::Visitors:
            requiredFields = {0, 2, 4};
            break;
        case Page::Points:
            requiredFields = {0, 1, 2};
            break;
        case Page::Accounts:
            requiredFields = {0, 1, 2, 3};
            break;
    }

    for (int index : requiredFields) {
        if (fields[index].empty()) {
            throw std::runtime_error("请填写所有必填项并选择关联数据。");
        }
    }
}

// 新增保存：将已转换为业务编码的表单逐项填入业务对象。
// 自动编号由业务系统返回；缴费、任务和工作人员账号沿用原有保存方式。
bool CommunityUI::createRecord(const std::array<std::string, 8>& fields,
                               std::optional<std::string>& generatedId) {
    switch (state_.page) {
        case Page::Owners: {
            const auto& name = fields[0];
            const auto& phone = fields[1];
            const auto& building = fields[2];
            const auto& unit = fields[3];
            const auto& room = fields[4];
            const auto& username = fields[5];
            const auto& password = fields[6];

            Owner owner("", name, phone, building, unit, room, username, password);
            generatedId = system_.owners_.addOwner(owner);
            return generatedId.has_value();
        }
        case Page::Fees: {
            const auto& dueText = fields[3];
            const auto& paidText = fields[4];
            std::size_t dueEnd = 0;
            std::size_t paidEnd = 0;
            const double due = std::stod(dueText, &dueEnd);
            const double paid = std::stod(paidText, &paidEnd);

            const bool entireInputParsed = dueEnd == dueText.size() && paidEnd == paidText.size();
            const bool amountsFinite = std::isfinite(due) && std::isfinite(paid);
            if (!entireInputParsed || !amountsFinite) {
                throw std::runtime_error("请输入有效的应缴和实缴金额。");
            }

            FeeRecord record;
            record.ownerId = fields[0];
            record.itemId = fields[1];
            record.month = fields[2];
            record.paymentDate = fields[5];
            record.due = due;
            record.paid = paid;
            return system_.property_.recordPayment(record);
        }
        case Page::Tasks: {
            PropertyTask task;
            task.id = fields[0];
            task.title = fields[1];
            task.description = fields[2];
            task.type = fields[3];
            task.createdAt = fields[4];
            return system_.property_.createTask(task);
        }
        case Page::People: {
            AccessRecord record;
            record.personId = fields[0];
            record.personType = "Owner";
            record.direction = fields[1];
            record.note = fields[2];
            record.gateId = fields[3];

            generatedId = system_.access_.registerPerson(record);
            return generatedId.has_value();
        }
        case Page::Vehicles: {
            VehicleAccessRecord record;
            record.plateNumber = fields[0];
            record.ownerId = fields[1];
            record.direction = fields[2];
            record.note = fields[3];
            record.gateId = fields[4];

            generatedId = system_.access_.registerVehicle(record);
            return generatedId.has_value();
        }
        case Page::Parking: {
            ParkingRecord record;
            record.plateNumber = fields[0];
            record.ownerId = fields[1];
            record.registeredAt = fields[2];

            generatedId = system_.access_.assignParking(record);
            return generatedId.has_value();
        }
        case Page::Visitors: {
            Visitor visitor;
            visitor.name = fields[0];
            visitor.phone = fields[1];
            visitor.hostOwnerId = fields[2];
            visitor.reason = fields[3];
            visitor.gateId = fields[4];

            const bool success = system_.access_.registerPerson(visitor);
            if (success) {
                generatedId = system_.access_.visitors().back().visitorId;
            }
            return success;
        }
        case Page::Points: {
            AccessPoint point;
            point.name = fields[0];
            point.type = fields[1];
            point.location = fields[2];

            generatedId = system_.access_.addAccessPoint(point);
            return generatedId.has_value();
        }
        case Page::Accounts: {
            const auto& username = fields[0];
            const auto& password = fields[1];
            const auto& name = fields[2];
            const auto& roleValue = fields[3];

            Role role = Role::PropertyStaff;
            if (roleValue == "Admin") {
                role = Role::Admin;
            } else if (roleValue == "AccessStaff") {
                role = Role::AccessStaff;
            }
            return system_.accounts_.addStaff(username, password, name, role);
        }
    }
    return false;
}

// 右侧操作面板的统一提交入口：权限检查 → 编码转换 → 业务调用 → 结果反馈。
void CommunityUI::submit(Action action) {
    // 1. 执行时再次检查权限，并确认操作属于当前页面。
    if (!canCreate()) {
        notify(false, "当前账号无权执行此操作。");
        return;
    }

    Page requiredPage = state_.page;
    switch (action) {
        case Action::DeleteOwner:
            requiredPage = Page::Owners;
            break;
        case Action::DeleteStaff:
            requiredPage = Page::Accounts;
            break;
        case Action::AssignTask:
        case Action::AdvanceTask:
            requiredPage = Page::Tasks;
            break;
        case Action::ReleaseParking:
            requiredPage = Page::Parking;
            break;
        case Action::CheckoutVisitor:
            requiredPage = Page::Visitors;
            break;
        case Action::DisablePoint:
            requiredPage = Page::Points;
            break;
        case Action::Create:
            break;
    }

    if (requiredPage != state_.page || !allowed(requiredPage)) {
        notify(false, "当前页面不允许执行此操作。");
        return;
    }

    try {
        // 2. 将 UTF-8 界面输入转换为业务编码，不直接修改原表单。
        std::array<std::string, 8> businessFields{};
        if (action == Action::Create || action == Action::AssignTask ||
            action == Action::CheckoutVisitor) {
            for (std::size_t index = 0; index < businessFields.size(); ++index) {
                businessFields[index] = toBusiness(state_.form[index]);
            }
        }
        const std::string selectedId = toBusiness(state_.selected);
        if (action != Action::Create && selectedId.empty()) {
            throw std::runtime_error("请先在列表中选择一条记录。");
        }

        // 3. 根据操作调用相应子系统；业务规则仍由子系统检查。
        bool success = false;
        std::optional<std::string> generatedId;
        switch (action) {
            case Action::Create:
                validateCreateForm(businessFields);
                success = createRecord(businessFields, generatedId);
                break;
            case Action::DeleteOwner:
                success = system_.owners_.removeOwner(selectedId);
                break;
            case Action::DeleteStaff:
                switch (system_.accounts_.removeStaff(selectedId, *system_.currentUser_)) {
                    case StaffRemovalResult::Removed:
                        success = true;
                        break;
                    case StaffRemovalResult::Forbidden:
                        notify(false, "当前账号无权删除工作人员。");
                        return;
                    case StaffRemovalResult::ProtectedAccount:
                        notify(false, "不能删除内置管理员或当前登录的账号。");
                        return;
                    case StaffRemovalResult::NotFound:
                        notify(false, "找不到所选工作人员账号。");
                        return;
                    case StaffRemovalResult::SaveFailed:
                        notify(false, "删除失败：无法保存工作人员账号文件。");
                        return;
                }
                break;
            case Action::AssignTask: {
                const auto& assigneeAccount = businessFields[5];
                bool assigneeExists = false;
                for (const auto& profile : system_.accounts_.staffProfiles()) {
                    if (profile.username == assigneeAccount) {
                        assigneeExists = true;
                        break;
                    }
                }
                if (!assigneeExists) {
                    throw std::runtime_error("请选择有效的负责人。");
                }
                success = system_.property_.assignTask(selectedId, assigneeAccount);
                break;
            }
            case Action::AdvanceTask:
                for (const auto& task : system_.property_.queryTasks()) {
                    if (task.id != selectedId || task.status == TaskStatus::Completed) {
                        continue;
                    }
                    if (task.status == TaskStatus::Unassigned) {
                        throw std::runtime_error("请先为任务分配负责人。");
                    }

                    const int nextStatusValue = static_cast<int>(task.status) + 1;
                    const auto nextStatus = static_cast<TaskStatus>(nextStatusValue);
                    success = system_.property_.updateTaskStatus(selectedId, nextStatus);
                    break;
                }
                break;
            case Action::ReleaseParking:
                success = system_.access_.releaseParking(selectedId);
                break;
            case Action::CheckoutVisitor: {
                const auto& exitGateId = businessFields[5];
                success = system_.access_.visitorCheckout(selectedId, exitGateId);
                break;
            }
            case Action::DisablePoint:
                success = system_.access_.disableAccessPoint(selectedId);
                break;
        }

        // 4. 失败时保留输入；成功时显示结果，再更新查询条件和表单。
        if (!success) {
            notify(false,
                   "操作失败：请检查必填项、编号或账号是否重复、记录状态及数据文件是否可写。");
            return;
        }

        if (generatedId.has_value()) {
            notify(true, "操作成功，编号：" + toUtf8(*generatedId));
        } else {
            notify(true, "操作成功，数据已保存。");
        }

        if (action == Action::Create) {
            if (state_.page == Page::Fees) {
                state_.queryOwner = state_.form[0];
            }
            if (state_.page == Page::People) {
                state_.queryOwner = state_.form[0];
                state_.query.clear();
            }
            if (state_.page == Page::Vehicles) {
                state_.query = state_.form[0];
            }

            initializeForm();
            state_.selected.clear();
            if (generatedId.has_value()) {
                state_.selected = toUtf8(*generatedId);
            }
        } else if (action == Action::DeleteOwner || action == Action::DeleteStaff) {
            state_.selected.clear();
        }
    } catch (const std::invalid_argument&) {
        notify(false, "金额格式不正确，请输入数字。");
    } catch (const std::out_of_range&) {
        notify(false, "输入的数字超出允许范围。");
    } catch (const std::exception& error) {
        notify(false, error.what());
    }
}

// ============================================================================
// 停靠布局：左侧导航 + 中央数据工作区 + 右侧操作面板。
// 仅首次创建或点击“恢复默认布局”时重建，平时保留用户的拖动与分离结果。
// ============================================================================
void CommunityUI::buildLayout(ImGuiID dockId) {
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->WorkSize);
    ImGuiID center = dockId;
    const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, .17f, nullptr, &center);
    const ImGuiID right =
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, .30f, nullptr, &center);
    ImGui::DockBuilderDockWindow("导航###Navigation", left);
    ImGui::DockBuilderDockWindow("数据工作区###Content", center);
    ImGui::DockBuilderDockWindow("操作面板###Actions", right);
    ImGui::DockBuilderFinish(dockId);
    state_.resetLayout = false;
}

// ============================================================================
// 窗口：欢迎登录（Login）
// 居中显示且不参与停靠，包含账号、遮挡密码、登录按钮及认证错误提示。
// ============================================================================
void CommunityUI::renderLogin() {
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(.5f, .5f));
    const float scale = ImGui::GetStyle().FontScaleDpi;
    ImGui::SetNextWindowSize(ImVec2(470 * scale, 360 * scale), ImGuiCond_Always);
    ImGui::Begin("欢迎登录###Login",
                 nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoDocking);
    EndWindow endWindow;
    ImGui::Spacing();
    ImGui::TextUnformatted("小区管理系统");
    ImGui::TextDisabled("COMMUNITY MANAGEMENT");
    ImGui::Separator();
    // 登录表单：密码框按 Enter 与点击登录按钮走同一提交逻辑。
    textField("账号", state_.username);
    ImGui::TextUnformatted("密码");
    ImGui::SetNextItemWidth(-1);
    const bool enter =
        ImGui::InputText("##密码",
                         &state_.password,
                         ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Spacing();
    const bool disabled = state_.username.empty() || state_.password.empty();
    ImGui::BeginDisabled(disabled);
    const bool clicked = actionButton("登录", 42);
    ImGui::EndDisabled();
    if (!disabled && (clicked || enter)) {
        login();
    }
    if (!state_.message.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.78f, .18f, .17f, 1));
        ImGui::TextWrapped("%s", state_.message.c_str());
        ImGui::PopStyleColor();
    }
}

// ============================================================================
// 窗口：导航（Navigation）——默认位于左侧。
// 按角色筛选页面入口，显示当前用户姓名、角色及退出登录按钮。
// ============================================================================
void CommunityUI::renderNavigation() {
    ImGui::Begin("导航###Navigation", nullptr, ImGuiWindowFlags_NoCollapse);
    EndWindow endWindow;
    ImGui::TextUnformatted("小区管理");
    ImGui::TextDisabled("工作空间");
    ImGui::Separator();
    // 功能导航列表：点击项只改变统一页面状态，不创建独立业务数据副本。
    for (int i = 0; i < 9; ++i) {
        const auto page = static_cast<Page>(i);
        if (!allowed(page)) {
            continue;
        }
        const std::string label = std::string(navIds[i]) + "  " + pageNames[i];
        if (ImGui::Selectable(label.c_str(), state_.page == page, 0, ImVec2(0, scaled(34)))) {
            changePage(page);
        }
    }
    ImGui::Separator();
    ImGui::TextWrapped("%s", toUtf8(system_.currentUser_->name()).c_str());
    ImGui::TextDisabled("%s", roleName(system_.currentUser_->role()));
    ImGui::Spacing();
    if (actionButton("退出登录")) {
        logout();
    }
}

// ============================================================================
// 窗口：数据工作区（Content）——默认位于中央。
// 上部放当前页查询条件及统计，下部共用记录表格；工作人员表仅显示公开资料。
// ============================================================================
void CommunityUI::renderContent() {
    ImGui::Begin("数据工作区###Content", nullptr, ImGuiWindowFlags_NoCollapse);
    EndWindow endWindow;
    ImGui::TextUnformatted(pageNames[static_cast<int>(state_.page)]);
    ImGui::Separator();
    // 查询区：根据业务页显示业主选择、访客编号、完整车牌或在访筛选。
    const bool ownerOnly = system_.currentUser_->role() == Role::Owner;
    if (!ownerOnly && (state_.page == Page::Fees || state_.page == Page::People)) {
        ownerCombo("查询业主", state_.queryOwner);
    }
    if (!ownerOnly && state_.page == Page::People) {
        textField("访客编号（留空查询所选业主）", state_.query);
    }
    if (state_.page == Page::Vehicles) {
        textField("查询车牌（完整车牌号）", state_.query);
    }
    if (state_.page == Page::Visitors) {
        ImGui::Checkbox("仅显示在访访客", &state_.visitingOnly);
    }
    // 缴费页统计区：与下方明细使用相同的业主范围。
    if (state_.page == Page::Fees) {
        std::string ownerId;
        if (ownerOnly) {
            ownerId = ownId();
        } else {
            ownerId = toBusiness(state_.queryOwner);
        }
        const double outstandingAmount = system_.property_.outstanding(ownerId);
        const std::string amountText = money(outstandingAmount);
        ImGui::Text("当前欠费合计：%s 元", amountText.c_str());
    }
    // 表格工具区：搜索仅影响当前列表，同时显示过滤后的记录数和空状态。
    textField("搜索当前列表", state_.search);
    const auto data = rows();
    ImGui::TextDisabled("%zu 条记录", data.size());
    if (data.empty()) {
        ImGui::Spacing();
        ImGui::TextUnformatted("暂无匹配记录。");
    } else {
        renderRecordTable(data);
    }
}

// 数据工作区 · 通用记录表格：各页面使用自己的列定义，支持调列宽和双向滚动。
void CommunityUI::renderRecordTable(const std::vector<Row>& records) {
    const auto headings = tableColumns();
    const auto flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                       ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX |
                       ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("##records",
                          static_cast<int>(headings.size()),
                          flags,
                          ImVec2(0, ImGui::GetContentRegionAvail().y))) {
        // 冻结第一行表头，纵向滚动时仍能辨认每一列。
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const auto* heading : headings) {
            ImGui::TableSetupColumn(heading);
        }
        ImGui::TableHeadersRow();
        // 只绘制可见行，避免记录较多时每帧提交全部单元格。
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(records.size()));
        while (clipper.Step()) {
            for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                const auto& row = records[rowIndex];
                ImGui::PushID(row.key.c_str());
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                // 首列提供整行选择区域；记录行标识供右侧详情和操作使用。
                const bool isSelected = state_.selected == row.key;
                const ImGuiSelectableFlags selectionFlags =
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
                if (ImGui::Selectable(row.cells[0].c_str(), isSelected, selectionFlags)) {
                    state_.selected = row.key;
                }
                for (int columnIndex = 1; columnIndex < static_cast<int>(row.cells.size());
                     ++columnIndex) {
                    ImGui::TableSetColumnIndex(columnIndex);
                    ImGui::TextUnformatted(row.cells[columnIndex].c_str());
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }
}

// ============================================================================
// 窗口：操作面板（Actions）——默认位于右侧。
// 从上到下依次为结果反馈、所选详情、新增表单、所选记录操作。
// ============================================================================
void CommunityUI::renderActions() {
    ImGui::Begin("操作面板###Actions", nullptr, ImGuiWindowFlags_NoCollapse);
    EndWindow endWindow;
    // 操作结果区：成功与失败用不同文字颜色，不影响表单内容。
    if (!state_.message.empty()) {
        ImVec4 messageColor(.1f, .46f, .3f, 1);
        if (state_.error) {
            messageColor = ImVec4(.78f, .18f, .17f, 1);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, messageColor);
        ImGui::TextWrapped("%s", state_.message.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    renderSelectedDetails();

    // 只读账号仍可查看详情，但不绘制新增表单与修改按钮。
    if (!canCreate()) {
        return;
    }
    ImGui::Text("新增 · %s", pageNames[static_cast<int>(state_.page)]);
    ImGui::Spacing();
    const bool formReady = renderCreateForm();

    // 统一提交区：必填项或下拉选项不完整时禁用新增按钮。
    ImGui::Spacing();
    ImGui::BeginDisabled(!formReady);
    if (actionButton("提交新增", 36)) {
        submit(Action::Create);
    }
    ImGui::EndDisabled();

    if (actionButton("清空表单", 30)) {
        initializeForm();
    }

    renderSelectedActions();
}

// 操作面板 · 所选记录详情：复用中央表格的列名与数据，长文本自动换行。
void CommunityUI::renderSelectedDetails() {
    if (state_.selected.empty()) {
        return;
    }

    const auto records = rows();
    for (const auto& row : records) {
        if (row.key != state_.selected) {
            continue;
        }

        if (ImGui::CollapsingHeader("所选记录详情")) {
            const auto headings = tableColumns();
            for (std::size_t columnIndex = 0; columnIndex < headings.size(); ++columnIndex) {
                ImGui::TextDisabled("%s", headings[columnIndex]);
                ImGui::TextWrapped("%s", row.cells[columnIndex].c_str());
            }
            ImGui::Separator();
        }
        break;
    }
}

// 操作面板 · 新增表单：字段通过引用绑定 state_.form，与提交入口共用输入。
bool CommunityUI::renderCreateForm() {
    switch (state_.page) {
        case Page::Owners: {
            // 业主表单：0姓名、1电话、2楼栋、3单元、4房号、5账号、6密码。
            auto& name = state_.form[0];
            auto& phone = state_.form[1];
            auto& building = state_.form[2];
            auto& unit = state_.form[3];
            auto& room = state_.form[4];
            auto& username = state_.form[5];
            auto& password = state_.form[6];

            textField("姓名", name);
            textField("联系电话", phone);
            textField("楼栋", building);
            textField("单元", unit);
            textField("房号", room);
            textField("登录账号", username);
            textField("登录密码", password, true);

            return !name.empty() && !username.empty() && !password.empty();
        }
        case Page::Fees: {
            // 缴费表单：0业主编号、1项目编号、2月份、3应缴、4实缴、5缴费日期。
            auto& ownerId = state_.form[0];
            auto& itemId = state_.form[1];
            auto& month = state_.form[2];
            auto& dueAmount = state_.form[3];
            auto& paidAmount = state_.form[4];
            auto& paymentDate = state_.form[5];

            const bool hasOwner = ownerCombo("缴费业主", ownerId);

            std::vector<std::pair<std::string, std::string>> feeOptions;
            for (const auto& item : system_.property_.feeItems()) {
                if (item.active) {
                    const std::string label =
                        toUtf8(item.id + "  " + item.name) + " / " + money(item.monthlyAmount);
                    feeOptions.emplace_back(toUtf8(item.id), label);
                }
            }
            const bool hasFeeItem = choices("收费项目", itemId, feeOptions);

            textField("计费月份", month);
            textField("应缴金额", dueAmount);
            textField("实缴金额", paidAmount);
            textField("缴费日期", paymentDate);

            const bool requiredTextFilled =
                !month.empty() && !dueAmount.empty() && !paidAmount.empty() && !paymentDate.empty();
            return hasOwner && hasFeeItem && requiredTextFilled;
        }
        case Page::Tasks: {
            // 任务表单：0编号、1标题、2描述、3类型、4创建时间；5留给负责人选择。
            auto& taskId = state_.form[0];
            auto& title = state_.form[1];
            auto& description = state_.form[2];
            auto& type = state_.form[3];
            auto& createdAt = state_.form[4];

            textField("任务编号", taskId);
            textField("标题", title);
            textField("描述", description, false, true);
            textField("任务类型", type);
            textField("创建时间", createdAt);

            return !taskId.empty() && !title.empty() && !createdAt.empty();
        }
        case Page::People: {
            // 人员登记表单：0业主编号、1方向、2备注、3门禁点编号。
            auto& ownerId = state_.form[0];
            auto& direction = state_.form[1];
            auto& note = state_.form[2];
            auto& gateId = state_.form[3];

            const bool hasOwner = ownerCombo("出入业主", ownerId);
            choices("出入方向", direction, {{"Enter", "进入"}, {"Leave", "离开"}});
            textField("备注", note);
            const bool hasGate = gateCombo("通行门禁点", gateId);

            return hasOwner && hasGate;
        }
        case Page::Vehicles: {
            // 车辆登记表单：0车牌、1业主编号、2方向、3备注、4门禁点编号。
            auto& plateNumber = state_.form[0];
            auto& ownerId = state_.form[1];
            auto& direction = state_.form[2];
            auto& note = state_.form[3];
            auto& gateId = state_.form[4];

            textField("车牌号", plateNumber);
            const bool hasOwner = ownerCombo("关联业主", ownerId);
            choices("出入方向", direction, {{"Enter", "进入"}, {"Leave", "离开"}});
            textField("备注", note);
            const bool hasGate = gateCombo("车辆门禁点", gateId, true);

            return hasOwner && hasGate && !plateNumber.empty();
        }
        case Page::Parking: {
            // 车位表单：0车牌、1业主编号、2登记时间。
            auto& plateNumber = state_.form[0];
            auto& ownerId = state_.form[1];
            auto& registeredAt = state_.form[2];

            textField("车牌号", plateNumber);
            const bool hasOwner = ownerCombo("关联业主", ownerId);
            textField("登记时间", registeredAt);

            return hasOwner && !plateNumber.empty() && !registeredAt.empty();
        }
        case Page::Visitors: {
            // 访客表单：0姓名、1电话、2被访业主、3事由、4进入门禁点；5用于签离。
            auto& name = state_.form[0];
            auto& phone = state_.form[1];
            auto& hostOwnerId = state_.form[2];
            auto& reason = state_.form[3];
            auto& gateId = state_.form[4];

            textField("访客姓名", name);
            textField("访客电话（可选）", phone);
            const bool hasOwner = ownerCombo("被访业主", hostOwnerId);
            textField("来访事由", reason);
            const bool hasGate = gateCombo("进入门禁点", gateId);

            return hasOwner && hasGate && !name.empty();
        }
        case Page::Points: {
            // 门禁点表单：0名称、1类型、2位置，类型通过中文选项映射到业务值。
            auto& name = state_.form[0];
            auto& type = state_.form[1];
            auto& location = state_.form[2];

            textField("门禁点名称", name);
            choices("门禁点类型",
                    type,
                    {{"CommunityGate", "小区大门"},
                     {"GarageGate", "车库大门"},
                     {"BuildingGate", "楼栋门"}});
            textField("位置描述", location);

            return !name.empty() && !location.empty();
        }
        case Page::Accounts: {
            // 工作人员表单：0账号、1密码、2姓名、3角色，密码输入采用遮挡模式。
            auto& username = state_.form[0];
            auto& password = state_.form[1];
            auto& name = state_.form[2];
            auto& role = state_.form[3];

            textField("工作人员账号", username);
            textField("工作人员密码", password, true);
            textField("工作人员姓名", name);
            choices(
                "账号角色",
                role,
                {{"PropertyStaff", "物业人员"}, {"AccessStaff", "门禁人员"}, {"Admin", "管理员"}});

            return !username.empty() && !password.empty() && !name.empty();
        }
    }
    return false;
}

// 操作面板 · 所选记录操作：必须先在中央表格选中记录，再操作对应业务编号。
void CommunityUI::renderSelectedActions() {
    if (!state_.selected.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("已选择：%s", state_.selected.c_str());
        if (state_.page == Page::Owners || state_.page == Page::Points ||
            state_.page == Page::Accounts) {
            const char* label = "删除所选业主";
            Action confirmAction = Action::DeleteOwner;
            if (state_.page == Page::Points) {
                label = "停用所选门禁点";
                confirmAction = Action::DisablePoint;
            } else if (state_.page == Page::Accounts) {
                label = "删除所选工作人员";
                confirmAction = Action::DeleteStaff;
            }
            if (actionButton(label)) {
                ImGui::OpenPopup("确认操作");
            }
            // 模态确认弹窗：显示所选记录的编号或账号，确认后才提交。
            if (ImGui::BeginPopupModal("确认操作", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("确认%s？", label);
                ImGui::TextUnformatted(state_.selected.c_str());
                if (actionButton("确认", 32, 100)) {
                    submit(confirmAction);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (actionButton("取消", 32, 100)) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        } else if (state_.page == Page::Tasks) {
            // 任务操作区：按姓名选择负责人，内部保留账号关联；支持顺序推进状态。
            auto& assigneeAccount = state_.form[5];
            const bool hasStaff = choices("负责人姓名", assigneeAccount, staffChoices());
            ImGui::BeginDisabled(!hasStaff);
            if (actionButton("分配负责人", 32)) {
                submit(Action::AssignTask);
            }
            ImGui::EndDisabled();
            if (actionButton("推进到下一状态", 32)) {
                submit(Action::AdvanceTask);
            }
        } else if (state_.page == Page::Parking) {
            // 车位操作区：释放所选车位，由业务层处理状态及关联资料。
            if (actionButton("释放所选车位")) {
                submit(Action::ReleaseParking);
            }
        } else if (state_.page == Page::Visitors) {
            // 访客签离区：选择签离门禁点，签离时间和离开流水由业务层生成。
            auto& exitGateId = state_.form[5];
            const bool hasGate = gateCombo("签离门禁点", exitGateId);
            ImGui::BeginDisabled(!hasGate);
            if (actionButton("所选访客签离")) {
                submit(Action::CheckoutVisitor);
            }
            ImGui::EndDisabled();
        }
    }
}

// ============================================================================
// 主界面每帧入口：顶部菜单 → 登录判断 → 停靠空间 → 三个业务窗口。
// ============================================================================
void CommunityUI::render() {
    // 顶部菜单栏：系统标题、当前身份与恢复默认布局入口。
    if (ImGui::BeginMainMenuBar()) {
        ImGui::TextUnformatted("小区管理系统");
        ImGui::Separator();
        if (system_.currentUser_) {
            ImGui::Text("%s · %s",
                        toUtf8(system_.currentUser_->name()).c_str(),
                        roleName(system_.currentUser_->role()));
            ImGui::Separator();
            if (ImGui::MenuItem("恢复默认布局")) {
                state_.resetLayout = true;
            }
        }
        ImGui::EndMainMenuBar();
    }
    // 未登录时仅绘制登录窗口；登录后才允许访问业务工作空间。
    if (!system_.currentUser_) {
        renderLogin();
        return;
    }
    const ImGuiID dockId = ImGui::GetID("CommunityWorkspace");
    const bool needsLayout = ImGui::DockBuilderGetNode(dockId) == nullptr;
    // 主视口停靠空间：先提交容器，再绘制可停靠的导航、数据和操作窗口。
    ImGui::DockSpaceOverViewport(dockId, ImGui::GetMainViewport());
    if (state_.resetLayout || needsLayout) {
        buildLayout(dockId);
    }
    try {
        renderNavigation();
        if (!system_.currentUser_) {
            return;
        }
        if (!allowed(state_.page)) {
            changePage(Page::Owners);
        }
        renderContent();
        renderActions();
    } catch (const std::exception& error) {
        // 查询输入也可能含有 GBK 无法保存的字符，显示错误并清空查询以便恢复。
        notify(false, error.what());
        state_.query.clear();
    }
}
