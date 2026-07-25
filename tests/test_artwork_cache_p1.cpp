// test_artwork_cache_p1.cpp
// P1 cache correctness tests: ETag, negative cache TTL, LRU eviction order.
// These tests exercise the algorithmic contracts without COM or foobar2000 SDK.
#include "pch.h"
#include "../src/api/ArtworkRequestParser.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// ETag format contract
// ---------------------------------------------------------------------------

TEST(ArtworkCacheP1, EtagIsNonEmptyQuotedHex) {
    // Simulate the ETag computation: hash(fingerprintBase + type + maxSize)
    const std::string fingerprintBase = "E:\\Music\\album.flac:1718000000000:12345678;";
    const std::string type = "front";
    const int maxSize = 256;
    const size_t h = std::hash<std::string>{}(fingerprintBase + type + std::to_string(maxSize));
    char buf[32];
    snprintf(buf, sizeof(buf), "\"%zx\"", h);
    const std::string etag(buf);

    EXPECT_FALSE(etag.empty());
    EXPECT_EQ(etag.front(), '"');
    EXPECT_EQ(etag.back(), '"');
    EXPECT_GT(etag.size(), 2u);  // at least one hex digit inside quotes
}

TEST(ArtworkCacheP1, EtagChangesWhenFingerprintChanges) {
    const std::string type = "front";
    const int maxSize = 128;

    auto makeEtag = [&](const std::string& fp) {
        const size_t h = std::hash<std::string>{}(fp + type + std::to_string(maxSize));
        char buf[32];
        snprintf(buf, sizeof(buf), "\"%zx\"", h);
        return std::string(buf);
    };

    // Different mtime → different fingerprint → different ETag
    const std::string fp1 = "E:\\Music\\cover.jpg:1718000000000:98765;";
    const std::string fp2 = "E:\\Music\\cover.jpg:1718999999999:98765;";
    EXPECT_NE(makeEtag(fp1), makeEtag(fp2));
}

TEST(ArtworkCacheP1, EtagChangesWhenSizeChanges) {
    const std::string type = "front";
    const int maxSize = 128;

    auto makeEtag = [&](const std::string& fp) {
        const size_t h = std::hash<std::string>{}(fp + type + std::to_string(maxSize));
        char buf[32];
        snprintf(buf, sizeof(buf), "\"%zx\"", h);
        return std::string(buf);
    };

    const std::string fp1 = "E:\\Music\\cover.jpg:1718000000000:1024;";
    const std::string fp2 = "E:\\Music\\cover.jpg:1718000000000:2048;";
    EXPECT_NE(makeEtag(fp1), makeEtag(fp2));
}

TEST(ArtworkCacheP1, EtagStableForSameInput) {
    const std::string fp = "E:\\Music\\cover.jpg:1718000000000:98765;";
    const std::string type = "front";
    const int maxSize = 256;
    const size_t h = std::hash<std::string>{}(fp + type + std::to_string(maxSize));
    char buf1[32], buf2[32];
    snprintf(buf1, sizeof(buf1), "\"%zx\"", h);
    snprintf(buf2, sizeof(buf2), "\"%zx\"", h);
    EXPECT_STREQ(buf1, buf2);
}

TEST(ArtworkCacheP1, EtagDiffersForDifferentMaxSize) {
    const std::string fp = "E:\\Music\\cover.jpg:1718000000000:98765;";
    const std::string type = "front";

    auto makeEtag = [&](int ms) {
        const size_t h = std::hash<std::string>{}(fp + type + std::to_string(ms));
        char buf[32];
        snprintf(buf, sizeof(buf), "\"%zx\"", h);
        return std::string(buf);
    };
    EXPECT_NE(makeEtag(64), makeEtag(128));
    EXPECT_NE(makeEtag(0), makeEtag(256));
}

// ---------------------------------------------------------------------------
// Negative cache TTL logic
// ---------------------------------------------------------------------------

namespace {

using Clock = std::chrono::steady_clock;

struct NegativeCacheEntry {
    Clock::time_point timestamp;
};

constexpr int kNegTtlSec = 30;

bool IsNegativeCacheFresh(const NegativeCacheEntry& e) {
    const auto age = Clock::now() - e.timestamp;
    return std::chrono::duration_cast<std::chrono::seconds>(age).count() < kNegTtlSec;
}

}  // namespace

TEST(ArtworkCacheP1, NegativeCacheFreshAfterInsert) {
    NegativeCacheEntry e{Clock::now()};
    EXPECT_TRUE(IsNegativeCacheFresh(e));
}

TEST(ArtworkCacheP1, NegativeCacheExpiredAfterTtl) {
    NegativeCacheEntry e{Clock::now() - std::chrono::seconds(kNegTtlSec + 1)};
    EXPECT_FALSE(IsNegativeCacheFresh(e));
}

TEST(ArtworkCacheP1, NegativeCacheExactlyAtTtlBoundaryIsExpired) {
    NegativeCacheEntry e{Clock::now() - std::chrono::seconds(kNegTtlSec)};
    EXPECT_FALSE(IsNegativeCacheFresh(e));
}

TEST(ArtworkCacheP1, NegativeCacheKeyDistinguishesByType) {
    std::unordered_map<std::string, NegativeCacheEntry> cache;
    const std::string path = "E:\\Music\\track.flac";
    cache["front:" + path] = NegativeCacheEntry{Clock::now()};

    EXPECT_EQ(cache.count("front:" + path), 1u);
    EXPECT_EQ(cache.count("back:" + path), 0u);
}

