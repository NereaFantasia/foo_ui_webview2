// test_match_pattern.cpp - Wildcard pattern matching
// Reimplementation of PortHub.cpp::MatchPattern (static function)
#include "pch.h"

// ============================================
// Reimplementation for testing
// ============================================
namespace reimpl {

static bool MatchPattern(const std::string& key, const std::string& pattern) {
    if (pattern == "*") return true;
    // Simple wildcard: only trailing * (e.g., "lyrics:*")
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.length() - 1);
        return key.compare(0, prefix.length(), prefix) == 0;
    }
    // Exact match
    return key == pattern;
}

// Reimplementation of PlaylistApi.cpp::IsHttpUrl
static bool IsHttpUrl(const std::string& path) {
    return path.starts_with("http://") || path.starts_with("https://");
}

// Reimplementation of ArtworkApi.cpp::ArtworkClassifyPathKind
static const char* ArtworkClassifyPathKind(const std::string& path) {
    if (path.find("file-relative://") == 0) return "file-relative://";
    if (path.find("file://") == 0) return "file://";
    if (path.find("|subsong:") != std::string::npos) return "path|subsong";
    return "native";
}

} // namespace reimpl

// ============================================
// MatchPattern
// ============================================

TEST(MatchPattern, WildcardMatchesAll) {
    EXPECT_TRUE(reimpl::MatchPattern("anything", "*"));
    EXPECT_TRUE(reimpl::MatchPattern("", "*"));
}

TEST(MatchPattern, PrefixWildcard) {
    EXPECT_TRUE(reimpl::MatchPattern("lyrics:synced", "lyrics:*"));
    EXPECT_TRUE(reimpl::MatchPattern("lyrics:", "lyrics:*"));
    EXPECT_FALSE(reimpl::MatchPattern("other:key", "lyrics:*"));
}

TEST(MatchPattern, ExactMatch) {
    EXPECT_TRUE(reimpl::MatchPattern("playback:volume", "playback:volume"));
    EXPECT_FALSE(reimpl::MatchPattern("playback:volume2", "playback:volume"));
}

TEST(MatchPattern, NoMatch) {
    EXPECT_FALSE(reimpl::MatchPattern("foo", "bar"));
}

// ============================================
// IsHttpUrl
// ============================================

TEST(IsHttpUrl, Http) {
    EXPECT_TRUE(reimpl::IsHttpUrl("http://example.com/stream"));
}

TEST(IsHttpUrl, Https) {
    EXPECT_TRUE(reimpl::IsHttpUrl("https://example.com/stream"));
}

TEST(IsHttpUrl, Ftp) {
    EXPECT_FALSE(reimpl::IsHttpUrl("ftp://example.com/file"));
}

TEST(IsHttpUrl, LocalPath) {
    EXPECT_FALSE(reimpl::IsHttpUrl("E:\\Music\\song.flac"));
}

TEST(IsHttpUrl, Empty) {
    EXPECT_FALSE(reimpl::IsHttpUrl(""));
}

// ============================================
// ArtworkClassifyPathKind
// ============================================

TEST(ArtworkClassifyPathKind, FileRelative) {
    EXPECT_STREQ(reimpl::ArtworkClassifyPathKind("file-relative://cover.jpg"), "file-relative://");
}

TEST(ArtworkClassifyPathKind, FileProtocol) {
    EXPECT_STREQ(reimpl::ArtworkClassifyPathKind("file://E:/Music/cover.jpg"), "file://");
}

TEST(ArtworkClassifyPathKind, WithSubsong) {
    EXPECT_STREQ(reimpl::ArtworkClassifyPathKind("E:\\Music\\album.cue|subsong:3"), "path|subsong");
}

TEST(ArtworkClassifyPathKind, NativePath) {
    EXPECT_STREQ(reimpl::ArtworkClassifyPathKind("E:\\Music\\song.flac"), "native");
}
