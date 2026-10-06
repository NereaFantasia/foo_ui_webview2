// test_path_security_virtual_protocol.cpp - path_protocol::IsVirtualOrNetworkProtocol
//
// Exercises the *real* classifier that decides whether a caller-supplied path
// is a virtual / network protocol (and therefore skips the whole local
// filesystem pipeline: traversal check, canonical resolution, allow / deny
// lists) or a local path that must go through it. The function lives in its
// own SDK-free header for the same reason as PathCanonicalForm.h: the test
// project deliberately does not build against the foobar2000 SDK.
//
// The contract under test:
//   - a virtual protocol is `scheme://` anchored at position 0
//   - scheme is 2..19 characters from [A-Za-z0-9+.-], matched case-insensitively
//   - schemes that embed a local path (file, archive, unpack) are NOT virtual
//   - a single-character scheme is never a protocol: `X://...` is a drive-rooted
//     path that Win32 normalises to `X:\...`, so it must reach the pipeline
//   - `://` anywhere other than right after the scheme does not count
//
// The single-letter rule prevents a local path from bypassing the deny list:
// std::filesystem / CRT can open `c://Windows/System32/drivers/etc/hosts` as
// C:\Windows\System32\drivers\etc\hosts, so the classifier must send that
// spelling through the local-path checks rather than release it as a protocol.
#include "pch.h"
#include "utils/PathProtocolScheme.h"
#include "utils/PathCanonicalForm.h"

#include <cwctype>
#include <filesystem>
#include <string>

namespace {

namespace fs = std::filesystem;
using path_protocol::IsVirtualOrNetworkProtocol;

bool IEquals(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (::towlower(a[i]) != ::towlower(b[i])) return false;
    }
    return true;
}

} // namespace

// ============================================
// Virtual / network protocols: released
// ============================================

TEST(PathSecurityVirtualProtocol, HttpAndHttpsAreVirtual) {
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"http://example.com/a.mp3"));
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"https://example.com/a.mp3"));
}

TEST(PathSecurityVirtualProtocol, SchemeMatchIsCaseInsensitive) {
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"HTTPS://EXAMPLE.COM/A.MP3"));
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"Cdda://"));
}

TEST(PathSecurityVirtualProtocol, Fb2kBuiltinProtocolsAreVirtual) {
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"cdda://"));
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"tone://440"));
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"silence://10"));
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"record://"));
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"tempfile://"));
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"tempmem://"));
}

TEST(PathSecurityVirtualProtocol, DigitLeadingSchemeIsVirtual) {
    // foo_youtube's legacy protocol. It violates RFC 3986 (scheme must start
    // with a letter) but still sits in users' playlists, so the classifier
    // must accept a leading digit.
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"3dydfy://www.youtube.com/watch?v=fJ9rUzIMcZQ"));
}

TEST(PathSecurityVirtualProtocol, SchemeWithPlusIsVirtual) {
    // foo_youtube 2.x: fy+<original scheme>.
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"fy+https://www.youtube.com/watch?v=fJ9rUzIMcZQ"));
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"fy+playlist://youtube.com?id=UU16niRr50&limit=50"));
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"fy+search://youtube.com?q=x"));
}

TEST(PathSecurityVirtualProtocol, SchemeWithHyphenAndDotIsVirtual) {
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"my-proto://x"));
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"my.proto://x"));
}

TEST(PathSecurityVirtualProtocol, TwoCharacterSchemeIsTheMinimum) {
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"ab://x"));
}

TEST(PathSecurityVirtualProtocol, NineteenCharacterSchemeIsTheMaximum) {
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"abcdefghijklmnopqrs://x"));    // 19
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"abcdefghijklmnopqrst://x"));   // 20
}

TEST(PathSecurityVirtualProtocol, FileRelativeStaysVirtual) {
    // "file-relative://" denotes a local path relative to fb2k's install
    // directory, whose base needs SDK resolution. The classifier treats it as
    // virtual, so it bypasses the local-path pipeline. This assertion records
    // that limitation; it does not establish safe access to the target.
    EXPECT_TRUE(IsVirtualOrNetworkProtocol(L"file-relative://Music\\x.flac"));
}

