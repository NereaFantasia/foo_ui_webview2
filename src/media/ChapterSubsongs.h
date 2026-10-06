#pragma once

#include "api/generated/MediaSchema.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace media {

struct SubsongLength {
    std::int64_t subsong = 0;
    std::optional<double> length;
};

// foobar2000 按章节拆出的子曲目与章节逐一对应时才填 subsong：个数相同且多于一个，
// 除最后一章外，时长与章节起止之差都不超过 10 ms。最后一章被延到音轨末尾，
// 与声明的终点可能差出编码器补齐的长度，所以不比。任何一处对不上就一个都不填。
inline bool AssignChapterSubsongs(std::vector<api::media::ContainerChapter>& chapters,
                                  const std::vector<SubsongLength>& subsongs) {
    constexpr double kTolerance = 0.010;
    if (subsongs.size() < 2 || subsongs.size() != chapters.size()) return false;
    for (std::size_t i = 0; i + 1 < chapters.size(); ++i) {
        const auto& chapter = chapters[i];
        if (!chapter.end.has_value() || !subsongs[i].length.has_value()) return false;
        if (std::abs(*subsongs[i].length - (*chapter.end - chapter.start)) > kTolerance) return false;
    }
    for (std::size_t i = 0; i < chapters.size(); ++i) chapters[i].subsong = subsongs[i].subsong;
    return true;
}

} // namespace media
