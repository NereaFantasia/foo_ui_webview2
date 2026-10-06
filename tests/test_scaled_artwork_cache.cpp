// ScaledArtworkCache tests: exercise the production cache type directly so the
// eviction, byte budget, TTL and freshness rules are verified where they run.

#include "pch.h"
#include "../src/webview/ScaledArtworkCache.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using artwork_cache::ScaledArtworkCache;
using artwork_cache::ScaledArtworkLimits;

namespace {

using Clock = ScaledArtworkCache::Clock;
using Seconds = std::chrono::seconds;

ScaledArtworkCache::SharedBytes Bytes(size_t size, uint8_t fill = 0xAB) {
    return std::make_shared<const std::vector<uint8_t>>(size, fill);
}

std::string Key(int i) { return "key" + std::to_string(i); }

ScaledArtworkLimits Limits(size_t maxEntries, size_t maxBytes, Seconds ttl = Seconds(300)) {
    ScaledArtworkLimits limits;
    limits.maxEntries = maxEntries;
    limits.maxBytes = maxBytes;
    limits.unverifiedTtl = ttl;
    return limits;
}

}  // namespace

// ---------------------------------------------------------------------------
// Entry-count LRU eviction
// ---------------------------------------------------------------------------

TEST(ScaledArtworkCache, PutBeyondMaxEntriesEvictsLeastRecentlyAccessedBatch) {
    ScaledArtworkCache cache(Limits(5, 1 << 20));
    const auto base = Clock::now();

    for (int i = 0; i < 5; ++i) {
        cache.Put(Key(i), Bytes(10), "image/jpeg", "", base + Seconds(i));
    }
    ASSERT_EQ(cache.EntryCount(), 5u);

    // Cache is full: the 6th insert evicts one entry (20% of 5) — the oldest lastAccess.
    cache.Put(Key(5), Bytes(10), "image/jpeg", "", base + Seconds(10));

    EXPECT_EQ(cache.EntryCount(), 5u);
    EXPECT_FALSE(cache.Get(Key(0), "", base + Seconds(11)).has_value());
    EXPECT_TRUE(cache.Get(Key(1), "", base + Seconds(11)).has_value());
    EXPECT_TRUE(cache.Get(Key(5), "", base + Seconds(11)).has_value());
}

TEST(ScaledArtworkCache, GetRefreshesLastAccessSoHotEntrySurvivesEviction) {
    ScaledArtworkCache cache(Limits(5, 1 << 20));
    const auto base = Clock::now();

    for (int i = 0; i < 5; ++i) {
        cache.Put(Key(i), Bytes(10), "image/jpeg", "", base + Seconds(i));
    }
    // Touch the oldest entry so it becomes the most recently accessed.
    ASSERT_TRUE(cache.Get(Key(0), "", base + Seconds(100)).has_value());

    cache.Put(Key(5), Bytes(10), "image/jpeg", "", base + Seconds(101));

    EXPECT_TRUE(cache.Get(Key(0), "", base + Seconds(102)).has_value());
    EXPECT_FALSE(cache.Get(Key(1), "", base + Seconds(102)).has_value());
}

TEST(ScaledArtworkCache, EvictionBatchIsTwentyPercentOfMaxEntries) {
    ScaledArtworkCache cache(Limits(10, 1 << 20));
    const auto base = Clock::now();

    for (int i = 0; i < 10; ++i) {
        cache.Put(Key(i), Bytes(10), "image/jpeg", "", base + Seconds(i));
    }
    cache.Put(Key(10), Bytes(10), "image/jpeg", "", base + Seconds(20));

    // 10 entries, batch = 2 evicted, then 1 inserted → 9.
    EXPECT_EQ(cache.EntryCount(), 9u);
    EXPECT_FALSE(cache.Get(Key(0), "", base + Seconds(21)).has_value());
    EXPECT_FALSE(cache.Get(Key(1), "", base + Seconds(21)).has_value());
    EXPECT_TRUE(cache.Get(Key(2), "", base + Seconds(21)).has_value());
}

// ---------------------------------------------------------------------------
// Byte budget
// ---------------------------------------------------------------------------

