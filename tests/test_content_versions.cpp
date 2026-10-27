// test_content_versions.cpp - 播放列表与队列的共享内容计数
//
// 直接链接生产头 core/ContentVersions.h；回调接入在 SDK 一侧，由实机套件覆盖。
#include "pch.h"
#include "core/ContentVersions.h"

using content_version::Counters;

TEST(ContentVersions, ListsCountSeparatelyByGuid) {
    Counters counters;
    EXPECT_EQ(counters.List("a"), 0u);
    counters.ListChanged("a");
    counters.ListChanged("a");
    EXPECT_EQ(counters.List("a"), 2u);
    EXPECT_EQ(counters.List("b"), 0u);
    EXPECT_EQ(counters.Queue(), 0u);
}

TEST(ContentVersions, QueueCountsIndependentlyOfLists) {
    Counters counters;
    counters.QueueChanged();
    EXPECT_EQ(counters.Queue(), 1u);
    EXPECT_EQ(counters.List("a"), 0u);
}

// 版本只比相等：进程标识不同，同一计数也不相等。
TEST(ContentVersions, FormatDiffersAcrossProcesses) {
    EXPECT_EQ(content_version::Format("p", 3), "p-3");
    EXPECT_NE(content_version::Format("p", 3), content_version::Format("q", 3));
    EXPECT_NE(content_version::Format("p", 3), content_version::Format("p", 4));
}
