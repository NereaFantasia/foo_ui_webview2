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
//     device-path interception
//     -> traversal detection
//     -> mapped-drive rewrite (X:\ or X:/ on a mapped letter -> \\host\share\rest)
//     -> UNC classification (remote: early return; loopback: rewrite to the
//        local root and continue; unresolved loopback: refuse; fewer than two
//        UNC segments: refuse)
//     -> subsong-suffix strip
//     -> canonical resolution
//     -> at most one re-classification when canonical yields UNC (remote:
//        return as remote; loopback: canonical once more; still UNC: refuse)
//     -> system-drive 8.3 short-name expansion
//
// Ordering is the security-relevant property: device paths (\\.\ and \\?\) share
// the leading \\ with UNC, so hoisting the UNC return above them would open a
// bypass channel.
//
// With network-share classification the early return holds only for shares
// classified as remote. Loopback shares (a \\host\share that is really a
// local volume) are rewritten to their local root and continue into the
// resolution stage, unresolvable loopbacks are refused outright, and mapped
// drive letters are rewritten to their UNC share root before classification.
// The two guards above run first, on the caller's spelling, before any
// rewrite. Cases using RunGuardChain / RunFullChain rely on their default
// classifier, which treats every share as remote.
//
// Mirrored production code (src/utils/PathSecurity.h and
// src/utils/NetworkShareResolver.h): PassBasicPathSafetyChecks (the guard
// section and the re-classification section), ClassifyAndRewriteUnc,
// SplitUncShareRoot, JoinLocalRoot, and ResolveToCanonicalForm (stood in for
// by CanonicalFn). Whenever the production step order changes, this file must
// be updated in the same change; the reimplementation is the only place the
// order is asserted.
#include "pch.h"
#include "utils/PathTraversalSegments.h"
#include <string>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <algorithm>
#include <cwctype>
#include <cwchar>

