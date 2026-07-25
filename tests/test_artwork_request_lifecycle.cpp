#include "pch.h"

#include "../src/webview/ArtworkRequestLifecycle.h"

#include <atomic>
#include <thread>
#include <vector>

namespace {

using artwork_request::ArtworkRequestLifecycle;
using artwork_request::CompletionResult;
using artwork_request::TerminalReason;
using artwork_request::TokenKind;

constexpr TokenKind kTokenKinds[] = {
    TokenKind::WebResourceRequested,
    TokenKind::NavigationStarting,
    TokenKind::NavigationCompleted,
};

void RegisterAllTokens(ArtworkRequestLifecycle& lifecycle) {
    for (const auto kind : kTokenKinds) {
        ASSERT_TRUE(lifecycle.RecordTokenAdd(kind, true));
    }
}

void ExpectTokenCounts(
    const ArtworkRequestLifecycle& lifecycle,
    std::uint64_t addCount,
    std::uint64_t removeCount,
    bool registered) {
    for (const auto kind : kTokenKinds) {
        const auto state = lifecycle.GetTokenState(kind);
        EXPECT_EQ(state.addCount, addCount);
        EXPECT_EQ(state.removeCount, removeCount);
        EXPECT_EQ(state.registered, registered);
    }
}

}  // namespace

TEST(ArtworkRequestLifecycle, NavigationStartingAdvancesAndCompletedDoesNot) {
    ArtworkRequestLifecycle lifecycle;
    EXPECT_EQ(lifecycle.OnControllerCreated(), 1u);

    EXPECT_EQ(lifecycle.OnNavigationStarting(), 1u);
    EXPECT_EQ(lifecycle.OnNavigationCompleted(), 1u);
    EXPECT_EQ(lifecycle.OnNavigationCompleted(), 1u);
    EXPECT_EQ(lifecycle.OnNavigationStarting(), 2u);
    EXPECT_EQ(lifecycle.NavigationGeneration(), 2u);
}

TEST(ArtworkRequestLifecycle, ControllerTransitionsAdvanceHostGeneration) {
    ArtworkRequestLifecycle lifecycle;

    EXPECT_EQ(lifecycle.OnControllerCreated(), 1u);
    EXPECT_EQ(lifecycle.OnControllerRecreated(), 2u);
    EXPECT_EQ(lifecycle.OnControllerRecovery(), 3u);
    EXPECT_EQ(lifecycle.OnClose(), 4u);
    EXPECT_FALSE(lifecycle.IsAlive());
    EXPECT_TRUE(lifecycle.IsClosing());
}

TEST(ArtworkRequestLifecycle, RequestCapturesBothGenerations) {
    ArtworkRequestLifecycle lifecycle;
    lifecycle.OnControllerCreated();
    lifecycle.OnNavigationStarting();
    lifecycle.OnNavigationStarting();

    const auto request = lifecycle.CaptureRequest();
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->navigationGeneration, 2u);
    EXPECT_EQ(request->hostGeneration, 1u);
    EXPECT_TRUE(lifecycle.CanCallbackAccessHost(*request));
    EXPECT_EQ(lifecycle.Complete(*request), CompletionResult::Accepted);
    EXPECT_EQ(lifecycle.GetTerminalReason(request->requestId), TerminalReason::Completed);
}

TEST(ArtworkRequestLifecycle, NavigationMismatchRejectsCompletion) {
    ArtworkRequestLifecycle lifecycle;
    lifecycle.OnControllerCreated();
    const auto request = lifecycle.CaptureRequest();
    ASSERT_TRUE(request.has_value());

    lifecycle.OnNavigationStarting();

    EXPECT_FALSE(lifecycle.CanCallbackAccessHost(*request));
    EXPECT_EQ(lifecycle.Complete(*request), CompletionResult::Stale);
    EXPECT_EQ(
        lifecycle.GetTerminalReason(request->requestId),
        TerminalReason::NavigationSuperseded);
}

TEST(ArtworkRequestLifecycle, HostMismatchRejectsCompletion) {
    ArtworkRequestLifecycle lifecycle;
    lifecycle.OnControllerCreated();
    const auto request = lifecycle.CaptureRequest();
    ASSERT_TRUE(request.has_value());

    lifecycle.OnControllerRecreated();

    EXPECT_FALSE(lifecycle.CanCallbackAccessHost(*request));
    EXPECT_EQ(lifecycle.Complete(*request), CompletionResult::Stale);
    EXPECT_EQ(
        lifecycle.GetTerminalReason(request->requestId),
        TerminalReason::HostSuperseded);
}

TEST(ArtworkRequestLifecycle, SuccessfulTokenAddsAreRemovedExactlyOnce) {
    ArtworkRequestLifecycle lifecycle;
    lifecycle.OnControllerCreated();
    RegisterAllTokens(lifecycle);
    ExpectTokenCounts(lifecycle, 1, 0, true);

    for (const auto kind : kTokenKinds) {
        EXPECT_TRUE(lifecycle.RemoveToken(kind));
        EXPECT_FALSE(lifecycle.RemoveToken(kind));
    }
    ExpectTokenCounts(lifecycle, 1, 1, false);
}

