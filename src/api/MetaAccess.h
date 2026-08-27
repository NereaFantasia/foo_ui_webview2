#pragma once
// ============================================
// MetaAccess.h - 多值标签的 fb2k 适配层
// ============================================
//
// 多值语义字段（artist / album artist / genre / composer）从 file_info 取值的
// 唯一入口，拼接与去重语义在纯值层 MetaValueOps.h。file_info 的完整定义由
// pch.h 提供（引用本头的 TU 全部使用 PCH），此处不自行 include fb2k 头。
//
// 四个函数会在主线程（SDK 回调）与 CPU worker 线程（deferred 序列化）两个
// 上下文被调用：禁止引入任何共享可变状态，含 static 缓冲。
//
// 实现约定：每次调用只做一次字段查找（meta_find），单值快路径不分配 vector，
// 与 meta_get 首值取法的现状开销持平；不得先 meta_get_count_by_name 再
// meta_get —— 那会让字段查找翻倍。

#include "api/MetaValueOps.h"
#include "utils/StringUtils.h"

#include <cstdint>
#include <string>
#include <vector>

// 白名单字段的显示串：多值按 sep 原序拼接（不去重、不丢空值）。
// 标签为合法 UTF-8 的单值曲目上与 meta_get 首值结果逐字节等价；
// 非法序列替换为 U+FFFD。
inline std::string MetaJoined(const file_info& info, const char* name, const char* sep = ", ") {
    const t_size index = info.meta_find(name);
    if (index == SIZE_MAX) return {};
    const t_size count = info.meta_enum_value_count(index);
    if (count == 1)  // 单值快路径
        return StringUtils::SafeUtf8(info.meta_enum_value(index, 0));
    std::vector<std::string> values;
    values.reserve(count);
    for (t_size i = 0; i < count; ++i)
        values.push_back(StringUtils::SafeUtf8(info.meta_enum_value(index, i)));
    return JoinMetaValues(values, sep);
}

// 身份/去重键的全值形态：曲内保序去重并丢弃空值。
inline std::vector<std::string> MetaValues(const file_info& info, const char* name) {
    const t_size index = info.meta_find(name);
    if (index == SIZE_MAX) return {};
    const t_size count = info.meta_enum_value_count(index);
    std::vector<std::string> values;
    values.reserve(count);
    for (t_size i = 0; i < count; ++i)
        values.push_back(StringUtils::SafeUtf8(info.meta_enum_value(index, i)));
    return DedupPreservingOrder(std::move(values));
}

// 对外 artists[] 的数组形态：原序枚举全部值，不去重、不丢空值。与 MetaValues
// 的唯一差别就是不过 DedupPreservingOrder。
//
// 锁定不变量（artists[] 存在的理由，不可破）：
//   JoinMetaValues(MetaValuesRaw(info, name), ", ") 与 MetaJoined(info, name)
//   逐字节相等
// 这条即 artists.join(", ") === artist 这项对外契约的实现依据。换成去重版
// MetaValues 就不成立：artist=["A","A"] 时拼接串是 "A, A"，数组却只剩一个元素。
inline std::vector<std::string> MetaValuesRaw(const file_info& info, const char* name) {
    const t_size index = info.meta_find(name);
    if (index == SIZE_MAX) return {};
    const t_size count = info.meta_enum_value_count(index);
    std::vector<std::string> values;
    values.reserve(count);
    for (t_size i = 0; i < count; ++i)
        values.push_back(StringUtils::SafeUtf8(info.meta_enum_value(index, i)));
    return values;
}

// 字段存在判定。与 meta_get 首值判空等价：fb2k 不变量保证存在的字段
// 至少有一个值。
inline bool MetaPresent(const file_info& info, const char* name) {
    return info.meta_find(name) != SIZE_MAX;
}
