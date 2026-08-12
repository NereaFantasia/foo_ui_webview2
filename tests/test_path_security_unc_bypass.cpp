// test_path_security_unc_bypass.cpp - UNC filesystem-resolution bypass ordering tests
//
// Covers the UNC early-return added to PathSecurity::PassBasicPathSafetyChecks,
// which skips fs::exists / fs::canonical / GetLongPathNameW for UNC paths to
// avoid three network metadata round-trips per path on NAS shares.
//
// The real PathSecurity singleton constructor depends on core_api (profile /
// install paths), so it is not unit-testable here. Following the convention of
// test_path_prefix_boundary.cpp and test_api_path_security.cpp, the guard chain
// is reimplemented so the *ordering contract* can be pinned:
//
//     device-path interception  ->  traversal detection  ->  UNC early return
//
// Ordering is the security-relevant property: device paths (\\.\ and \\?\) share
// the leading \\ with UNC, so hoisting the UNC return above them would open a
// bypass channel.
#include "pch.h"
#include <string>
#include <functional>
#include <cwctype>
#include <cwchar>

namespace {

// Reimpl of PathSecurity::IsUNCPath
bool IsUNCPath(const std::wstring& path) {
    return path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\';
}

// Reimpl of PathSecurity::ContainsTraversal
bool ContainsTraversal(const std::wstring& path) {
    return path.find(L"..") != std::wstring::npos ||
           path.find(L"./") != std::wstring::npos ||
           path.find(L".\\") != std::wstring::npos;
}

// Reimpl of PathSecurity::StripSubsongSuffix
std::wstring StripSubsongSuffix(const std::wstring& path) {
    const size_t pos = path.find(L"|subsong:");
    return pos == std::wstring::npos ? path : path.substr(0, pos);
}

enum class Outcome {
    DeviceRejected,
    TraversalRejected,
    UncEarlyReturn,      // accepted without touching the filesystem
    FellThroughToResolve, // reached fs::exists / canonical / GetLongPathNameW
    // Terminal states once the resolution stage is modelled as well; see the
    // short-name-expansion section at the bottom of this file.
    ResolvedWithShortNameExpansion, // canonical result on the system drive
    ResolvedWithoutExpansion        // canonical result off the system drive
};

// Reimpl of the guard chain in PassBasicPathSafetyChecks, in source order.
// Virtual-protocol and PreprocessProtocolPath handling are out of scope; inputs
// are already post-preprocessing paths.
Outcome RunGuardChain(const std::wstring& path) {
    if (path.starts_with(L"\\\\.\\") || path.starts_with(L"\\\\?\\") ||
        path.starts_with(L"\\\\.\\.") || path.starts_with(L"\\\\?\\.")) {
        return Outcome::DeviceRejected;
    }
    if (ContainsTraversal(path)) {
        return Outcome::TraversalRejected;
    }
    if (IsUNCPath(path)) {
        return Outcome::UncEarlyReturn;
    }
    return Outcome::FellThroughToResolve;
}

} // namespace

// ============================================
// UNC early return — the performance win
// ============================================

TEST(PathSecurityUncBypass, PlainUncPathSkipsFilesystemResolution) {
    EXPECT_EQ(RunGuardChain(L"\\\\nas\\music\\album\\track.flac"), Outcome::UncEarlyReturn);
}

TEST(PathSecurityUncBypass, UncShareRootSkipsFilesystemResolution) {
    EXPECT_EQ(RunGuardChain(L"\\\\nas\\music"), Outcome::UncEarlyReturn);
}

TEST(PathSecurityUncBypass, UncWithIpv4HostSkipsFilesystemResolution) {
    EXPECT_EQ(RunGuardChain(L"\\\\192.168.1.10\\media\\track.flac"), Outcome::UncEarlyReturn);
}

// ============================================
// Ordering: device-path interception still wins
// ============================================

