// ============================================================================
// 文件: CsvUtils.cpp
// 职责: 实现 CSV 通用工具函数，接口语义详见 CsvUtils.h 的声明注释。
// ============================================================================
#include "common/CsvUtils.h"

#include <filesystem>

/// 按 CSV 规则拆分一行文本为字段列表
/// 逻辑：逐字符扫描，双引号包裹的字段内不做切分，
///       "" 转义被还原为单个引号，未包裹的逗号作为字段分隔符。
std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char current = line[i];
        if (current == '"') {
            // 双引号内的连续双引号 "" 表示一个转义引号
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                field.push_back('"');
                ++i;
            } else {
                quoted = !quoted; // 进入/离开引号包裹状态
            }
        } else if (current == ',' && !quoted) {
            // 引号外的逗号切分字段
            fields.push_back(field);
            field.clear();
        } else {
            field.push_back(current);
        }
    }

    fields.push_back(field); // 收尾：写入最后一个字段
    return fields;
}

/// 将字段值转义为 CSV 安全文本
/// 逻辑：仅当值含逗号/双引号/换行时才处理——整体加双引号包裹，
///       内部出现的引号翻倍（"" 转义）。
std::string csvSafe(const std::string& value) {
    if (value.find_first_of(",\"\n") == std::string::npos) {
        return value;
    }

    std::string escaped = "\"";
    for (char current : value) {
        if (current == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(current);
        }
    }
    escaped += "\"";
    return escaped;
}

/// 确保文件父目录存在
/// 逻辑：父目录为空（相对路径无目录部分）时无需处理；
///       创建失败（如权限不足）时捕获异常并返回 false。
bool ensureParentDirectory(const std::string& filePath) {
    try {
        const std::filesystem::path path(filePath);
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }
        return true;
    } catch (...) {
        return false;
    }
}

/// 安全解析 double
/// 逻辑：std::stod 抛异常（非数字文本）或未消费完全部字符
///       （如 "12abc"）时，ok 置为 false 并返回 0.0。
double parseDouble(const std::string& value, bool& ok) {
    try {
        std::size_t position = 0;
        const double result = std::stod(value, &position);
        ok = position == value.size();
        return result;
    } catch (...) {
        ok = false;
        return 0.0;
    }
}
