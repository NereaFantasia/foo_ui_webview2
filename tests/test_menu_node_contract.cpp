// test_menu_node_contract.cpp - L1 tests for the unified menu node model
// (docs/menu-subsystem/SPEC.md §9.2). Drives the SAME header the API layer
// includes, not a reimplementation, so a regression in product code fails here.
//
// Coverage in this file is deliberately EXHAUSTIVE over flag bit combinations
// rather than sampled: the pre-refactor defects were all "a state bit was never
// read", which sampling does not reliably catch.
#include "pch.h"
#include "api/MenuNodeContract.h"

using namespace menu_node;

namespace {

// Every combination of the four main-menu display bits.
constexpr std::uint32_t kMainMenuAllBits =
    mainmenu_flag::kDisabled | mainmenu_flag::kChecked |
    mainmenu_flag::kRadioChecked | mainmenu_flag::kDefaultHidden;

// Every combination of the four context-menu display bits.
constexpr std::uint32_t kContextAllBits =
    contextmenu_flag::kChecked | contextmenu_flag::kDisabled |
    contextmenu_flag::kGrayed | contextmenu_flag::kRadioChecked;

}  // namespace

// ===========================================================================
// SPEC §9.2 case 1 - flag -> boolean normalization, exhaustive
// ===========================================================================

TEST(MenuNodeContractTest, MainMenuNormalizationIsExhaustivelyCorrect) {
    for (std::uint32_t bits = 0; bits <= kMainMenuAllBits; ++bits) {
        const State s = NormalizeMainMenu(bits, /*displayReturnedTrue=*/true);

        EXPECT_EQ(s.flags, bits) << "raw flags must survive untranslated";
        EXPECT_EQ(s.enabled, (bits & mainmenu_flag::kDisabled) == 0);
        EXPECT_EQ(s.radioChecked, (bits & mainmenu_flag::kRadioChecked) != 0);
        // radiochecked implies checked so a radio item never reports unchecked.
        const bool expectChecked = (bits & (mainmenu_flag::kChecked |
                                            mainmenu_flag::kRadioChecked)) != 0;
        EXPECT_EQ(s.checked, expectChecked) << "bits=" << bits;
        EXPECT_EQ(s.hidden, (bits & mainmenu_flag::kDefaultHidden) != 0);
    }
}

TEST(MenuNodeContractTest, ContextMenuNormalizationIsExhaustivelyCorrect) {
    for (std::uint32_t bits = 0; bits <= kContextAllBits; ++bits) {
        const State s = NormalizeContextMenu(bits, /*displayReturnedTrue=*/true,
                                             ContextEnabledState::DefaultOn);

        EXPECT_EQ(s.flags, bits);
        // DISABLED and GRAYED are independent: components set either or both.
        const bool expectEnabled =
            (bits & (contextmenu_flag::kDisabled | contextmenu_flag::kGrayed)) == 0;
        EXPECT_EQ(s.enabled, expectEnabled) << "bits=" << bits;
        EXPECT_EQ(s.radioChecked, (bits & contextmenu_flag::kRadioChecked) != 0);
        const bool expectChecked = (bits & (contextmenu_flag::kChecked |
                                            contextmenu_flag::kRadioChecked)) != 0;
        EXPECT_EQ(s.checked, expectChecked) << "bits=" << bits;
        EXPECT_FALSE(s.hidden);
    }
}

TEST(MenuNodeContractTest, GrayedOnlyStillCountsAsDisabled) {
    // Regression guard: testing against MFS_DISABLED (a union) or against
    // FLAG_DISABLED alone would wrongly report these as enabled.
    EXPECT_FALSE(NormalizeContextMenu(contextmenu_flag::kGrayed, true,
                                      ContextEnabledState::DefaultOn).enabled);
    EXPECT_FALSE(NormalizeHmenu(hmenu_state::kGrayed).enabled);
}

// ===========================================================================
// SPEC §9.2 case 2 - get_display() returning false means hidden, not dropped
// ===========================================================================

TEST(MenuNodeContractTest, MainMenuDisplayReturningFalseMeansHidden) {
    const State s = NormalizeMainMenu(0, /*displayReturnedTrue=*/false);

    EXPECT_TRUE(s.hidden);
    // Still a usable command otherwise - it is shortcut-only, not broken.
    EXPECT_TRUE(s.enabled);
}

TEST(MenuNodeContractTest, ContextDisplayReturningFalseMeansHidden) {
    EXPECT_TRUE(NormalizeContextMenu(0, /*displayReturnedTrue=*/false,
                                     ContextEnabledState::DefaultOn).hidden);
}

