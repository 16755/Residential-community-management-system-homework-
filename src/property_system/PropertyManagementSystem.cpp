// ============================================================================
// 文件: PropertyManagementSystem.cpp
// 职责: 实现物业管理系统的数据加载/保存、收费管理与任务管理逻辑。
// ============================================================================
#include "property_system/PropertyManagementSystem.h"

#include <algorithm>
#include <fstream>
#include <utility>

#include "common/CsvUtils.h"

/// 构造函数：绑定业主查询接口，拼接三个数据文件路径并立即加载
PropertyManagementSystem::PropertyManagementSystem(
    const IOwnerQuery& owners, std::string dataDirectory)
    : owners_(owners),
      itemFile_(dataDirectory + "/fee_items.csv"),
      feeFile_(dataDirectory + "/fee_records.csv"),
      taskFile_(dataDirectory + "/property_tasks.csv") {
    load();
}

/// 从 3 个 CSV 文件加载全部数据
/// 逻辑：收费项目解析金额并读启用标志；缴费记录解析应缴/实缴金额；
///       任务记录解析状态枚举；收费项目为空时填充 5 个内置默认项目。
void PropertyManagementSystem::load() {
    items_.clear();
    fees_.clear();
    tasks_.clear();

    std::string line;
    std::ifstream itemInput(itemFile_);
    std::getline(itemInput, line); // 跳过表头
    while (std::getline(itemInput, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        bool validAmount = false;
        if (fields.size() >= 4) {
            const double amount = parseDouble(fields[2], validAmount);
            if (validAmount) {
                items_.push_back({fields[0], fields[1], amount,
                                  fields[3] == "1"});
            }
        }
    }

    // 兜底：首次运行（无收费项目文件）时内置 5 个默认收费项目
    if (items_.empty()) {
        items_ = {
            {"F001", "基础物业服务", 120.0, true},
            {"F002", "公共区域保洁", 30.0, true},
            {"F003", "绿化维护", 20.0, true},
            {"F004", "秩序维护", 40.0, true},
            {"F005", "公共能耗", 25.0, true}
        };
    }

    std::ifstream feeInput(feeFile_);
    std::getline(feeInput, line);
    while (std::getline(feeInput, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        if (fields.size() < 6) {
            continue;
        }
        bool validDue = false;
        bool validPaid = false;
        const double due = parseDouble(fields[3], validDue);
        const double paid = parseDouble(fields[4], validPaid);
        if (validDue && validPaid) {
            fees_.push_back({fields[0], fields[1], fields[2], fields[5],
                              due, paid});
        }
    }

    std::ifstream taskInput(taskFile_);
    std::getline(taskInput, line);
    while (std::getline(taskInput, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        TaskStatus status;
        if (fields.size() >= 8 && parseTaskStatus(fields[6], status)) {
            tasks_.push_back({fields[0], fields[1], fields[2], fields[3],
                              fields[4], fields[5], fields[7], status});
        }
    }
}

/// 将内存中的全部数据写回 3 个 CSV 文件
/// 逻辑：先确保目录存在，再分别写入表头与数据行（字段统一转义）。
bool PropertyManagementSystem::save() const {
    if (!ensureParentDirectory(itemFile_)) {
        return false;
    }

    std::ofstream itemOutput(itemFile_);
    std::ofstream feeOutput(feeFile_);
    std::ofstream taskOutput(taskFile_);
    if (!itemOutput || !feeOutput || !taskOutput) {
        return false;
    }

    itemOutput << "id,name,amount,active\n";
    for (const FeeItem& item : items_) {
        itemOutput << csvSafe(item.id) << ',' << csvSafe(item.name) << ','
                   << item.monthlyAmount << ',' << (item.active ? 1 : 0)
                   << '\n';
    }

    feeOutput << "owner,item,month,due,paid,date\n";
    for (const FeeRecord& fee : fees_) {
        feeOutput << csvSafe(fee.ownerId) << ',' << csvSafe(fee.itemId) << ','
                  << csvSafe(fee.month) << ',' << fee.due << ',' << fee.paid
                  << ',' << csvSafe(fee.paymentDate) << '\n';
    }

    taskOutput << "id,title,description,type,created,assignee,status,note\n";
    for (const PropertyTask& task : tasks_) {
        taskOutput << csvSafe(task.id) << ',' << csvSafe(task.title) << ','
                   << csvSafe(task.description) << ',' << csvSafe(task.type)
                   << ',' << csvSafe(task.createdAt) << ','
                   << csvSafe(task.assignee) << ','
                   << taskStatusToString(task.status) << ','
                   << csvSafe(task.completionNote) << '\n';
    }
    return true;
}

/// 新增收费项目：校验编号非空、金额非负、编号唯一，通过后追加并落盘
bool PropertyManagementSystem::addFeeItem(const FeeItem& item) {
    const bool duplicate = std::any_of(
        items_.begin(), items_.end(), [&](const FeeItem& current) {
            return current.id == item.id;
        });
    if (item.id.empty() || item.monthlyAmount < 0.0 || duplicate) {
        return false;
    }
    items_.push_back(item);
    return save();
}

/// 按编号覆盖更新收费项目：找到同编号项目则整体替换并落盘
bool PropertyManagementSystem::updateFeeItem(const FeeItem& item) {
    for (FeeItem& current : items_) {
        if (current.id == item.id) {
            current = item;
            return save();
        }
    }
    return false; // 未找到该编号
}

/// 登记一笔缴费
/// 校验：业主存在、项目编号/月份非空、金额合法（0 <= paid <= due）；
///       通过后追加缴费记录并落盘。
bool PropertyManagementSystem::recordPayment(const FeeRecord& record) {
    if (!owners_.findOwnerById(record.ownerId).has_value() ||
        record.itemId.empty() || record.month.empty() || record.due < 0.0 ||
        record.paid < 0.0 || record.paid > record.due) {
        return false;
    }
    fees_.push_back(record);
    return save();
}

/// 统计某业主累计欠费：遍历该业主全部缴费记录，累加 (应缴 - 实缴)
double PropertyManagementSystem::outstanding(const std::string& ownerId) const {
    double total = 0.0;
    for (const FeeRecord& fee : fees_) {
        if (fee.ownerId == ownerId) {
            total += fee.due - fee.paid;
        }
    }
    return total;
}

/// 查询某业主的全部缴费记录（线性过滤）
std::vector<FeeRecord> PropertyManagementSystem::queryFees(
    const std::string& ownerId) const {
    std::vector<FeeRecord> result;
    for (const FeeRecord& fee : fees_) {
        if (fee.ownerId == ownerId) {
            result.push_back(fee);
        }
    }
    return result;
}

/// 创建物业任务：校验编号非空且唯一，通过后追加并落盘
bool PropertyManagementSystem::createTask(const PropertyTask& task) {
    const bool duplicate = std::any_of(
        tasks_.begin(), tasks_.end(), [&](const PropertyTask& current) {
            return current.id == task.id;
        });
    if (task.id.empty() || duplicate) {
        return false;
    }
    tasks_.push_back(task);
    return save();
}

/// 分配任务：仅允许给"未分配"任务指定负责人，并将状态置为 Assigned
bool PropertyManagementSystem::assignTask(
    const std::string& taskId, const std::string& staffUsername) {
    for (PropertyTask& task : tasks_) {
        if (task.id == taskId && task.status == TaskStatus::Unassigned &&
            !staffUsername.empty()) {
            task.assignee = staffUsername;
            task.status = TaskStatus::Assigned;
            return save();
        }
    }
    return false;
}

/// 按顺序规则流转任务状态
/// 逻辑：任务状态必须严格推进到"下一个"状态
///       （Unassigned->Assigned->InProgress->Completed），
///       跳级或回退均被拒绝。
bool PropertyManagementSystem::updateTaskStatus(
    const std::string& taskId, TaskStatus status) {
    for (PropertyTask& task : tasks_) {
        const int expected = static_cast<int>(task.status) + 1;
        if (task.id == taskId && static_cast<int>(status) == expected) {
            task.status = status;
            return save();
        }
    }
    return false;
}

/// 查询任务列表：可按状态过滤（缺省返回全部任务）
std::vector<PropertyTask> PropertyManagementSystem::queryTasks(
    std::optional<TaskStatus> status) const {
    std::vector<PropertyTask> result;
    for (const PropertyTask& task : tasks_) {
        if (!status.has_value() || task.status == status.value()) {
            result.push_back(task);
        }
    }
    return result;
}

/// 获取全部收费项目（只读引用，避免拷贝）
const std::vector<FeeItem>& PropertyManagementSystem::feeItems() const {
    return items_;
}