TEST(PathSecurityUncBypass, DeviceDotPrefixStillRejected) {
    EXPECT_EQ(RunGuardChain(L"\\\\.\\PhysicalDrive0"), Outcome::DeviceRejected);
}

TEST(PathSecurityUncBypass, DeviceQuestionPrefixStillRejected) {
    EXPECT_EQ(RunGuardChain(L"\\\\?\\C:\\Windows\\System32\\config\\SAM"), Outcome::DeviceRejected);
}

TEST(PathSecurityUncBypass, DeviceQuestionUncFormStillRejected) {
    // \\?\UNC\server\share is the extended-length UNC form; it must not reach
    // the UNC early return, because \\?\ also disables path normalization.
    EXPECT_EQ(RunGuardChain(L"\\\\?\\UNC\\nas\\music\\track.flac"), Outcome::DeviceRejected);
}

TEST(PathSecurityUncBypass, DevicePipePrefixStillRejected) {
    EXPECT_EQ(RunGuardChain(L"\\\\.\\pipe\\somepipe"), Outcome::DeviceRejected);
}

// ============================================
// Ordering: traversal detection still wins
// ============================================

TEST(PathSecurityUncBypass, UncWithParentTraversalStillRejected) {
    EXPECT_EQ(RunGuardChain(L"\\\\nas\\music\\..\\..\\secrets\\key.txt"),
              Outcome::TraversalRejected);
}

TEST(PathSecurityUncBypass, UncWithDotBackslashStillRejected) {
    EXPECT_EQ(RunGuardChain(L"\\\\nas\\music\\.\\track.flac"), Outcome::TraversalRejected);
}

TEST(PathSecurityUncBypass, UncWithDotForwardSlashStillRejected) {
    EXPECT_EQ(RunGuardChain(L"\\\\nas/music/./track.flac"), Outcome::TraversalRejected);
}

// ============================================
// Local paths unaffected — no behavioural drift
// ============================================

TEST(PathSecurityUncBypass, LocalDrivePathStillResolves) {
    EXPECT_EQ(RunGuardChain(L"D:\\Music\\album\\track.flac"), Outcome::FellThroughToResolve);
}

TEST(PathSecurityUncBypass, SystemDrivePathStillResolves) {
    // Must still reach resolution so canonical / 8.3 expansion can feed the
    // blacklist and whitelist checks that follow.
    EXPECT_EQ(RunGuardChain(L"C:\\Windows\\System32\\drivers\\etc\\hosts"),
              Outcome::FellThroughToResolve);
}

TEST(PathSecurityUncBypass, ShortNameLocalPathStillResolves) {
    EXPECT_EQ(RunGuardChain(L"C:\\PROGRA~1\\evil\\payload.exe"), Outcome::FellThroughToResolve);
}

TEST(PathSecurityUncBypass, SingleBackslashPrefixIsNotUnc) {
    EXPECT_EQ(RunGuardChain(L"\\Music\\track.flac"), Outcome::FellThroughToResolve);
}

// ================================================================
// Resolution stage: system-drive gating of the 8.3 short-name
// expansion, plus the two-stage GetLongPathNameW call.
// ================================================================
//
// PassBasicPathSafetyChecks only runs GetLongPathNameW when the *canonical*
// result sits on the system drive, because 8.3 expansion exists solely to feed
// the system-drive blacklist / whitelist prefix matching.
//
// Two properties are security-relevant and pinned below:
//   1. The drive predicate reads the canonical path, not the raw path. Reading
//      the raw drive letter would let a junction D:\link -> C:\Windows\System32
//      look like a non-system drive, skip expansion, and slip a PROGRA~1-style
//      short name past the blacklist.
//   2. GetLongPathNameW is called in two stages. When the caller buffer is too
//      small the API writes nothing and returns the required length, so a
//      single MAX_PATH-buffer call silently leaves over-long paths unexpanded --
//      the same blacklist-weakening hole as (1), reached by length instead.

