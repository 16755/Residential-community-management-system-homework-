// ============================================================================
// 文件: User.cpp
// 职责: 实现 User 抽象基类与角色字符串转换函数。
// ============================================================================
#include "models/User.h"

#include <utility>

/// 角色枚举转字符串：与 CSV 文件中存储的角色文本保持一致
std::string roleToString(Role role) {
    switch (role) {
    case Role::Owner:
        return "Owner";
    case Role::PropertyStaff:
        return "PropertyStaff";
    case Role::AccessStaff:
        return "AccessStaff";
    case Role::Admin:
        return "Admin";
    }
    return "Unknown"; // 防御性分支：未知角色
}

/// 构造用户账号：以移动语义接收四个参数，避免字符串拷贝
User::User(std::string username, std::string password,
           std::string name, Role role)
    : username_(std::move(username)),
      password_(std::move(password)),
      name_(std::move(name)),
      role_(role) {}

/// 以下为账号各字段的只读访问器（内联实现，返回 const 引用避免拷贝）
const std::string& User::username() const { return username_; }
const std::string& User::password() const { return password_; }
const std::string& User::name() const { return name_; }
Role User::role() const { return role_; }

/// 生成默认档案文本："姓名 (角色)"
/// 派生类（如 Owner）可在此基础上追加更多信息
std::string User::profile() const {
    return name_ + " (" + roleToString(role_) + ")";
}
