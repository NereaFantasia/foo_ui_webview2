/**
 * SubsongPath.h - 子曲目路径 "<路径>|subsong:N" 的拆分与拼接（SDK-free）
 *
 * CUE 与多子曲目文件里的一首曲目，在 API 上写成 "<路径>|subsong:N"。宿主里
 * 拆这个后缀、拼这个后缀都经本头的函数，规则只有一份。读不出序号时记控制台、
 * 按拆出的路径建 metadb handle 这类需要 SDK 的包装在 SubsongUtils.h。
 */
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace fb2k_paths {

    // '|' 是 Win32 文件名保留字符，真实文件名里不会出现这个标记。
    inline constexpr std::string_view kSubsongMarker = "|subsong:";

    struct SubsongPath {
        std::string path;          // 标记之前的部分；没有标记时是整个输入
        uint32_t subsong = 0;      // 标记之后读出的序号；没有标记或读不出时为 0
        bool hasSuffix = false;    // 输入里有标记
        bool suffixValid = false;  // 有标记，且标记之后读出了序号
    };

    // 在第一个标记处截断：标记之后的文字都不属于路径，其中再出现标记也一样。
    //
    // 序号按 std::stoul 的规则读：跳过前导空白，接受正负号，读到第一个非数字为止
    // （"3abc" 得 3）。一个数字也没有、或超出 unsigned long（MSVC 上为 32 位）时
    // 算读不出，序号为 0，路径照样截断。负号按 stoul 回绕（"-1" 得 4294967295），
    // 与各调用点改用本函数之前的结果相同。
    inline SubsongPath SplitSubsongPath(std::string_view in) {
        SubsongPath out;
        const size_t pos = in.find(kSubsongMarker);
        if (pos == std::string_view::npos) {
            out.path.assign(in);
            return out;
        }

        out.path.assign(in.substr(0, pos));
        out.hasSuffix = true;
        const std::string digits(in.substr(pos + kSubsongMarker.size()));
        try {
            out.subsong = static_cast<uint32_t>(std::stoul(digits));
            out.suffixValid = true;
        } catch (const std::invalid_argument&) {
            out.subsong = 0;
        } catch (const std::out_of_range&) {
            out.subsong = 0;
        }
        return out;
    }

    // 拼出 API 上的曲目键：subsong 为 0 或路径为空时原样返回路径，不拼后缀。
    inline std::string JoinSubsongPath(std::string_view path, uint32_t subsong) {
        std::string out(path);
        if (subsong > 0 && !out.empty()) {
            out += kSubsongMarker;
            out += std::to_string(subsong);
        }
        return out;
    }

    struct LegacyTrackPath {
        std::string path;
        uint32_t subsong = 0;
    };

    // metadata.* 与 rating.* 另认两种写法（见 src/api/schema/metadata.ts、rating.ts）：
    // cueIndex 不小于 0 时序号就是它；否则取 "|subsong:N"；两者都没有时取结尾的
    // "#N"，即最后一个 '#' 之后全是十进制数字。
    //
    // "|subsong:N" 后缀总是剥掉，cueIndex 盖过它也一样；"#N" 只在最后一步才拆，
    // 所以 cueIndex 或 "|subsong:N" 生效时路径里的 "#N" 原样保留。"#N" 超出 int
    // 范围时整条当路径、序号为 0。
    //
    // 调用方给的是用户拖进来的任意文件名时不能用本函数：以 "#<数字>" 结尾的文件名
    // 会被误切。
    inline LegacyTrackPath SplitLegacyTrackPath(std::string_view in, int64_t cueIndex) {
        SubsongPath parts = SplitSubsongPath(in);
        if (cueIndex >= 0) return {std::move(parts.path), static_cast<uint32_t>(cueIndex)};
        if (parts.hasSuffix) return {std::move(parts.path), parts.subsong};

        const size_t hashPos = in.rfind('#');
        if (hashPos == std::string_view::npos || hashPos + 1 >= in.size()) {
            return {std::string(in), 0};
        }

        constexpr uint64_t kMaxIndex = 2147483647;  // std::stoi 能表示的上限
        uint64_t value = 0;
        for (const char c : in.substr(hashPos + 1)) {
            if (c < '0' || c > '9') return {std::string(in), 0};
            value = value * 10 + static_cast<uint64_t>(c - '0');
            if (value > kMaxIndex) return {std::string(in), 0};
        }
        return {std::string(in.substr(0, hashPos)), static_cast<uint32_t>(value)};
    }

} // namespace fb2k_paths