// ============================================
// Protocols that embed a local path: NOT virtual
// ============================================

TEST(PathSecurityVirtualProtocol, FileSchemeIsNotVirtual) {
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"file://C:\\Music\\x.flac"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"FILE://C:\\Music\\x.flac"));
}

TEST(PathSecurityVirtualProtocol, ArchiveAndUnpackSchemesAreNotVirtual) {
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"archive://D:\\a.zip|/t.flac"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"unpack://zip|123|file://D:\\a.zip|t.flac"));
}

// ============================================
// Drive-letter forms: NOT virtual
// ============================================

TEST(PathSecurityVirtualProtocol, SingleLetterSchemeIsADriveNotAProtocol) {
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"c://Windows/System32/drivers/etc/hosts"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"C://x"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"d://x"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"x://"));
}

TEST(PathSecurityVirtualProtocol, NativePathFormsAreNotVirtual) {
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"C:\\Music\\x.flac"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"D:/Music/x.flac"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"\\\\server\\share\\x.flac"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"relative\\x.flac"));
}

// ============================================
// Malformed / unanchored: NOT virtual
// ============================================

TEST(PathSecurityVirtualProtocol, SeparatorNotAtStartDoesNotCount) {
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"C:\\a\\b://c"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L" http://x"));
}

TEST(PathSecurityVirtualProtocol, EmptyAndBareSeparatorAreNotVirtual) {
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L""));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"://"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"://x"));
}

TEST(PathSecurityVirtualProtocol, IncompleteSeparatorIsNotVirtual) {
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"http:/x"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"http:x"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"http"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"http:"));
}

TEST(PathSecurityVirtualProtocol, CharactersOutsideSchemeSetAreNotAScheme) {
    // RFC 3986 scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ), widened
    // only by the leading digit. Anything else before "://" is not a scheme.
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"ht tp://x"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"ht_tp://x"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"ht\\tp://x"));
    EXPECT_FALSE(IsVirtualOrNetworkProtocol(L"ht/tp://x"));
}

// ============================================
// Pipeline consequence: the spoofed form reaches the deny-list stage
// ============================================

TEST(PathSecurityVirtualProtocol, DriveDoubleSlashFormCanonicalisesToTheRealSystemFile) {
    // Two real stages chained: once the classifier stops releasing
    // "c://Windows/...", PassBasicPathSafetyChecks hands the string to
    // ResolveToCanonicalForm, whose output is the X:\ spelling the deny list
    // is keyed on. Uses the real Windows directory so the test does not
    // hard-code a drive letter.
    wchar_t winDir[MAX_PATH] = {};
    const UINT n = ::GetWindowsDirectoryW(winDir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH || winDir[1] != L':') {
        SUCCEED();
        return;
    }
    const std::wstring windows(winDir, n);
    const std::wstring real = windows + L"\\System32\\drivers\\etc\\hosts";
    if (!fs::exists(real)) {
        SUCCEED();
        return;
    }

    const wchar_t drive = static_cast<wchar_t>(::towupper(windows[0]));
    std::wstring spoofed = std::wstring(1, static_cast<wchar_t>(::towlower(drive))) + L"://" +
                           windows.substr(3) + L"/System32/drivers/etc/hosts";

    EXPECT_FALSE(IsVirtualOrNetworkProtocol(spoofed));

    const std::wstring canonSpoofed = path_canonical::ResolveToCanonicalForm(spoofed, drive);
    const std::wstring canonReal = path_canonical::ResolveToCanonicalForm(real, drive);
    EXPECT_TRUE(IEquals(canonSpoofed, canonReal))
        << "spoofed=" << ::testing::PrintToString(canonSpoofed)
        << " real=" << ::testing::PrintToString(canonReal);
    EXPECT_EQ(canonSpoofed[1], L':');
    EXPECT_EQ(::towupper(canonSpoofed[0]), drive);
}
