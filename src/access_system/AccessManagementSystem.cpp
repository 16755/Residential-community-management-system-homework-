// ============================================================================
// 文件: AccessManagementSystem.cpp
// 职责: 实现门禁管理系统的数据加载/保存、出入登记、车位分配与查询。
// ============================================================================
#include "access_system/AccessManagementSystem.h"

#include <algorithm>
#include <fstream>
#include <utility>

#include "common/CsvUtils.h"

/// 构造函数：绑定业主查询接口，拼接三个数据文件路径并立即加载
AccessManagementSystem::AccessManagementSystem(
    const IOwnerQuery& owners, std::string dataDirectory)
    : owners_(owners),
      peopleFile_(dataDirectory + "/access_records.csv"),
      vehicleFile_(dataDirectory + "/vehicle_access_records.csv"),
      parkingFile_(dataDirectory + "/parking_records.csv") {
    load();
}

/// 从 3 个 CSV 文件加载全部数据
/// 逻辑：分别跳过各文件表头，按固定字段顺序构造记录：
///       人员出入 6 列、车辆出入 6 列、车位 5 列。
void AccessManagementSystem::load() {
    people_.clear();
    vehicles_.clear();
    parking_.clear();

    std::string line;
    std::ifstream peopleInput(peopleFile_);
    std::getline(peopleInput, line); // 跳过表头
    while (std::getline(peopleInput, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        if (fields.size() >= 6) {
            people_.push_back(
                {fields[0], fields[1], fields[2], fields[3], fields[4], fields[5]});
        }
    }

    std::ifstream vehicleInput(vehicleFile_);
    std::getline(vehicleInput, line);
    while (std::getline(vehicleInput, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        if (fields.size() >= 6) {
            vehicles_.push_back(
                {fields[0], fields[1], fields[2], fields[3], fields[4], fields[5]});
        }
    }

    std::ifstream parkingInput(parkingFile_);
    std::getline(parkingInput, line);
    while (std::getline(parkingInput, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        if (fields.size() >= 5) {
            parking_.push_back(
                {fields[0], fields[1], fields[2], fields[3], fields[4]});
        }
    }
}

/// 将内存中的全部数据写回 3 个 CSV 文件
/// 逻辑：先确保目录存在，再分别写入表头与数据行（字段统一转义）。
bool AccessManagementSystem::save() const {
    if (!ensureParentDirectory(peopleFile_)) {
        return false;
    }

    std::ofstream peopleOutput(peopleFile_);
    std::ofstream vehicleOutput(vehicleFile_);
    std::ofstream parkingOutput(parkingFile_);
    if (!peopleOutput || !vehicleOutput || !parkingOutput) {
        return false;
    }

    peopleOutput << "id,personId,personType,direction,time,note\n";
    for (const AccessRecord& record : people_) {
        peopleOutput << csvSafe(record.id) << ','
                     << csvSafe(record.personId) << ','
                     << csvSafe(record.personType) << ','
                     << csvSafe(record.direction) << ','
                     << csvSafe(record.time) << ','
                     << csvSafe(record.note) << '\n';
    }

    vehicleOutput << "id,plateNumber,ownerId,direction,time,note\n";
    for (const VehicleAccessRecord& record : vehicles_) {
        vehicleOutput << csvSafe(record.id) << ','
                      << csvSafe(record.plateNumber) << ','
                      << csvSafe(record.ownerId) << ','
                      << csvSafe(record.direction) << ','
                      << csvSafe(record.time) << ','
                      << csvSafe(record.note) << '\n';
    }

    parkingOutput << "spaceId,plateNumber,ownerId,status,registeredAt\n";
    for (const ParkingRecord& record : parking_) {
        parkingOutput << csvSafe(record.spaceId) << ','
                      << csvSafe(record.plateNumber) << ','
                      << csvSafe(record.ownerId) << ','
                      << csvSafe(record.status) << ','
                      << csvSafe(record.registeredAt) << '\n';
    }

    return true;
}

/// 登记一次人员出入
/// 校验：编号/人员编号/类型/方向/时间非空、记录编号唯一、
///       关联业主必须存在于业主系统；通过后追加并落盘。
bool AccessManagementSystem::registerPerson(const AccessRecord& record) {
    const bool duplicate = std::any_of(
        people_.begin(), people_.end(), [&](const AccessRecord& current) {
            return current.id == record.id;
        });
    const bool ownerExists = owners_.findOwnerById(record.personId).has_value();
    if (record.id.empty() || record.personId.empty() || record.personType.empty() ||
        record.direction.empty() || record.time.empty() || duplicate || !ownerExists) {
        return false;
    }

    people_.push_back(record);
    return save();
}

/// 登记一次车辆出入
/// 校验：编号/车牌/业主编号/方向/时间非空、记录编号唯一、
///       关联业主必须存在；通过后追加并落盘。
bool AccessManagementSystem::registerVehicle(
    const VehicleAccessRecord& record) {
    const bool duplicate = std::any_of(
        vehicles_.begin(), vehicles_.end(),
        [&](const VehicleAccessRecord& current) {
            return current.id == record.id;
        });
    const bool ownerExists = owners_.findOwnerById(record.ownerId).has_value();
    if (record.id.empty() || record.plateNumber.empty() || record.ownerId.empty() ||
        record.direction.empty() || record.time.empty() || duplicate || !ownerExists) {
        return false;
    }

    vehicles_.push_back(record);
    return save();
}

/// 分配车位
/// 逻辑：校验业主存在且关键字段非空；车位已被占用（Occupied）则拒绝；
///       若该车位已有记录则更新其车牌/业主并保持占用，否则新增记录。
bool AccessManagementSystem::assignParking(const ParkingRecord& record) {
    const bool ownerExists = owners_.findOwnerById(record.ownerId).has_value();
    if (record.spaceId.empty() || record.plateNumber.empty() ||
        record.ownerId.empty() || record.registeredAt.empty() || !ownerExists) {
        return false;
    }

    // 车位被其他车辆占用时拒绝分配
    for (const ParkingRecord& current : parking_) {
        if (current.spaceId == record.spaceId && current.status == "Occupied") {
            return false;
        }
    }

    ParkingRecord newRecord = record;
    newRecord.status = "Occupied";
    // 车位已登记过（如曾释放）：原地更新，不新增重复记录
    for (ParkingRecord& current : parking_) {
        if (current.spaceId == record.spaceId) {
            current = newRecord;
            return save();
        }
    }

    parking_.push_back(newRecord);
    return save();
}

/// 释放车位：找到占用中的车位，置为 Released 并清空车牌与业主
bool AccessManagementSystem::releaseParking(const std::string& spaceId) {
    for (ParkingRecord& record : parking_) {
        if (record.spaceId == spaceId && record.status == "Occupied") {
            record.status = "Released";
            record.plateNumber.clear();
            record.ownerId.clear();
            return save();
        }
    }
    return false; // 车位不存在或未被占用
}

/// 按人员编号查询全部出入记录（线性过滤）
std::vector<AccessRecord> AccessManagementSystem::queryPerson(
    const std::string& personId) const {
    std::vector<AccessRecord> result;
    for (const AccessRecord& record : people_) {
        if (record.personId == personId) {
            result.push_back(record);
        }
    }
    return result;
}

/// 按车牌号查询全部出入记录（线性过滤）
std::vector<VehicleAccessRecord> AccessManagementSystem::queryVehicle(
    const std::string& plateNumber) const {
    std::vector<VehicleAccessRecord> result;
    for (const VehicleAccessRecord& record : vehicles_) {
        if (record.plateNumber == plateNumber) {
            result.push_back(record);
        }
    }
    return result;
}

/// 获取全部车位记录（只读引用，避免拷贝）
const std::vector<ParkingRecord>& AccessManagementSystem::parking() const {
    return parking_;
}
