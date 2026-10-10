// ============================================================================
// 文件: VehicleAccessRecord.h
// 职责: 定义车辆出入记录结构体 VehicleAccessRecord，记录一次车辆进出
//       小区的登记信息，由门禁管理系统维护。
// ============================================================================
#pragma once

#include <string>

/// 车辆出入记录：一次车辆进出小区的登记
struct VehicleAccessRecord {
    std::string id;         ///< 记录编号（业务主键）
    std::string plateNumber;///< 车牌号
    std::string ownerId;    ///< 关联业主编号
    std::string direction;  ///< 进出方向（Enter/Leave）
    std::string time;       ///< 出入时间
    std::string note;       ///< 备注
};
