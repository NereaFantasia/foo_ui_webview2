// test_path_security_traversal.cpp - path_traversal::ContainsTraversalSegment
//
// Exercises the *real* traversal classifier that PathSecurity::ContainsTraversal
// delegates to. It runs at the head of PassBasicPathSafetyChecks, before the
// mapped-drive rewrite, the UNC classification and the canonical resolution;
// for a remote share it is the only traversal defence, because those return
// without touching the filesystem. The function lives in its own SDK-free
// header for the same reason as PathProtocolScheme.h: this test project does
// not build against the foobar2000 SDK.
//
// The contract under test:
//   - the path is split on `\` and `/`; the unit of judgement is the segment
//   - a segment is a relative component when, with every space (U+0020)
//     removed, nothing but periods remain and there is at least one period:
//     `.`, `..`, and their Win32-trimmed spellings such as `.. ` (the last
//     segment loses trailing periods and spaces during normalisation, so
//     `.. ` is `..`); `...` falls inside the same conservative rule
//   - periods inside or at the end of a name are not traversal: `x..flac`,
//     `Wind blows....flac`, `.hidden`, `..hidden`, `album.\x.flac`
//   - empty segments (leading `\\` of UNC, doubled or trailing separators)
//     are not traversal
//   - the string is judged whole: no protocol stripping, no `|subsong:N`
//     stripping, so a `\..\` hidden after the suffix is still a segment
//
// Why the unit is the segment: a substring search for `..` also matches what
// a title ending in a period produces once the extension is appended
// (`Bonus Track..flac`). ValidateMediaAccess treats a basic-check failure as
// a hard refusal, so every such library file would be refused by the whole
// MediaRead tier - metadata.read, titleformat.*, artwork.* - which is what
// the e2e regression suite observed against a real library. The "released"
// cases below quote paths from that library.
#include "pch.h"
#include "utils/PathTraversalSegments.h"

#include <string>
#include <string_view>

namespace {

using path_traversal::ContainsTraversalSegment;
using path_traversal::IsPlainFilename;
using path_traversal::IsTraversalSegment;

} // namespace

// ============================================
// Relative components: rejected
// ============================================

TEST(PathSecurityTraversal, ParentSegmentIsTraversal) {
    EXPECT_TRUE(ContainsTraversalSegment(L"C:\\Users\\..\\Windows\\System32\\config\\SAM"));
    EXPECT_TRUE(ContainsTraversalSegment(L"D:/music/../secret"));
    EXPECT_TRUE(ContainsTraversalSegment(L"..\\x.flac"));
    EXPECT_TRUE(ContainsTraversalSegment(L"../x.flac"));
    EXPECT_TRUE(ContainsTraversalSegment(L"E:\\OST\\.."));
    EXPECT_TRUE(ContainsTraversalSegment(L"E:\\OST\\..\\"));
    EXPECT_TRUE(ContainsTraversalSegment(L".."));
}

TEST(PathSecurityTraversal, CurrentSegmentIsTraversal) {
    // `.` is not an escape by itself, but it is a relative component that
    // normalisation removes, so it is refused like `..` - including a trailing
    // `\.`, which a substring search for `.\` or `./` cannot see.
    EXPECT_TRUE(ContainsTraversalSegment(L"\\\\nas\\music\\.\\track.flac"));
    EXPECT_TRUE(ContainsTraversalSegment(L"\\\\nas/music/./track.flac"));
    EXPECT_TRUE(ContainsTraversalSegment(L"E:\\OST\\.\\x.flac"));
    EXPECT_TRUE(ContainsTraversalSegment(L"E:\\OST\\."));
    EXPECT_TRUE(ContainsTraversalSegment(L"."));
}

TEST(PathSecurityTraversal, UncAndMappedDriveFormsAreJudgedTheSameWay) {
    // These spellings are the ones the guard-chain ordering tests in
    // test_path_security_unc_bypass.cpp feed through the real chain.
    EXPECT_TRUE(ContainsTraversalSegment(L"\\\\nas\\music\\..\\..\\secrets\\key.txt"));
    EXPECT_TRUE(ContainsTraversalSegment(L"\\\\localhost\\C$\\Users\\..\\Windows\\win.ini"));
    EXPECT_TRUE(ContainsTraversalSegment(L"Z:\\..\\secret"));
}

