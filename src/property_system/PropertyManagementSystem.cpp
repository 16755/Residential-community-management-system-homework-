// ============================================================================
// 文件: PropertyManagementSystem.cpp
// 职责: 实现物业管理系统的数据加载/保存、收费管理与任务管理逻辑。
// ============================================================================
#include "property_system/PropertyManagementSystem.h"

#include <algorithm>
#include <atomic>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <process.h>
#include <utility>

#include "common/CsvUtils.h"

namespace {
bool hasText(const std::string& text) {
    return text.find_first_not_of(" \t\r\n\v\f") != std::string::npos;
}

bool reportedBy(const PropertyTask& task, const Owner& reporter) {
    return task.isRepair() && task.reporterOwnerId == reporter.id() &&
           task.reporterUsername == reporter.username();
}

// Task descriptions and resolution notes can span multiple physical CSV lines.
bool readTaskRow(std::istream& input, std::string& row) {
    if (!std::getline(input, row)) {
        return false;
    }
    bool quoted = std::count(row.begin(), row.end(), '"') % 2 != 0;
    std::string line;
    while (quoted && std::getline(input, line)) {
        row += '\n';
        row += line;
        if (std::count(line.begin(), line.end(), '"') % 2 != 0) {
            quoted = !quoted;
        }
    }
    return !quoted;
}
} // namespace

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
    while (readTaskRow(taskInput, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        TaskStatus status;
        if (fields.size() >= 8 && parseTaskStatus(fields[6], status)) {
            PropertyTask task{fields[0], fields[1], fields[2], fields[3],
                              fields[4], fields[5], fields[7], status};
            if (fields.size() >= 10) {
                task.reporterOwnerId = fields[8];
                task.reporterUsername = fields[9];
            }
            tasks_.push_back(std::move(task));
        }
    }
}

/// 将内存中的全部数据写回 3 个 CSV 文件
bool PropertyManagementSystem::save() const {
    return saveBilling() && saveTasks();
}

