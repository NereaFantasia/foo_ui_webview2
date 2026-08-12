// test_media_membership_index.cpp - membership set semantics and thread safety.
#include "pch.h"
#include "../src/utils/MediaMembershipIndex.h"

#include <atomic>
#include <thread>

using fb2k_utils::MediaMembershipIndex;

TEST(MediaMembershipIndex, FreshIndexIsInvalidAndMissesEverything) {
    MediaMembershipIndex index;
    EXPECT_FALSE(index.IsValid());
    EXPECT_EQ(index.Size(), 0u);
    EXPECT_FALSE(index.Contains("C:\\music\\a.flac"));
    EXPECT_FALSE(index.Contains(""));
}

TEST(MediaMembershipIndex, RebuildStoresHitsAndReportsMisses) {
    MediaMembershipIndex index;
    index.Rebuild({"C:\\music\\a.flac", "C:\\music\\b.flac"});

    EXPECT_TRUE(index.IsValid());
    EXPECT_EQ(index.Size(), 2u);
    EXPECT_TRUE(index.Contains("C:\\music\\a.flac"));
    EXPECT_TRUE(index.Contains("C:\\music\\b.flac"));
    EXPECT_FALSE(index.Contains("C:\\music\\c.flac"));
}

TEST(MediaMembershipIndex, MatchingIsByteExact) {
    MediaMembershipIndex index;
    index.Rebuild({"C:\\music\\a.flac"});

    // Canonicalisation is the caller's job, so no case folding or separator
    // fixing happens here.
    EXPECT_FALSE(index.Contains("c:\\music\\a.flac"));
    EXPECT_FALSE(index.Contains("C:/music/a.flac"));
}

TEST(MediaMembershipIndex, RebuildReplacesPreviousContents) {
    MediaMembershipIndex index;
    index.Rebuild({"C:\\music\\a.flac"});
    index.Rebuild({"C:\\music\\b.flac"});

    EXPECT_EQ(index.Size(), 1u);
    EXPECT_FALSE(index.Contains("C:\\music\\a.flac"));
    EXPECT_TRUE(index.Contains("C:\\music\\b.flac"));
}

TEST(MediaMembershipIndex, InvalidateClearsAndMarksInvalid) {
    MediaMembershipIndex index;
    index.Rebuild({"C:\\music\\a.flac", "C:\\music\\b.flac"});
    index.Invalidate();

    EXPECT_FALSE(index.IsValid());
    EXPECT_EQ(index.Size(), 0u);
    EXPECT_FALSE(index.Contains("C:\\music\\a.flac"));
}

TEST(MediaMembershipIndex, EmptyRebuildIsValidButEmpty) {
    MediaMembershipIndex index;
    index.Rebuild({});

    // "built, and the set really is empty" must stay distinguishable from
    // "never built": both miss every query, only the former is authoritative.
    EXPECT_TRUE(index.IsValid());
    EXPECT_EQ(index.Size(), 0u);
    EXPECT_FALSE(index.Contains("C:\\music\\a.flac"));
}

TEST(MediaMembershipIndex, QueryDistinguishesInvalidFromAbsent) {
    MediaMembershipIndex index;

    // Contains() collapses both states to false; Query() is what lets a caller
    // tell "index unusable, fall back" apart from "definitely not a member".
    EXPECT_FALSE(index.Query("C:\\music\\a.flac").has_value());

    index.Rebuild({"C:\\music\\a.flac"});

    auto hit = index.Query("C:\\music\\a.flac");
    ASSERT_TRUE(hit.has_value());
    EXPECT_TRUE(*hit);

    auto miss = index.Query("C:\\music\\zzz.flac");
    ASSERT_TRUE(miss.has_value());
    EXPECT_FALSE(*miss);
}

TEST(MediaMembershipIndex, QueryOnEmptyButValidIndexAnswersAuthoritatively) {
    MediaMembershipIndex index;
    index.Rebuild({});

    auto answer = index.Query("C:\\music\\a.flac");
    ASSERT_TRUE(answer.has_value());
    EXPECT_FALSE(*answer);
}

