// test_rating_select.cpp — 评分取值顺序的语义锁
//
// 评分有两个独立存储（foo_playcount 的统计项、文件里的 RATING 标签），顺序是
// 统计优先，取不到有效统计值再读标签。两边都有有效值且不相等时，才能区分
// 两种取值顺序，防止播放列表行与 rating.get 对同一首曲目选择不同来源。
// 测试直接验证 SelectRating 的顺序、来源标记与两类输入各自的越界处理，
// 不验证宿主实际读取统计项或文件标签的过程。
#include "pch.h"
#include "api/RatingSelect.h"

namespace rating_select_test {

TEST(SelectRating, StatsWinsWhenBothStoresDisagree) {
    // 唯一能区分两种顺序的输入：翻过来的话这里会得到 2
    const TrackRating picked = SelectRating(4, 2);
    EXPECT_EQ(picked.value, 4);
    EXPECT_EQ(picked.source, RatingSource::kStats);
}

TEST(SelectRating, TagAnswersWhenStatsHasNone) {
    const TrackRating picked = SelectRating(std::nullopt, 3);
    EXPECT_EQ(picked.value, 3);
    EXPECT_EQ(picked.source, RatingSource::kTag);
}

TEST(SelectRating, NeitherStoreAnswersMeansUnrated) {
    const TrackRating picked = SelectRating(std::nullopt, std::nullopt);
    EXPECT_EQ(picked.value, 0);
    EXPECT_EQ(picked.source, RatingSource::kNone);
}

TEST(SelectRating, TagZeroIsUnratedButStillCountsAsAnAnswer) {
    // 值与"两边都没有"相同，来源必须不同：rating.get 的 storage 靠它区分
    const TrackRating picked = SelectRating(std::nullopt, 0);
    EXPECT_EQ(picked.value, 0);
    EXPECT_EQ(picked.source, RatingSource::kTag);
}

TEST(SelectRating, StatsOutOfRangeFallsThroughToTag) {
    // %rating% 只会给 1 到 5 或空，给别的说明那不是等级，不该报成 stats
    const TrackRating picked = SelectRating(7, 3);
    EXPECT_EQ(picked.value, 3);
    EXPECT_EQ(picked.source, RatingSource::kTag);
}

TEST(SelectRating, StatsZeroIsNotAnAnswer) {
    // 0 不是有效的统计评分；选择器应继续尝试标签，而不是将其报告为 stats。
    const TrackRating picked = SelectRating(0, 2);
    EXPECT_EQ(picked.value, 2);
    EXPECT_EQ(picked.source, RatingSource::kTag);
}

TEST(SelectRating, TagOutOfRangeIsClampedNotDiscarded) {
    // RATING 标签没有统一标度，0 到 100 的打标工具真实存在：夹成 5 偏高但仍是
    // 评分，判成未评分会让这类库整片变空
    const TrackRating high = SelectRating(std::nullopt, 100);
    EXPECT_EQ(high.value, kMaxRating);
    EXPECT_EQ(high.source, RatingSource::kTag);

    const TrackRating negative = SelectRating(std::nullopt, -1);
    EXPECT_EQ(negative.value, 0);
    EXPECT_EQ(negative.source, RatingSource::kTag);
}

TEST(SelectRating, EveryStarLevelRoundTrips) {
    for (int star = 1; star <= kMaxRating; ++star) {
        const TrackRating picked = SelectRating(star, std::nullopt);
        EXPECT_EQ(picked.value, star);
        EXPECT_EQ(picked.source, RatingSource::kStats);
    }
}

}  // namespace rating_select_test
