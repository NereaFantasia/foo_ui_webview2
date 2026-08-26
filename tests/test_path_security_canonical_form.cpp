// test_path_security_canonical_form.cpp - path_canonical::ResolveToCanonicalForm
//
// First PathSecurity-adjacent test that exercises the *real* implementation
// instead of a reimplementation. Two properties make that possible: the
// function is free-standing (no core_api-dependent singleton to construct), and
// it lives in its own header that does not pull the foobar2000 SDK -- the SDK
// drags in winsock2.h, which collides with the winsock v1 declarations this
// project's test PCH already got from Windows.h.
//
// The function is the single canonical-form authority shared by the three
// allow/deny list builders and by PassBasicPathSafetyChecks. The property under
// test is its input/output contract:
//   - empty input returns empty without touching the filesystem
//   - existing paths go through fs::canonical (symlink/junction resolution)
//   - missing paths go through fs::weakly_canonical (existing prefix resolved,
//     missing tail kept literally) and never throw
//   - "." / ".." components are collapsed
//   - the 8.3 short-name expansion is gated on the systemDrive parameter, so
//     off-system-drive inputs keep their short-name spelling
//   - resolution failures fall back to the original value instead of throwing
//
// Traversal rejection is deliberately NOT this function's job: it feeds the
// list builders (whose inputs come from trusted shell APIs), while
// ContainsTraversal guards the validation-side entry point.
#include "pch.h"
#include "utils/PathCanonicalForm.h"

#include <filesystem>
#include <string>

namespace {

namespace fs = std::filesystem;

// Any drive letter different from `d`, for parameterising the systemDrive
// argument so the 8.3 stage is provably gated off for the input at hand.
wchar_t OtherDriveThan(wchar_t d) {
    const wchar_t upper = static_cast<wchar_t>(::towupper(d));
    return upper == L'Q' ? L'R' : L'Q';
}

// A drive letter with no volume behind it, taken from the GetLogicalDrives
// bitmask. Returns 0 when all 26 letters are taken (then the caller bails out
// with SUCCEED, as this GoogleTest 1.8.1 vintage has no GTEST_SKIP).
wchar_t FindMissingDriveLetter() {
    const DWORD mask = ::GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if ((mask & (1u << i)) == 0) {
            return static_cast<wchar_t>(L'A' + i);
        }
    }
    return 0;
}

} // namespace

// ============================================
// Empty input
// ============================================

TEST(PathSecurityCanonicalForm, EmptyInputReturnsEmpty) {
    EXPECT_EQ(path_canonical::ResolveToCanonicalForm(L"", L'C'), L"");
}

// ============================================
// Existing path: fs::canonical branch
// ============================================

TEST(PathSecurityCanonicalForm, ExistingPathResolvesToEquivalentLocation) {
    // The temp directory always exists, so this pins the fs::canonical branch.
    // The output spelling is machine-dependent (junctions and known-folder
    // redirection may rewrite the prefix -- that rewrite being applied to list
    // entries is the whole point of the function), so assert identity of the
    // filesystem object rather than string equality.
    const fs::path tmp = fs::temp_directory_path();
    const std::wstring input = tmp.wstring();

    const std::wstring out =
        path_canonical::ResolveToCanonicalForm(input, OtherDriveThan(input[0]));

    ASSERT_FALSE(out.empty());
    EXPECT_TRUE(fs::exists(out));
    EXPECT_TRUE(fs::equivalent(fs::path(out), tmp));
}

// ============================================
// Missing path: fs::weakly_canonical branch
// ============================================

TEST(PathSecurityCanonicalForm, MissingPathKeepsLiteralTailAndDoesNotThrow) {
    // Existing prefix (temp) is resolved; the missing tail must be appended
    // literally, exactly as weakly_canonical specifies. This is the branch
    // list building relies on when a directory is not yet accessible during
    // singleton construction.
    const fs::path tmp = fs::temp_directory_path();
    const std::wstring input =
        (tmp / L"fb2k_ut_canonical_missing" / L"leaf.flac").wstring();

    std::wstring out;
    EXPECT_NO_THROW(
        out = path_canonical::ResolveToCanonicalForm(input, OtherDriveThan(input[0])));

    ASSERT_FALSE(out.empty());
    EXPECT_TRUE(out.ends_with(L"fb2k_ut_canonical_missing\\leaf.flac"));
}

