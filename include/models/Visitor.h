// ============================================================================
// 文件: Visitor.h
// 职责: 定义访客档案结构体 Visitor。描述一次完整的访客到访：
//       访客是谁、拜访哪位业主、来访事由、当前状态、进/出时间与
//       进入的门禁点。每次到访新建一条访客档案，签离后档案保留（不删除）。
// ============================================================================
#pragma once

#include <string>

/// 访客档案：一次完整到访（从登记到签离）
/// 与 AccessRecord 的分工：
///   - Visitor 是"主数据/状态型"，一次到访一条；
///   - AccessRecord 是"事件流水"，一次到访产生两条（Enter + Leave）；
///   - 访客流水通过 personType=Visitor、personId=visitorId 关联回本档案。
struct Visitor {
    std::string visitorId;   ///< 访客编号（自动生成：YYYY.MM.DD-visitor，同日追加序号）
    std::string name;        ///< 访客姓名（必填）
    std::string phone;       ///< 联系电话（可留空）
    std::string hostOwnerId; ///< 被访业主编号（必填，必须存在于业主系统）
    std::string reason;      ///< 来访事由
    std::string enterTime;   ///< 进入时间（登记时自动取系统时间）
    std::string leaveTime;   ///< 签离时间（未签离为空，签离时自动写入）
    std::string status;      ///< 状态：Visiting=在访 / Left=已离开
    std::string gateId;      ///< 进入门禁点编号（关联 AccessPoint.pointId）
};
