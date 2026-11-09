// ============================================================================
// 文件: OwnerManagementSystem.h
// 职责: 业主信息管理系统。负责业主数据的加载/保存（data/owners.csv）、
//       增删改查与关键字搜索；同时实现 IOwnerQuery 接口，
//       供物业、门禁等子系统校验业主是否存在。
// ============================================================================
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "owner_system/IOwnerQuery.h"

/// 业主管理系统（final）
/// 职责：业主档案的"数据存储层 + 业务校验层"：
///       - 启动时从 CSV 加载全部业主，内存中持有容器；
///       - 提供增/改/删/查与关键字搜索，新增时自动编号并校验账号唯一性；
///       - 每次变更成功即落盘到 CSV；
///       - 作为 IOwnerQuery 实现，向其他子系统暴露只读查询。
class OwnerManagementSystem final : public IOwnerQuery {
private:
    std::vector<Owner> owners_; ///< 内存中的业主列表
    std::string file_;          ///< 业主数据文件路径

    /// 根据现有业主编号生成下一个 Oxxxx 编号
    std::string generateOwnerId() const;

public:
    /// 构造函数：指定数据文件路径并立即加载全部业主
    explicit OwnerManagementSystem(std::string file = "data/owners.csv");

    /// 从 CSV 文件加载全部业主到内存（覆盖式刷新）
    void load();
    /// 将内存中的业主列表全部写回 CSV 文件
    bool save() const;
    /// 新增业主（自动生成编号，校验账号非空且不重复）
    /// 成功返回生成的编号，失败返回 std::nullopt
    std::optional<std::string> addOwner(const Owner& owner);
    /// 按编号覆盖更新业主信息（不存在则失败）
    bool updateOwner(const Owner& owner);
    /// 按编号删除业主（不存在则失败）
    bool removeOwner(const std::string& ownerId);

    /// 按编号查找业主（实现 IOwnerQuery 接口）
    std::optional<Owner> findOwnerById(
        const std::string& ownerId) const override;
    /// 按 编号/姓名/房号 关键字模糊搜索业主
    std::vector<Owner> search(const std::string& keyword) const;
    /// 获取全部业主列表（只读引用，避免拷贝）
    const std::vector<Owner>& all() const;
};
