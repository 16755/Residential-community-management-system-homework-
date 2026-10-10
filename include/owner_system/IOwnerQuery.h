// ============================================================================
// 文件: IOwnerQuery.h
// 职责: 定义业主查询抽象接口。物业、门禁等子系统只依赖本接口
//       （而非 OwnerManagementSystem 具体类），实现依赖倒置，
//       便于解耦与测试。
// ============================================================================
#pragma once

#include <optional>
#include <string>

#include "models/Owner.h"

/// 业主查询接口：向其他子系统暴露"按编号查询业主"的只读能力
class IOwnerQuery {
public:
    virtual ~IOwnerQuery() = default;

    /// 按业主编号查询业主
    /// @param ownerId 业主编号
    /// @return 找到则返回该业主的副本，否则返回 std::nullopt
    virtual std::optional<Owner> findOwnerById(
        const std::string& ownerId) const = 0;
};