namespace {

// Stand-in for fs::exists + fs::canonical / fs::weakly_canonical. Injected so a
// junction can be modelled without creating one on the real filesystem.
struct CanonicalFake {
    std::wstring result;
    int calls = 0;
    std::wstring lastInput;  // what the resolution stage was actually handed

    std::wstring operator()(const std::wstring& input) {
        ++calls;
        lastInput = input;
        return result.empty() ? input : result;
    }
};

// Stand-in for GetLongPathNameW, reproducing the Win32 return contract:
//   0                          -> failure
//   chars copied (no NUL)      -> buffer was large enough
//   required length incl. NUL  -> buffer too small, nothing written
struct LongPathFake {
    std::wstring expanded;  // what the API would expand the input to
    bool fail = false;      // simulate API failure
    int calls = 0;

    DWORD operator()(const std::wstring& input, wchar_t* buffer, DWORD bufferLen) {
        ++calls;
        if (fail) {
            return 0;
        }
        const std::wstring& out = expanded.empty() ? input : expanded;
        const DWORD needed = static_cast<DWORD>(out.size());
        if (needed < bufferLen) {
            std::wmemcpy(buffer, out.c_str(), needed);
            return needed;
        }
        return needed + 1;
    }
};

using CanonicalFn = std::function<std::wstring(const std::wstring&)>;
using LongPathFn = std::function<DWORD(const std::wstring&, wchar_t*, DWORD)>;

// Reimpl of the onSystemDrive predicate. systemDrive is passed in rather than
// hardcoded to 'C' because the real member is derived from GetWindowsDirectory
// and the test machine's system drive is not guaranteed.
bool IsOnSystemDrive(const std::wstring& resolvedPath, wchar_t systemDrive) {
    return resolvedPath.length() >= 2 &&
           resolvedPath[1] == L':' &&
           static_cast<wchar_t>(::towupper(resolvedPath[0])) == systemDrive;
}

// Reimpl of the two-stage GetLongPathNameW block.
void ExpandShortName(std::wstring& resolvedPath, const LongPathFn& longPath) {
    wchar_t stackBuf[MAX_PATH];
    DWORD len = longPath(resolvedPath, stackBuf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        resolvedPath.assign(stackBuf, len);
    } else if (len >= MAX_PATH) {
        std::wstring longBuf(len, L'\0');
        DWORD written = longPath(resolvedPath, longBuf.data(), len);
        if (written > 0 && written < len) {
            longBuf.resize(written);
            resolvedPath = std::move(longBuf);
        }
    }
}

struct ChainResult {
    Outcome outcome;
    std::wstring resolvedPath;
};

// Reimpl of the whole guard chain including the resolution stage, in source
// order: early guards -> subsong strip -> canonical -> system-drive gate ->
// 8.3 expansion.
ChainResult RunFullChain(const std::wstring& path,
                         wchar_t systemDrive,
                         const CanonicalFn& canonical,
                         const LongPathFn& longPath) {
    const Outcome early = RunGuardChain(path);
    if (early != Outcome::FellThroughToResolve) {
        return {early, path};
    }
    std::wstring resolved = canonical(StripSubsongSuffix(path));
    if (!IsOnSystemDrive(resolved, systemDrive)) {
        return {Outcome::ResolvedWithoutExpansion, resolved};
    }
    ExpandShortName(resolved, longPath);
    return {Outcome::ResolvedWithShortNameExpansion, resolved};
}

} // namespace

// ============================================
// Drive gating — positive direction
// ============================================

TEST(PathSecurityShortNameGate, SystemDrivePathExpandsShortName) {
    CanonicalFake canonical{L"C:\\PROGRA~1\\evil\\payload.exe"};
    LongPathFake longPath{L"C:\\Program Files\\evil\\payload.exe"};

    const ChainResult r = RunFullChain(L"C:\\PROGRA~1\\evil\\payload.exe", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::ResolvedWithShortNameExpansion);
    EXPECT_EQ(r.resolvedPath, L"C:\\Program Files\\evil\\payload.exe");
    EXPECT_EQ(longPath.calls, 1);
}