// ===========================================================================
// SPEC §9.2 case 3 - FORCE_OFF is hidden AND distinguishable from
//                    defaultHidden through the retained raw flags
// ===========================================================================

TEST(MenuNodeContractTest, ForceOffIsHidden) {
    const State s = NormalizeContextMenu(0, true, ContextEnabledState::ForceOff);
    EXPECT_TRUE(s.hidden) << "FORCE_OFF is shortcut-list-only per the SDK";
}

TEST(MenuNodeContractTest, DefaultOffIsNotHiddenByItself) {
    // DEFAULT_OFF only means "not shown by default"; the user may enable it,
    // so it must not be filtered out the way FORCE_OFF is.
    EXPECT_FALSE(NormalizeContextMenu(0, true,
                                      ContextEnabledState::DefaultOff).hidden);
}

TEST(MenuNodeContractTest, ForceOffAndDefaultHiddenRemainDistinguishable) {
    const State forceOff =
        NormalizeContextMenu(0, true, ContextEnabledState::ForceOff);
    const State defaultHidden =
        NormalizeMainMenu(mainmenu_flag::kDefaultHidden, true);

    EXPECT_TRUE(forceOff.hidden);
    EXPECT_TRUE(defaultHidden.hidden);
    // Same normalized bit, different raw evidence - advanced callers can tell
    // "user may reveal with Shift" from "never show outside the hotkey list".
    EXPECT_EQ(forceOff.flags, 0u);
    EXPECT_NE(defaultHidden.flags, 0u);
}

// ===========================================================================
// SPEC §9.2 case 4 - an unaddressable node always carries a reason
// SPEC §9.2 case 5 - a dynamic parent is never addressable
// ===========================================================================

TEST(MenuNodeContractTest, DynamicParentIsNeverAddressable) {
    // Executing a v2 dynamic slot is undefined behaviour in the SDK and was
    // observed to be able to bugcheck the host, so a stable GUID and a name
    // must not be enough to make it look invokable.
    const Unaddressable reason = ClassifyAddressability(
        Kind::Command, /*isDynamicParent=*/true, /*hasName=*/true,
        /*hasStableGuid=*/true);

    EXPECT_EQ(reason, Unaddressable::DynamicParent);
    EXPECT_FALSE(IsExecutable(reason));
}

TEST(MenuNodeContractTest, SeparatorIsNeverAddressable) {
    EXPECT_EQ(ClassifyAddressability(Kind::Separator, false, false, false),
              Unaddressable::Separator);
}

TEST(MenuNodeContractTest, MissingStableGuidIsReported) {
    // The HMENU fallback tier without a resolved GUID: listed but not runnable.
    EXPECT_EQ(ClassifyAddressability(Kind::Command, false, /*hasName=*/true,
                                     /*hasStableGuid=*/false),
              Unaddressable::NoStableIdentifier);
}

TEST(MenuNodeContractTest, EmptyNodeIsReported) {
    // The ESLyric idx=9 shape: registered, no name, expands to nothing.
    EXPECT_EQ(ClassifyAddressability(Kind::Command, false, /*hasName=*/false,
                                     /*hasStableGuid=*/true),
              Unaddressable::EmptyNode);
}

TEST(MenuNodeContractTest, OrdinaryCommandIsExecutable) {
    const Unaddressable reason =
        ClassifyAddressability(Kind::Command, false, true, true);

    EXPECT_EQ(reason, Unaddressable::None);
    EXPECT_TRUE(IsExecutable(reason));
}

TEST(MenuNodeContractTest, EveryUnaddressableReasonHasAWireToken) {
    // A missing token would serialize as an empty string and look like "fine".
    const Unaddressable reasons[] = {
        Unaddressable::Separator, Unaddressable::DynamicParent,
        Unaddressable::NoStableIdentifier, Unaddressable::EmptyNode};

    for (const Unaddressable r : reasons) {
        EXPECT_STRNE(ToString(r), "") << "unaddressable reason needs a token";
    }
    EXPECT_STREQ(ToString(Unaddressable::None), "");
}

// ===========================================================================
// Address invariants
// ===========================================================================

TEST(MenuNodeContractTest, AddressValidityRequiresOwnerGuid) {
    Address a;
    EXPECT_FALSE(a.valid()) << "an empty address must never look usable";

    a.guid = "{77CFBCD0-98DC-4015-B327-D7142C664806}";
    EXPECT_TRUE(a.valid());
    EXPECT_FALSE(a.hasSubGuid());

    a.subGuid = "{A222D5A9-2903-AA8C-EEAE-4B9230558B55}";
    EXPECT_TRUE(a.hasSubGuid());
}

