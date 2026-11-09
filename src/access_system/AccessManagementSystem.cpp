// ============================================================================
// 文件: AccessManagementSystem.cpp
// 职责: 实现门禁管理系统的数据加载/保存、门禁点登记/停用、业主/访客
//       出入登记、访客签离、车位分配与查询。
// ============================================================================
#include "access_system/AccessManagementSystem.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

#include "common/CsvUtils.h"

namespace {
struct DateTimeStamp {
    std::string display; ///< YYYY.MM.DD HH:MM:SS
    std::string compact; ///< YYYYMMDDHHMMSS
};

/// 一次取时，同时生成显示时间与编号时间，避免跨秒不一致
DateTimeStamp currentDateTime() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char displayBuffer[32];
    char compactBuffer[20];
    std::strftime(displayBuffer, sizeof(displayBuffer),
                  "%Y.%m.%d %H:%M:%S", &local);
    std::strftime(compactBuffer, sizeof(compactBuffer), "%Y%m%d%H%M%S",
                  &local);
    return {displayBuffer, compactBuffer};
}

/// 在已有编号中查找可用编号；基础编号冲突时追加 -2、-3……
template <typename Record, typename IdGetter>
std::string uniqueId(const std::string& base,
                     const std::vector<Record>& records,
                     IdGetter getId) {
    std::string candidate = base;
    int suffix = 2;
    while (std::any_of(records.begin(), records.end(),
                       [&](const Record& record) {
                           return getId(record) == candidate;
                       })) {
        candidate = base + "-" + std::to_string(suffix++);
    }
    return candidate;
}
} // namespace

/// 构造函数：绑定业主查询接口，拼接五个数据文件路径并立即加载
AccessManagementSystem::AccessManagementSystem(
    const IOwnerQuery& owners, std::string dataDirectory)
    : owners_(owners),
      pointsFile_(dataDirectory + "/access_points.csv"),
      peopleFile_(dataDirectory + "/access_records.csv"),
      vehicleFile_(dataDirectory + "/vehicle_access_records.csv"),
      parkingFile_(dataDirectory + "/parking_records.csv"),
      visitorFile_(dataDirectory + "/visitors.csv") {
    load();
}

