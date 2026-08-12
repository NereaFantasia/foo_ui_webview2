// test_path_canonical_cache.cpp - stamp-guarded reuse, LRU eviction and thread safety.
#include "pch.h"
#include "../src/utils/PathCanonicalCache.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using fb2k_utils::PathCanonicalCache;

namespace {

std::wstring KeyAt(size_t index) {
    return L"D:\\music\\track" + std::to_wstring(index) + L".flac";
}

}  // namespace

TEST(PathCanonicalCache, FreshCacheMissesEverything) {
    PathCanonicalCache cache;
    EXPECT_EQ(cache.Size(), 0u);
    EXPECT_FALSE(cache.Lookup(L"D:\\music\\a.flac", 1).has_value());
    EXPECT_EQ(cache.MissCount(), 1u);
    EXPECT_EQ(cache.HitCount(), 0u);
}

TEST(PathCanonicalCache, StoredEntryIsReusedWhenStampMatches) {
    PathCanonicalCache cache;
    cache.Store(L"D:\\music\\a.flac", L"D:\\music\\real.flac", 42);

    auto hit = cache.Lookup(L"D:\\music\\a.flac", 42);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, L"D:\\music\\real.flac");
    EXPECT_EQ(cache.HitCount(), 1u);
    EXPECT_EQ(cache.MissCount(), 0u);
}

TEST(PathCanonicalCache, StampChangeInvalidatesTheEntry) {
    PathCanonicalCache cache;
    cache.Store(L"D:\\music\\a.flac", L"D:\\music\\real.flac", 42);

    // A different parent stamp means the directory entry moved underneath the
    // recorded resolution, so the stale value must not be handed back.
    EXPECT_FALSE(cache.Lookup(L"D:\\music\\a.flac", 43).has_value());
    EXPECT_EQ(cache.MissCount(), 1u);

    // The stale entry is dropped rather than kept for a second chance, so even
    // the original stamp no longer resolves until the caller re-stores.
    EXPECT_FALSE(cache.Lookup(L"D:\\music\\a.flac", 42).has_value());
    EXPECT_EQ(cache.Size(), 0u);
}

TEST(PathCanonicalCache, RestoringAfterStampChangeYieldsTheNewValue) {
    PathCanonicalCache cache;
    cache.Store(L"D:\\music\\a.flac", L"D:\\old\\real.flac", 1);
    EXPECT_FALSE(cache.Lookup(L"D:\\music\\a.flac", 2).has_value());

    cache.Store(L"D:\\music\\a.flac", L"D:\\new\\real.flac", 2);
    auto hit = cache.Lookup(L"D:\\music\\a.flac", 2);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, L"D:\\new\\real.flac");
}

TEST(PathCanonicalCache, StoreOverwritesInPlaceWithoutGrowing) {
    PathCanonicalCache cache;
    cache.Store(L"D:\\music\\a.flac", L"D:\\one.flac", 7);
    cache.Store(L"D:\\music\\a.flac", L"D:\\two.flac", 7);

    EXPECT_EQ(cache.Size(), 1u);
    auto hit = cache.Lookup(L"D:\\music\\a.flac", 7);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, L"D:\\two.flac");
}

TEST(PathCanonicalCache, KeyMatchingIsByteExact) {
    PathCanonicalCache cache;
    cache.Store(L"D:\\music\\a.flac", L"D:\\music\\real.flac", 1);

    // Normalisation belongs to the caller, so no case folding or separator
    // rewriting happens here.
    EXPECT_FALSE(cache.Lookup(L"d:\\music\\a.flac", 1).has_value());
    EXPECT_FALSE(cache.Lookup(L"D:/music/a.flac", 1).has_value());
}

TEST(PathCanonicalCache, EvictsLeastRecentlyUsedOnceCapacityIsExceeded) {
    PathCanonicalCache cache(3);
    cache.Store(KeyAt(0), L"r0", 1);
    cache.Store(KeyAt(1), L"r1", 1);
    cache.Store(KeyAt(2), L"r2", 1);
    EXPECT_EQ(cache.Size(), 3u);
    EXPECT_EQ(cache.EvictionCount(), 0u);

    cache.Store(KeyAt(3), L"r3", 1);
    EXPECT_EQ(cache.Size(), 3u);
    EXPECT_EQ(cache.EvictionCount(), 1u);
    EXPECT_FALSE(cache.Lookup(KeyAt(0), 1).has_value());
    EXPECT_TRUE(cache.Lookup(KeyAt(3), 1).has_value());
}