// ===========================================================================
// SPEC §9.2 case 6 - path matching is EXACT, never a substring test
//
// This is the D9 regression lock. The pre-refactor matcher accepted a substring
// hit in either direction and took the first winner, so "Rating/1" could resolve
// to "Rating/10" and execute the wrong command while reporting success.
// ===========================================================================

TEST(MenuNodeContractTest, RatingOneMustNotMatchRatingTen) {
    EXPECT_FALSE(SegmentsEqual("1", "10"));
    EXPECT_FALSE(SegmentsEqual("10", "1"));
    EXPECT_TRUE(SegmentsEqual("1", "1"));
}

TEST(MenuNodeContractTest, SubstringInEitherDirectionIsNotAMatch) {
    // Both directions of the old fuzzy test must now fail.
    EXPECT_FALSE(SegmentsEqual("Sort", "Sort by album"));
    EXPECT_FALSE(SegmentsEqual("Sort by album", "Sort"));
    // A single character used to match nearly anything.
    EXPECT_FALSE(SegmentsEqual("S", "Sort"));
}

TEST(MenuNodeContractTest, NormalizationIgnoresMnemonicsEllipsisAndAccelerator) {
    // The same command as rendered in different tiers must compare equal.
    EXPECT_TRUE(SegmentsEqual("&Open...", "open"));
    EXPECT_TRUE(SegmentsEqual("Open...\tCtrl+O", "Open"));
    EXPECT_TRUE(SegmentsEqual("  Preferences  ", "preferences"));
}

TEST(MenuNodeContractTest, AsciiFoldingDoesNotTouchNonAsciiBytes) {
    // CJK has no case to fold; the bytes must survive verbatim so a
    // multi-byte sequence is never corrupted mid-character.
    const std::string zh = "\xE6\xA1\x8C\xE9\x9D\xA2\xE6\xAD\x8C\xE8\xAF\x8D";  // 桌面歌词
    EXPECT_EQ(NormalizeLabel(zh), zh);
    EXPECT_TRUE(SegmentsEqual(zh, zh));

    // Distinct CJK labels must stay distinct.
    const std::string zhOther = "\xE6\x82\xAC\xE6\xB5\xAE\xE6\xAD\x8C\xE8\xAF\x8D";  // 悬浮歌词
    EXPECT_FALSE(SegmentsEqual(zh, zhOther));
}

TEST(MenuNodeContractTest, HighBytesAreNeverPassedToCtypeFunctions) {
    // Guards the UB the old NormalizeLabel had: ::tolower(negative char).
    // A byte-wise fold would alter these; ours must not.
    std::string raw;
    for (int b = 0x80; b <= 0xFF; ++b) raw.push_back(static_cast<char>(b));

    const std::string folded = NormalizeLabel(raw);
    EXPECT_EQ(folded, raw) << "no byte >= 0x80 may be altered";
}

TEST(MenuNodeContractTest, PathSplittingIsSeparatorAgnosticAndDropsEmpties) {
    const auto a = SplitPath("View/ESLyric/Reset position");
    ASSERT_EQ(a.size(), 3u);
    EXPECT_EQ(a[0], "View");
    EXPECT_EQ(a[2], "Reset position");

    // Backslash is accepted, and repeated separators collapse.
    EXPECT_EQ(SplitPath("a\\b").size(), 2u);
    EXPECT_EQ(SplitPath("a//b").size(), 2u);
    EXPECT_EQ(SplitPath("/a/").size(), 1u);
    EXPECT_TRUE(SplitPath("").empty());
}

TEST(MenuNodeContractTest, JoinPathRoundTripsThroughSplit) {
    const std::string path = "View/ESLyric/Desktop lyrics/Reset position";
    EXPECT_EQ(JoinPath(SplitPath(path)), path);
}

// ===========================================================================
// SPEC §9.2 case 7 - an ambiguous path yields ALL candidates, not the first
//
// The live host has 15 duplicated labels (e.g. "重置位置" x3), so first-match-
// wins is not a theoretical concern.
// ===========================================================================

TEST(MenuNodeContractTest, AmbiguityIsClassifiedRatherThanSilentlyResolved) {
    EXPECT_EQ(ClassifyMatch(0), MatchKind::NotFound);
    EXPECT_EQ(ClassifyMatch(1), MatchKind::Unique);
    EXPECT_EQ(ClassifyMatch(2), MatchKind::Ambiguous);
    EXPECT_EQ(ClassifyMatch(3), MatchKind::Ambiguous)
        << "the observed 3x duplicate label must report ambiguous";
}