TEST(PathSecurityShortNameGate, SystemDriveIsParameterisedNotHardcodedToC) {
    // Same input, but the machine's system drive is E:. The C: path must now be
    // treated as a non-system drive and skip expansion.
    CanonicalFake canonical{L"C:\\PROGRA~1\\evil\\payload.exe"};
    LongPathFake longPath{L"C:\\Program Files\\evil\\payload.exe"};

    const ChainResult r = RunFullChain(L"C:\\PROGRA~1\\evil\\payload.exe", L'E',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::ResolvedWithoutExpansion);
    EXPECT_EQ(longPath.calls, 0);
}

// ============================================
// Drive gating — negative direction
// ============================================

TEST(PathSecurityShortNameGate, NonSystemDriveDSkipsExpansion) {
    CanonicalFake canonical{L"D:\\MUSIC~1\\album\\track.flac"};
    LongPathFake longPath{L"D:\\Music Library\\album\\track.flac"};

    const ChainResult r = RunFullChain(L"D:\\MUSIC~1\\album\\track.flac", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::ResolvedWithoutExpansion);
    EXPECT_EQ(r.resolvedPath, L"D:\\MUSIC~1\\album\\track.flac");
    EXPECT_EQ(longPath.calls, 0);
}

TEST(PathSecurityShortNameGate, NonSystemDriveESkipsExpansion) {
    CanonicalFake canonical{L"E:\\PROGRA~1\\tool.exe"};
    LongPathFake longPath{L"E:\\Program Files\\tool.exe"};

    const ChainResult r = RunFullChain(L"E:\\PROGRA~1\\tool.exe", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::ResolvedWithoutExpansion);
    EXPECT_EQ(longPath.calls, 0);
}

// ============================================
// Junction counter-example — the security-critical case
// ============================================

TEST(PathSecurityShortNameGate, JunctionOffSystemDriveResolvingOntoSystemDriveExpands) {
    // Raw path is on D: (non-system), but the junction resolves onto the system
    // drive. Deciding on the raw drive letter would skip expansion here and let
    // PROGRA~1 reach the blacklist unexpanded -- a bypass channel. The predicate
    // must therefore read the canonical result.
    CanonicalFake canonical{L"C:\\Windows\\System32\\PROGRA~1\\payload.dll"};
    LongPathFake longPath{L"C:\\Windows\\System32\\Program Files\\payload.dll"};

    const ChainResult r = RunFullChain(L"D:\\link\\PROGRA~1\\payload.dll", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::ResolvedWithShortNameExpansion);
    EXPECT_EQ(r.resolvedPath, L"C:\\Windows\\System32\\Program Files\\payload.dll");
    EXPECT_EQ(longPath.calls, 1);
}

TEST(PathSecurityShortNameGate, RawDriveLetterAloneWouldMisclassifyTheJunction) {
    // Explicit statement of the wrong implementation: the raw path's drive
    // letter is D, so a raw-based gate reports "not on system drive" even though
    // the canonical target is C:\Windows\System32.
    const std::wstring raw = L"D:\\link\\PROGRA~1\\payload.dll";
    const std::wstring canonical = L"C:\\Windows\\System32\\PROGRA~1\\payload.dll";

    EXPECT_FALSE(IsOnSystemDrive(raw, L'C'));
    EXPECT_TRUE(IsOnSystemDrive(canonical, L'C'));
}