TEST(PathCanonicalCache, LookupRefreshesRecencySoTheEntrySurvivesEviction) {
    PathCanonicalCache cache(3);
    cache.Store(KeyAt(0), L"r0", 1);
    cache.Store(KeyAt(1), L"r1", 1);
    cache.Store(KeyAt(2), L"r2", 1);

    // Touching the oldest entry makes it the newest, so the next insert must
    // evict what is now the oldest instead.
    ASSERT_TRUE(cache.Lookup(KeyAt(0), 1).has_value());
    cache.Store(KeyAt(3), L"r3", 1);

    EXPECT_TRUE(cache.Lookup(KeyAt(0), 1).has_value());
    EXPECT_FALSE(cache.Lookup(KeyAt(1), 1).has_value());
}

TEST(PathCanonicalCache, OverwriteRefreshesRecencyToo) {
    PathCanonicalCache cache(2);
    cache.Store(KeyAt(0), L"r0", 1);
    cache.Store(KeyAt(1), L"r1", 1);
    cache.Store(KeyAt(0), L"r0b", 1);

    cache.Store(KeyAt(2), L"r2", 1);
    EXPECT_TRUE(cache.Lookup(KeyAt(0), 1).has_value());
    EXPECT_FALSE(cache.Lookup(KeyAt(1), 1).has_value());
}

TEST(PathCanonicalCache, ZeroCapacityStoresNothingAndStaysUsable) {
    PathCanonicalCache cache(0);
    cache.Store(L"D:\\music\\a.flac", L"D:\\music\\real.flac", 1);

    EXPECT_EQ(cache.Size(), 0u);
    EXPECT_FALSE(cache.Lookup(L"D:\\music\\a.flac", 1).has_value());
}

TEST(PathCanonicalCache, ClearDropsEveryEntry) {
    PathCanonicalCache cache;
    cache.Store(KeyAt(0), L"r0", 1);
    cache.Store(KeyAt(1), L"r1", 1);
    cache.Clear();

    EXPECT_EQ(cache.Size(), 0u);
    EXPECT_FALSE(cache.Lookup(KeyAt(0), 1).has_value());

    // Clearing must not wedge the cache: it accepts entries again afterwards.
    cache.Store(KeyAt(0), L"r0", 1);
    EXPECT_TRUE(cache.Lookup(KeyAt(0), 1).has_value());
}

TEST(PathCanonicalCache, ZeroStampIsAValidStampNotASentinel) {
    PathCanonicalCache cache;
    cache.Store(L"D:\\music\\a.flac", L"D:\\music\\real.flac", 0);

    auto hit = cache.Lookup(L"D:\\music\\a.flac", 0);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, L"D:\\music\\real.flac");
}

TEST(PathCanonicalCache, HoldsFarMoreEntriesThanOneBatchWhenCapacityAllows) {
    constexpr size_t kCount = 20000;
    PathCanonicalCache cache(kCount);
    for (size_t i = 0; i < kCount; ++i) {
        cache.Store(KeyAt(i), L"r" + std::to_wstring(i), 1);
    }

    EXPECT_EQ(cache.Size(), kCount);
    EXPECT_EQ(cache.EvictionCount(), 0u);
    auto hit = cache.Lookup(KeyAt(kCount - 1), 1);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, L"r" + std::to_wstring(kCount - 1));
}

TEST(PathCanonicalCache, ConcurrentLookupAndStoreStayConsistent) {
    PathCanonicalCache cache(256);
    constexpr int kThreads = 4;
    constexpr int kIterations = 2000;

    std::atomic<bool> mismatch{false};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&cache, &mismatch, t]() {
            for (int i = 0; i < kIterations; ++i) {
                const std::wstring key = KeyAt(static_cast<size_t>(i % 64));
                const std::wstring value = L"resolved" + std::to_wstring(i % 64);
                cache.Store(key, value, 1);
                if (auto hit = cache.Lookup(key, 1); hit.has_value() && *hit != value) {
                    mismatch.store(true);
                }
                // A second stamp exercises the invalidation path concurrently.
                if (t % 2 == 0) {
                    cache.Lookup(key, 2);
                }
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_FALSE(mismatch.load());
    EXPECT_LE(cache.Size(), 256u);
}
