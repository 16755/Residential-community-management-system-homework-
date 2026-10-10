// ============================================================================
// 文件: ParkingRecord.h
// 职责: 定义车位记录结构体 ParkingRecord，描述小区车位与车牌、
//       业主的绑定关系及当前占用状态，由门禁管理系统维护。
// ============================================================================
#pragma once

#include <string>

/// 车位记录：描述一个车位的占用情况
struct ParkingRecord {
    std::string spaceId;      ///< 车位编号（业务主键）
    std::string plateNumber;  ///< 绑定车牌号（车位释放后清空）
    std::string ownerId;      ///< 关联业主编号（车位释放后清空）
    std::string status;       ///< 车位状态（Occupied=占用 / Released=已释放）
    std::string registeredAt; ///< 登记时间
};