// ============================================
// Dot / dot-dot collapsing
// ============================================

TEST(PathSecurityCanonicalForm, DotAndDotDotComponentsAreCollapsed) {
    // weakly_canonical lexically normalises before per-component resolution,
    // so "a/../b/./leaf" collapses to "b/leaf" even though neither a nor b
    // exists. The swallowed component must not survive in the output.
    const fs::path tmp = fs::temp_directory_path();
    const std::wstring input =
        (tmp / L"fb2k_ut_cf_a" / L".." / L"fb2k_ut_cf_b" / L"." / L"leaf.txt").wstring();

    const std::wstring out =
        path_canonical::ResolveToCanonicalForm(input, OtherDriveThan(input[0]));

    ASSERT_FALSE(out.empty());
    EXPECT_EQ(out.find(L".."), std::wstring::npos);
    EXPECT_EQ(out.find(L"\\.\\"), std::wstring::npos);
    EXPECT_EQ(out.find(L"fb2k_ut_cf_a"), std::wstring::npos);
    EXPECT_TRUE(out.ends_with(L"fb2k_ut_cf_b\\leaf.txt"));
}

// ============================================
// 8.3 expansion gating: off-system-drive inputs
// ============================================

TEST(PathSecurityCanonicalForm, OffSystemDriveShortNameSpellingIsPreserved) {
    // systemDrive is passed explicitly and differs from the input's drive, so
    // the 8.3 stage is gated off and the short-name spelling must survive.
    //
    // Observation limit, stated honestly: for a nonexistent path the
    // GetLongPathNameW call would fail without effect even if the gate were
    // broken, so this case pins the observable contract (short name preserved,
    // no throw) rather than proving the call was skipped. The gate's junction
    // counter-examples live in test_path_security_unc_bypass.cpp.
    const wchar_t missing = FindMissingDriveLetter();
    if (missing == 0) {
        SUCCEED() << "all 26 drive letters are in use; case not runnable here";
        return;
    }

    const std::wstring input =
        std::wstring(1, missing) + L":\\PROGRA~1\\payload.exe";

    std::wstring out;
    EXPECT_NO_THROW(
        out = path_canonical::ResolveToCanonicalForm(input, OtherDriveThan(missing)));

    EXPECT_EQ(out, input);
}

TEST(PathSecurityCanonicalForm, MissingPathShortNameAlsoSurvivesOnSystemDrive) {
    // Control case for the one above: with systemDrive equal to the input's
    // drive the 8.3 stage does run, but GetLongPathNameW fails on a missing
    // path and must leave the value untouched. Together the two cases pin
    // "missing-path short names are never rewritten, under either gating".
    const wchar_t missing = FindMissingDriveLetter();
    if (missing == 0) {
        SUCCEED() << "all 26 drive letters are in use; case not runnable here";
        return;
    }

    const std::wstring input =
        std::wstring(1, missing) + L":\\PROGRA~1\\payload.exe";

    std::wstring out;
    EXPECT_NO_THROW(out = path_canonical::ResolveToCanonicalForm(input, missing));

    EXPECT_EQ(out, input);
}

// ============================================
// Failure fallback: original value, no throw
// ============================================

TEST(PathSecurityCanonicalForm, OverlongPathFallsBackToOriginalValueWithoutThrowing) {
    // A path beyond the 32767-char NT limit makes fs::exists / weakly_canonical
    // report ERROR_FILENAME_EXCED_RANGE, which MSVC's STL does not classify as
    // not-found (xfilesystem_abi.h __std_is_file_not_found), so the throwing
    // overloads raise filesystem_error and the function's catch(...) must fall
    // back to the input. Should an STL revision reclassify the error, the
    // lexical fallback returns the same spelling -- the asserted contract
    // (no throw, original value) holds either way.
    std::wstring input = L"C:\\";
    input.append(40000, L'x');

    std::wstring out;
    EXPECT_NO_THROW(out = path_canonical::ResolveToCanonicalForm(input, L'Z'));

    EXPECT_EQ(out, input);
}