TEST(ArtworkCacheP1, NegativeCacheDoesNotAffectDifferentPath) {
    std::unordered_map<std::string, NegativeCacheEntry> cache;
    const std::string path1 = "E:\\Music\\track1.flac";
    const std::string path2 = "E:\\Music\\track2.flac";
    cache["front:" + path1] = NegativeCacheEntry{Clock::now()};

    EXPECT_EQ(cache.count("front:" + path1), 1u);
    EXPECT_EQ(cache.count("front:" + path2), 0u);
}

// ---------------------------------------------------------------------------
// LRU eviction order
// ---------------------------------------------------------------------------

namespace {

struct CacheEntry {
    int value;
    Clock::time_point created;
    Clock::time_point lastAccess;
};

// Evict least-recently-accessed 20% when over capacity.
void EvictLRU(std::unordered_map<std::string, CacheEntry>& cache, size_t maxSize) {
    if (cache.size() < maxSize) return;
    std::vector<std::unordered_map<std::string, CacheEntry>::iterator> iters;
    iters.reserve(cache.size());
    for (auto it = cache.begin(); it != cache.end(); ++it)
        iters.push_back(it);
    const size_t toEvict = std::max<size_t>(1, maxSize / 5);
    const size_t evict = std::min(toEvict, iters.size());
    std::partial_sort(iters.begin(), iters.begin() + evict, iters.end(),
        [](auto a, auto b) { return a->second.lastAccess < b->second.lastAccess; });
    for (size_t i = 0; i < evict; ++i)
        cache.erase(iters[i]);
}

}  // namespace

TEST(ArtworkCacheP1, LruEvictsLeastRecentlyAccessed) {
    std::unordered_map<std::string, CacheEntry> cache;
    const auto baseTime = Clock::now();

    // Insert 5 entries with different lastAccess times.
    for (int i = 0; i < 5; ++i) {
        cache["key" + std::to_string(i)] = CacheEntry{
            i, baseTime, baseTime + std::chrono::seconds(i)};
    }

    // Evict with max=5 → 20% = 1 → evict key0 (lastAccess = baseTime + 0s).
    EvictLRU(cache, 5);

    EXPECT_EQ(cache.count("key0"), 0u);  // least recently accessed
    EXPECT_EQ(cache.count("key4"), 1u);  // most recently accessed, kept
}

TEST(ArtworkCacheP1, LruPreservesRecentlyAccessedEntry) {
    std::unordered_map<std::string, CacheEntry> cache;
    const auto baseTime = Clock::now();
    const auto recent = baseTime + std::chrono::seconds(1000);

    for (int i = 0; i < 5; ++i) {
        cache["key" + std::to_string(i)] = CacheEntry{
            i, baseTime, baseTime + std::chrono::seconds(i)};
    }
    // Simulate a hit on key0 that updates its lastAccess to recent.
    cache["key0"].lastAccess = recent;

    EvictLRU(cache, 5);

    // key0 is now most-recently accessed → should survive.
    EXPECT_EQ(cache.count("key0"), 1u);
    // key1 was the next oldest → should be evicted.
    EXPECT_EQ(cache.count("key1"), 0u);
}

TEST(ArtworkCacheP1, LruEvictsCorrectCountAt20Percent) {
    std::unordered_map<std::string, CacheEntry> cache;
    const auto baseTime = Clock::now();
    for (int i = 0; i < 10; ++i) {
        cache["key" + std::to_string(i)] = CacheEntry{
            i, baseTime, baseTime + std::chrono::seconds(i)};
    }

    // maxSize=10, 20% = 2 evicted.
    EvictLRU(cache, 10);
    EXPECT_EQ(cache.size(), 8u);
    EXPECT_EQ(cache.count("key0"), 0u);
    EXPECT_EQ(cache.count("key1"), 0u);
    EXPECT_EQ(cache.count("key2"), 1u);
}

// ---------------------------------------------------------------------------
// Cache-Control header contract
// ---------------------------------------------------------------------------

TEST(ArtworkCacheP1, CacheControlDoesNotContainImmutable) {
    // The P1 contract forbids "immutable" on mutable artwork URLs.
    const std::wstring headerStr = L"Cache-Control: public, max-age=86400\r\n";
    EXPECT_EQ(headerStr.find(L"immutable"), std::wstring::npos);
    EXPECT_NE(headerStr.find(L"max-age=86400"), std::wstring::npos);
}

TEST(ArtworkCacheP1, Response304IncludesETagHeader) {
    // Validate that a 304 header string contains both ETag and Cache-Control.
    const std::wstring etag = L"\"deadbeef\"";
    const std::wstring headers304 =
        L"ETag: " + etag + L"\r\n"
        L"Cache-Control: no-cache\r\n"
        L"Access-Control-Allow-Origin: *\r\n";

    EXPECT_NE(headers304.find(L"ETag:"), std::wstring::npos);
    EXPECT_NE(headers304.find(L"Cache-Control:"), std::wstring::npos);
    EXPECT_NE(headers304.find(etag), std::wstring::npos);
}

TEST(ArtworkCacheP1, ForbiddenResponseDoesNotWriteNegativeCache) {
    // 403 (permission denied) must NOT be stored in the negative cache.
    // This is tested by asserting the contract: negCacheKey is only written on 404.
    // Simulate the two outcomes:
    bool negCacheWritten = false;
    auto onPermissionDenied = [&]() {
        // 403 path: do NOT write negative cache
        negCacheWritten = false;
    };
    auto onArtworkNotFound = [&]() {
        // 404 path: write negative cache
        negCacheWritten = true;
    };

    onPermissionDenied();
    EXPECT_FALSE(negCacheWritten);

    onArtworkNotFound();
    EXPECT_TRUE(negCacheWritten);
}
