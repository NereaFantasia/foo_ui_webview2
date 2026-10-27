// test_library_change_log.cpp - library.getChanges 的修订号与变化表
//
// 直接链接生产头 domain/library/LibraryChangeLog.h；落盘与回调接入在 SDK 一侧，由实机覆盖。
#include "pch.h"
#include "domain/library/LibraryChangeLog.h"

#include <algorithm>

using library_sync::ChangeLog;
using library_sync::Resume;
using library_sync::Stored;

namespace {

std::vector<std::string> Sorted(std::vector<std::string> v) {
    std::sort(v.begin(), v.end());
    return v;
}

}  // namespace

// 启动时 fb2k 把整库按新增报一遍，这批不是变化。
TEST(LibraryChangeLog, IgnoresCallbacksBeforeLoaded) {
    ChangeLog log(7);
    log.Record({"a", "b"}, false);
    EXPECT_EQ(log.Revision(), 7u);
    log.MarkLoaded();
    std::vector<std::string> changed, removed;
    ASSERT_TRUE(log.Since(7, changed, removed));
    EXPECT_TRUE(changed.empty());
    EXPECT_TRUE(removed.empty());
}

TEST(LibraryChangeLog, EachCallbackAdvancesOnce) {
    ChangeLog log(7);
    log.MarkLoaded();
    log.Record({"a", "b"}, false);
    log.Record({"c"}, true);
    log.Record({}, false);
    EXPECT_EQ(log.Floor(), 7u);
    EXPECT_EQ(log.Revision(), 9u);
}

TEST(LibraryChangeLog, ReportsOnlyChangesAfterSince) {
    ChangeLog log;
    log.MarkLoaded();
    log.Record({"a"}, false);
    log.Record({"b"}, false);
    log.Record({"c"}, true);
    std::vector<std::string> changed, removed;
    ASSERT_TRUE(log.Since(1, changed, removed));
    EXPECT_EQ(changed, std::vector<std::string>{"b"});
    EXPECT_EQ(removed, std::vector<std::string>{"c"});
}

TEST(LibraryChangeLog, KeepsLastChangePerHandle) {
    ChangeLog log;
    log.MarkLoaded();
    log.Record({"a", "b"}, false);
    log.Record({"a"}, true);   // 加入后移出
    log.Record({"b"}, true);
    log.Record({"b"}, false);  // 移出后又加入
    std::vector<std::string> changed, removed;
    ASSERT_TRUE(log.Since(0, changed, removed));
    EXPECT_EQ(Sorted(changed), std::vector<std::string>{"b"});
    EXPECT_EQ(Sorted(removed), std::vector<std::string>{"a"});
}

TEST(LibraryChangeLog, OutOfRangeSinceAsksForResync) {
    ChangeLog log(5);
    log.MarkLoaded();
    log.Record({"a"}, false);
    std::vector<std::string> changed, removed;
    EXPECT_FALSE(log.Since(4, changed, removed));  // 早于启动时的修订号，变化表不全
    EXPECT_FALSE(log.Since(7, changed, removed));  // 比当前修订号还新
    EXPECT_TRUE(log.Since(5, changed, removed));
    EXPECT_TRUE(log.Since(6, changed, removed));
}

TEST(LibraryChangeLog, ResumeContinuesAfterCleanExit) {
    const Stored next = Resume({"{E}", 42, false}, "{NEW}");
    EXPECT_EQ(next.epoch, "{E}");
    EXPECT_EQ(next.revision, 42u);
    EXPECT_TRUE(next.sessionOpen);
}

TEST(LibraryChangeLog, ResumeStartsOverAfterCrashOrFirstRun) {
    const Stored crashed = Resume({"{E}", 42, true}, "{NEW}");
    EXPECT_EQ(crashed.epoch, "{NEW}");
    EXPECT_EQ(crashed.revision, 0u);
    EXPECT_TRUE(crashed.sessionOpen);

    const Stored first = Resume({"", 0, false}, "{NEW}");
    EXPECT_EQ(first.epoch, "{NEW}");
    EXPECT_EQ(first.revision, 0u);
}
