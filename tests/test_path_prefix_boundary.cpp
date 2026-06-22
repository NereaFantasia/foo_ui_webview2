// test_path_prefix_boundary.cpp - MR-1201 directory boundary bypass regression tests
#include "pch.h"
#include <string>
#include <algorithm>

// Reimpl of PathSecurity::IsPathPrefixOf for unit testing without singleton
namespace {
bool IsPathPrefixOf(const std::wstring& prefix, const std::wstring& path) {
    if (path.size() < prefix.size()) return false;
    if (_wcsnicmp(path.c_str(), prefix.c_str(), prefix.size()) != 0) return false;
    if (path.size() == prefix.size()) return true;
    wchar_t lastPrefixChar = prefix.back();
    if (lastPrefixChar == L'\\' || lastPrefixChar == L'/') return true;
    wchar_t next = path[prefix.size()];
    return next == L'\\' || next == L'/';
}
}

// MR-1201: Exact match
TEST(PathPrefixBoundary, ExactMatch) {
    EXPECT_TRUE(IsPathPrefixOf(L"C:\\Windows", L"C:\\Windows"));
}

// MR-1201: Valid subdirectory
TEST(PathPrefixBoundary, ValidSubdir) {
    EXPECT_TRUE(IsPathPrefixOf(L"C:\\Windows", L"C:\\Windows\\System32"));
}

// MR-1201: Bypass attempt - prefix without boundary
TEST(PathPrefixBoundary, BypassAttempt) {
    EXPECT_FALSE(IsPathPrefixOf(L"C:\\Windows", L"C:\\WindowsEvil\\payload.exe"));
}

// MR-1201: Bypass with similar prefix
TEST(PathPrefixBoundary, BypassSimilarName) {
    EXPECT_FALSE(IsPathPrefixOf(L"C:\\Program Files", L"C:\\Program Files2\\evil.exe"));
}

// MR-1201: Prefix ending with backslash
TEST(PathPrefixBoundary, PrefixWithTrailingSlash) {
    EXPECT_TRUE(IsPathPrefixOf(L"C:\\Windows\\", L"C:\\Windows\\System32"));
}

// MR-1201: Forward slash boundary
TEST(PathPrefixBoundary, ForwardSlashBoundary) {
    EXPECT_TRUE(IsPathPrefixOf(L"C:\\Music", L"C:\\Music/album/track.mp3"));
}

// MR-1201: Path shorter than prefix
TEST(PathPrefixBoundary, PathShorterThanPrefix) {
    EXPECT_FALSE(IsPathPrefixOf(L"C:\\Windows\\System32", L"C:\\Windows"));
}

// MR-1201: Case insensitive (via _wcsnicmp)
TEST(PathPrefixBoundary, CaseInsensitive) {
    EXPECT_TRUE(IsPathPrefixOf(L"c:\\windows", L"C:\\WINDOWS\\System32"));
}
