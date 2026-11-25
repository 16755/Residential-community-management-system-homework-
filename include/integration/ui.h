// ============================================================================
// 文件：ui.h
// 职责：统一引入 ImGui 头文件，声明界面入口、窗口绘制函数及交互状态。
// 窗口分工：登录窗口、左侧导航、中央数据工作区、右侧操作面板。
// ============================================================================
#pragma once

// 项目自有界面代码统一通过本文件使用 ImGui。
// 第三方库的实现宏仅由其原始源文件定义，避免重复实现。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "ImGui_docking/imconfig.h"
#include "ImGui_docking/imgui.h"
#include "ImGui_docking/imgui_internal.h"
#include "ImGui_docking/imgui_impl_win32.h"
#include "ImGui_docking/imgui_impl_dx11.h"
#include "ImGui_docking/imgui_stdlib.h"
#include "ImGui_docking/imstb_rectpack.h"
#include "ImGui_docking/imstb_textedit.h"
#include "ImGui_docking/imstb_truetype.h"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "integration/IntegrationSystem.h"

namespace community_ui {
// 业务字符串及 CSV 使用 GBK，ImGui 显示与输入使用 UTF-8。
std::string toUtf8(const std::string& value);
std::string toBusiness(const std::string& value);
// 从程序所在目录向上寻找数据目录，使启动位置不影响数据加载。
std::filesystem::path findDataDirectory(const std::filesystem::path& exeDirectory);
// 创建原生窗口及渲染环境，进入界面消息循环，退出时释放资源。
int runApplication();
} // namespace community_ui

// 可视化界面控制器：共用 IntegrationSystem 的业务对象和当前登录用户。
class CommunityUI {
public:
    explicit CommunityUI(IntegrationSystem& system);
    // 每帧入口：绘制顶部菜单，按登录状态选择登录窗口或三个业务窗口。
    void render();

private:
    friend struct CommunityUiTestAccess;
    // 导航页顺序与页面名称、导航序号的索引保持一致。
    enum class Page {
        Owners,
        Fees,
        Tasks,
        People,
        Vehicles,
        Parking,
        Visitors,
        Points,
        Accounts
    };
    // 操作面板按钮统一分发到 submit，集中检查权限并调用业务接口。
    enum class Action {
        Create,
        DeleteOwner,
        AssignTask,
        AdvanceTask,
        ReleaseParking,
        CheckoutVisitor,
        DisablePoint
    };
    // 数据工作区的通用表格行；cells 的顺序与对应页面表头一致。
    struct Row {
        std::string key; // UTF-8 行标识；通常为业务编号，缴费记录使用查询结果索引。
        std::vector<std::string> cells;
    };
    // 界面临时状态：不持有第二份业务数据，切页或退出登录时按需清空。
    struct State {
        Page page = Page::Owners;
        std::string username;   // 登录账号。
        std::string password;   // 登录密码。
        std::string search;     // 当前表格的搜索词。
        std::string selected;   // 当前选中行的标识。
        std::string queryOwner; // 查询的业主编号。
        std::string query;      // 人员编号或完整车牌查询条件。
        // 当前页面的表单字段，具体下标含义见 renderCreateForm 中各页面注释。
        std::array<std::string, 8> form;
        std::string message; // 登录窗口或操作面板显示的最近一次操作结果。
        bool error = false;
        bool visitingOnly = true;
        bool resetLayout = false;
    } state_;
    IntegrationSystem& system_;

    // 权限、登录与页面状态管理。
    bool allowed(Page page) const;
    bool canCreate() const;
    bool login();
    void logout();
    void changePage(Page page);
    void initializeForm();
    void submit(Action action);
    // 新增操作：先检查当前页必填字段，再构造业务对象并保存。
    void validateCreateForm(const std::array<std::string, 8>& fields) const;
    bool createRecord(const std::array<std::string, 8>& fields,
                      std::optional<std::string>& generatedId);
    void notify(bool success, const std::string& text);
    // 登录窗口：账号、密码及登录结果。
    void renderLogin();
    // 左侧导航窗口：功能入口、用户资料与退出登录。
    void renderNavigation();
    // 中央数据工作区：查询条件、统计信息与记录表格。
    void renderContent();
    // 数据工作区的记录表格：固定表头、滚动和行选择。
    void renderRecordTable(const std::vector<Row>& records);
    // 右侧操作面板：所选记录详情、新增表单及记录操作弹窗。
    void renderActions();
    // 操作面板按显示顺序拆分，便于分别修改详情、表单及记录操作。
    void renderSelectedDetails();
    bool renderCreateForm(); // 返回当前表单是否允许点击“提交新增”。
    void renderSelectedActions();
    // 建立默认三栏停靠布局；用户主动恢复时也调用此函数。
    void buildLayout(ImGuiID dockId);
    // 表格数据与各表单下拉选项的准备函数。
    std::vector<const char*> tableColumns() const;
    std::vector<Row> rows() const;
    std::string ownId() const;
    bool ownerCombo(const char* label, std::string& selected);
    bool gateCombo(const char* label, std::string& selected, bool vehicle = false);
    std::vector<std::pair<std::string, std::string>> staffChoices() const;
    std::string assigneeName(const std::string& username) const;
};
