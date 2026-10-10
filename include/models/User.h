// ============================================================================
// 文件: User.h
// 职责: 定义系统账号模型的抽象基类 User，以及角色(Role)、权限(Permission)
//       两类枚举。所有登录账号（业主、物业人员、门禁人员、管理员）均继承
//       User，并通过重写 hasPermission() 实现各自的权限判定。
// ============================================================================
#pragma once

#include <string>

/// 系统角色枚举：决定账号登录后能进入哪个功能菜单
enum class Role {
    Owner,          ///< 业主：登录后进入业主菜单
    PropertyStaff,  ///< 物业人员：可管理收费与任务
    AccessStaff,    ///< 门禁人员：可管理出入登记与车位
    Admin           ///< 管理员：拥有全部权限，可管理业主与账号
};

/// 系统权限枚举：细粒度定义各类可执行操作，供菜单入口做权限校验
enum class Permission {
    QueryOwner,    ///< 查询业主信息
    ManageOwner,   ///< 增删改业主信息
    ManageFees,    ///< 管理物业收费
    ManageTasks,   ///< 管理物业任务
    ManageAccess,  ///< 管理门禁出入与车位
    ManageAccounts ///< 管理工作人员账号
};

/// 将角色枚举转换为字符串（用于 CSV 持久化与界面显示）
std::string roleToString(Role role);

/// 用户抽象基类：保存登录所需的通用账号信息
/// 职责：作为多态基类，统一管理 用户名/密码/姓名/角色 的存取；
///       通过纯虚函数 hasPermission() 把"某个账号是否具备某项权限"的
///       判定逻辑下沉到各派生类（Owner/PropertyStaff/AccessStaff/Admin）。
class User {
protected:
    std::string username_; ///< 登录账号名
    std::string password_; ///< 登录密码
    std::string name_;     ///< 真实姓名
    Role role_;            ///< 账号角色

public:
    /// 构造一个用户账号（以移动语义接收参数，避免不必要的拷贝）
    /// @param username 登录账号名
    /// @param password 登录密码
    /// @param name     真实姓名
    /// @param role     账号角色
    User(std::string username, std::string password,
         std::string name, Role role);
    virtual ~User() = default;

    /// 获取登录账号名
    const std::string& username() const;
    /// 获取登录密码
    const std::string& password() const;
    /// 获取真实姓名
    const std::string& name() const;
    /// 获取账号角色
    Role role() const;

    /// 判断当前用户是否具备指定权限（纯虚函数，由各派生类实现）
    virtual bool hasPermission(Permission permission) const = 0;
    /// 生成用户简要档案文本（默认 "姓名 (角色)"，派生类可扩展）
    virtual std::string profile() const;
};