TEST(PathSecurityShortNameGate, JunctionOntoNonSystemDriveStillSkipsExpansion) {
    // Mirror case: raw looks like the system drive, canonical lands on D:.
    // Skipping expansion is correct and strictly narrows, not widens, coverage.
    CanonicalFake canonical{L"D:\\Media\\MUSIC~1\\track.flac"};
    LongPathFake longPath{L"D:\\Media\\Music Library\\track.flac"};

    const ChainResult r = RunFullChain(L"C:\\mount\\MUSIC~1\\track.flac", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::ResolvedWithoutExpansion);
    EXPECT_EQ(longPath.calls, 0);
}

// ============================================
// Ordering contract: the gate sits after the UNC early return
// ============================================

TEST(PathSecurityShortNameGate, UncPathNeverReachesCanonicalOrExpansion) {
    CanonicalFake canonical{L"C:\\should\\not\\be\\used"};
    LongPathFake longPath{L"C:\\should\\not\\be\\used"};

    const ChainResult r = RunFullChain(L"\\\\nas\\music\\album\\track.flac", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::UncEarlyReturn);
    EXPECT_EQ(canonical.calls, 0);
    EXPECT_EQ(longPath.calls, 0);
}

TEST(PathSecurityShortNameGate, DevicePathNeverReachesCanonicalOrExpansion) {
    CanonicalFake canonical{L"C:\\Windows\\System32\\config\\SAM"};
    LongPathFake longPath{L"C:\\Windows\\System32\\config\\SAM"};

    const ChainResult r = RunFullChain(L"\\\\?\\C:\\Windows\\System32\\config\\SAM", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::DeviceRejected);
    EXPECT_EQ(canonical.calls, 0);
    EXPECT_EQ(longPath.calls, 0);
}

TEST(PathSecurityShortNameGate, TraversalPathNeverReachesCanonicalOrExpansion) {
    CanonicalFake canonical{L"C:\\Windows\\System32\\drivers\\etc\\hosts"};
    LongPathFake longPath{L"C:\\Windows\\System32\\drivers\\etc\\hosts"};

    const ChainResult r = RunFullChain(L"C:\\Users\\..\\Windows\\System32\\config\\SAM", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::TraversalRejected);
    EXPECT_EQ(canonical.calls, 0);
    EXPECT_EQ(longPath.calls, 0);
}

// ============================================
// Two-stage GetLongPathNameW
// ============================================

TEST(PathSecurityLongPathTwoStage, ShortResultUsesStackBufferOnly) {
    std::wstring resolved = L"C:\\PROGRA~1\\app.exe";
    LongPathFake longPath{L"C:\\Program Files\\app.exe"};

    ExpandShortName(resolved, std::ref(longPath));

    EXPECT_EQ(resolved, L"C:\\Program Files\\app.exe");
    EXPECT_EQ(longPath.calls, 1);
}

TEST(PathSecurityLongPathTwoStage, OverMaxPathResultTriggersSecondCallAndStillExpands) {
    // Expanded form exceeds MAX_PATH, so the first call writes nothing and
    // returns the required length. The old single-stage code dropped the result
    // here and left the 8.3 name in place.
    const std::wstring expanded =
        L"C:\\Program Files\\" + std::wstring(MAX_PATH + 64, L'a') + L"\\app.exe";
    ASSERT_GT(expanded.size(), static_cast<size_t>(MAX_PATH));

    std::wstring resolved = L"C:\\PROGRA~1\\" + std::wstring(MAX_PATH + 64, L'a') + L"\\app.exe";
    LongPathFake longPath{expanded};

    ExpandShortName(resolved, std::ref(longPath));

    EXPECT_EQ(resolved, expanded);
    EXPECT_EQ(longPath.calls, 2);
    EXPECT_EQ(resolved.find(L"PROGRA~1"), std::wstring::npos);
}

