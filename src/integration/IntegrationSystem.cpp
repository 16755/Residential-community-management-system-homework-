// ============================================================================
// 文件: IntegrationSystem.cpp
// 职责: 实现系统集成与主控：初始化四大子系统、登录循环、
//       以及按角色分发到四个功能菜单（命令行交互层）。
// ============================================================================
#include "integration/IntegrationSystem.h"

#include <iostream>
#include <limits>
#include <optional>
#include <utility>

#include "models/Owner.h"

/// 构造函数：按固定数据路径初始化四个子系统
/// 逻辑：业主/物业/门禁系统在构造时即加载数据；
///       accounts_ 构造后再 load() 并立即 save() 一次，
///       确保内置管理员账号落盘存在。
IntegrationSystem::IntegrationSystem()
    : owners_("data/owners.csv"),
      accounts_("data/accounts.csv"),
      property_(owners_, "data"),
      access_(owners_, "data") {
    accounts_.load(owners_);
    accounts_.save();
}

/// 读取一行用户输入：打印提示语后从标准输入读取
/// @param prompt 显示给用户的提示文本
/// @return 用户输入的一行内容
std::string IntegrationSystem::input(const std::string& prompt) {
    std::cout << prompt;
    std::string value;
    std::getline(std::cin, value);
    return value;
}

/// 业主菜单：业主登录后可见
/// 权限：仅业主角色可进入
/// 功能：查看本人资料、查看物业费与欠费、查看人员出入记录、
///       按车牌查询车辆出入记录并查看本人车位。
void IntegrationSystem::ownerMenu() {
    // 权限校验：非业主直接拒绝
    if (!currentUser_ || currentUser_->role() != Role::Owner) {
        std::cout << "权限不足。\n";
        return;
    }

    // 从多态 User 指针还原为 Owner，以读取房产字段
    const std::shared_ptr<Owner> owner =
        std::dynamic_pointer_cast<Owner>(currentUser_);
    if (!owner) {
        std::cout << "无法读取当前业主资料。\n";
        return;
    }

    while (true) {
        std::cout << "\n业主菜单\n"
                  << "1. 查看本人资料\n"
                  << "2. 查看物业费\n"
                  << "3. 查看人员出入记录\n"
                  << "4. 查看车辆和车位信息\n"
                  << "0. 返回\n";
        const std::string choice = input("请选择：");

        if (choice == "0") {
            return; // 返回上一层（登出）
        }

        if (choice == "1") {
            // 展示业主档案文本与联系电话
            std::cout << owner->profile() << "，电话：" << owner->phone()
                      << '\n';
        } else if (choice == "2") {
            // 列出本人全部缴费记录，并给出累计欠费
            const std::vector<FeeRecord> records = property_.queryFees(owner->id());
            for (const FeeRecord& record : records) {
                std::cout << record.month << " " << record.itemId
                          << " 应缴：" << record.due
                          << " 实缴：" << record.paid << '\n';
            }
            std::cout << "当前欠费：" << property_.outstanding(owner->id())
                      << '\n';
        } else if (choice == "3") {
            // 按业主编号查询本人出入记录
            const std::vector<AccessRecord> records =
                access_.queryPerson(owner->id());
            for (const AccessRecord& record : records) {
                std::cout << record.time << " " << record.direction
                          << " " << record.note << '\n';
            }
        } else if (choice == "4") {
            // 输入车牌查询车辆出入记录，仅展示属于本人的记录
            const std::string plate = input("请输入车牌号：");
            const std::vector<VehicleAccessRecord> records =
                access_.queryVehicle(plate);
            for (const VehicleAccessRecord& record : records) {
                if (record.ownerId == owner->id()) {
                    std::cout << record.time << " " << record.direction
                              << " " << record.note << '\n';
                }
            }
            // 展示本人名下的车位
            for (const ParkingRecord& record : access_.parking()) {
                if (record.ownerId == owner->id()) {
                    std::cout << "车位：" << record.spaceId
                              << "，状态：" << record.status << '\n';
                }
            }
        } else {
            std::cout << "无效选项。\n";
        }
    }
}

