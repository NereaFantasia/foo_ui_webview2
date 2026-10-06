#pragma once
// RatingSelect.h - Select and clamp a rating from statistics and file tags.
// The host adapter is defined in RatingResolve.h.
/*
 * 统计评分与 RATING 标签可能同时存在且不一致。优先采用有效统计值，缺失或
 * 越界时回退到标签，防止旧标签覆盖统计值。rating.set 优先写统计，但也可能
 * 回退写标签，因此两种来源都需要支持。
 *
 * 此处只处理候选整数，不依赖 fb2k 类型，供宿主读取路径与独立单元测试共用。
 */

#include <optional>

// 评分来源。rating.get 将 kStats 映射为 "stats"，其余来源均映射为 "file"。
enum class RatingSource {
    kNone,   // 两个存储都没给出可用的值
    kStats,  // foo_playcount 的统计项
    kTag,    // 文件里的 RATING 标签
};

struct TrackRating {
    int value = 0;  // 恒在 0 到 5
    RatingSource source = RatingSource::kNone;
};

// 评分的合法范围。0 不是一个等级，是"未评分"。
inline constexpr int kMaxRating = 5;

/*
 * nullopt 表示该来源缺失，标签值 0 则表示标签存在但未评分。返回的 source
 * 保留 kNone 与 kTag 的区别；rating.get 的公开 storage 字段不区分这两种情况。
 *
 * 统计候选值仅接受 1 到 5，其他值继续回退。标签候选值限制到 0 到 5，
 * 不按其他评分标度换算；例如标签值 100 返回 5，而不是将标签视为缺失。
 */
inline TrackRating SelectRating(std::optional<int> stats, std::optional<int> tag) {
    if (stats.has_value() && *stats >= 1 && *stats <= kMaxRating)
        return {*stats, RatingSource::kStats};
    if (tag.has_value()) {
        const int clamped = *tag < 0 ? 0 : (*tag > kMaxRating ? kMaxRating : *tag);
        return {clamped, RatingSource::kTag};
    }
    return {};
}