TEST(PathSecurityLongPathTwoStage, ExactlyMaxPathResultTriggersSecondCall) {
    // Boundary: a result of exactly MAX_PATH chars does not fit alongside the
    // NUL, so it must take the second stage rather than the len < MAX_PATH path.
    std::wstring expanded = L"C:\\Program Files\\";
    expanded.append(MAX_PATH - expanded.size(), L'b');
    ASSERT_EQ(expanded.size(), static_cast<size_t>(MAX_PATH));

    std::wstring resolved = L"C:\\PROGRA~1\\app.exe";
    LongPathFake longPath{expanded};

    ExpandShortName(resolved, std::ref(longPath));

    EXPECT_EQ(resolved, expanded);
    EXPECT_EQ(longPath.calls, 2);
}

TEST(PathSecurityLongPathTwoStage, JustUnderMaxPathResultUsesFirstCall) {
    std::wstring expanded = L"C:\\Program Files\\";
    expanded.append(MAX_PATH - 1 - expanded.size(), L'b');
    ASSERT_EQ(expanded.size(), static_cast<size_t>(MAX_PATH - 1));

    std::wstring resolved = L"C:\\PROGRA~1\\app.exe";
    LongPathFake longPath{expanded};

    ExpandShortName(resolved, std::ref(longPath));

    EXPECT_EQ(resolved, expanded);
    EXPECT_EQ(longPath.calls, 1);
}

TEST(PathSecurityLongPathTwoStage, ApiFailureKeepsOriginalResolvedPath) {
    std::wstring resolved = L"C:\\PROGRA~1\\app.exe";
    LongPathFake longPath{L"C:\\Program Files\\app.exe", /*fail=*/true};

    ExpandShortName(resolved, std::ref(longPath));

    EXPECT_EQ(resolved, L"C:\\PROGRA~1\\app.exe");
    EXPECT_EQ(longPath.calls, 1);
}

TEST(PathSecurityLongPathTwoStage, OverMaxPathThenFailureOnSecondCallKeepsOriginal) {
    // First call reports the required length, second call fails. resolvedPath
    // must be left untouched rather than replaced with a NUL-filled buffer.
    const std::wstring original = L"C:\\PROGRA~1\\app.exe";
    std::wstring resolved = original;

    int calls = 0;
    const std::wstring expanded(MAX_PATH + 32, L'c');
    LongPathFn longPath = [&](const std::wstring&, wchar_t*, DWORD) -> DWORD {
        ++calls;
        return calls == 1 ? static_cast<DWORD>(expanded.size()) + 1 : 0;
    };

    ExpandShortName(resolved, longPath);

    EXPECT_EQ(resolved, original);
    EXPECT_EQ(calls, 2);
}

TEST(PathSecurityLongPathTwoStage, FullChainExpandsOverMaxPathOnSystemDrive) {
    const std::wstring rawTail = std::wstring(MAX_PATH + 40, L'd');
    const std::wstring raw = L"C:\\PROGRA~1\\" + rawTail + L"\\app.exe";
    const std::wstring expanded = L"C:\\Program Files\\" + rawTail + L"\\app.exe";

    CanonicalFake canonical{raw};
    LongPathFake longPath{expanded};

    const ChainResult r = RunFullChain(raw, L'C', std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::ResolvedWithShortNameExpansion);
    EXPECT_EQ(r.resolvedPath, expanded);
    EXPECT_EQ(longPath.calls, 2);
}

// ============================================
// Drive predicate boundaries
// ============================================

TEST(PathSecurityShortNameGate, EmptyPathIsNotOnSystemDrive) {
    EXPECT_FALSE(IsOnSystemDrive(L"", L'C'));
}

TEST(PathSecurityShortNameGate, SingleCharPathIsNotOnSystemDrive) {
    // length < 2 must short-circuit before indexing [1].
    EXPECT_FALSE(IsOnSystemDrive(L"C", L'C'));
}

TEST(PathSecurityShortNameGate, BareDriveWithColonIsOnSystemDrive) {
    EXPECT_TRUE(IsOnSystemDrive(L"C:", L'C'));
}

