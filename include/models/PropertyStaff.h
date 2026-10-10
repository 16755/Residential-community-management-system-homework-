// ============================================================================
// 文件: PropertyStaff.h
// 职责: 定义物业人员类 PropertyStaff。负责物业收费与任务管理，
//       并可查询业主信息。
// ============================================================================
#pragma once

#include "models/User.h"

/// 物业人员类（final）
/// 职责：物业管理岗位账号。具备 查询业主、管理收费、管理任务 三项权限，
///       无管理业主档案与门禁的权限。
class PropertyStaff final : public User {
public:
    /// 构造物业人员账号
    /// @param username 登录账号名
    /// @param password 登录密码
    /// @param name     真实姓名
    PropertyStaff(std::string username, std::string password,
                  std::string name);

    /// 权限判定：允许 查询业主 / 管理收费 / 管理任务
    bool hasPermission(Permission permission) const override;
};
