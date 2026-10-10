// ============================================================================
// 文件: AccessRecord.h
// 职责: 定义人员出入记录结构体 AccessRecord，记录一次人员进出小区的
//       登记信息，由门禁管理系统维护。
// ============================================================================
#pragma once

#include <string>

/// 人员出入记录：一次人员进出小区的登记
struct AccessRecord {
    std::string id;         ///< 记录编号（业务主键）
    std::string personId;   ///< 人员编号（关联业主编号）
    std::string personType; ///< 人员类型（如 Owner/Visitor）
    std::string direction;  ///< 进出方向（Enter/Leave）
    std::string time;       ///< 出入时间
    std::string note;       ///< 备注
};
