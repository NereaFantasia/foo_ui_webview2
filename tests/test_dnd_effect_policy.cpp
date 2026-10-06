// test_dnd_effect_policy.cpp - DROPEFFECT decision rules.
//
// The MOVE/LINK cases are data-loss guards, not style checks: returning MOVE
// tells Explorer it may delete the user's source files.
#include "pch.h"
#include "../src/webview/dnd/DropEffectPolicy.h"

using fb2k_dnd::ChooseDropEffect;
using fb2k_dnd::kRendererAnswerGraceMs;
using fb2k_dnd::RendererHasAnswered;

namespace {

constexpr bool kPending = false;
constexpr bool kAnswered = true;

}  // namespace

// --- Data-loss red lines ---

TEST(DropEffectPolicy, NeverReturnsMoveEvenWhenAllowed) {
    const DWORD all = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_MOVE, all, true, kAnswered) & DROPEFFECT_MOVE, 0u);
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_NONE, all, true, kPending) & DROPEFFECT_MOVE, 0u);
}

TEST(DropEffectPolicy, NeverReturnsLinkEvenWhenAllowed) {
    const DWORD all = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_LINK, all, true, kAnswered) & DROPEFFECT_LINK, 0u);
}

TEST(DropEffectPolicy, SourceForbiddingCopyGetsNone) {
    // Source only permits MOVE. We must not force COPY onto it.
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_MOVE, DROPEFFECT_MOVE, true, kAnswered),
              static_cast<DWORD>(DROPEFFECT_NONE));
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_NONE, DROPEFFECT_MOVE, true, kPending),
              static_cast<DWORD>(DROPEFFECT_NONE));
}

TEST(DropEffectPolicy, EmptyAllowedMaskGetsNone) {
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_COPY, DROPEFFECT_NONE, true, kAnswered),
              static_cast<DWORD>(DROPEFFECT_NONE));
}

// --- Not answered yet, refused, accepted ---

TEST(DropEffectPolicy, FileDragNotYetAnsweredShowsCopy) {
    // Every DragOver before the renderer's first answer reports NONE. Showing
    // that would flash "forbidden" on the way into a window that accepts.
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_NONE, DROPEFFECT_COPY, true, kPending),
              static_cast<DWORD>(DROPEFFECT_COPY));
}

TEST(DropEffectPolicy, FileDragAnsweredNoneShowsForbidden) {
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_NONE, DROPEFFECT_COPY, true, kAnswered),
              static_cast<DWORD>(DROPEFFECT_NONE));
}

TEST(DropEffectPolicy, FileDragAnsweredCopyShowsCopy) {
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_COPY, DROPEFFECT_COPY, true, kAnswered),
              static_cast<DWORD>(DROPEFFECT_COPY));
    const DWORD all = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_COPY, all, true, kPending),
              static_cast<DWORD>(DROPEFFECT_COPY));
}

TEST(DropEffectPolicy, NonFileDragRespectsDownstreamRejection) {
    // No file list, nothing of the user's at stake: the WebView's NONE stands
    // even before it has answered.
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_NONE, DROPEFFECT_COPY, false, kPending),
              static_cast<DWORD>(DROPEFFECT_NONE));
}

TEST(DropEffectPolicy, NonFileDragWithDownstreamCopyIsHonoured) {
    // Text or URL drags still work; we simply have no paths for them.
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_COPY, DROPEFFECT_COPY, false, kAnswered),
              static_cast<DWORD>(DROPEFFECT_COPY));
}

// --- When the WebView's answer counts ---

TEST(RendererAnswer, AnEffectAnswersAtOnce) {
    EXPECT_TRUE(RendererHasAnswered(true, 0));
}

TEST(RendererAnswer, NoneCountsOnlyAfterTheGracePeriod) {
    EXPECT_FALSE(RendererHasAnswered(false, 0));
    EXPECT_FALSE(RendererHasAnswered(false, kRendererAnswerGraceMs - 1));
    EXPECT_TRUE(RendererHasAnswered(false, kRendererAnswerGraceMs));
}

TEST(RendererAnswer, ClockGoingBackwardsIsNotAnAnswer) {
    EXPECT_FALSE(RendererHasAnswered(false, -1));
}

// --- Drag-out gate on the effects the page offered ---
//
// An offered mask containing MOVE lets Explorer move the source out of the
// library. Drag-out must accept exactly COPY, not a broader mask such as
// COPY|MOVE|LINK that an unset effectAllowed can offer.

using fb2k_dnd::DragOutMaskIsCopyOnly;

TEST(DragOutMask, ExactCopyPasses) {
    EXPECT_TRUE(DragOutMaskIsCopyOnly(DROPEFFECT_COPY));
}

TEST(DragOutMask, AnyMaskContainingMoveIsRefused) {
    EXPECT_FALSE(DragOutMaskIsCopyOnly(DROPEFFECT_MOVE));
    EXPECT_FALSE(DragOutMaskIsCopyOnly(DROPEFFECT_COPY | DROPEFFECT_MOVE));
    EXPECT_FALSE(DragOutMaskIsCopyOnly(DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK));
}

TEST(DragOutMask, AnyMaskContainingLinkIsRefused) {
    EXPECT_FALSE(DragOutMaskIsCopyOnly(DROPEFFECT_LINK));
    EXPECT_FALSE(DragOutMaskIsCopyOnly(DROPEFFECT_COPY | DROPEFFECT_LINK));
}

TEST(DragOutMask, EmptyMaskIsRefused) {
    EXPECT_FALSE(DragOutMaskIsCopyOnly(DROPEFFECT_NONE));
}

TEST(DragOutMask, ScrollBitIsRefused) {
    // DROPEFFECT_SCROLL is a hint bit, not an operation; a mask carrying it is
    // still not "exactly copy".
    EXPECT_FALSE(DragOutMaskIsCopyOnly(DROPEFFECT_COPY | DROPEFFECT_SCROLL));
}
