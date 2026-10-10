// ============================================================================
// 文件: PropertyTask.cpp
// 职责: 实现任务状态枚举与字符串之间的互转函数。
// ============================================================================
#include "models/PropertyTask.h"

/// 任务状态枚举转字符串：与 CSV 文件中存储的状态文本保持一致
std::string taskStatusToString(TaskStatus status) {
    switch (status) {
    case TaskStatus::Unassigned:
        return "Unassigned";
    case TaskStatus::Assigned:
        return "Assigned";
    case TaskStatus::InProgress:
        return "InProgress";
    case TaskStatus::Completed:
        return "Completed";
    }
    return "Unknown"; // 防御性分支：未知状态
}

/// 字符串解析为任务状态枚举：无法识别时返回 false，状态保持原值
bool parseTaskStatus(const std::string& value, TaskStatus& status) {
    if (value == "Unassigned") {
        status = TaskStatus::Unassigned;
    } else if (value == "Assigned") {
        status = TaskStatus::Assigned;
    } else if (value == "InProgress") {
        status = TaskStatus::InProgress;
    } else if (value == "Completed") {
        status = TaskStatus::Completed;
    } else {
        return false;
    }
    return true;
}
