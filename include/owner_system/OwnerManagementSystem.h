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

class AccountService;

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
    const AccountService* accounts_ = nullptr; ///< 全局账号唯一性校验来源

    /// 根据现有业主编号生成下一个 Oxxxx 编号
    std::string generateOwnerId() const;
    /// 按预期的前置状态执行单向状态流转，并在保存失败时回滚。
    bool changeStatus(const std::string& ownerId,
                      OwnerStatus expected,
                      OwnerStatus target);

public:
    /// 构造函数：指定数据文件路径并立即加载全部业主
    explicit OwnerManagementSystem(std::string file = "data/owners.csv");

    /// 绑定账号服务，使业主新增和修改能够检查工作人员账号。
    void bindAccountService(const AccountService& accounts);
    /// 查询业主账号是否已存在；可排除正在修改的业主编号。
    bool hasUsername(const std::string& username,
                     const std::string& excludeOwnerId = "") const;

    /// 从 CSV 文件加载全部业主到内存（覆盖式刷新）
    void load();
    /// 将内存中的业主列表全部写回 CSV 文件
    bool save() const;
    /// 新增业主（自动生成编号并统一登记为 Pending）
    /// 成功返回生成的编号，失败返回 std::nullopt
    std::optional<std::string> addOwner(const Owner& owner);
    /// 按编号覆盖更新业主信息（不存在则失败）
    bool updateOwner(const Owner& owner);
    /// 审核待审核业主：仅允许 Pending -> Active
    bool approveOwner(const std::string& ownerId);
    /// 办理业主迁出：仅允许 Active -> MovedOut
    bool moveOutOwner(const std::string& ownerId);
    /// 兼容旧调用；不再物理删除，等价于办理迁出
    bool removeOwner(const std::string& ownerId);

    /// 按编号查找业主（实现 IOwnerQuery 接口）
    std::optional<Owner> findOwnerById(
        const std::string& ownerId) const override;
    /// 按 编号/姓名/房号 关键字模糊搜索业主
    std::vector<Owner> search(const std::string& keyword) const;
    /// 获取全部业主列表（只读引用，避免拷贝）
    const std::vector<Owner>& all() const;
};
