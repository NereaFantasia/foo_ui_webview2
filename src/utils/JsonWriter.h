#pragma once
// ============================================
// JsonWriter.h - UTF-8 JSON 直写原语（header-only）
// ============================================
//
// 大结果集响应的热路径不再经 nlohmann DOM：调用方把值直接追加成 UTF-8 JSON
// 字符串，省掉「建 DOM → dump」两段与其间的深拷贝。本文件只提供最小原语
// （字符串 / 整型 / 浮点 / 布尔），对象与数组的括号、逗号、键名由调用方拼，
// 因为热路径的字段集是编译期已知的固定形状，通用 builder 反而多一层开销。
//
// 与 nlohmann dump() 的等价口径（直写产物与 DOM 产物必须可互换）：
//   - 转义面：仅 `"` `\` 与控制字符 U+0000–U+001F 被转义，`/` 与 DEL(0x7F)
//     不转义，非 ASCII 的 UTF-8 字节原样透传（nlohmann 默认不作 \u 转义）；
//   - 非有限浮点：NaN / ±Inf 无 JSON 表示，与 dump() 同样输出字面 null；
//   - 有限浮点：最短往返形态（std::to_chars），整数形态补 ".0" 与 dump() 对齐。
//   字节级不承诺与 dump() 全等，语义级（parse 回来相等）承诺全等。
//
// UTF-8 有效性不在本文件校验：入参必须已是合法 UTF-8。来源不可信的字符串
// （插件提供的标签、菜单名等）先过 StringUtils::SafeUtf8 —— 截断的多字节序列
// 直写不会像 dump() 那样抛 type_error.316，而是产出页面侧解析失败的坏消息。
//
// ============================================

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

namespace JsonWriter {

// 追加带引号的 JSON 字符串字面量（含首尾双引号）。
inline void AppendJsonString(std::string& out, std::string_view value) {
    static constexpr char kHexDigits[] = "0123456789abcdef";  // 小写与 dump() 的 \u%04x 一致

    if (value.empty()) {
        out.append("\"\"", 2);  // 空 view 的 data() 可能为空指针，不进 append 路径
        return;
    }

    out.push_back('"');

    // 逐字节判定、按区间成批 append：无需转义的连续段一次拷贝，只有真正要转义的
    // 字节才打断区间。非 ASCII 字节（UTF-8 前导/续字节）与 DEL(0x7F) 都落在区间内，
    // 与 dump() 的默认 ensure_ascii=false 行为一致。
    size_t plainBegin = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        const auto byte = static_cast<unsigned char>(value[i]);
        if (byte >= 0x20 && byte != '"' && byte != '\\') {
            continue;
        }

        out.append(value.data() + plainBegin, i - plainBegin);
        plainBegin = i + 1;

        // case 表照 nlohmann 3.11.3 dump_escaped 抄写，便于并排核对
        switch (byte) {
            case 0x08: out.append("\\b", 2);  continue;
            case 0x09: out.append("\\t", 2);  continue;
            case 0x0A: out.append("\\n", 2);  continue;
            case 0x0C: out.append("\\f", 2);  continue;
            case 0x0D: out.append("\\r", 2);  continue;
            case 0x22: out.append("\\\"", 2); continue;
            case 0x5C: out.append("\\\\", 2); continue;
            default: break;
        }

        // 其余控制字符（U+0000–U+001F 中无短转义者）→ \u00xx
        out.append("\\u00", 4);
        out.push_back(kHexDigits[byte >> 4]);
        out.push_back(kHexDigits[byte & 0x0F]);
    }
    out.append(value.data() + plainBegin, value.size() - plainBegin);

    out.push_back('"');
}

// 追加 JSON 整型。
inline void AppendJsonInt(std::string& out, int64_t value) {
    char buffer[24];  // int64 十进制最长 20 字节（含负号）
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    out.append(buffer, static_cast<size_t>(result.ptr - buffer));
}

// 追加 JSON 浮点。非有限值输出字面 null（对齐 nlohmann dump 语义）。
inline void AppendJsonNumber(std::string& out, double value) {
    if (!std::isfinite(value)) {
        out.append("null");
        return;
    }

    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (result.ec != std::errc{}) {
        // 不可达：64 字节远超 double 最短往返形态的上界（24 字节）。真发生时按
        // 非有限值同口径写 null —— 宁可字段值可见缺失，不可产出坏 JSON。
        out.append("null");
        return;
    }

    // to_chars 的最短形态对整数值不带小数点（5.0 -> "5"），dump() 会补 ".0"。
    // 补齐以保持类型形态一致，避免与 DOM 产物对拍时出现整型/浮点差异。
    const size_t length = static_cast<size_t>(result.ptr - buffer);
    bool hasFractionOrExponent = false;
    for (size_t i = 0; i < length; ++i) {
        if (buffer[i] == '.' || buffer[i] == 'e') {
            hasFractionOrExponent = true;
            break;
        }
    }

    out.append(buffer, length);
    if (!hasFractionOrExponent) {
        out.append(".0");
    }
}

// 追加 JSON 布尔。
inline void AppendJsonBool(std::string& out, bool value) {
    out.append(value ? "true" : "false");
}

}  // namespace JsonWriter
