// ============================================================================
// 文件: Owner.h
// 职责: 定义业主类 Owner。业主是小区系统的核心业务对象，
//       既包含登录账号信息（继承 User），也包含房产信息
//       （编号/电话/楼栋/单元/房号），由管理员菜单维护。
// ============================================================================
#pragma once

#include "models/User.h"

/// 业主类（final：不允许再派生子类）
/// 职责：聚合"登录账号信息 + 房产信息"。业主登录后仅具备查询
///       自己资料的权限（QueryOwner），并重写 profile() 展示房产档案。
class Owner final : public User {
private:
    std::string ownerId_;  ///< 业主编号（业务主键）
    std::string phone_;    ///< 联系电话
    std::string building_; ///< 楼栋
    std::string unit_;     ///< 单元
    std::string room_;     ///< 房号

public:
    /// 默认构造：生成一个空业主（角色固定为 Owner，各字段为空）
    Owner();
    /// 完整构造：同时初始化房产信息与登录账号信息
    Owner(std::string id, std::string name, std::string phone,
          std::string building, std::string unit, std::string room,
          std::string username, std::string password);

    /// 业主权限判定：仅允许查询业主信息，其余权限一律拒绝
    bool hasPermission(Permission permission) const override;
    /// 业主档案文本："姓名 (角色) [编号 楼栋-单元-房号]"
    std::string profile() const override;

    /// 获取业主编号
    const std::string& id() const;
    /// 获取联系电话
    const std::string& phone() const;
    /// 获取楼栋
    const std::string& building() const;
    /// 获取单元
    const std::string& unit() const;
    /// 获取房号
    const std::string& room() const;
};
