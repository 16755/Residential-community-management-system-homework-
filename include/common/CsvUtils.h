// ============================================================================
// 文件: CsvUtils.h
// 职责: CSV 通用工具函数集。本项目所有数据均以 CSV 文件持久化，
//       本模块提供统一的 CSV 解析、转义、目录保障与数字解析能力，
//       供各业务模块（业主、物业、门禁、账号）读写数据文件时复用。
// ============================================================================
#pragma once

#include <string>
#include <vector>

/// 将一行 CSV 文本按逗号拆分为字段列表
/// 支持双引号包裹字段及 "" 转义，字段内可包含逗号
/// @param line 待拆分的原始 CSV 行文本
/// @return 拆分后的字段列表（已去除包裹引号并还原转义引号）
std::vector<std::string> splitCsv(const std::string& line);

/// 将单个字段值转换为安全可写入 CSV 的文本
/// 当值包含逗号、双引号或换行时，用双引号包裹并将内部引号翻倍转义
/// @param value 原始字段值
/// @return 可直接写入 CSV 的安全文本
std::string csvSafe(const std::string& value);

/// 确保指定文件所在的父目录存在，不存在则递归创建
/// @param filePath 目标文件路径
/// @return 目录创建成功（或原本就存在）返回 true
bool ensureParentDirectory(const std::string& filePath);

/// 将字符串安全解析为 double，并报告是否完整解析成功
/// @param value 待解析的字符串
/// @param ok    输出参数：解析成功且消费完全部字符时为 true
/// @return 解析出的数值；失败时返回 0.0
double parseDouble(const std::string& value, bool& ok);
