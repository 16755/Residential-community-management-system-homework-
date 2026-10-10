// ============================================================================
// 文件: IntegrationSystem.h
// 职责: 系统集成与主控。组装业主、账号、物业、门禁四个子系统，
//       维护"当前登录用户"状态，按角色分发到对应功能菜单，
//       是控制台程序的顶层入口（run()）。
// ============================================================================
#pragma once

#include <memory>
#include <string>

#include "access_system/AccessManagementSystem.h"
#include "owner_system/AccountService.h"
#include "owner_system/OwnerManagementSystem.h"
#include "property_system/PropertyManagementSystem.h"

/// 系统集成类
/// 职责：整个程序的主控与门面：
///       - 组合持有四大子系统（业主/账号/物业/门禁）并负责初始化；
///       - 维护当前登录用户 currentUser_，贯穿一次登录会话；
///       - run() 提供登录循环，登录成功后按角色分发到对应菜单；
///       - 各菜单方法（ownerMenu/propertyMenu/accessMenu/adminMenu）
///         负责具体的命令行交互与权限校验。
class IntegrationSystem {
private:
    OwnerManagementSystem owners_;     ///< 业主管理系统
    AccountService accounts_;          ///< 账号服务
    PropertyManagementSystem property_;///< 物业管理系统
    AccessManagementSystem access_;    ///< 门禁管理系统
    std::shared_ptr<User> currentUser_;///< 当前登录用户（登出后重置为空）

    /// 读取一行用户输入（打印提示语后从标准输入读取一行）
    static std::string input(const std::string& prompt);
    /// 业主菜单：查看本人资料 / 物业费 / 出入记录 / 车辆与车位
    void ownerMenu();
    /// 物业菜单：业主查询 / 收费登记与查询 / 任务管理
    void propertyMenu();
    /// 门禁菜单：出入登记 / 车位分配与释放 / 记录查询
    void accessMenu();
    /// 管理员菜单：业主与账号管理，并可进入物业/门禁菜单
    void adminMenu();

public:
    /// 构造函数：按固定数据路径初始化四个子系统并加载账号
    IntegrationSystem();
    /// 程序主循环：登录 -> 按角色进入菜单 -> 登出，输入 0 退出程序
    void run();
};
