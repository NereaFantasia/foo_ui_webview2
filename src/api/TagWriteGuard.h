#pragma once
// WAV 标签写入的前置检查。
//
// foobar2000 重写 WAV 时，RIFF INFO 块只能存 ASCII：非 ASCII 字符写成 '?'，
// GBK 等本地编码的 INFO 块又被宿主按 Latin-1 读入，重写后原标签无法还原。
// 写入前用本头判断标签里是否含非 ASCII，含则拒写。
//
// 只依赖标准库与 ApiResult.h；调用方在包含本头前要先满足 ApiResult.h 的要求
// （测试工程先包含 compat/fb2k_types.h 的桩）。
#include "api/ApiResult.h"
#include "api/ErrorEnvelope.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace api::tag_guard {

// 路径的扩展名是否属于会折叠 INFO 块的容器。AIFF 与内嵌封面的写入没有这个问题，不在内
inline bool FoldsNonAscii(std::string_view path) {
    const std::size_t dot = path.rfind('.');
    const std::size_t sep = path.find_last_of("/\\");
    if (dot == std::string_view::npos || (sep != std::string_view::npos && dot < sep)) return false;
    const std::string_view ext = path.substr(dot + 1);
    const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    return ext.size() == 3 && lower(ext[0]) == 'w' && lower(ext[1]) == 'a' && lower(ext[2]) == 'v';
}

inline bool HasNonAscii(std::string_view text) {
    return std::any_of(text.begin(), text.end(),
                       [](unsigned char c) { return c >= 0x80; });
}

// 第一个取值含非 ASCII 的标签名；没有则返回空。Info 取 file_info 的 meta 接口，便于测试里换成桩
template <class Info>
std::optional<std::string> FirstNonAsciiField(const Info& info) {
    for (std::size_t i = 0, count = info.meta_get_count(); i < count; ++i) {
        for (std::size_t j = 0, values = info.meta_enum_value_count(i); j < values; ++j) {
            if (HasNonAscii(info.meta_enum_value(i, j))) return std::string(info.meta_enum_name(i));
        }
    }
    return std::nullopt;
}

inline Failure Refusal(const std::string& path, const std::string& field) {
    return Fail("WAV files keep tag text as ASCII in the RIFF INFO chunk; "
                "rewriting this file would replace its non-ASCII text with '?'",
                ApiErrorCode::NOT_SUPPORTED,
                {{"path", path}, {"details", {{"reason", "wavInfoNonAscii"}, {"field", field}}}});
}

} // namespace api::tag_guard
