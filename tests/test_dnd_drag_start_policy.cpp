// test_dnd_drag_start_policy.cpp - outcome of a drag that carries a drag token.
//
// A refusal must not take the page's own drag with it, and must never let the
// token text leave the window.
#include "pch.h"
#include "compat/fb2k_types.h"  // console:: stub before ErrorEnvelope.h
#include "../src/api/ErrorEnvelope.h"
#include "../src/webview/dnd/DragStartPolicy.h"

#include <string>

using fb2k_dnd::DecideDragStart;
using fb2k_dnd::DragOutStage;

namespace {

std::string CodeOf(const fb2k_dnd::DragStartVerdict& verdict) {
    return verdict.failureCode ? verdict.failureCode : "";
}

}  // namespace

TEST(DragStartPolicy, CodesAreTheSharedErrorCodes) {
    EXPECT_EQ(CodeOf(DecideDragStart(DragOutStage::TokenRejected, true)),
              ApiErrorCode::PERMISSION_DENIED);
    EXPECT_EQ(CodeOf(DecideDragStart(DragOutStage::EffectsNotCopy, true)),
              ApiErrorCode::INVALID_PARAMS);
    EXPECT_EQ(CodeOf(DecideDragStart(DragOutStage::FilesNotAttached, true)),
              ApiErrorCode::OPERATION_FAILED);
}

TEST(DragStartPolicy, AttachedFilesGoOutWithoutAnEvent) {
    const auto verdict = DecideDragStart(DragOutStage::FilesAttached, true);
    EXPECT_TRUE(verdict.handBack);
    EXPECT_EQ(verdict.failureCode, nullptr);
}

TEST(DragStartPolicy, AttachedFilesGoOutEvenWhenTheSpentTokenStays) {
    // The token was consumed, so text left behind by a failed blanking cannot
    // be redeemed; cancelling would cost the user the files for nothing.
    const auto verdict = DecideDragStart(DragOutStage::FilesAttached, false);
    EXPECT_TRUE(verdict.handBack);
    EXPECT_EQ(verdict.failureCode, nullptr);
}

TEST(DragStartPolicy, RejectedTokenFallsBackToAPageDrag) {
    const auto verdict = DecideDragStart(DragOutStage::TokenRejected, true);
    EXPECT_TRUE(verdict.handBack);
    EXPECT_EQ(CodeOf(verdict), "PERMISSION_DENIED");
}

TEST(DragStartPolicy, WrongEffectsFallBackToAPageDrag) {
    // No CF_HDROP is attached on this path, so a move offered by the page has
    // no user file to relocate.
    const auto verdict = DecideDragStart(DragOutStage::EffectsNotCopy, true);
    EXPECT_TRUE(verdict.handBack);
    EXPECT_EQ(CodeOf(verdict), "INVALID_PARAMS");
}

TEST(DragStartPolicy, UnattachableFilesFallBackToAPageDrag) {
    const auto verdict = DecideDragStart(DragOutStage::FilesNotAttached, true);
    EXPECT_TRUE(verdict.handBack);
    EXPECT_EQ(CodeOf(verdict), "OPERATION_FAILED");
}

TEST(DragStartPolicy, RefusalWithTokenTextLeftIsCancelled) {
    for (const auto stage : {DragOutStage::TokenRejected, DragOutStage::EffectsNotCopy,
                             DragOutStage::FilesNotAttached}) {
        const auto verdict = DecideDragStart(stage, false);
        EXPECT_FALSE(verdict.handBack);
        EXPECT_NE(verdict.failureCode, nullptr);
    }
}

TEST(DragStartPolicy, EveryRefusalCarriesAReason) {
    for (const auto stage : {DragOutStage::TokenRejected, DragOutStage::EffectsNotCopy,
                             DragOutStage::FilesNotAttached}) {
        const auto verdict = DecideDragStart(stage, true);
        ASSERT_NE(verdict.error, nullptr);
        EXPECT_GT(std::string(verdict.error).size(), 0u);
    }
}
