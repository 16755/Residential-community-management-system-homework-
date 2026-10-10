// ============================================================================
// 文件: PropertyStaff.cpp
// 职责: 实现物业人员类 PropertyStaff 的构造与权限判定。
// ============================================================================
#include "models/PropertyStaff.h"

#include <utility>

/// 构造物业人员账号：角色固定为 PropertyStaff
PropertyStaff::PropertyStaff(std::string username, std::string password,
                             std::string name)
    : User(std::move(username), std::move(password), std::move(name),
           Role::PropertyStaff) {}

/// 权限判定：允许 查询业主 / 管理收费 / 管理任务，
/// 无管理业主档案与门禁的权限
bool PropertyStaff::hasPermission(Permission permission) const {
    return permission == Permission::QueryOwner ||
           permission == Permission::ManageFees ||
           permission == Permission::ManageTasks;
}
