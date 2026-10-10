// ============================================================================
// 文件: Owner.cpp
// 职责: 实现业主类 Owner 的构造、权限判定、档案文本与字段访问器。
// ============================================================================
#include "models/Owner.h"

#include <utility>

/// 默认构造：生成空业主（账号字段为空、角色固定为 Owner）
/// 用于占位或创建临时对象，随后通过完整构造或赋值填充。
Owner::Owner() : User("", "", "", Role::Owner) {}

/// 完整构造：同时初始化房产信息与登录账号信息
Owner::Owner(std::string id, std::string name, std::string phone,
             std::string building, std::string unit, std::string room,
             std::string username, std::string password)
    : User(std::move(username), std::move(password), std::move(name),
           Role::Owner),
      ownerId_(std::move(id)),
      phone_(std::move(phone)),
      building_(std::move(building)),
      unit_(std::move(unit)),
      room_(std::move(room)) {}

/// 业主权限判定：仅拥有查询业主权限，其余权限一律拒绝
/// 说明：业主只能查看自己相关资料，不能管理系统数据。
bool Owner::hasPermission(Permission permission) const {
    return permission == Permission::QueryOwner;
}

/// 业主档案文本：在父类基础上追加房产位置 [编号 楼栋-单元-房号]
std::string Owner::profile() const {
    return User::profile() + " [" + ownerId_ + " " + building_ + "-" +
           unit_ + "-" + room_ + "]";
}

/// 以下为房产字段的只读访问器（内联实现）
const std::string& Owner::id() const { return ownerId_; }
const std::string& Owner::phone() const { return phone_; }
const std::string& Owner::building() const { return building_; }
const std::string& Owner::unit() const { return unit_; }
const std::string& Owner::room() const { return room_; }
