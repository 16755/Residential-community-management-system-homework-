// ============================================================================
// 文件: AccessPoint.h
// 职责: 定义门禁点结构体 AccessPoint，描述小区内一个可以登记出入的
//       具体地点（如地下车库大门、楼栋单元门口、小区大门），
//       由门禁管理系统维护。各类出入记录通过 pointId 与之关联。
// ============================================================================
#pragma once

#include <string>

/// 门禁点：一个可登记人员/车辆出入的具体地点
struct AccessPoint {
    std::string pointId;  ///< 门禁点编号（业务主键，如 AP001）
    std::string name;     ///< 门禁点名称（如"地下车库大门"）
    std::string type;     ///< 类型（CommunityGate=小区大门 / GarageGate=车库大门 / BuildingGate=楼栋门）
    std::string location; ///< 位置描述（如"B1层东侧"、"1栋1单元"）
    std::string status;   ///< 状态（Enabled=启用 / Disabled=停用）
};
