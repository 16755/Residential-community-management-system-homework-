// ============================================================================
// 文件: PropertyTask.h
// 职责: 定义物业任务结构体 PropertyTask 与任务状态枚举 TaskStatus。
//       物业任务（维修/保洁等工单）的生命周期由物业管理模块负责流转。
// ============================================================================
#pragma once

#include <string>

/// 物业任务状态枚举：任务按顺序流转
/// Unassigned -> Assigned -> InProgress -> Completed
enum class TaskStatus {
    Unassigned, ///< 未分配
    Assigned,   ///< 已分配负责人
    InProgress, ///< 进行中
    Completed   ///< 已完成
};

/// 将任务状态枚举转换为字符串（用于 CSV 持久化与界面显示）
std::string taskStatusToString(TaskStatus status);

/// 将字符串解析为任务状态枚举（CSV 读取时使用）
/// @param value  状态字符串（Unassigned/Assigned/InProgress/Completed）
/// @param status 输出参数：解析得到的任务状态
/// @return 解析成功返回 true，无法识别返回 false
bool parseTaskStatus(const std::string& value, TaskStatus& status);

/// 物业任务：维修、保洁等工单信息
struct PropertyTask {
    std::string id;             ///< 任务编号（业务主键）
    std::string title;          ///< 任务标题
    std::string description;    ///< 任务描述
    std::string type;           ///< 任务类型（如 维修/保洁）
    std::string createdAt;      ///< 创建时间
    std::string assignee;       ///< 负责人账号（未分配时为空）
    std::string completionNote; ///< 完成备注
    TaskStatus status = TaskStatus::Unassigned; ///< 当前任务状态
};