/// 物业菜单：物业人员/管理员可进入
/// 权限：具备 管理收费 或 管理任务 权限之一
/// 功能：业主查询、缴费登记、缴费查询、任务创建/分配/状态流转/列表。
void IntegrationSystem::propertyMenu() {
    // 权限校验：必须拥有收费或任务管理权限
    if (!currentUser_ ||
        (!currentUser_->hasPermission(Permission::ManageFees) &&
         !currentUser_->hasPermission(Permission::ManageTasks))) {
        std::cout << "权限不足。\n";
        return;
    }

    while (true) {
        std::cout << "\n物业管理菜单\n"
                  << "1. 查询业主\n"
                  << "2. 登记物业费\n"
                  << "3. 查询业主物业费\n"
                  << "4. 创建物业任务\n"
                  << "5. 分配物业任务\n"
                  << "6. 更新任务状态\n"
                  << "7. 列出全部任务\n"
                  << "0. 返回\n";
        const std::string choice = input("请选择：");

        if (choice == "0") {
            return;
        }

        if (choice == "1") {
            // 按 编号/姓名/房号 关键字搜索业主
            const std::string keyword = input("请输入编号、姓名或房号：");
            for (const Owner& owner : owners_.search(keyword)) {
                std::cout << owner.profile() << "，电话：" << owner.phone()
                          << '\n';
            }
        } else if (choice == "2") {
            // 逐项录入缴费信息；金额用 stod 解析，失败则登记失败
            FeeRecord record;
            record.ownerId = input("业主编号：");
            record.itemId = input("收费项目编号：");
            record.month = input("计费月份：");
            bool validDue = false;
            bool validPaid = false;
            try {
                record.due = std::stod(input("应缴金额："));
                validDue = true;
                record.paid = std::stod(input("实缴金额："));
                validPaid = true;
            } catch (...) {
                validDue = false;
                validPaid = false;
            }
            record.paymentDate = input("缴费日期：");
            const bool success = validDue && validPaid &&
                                 property_.recordPayment(record);
            std::cout << (success ? "登记成功。\n" : "登记失败。\n");
        } else if (choice == "3") {
            // 按业主编号列出缴费记录与欠费合计
            const std::string ownerId = input("业主编号：");
            for (const FeeRecord& record : property_.queryFees(ownerId)) {
                std::cout << record.month << " " << record.itemId
                          << " 应缴：" << record.due
                          << " 实缴：" << record.paid << '\n';
            }
            std::cout << "欠费合计：" << property_.outstanding(ownerId)
                      << '\n';
        } else if (choice == "4") {
            // 录入任务基本信息（初始状态为 Unassigned）
            PropertyTask task;
            task.id = input("任务编号：");
            task.title = input("标题：");
            task.description = input("描述：");
            task.type = input("类型：");
            task.createdAt = input("创建时间：");
            std::cout << (property_.createTask(task) ? "创建成功。\n"
                                                     : "创建失败。\n");
        } else if (choice == "5") {
            // 为任务指定负责人
            const std::string taskId = input("任务编号：");
            const std::string staff = input("负责人账号：");
            std::cout << (property_.assignTask(taskId, staff) ? "分配成功。\n"
                                                              : "分配失败。\n");
        } else if (choice == "6") {
            // 更新任务状态：仅允许按 Unassigned->Assigned->InProgress->Completed 顺序推进
            const std::string taskId = input("任务编号：");
            const std::string statusValue =
                input("目标状态（Assigned/InProgress/Completed）：");
            TaskStatus status;
            const bool validStatus = parseTaskStatus(statusValue, status);
            const bool success = validStatus &&
                                 property_.updateTaskStatus(taskId, status);
            std::cout << (success ? "更新成功。\n" : "更新失败。\n");
        } else if (choice == "7") {
            // 列出全部任务及其状态、负责人
            for (const PropertyTask& task : property_.queryTasks()) {
                std::cout << task.id << " " << task.title << " "
                          << taskStatusToString(task.status)
                          << " 负责人：" << task.assignee << '\n';
            }
        } else {
            std::cout << "无效选项。\n";
        }
    }
}