TEST(MenuNodeContractTest, EveryMatchKindHasAWireToken) {
    EXPECT_STREQ(ToString(MatchKind::NotFound), "notFound");
    EXPECT_STREQ(ToString(MatchKind::Unique), "unique");
    EXPECT_STREQ(ToString(MatchKind::Ambiguous), "ambiguous");
}

// ===========================================================================
// SPEC §9.2 cases 8 and 9 - truncation is reported; one shared depth cap
// ===========================================================================

TEST(MenuNodeContractTest, DepthCapIsSingleSourceOfTruth) {
    // Pre-refactor the same tree was walked to depth 16, 10, or unbounded
    // depending on the endpoint.
    EXPECT_FALSE(DepthExceeded(kMaxMenuTreeDepth));
    EXPECT_TRUE(DepthExceeded(kMaxMenuTreeDepth + 1));
    EXPECT_FALSE(DepthExceeded(0));
}

TEST(MenuNodeContractTest, ChildrenCapIsReportedNotSilent) {
    EXPECT_FALSE(ChildrenExceeded(static_cast<std::size_t>(kMaxChildrenPerNode)));
    EXPECT_TRUE(ChildrenExceeded(static_cast<std::size_t>(kMaxChildrenPerNode) + 1));
}

TEST(MenuNodeContractTest, TruncationAggregatesBothCauses) {
    Truncation t;
    EXPECT_FALSE(t.any()) << "a complete walk must not claim truncation";

    t.depthExceeded = true;
    EXPECT_TRUE(t.any());

    Truncation c;
    c.childrenExceeded = true;
    EXPECT_TRUE(c.any());
}

// ===========================================================================
// Enum wire tokens - response encoding must not drift from the enums
// ===========================================================================

TEST(MenuNodeContractTest, KindAndSourceTokensAreStable) {
    EXPECT_STREQ(ToString(Kind::Command), "command");
    EXPECT_STREQ(ToString(Kind::Submenu), "submenu");
    EXPECT_STREQ(ToString(Kind::Separator), "separator");

    EXPECT_STREQ(ToString(Source::MainMenuStatic), "mainmenu_static");
    EXPECT_STREQ(ToString(Source::MainMenuDynamic), "mainmenu_dynamic");
    EXPECT_STREQ(ToString(Source::ContextMenuStatic), "contextmenu_static");
    EXPECT_STREQ(ToString(Source::ContextMenuDynamic), "contextmenu_dynamic");
    EXPECT_STREQ(ToString(Source::HmenuFallback), "hmenu_fallback");
}

// ===========================================================================
// Live-host regression anchors (SPEC §2.3)
//
// These encode states actually observed on foobar2000 v2.25.3 zh-CN so the
// normalizers stay correct for the shapes that triggered this work.
// ===========================================================================

TEST(MenuNodeContractTest, ObservedEsLyricDynamicChildStates) {
    // "桌面歌词/显示" reported flags=0 -> plain enabled, unchecked leaf.
    const State show = NormalizeMainMenu(0, true);
    EXPECT_TRUE(show.enabled);
    EXPECT_FALSE(show.checked);
    EXPECT_FALSE(show.hidden);

    // "桌面歌词/窗口置顶" reported flags=3 == disabled|checked. The real menu
    // showed it greyed AND ticked; the pre-refactor API reported neither.
    const State alwaysOnTop = NormalizeMainMenu(3, true);
    EXPECT_FALSE(alwaysOnTop.enabled);
    EXPECT_TRUE(alwaysOnTop.checked);

    // "悬浮歌词/锁定" reported flags=1 == disabled only.
    const State locked = NormalizeMainMenu(1, true);
    EXPECT_FALSE(locked.enabled);
    EXPECT_FALSE(locked.checked);
}

TEST(MenuNodeContractTest, ObservedOutputDeviceRadioState) {
    // Output device rows reported flags=8 (defaulthidden) and the active one
    // flags=12 (defaulthidden|radiochecked).
    const State inactive = NormalizeMainMenu(8, true);
    EXPECT_TRUE(inactive.hidden);
    EXPECT_FALSE(inactive.checked);

    const State active = NormalizeMainMenu(12, true);
    EXPECT_TRUE(active.hidden);
    EXPECT_TRUE(active.radioChecked);
    EXPECT_TRUE(active.checked) << "radiochecked must imply checked";
}