TEST(ScaledArtworkCache, PutBeyondMaxBytesEvictsUntilNewEntryFits) {
    // Entry count never reaches the limit; only the byte budget triggers eviction.
    ScaledArtworkCache cache(Limits(100, 1000));
    const auto base = Clock::now();

    for (int i = 0; i < 5; ++i) {
        cache.Put(Key(i), Bytes(200), "image/jpeg", "", base + Seconds(i));
    }
    ASSERT_EQ(cache.TotalBytes(), 1000u);

    cache.Put(Key(5), Bytes(500), "image/jpeg", "", base + Seconds(10));

    EXPECT_LE(cache.TotalBytes(), 1000u);
    EXPECT_TRUE(cache.Get(Key(5), "", base + Seconds(11)).has_value());
    // Oldest entries went first.
    EXPECT_FALSE(cache.Get(Key(0), "", base + Seconds(11)).has_value());
    EXPECT_TRUE(cache.Get(Key(4), "", base + Seconds(11)).has_value());
}

TEST(ScaledArtworkCache, TotalBytesTracksInsertsReplacementsAndEvictions) {
    ScaledArtworkCache cache(Limits(100, 1 << 20));
    const auto base = Clock::now();

    cache.Put(Key(0), Bytes(100), "image/jpeg", "", base);
    cache.Put(Key(1), Bytes(250), "image/jpeg", "", base);
    EXPECT_EQ(cache.TotalBytes(), 350u);

    // Replacing a key swaps its bytes instead of adding to the total.
    cache.Put(Key(0), Bytes(40), "image/jpeg", "", base + Seconds(1));
    EXPECT_EQ(cache.EntryCount(), 2u);
    EXPECT_EQ(cache.TotalBytes(), 290u);
}

TEST(ScaledArtworkCache, PayloadLargerThanWholeBudgetIsNotCached) {
    ScaledArtworkCache cache(Limits(100, 1000));
    const auto base = Clock::now();

    cache.Put(Key(0), Bytes(1001), "image/jpeg", "", base);

    EXPECT_EQ(cache.EntryCount(), 0u);
    EXPECT_EQ(cache.TotalBytes(), 0u);
}

// ---------------------------------------------------------------------------
// TTL for entries without a source fingerprint
// ---------------------------------------------------------------------------

TEST(ScaledArtworkCache, UnverifiedEntryExpiresAfterTtlOnGet) {
    ScaledArtworkCache cache(Limits(100, 1 << 20, Seconds(300)));
    const auto base = Clock::now();

    cache.Put(Key(0), Bytes(10), "image/jpeg", "", base);

    EXPECT_TRUE(cache.Get(Key(0), "", base + Seconds(299)).has_value());
    EXPECT_FALSE(cache.Get(Key(0), "", base + Seconds(300)).has_value());
    // The stale entry is removed rather than left behind.
    EXPECT_EQ(cache.EntryCount(), 0u);
    EXPECT_EQ(cache.TotalBytes(), 0u);
}

TEST(ScaledArtworkCache, ExpiredUnverifiedEntriesAreSweptOnPut) {
    ScaledArtworkCache cache(Limits(100, 1 << 20, Seconds(300)));
    const auto base = Clock::now();

    cache.Put(Key(0), Bytes(10), "image/jpeg", "", base);
    cache.Put(Key(1), Bytes(10), "image/jpeg", "", base);
    ASSERT_EQ(cache.EntryCount(), 2u);

    // A write for an unrelated key past the TTL sweeps the expired ones even
    // though nobody asked for them again.
    cache.Put(Key(2), Bytes(10), "image/jpeg", "", base + Seconds(301));

    EXPECT_EQ(cache.EntryCount(), 1u);
    EXPECT_EQ(cache.TotalBytes(), 10u);
    EXPECT_TRUE(cache.Get(Key(2), "", base + Seconds(302)).has_value());
}

// ---------------------------------------------------------------------------
// Source fingerprint freshness for verified entries
// ---------------------------------------------------------------------------

TEST(ScaledArtworkCache, VerifiedEntryHitsWhileFingerprintMatches) {
    ScaledArtworkCache cache(Limits(100, 1 << 20));
    const auto base = Clock::now();

    cache.Put(Key(0), Bytes(10), "image/jpeg", "cover.jpg:1000:2048;", base);

    EXPECT_TRUE(cache.Get(Key(0), "cover.jpg:1000:2048;", base + Seconds(1)).has_value());
    EXPECT_EQ(cache.EntryCount(), 1u);
}

