// ============================================================================
// 文件: AccountService.h
// 职责: 账号服务。管理工作人员账号（管理员/物业/门禁）的加载、保存、
//       登录认证、新增与删除；业主账号由 OwnerManagementSystem 存储，
//       认证时本服务会连同业主账号一并校验。
// ============================================================================
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "models/User.h"
#include "owner_system/OwnerManagementSystem.h"

/// 用于界面选择工作人员的公开资料，不包含密码。
struct StaffProfile {
    std::string username;
    std::string name;
    Role role;
    bool usernameConflict = false;
};

enum class StaffRemovalResult {
    Removed,
    Forbidden,
    ProtectedAccount,
    NotFound,
    SaveFailed
};

/// 账号服务类
/// 职责：工作人员账号的存储层 + 全系统唯一的登录认证入口：
///       - 从 data/accounts.csv 加载工作人员账号（内存容器持有 shared_ptr<User>）；
///       - authenticate() 依次在业主列表与工作人员列表中校验用户名/密码，
///         成功时返回对应的 User 对象；
///       - addStaff() 和 removeStaff() 用于维护工作人员账号。
class AccountService {
private:
    std::vector<std::shared_ptr<User>> staff_; ///< 内存中的工作人员账号列表
    std::string file_;                         ///< 账号数据文件路径
    const OwnerManagementSystem* owners_ = nullptr; ///< 已绑定的业主账号来源

public:
    /// 构造函数：指定账号文件路径
    explicit AccountService(std::string file = "data/accounts.csv");

    /// 从 CSV 加载工作人员账号；拒绝冒用 admin 的非管理员并补建内置管理员
    void load(OwnerManagementSystem& owners);
    /// 将内存中的工作人员账号列表写回 CSV 文件
    bool save() const;
    /// 获取工作人员公开资料，供负责人选择和姓名展示使用。
    std::vector<StaffProfile> staffProfiles() const;
    /// 查询工作人员账号是否已存在。
    bool hasStaffUsername(const std::string& username) const;
    /// 检查账号是否在业主或工作人员数据中对应多个身份。
    bool isUsernameConflicted(
        const std::string& username,
        const OwnerManagementSystem& owners) const;
    /// 登录认证：先匹配业主账号，再匹配工作人员账号
    /// @param username 用户名
    /// @param password 密码
    /// @param owners   业主管理系统（用于校验业主账号）
    /// @param result   输出参数：认证成功后返回对应的 User 对象
    /// @return 认证成功返回 true
    bool authenticate(const std::string& username,
                     const std::string& password,
                     const OwnerManagementSystem& owners,
                     std::shared_ptr<User>& result) const;
    /// 新增工作人员账号（校验非空、不重复、角色不能为业主）
    bool addStaff(const std::string& username,
                  const std::string& password,
                  const std::string& name,
                  Role role);
    /// 按账号删除工作人员；内置 admin 和当前操作者的账号受保护。
    StaffRemovalResult removeStaff(const std::string& username, const User& requester);
};
