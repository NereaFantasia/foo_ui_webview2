#pragma once
// ============================================
// MetaValueOps.h - 多值标签的纯值层
// ============================================
//
// 多值 tag 的拼接与去重逻辑在此与 file_info 解耦：本文件零 fb2k 依赖，全部
// 函数可在 tests 工程离线单测。字段枚举与 UTF-8 清洗留在适配层 MetaAccess.h，
// 适配层的多值分支必须走这一层取结果，不得用 fb2k 自带的 meta_format_entry
// 代替 —— 拼接一旦发生在 fb2k 二进制内，这层就成了单测覆盖不到生产路径的
// 死代码。
//
// 两个函数语义刻意不同，是对外契约的一部分，不是疏漏：
// - JoinMetaValues 不去重、不丢空值 —— 显示串必须与 metadata.read 的全值
//   数组按同一分隔符 join 后逐字节一致（artist=["A","A"] 出 "A, A"）。
// - DedupPreservingOrder 保序去重并丢弃空值 —— 身份/去重键的计数口径，与
//   getArtists / getStats 既有的 strlen>0 过滤保持一致。
//
// 对外 artists[] 用的第三态（原序、不去重、不丢空值）没有对应的纯值函数：它就是
// 枚举结果本身，由适配层 MetaAccess.h 的 MetaValuesRaw 原样产出，本层无变换可做。
// 三态对照：JoinMetaValues 不去重不丢空 / DedupPreservingOrder 去重且丢空 /
// raw 由适配层直接按枚举原样给出，再交给 JoinMetaValues 就还原成显示串。

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

// 按 sep 原序拼接；空列表返回 ""；单元素返回该元素本身（不追加分隔符）。
inline std::string JoinMetaValues(const std::vector<std::string>& values, const char* sep) {
    if (values.empty()) return {};
    const size_t sepLen = std::strlen(sep);
    size_t total = sepLen * (values.size() - 1);
    for (const auto& value : values) total += value.size();
    std::string result;
    result.reserve(total);
    result += values.front();
    for (size_t i = 1; i < values.size(); ++i) {
        result += sep;
        result += values[i];
    }
    return result;
}

// 保序去重（fb2k 允许同字段重复值）并丢弃空值。值个数通常是个位数，
// 线性查重足够，不值得为此引入哈希容器。
inline std::vector<std::string> DedupPreservingOrder(std::vector<std::string> values) {
    std::vector<std::string> result;
    result.reserve(values.size());
    for (auto& value : values) {
        if (value.empty()) continue;
        if (std::find(result.begin(), result.end(), value) != result.end()) continue;
        result.push_back(std::move(value));
    }
    return result;
}
