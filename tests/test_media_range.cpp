#include "pch.h"
#include "media/MediaRange.h"

TEST(MediaRange, AFileWithoutRangeMustFitInOneResponse) {
    EXPECT_EQ(media::SelectRange("", 0).status, 200);
    EXPECT_EQ(media::SelectRange("", media::kMediaSegmentBytes).length, media::kMediaSegmentBytes);
    EXPECT_EQ(media::SelectRange("", media::kMediaSegmentBytes + 1).status, 416);
}

TEST(MediaRange, OpenEndedAndLongRangesAreCapped) {
    const auto open = media::SelectRange("bytes=17-", 10000000);
    EXPECT_EQ(open.status, 206);
    EXPECT_EQ(open.offset, 17u);
    EXPECT_EQ(open.length, media::kMediaSegmentBytes);
    const auto bounded = media::SelectRange("bytes=1-9999999", 10000000);
    EXPECT_EQ(bounded.length, media::kMediaSegmentBytes);
}

TEST(MediaRange, EndIsInclusiveAndClampedToTheFile) {
    const auto one = media::SelectRange("bytes=2-2", 10);
    EXPECT_EQ(one.status, 206);
    EXPECT_EQ(one.offset, 2u);
    EXPECT_EQ(one.length, 1u);
    EXPECT_EQ(media::SelectRange("bytes=8-999", 10).length, 2u);
}

TEST(MediaRange, SuffixStartsAtTheRequestedTailAndStillHasABoundedResponse) {
    const auto suffix = media::SelectRange("bytes=-3", 10);
    EXPECT_EQ(suffix.status, 206);
    EXPECT_EQ(suffix.offset, 7u);
    EXPECT_EQ(suffix.length, 3u);
    EXPECT_EQ(media::SelectRange("bytes=-99", 10).offset, 0u);
    EXPECT_EQ(media::SelectRange("bytes=-9000000", 10000000).length, media::kMediaSegmentBytes);
}

TEST(MediaRange, MalformedMultipleEmptyAndOverflowRangesAreRejected) {
    for (const auto* value : {"bytes=", "bytes=-", "bytes=-0", "bytes=10-", "bytes=9-1",
             "bytes=0-1,3-4", "items=0-1", "bytes=+1-2", "bytes=0-2x", "bytes=0--1",
             "bytes=18446744073709551616-", "bytes=0-18446744073709551616"}) {
        EXPECT_EQ(media::SelectRange(value, 10).status, 416) << value;
    }
    EXPECT_EQ(media::SelectRange("bytes=0-", 0).status, 416);
}

TEST(MediaRange, MaximumIntegerEndDoesNotOverflowLengthCalculation) {
    const auto range = media::SelectRange("bytes=0-18446744073709551615", UINT64_MAX);
    EXPECT_EQ(range.status, 206);
    EXPECT_EQ(range.length, media::kMediaSegmentBytes);
}
