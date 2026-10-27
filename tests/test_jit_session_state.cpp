#include "pch.h"
#include "compat/fb2k_types.h"
#include "core/QueueSessionState.h"

namespace session = jit_session_runtime;
namespace wire = api::jitSession;

namespace {
session::Session Running() {
    session::Session state;
    state.id = "s";
    state.token = "private-token";
    state.current = session::Entry{"entry", "Current", "file.flac", {}, "attempt", "op", 1};
    state.current->status = "playing";
    state.current->execution = session::ExecutionStage::Playing;
    state.operations.Begin({"op", jit_session::OpKind::Play, "entry", "attempt"});
    return state;
}
}

TEST(JitSessionState, SnapshotIncludesItsOwnEventSequenceAndNeverTheToken) {
    auto state = Running();
    state.PublishState();
    ASSERT_EQ(state.events.size(), 1u);
    const auto& event = std::get<wire::StateChangedPayload>(state.events.front().payload);
    EXPECT_EQ(event.eventSeq, event.state.lastEventSeq);
    EXPECT_EQ(event.state.revision, event.revision);
    EXPECT_EQ(wire::ToJson(event).dump().find("private-token"), std::string::npos);
}

TEST(JitSessionState, CompletingAnOperationTwiceProducesOneTerminal) {
    auto state = Running();
    EXPECT_TRUE(state.FinishOperation("op", jit_session::OpStatus::Superseded, "replaced"));
    EXPECT_FALSE(state.FinishOperation("op", jit_session::OpStatus::Completed));
    ASSERT_EQ(state.events.size(), 1u);
    const auto& event = std::get<wire::OperationCompletedPayload>(state.events.front().payload);
    EXPECT_EQ(event.operation.status, "superseded");
    EXPECT_EQ(event.operation.attemptId, "attempt");
}

TEST(JitSessionState, AttemptFactsRemainAssociatedWithTheEndedAttempt) {
    auto state = Running();
    state.position = 9.5;
    state.duration = 10;
    state.EndCurrent("completed", "eof");
    state.EndCurrent("stopped", "user");
    ASSERT_EQ(state.events.size(), 1u);
    const auto& event = std::get<wire::AttemptFinishedPayload>(state.events.front().payload);
    EXPECT_EQ(event.attemptId, "attempt");
    EXPECT_EQ(event.outcome, "completed");
    EXPECT_EQ(event.stopReason, "eof");
    EXPECT_EQ(event.position, 9.5);
    EXPECT_EQ(state.current->status, "ended");
}

TEST(JitSessionState, NeedNextIsReadableAndDoesNotRepeatUntilAnswered) {
    auto state = Running();
    state.RequestNext();
    state.RequestNext();
    ASSERT_EQ(state.events.size(), 1u);
    const auto& event = std::get<wire::NeedNextPayload>(state.events.front().payload);
    EXPECT_EQ(event.afterAttemptId, "attempt");
    const auto snapshot = state.Snapshot();
    EXPECT_EQ(snapshot.neededAfterAttemptId, event.afterAttemptId);
    EXPECT_EQ(snapshot.neededRequestKey, event.requestKey);
    EXPECT_EQ(snapshot.supplyState, "needed");
}

TEST(JitSessionState, ABufferedEntryOrDeclaredEndSuppressesSupplyRequests) {
    auto state = Running();
    state.next = session::Entry{"next", "Next", "next.flac", {}, {}, "next-op", 2};
    state.RequestNext();
    EXPECT_TRUE(state.events.empty());
    EXPECT_EQ(state.Snapshot().supplyState, "preparing");
    state.nextReady = true;
    EXPECT_EQ(state.Snapshot().supplyState, "ready");
    state.next.reset();
    state.endOfQueue = true;
    state.RequestNext();
    EXPECT_TRUE(state.events.empty());
    EXPECT_EQ(state.Snapshot().supplyState, "exhausted");
}

TEST(JitSessionState, ReleasedRecordsExpireAfterTenMinutesButStoppedRecordsRemain) {
    auto state = Running();
    const auto now = std::chrono::steady_clock::now();
    state.releasedAt = now;
    state.phase = "released";
    EXPECT_FALSE(state.Expired(now + std::chrono::minutes(9)));
    EXPECT_TRUE(state.Expired(now + std::chrono::minutes(10)));
    state.phase = "stopped";
    EXPECT_FALSE(state.Expired(now + std::chrono::hours(1)));
}

TEST(JitSessionState, ANewLogicalTargetDoesNotOwnThePreviousPlayersEof) {
    auto state = Running();
    EXPECT_TRUE(state.CurrentPlaybackSubmitted());
    state.current = session::Entry{"replacement", "New", "delayed.m3u", {}, "new-attempt", "new-op", 2};
    EXPECT_FALSE(state.CurrentPlaybackSubmitted());
    state.PlaybackSubmitted(1);
    EXPECT_FALSE(state.CurrentPlaybackSubmitted());
    state.PlaybackSubmitted(2);
    EXPECT_TRUE(state.CurrentPlaybackSubmitted());
    EXPECT_EQ(state.current->status, "preparing");
}

TEST(JitSessionState, BufferReadinessDoesNotSubmitTheCurrentTargetToPlayback) {
    session::Session state;
    state.current = session::Entry{"current", "Current", "delayed.m3u", {}, "attempt", "op", 1};
    state.next = session::Entry{"next", "Next", "next.flac", {}, {}, "next-op", 2};
    state.nextReady = true;
    EXPECT_FALSE(state.CurrentPlaybackSubmitted());
    state.PlaybackSubmitted(2);
    EXPECT_EQ(state.next->execution, session::ExecutionStage::Submitted);
    EXPECT_FALSE(state.CurrentPlaybackSubmitted());
    state.current = std::move(state.next);
    EXPECT_TRUE(state.CurrentPlaybackSubmitted());
}

TEST(JitSessionState, AReleasedSessionDoesNotAcquirePlaybackSubmission) {
    session::Session state;
    state.current = session::Entry{"current", "Current", "delayed.m3u", {}, "attempt", "op", 1};
    state.phase = "released";
    state.PlaybackSubmitted(1);
    EXPECT_FALSE(state.CurrentPlaybackSubmitted());
}
