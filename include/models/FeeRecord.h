// ============================================================================
// 文件: FeeRecord.h
// 职责: 定义缴费记录结构体 FeeRecord，记录业主某月份对某收费项目的
//       应缴与实缴情况，由物业管理模块维护。
// ============================================================================
#pragma once

#include <string>

/// 缴费记录：业主某一月份对某一收费项目的应缴/实缴情况
struct FeeRecord {
    std::string ownerId;     ///< 业主编号
    std::string itemId;      ///< 收费项目编号
    std::string month;       ///< 计费月份（如 2026-10）
    std::string paymentDate; ///< 缴费日期
    double due = 0.0;        ///< 应缴金额
    double paid = 0.0;       ///< 实缴金额（合法范围 0 <= paid <= due）
};