/// 门禁菜单：门禁人员/管理员可进入
/// 权限：具备 管理门禁 权限
/// 功能：人员出入登记、车辆出入登记、车位分配/释放、记录查询。
void IntegrationSystem::accessMenu() {
    // 权限校验：必须拥有门禁管理权限
    if (!currentUser_ ||
        !currentUser_->hasPermission(Permission::ManageAccess)) {
        std::cout << "权限不足。\n";
        return;
    }

    while (true) {
        std::cout << "\n门禁管理菜单\n"
                  << "1. 登记人员出入\n"
                  << "2. 登记车辆出入\n"
                  << "3. 分配车位\n"
                  << "4. 释放车位\n"
                  << "5. 查询人员记录\n"
                  << "6. 查询车辆记录\n"
                  << "7. 列出车位\n"
                  << "0. 返回\n";
        const std::string choice = input("请选择：");

        if (choice == "0") {
            return;
        }

        if (choice == "1") {
            // 录入一次人员出入记录
            AccessRecord record;
            record.id = input("记录编号：");
            record.personId = input("人员编号：");
            record.personType = input("人员类型：");
            record.direction = input("方向（Enter/Leave）：");
            record.time = input("时间：");
            record.note = input("备注：");
            std::cout << (access_.registerPerson(record) ? "登记成功。\n"
                                                        : "登记失败。\n");
        } else if (choice == "2") {
            // 录入一次车辆出入记录
            VehicleAccessRecord record;
            record.id = input("记录编号：");
            record.plateNumber = input("车牌号：");
            record.ownerId = input("关联业主编号：");
            record.direction = input("方向（Enter/Leave）：");
            record.time = input("时间：");
            record.note = input("备注：");
            std::cout << (access_.registerVehicle(record) ? "登记成功。\n"
                                                         : "登记失败。\n");
        } else if (choice == "3") {
            // 录入车位绑定信息（车牌 + 业主）
            ParkingRecord record;
            record.spaceId = input("车位编号：");
            record.plateNumber = input("车牌号：");
            record.ownerId = input("关联业主编号：");
            record.registeredAt = input("登记时间：");
            std::cout << (access_.assignParking(record) ? "分配成功。\n"
                                                       : "分配失败。\n");
        } else if (choice == "4") {
            // 释放指定车位
            const std::string spaceId = input("车位编号：");
            std::cout << (access_.releaseParking(spaceId) ? "释放成功。\n"
                                                         : "释放失败。\n");
        } else if (choice == "5") {
            // 按人员编号查询出入记录
            const std::string personId = input("人员编号：");
            for (const AccessRecord& record : access_.queryPerson(personId)) {
                std::cout << record.id << " " << record.time << " "
                          << record.direction << " " << record.note << '\n';
            }
        } else if (choice == "6") {
            // 按车牌号查询出入记录
            const std::string plate = input("车牌号：");
            for (const VehicleAccessRecord& record :
                 access_.queryVehicle(plate)) {
                std::cout << record.id << " " << record.time << " "
                          << record.direction << " " << record.note << '\n';
            }
        } else if (choice == "7") {
            // 列出全部车位及其占用状态
            for (const ParkingRecord& record : access_.parking()) {
                std::cout << record.spaceId << " " << record.plateNumber
                          << " " << record.ownerId << " " << record.status
                          << '\n';
            }
        } else {
            std::cout << "无效选项。\n";
        }
    }
}

