// ============================================================================
// 文件: Admin.h
// 职责: 定义管理员类 Admin。管理员拥有系统全部权限，
//       可管理业主档案、工作人员账号，并可进入物业/门禁菜单。
// ============================================================================
#pragma once

#include "models/User.h"

/// 管理员类（final）
/// 职责：系统最高权限账号。hasPermission() 恒定返回 true，
///       即对所有权限操作放行。
class Admin final : public User {
public:
    /// 构造管理员账号
    /// @param username 登录账号名
    /// @param password 登录密码
    /// @param name     真实姓名
    Admin(std::string username, std::string password, std::string name);

    /// 权限判定：管理员拥有全部权限，无条件返回 true
    bool hasPermission(Permission permission) const override;
};
