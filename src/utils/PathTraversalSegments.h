#pragma once
// ============================================
// PathTraversalSegments.h - 路径遍历判定的单点实现
// ============================================
//
// 本头只依赖标准库，让 PathSecurity 与不链接 fb2k SDK 的测试使用同一份判定。
//
// 判定回答的问题是: 这条拼写里有没有会让 Win32 路径归一化改写目录层级的相对
// 分量 (`..` 回到上一级、`.` 指向当前级)。有的必须在流水线入口拒绝——真远程共享
// 在其后零 IO 早退, 这一步是它们唯一的遍历防线; 没有的进入 canonical 与黑白名单。
//
// 契约: 按 `\` 与 `/` 切段, 某一段去掉全部空格 (U+0020) 后只剩句点、且至少含
// 一个句点, 即判遍历。判定单位是段, 不是子串:
//
//   - `..` / `.` 恰好成段才是相对分量。句点出现在名字内部或末尾 (`Bonus Track..flac`、
//     `Wind blows....flac`、`.hidden`、`..hidden`、`album.\x`) 是合法文件名, 不是遍历。
//     以句点结尾的曲名接上扩展名就带 `..`, 媒体库里这类文件并不罕见; 按子串找 `..`
//     会把它们连同整条 MediaRead 链一起拒掉, 所以判定不能落在子串上。
//   - 段内允许空格, 是因为 Win32 对末段会剥掉尾随句点与空格 (`.. ` 归一化后就是
//     `..`); 段只由句点与空格组成时一律按遍历处理, 宁可多拒一个没人会起的名字。
//   - 三个及以上句点成段 (`...`) 在 Win32 下是合法名字, 但落在上一条的保守范围内,
//     同样拒绝; 没有已知媒体路径用它当目录名。
//   - 空段 (UNC 前导 `\\`、连续分隔符、尾随分隔符) 与只有空格的段不是遍历,
//     交给下游解析。
//   - 不剥 `|subsong:N` 后缀、不解协议前缀: 调用方把整条拼写原样交进来, 后缀之后
//     藏的 `\..\` 同样成段、同样被拒。顺序契约见 PathSecurity 的 StripSubsongSuffix 注释。
//
// 只收单个文件名的参数 (歌词、封面的输出名, 日志文件名) 用 IsPlainFilename: 名字本身
// 就是唯一一段, 同一条段判定之外再禁分隔符。这类参数是 UTF-8 窄串, 句点与空格都是
// ASCII, 逐字节判定与宽串结论相同。
//
// ============================================

#include <string_view>

namespace path_traversal {

inline bool IsPathSeparator(wchar_t c) {
    return c == L'\\' || c == L'/';
}

namespace detail {

template <typename CharT>
bool IsTraversalSegmentImpl(std::basic_string_view<CharT> segment) {
    size_t periods = 0;
    for (const CharT c : segment) {
        if (c == CharT('.')) {
            ++periods;
        } else if (c != CharT(' ')) {
            return false;
        }
    }
    return periods > 0;
}

}  // namespace detail

// 某一段是否为相对分量: 去掉空格后只剩句点, 且至少一个句点。
inline bool IsTraversalSegment(std::wstring_view segment) {
    return detail::IsTraversalSegmentImpl(segment);
}

inline bool IsTraversalSegment(std::string_view segment) {
    return detail::IsTraversalSegmentImpl(segment);
}

// 是否为可直接拼到目录后面的单个文件名: 不含分隔符, 本身也不是相对分量。
// `Bonus Track..lrc`、`..jpg` 放行, `..`、`.`、`.. ` 拒绝。空名交调用方判定。
inline bool IsPlainFilename(std::string_view name) {
    return name.find_first_of("\\/") == std::string_view::npos && !IsTraversalSegment(name);
}

// 整条拼写中是否存在相对分量段。
inline bool ContainsTraversalSegment(std::wstring_view path) {
    size_t segmentStart = 0;
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || IsPathSeparator(path[i])) {
            if (IsTraversalSegment(path.substr(segmentStart, i - segmentStart))) return true;
            segmentStart = i + 1;
        }
    }
    return false;
}

}  // namespace path_traversal
