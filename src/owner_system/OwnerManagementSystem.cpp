// ============================================================================
// 文件: OwnerManagementSystem.cpp
// 职责: 实现业主管理系统的数据加载/保存、增删改查与搜索逻辑。
// ============================================================================
#include "owner_system/OwnerManagementSystem.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

#include "common/CsvUtils.h"

/// 构造函数：指定数据文件路径并立即加载全部业主
OwnerManagementSystem::OwnerManagementSystem(std::string file)
    : file_(std::move(file)) {
    load();
}

/// 从 CSV 文件加载全部业主
/// 逻辑：第九列为状态；旧的八列数据按 Active 加载以保持兼容。
void OwnerManagementSystem::load() {
    owners_.clear();
    std::ifstream input(file_);
    std::string line;
    std::getline(input, line); // 跳过表头

    while (std::getline(input, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        if (fields.size() >= 8) {
            const OwnerStatus status = fields.size() >= 9
                ? ownerStatusFromString(fields[8]) : OwnerStatus::Active;
            owners_.emplace_back(fields[0], fields[1], fields[2], fields[3],
                                 fields[4], fields[5], fields[6], fields[7],
                                 status);
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

    output << "id,name,phone,building,unit,room,username,password,status\n";
    for (const Owner& owner : owners_) {
        output << csvSafe(owner.id()) << ',' << csvSafe(owner.name()) << ','
                << csvSafe(owner.phone()) << ',' << csvSafe(owner.building())
                << ',' << csvSafe(owner.unit()) << ',' << csvSafe(owner.room())
                << ',' << csvSafe(owner.username()) << ','
                << csvSafe(owner.password()) << ','
                << ownerStatusToString(owner.status()) << '\n';
    }
    return true;
}

/// 扫描规范编号 Oxxxx，生成最大流水号的下一号
std::string OwnerManagementSystem::generateOwnerId() const {
    unsigned long maxSequence = 0;
    for (const Owner& owner : owners_) {
        const std::string& id = owner.id();
        if (id.size() <= 1 || id[0] != 'O') {
            continue;
        }
        try {
            std::size_t parsed = 0;
            const unsigned long sequence = std::stoul(id.substr(1), &parsed);
            if (parsed == id.size() - 1) {
                maxSequence = std::max(maxSequence, sequence);
            }
        } catch (...) {
            // 非规范历史编号不参与流水号计算
        }
    }

    std::ostringstream output;
    output << 'O' << std::setw(4) << std::setfill('0') << maxSequence + 1;
    return output.str();
}

/// 新增业主
/// 校验账号非空且不重复；编号由系统生成，新档案统一进入待审核状态。
std::optional<std::string> OwnerManagementSystem::addOwner(
    const Owner& owner) {
    const bool duplicateUsername = std::any_of(
        owners_.begin(), owners_.end(), [&](const Owner& current) {
            return current.username() == owner.username();
        });

    if (owner.username().empty() || duplicateUsername) {
        return std::nullopt;
    }

    const std::string id = generateOwnerId();
    owners_.emplace_back(id, owner.name(), owner.phone(), owner.building(),
                         owner.unit(), owner.room(), owner.username(),
                         owner.password(), OwnerStatus::Pending);
    if (!save()) {
        owners_.pop_back();
        return std::nullopt;
    }
    return id;
}

/// 按编号更新资料；生命周期状态只能由专用流转接口修改。
bool OwnerManagementSystem::updateOwner(const Owner& owner) {
    const bool duplicateUsername = std::any_of(
        owners_.begin(), owners_.end(), [&](const Owner& current) {
            return current.id() != owner.id() &&
                   current.username() == owner.username();
        });
    if (owner.username().empty() || duplicateUsername) {
        return false;
    }

    for (Owner& current : owners_) {
        if (current.id() == owner.id()) {
            const Owner previous = current;
            current = Owner(current.id(), owner.name(), owner.phone(),
                            owner.building(), owner.unit(), owner.room(),
                            owner.username(), owner.password(),
                            current.status());
            if (!save()) {
                current = previous;
                return false;
            }
            return true;
        }
    }
    return false; // 未找到该编号
}

bool OwnerManagementSystem::changeStatus(const std::string& ownerId,
                                         OwnerStatus expected,
                                         OwnerStatus target) {
    for (Owner& owner : owners_) {
        if (owner.id() != ownerId || owner.status() != expected) {
            continue;
        }

        const Owner previous = owner;
        owner = Owner(owner.id(), owner.name(), owner.phone(), owner.building(),
                      owner.unit(), owner.room(), owner.username(),
                      owner.password(), target);
        if (!save()) {
            owner = previous;
            return false;
        }
        return true;
    }
    return false;
}

bool OwnerManagementSystem::approveOwner(const std::string& ownerId) {
    return changeStatus(ownerId, OwnerStatus::Pending, OwnerStatus::Active);
}

bool OwnerManagementSystem::moveOutOwner(const std::string& ownerId) {
    return changeStatus(ownerId, OwnerStatus::Active, OwnerStatus::MovedOut);
}

bool OwnerManagementSystem::removeOwner(const std::string& ownerId) {
    return moveOutOwner(ownerId);
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
