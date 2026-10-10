// ============================================================================
// 文件: AccessStaff.h
// 职责: 定义门禁人员类 AccessStaff。负责小区出入登记与车位管理，
//       并可查询业主信息。
// ============================================================================
#pragma once

#include "models/User.h"

/// 门禁人员类（final）
/// 职责：门禁岗位账号。具备 查询业主、管理门禁 两项权限，
///       无收费与任务管理权限。
class AccessStaff final : public User {
public:
    /// 构造门禁人员账号
    /// @param username 登录账号名
    /// @param password 登录密码
    /// @param name     真实姓名
    AccessStaff(std::string username, std::string password,
                std::string name);

    /// 权限判定：允许 查询业主 / 管理门禁
    bool hasPermission(Permission permission) const override;
};