TEST(PathSecurityShortNameGate, PathWithoutColonIsNotOnSystemDrive) {
    EXPECT_FALSE(IsOnSystemDrive(L"CX\\Windows", L'C'));
    EXPECT_FALSE(IsOnSystemDrive(L"\\Windows\\System32", L'C'));
    EXPECT_FALSE(IsOnSystemDrive(L"Windows", L'C'));
}

TEST(PathSecurityShortNameGate, DriveLetterComparisonIsCaseInsensitive) {
    EXPECT_TRUE(IsOnSystemDrive(L"c:\\Windows\\System32", L'C'));
    EXPECT_TRUE(IsOnSystemDrive(L"C:\\Windows\\System32", L'C'));
}

TEST(PathSecurityShortNameGate, LowercaseSystemDrivePathExpands) {
    CanonicalFake canonical{L"c:\\PROGRA~1\\app.exe"};
    LongPathFake longPath{L"c:\\Program Files\\app.exe"};

    const ChainResult r = RunFullChain(L"c:\\PROGRA~1\\app.exe", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::ResolvedWithShortNameExpansion);
    EXPECT_EQ(longPath.calls, 1);
}

TEST(PathSecurityShortNameGate, ForwardSlashSystemDrivePathExpands) {
    // fs::canonical normalises separators, but the predicate only inspects the
    // first two characters, so separator style must not matter.
    CanonicalFake canonical{L"C:/PROGRA~1/app.exe"};
    LongPathFake longPath{L"C:/Program Files/app.exe"};

    const ChainResult r = RunFullChain(L"C:/PROGRA~1/app.exe", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::ResolvedWithShortNameExpansion);
    EXPECT_EQ(longPath.calls, 1);
}

// ============================================
// Subsong suffix stripping
// ============================================
//
// '|' is reserved in Win32 filenames, so a "|subsong:N" string can never name a
// real file: fs::exists always missed and resolution fell to weakly_canonical,
// which probes upward one directory level at a time (measured P50 1144us versus
// 163us for a canonical hit). Stripping the suffix resolves the container file
// itself instead.
//
// The suffix only selects a track inside that container, and every drive,
// UNC and allow/deny decision is made on directory prefixes, so removing it
// cannot change a verdict. These cases pin that down, plus the ordering
// constraint that makes it safe.

