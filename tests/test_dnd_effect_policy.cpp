// test_dnd_effect_policy.cpp - DROPEFFECT decision rules.
//
// The MOVE/LINK cases are data-loss guards, not style checks: returning MOVE
// tells Explorer it may delete the user's source files.
#include "pch.h"
#include "../src/webview/dnd/DropEffectPolicy.h"

using fb2k_dnd::ChooseDropEffect;

// --- Data-loss red lines ---

TEST(DropEffectPolicy, NeverReturnsMoveEvenWhenAllowed) {
    const DWORD all = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_MOVE, all, true) & DROPEFFECT_MOVE, 0u);
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_NONE, all, true) & DROPEFFECT_MOVE, 0u);
}

TEST(DropEffectPolicy, NeverReturnsLinkEvenWhenAllowed) {
    const DWORD all = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_LINK, all, true) & DROPEFFECT_LINK, 0u);
}

TEST(DropEffectPolicy, SourceForbiddingCopyGetsNone) {
    // Source only permits MOVE. We must not force COPY onto it.
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_MOVE, DROPEFFECT_MOVE, true),
              static_cast<DWORD>(DROPEFFECT_NONE));
}

TEST(DropEffectPolicy, EmptyAllowedMaskGetsNone) {
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_COPY, DROPEFFECT_NONE, true),
              static_cast<DWORD>(DROPEFFECT_NONE));
}

// --- Optimistic fallback ---

TEST(DropEffectPolicy, OptimisticCopyWhenDownstreamNotYetConverged) {
    // Measured: 179 of 180 DragOver calls reported NONE while the page did
    // accept the drag. Without this fallback the cursor shows "forbidden"
    // for nearly the whole drag.
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_NONE, DROPEFFECT_COPY, true),
              static_cast<DWORD>(DROPEFFECT_COPY));
}

TEST(DropEffectPolicy, NonFileDragRespectsDownstreamRejection) {
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_NONE, DROPEFFECT_COPY, false),
              static_cast<DWORD>(DROPEFFECT_NONE));
}

TEST(DropEffectPolicy, DownstreamCopyPassesThrough) {
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_COPY, DROPEFFECT_COPY, true),
              static_cast<DWORD>(DROPEFFECT_COPY));
}

TEST(DropEffectPolicy, NonFileDragWithDownstreamCopyIsHonoured) {
    // Text or URL drags still work; we simply have no paths for them.
    EXPECT_EQ(ChooseDropEffect(DROPEFFECT_COPY, DROPEFFECT_COPY, false),
              static_cast<DWORD>(DROPEFFECT_COPY));
}