/// 从 5 个 CSV 文件加载全部数据
/// 逻辑：分别跳过各文件表头，按固定字段顺序构造记录：
///       门禁点 5 列、人员出入 7 列（旧文件 6 列，gateId 留空）、
///       车辆出入 7 列（旧文件 6 列，gateId 留空）、车位 5 列、
///       访客档案 9 列（旧文件 8 列，gateId 留空）。
void AccessManagementSystem::load() {
    points_.clear();
    people_.clear();
    vehicles_.clear();
    parking_.clear();
    visitors_.clear();

    std::string line;

    std::ifstream pointInput(pointsFile_);
    std::getline(pointInput, line); // 跳过表头
    while (std::getline(pointInput, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        if (fields.size() >= 5) {
            points_.push_back(
                {fields[0], fields[1], fields[2], fields[3], fields[4]});
        }
    }

    std::ifstream peopleInput(peopleFile_);
    std::getline(peopleInput, line);
    while (std::getline(peopleInput, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        if (fields.size() >= 6) {
            AccessRecord record{fields[0], fields[1], fields[2],
                                fields[3], fields[4], fields[5]};
            if (fields.size() >= 7) {
                record.gateId = fields[6]; // 兼容旧文件：无第 7 列时留空
            }
            people_.push_back(record);
        }
    }

    std::ifstream vehicleInput(vehicleFile_);
    std::getline(vehicleInput, line);
    while (std::getline(vehicleInput, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        if (fields.size() >= 6) {
            VehicleAccessRecord record{fields[0], fields[1], fields[2],
                                       fields[3], fields[4], fields[5]};
            if (fields.size() >= 7) {
                record.gateId = fields[6]; // 兼容旧文件：无第 7 列时留空
            }
            vehicles_.push_back(record);
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

    std::ifstream visitorInput(visitorFile_);
    std::getline(visitorInput, line); // 跳过表头
    while (std::getline(visitorInput, line)) {
        const std::vector<std::string> fields = splitCsv(line);
        if (fields.size() >= 8) {
            Visitor visitor{fields[0], fields[1], fields[2], fields[3],
                            fields[4], fields[5], fields[6], fields[7]};
            if (fields.size() >= 9) {
                visitor.gateId = fields[8]; // 兼容旧文件：无第 9 列时留空
            }
            visitors_.push_back(visitor);
        }
    }
}

/// 将内存中的全部数据写回 5 个 CSV 文件
/// 逻辑：先确保目录存在，再分别写入表头与数据行（字段统一转义）。
bool AccessManagementSystem::save() const {
    if (!ensureParentDirectory(pointsFile_)) {
        return false;
    }

    std::ofstream pointOutput(pointsFile_);
    std::ofstream peopleOutput(peopleFile_);
    std::ofstream vehicleOutput(vehicleFile_);
    std::ofstream parkingOutput(parkingFile_);
    std::ofstream visitorOutput(visitorFile_);
    if (!pointOutput || !peopleOutput || !vehicleOutput || !parkingOutput ||
        !visitorOutput) {
        return false;
    }

    pointOutput << "pointId,name,type,location,status\n";
    for (const AccessPoint& point : points_) {
        pointOutput << csvSafe(point.pointId) << ','
                    << csvSafe(point.name) << ','
                    << csvSafe(point.type) << ','
                    << csvSafe(point.location) << ','
                    << csvSafe(point.status) << '\n';
    }

    peopleOutput << "id,personId,personType,direction,time,note,gateId\n";
    for (const AccessRecord& record : people_) {
        peopleOutput << csvSafe(record.id) << ','
                     << csvSafe(record.personId) << ','
                     << csvSafe(record.personType) << ','
                     << csvSafe(record.direction) << ','
                     << csvSafe(record.time) << ','
                     << csvSafe(record.note) << ','
                     << csvSafe(record.gateId) << '\n';
    }

    vehicleOutput << "id,plateNumber,ownerId,direction,time,note,gateId\n";
    for (const VehicleAccessRecord& record : vehicles_) {
        vehicleOutput << csvSafe(record.id) << ','
                      << csvSafe(record.plateNumber) << ','
                      << csvSafe(record.ownerId) << ','
                      << csvSafe(record.direction) << ','
                      << csvSafe(record.time) << ','
                      << csvSafe(record.note) << ','
                      << csvSafe(record.gateId) << '\n';
    }

    parkingOutput << "spaceId,plateNumber,ownerId,status,registeredAt\n";
    for (const ParkingRecord& record : parking_) {
        parkingOutput << csvSafe(record.spaceId) << ','
                      << csvSafe(record.plateNumber) << ','
                      << csvSafe(record.ownerId) << ','
                      << csvSafe(record.status) << ','
                      << csvSafe(record.registeredAt) << '\n';
    }

    visitorOutput << "visitorId,name,phone,hostOwnerId,reason,"
                     "enterTime,leaveTime,status,gateId\n";
    for (const Visitor& visitor : visitors_) {
        visitorOutput << csvSafe(visitor.visitorId) << ','
                      << csvSafe(visitor.name) << ','
                      << csvSafe(visitor.phone) << ','
                      << csvSafe(visitor.hostOwnerId) << ','
                      << csvSafe(visitor.reason) << ','
                      << csvSafe(visitor.enterTime) << ','
                      << csvSafe(visitor.leaveTime) << ','
                      << csvSafe(visitor.status) << ','
                      << csvSafe(visitor.gateId) << '\n';
    }

    return true;
}

/// 查找编号匹配且处于启用状态的门禁点
const AccessPoint* AccessManagementSystem::findEnabledPoint(
    const std::string& pointId) const {
    for (const AccessPoint& point : points_) {
        if (point.pointId == pointId && point.status == "Enabled") {
            return &point;
        }
    }
    return nullptr;
}

/// 扫描规范编号 APxxxx，生成最大流水号的下一号
std::string AccessManagementSystem::generateAccessPointId() const {
    unsigned long maxSequence = 0;
    for (const AccessPoint& point : points_) {
        if (point.pointId.rfind("AP", 0) != 0 || point.pointId.size() <= 2) {
            continue;
        }
        try {
            std::size_t parsed = 0;
            const unsigned long sequence =
                std::stoul(point.pointId.substr(2), &parsed);
            if (parsed == point.pointId.size() - 2) {
                maxSequence = std::max(maxSequence, sequence);
            }
        } catch (...) {
            // 非规范历史编号不参与流水号计算
        }
    }

    std::ostringstream output;
    output << "AP" << std::setw(4) << std::setfill('0') << maxSequence + 1;
    return output.str();
}

/// 登记门禁点：编号由系统生成；未指定状态时默认启用。
std::optional<std::string> AccessManagementSystem::addAccessPoint(
    const AccessPoint& point) {
    if (point.name.empty() || point.type.empty() || point.location.empty()) {
        return std::nullopt;
    }

    AccessPoint newPoint = point;
    newPoint.pointId = generateAccessPointId();
    if (newPoint.status.empty()) {
        newPoint.status = "Enabled"; // 未指定状态时默认启用
    }
    points_.push_back(newPoint);
    if (!save()) {
        points_.pop_back();
        return std::nullopt;
    }
    return newPoint.pointId;
}

/// 停用门禁点：仅停用启用中的门禁点，不删除以保留历史追溯
bool AccessManagementSystem::disableAccessPoint(const std::string& pointId) {
    for (AccessPoint& point : points_) {
        if (point.pointId == pointId && point.status == "Enabled") {
            point.status = "Disabled";
            return save();
        }
    }
    return false; // 门禁点不存在或已停用
}

/// 获取全部门禁点（只读引用，避免拷贝）
const std::vector<AccessPoint>& AccessManagementSystem::accessPoints() const {
    return points_;
}

/// 登记一次业主出入：编号和时间由系统使用同一次取时结果生成。
std::optional<std::string> AccessManagementSystem::registerPerson(
    const AccessRecord& record) {
    const bool ownerExists = owners_.findOwnerById(record.personId).has_value();
    const AccessPoint* point = findEnabledPoint(record.gateId);
    if (record.personId.empty() || record.personType != "Owner" ||
        record.direction.empty() || record.gateId.empty() || !ownerExists ||
        point == nullptr) {
        return std::nullopt;
    }

    const DateTimeStamp timestamp = currentDateTime();
    AccessRecord newRecord = record;
    newRecord.id = uniqueId(
        "AR-" + timestamp.compact, people_,
        [](const AccessRecord& current) -> const std::string& {
            return current.id;
        });
    newRecord.time = timestamp.display;
    people_.push_back(newRecord);
    if (!save()) {
        people_.pop_back();
        return std::nullopt;
    }
    return newRecord.id;
}

/// 生成访客编号
/// 规则：基础为 "YYYY.MM.DD-visitor"；同日已存在时追加 -2、-3... 保证唯一。
std::string AccessManagementSystem::generateVisitorId() const {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char dateBuffer[16];
    std::strftime(dateBuffer, sizeof(dateBuffer), "%Y.%m.%d", &local);
    const std::string base = std::string(dateBuffer) + "-visitor";

    std::string candidate = base;
    int suffix = 2;
    bool alreadyUsed = true;
    while (alreadyUsed) {
        alreadyUsed = std::any_of(
            visitors_.begin(), visitors_.end(),
            [&](const Visitor& visitor) {
                return visitor.visitorId == candidate;
            });
        if (alreadyUsed) {
            candidate = base + "-" + std::to_string(suffix++);
        }
    }
    return candidate;
}

/// 登记访客到访（registerPerson 重载）
/// 逻辑：校验姓名/被访业主/门禁点非空、被访业主存在、门禁点有效；
///       自动生成编号、进入时间，置为 Visiting；保存访客档案，并同步
///       写入一条 Enter 流水（继承门禁点）后落盘。
bool AccessManagementSystem::registerPerson(const Visitor& input) {
    if (input.name.empty() || input.hostOwnerId.empty() ||
        input.gateId.empty()) {
        return false;
    }
    if (!owners_.findOwnerById(input.hostOwnerId).has_value()) {
        return false;
    }
    if (findEnabledPoint(input.gateId) == nullptr) {
        return false;
    }

    Visitor visitor = input;
    visitor.visitorId = generateVisitorId();
    visitor.enterTime = currentDateTime().display;
    visitor.leaveTime.clear();
    visitor.status = "Visiting";

    AccessRecord enterRecord;
    enterRecord.id = visitor.visitorId + "-E";
    enterRecord.personId = visitor.visitorId;
    enterRecord.personType = "Visitor";
    enterRecord.direction = "Enter";
    enterRecord.time = visitor.enterTime;
    enterRecord.note = visitor.reason;
    enterRecord.gateId = visitor.gateId;

    visitors_.push_back(visitor);
    people_.push_back(enterRecord);
    return save();
}

/// 访客签离
/// 逻辑：校验签离门禁点有效；找到状态为 Visiting 的访客，置为 Left、
///       写签离时间，同步写入一条 Leave 流水（使用签离门禁点）后落盘；
///       档案保留不删除。
bool AccessManagementSystem::visitorCheckout(const std::string& visitorId,
                                             const std::string& gateId) {
    if (findEnabledPoint(gateId) == nullptr) {
        return false; // 签离门禁点不存在或已停用
    }

    for (Visitor& visitor : visitors_) {
        if (visitor.visitorId == visitorId &&
            visitor.status == "Visiting") {
            visitor.status = "Left";
            visitor.leaveTime = currentDateTime().display;

            AccessRecord leaveRecord;
            leaveRecord.id = visitor.visitorId + "-L";
            leaveRecord.personId = visitor.visitorId;
            leaveRecord.personType = "Visitor";
            leaveRecord.direction = "Leave";
            leaveRecord.time = visitor.leaveTime;
            leaveRecord.note = visitor.reason;
            leaveRecord.gateId = gateId;
            people_.push_back(leaveRecord);
            return save();
        }
    }
    return false; // 访客不存在或已签离
}

/// 登记一次车辆出入：编号和时间由系统使用同一次取时结果生成。
/// 车辆只能通过车库大门或小区大门，楼栋门禁止车辆通行。
std::optional<std::string> AccessManagementSystem::registerVehicle(
    const VehicleAccessRecord& record) {
    const bool ownerExists = owners_.findOwnerById(record.ownerId).has_value();
    const AccessPoint* point = findEnabledPoint(record.gateId);
    const bool vehicleAllowed = point != nullptr &&
        (point->type == "GarageGate" || point->type == "CommunityGate");
    if (record.plateNumber.empty() || record.ownerId.empty() ||
        record.direction.empty() || record.gateId.empty() || !ownerExists ||
        !vehicleAllowed) {
        return std::nullopt;
    }

    const DateTimeStamp timestamp = currentDateTime();
    VehicleAccessRecord newRecord = record;
    newRecord.id = uniqueId(
        "VR-" + timestamp.compact, vehicles_,
        [](const VehicleAccessRecord& current) -> const std::string& {
            return current.id;
        });
    newRecord.time = timestamp.display;
    vehicles_.push_back(newRecord);
    if (!save()) {
        vehicles_.pop_back();
        return std::nullopt;
    }
    return newRecord.id;
}

/// 分配车位
/// 编号基于车牌生成；同车牌已占用则拒绝，已释放则复用原编号。
std::optional<std::string> AccessManagementSystem::assignParking(
    const ParkingRecord& record) {
    const bool ownerExists = owners_.findOwnerById(record.ownerId).has_value();
    if (record.plateNumber.empty() || record.ownerId.empty() ||
        record.registeredAt.empty() || !ownerExists) {
        return std::nullopt;
    }

    for (ParkingRecord& current : parking_) {
        if (current.plateNumber != record.plateNumber) {
            continue;
        }
        if (current.status == "Occupied") {
            return std::nullopt;
        }
        if (current.status == "Released") {
            const ParkingRecord oldRecord = current;
            current.ownerId = record.ownerId;
            current.status = "Occupied";
            current.registeredAt = record.registeredAt;
            if (!save()) {
                current = oldRecord;
                return std::nullopt;
            }
            return current.spaceId;
        }
    }

    ParkingRecord newRecord = record;
    newRecord.spaceId = uniqueId(
        "PK-" + record.plateNumber, parking_,
        [](const ParkingRecord& current) -> const std::string& {
            return current.spaceId;
        });
    newRecord.status = "Occupied";
    parking_.push_back(newRecord);
    if (!save()) {
        parking_.pop_back();
        return std::nullopt;
    }
    return newRecord.spaceId;
}

/// 释放车位：置为 Released，保留车牌以便同车牌再次分配时复用编号
bool AccessManagementSystem::releaseParking(const std::string& spaceId) {
    for (ParkingRecord& record : parking_) {
        if (record.spaceId == spaceId && record.status == "Occupied") {
            record.status = "Released";
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

/// 查询当前所有在访（Visiting）访客
std::vector<Visitor> AccessManagementSystem::queryVisiting() const {
    std::vector<Visitor> result;
    for (const Visitor& visitor : visitors_) {
        if (visitor.status == "Visiting") {
            result.push_back(visitor);
        }
    }
    return result;
}

/// 获取全部访客档案（只读引用，避免拷贝）
const std::vector<Visitor>& AccessManagementSystem::visitors() const {
    return visitors_;
}