TEST(PathSecuritySubsongStrip, ResolutionStageReceivesContainerPath) {
    CanonicalFake canonical{};  // echoes its input
    LongPathFake longPath{};

    const ChainResult r = RunFullChain(L"D:\\Music\\Album.flac|subsong:3", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(canonical.lastInput, L"D:\\Music\\Album.flac");
    EXPECT_EQ(r.resolvedPath, L"D:\\Music\\Album.flac");
}

TEST(PathSecuritySubsongStrip, PathWithoutSuffixIsUntouched) {
    EXPECT_EQ(StripSubsongSuffix(L"D:\\Music\\Album.flac"), L"D:\\Music\\Album.flac");
    EXPECT_EQ(StripSubsongSuffix(L""), L"");
}

TEST(PathSecuritySubsongStrip, OnlyFirstOccurrenceBoundsTheResult) {
    // A second occurrence lives inside the part being discarded, so the cut
    // point is the first one.
    EXPECT_EQ(StripSubsongSuffix(L"D:\\a.flac|subsong:1|subsong:2"), L"D:\\a.flac");
}

TEST(PathSecuritySubsongStrip, MalformedIndexStillStripsSuffix) {
    // Index parsing is the caller's concern; this stage only needs the container
    // path, so a non-numeric index must not leave the suffix attached.
    EXPECT_EQ(StripSubsongSuffix(L"D:\\a.flac|subsong:abc"), L"D:\\a.flac");
    EXPECT_EQ(StripSubsongSuffix(L"D:\\a.flac|subsong:"), L"D:\\a.flac");
}

// Ordering — the security-critical part
// ============================================

TEST(PathSecuritySubsongStrip, TraversalInsideSuffixIsStillRejected) {
    // ContainsTraversal runs on the full string before anything is stripped. If
    // the strip moved ahead of it, "..' hidden after the separator would escape
    // the check.
    EXPECT_EQ(RunGuardChain(L"D:\\a.flac|subsong:..\\..\\Windows"),
              Outcome::TraversalRejected);
    EXPECT_EQ(RunGuardChain(L"D:\\..\\a.flac|subsong:1"),
              Outcome::TraversalRejected);
}

TEST(PathSecuritySubsongStrip, DevicePathWithSuffixIsStillRejected) {
    EXPECT_EQ(RunGuardChain(L"\\\\.\\PhysicalDrive0|subsong:1"),
              Outcome::DeviceRejected);
    EXPECT_EQ(RunGuardChain(L"\\\\?\\C:\\Windows\\x.dll|subsong:1"),
              Outcome::DeviceRejected);
}

TEST(PathSecuritySubsongStrip, UncPathKeepsSuffixBecauseItReturnsEarlier) {
    // The UNC early return precedes the strip, and UNC paths never reach the
    // allow/deny lists, so leaving the suffix on costs nothing.
    CanonicalFake canonical{};
    LongPathFake longPath{};

    const ChainResult r = RunFullChain(L"\\\\nas\\music\\a.flac|subsong:2", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::UncEarlyReturn);
    EXPECT_EQ(r.resolvedPath, L"\\\\nas\\music\\a.flac|subsong:2");
    EXPECT_EQ(canonical.calls, 0);
}

// Verdict preservation
// ============================================

TEST(PathSecuritySubsongStrip, BlacklistedContainerStillReachesExpansion) {
    // Stripping must not let a system-drive path skip 8.3 expansion, which is
    // what feeds the deny list.
    CanonicalFake canonical{L"C:\\Windows\\System32\\PROGRA~1\\x.dll"};
    LongPathFake longPath{L"C:\\Windows\\System32\\Program Files\\x.dll"};

    const ChainResult r = RunFullChain(L"C:\\Windows\\System32\\PROGRA~1\\x.dll|subsong:1",
                                       L'C', std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(canonical.lastInput, L"C:\\Windows\\System32\\PROGRA~1\\x.dll");
    EXPECT_EQ(r.outcome, Outcome::ResolvedWithShortNameExpansion);
    EXPECT_EQ(r.resolvedPath, L"C:\\Windows\\System32\\Program Files\\x.dll");
}

TEST(PathSecuritySubsongStrip, NonSystemDriveContainerStillSkipsExpansion) {
    CanonicalFake canonical{L"D:\\Music\\MUSIC~1.FLA"};
    LongPathFake longPath{};

    const ChainResult r = RunFullChain(L"D:\\Music\\MUSIC~1.FLA|subsong:5", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(r.outcome, Outcome::ResolvedWithoutExpansion);
    EXPECT_EQ(longPath.calls, 0);
}

TEST(PathSecuritySubsongStrip, StrippedPathIsWhatGetsSymlinkResolved) {
    // The behavioural change worth stating: the suffix used to force the
    // weakly_canonical branch, which leaves the final component unresolved.
    // Stripping lets the container hit fs::exists, so a junction on that
    // component is now followed. Strictly narrowing - a container reached
    // through a link onto the system drive can no longer present itself as an
    // off-drive path.
    CanonicalFake canonical{L"C:\\Windows\\System32\\a.flac"};
    LongPathFake longPath{L"C:\\Windows\\System32\\a.flac"};

    const ChainResult r = RunFullChain(L"D:\\link\\a.flac|subsong:1", L'C',
                                       std::ref(canonical), std::ref(longPath));

    EXPECT_EQ(canonical.lastInput, L"D:\\link\\a.flac");
    EXPECT_EQ(r.outcome, Outcome::ResolvedWithShortNameExpansion);
    EXPECT_EQ(r.resolvedPath, L"C:\\Windows\\System32\\a.flac");
}
