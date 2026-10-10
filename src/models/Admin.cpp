// ============================================================================
// 文件: Admin.cpp
// 职责: 实现管理员类 Admin 的构造与全权限判定。
// ============================================================================
#include "models/Admin.h"

#include <utility>

/// 构造管理员账号：角色固定为 Admin
Admin::Admin(std::string username, std::string password, std::string name)
    : User(std::move(username), std::move(password), std::move(name),
           Role::Admin) {}

/// 权限判定：管理员拥有全部权限，对任何权限请求均返回 true
bool Admin::hasPermission(Permission) const {
    return true;
}