TEST(PathSecurityTraversal, SubsongSuffixDoesNotHideATraversalSegment) {
    // The classifier runs on the full string before StripSubsongSuffix, so a
    // `\..\` hidden behind the `|subsong:` separator is still a segment.
    EXPECT_TRUE(ContainsTraversalSegment(L"D:\\a.flac|subsong:..\\..\\Windows"));
    EXPECT_TRUE(ContainsTraversalSegment(L"D:\\..\\a.flac|subsong:1"));
}

TEST(PathSecurityTraversal, TrailingSpacesAndExtraPeriodsStillMakeARelativeComponent) {
    // Win32 strips trailing periods and spaces from the last segment, so
    // `.. ` names the parent directory as surely as `..` does. A segment
    // made only of periods and spaces is therefore treated as relative
    // wherever it sits; no real media path is named that way.
    EXPECT_TRUE(ContainsTraversalSegment(L"E:\\OST\\.. "));
    EXPECT_TRUE(ContainsTraversalSegment(L"E:\\OST\\.. \\x.flac"));
    EXPECT_TRUE(ContainsTraversalSegment(L"E:\\OST\\. \\x.flac"));
    EXPECT_TRUE(ContainsTraversalSegment(L"E:\\OST\\. .\\x.flac"));
    EXPECT_TRUE(ContainsTraversalSegment(L"E:\\OST\\...\\x.flac"));
    EXPECT_TRUE(ContainsTraversalSegment(L"E:\\OST\\..."));
}

// ============================================
// Legitimate names: released
// ============================================

TEST(PathSecurityTraversal, NameEndingInAPeriodBeforeTheExtensionIsNotTraversal) {
    // Real library paths that a substring search for `..` would refuse.
    EXPECT_FALSE(ContainsTraversalSegment(
        L"E:\\OST\\Diverse System\\[DVSP-0068] AD：TRANCE (C80)\\11. doku — Sugar Beauty..flac"));
    EXPECT_FALSE(ContainsTraversalSegment(
        L"E:\\OST\\Feuille-Morte\\[M3-46] Feuille-Morte - トキノウタ {FMTS-0006} [CD-FLAC] (100% Log)\\04 - めらみぽっぷ　 - Bonus Track..flac"));
    EXPECT_FALSE(ContainsTraversalSegment(
        L"E:\\OST\\Kara no Kyoukai OST\\Complete OST Collection\\Kalafina - Re-oblivious\\03. Kimi ga Hikari ni Kaete Yuku ~ acoustic ver..flac"));
    EXPECT_FALSE(ContainsTraversalSegment(
        L"E:\\OST\\OPUS Series (SIGONO)\\[2021] Echo of Starsong Complete\\65. I'll. Help. You..mp3"));
}

TEST(PathSecurityTraversal, EllipsisInANameIsNotTraversal) {
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\FELT\\(FELT-032) START\\04 Wind blows....flac"));
    EXPECT_FALSE(ContainsTraversalSegment(
        L"E:\\OST\\OPUS Series (SIGONO)\\[2021] Echo of Starsong - Best Selection\\22. Can I... Not Like Being Alone.mp3"));
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\RD-Sounds\\宴\\07 - Unknown Ex（平安のエイリアン...）.mp3"));
    EXPECT_FALSE(ContainsTraversalSegment(
        L"E:\\OST\\RD-Sounds\\[Autumn Reitaisai 4] 凋叶棕 & 京都幻想劇団 - 秘封活動記録-祝-Original Soundtrack\\京都幻想劇団 - 秘封活動記録-祝-Original Soundtrack\\05 - こちや...早苗-.flac"));
}

TEST(PathSecurityTraversal, PeriodsAtTheStartOfANameAreNotTraversal) {
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\.hidden\\x.flac"));
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\..hidden\\x.flac"));
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\...three\\x.flac"));
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\.. spaced\\x.flac"));
}

TEST(PathSecurityTraversal, PeriodRightBeforeASeparatorInsideANameIsNotTraversal) {
    // A substring search would match these through `.\` and `./`. Win32 trims
    // the single trailing period (`album.` opens as `album`), which changes the
    // spelling of a name, not the directory level, so it is not traversal.
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\album.\\x.flac"));
    EXPECT_FALSE(ContainsTraversalSegment(L"E:/OST/album./x.flac"));
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\Vol. 2\\x.flac"));
}