namespace {

// Reimpl of PathSecurity::IsUNCPath
bool IsUNCPath(const std::wstring& path) {
    return path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\';
}

// PathSecurity::ContainsTraversal delegates to the SDK-free classifier in
// utils/PathTraversalSegments.h, so this chain calls the real one rather than
// a copy: a change to the classifier is exercised here, not masked by a
// reimplementation that happens to agree. Its own contract (segment-based,
// not substring-based) is pinned in test_path_security_traversal.cpp.
bool ContainsTraversal(const std::wstring& path) {
    return path_traversal::ContainsTraversalSegment(path);
}

// Reimpl of PathSecurity::StripSubsongSuffix
std::wstring StripSubsongSuffix(const std::wstring& path) {
    const size_t pos = path.find(L"|subsong:");
    return pos == std::wstring::npos ? path : path.substr(0, pos);
}

enum class Outcome {
    DeviceRejected,
    TraversalRejected,
    UncEarlyReturn,      // accepted without touching the filesystem (remote share)
    FellThroughToResolve, // reached fs::exists / canonical / GetLongPathNameW
    LoopbackUnresolvedRejected,   // share proven local but not locatable
    MalformedUncRejected,         // \\ prefix without both a host and a share name
    // Terminal states once the resolution stage is modelled as well; see the
    // short-name-expansion section at the bottom of this file.
    DidNotConvergeRejected,       // canonical still UNC after the one allowed re-pass
    ResolvedWithShortNameExpansion, // canonical result on the system drive
    ResolvedWithoutExpansion        // canonical result off the system drive
};

// Reimpl of fb2k_utils::SplitUncShareRoot. Works on a copy in which every /
// is folded to a backslash and cuts \\host\share[\rest] into (\\host\share,
// \rest); a lone trailing separator counts as an empty rest. Fewer than two
// segments is a failure, which the caller turns into a refusal.
std::optional<std::pair<std::wstring, std::wstring>> SplitUncShareRoot(const std::wstring& path) {
    std::wstring p = path;
    std::replace(p.begin(), p.end(), L'/', L'\\');
    if (p.size() < 2 || p[0] != L'\\' || p[1] != L'\\') {
        return std::nullopt;
    }
    const size_t hostBegin = 2;
    const size_t hostEnd = p.find(L'\\', hostBegin);
    if (hostEnd == std::wstring::npos || hostEnd == hostBegin) {
        return std::nullopt;
    }
    const size_t shareBegin = hostEnd + 1;
    size_t shareEnd = p.find(L'\\', shareBegin);
    if (shareEnd == std::wstring::npos) {
        shareEnd = p.size();
    }
    if (shareEnd == shareBegin) {
        return std::nullopt;
    }
    std::wstring root = p.substr(0, shareEnd);
    std::wstring rest = p.substr(shareEnd);
    if (rest == L"\\") {
        rest.clear();
    }
    return std::make_pair(std::move(root), std::move(rest));
}

// Reimpl of fb2k_utils::JoinLocalRoot. Both \ and / count as separators: a
// root ending in one and a rest starting with one collapse to a single
// separator; a root without one and a non-empty rest without one get a
// backslash inserted; anything else is appended as-is (so a forward slash in
// rest is kept, only the share-root split normalises separators).
std::wstring JoinLocalRoot(std::wstring root, std::wstring rest) {
    const auto isSep = [](wchar_t c) { return c == L'\\' || c == L'/'; };
    const bool rootEndsWithSep = !root.empty() && isSep(root.back());
    const bool restStartsWithSep = !rest.empty() && isSep(rest.front());
    if (rootEndsWithSep && restStartsWithSep) {
        rest.erase(0, 1);
    } else if (!rootEndsWithSep && !rest.empty() && !restStartsWithSep) {
        root.push_back(L'\\');
    }
    return root + rest;
}

// Stand-in for NetworkShareResolver::Classify. Defaults to "every UNC share is
// remote"; loopback and unresolved cases must supply a different verdict.
enum class FakeKind { Remote, Loopback, LoopbackUnresolved };
struct ClassifyFake {
    FakeKind kind = FakeKind::Remote;
    std::wstring localRoot;   // used when kind == Loopback
    int calls = 0;
    FakeKind operator()(const std::wstring&) { ++calls; return kind; }
};

// Stand-in for the mapped-drive table: letter -> share root, or empty. The
// case folding lives here because it belongs to the replaced
// ShareRootOfLetter, which upper-cases the queried letter before comparing;
// table keys are therefore upper-case letters.
struct MappedFake {
    std::map<wchar_t, std::wstring> table;
    int calls = 0;
    std::wstring operator()(wchar_t l) {
        ++calls;
        const auto it = table.find(static_cast<wchar_t>(::towupper(l)));
        return it == table.end() ? std::wstring() : it->second;
    }
};
using MappedFn = std::function<std::wstring(wchar_t)>;
inline std::wstring NoMappedDrives(wchar_t) { return L""; }

struct GuardResult {
    Outcome outcome;
    std::wstring path;   // what the resolution stage would receive
};

// Reimpl of PathSecurity::ClassifyAndRewriteUnc. Non-UNC input passes through
// untouched. A remote verdict keeps the caller's spelling, separators
// included, because the split works on a copy; a loopback verdict is rewritten
// to localRoot + rest with the separator-normalised rest, exactly as
// production hands split->second to JoinLocalRoot.
GuardResult ClassifyAndRewriteUnc(std::wstring path, ClassifyFake& classify) {
    if (!IsUNCPath(path)) {
        return {Outcome::FellThroughToResolve, path};
    }
    const auto split = SplitUncShareRoot(path);
    if (!split) {
        return {Outcome::MalformedUncRejected, path};
    }
    const FakeKind kind = classify(split->first);
    if (kind == FakeKind::Remote) {
        return {Outcome::UncEarlyReturn, path};
    }
    if (kind == FakeKind::LoopbackUnresolved) {
        return {Outcome::LoopbackUnresolvedRejected, path};
    }
    return {Outcome::FellThroughToResolve, JoinLocalRoot(classify.localRoot, split->second)};
}

// Reimpl of the guard section of PassBasicPathSafetyChecks, in source order.
// Virtual-protocol and PreprocessProtocolPath handling are out of scope; inputs
// are already post-preprocessing paths.
GuardResult RunGuardChainEx(std::wstring path,
                            ClassifyFake& classify,
                            const MappedFn& mapped = NoMappedDrives) {
    if (path.starts_with(L"\\\\.\\") || path.starts_with(L"\\\\?\\") ||
        path.starts_with(L"\\\\.\\.") || path.starts_with(L"\\\\?\\.")) {
        return {Outcome::DeviceRejected, path};
    }
    if (ContainsTraversal(path)) {
        return {Outcome::TraversalRejected, path};
    }
    // Mapped-drive rewrite fires only on the absolute forms X:\ and X:/. A
    // drive-relative spelling (X:dir\f, bare X:) means "under the current
    // directory of X:" in Win32, so it is left for the resolution stage rather
    // than rewritten into an object the OS would not open.
    if (path.length() >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) {
        const std::wstring share = mapped(path[0]);
        if (!share.empty()) path = JoinLocalRoot(share, path.substr(2));
    }
    return ClassifyAndRewriteUnc(path, classify);
}

// Guard-chain helper with a remote-share classifier and no mapped drives.
Outcome RunGuardChain(const std::wstring& path) {
    ClassifyFake allRemote;
    return RunGuardChainEx(path, allRemote).outcome;
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
// order: early guards -> subsong strip -> canonical -> one loopback re-pass ->
// system-drive gate -> 8.3 expansion.
ChainResult RunFullChain(const std::wstring& path,
                         wchar_t systemDrive,
                         const CanonicalFn& canonical,
                         const LongPathFn& longPath,
                         ClassifyFake* classify = nullptr,
                         const MappedFn& mapped = NoMappedDrives) {
    ClassifyFake allRemote;
    ClassifyFake& cls = classify ? *classify : allRemote;
    const GuardResult early = RunGuardChainEx(path, cls, mapped);
    if (early.outcome != Outcome::FellThroughToResolve) {
        return {early.outcome, early.path};
    }
    std::wstring resolved = canonical(StripSubsongSuffix(early.path));
    if (IsUNCPath(resolved)) {
        // One re-pass through classification. A remote verdict here is returned
        // as remote (the caller's spelling is what callerForm keeps); a loopback
        // verdict is resolved once more and must then be local. Like the
        // re-classification section of the production PassBasicPathSafetyChecks,
        // only ClassifyAndRewriteUnc runs here: the device, traversal and
        // mapped-drive guards are not repeated on the canonical output.
        const GuardResult second = ClassifyAndRewriteUnc(resolved, cls);
        if (second.outcome == Outcome::UncEarlyReturn) {
            return {Outcome::UncEarlyReturn, second.path};
        }
        if (second.outcome != Outcome::FellThroughToResolve) {
            return {second.outcome, second.path};
        }
        resolved = canonical(second.path);
        if (IsUNCPath(resolved)) {
            return {Outcome::DidNotConvergeRejected, resolved};
        }
    }
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

// ============================================
// Network-share classification: ordering and refusal outcomes
// ============================================
//
// Shares return early only when classified as remote;
// loopback shares are rewritten to their local root and go through the
// resolution stage, and mapped drives are rewritten to UNC before
// classification. The refusal outcomes distinguish:
// LoopbackUnresolvedRejected (share proven local but not locatable),
// MalformedUncRejected (a \\ prefix with fewer than two segments), and
// DidNotConvergeRejected (still UNC after the one allowed re-pass). The
// ordering contract requires device-path interception and traversal detection
// to run first, on the caller's spelling, before any rewrite.

TEST(PathSecurityShareClassify, LoopbackRewritesToLocalRootAndResolves) {
    ClassifyFake loopback{FakeKind::Loopback, L"E:\\"};
    const GuardResult r = RunGuardChainEx(L"\\\\localhost\\E$\\OST\\x.mp3", loopback);
    EXPECT_EQ(r.outcome, Outcome::FellThroughToResolve);
    EXPECT_EQ(r.path, L"E:\\OST\\x.mp3");
    EXPECT_EQ(loopback.calls, 1);
}

TEST(PathSecurityShareClassify, LoopbackRootWithoutTrailingSeparatorJoinsCleanly) {
    ClassifyFake loopback{FakeKind::Loopback, L"E:\\OST"};
    const GuardResult r = RunGuardChainEx(L"\\\\localhost\\ost$\\x.mp3", loopback);
    EXPECT_EQ(r.outcome, Outcome::FellThroughToResolve);
    EXPECT_EQ(r.path, L"E:\\OST\\x.mp3");
}

TEST(PathSecurityShareClassify, LoopbackUnresolvedIsRefusedNotTreatedAsRemote) {
    ClassifyFake unresolved{FakeKind::LoopbackUnresolved};
    EXPECT_EQ(RunGuardChainEx(L"\\\\localhost\\C$\\Windows\\win.ini", unresolved).outcome,
              Outcome::LoopbackUnresolvedRejected);
}

TEST(PathSecurityShareClassify, DevicePathStillWinsOverLoopbackRewrite) {
    ClassifyFake loopback{FakeKind::Loopback, L"C:\\"};
    EXPECT_EQ(RunGuardChainEx(L"\\\\?\\UNC\\localhost\\C$\\Windows\\win.ini", loopback).outcome,
              Outcome::DeviceRejected);
    EXPECT_EQ(loopback.calls, 0);
}

TEST(PathSecurityShareClassify, TraversalStillWinsOverLoopbackRewrite) {
    ClassifyFake loopback{FakeKind::Loopback, L"C:\\"};
    EXPECT_EQ(RunGuardChainEx(L"\\\\localhost\\C$\\Users\\..\\Windows\\win.ini", loopback).outcome,
              Outcome::TraversalRejected);
    EXPECT_EQ(loopback.calls, 0);
}

TEST(PathSecurityShareClassify, MappedDriveIsRewrittenToUncBeforeClassification) {
    ClassifyFake remote;
    MappedFake m;
    m.table[L'Z'] = L"\\\\nas\\music";
    const GuardResult r = RunGuardChainEx(L"Z:\\album\\track.flac", remote, std::ref(m));
    EXPECT_EQ(r.outcome, Outcome::UncEarlyReturn);
    EXPECT_EQ(r.path, L"\\\\nas\\music\\album\\track.flac");
    EXPECT_EQ(remote.calls, 1);
}

TEST(PathSecurityShareClassify, MappedDriveToLoopbackShareResolvesLocally) {
    ClassifyFake loopback{FakeKind::Loopback, L"E:\\"};
    const MappedFn mapped = [](wchar_t l) { return l == L'Z' ? std::wstring(L"\\\\localhost\\E$") : std::wstring(); };
    const GuardResult r = RunGuardChainEx(L"Z:\\OST\\x.mp3", loopback, mapped);
    EXPECT_EQ(r.outcome, Outcome::FellThroughToResolve);
    EXPECT_EQ(r.path, L"E:\\OST\\x.mp3");
}

TEST(PathSecurityShareClassify, TraversalOnMappedDriveIsCaughtBeforeRewrite) {
    ClassifyFake remote;
    MappedFake m;
    m.table[L'Z'] = L"\\\\nas\\music";
    EXPECT_EQ(RunGuardChainEx(L"Z:\\..\\secret", remote, std::ref(m)).outcome, Outcome::TraversalRejected);
    EXPECT_EQ(m.calls, 0);
    EXPECT_EQ(remote.calls, 0);
}

TEST(PathSecurityShareClassify, UnmappedLetterGoesStraightToResolution) {
    ClassifyFake remote;
    const GuardResult r = RunGuardChainEx(L"D:\\Music\\x.flac", remote);
    EXPECT_EQ(r.outcome, Outcome::FellThroughToResolve);
    EXPECT_EQ(remote.calls, 0);
}

TEST(PathSecurityShareClassify, CanonicalYieldingUncOfRemoteShareReturnsRemoteOnSecondPass) {
    // Mapped table is stale, so Z: is not rewritten; canonical turns it into UNC;
    // the re-pass classifies it as remote and returns the remote target. (The
    // caller's Z: spelling survives in callerForm on the real type; this
    // reimplementation only models the outcome.)
    CanonicalFake canonical{L"\\\\nas\\music\\x.flac"};
    LongPathFake longPath{};
    ClassifyFake remote;
    const ChainResult r = RunFullChain(L"Z:\\x.flac", L'C', std::ref(canonical), std::ref(longPath), &remote);
    EXPECT_EQ(r.outcome, Outcome::UncEarlyReturn);
    EXPECT_EQ(r.resolvedPath, L"\\\\nas\\music\\x.flac");
    EXPECT_EQ(remote.calls, 1);
    EXPECT_EQ(canonical.calls, 1);
}

TEST(PathSecurityShareClassify, SecondPassUnresolvedLoopbackIsRefused) {
    CanonicalFake canonical{L"\\\\localhost\\E$\\x.flac"};
    LongPathFake longPath{};
    ClassifyFake unresolved{FakeKind::LoopbackUnresolved};
    const ChainResult r = RunFullChain(L"Z:\\x.flac", L'C', std::ref(canonical), std::ref(longPath), &unresolved);
    EXPECT_EQ(r.outcome, Outcome::LoopbackUnresolvedRejected);
    EXPECT_EQ(canonical.calls, 1);
}

TEST(PathSecurityShareClassify, CanonicalYieldingUncOfLoopbackShareConvergesOnSecondPass) {
    // Same stale-table scenario, but the share is loopback: the re-pass rewrites
    // to the local root and the second canonical is local.
    int calls = 0;
    CanonicalFn canonical = [&](const std::wstring& in) {
        ++calls;
        return calls == 1 ? std::wstring(L"\\\\localhost\\E$\\x.flac") : in;
    };
    LongPathFake longPath{};
    ClassifyFake loopback{FakeKind::Loopback, L"E:\\"};
    const ChainResult r = RunFullChain(L"Z:\\x.flac", L'C', canonical, std::ref(longPath), &loopback);
    EXPECT_EQ(r.outcome, Outcome::ResolvedWithoutExpansion);
    EXPECT_EQ(r.resolvedPath, L"E:\\x.flac");
    EXPECT_EQ(calls, 2);
}

TEST(PathSecurityShareClassify, StillUncAfterSecondCanonicalIsRefused) {
    CanonicalFake canonical{L"\\\\localhost\\E$\\x.flac"};   // never turns local
    LongPathFake longPath{};
    ClassifyFake loopback{FakeKind::Loopback, L"E:\\"};
    const ChainResult r = RunFullChain(L"Z:\\x.flac", L'C', std::ref(canonical), std::ref(longPath), &loopback);
    EXPECT_EQ(r.outcome, Outcome::DidNotConvergeRejected);
    EXPECT_EQ(canonical.calls, 2);
    EXPECT_EQ(loopback.calls, 1);
}

TEST(PathSecurityShareClassify, LoopbackShareInsideCanonicalGetsJunctionResolved) {
    // Server-side reparse points are invisible over SMB; once the loopback is
    // rewritten to a local path the resolution stage can follow them.
    CanonicalFake canonical{L"C:\\Windows\\System32\\x.dll"};
    LongPathFake longPath{L"C:\\Windows\\System32\\x.dll"};
    ClassifyFake loopback{FakeKind::Loopback, L"E:\\"};
    const ChainResult r = RunFullChain(L"\\\\localhost\\E$\\link\\x.dll", L'C',
                                       std::ref(canonical), std::ref(longPath), &loopback);
    EXPECT_EQ(r.outcome, Outcome::ResolvedWithShortNameExpansion);
    EXPECT_EQ(canonical.lastInput, L"E:\\link\\x.dll");
}

// Mapped-drive rewrite fires only on the absolute forms (X:\ and X:/); the
// drive-relative spellings fall through to resolution untouched.

TEST(PathSecurityShareClassify, DriveRelativeSpellingIsNotRewrittenToMappedShare) {
    // Z:dir\f means "dir\f under the current directory of Z:"; rewriting it as
    // Z:\dir\f would judge a different object than the one Win32 opens.
    ClassifyFake remote;
    MappedFake m;
    m.table[L'Z'] = L"\\\\nas\\music";
    const GuardResult r = RunGuardChainEx(L"Z:dir\\f.flac", remote, std::ref(m));
    EXPECT_EQ(r.outcome, Outcome::FellThroughToResolve);
    EXPECT_EQ(r.path, L"Z:dir\\f.flac");
    EXPECT_EQ(remote.calls, 0);
}

TEST(PathSecurityShareClassify, BareDriveLetterIsNotRewrittenToMappedShare) {
    ClassifyFake remote;
    MappedFake m;
    m.table[L'Z'] = L"\\\\nas\\music";
    const GuardResult r = RunGuardChainEx(L"Z:", remote, std::ref(m));
    EXPECT_EQ(r.outcome, Outcome::FellThroughToResolve);
    EXPECT_EQ(r.path, L"Z:");
    EXPECT_EQ(remote.calls, 0);
}

TEST(PathSecurityShareClassify, LowercaseLetterWithForwardSlashIsRewritten) {
    ClassifyFake remote;
    MappedFake m;
    m.table[L'Z'] = L"\\\\nas\\music";
    const GuardResult r = RunGuardChainEx(L"z:/album/x.flac", remote, std::ref(m));
    EXPECT_EQ(r.outcome, Outcome::UncEarlyReturn);
    EXPECT_EQ(r.path, L"\\\\nas\\music/album/x.flac");
    EXPECT_EQ(remote.calls, 1);
}

// The share-root split and the local-root join follow the production string
// rules: fewer than two UNC segments is a refusal, separators are normalised
// only for the split, and the subsong suffix survives a remote early return
// because stripping happens after classification.

TEST(PathSecurityShareClassify, HostOnlyUncIsRefusedNotTreatedAsRemote) {
    // A \\ prefix without both a host and a share names no share at all. The
    // production SplitUncShareRoot fails on it and the path is refused before
    // classification is consulted; treating it as remote would be fail-open.
    ClassifyFake remote;
    const GuardResult hostOnly = RunGuardChainEx(L"\\\\nas", remote);
    EXPECT_EQ(hostOnly.outcome, Outcome::MalformedUncRejected);
    EXPECT_EQ(hostOnly.path, L"\\\\nas");
    const GuardResult hostWithSeparator = RunGuardChainEx(L"\\\\nas\\", remote);
    EXPECT_EQ(hostWithSeparator.outcome, Outcome::MalformedUncRejected);
    EXPECT_EQ(hostWithSeparator.path, L"\\\\nas\\");
    const GuardResult prefixOnly = RunGuardChainEx(L"\\\\", remote);
    EXPECT_EQ(prefixOnly.outcome, Outcome::MalformedUncRejected);
    EXPECT_EQ(prefixOnly.path, L"\\\\");
    EXPECT_EQ(remote.calls, 0);
}

TEST(PathSecurityShareClassify, ForwardSlashLoopbackIsNormalisedBeforeRewrite) {
    // The split folds / to \ before cutting host and share, so the rest handed
    // to the join is already backslash-separated.
    ClassifyFake loopback{FakeKind::Loopback, L"E:\\"};
    const GuardResult r = RunGuardChainEx(L"\\\\localhost/E$/OST/x.mp3", loopback);
    EXPECT_EQ(r.outcome, Outcome::FellThroughToResolve);
    EXPECT_EQ(r.path, L"E:\\OST\\x.mp3");
    EXPECT_EQ(loopback.calls, 1);
}

TEST(PathSecurityShareClassify, MappedDriveRemoteKeepsSubsongSuffix) {
    // The remote early return precedes the strip, exactly as for a UNC spelling
    // typed by the caller.
    ClassifyFake remote;
    MappedFake m;
    m.table[L'Z'] = L"\\\\nas\\music";
    const GuardResult r = RunGuardChainEx(L"Z:\\OST\\x.mp3|subsong:3", remote, std::ref(m));
    EXPECT_EQ(r.outcome, Outcome::UncEarlyReturn);
    EXPECT_EQ(r.path, L"\\\\nas\\music\\OST\\x.mp3|subsong:3");
}

TEST(PathSecurityShareClassify, MappedDriveLoopbackStripsSuffixBeforeCanonical) {
    // Mapped letter -> loopback share -> local root; the suffix is then stripped
    // before the container path reaches canonical.
    CanonicalFake canonical{};  // echoes its input
    LongPathFake longPath{};
    ClassifyFake loopback{FakeKind::Loopback, L"E:\\"};
    MappedFake m;
    m.table[L'Z'] = L"\\\\localhost\\E$";
    RunFullChain(L"Z:\\OST\\x.mp3|subsong:3", L'C',
                 std::ref(canonical), std::ref(longPath), &loopback, std::ref(m));
    EXPECT_EQ(canonical.lastInput, L"E:\\OST\\x.mp3");
}