TEST(MediaMembershipIndex, QueryReturnsNulloptAfterInvalidate) {
    MediaMembershipIndex index;
    index.Rebuild({"C:\\music\\a.flac"});
    index.Invalidate();

    EXPECT_FALSE(index.Query("C:\\music\\a.flac").has_value());
}

TEST(MediaMembershipIndex, DuplicateInputCollapses) {
    MediaMembershipIndex index;
    index.Rebuild({"C:\\music\\a.flac", "C:\\music\\a.flac", "C:\\music\\b.flac",
                   "C:\\music\\a.flac"});

    EXPECT_EQ(index.Size(), 2u);
    EXPECT_TRUE(index.Contains("C:\\music\\a.flac"));
    EXPECT_TRUE(index.Contains("C:\\music\\b.flac"));
}

TEST(MediaMembershipIndex, GenerationAdvancesOnEachMutation) {
    MediaMembershipIndex index;
    const uint64_t initial = index.Generation();
    EXPECT_EQ(index.Generation(), initial);  // reads alone never advance it

    index.Rebuild({"C:\\music\\a.flac"});
    const uint64_t afterFirstRebuild = index.Generation();
    EXPECT_GT(afterFirstRebuild, initial);

    EXPECT_TRUE(index.Contains("C:\\music\\a.flac"));
    EXPECT_FALSE(index.Contains("C:\\music\\zz.flac"));
    EXPECT_EQ(index.Generation(), afterFirstRebuild);

    index.Rebuild({"C:\\music\\b.flac"});
    const uint64_t afterSecondRebuild = index.Generation();
    EXPECT_GT(afterSecondRebuild, afterFirstRebuild);

    index.Invalidate();
    const uint64_t afterInvalidate = index.Generation();
    EXPECT_GT(afterInvalidate, afterSecondRebuild);

    index.Invalidate();
    EXPECT_GT(index.Generation(), afterInvalidate);
}

TEST(MediaMembershipIndex, NoEntryCapAtLegacyFiftyThousandLimit) {
    // Locks the fix for the old PathSecurity::IsItemInLibraryOrPlaylist
    // behaviour, which stopped scanning at 50000 items and returned false past
    // that point, making "absent" indistinguishable from "truncated". Entry
    // 59999 sits beyond the retired cap and must still be found.
    constexpr size_t kEntryCount = 60000;

    std::vector<std::string> paths;
    paths.reserve(kEntryCount);
    for (size_t i = 0; i < kEntryCount; ++i) {
        paths.push_back("C:\\music\\track" + std::to_string(i) + ".flac");
    }

    MediaMembershipIndex index;
    index.Rebuild(std::move(paths));

    EXPECT_EQ(index.Size(), kEntryCount);
    EXPECT_TRUE(index.Contains("C:\\music\\track59999.flac"));
    EXPECT_TRUE(index.Contains("C:\\music\\track50000.flac"));
    EXPECT_TRUE(index.Contains("C:\\music\\track0.flac"));
    EXPECT_FALSE(index.Contains("C:\\music\\track60000.flac"));
}

TEST(MediaMembershipIndex, ConcurrentReadsAgree) {
    constexpr size_t kEntryCount = 2000;
    constexpr int kThreadCount = 8;

    std::vector<std::string> paths;
    paths.reserve(kEntryCount);
    for (size_t i = 0; i < kEntryCount; ++i) {
        paths.push_back("C:\\music\\track" + std::to_string(i) + ".flac");
    }

    MediaMembershipIndex index;
    index.Rebuild(paths);

    std::atomic<size_t> hits{0};
    std::atomic<size_t> misses{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (int t = 0; t < kThreadCount; ++t) {
        workers.emplace_back([&] {
            for (size_t i = 0; i < kEntryCount; ++i) {
                if (index.Contains(paths[i])) {
                    hits.fetch_add(1, std::memory_order_relaxed);
                }
                if (!index.Contains("C:\\music\\absent" + std::to_string(i) + ".flac")) {
                    misses.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(hits.load(), kEntryCount * kThreadCount);
    EXPECT_EQ(misses.load(), kEntryCount * kThreadCount);
    EXPECT_EQ(index.Size(), kEntryCount);
}