TEST(ScaledArtworkCache, VerifiedEntryIsDroppedWhenFingerprintChanges) {
    ScaledArtworkCache cache(Limits(100, 1 << 20));
    const auto base = Clock::now();

    cache.Put(Key(0), Bytes(10), "image/jpeg", "cover.jpg:1000:2048;", base);

    // Same key, but the cover file was rewritten (new mtime/size).
    EXPECT_FALSE(cache.Get(Key(0), "cover.jpg:2000:4096;", base + Seconds(1)).has_value());
    EXPECT_EQ(cache.EntryCount(), 0u);
    EXPECT_EQ(cache.TotalBytes(), 0u);
}

TEST(ScaledArtworkCache, VerifiedEntryOutlivesUnverifiedTtl) {
    ScaledArtworkCache cache(Limits(100, 1 << 20, Seconds(300)));
    const auto base = Clock::now();

    cache.Put(Key(0), Bytes(10), "image/jpeg", "cover.jpg:1000:2048;", base);

    // Well past the TTL that governs unverified entries; freshness is proven by
    // the fingerprint instead, so the entry stays hot.
    EXPECT_TRUE(cache.Get(Key(0), "cover.jpg:1000:2048;", base + Seconds(3600)).has_value());
}

TEST(ScaledArtworkCache, VerifiedEntryIsNotSweptByTtlOnPut) {
    ScaledArtworkCache cache(Limits(100, 1 << 20, Seconds(300)));
    const auto base = Clock::now();

    cache.Put(Key(0), Bytes(10), "image/jpeg", "cover.jpg:1000:2048;", base);
    cache.Put(Key(1), Bytes(10), "image/jpeg", "", base);

    cache.Put(Key(2), Bytes(10), "image/jpeg", "", base + Seconds(301));

    // The unverified entry aged out; the verified one did not.
    EXPECT_EQ(cache.EntryCount(), 2u);
    EXPECT_TRUE(cache.Get(Key(0), "cover.jpg:1000:2048;", base + Seconds(302)).has_value());
    EXPECT_FALSE(cache.Get(Key(1), "", base + Seconds(302)).has_value());
}

TEST(ScaledArtworkCache, StaleVerifiedEntryIsReplacedByNextPut) {
    ScaledArtworkCache cache(Limits(100, 1 << 20));
    const auto base = Clock::now();

    cache.Put(Key(0), Bytes(10, 0x01), "image/jpeg", "cover.jpg:1000:2048;", base);
    EXPECT_FALSE(cache.Get(Key(0), "cover.jpg:2000:4096;", base + Seconds(1)).has_value());

    cache.Put(Key(0), Bytes(10, 0x02), "image/jpeg", "cover.jpg:2000:4096;", base + Seconds(2));

    const auto hit = cache.Get(Key(0), "cover.jpg:2000:4096;", base + Seconds(3));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ((*hit->bytes)[0], 0x02);
    EXPECT_EQ(cache.EntryCount(), 1u);
}

// ---------------------------------------------------------------------------
// Basic contract
// ---------------------------------------------------------------------------

TEST(ScaledArtworkCache, MissReturnsNulloptForUnknownKey) {
    ScaledArtworkCache cache(Limits(100, 1 << 20));
    EXPECT_FALSE(cache.Get("nope", "", Clock::now()).has_value());
}

TEST(ScaledArtworkCache, HitReturnsSharedBytesAndMimeType) {
    ScaledArtworkCache cache(Limits(100, 1 << 20));
    const auto base = Clock::now();
    auto bytes = Bytes(3, 0x5A);

    cache.Put(Key(0), bytes, "image/webp", "", base);

    const auto hit = cache.Get(Key(0), "", base + Seconds(1));
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->mimeType, "image/webp");
    ASSERT_TRUE(hit->bytes);
    EXPECT_EQ(hit->bytes->size(), 3u);
    EXPECT_EQ((*hit->bytes)[0], 0x5A);
    // The cache hands out the same buffer it stores; no per-hit copy.
    EXPECT_EQ(hit->bytes.get(), bytes.get());
}

TEST(ScaledArtworkCache, NullBytesAreIgnored) {
    ScaledArtworkCache cache(Limits(100, 1 << 20));
    cache.Put(Key(0), nullptr, "image/jpeg", "", Clock::now());
    EXPECT_EQ(cache.EntryCount(), 0u);
}