TEST(ArtworkRequestLifecycle, FailedTokenAddsAreNeverRemoved) {
    ArtworkRequestLifecycle lifecycle;
    lifecycle.OnControllerCreated();

    for (const auto kind : kTokenKinds) {
        EXPECT_FALSE(lifecycle.RecordTokenAdd(kind, false));
        EXPECT_FALSE(lifecycle.RemoveToken(kind));
    }
    ExpectTokenCounts(lifecycle, 0, 0, false);
}

TEST(ArtworkRequestLifecycle, RecreateRemovesOldTokensBeforeNewRegistration) {
    ArtworkRequestLifecycle lifecycle;
    lifecycle.OnControllerCreated();
    RegisterAllTokens(lifecycle);

    EXPECT_EQ(lifecycle.OnControllerRecreated(), 2u);
    ExpectTokenCounts(lifecycle, 1, 1, false);

    RegisterAllTokens(lifecycle);
    ExpectTokenCounts(lifecycle, 2, 1, true);
}

TEST(ArtworkRequestLifecycle, RecoveryRemovesOldTokensBeforeNewRegistration) {
    ArtworkRequestLifecycle lifecycle;
    lifecycle.OnControllerCreated();
    RegisterAllTokens(lifecycle);

    EXPECT_EQ(lifecycle.OnControllerRecovery(), 2u);
    ExpectTokenCounts(lifecycle, 1, 1, false);

    RegisterAllTokens(lifecycle);
    ExpectTokenCounts(lifecycle, 2, 1, true);
}

TEST(ArtworkRequestLifecycle, CloseRemovesTokensAndRejectsCallbacksAndMailboxCompletion) {
    ArtworkRequestLifecycle lifecycle;
    lifecycle.OnControllerCreated();
    RegisterAllTokens(lifecycle);
    const auto request = lifecycle.CaptureRequest();
    ASSERT_TRUE(request.has_value());

    EXPECT_EQ(lifecycle.OnClose(), 2u);
    ExpectTokenCounts(lifecycle, 1, 1, false);
    EXPECT_FALSE(lifecycle.CanCallbackAccessHost(*request));
    EXPECT_EQ(lifecycle.Complete(*request), CompletionResult::Stale);
    EXPECT_EQ(lifecycle.GetTerminalReason(request->requestId), TerminalReason::HostClosed);
    EXPECT_FALSE(lifecycle.CaptureRequest().has_value());
    EXPECT_FALSE(lifecycle.RecordTokenAdd(TokenKind::WebResourceRequested, true));
    EXPECT_EQ(lifecycle.OnNavigationStarting(), 0u);
}

TEST(ArtworkRequestLifecycle, DuplicateCompletionRaceHasOneTerminalWinner) {
    ArtworkRequestLifecycle lifecycle;
    lifecycle.OnControllerCreated();
    const auto request = lifecycle.CaptureRequest();
    ASSERT_TRUE(request.has_value());

    constexpr int kThreadCount = 16;
    std::atomic<bool> start = false;
    std::atomic<int> accepted = 0;
    std::atomic<int> alreadyTerminal = 0;
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (int index = 0; index < kThreadCount; ++index) {
        workers.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            const auto result = lifecycle.Complete(*request);
            if (result == CompletionResult::Accepted) {
                ++accepted;
            } else if (result == CompletionResult::AlreadyTerminal) {
                ++alreadyTerminal;
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(accepted.load(), 1);
    EXPECT_EQ(alreadyTerminal.load(), kThreadCount - 1);
    EXPECT_EQ(lifecycle.GetTerminalReason(request->requestId), TerminalReason::Completed);
}

TEST(ArtworkRequestLifecycle, StaleDuplicateCompletionRaceHasOneTerminalWinner) {
    ArtworkRequestLifecycle lifecycle;
    lifecycle.OnControllerCreated();
    const auto request = lifecycle.CaptureRequest();
    ASSERT_TRUE(request.has_value());
    lifecycle.OnNavigationStarting();

    constexpr int kThreadCount = 16;
    std::atomic<bool> start = false;
    std::atomic<int> stale = 0;
    std::atomic<int> alreadyTerminal = 0;
    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (int index = 0; index < kThreadCount; ++index) {
        workers.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            const auto result = lifecycle.Complete(*request);
            if (result == CompletionResult::Stale) {
                ++stale;
            } else if (result == CompletionResult::AlreadyTerminal) {
                ++alreadyTerminal;
            }
        });
    }

    start.store(true, std::memory_order_release);
    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(stale.load(), 1);
    EXPECT_EQ(alreadyTerminal.load(), kThreadCount - 1);
    EXPECT_EQ(
        lifecycle.GetTerminalReason(request->requestId),
        TerminalReason::NavigationSuperseded);
}