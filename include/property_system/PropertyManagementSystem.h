// ============================================================================
// 文件: PropertyManagementSystem.h
// 职责: 物业管理系统。管理三类数据：收费项目(FeeItem)、缴费记录(FeeRecord)、
//       物业任务(PropertyTask)；提供收费项目维护、缴费登记与欠费统计、
//       任务创建/分配/状态流转与查询能力。
// ============================================================================
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "models/FeeItem.h"
#include "models/FeeRecord.h"
#include "models/PropertyTask.h"
#include "owner_system/IOwnerQuery.h"

/// 物业管理系统
/// 职责：物业域的"数据存储层 + 业务规则层"：
///       - 加载/保存 3 个 CSV（收费项目、缴费记录、物业任务）；
///       - 收费项目为空时初始化 5 个内置默认项目；
///       - 缴费记录校验业主存在、金额合法（0 <= paid <= due）；
///       - 欠费统计 = 各记录 (应缴 - 实缴) 的累计；
///       - 任务状态只能按 Unassigned->Assigned->InProgress->Completed
///         顺序逐级流转（updateTaskStatus 校验相邻步进）。
class PropertyManagementSystem {
private:
    const IOwnerQuery& owners_;  ///< 业主查询接口（校验业主存在性）
    std::vector<FeeItem> items_;     ///< 收费项目列表
    std::vector<FeeRecord> fees_;    ///< 缴费记录列表
    std::vector<PropertyTask> tasks_;///< 物业任务列表
    std::string itemFile_; ///< 收费项目文件路径
    std::string feeFile_;  ///< 缴费记录文件路径
    std::string taskFile_; ///< 物业任务文件路径

public:
    /// 构造函数：绑定业主查询接口与数据目录，并立即加载全部数据
    explicit PropertyManagementSystem(const IOwnerQuery& owners,
                                      std::string dataDirectory = "data");

    /// 从 3 个 CSV 文件加载全部数据（收费项目为空时填充默认项目）
    void load();
    /// 将内存中的全部数据写回 3 个 CSV 文件
    bool save() const;

    /// 新增收费项目（校验编号非空、金额非负、编号唯一）
    bool addFeeItem(const FeeItem& item);
    /// 按编号覆盖更新收费项目（不存在则失败）
    bool updateFeeItem(const FeeItem& item);
    /// 登记一笔缴费（校验业主存在、金额合法）
    bool recordPayment(const FeeRecord& record);
    /// 统计某业主的累计欠费金额（所有记录 due-paid 之和）
    double outstanding(const std::string& ownerId) const;
    /// 查询某业主的全部缴费记录
    std::vector<FeeRecord> queryFees(const std::string& ownerId) const;

    /// 创建物业任务（校验编号非空且唯一）
    bool createTask(const PropertyTask& task);
    /// 给未分配任务指定负责人，并将状态置为 Assigned
    bool assignTask(const std::string& taskId,
                    const std::string& staffUsername);
    /// 按顺序规则流转任务状态（仅允许推进到下一个状态）
    bool updateTaskStatus(const std::string& taskId, TaskStatus status);
    /// 查询任务列表（可按状态过滤，缺省返回全部）
    std::vector<PropertyTask> queryTasks(
        std::optional<TaskStatus> status = std::nullopt) const;

    /// 获取全部收费项目（只读引用，避免拷贝）
    const std::vector<FeeItem>& feeItems() const;
};
