// ============================================================================
// 文件: AccessManagementSystem.h
// 职责: 门禁管理系统。管理五类数据：门禁点、人员出入记录、车辆出入
//       记录、车位占用情况、访客档案；提供门禁点登记/停用、出入登记、
//       访客登记/签离、车位分配/释放、按人/按车/在访查询能力。
//       通过 IOwnerQuery 接口校验关联业主是否存在。
// ============================================================================
#pragma once

#include <string>
#include <vector>

#include "models/AccessPoint.h"
#include "models/AccessRecord.h"
#include "models/ParkingRecord.h"
#include "models/VehicleAccessRecord.h"
#include "models/Visitor.h"
#include "owner_system/IOwnerQuery.h"

/// 门禁管理系统
/// 职责：小区门禁域的"数据存储层 + 业务校验层"：
///       - 加载/保存 5 个 CSV（门禁点、人员出入、车辆出入、车位、访客档案）；
///       - 登记门禁点时校验编号不重复，可停用而不删除；
///       - 登记业主出入记录时校验业主存在、记录编号不重复、门禁点有效；
///       - 登记访客到访时自动生成访客编号、建档并写入 Enter 流水（含门禁点）；
///       - 访客签离时更新档案状态并写入 Leave 流水（档案保留）；
///       - 车辆出入还需校验门禁点允许车辆通行；
///       - 分配/释放车位维护车位状态（Occupied/Released）；
///       - 对外提供按人员、按车辆、在访访客、门禁点的查询能力。
class AccessManagementSystem {
private:
    const IOwnerQuery& owners_;                 ///< 业主查询接口（校验业主存在性）
    std::vector<AccessPoint> points_;           ///< 门禁点列表
    std::vector<AccessRecord> people_;          ///< 人员出入记录列表
    std::vector<VehicleAccessRecord> vehicles_; ///< 车辆出入记录列表
    std::vector<ParkingRecord> parking_;        ///< 车位记录列表
    std::vector<Visitor> visitors_;            ///< 访客档案列表（每次到访一条）
    std::string pointsFile_;                    ///< 门禁点文件路径
    std::string peopleFile_;                    ///< 人员出入记录文件路径
    std::string vehicleFile_;                   ///< 车辆出入记录文件路径
    std::string parkingFile_;                   ///< 车位记录文件路径
    std::string visitorFile_;                   ///< 访客档案文件路径

    /// 查找编号匹配且处于启用状态的门禁点（不存在返回 nullptr）
    const AccessPoint* findEnabledPoint(const std::string& pointId) const;
    /// 按当前日期生成唯一访客编号（YYYY.MM.DD-visitor，同日重复追加序号）
    std::string generateVisitorId() const;

public:
    /// 构造函数：绑定业主查询接口与数据目录，并立即加载全部数据
    explicit AccessManagementSystem(const IOwnerQuery& owners,
                                    std::string dataDirectory = "data");

    /// 从 5 个 CSV 文件加载全部数据到内存
    void load();
    /// 将内存中的全部数据写回 5 个 CSV 文件
    bool save() const;

    /// 登记一个门禁点（校验编号非空且唯一）
    bool addAccessPoint(const AccessPoint& point);
    /// 停用门禁点（置为 Disabled，保留记录以便历史追溯）
    bool disableAccessPoint(const std::string& pointId);
    /// 获取全部门禁点（只读引用，避免拷贝）
    const std::vector<AccessPoint>& accessPoints() const;

    /// 登记一次业主出入（仅接受 personType=Owner，校验业主存在、门禁点有效）
    bool registerPerson(const AccessRecord& record);
    /// 登记访客到访：自动编号、建档（Visiting）并写入一条 Enter 流水
    /// 调用方只需填写 name/phone/hostOwnerId/reason/gateId，其余由系统生成
    bool registerPerson(const Visitor& visitor);
    /// 访客签离：将档案置为 Left、写签离时间，并写入一条 Leave 流水
    /// 签离时间自动取系统时间；gateId 为签离门禁点；档案保留不删除
    bool visitorCheckout(const std::string& visitorId,
                         const std::string& gateId);
    /// 登记一次车辆出入（校验门禁点有效且允许车辆通行）
    bool registerVehicle(const VehicleAccessRecord& record);
    /// 分配车位（车位已被占用时拒绝；已登记的车位则更新车牌/业主信息）
    bool assignParking(const ParkingRecord& record);
    /// 释放车位（将占用车位置为 Released 并清空车牌与业主）
    bool releaseParking(const std::string& spaceId);

    /// 按人员编号查询全部出入记录
    std::vector<AccessRecord> queryPerson(
        const std::string& personId) const;
    /// 按车牌号查询全部出入记录
    std::vector<VehicleAccessRecord> queryVehicle(
        const std::string& plateNumber) const;
    /// 查询当前所有在访（Visiting）访客
    std::vector<Visitor> queryVisiting() const;
    /// 获取全部车位记录
    const std::vector<ParkingRecord>& parking() const;
    /// 获取全部访客档案
    const std::vector<Visitor>& visitors() const;
};
