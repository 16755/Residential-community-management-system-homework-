// ============================================================================
// 文件: AccountService.cpp
// 职责: 实现账号服务的加载/保存、登录认证与工作人员账号新增。
// ============================================================================
#include "owner_system/AccountService.h"

#include <algorithm>
#include <fstream>
#include <utility>

#include "common/CsvUtils.h"
#include "models/AccessStaff.h"
#include "models/Admin.h"
#include "models/PropertyStaff.h"

/// 构造函数：仅保存账号文件路径（数据加载由 load() 完成）
AccountService::AccountService(std::string file) : file_(std::move(file)) {}

/// 从 CSV 加载工作人员账号
/// 逻辑：跳过表头，按角色字段（第 4 列）实例化对应派生类；
///       若文件中不存在内置 admin 账号，则自动补建默认管理员
///       （admin / admin123），保证系统始终有最高权限入口。
void AccountService::load(const OwnerManagementSystem&) {
    staff_.clear();
    std::ifstream input(file_);
    std::string line;
    std::getline(input, line); // 跳过表头

    while (std::getline(input, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        if (fields.size() < 4) {
            continue;
        }

        if (fields[3] == "Admin") {
            staff_.push_back(
                std::make_shared<Admin>(fields[0], fields[1], fields[2]));
        } else if (fields[3] == "PropertyStaff") {
            staff_.push_back(std::make_shared<PropertyStaff>(
                fields[0], fields[1], fields[2]));
        } else if (fields[3] == "AccessStaff") {
            staff_.push_back(std::make_shared<AccessStaff>(
                fields[0], fields[1], fields[2]));
        }
    }

    // 兜底：确保始终存在内置管理员账号
    const bool hasAdmin = std::any_of(
        staff_.begin(), staff_.end(), [](const std::shared_ptr<User>& user) {
            return user->username() == "admin";
        });
    if (!hasAdmin) {
        staff_.push_back(std::make_shared<Admin>(
            "admin", "admin123", "System Administrator"));
    }
}

/// 将工作人员账号列表写回 CSV
/// 逻辑：写入表头 username,password,name,role 及全部账号行。
bool AccountService::save() const {
    if (!ensureParentDirectory(file_)) {
        return false;
    }

    std::ofstream output(file_);
    if (!output) {
        return false;
    }

    output << "username,password,name,role\n";
    for (const std::shared_ptr<User>& user : staff_) {
        output << csvSafe(user->username()) << ','
                << csvSafe(user->password()) << ',' << csvSafe(user->name())
                << ',' << roleToString(user->role()) << '\n';
    }
    return true;
}

/// 登录认证：全系统唯一的登录入口
/// 逻辑：先遍历业主列表匹配 用户名+密码（命中则构造 Owner 副本返回），
///       再遍历工作人员账号列表匹配；都未命中则返回 false。
bool AccountService::authenticate(
    const std::string& username, const std::string& password,
    const OwnerManagementSystem& owners,
    std::shared_ptr<User>& result) const {
    // 第一步：匹配业主账号
    for (const Owner& owner : owners.all()) {
        if (owner.username() == username && owner.password() == password) {
            result = std::make_shared<Owner>(owner);
            return true;
        }
    }

    // 第二步：匹配工作人员账号
    for (const std::shared_ptr<User>& user : staff_) {
        if (user->username() == username && user->password() == password) {
            result = user;
            return true;
        }
    }
    return false;
}

/// 新增工作人员账号
/// 校验：账号/密码非空、账号不重复、角色不能为业主；
///       通过后按角色实例化派生类并追加、落盘。
bool AccountService::addStaff(const std::string& username,
                              const std::string& password,
                              const std::string& name, Role role) {
    const bool duplicate = std::any_of(
        staff_.begin(), staff_.end(), [&](const std::shared_ptr<User>& user) {
            return user->username() == username;
        });
    if (username.empty() || password.empty() || role == Role::Owner ||
        duplicate) {
        return false;
    }

    if (role == Role::Admin) {
        staff_.push_back(std::make_shared<Admin>(username, password, name));
    } else if (role == Role::PropertyStaff) {
        staff_.push_back(
            std::make_shared<PropertyStaff>(username, password, name));
    } else {
        staff_.push_back(
            std::make_shared<AccessStaff>(username, password, name));
    }
    return save();
}
