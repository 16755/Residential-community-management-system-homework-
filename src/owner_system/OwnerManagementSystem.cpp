// ============================================================================
// 文件: OwnerManagementSystem.cpp
// 职责: 实现业主管理系统的数据加载/保存、增删改查与搜索逻辑。
// ============================================================================
#include "owner_system/OwnerManagementSystem.h"

#include <algorithm>
#include <fstream>
#include <utility>

#include "common/CsvUtils.h"

/// 构造函数：指定数据文件路径并立即加载全部业主
OwnerManagementSystem::OwnerManagementSystem(std::string file)
    : file_(std::move(file)) {
    load();
}

/// 从 CSV 文件加载全部业主
/// 逻辑：跳过表头行，逐行拆分；字段数 >= 8 时按
///       (编号,姓名,电话,楼栋,单元,房号,账号,密码) 顺序构造 Owner。
void OwnerManagementSystem::load() {
    owners_.clear();
    std::ifstream input(file_);
    std::string line;
    std::getline(input, line); // 跳过表头

    while (std::getline(input, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        if (fields.size() >= 8) {
            owners_.emplace_back(fields[0], fields[1], fields[2], fields[3],
                                 fields[4], fields[5], fields[6], fields[7]);
        }
    }
}

/// 将内存中的业主列表写回 CSV
/// 逻辑：先确保父目录存在，再写入表头与全部业主行（字段统一转义）。
bool OwnerManagementSystem::save() const {
    if (!ensureParentDirectory(file_)) {
        return false;
    }

    std::ofstream output(file_);
    if (!output) {
        return false;
    }

    output << "id,name,phone,building,unit,room,username,password\n";
    for (const Owner& owner : owners_) {
        output << csvSafe(owner.id()) << ',' << csvSafe(owner.name()) << ','
                << csvSafe(owner.phone()) << ',' << csvSafe(owner.building())
                << ',' << csvSafe(owner.unit()) << ',' << csvSafe(owner.room())
                << ',' << csvSafe(owner.username()) << ','
                << csvSafe(owner.password()) << '\n';
    }
    return true;
}

/// 新增业主
/// 校验：编号与账号非空、编号不重复、账号不重复；通过后追加并落盘。
bool OwnerManagementSystem::addOwner(const Owner& owner) {
    const bool duplicateId = findOwnerById(owner.id()).has_value();
    const bool duplicateUsername = std::any_of(
        owners_.begin(), owners_.end(), [&](const Owner& current) {
            return current.username() == owner.username();
        });

    if (owner.id().empty() || owner.username().empty() || duplicateId ||
        duplicateUsername) {
        return false;
    }

    owners_.push_back(owner);
    return save();
}

/// 按编号覆盖更新业主信息：找到同编号业主则整体替换并落盘
bool OwnerManagementSystem::updateOwner(const Owner& owner) {
    for (Owner& current : owners_) {
        if (current.id() == owner.id()) {
            current = owner;
            return save();
        }
    }
    return false; // 未找到该编号
}

/// 按编号删除业主：使用 remove_if 移出所有匹配记录
bool OwnerManagementSystem::removeOwner(const std::string& ownerId) {
    const auto newEnd = std::remove_if(
        owners_.begin(), owners_.end(), [&](const Owner& owner) {
            return owner.id() == ownerId;
        });

    if (newEnd == owners_.end()) {
        return false; // 没有匹配项，删除失败
    }

    owners_.erase(newEnd, owners_.end());
    return save();
}

/// 按编号查找业主：线性遍历返回第一个匹配项（实现 IOwnerQuery 接口）
std::optional<Owner> OwnerManagementSystem::findOwnerById(
    const std::string& ownerId) const {
    for (const Owner& owner : owners_) {
        if (owner.id() == ownerId) {
            return owner;
        }
    }
    return std::nullopt;
}

/// 关键字模糊搜索：编号、姓名、房号任一包含关键字即命中
std::vector<Owner> OwnerManagementSystem::search(
    const std::string& keyword) const {
    std::vector<Owner> result;
    for (const Owner& owner : owners_) {
        if (owner.id().find(keyword) != std::string::npos ||
            owner.name().find(keyword) != std::string::npos ||
            owner.room().find(keyword) != std::string::npos) {
            result.push_back(owner);
        }
    }
    return result;
}

/// 获取全部业主列表（只读引用，避免拷贝）
const std::vector<Owner>& OwnerManagementSystem::all() const {
    return owners_;
}