/// 管理员菜单：管理员专属
/// 权限：仅 Admin 角色可进入
/// 功能：业主新增/删除/列表、工作人员账号新增，
///       并可一键进入物业与门禁菜单。
void IntegrationSystem::adminMenu() {
    // 权限校验：仅管理员角色
    if (!currentUser_ || currentUser_->role() != Role::Admin) {
        std::cout << "权限不足。\n";
        return;
    }

    while (true) {
        std::cout << "\n管理员菜单\n"
                  << "1. 添加业主\n"
                  << "2. 删除业主\n"
                  << "3. 列出业主\n"
                  << "4. 添加工作人员账号\n"
                  << "5. 进入物业管理\n"
                  << "6. 进入门禁管理\n"
                  << "0. 返回\n";
        const std::string choice = input("请选择：");

        if (choice == "0") {
            return;
        }

        if (choice == "1") {
            // 逐项录入业主信息（房产 + 登录账号），交给业主系统校验并保存
            const std::string id = input("业主编号：");
            const std::string name = input("姓名：");
            const std::string phone = input("电话：");
            const std::string building = input("楼栋：");
            const std::string unit = input("单元：");
            const std::string room = input("房号：");
            const std::string username = input("账号：");
            const std::string password = input("密码：");
            const Owner owner(id, name, phone, building, unit, room,
                              username, password);
            std::cout << (owners_.addOwner(owner) ? "添加成功。\n"
                                                  : "添加失败。\n");
        } else if (choice == "2") {
            // 按编号删除业主
            const std::string ownerId = input("业主编号：");
            std::cout << (owners_.removeOwner(ownerId) ? "删除成功。\n"
                                                       : "删除失败。\n");
        } else if (choice == "3") {
            // 列出全部业主
            for (const Owner& owner : owners_.all()) {
                std::cout << owner.profile() << "，电话：" << owner.phone()
                          << '\n';
            }
        } else if (choice == "4") {
            // 新增工作人员账号：先解析角色文本，再交账号服务校验保存
            const std::string username = input("账号：");
            const std::string password = input("密码：");
            const std::string name = input("姓名：");
            const std::string roleValue =
                input("角色（Admin/PropertyStaff/AccessStaff）：");
            Role role = Role::AccessStaff;
            bool validRole = true;
            if (roleValue == "Admin") {
                role = Role::Admin;
            } else if (roleValue == "PropertyStaff") {
                role = Role::PropertyStaff;
            } else if (roleValue == "AccessStaff") {
                role = Role::AccessStaff;
            } else {
                validRole = false; // 无法识别的角色文本
            }
            const bool success = validRole &&
                                 accounts_.addStaff(username, password, name,
                                                    role);
            std::cout << (success ? "账号创建成功。\n" : "账号创建失败。\n");
        } else if (choice == "5") {
            propertyMenu(); // 管理员代管物业菜单
        } else if (choice == "6") {
            accessMenu();   // 管理员代管门禁菜单
        } else {
            std::cout << "无效选项。\n";
        }
    }
}

/// 程序主循环：登录 -> 按角色进入菜单 -> 登出
/// 逻辑：循环读取 用户名/密码；输入 0 退出程序；
///       authenticate() 校验通过后按角色分发到对应菜单；
///       菜单返回后清空当前用户（登出），等待下一次登录。
void IntegrationSystem::run() {
    std::cout << "小区管理系统\n";

    while (true) {
        const std::string username = input("用户名（输入 0 退出）：");
        if (username == "0") {
            return; // 退出程序
        }

        const std::string password = input("密码：");
        if (!accounts_.authenticate(username, password, owners_, currentUser_)) {
            std::cout << "用户名或密码错误。\n";
            continue; // 认证失败，重新登录
        }

        std::cout << "登录成功：" << currentUser_->profile() << '\n';
        // 按角色分发到对应功能菜单
        if (currentUser_->role() == Role::Owner) {
            ownerMenu();
        } else if (currentUser_->role() == Role::PropertyStaff) {
            propertyMenu();
        } else if (currentUser_->role() == Role::AccessStaff) {
            accessMenu();
        } else if (currentUser_->role() == Role::Admin) {
            adminMenu();
        }
        currentUser_.reset(); // 菜单返回后登出
    }
}
