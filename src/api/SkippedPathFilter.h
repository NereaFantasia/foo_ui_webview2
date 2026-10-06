#pragma once

// 声明了 skipInvalid 的数组路径参数：校验层跳过没通过的条目，交给 handler 的参数里
// 必须同时去掉它们。只计数不去掉时，被拒绝的路径照样到达 handler（playback.playPaths
// 会把它加入播放列表并播放），路径安全层级就形同虚设。

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace api {

// 一个数组参数里被跳过的条目。
struct SkippedEntries {
    std::string key;                // 参数键
    std::vector<std::size_t> indices;  // 升序的下标
};

// 返回去掉被跳过条目后的参数副本；键不存在或不是数组时这一项不动。
inline nlohmann::json WithoutSkippedEntries(const nlohmann::json& params,
                                            const std::vector<SkippedEntries>& skipped) {
    nlohmann::json out = params;
    for (const auto& s : skipped) {
        if (s.indices.empty()) continue;
        auto it = out.find(s.key);
        if (it == out.end() || !it->is_array()) continue;
        nlohmann::json kept = nlohmann::json::array();
        std::size_t next = 0;
        for (std::size_t i = 0; i < it->size(); ++i) {
            if (next < s.indices.size() && s.indices[next] == i) {
                ++next;
                continue;
            }
            kept.push_back((*it)[i]);
        }
        *it = std::move(kept);
    }
    return out;
}

}  // namespace api