bool PropertyManagementSystem::saveBilling() const {
    if (!ensureParentDirectory(itemFile_)) {
        return false;
    }

    std::ofstream itemOutput(itemFile_);
    std::ofstream feeOutput(feeFile_);
    if (!itemOutput || !feeOutput) {
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

    itemOutput.close();
    feeOutput.close();
    return itemOutput && feeOutput;
}

bool PropertyManagementSystem::saveTasks() const {
    if (!ensureParentDirectory(taskFile_)) {
        return false;
    }

    // Write the complete work-order file before replacing the existing history.
    static std::atomic<unsigned> saveSequence{0};
    const std::filesystem::path destination(taskFile_);
    auto temporary = destination;
    temporary += "." + std::to_string(_getpid()) + "." + std::to_string(saveSequence++) + ".tmp";
    std::ofstream taskOutput(temporary);
    if (!taskOutput) {
        return false;
    }
    taskOutput << "id,title,description,type,created,assignee,status,note,reporter_owner_id,reporter_username\n";
    for (const PropertyTask& task : tasks_) {
        taskOutput << csvSafe(task.id) << ',' << csvSafe(task.title) << ','
                   << csvSafe(task.description) << ',' << csvSafe(task.type)
                   << ',' << csvSafe(task.createdAt) << ','
                   << csvSafe(task.assignee) << ','
                   << taskStatusToString(task.status) << ','
                   << csvSafe(task.completionNote) << ','
                   << csvSafe(task.reporterOwnerId) << ','
                   << csvSafe(task.reporterUsername) << '\n';
    }
    taskOutput.close();
    std::error_code error;
    if (taskOutput) {
        std::filesystem::rename(temporary, destination, error);
        if (!error) {
            return true;
        }
    }
    std::filesystem::remove(temporary, error);
    return false;
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
    return saveBilling();
}

/// 按编号覆盖更新收费项目：找到同编号项目则整体替换并落盘
bool PropertyManagementSystem::updateFeeItem(const FeeItem& item) {
    for (FeeItem& current : items_) {
        if (current.id == item.id) {
            current = item;
            return saveBilling();
        }
    }
    return false; // 未找到该编号
}

/// 登记一笔缴费
/// 校验：业主存在、项目编号/月份非空、金额合法（0 <= paid <= due）；
///       通过后追加缴费记录并落盘。
bool PropertyManagementSystem::recordPayment(const FeeRecord& record) {
    if (!owners_.isOwnerActive(record.ownerId) ||
        record.itemId.empty() || record.month.empty() || record.due < 0.0 ||
        record.paid < 0.0 || record.paid > record.due) {
        return false;
    }
    fees_.push_back(record);
    return saveBilling();
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

PropertyTask* PropertyManagementSystem::findTask(const std::string& taskId) {
    const auto it = std::find_if(tasks_.begin(), tasks_.end(), [&](const PropertyTask& task) {
        return task.id == taskId;
    });
    return it != tasks_.end() ? &*it : nullptr;
}

bool PropertyManagementSystem::appendTask(const PropertyTask& task) {
    tasks_.push_back(task);
    if (!saveTasks()) {
        tasks_.pop_back();
        return false;
    }
    return true;
}

bool PropertyManagementSystem::storeTaskUpdate(PropertyTask& task, PropertyTask updated) {
    std::swap(task, updated);
    if (!saveTasks()) {
        std::swap(task, updated);
        return false;
    }
    return true;
}

bool PropertyManagementSystem::createTask(const PropertyTask& task) {
    if (task.id.empty() || findTask(task.id) || task.isRepair() ||
        !task.reporterUsername.empty() || task.status == TaskStatus::AwaitingConfirmation) {
        return false;
    }
    return appendTask(task);
}

/// 分配任务：仅允许给"未分配"任务指定负责人，并将状态置为 Assigned
bool PropertyManagementSystem::assignTask(
    const std::string& taskId, const std::string& staffUsername) {
    auto* task = findTask(taskId);
    if (!task || task->status != TaskStatus::Unassigned || !hasText(staffUsername)) {
        return false;
    }
    auto updated = *task;
    updated.assignee = staffUsername;
    updated.status = TaskStatus::Assigned;
    return storeTaskUpdate(*task, std::move(updated));
}

/// 报修进展到进行中后，只能通过 finishRepair / confirmRepair 继续流转。
bool PropertyManagementSystem::updateTaskStatus(
    const std::string& taskId, TaskStatus status) {
    auto* task = findTask(taskId);
    if (!task) {
        return false;
    }
    const bool valid = (task->status == TaskStatus::Assigned && status == TaskStatus::InProgress) ||
        (!task->isRepair() && ((task->status == TaskStatus::Unassigned && status == TaskStatus::Assigned) ||
                              (task->status == TaskStatus::InProgress && status == TaskStatus::Completed)));
    if (!valid) {
        return false;
    }
    auto updated = *task;
    updated.status = status;
    return storeTaskUpdate(*task, std::move(updated));
}

bool PropertyManagementSystem::isCurrentOwner(const Owner& owner) const {
    const auto current = owners_.findOwnerById(owner.id());
    return current && current->status() == OwnerStatus::Active &&
           !owner.username().empty() && current->username() == owner.username();
}

std::optional<std::string> PropertyManagementSystem::submitRepair(
    const Owner& reporter, const std::string& title, const std::string& description) {
    if (!isCurrentOwner(reporter) || !hasText(title) || !hasText(description)) {
        return std::nullopt;
    }

    const auto now = std::time(nullptr);
    std::tm local{};
    char timestamp[32]{};
    if (localtime_s(&local, &now) != 0 ||
        !std::strftime(timestamp, sizeof(timestamp), "%Y.%m.%d %H:%M:%S", &local)) {
        return std::nullopt;
    }

    PropertyTask task;
    auto sequence = tasks_.size() + 1;
    do {
        task.id = "R-" + std::to_string(sequence++);
    } while (findTask(task.id));
    task.title = title;
    task.description = description;
    task.createdAt = timestamp;
    task.reporterOwnerId = reporter.id();
    task.reporterUsername = reporter.username();
    return appendTask(task) ? std::optional<std::string>(task.id) : std::nullopt;
}

std::vector<PropertyTask> PropertyManagementSystem::queryRepairs(const Owner& reporter) const {
    std::vector<PropertyTask> result;
    if (isCurrentOwner(reporter)) {
        std::copy_if(tasks_.begin(), tasks_.end(), std::back_inserter(result),
                     [&](const PropertyTask& task) { return reportedBy(task, reporter); });
    }
    return result;
}

bool PropertyManagementSystem::finishRepair(const std::string& taskId, const std::string& note) {
    auto* task = findTask(taskId);
    if (!task || !task->isRepair() || task->status != TaskStatus::InProgress || !hasText(note)) {
        return false;
    }
    auto updated = *task;
    updated.completionNote = note;
    updated.status = TaskStatus::AwaitingConfirmation;
    return storeTaskUpdate(*task, std::move(updated));
}

bool PropertyManagementSystem::confirmRepair(const std::string& taskId, const Owner& reporter) {
    auto* task = findTask(taskId);
    if (!task || task->status != TaskStatus::AwaitingConfirmation ||
        !isCurrentOwner(reporter) || !reportedBy(*task, reporter)) {
        return false;
    }
    auto updated = *task;
    updated.status = TaskStatus::Completed;
    return storeTaskUpdate(*task, std::move(updated));
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
