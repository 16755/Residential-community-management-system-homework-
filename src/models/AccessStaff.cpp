// ============================================================================
// 文件: AccessStaff.cpp
// 职责: 实现门禁人员类 AccessStaff 的构造与权限判定。
// ============================================================================
#include "models/AccessStaff.h"

#include <utility>

/// 构造门禁人员账号：角色固定为 AccessStaff
AccessStaff::AccessStaff(std::string username, std::string password,
                         std::string name)
    : User(std::move(username), std::move(password), std::move(name),
           Role::AccessStaff) {}

/// 权限判定：允许 查询业主 / 管理门禁，
/// 无收费与任务管理的权限
bool AccessStaff::hasPermission(Permission permission) const {
    return permission == Permission::QueryOwner ||
           permission == Permission::ManageAccess;
}