TEST(PathSecurityTraversal, EmptySegmentsAreNotTraversal) {
    EXPECT_FALSE(ContainsTraversalSegment(L""));
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\"));
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\\\x.flac"));
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\album\\"));
    EXPECT_FALSE(ContainsTraversalSegment(L"\\\\nas\\music"));
    EXPECT_FALSE(ContainsTraversalSegment(L"\\\\"));
}

TEST(PathSecurityTraversal, PlainPathsAreNotTraversal) {
    EXPECT_FALSE(ContainsTraversalSegment(L"C:\\Music\\x.flac"));
    EXPECT_FALSE(ContainsTraversalSegment(L"D:/Music/x.flac"));
    EXPECT_FALSE(ContainsTraversalSegment(L"relative\\x.flac"));
    EXPECT_FALSE(ContainsTraversalSegment(L"\\\\nas\\music\\album\\track.flac"));
    EXPECT_FALSE(ContainsTraversalSegment(L"D:\\a.flac|subsong:3"));
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\Vol. 2\\01. Intro.flac"));
}

TEST(PathSecurityTraversal, SegmentsMadeOnlyOfSpacesAreNotTraversal) {
    // A space-only segment has no period; there is nothing for normalisation
    // to turn into a relative component. Left for the resolution stage.
    EXPECT_FALSE(ContainsTraversalSegment(L"E:\\OST\\ \\x.flac"));
    EXPECT_FALSE(IsTraversalSegment(L" "));
}

// ============================================
// Segment classifier
// ============================================

TEST(PathSecurityTraversal, SegmentClassifierRecognisesOnlyPeriodsAndSpaces) {
    EXPECT_TRUE(IsTraversalSegment(L"."));
    EXPECT_TRUE(IsTraversalSegment(L".."));
    EXPECT_TRUE(IsTraversalSegment(L"..."));
    EXPECT_TRUE(IsTraversalSegment(L".. "));
    EXPECT_TRUE(IsTraversalSegment(L" ."));
    EXPECT_TRUE(IsTraversalSegment(L". ."));

    EXPECT_FALSE(IsTraversalSegment(L""));
    EXPECT_FALSE(IsTraversalSegment(L"a."));
    EXPECT_FALSE(IsTraversalSegment(L"a.."));
    EXPECT_FALSE(IsTraversalSegment(L"..a"));
    EXPECT_FALSE(IsTraversalSegment(L".a"));
    EXPECT_FALSE(IsTraversalSegment(L". a"));
    EXPECT_FALSE(IsTraversalSegment(L"x..flac"));
}

TEST(PathSecurityTraversal, NarrowSegmentClassifierMatchesWide) {
    // Filename parameters arrive as UTF-8; period and space are single bytes,
    // so the byte-wise verdict must equal the wide one.
    EXPECT_TRUE(IsTraversalSegment(std::string_view("..")));
    EXPECT_TRUE(IsTraversalSegment(std::string_view(".. ")));
    EXPECT_FALSE(IsTraversalSegment(std::string_view("x..flac")));
    EXPECT_FALSE(IsTraversalSegment(std::string_view("曲名..lrc")));
}

// ============================================
// Plain filename parameters (lyrics / artwork / log output names)
// ============================================

TEST(PathSecurityTraversal, PlainFilenameAllowsPeriodsInsideTheName) {
    EXPECT_TRUE(IsPlainFilename("Bonus Track..lrc"));
    EXPECT_TRUE(IsPlainFilename("Wind blows....lrc"));
    EXPECT_TRUE(IsPlainFilename("cover..jpg"));
    EXPECT_TRUE(IsPlainFilename("..jpg"));
    EXPECT_TRUE(IsPlainFilename(".hidden.txt"));
    EXPECT_TRUE(IsPlainFilename("曲名..lrc"));
}

TEST(PathSecurityTraversal, PlainFilenameRejectsRelativeComponentsAndSeparators) {
    EXPECT_FALSE(IsPlainFilename("."));
    EXPECT_FALSE(IsPlainFilename(".."));
    EXPECT_FALSE(IsPlainFilename(".. "));
    EXPECT_FALSE(IsPlainFilename("..\\cover.jpg"));
    EXPECT_FALSE(IsPlainFilename("../cover.jpg"));
    EXPECT_FALSE(IsPlainFilename("sub\\cover.jpg"));
    EXPECT_FALSE(IsPlainFilename("sub/cover.jpg"));
    EXPECT_FALSE(IsPlainFilename("C:\\Windows\\x.log"));
}
