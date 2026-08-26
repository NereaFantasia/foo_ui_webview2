// test_async_operation_registry.cpp - cancellation bookkeeping, batch-emit
// pacing, operationId format, and the failure-classification catch order used
// by metadata.probeBatchAsync.
#include "pch.h"
#include "../src/api/AsyncOperationRegistry.h"

#include <atomic>
#include <exception>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using fb2k_api::AsyncOperationRegistry;
using fb2k_api::BatchEmitScheduler;
using fb2k_api::FormatAsyncOperationId;

namespace {

// Stand-in for the SDK's abort_callback_impl. The registry is a template over
// the token type precisely so this file needs no foobar2000 SDK linkage: the
// test project supplies SDK include paths but links none of its libs, so
// pfc::event (which abort_callback_impl holds) is not available here.
//
// Only the two properties the registry depends on are reproduced: abort() is
// externally callable, and the type is non-copyable like the SDK original
// (abort_callback.h:82-83), which is what forces shared ownership.
class FakeAbortToken {
public:
    FakeAbortToken() = default;
    FakeAbortToken(const FakeAbortToken&) = delete;
    FakeAbortToken& operator=(const FakeAbortToken&) = delete;

    void abort() { aborted_.store(true); }
    bool is_aborting() const { return aborted_.load(); }

private:
    std::atomic<bool> aborted_{false};
};

// Counts abort() invocations so a repeated cancel is observable.
class CountingAbortToken {
public:
    void abort() { calls_.fetch_add(1); }
    int AbortCallCount() const { return calls_.load(); }

private:
    std::atomic<int> calls_{0};
};

using Registry = AsyncOperationRegistry<FakeAbortToken>;

}  // namespace

// ===========================================================================
// AsyncOperationRegistry
// ===========================================================================

TEST(AsyncOperationRegistry, FreshRegistryIsEmpty) {
    Registry registry;
    EXPECT_EQ(registry.Size(), 0u);
    EXPECT_FALSE(registry.Contains("probe_1"));
}

TEST(AsyncOperationRegistry, RegisterThenCancelSignalsTheToken) {
    Registry registry;
    auto token = std::make_shared<FakeAbortToken>();

    ASSERT_TRUE(registry.Register("probe_1", token));
    EXPECT_EQ(registry.Size(), 1u);
    EXPECT_TRUE(registry.Contains("probe_1"));
    EXPECT_FALSE(token->is_aborting());

    EXPECT_TRUE(registry.Cancel("probe_1"));
    EXPECT_TRUE(token->is_aborting());
}

TEST(AsyncOperationRegistry, CancelUnknownIdReportsFalse) {
    Registry registry;
    // This is the `cancelled: false` branch of metadata.cancelProbe: the id
    // was never registered.
    EXPECT_FALSE(registry.Cancel("no_such_operation"));
}

TEST(AsyncOperationRegistry, CancelAfterRemovalReportsFalse) {
    Registry registry;
    auto token = std::make_shared<FakeAbortToken>();
    ASSERT_TRUE(registry.Register("probe_1", token));
    ASSERT_TRUE(registry.Remove("probe_1"));

    // A finished operation and a never-existing one are deliberately
    // indistinguishable to the caller.
    EXPECT_FALSE(registry.Cancel("probe_1"));
    EXPECT_FALSE(token->is_aborting());
}

TEST(AsyncOperationRegistry, RepeatedCancelStaysLiveUntilRemoved) {
    AsyncOperationRegistry<CountingAbortToken> registry;
    auto token = std::make_shared<CountingAbortToken>();
    ASSERT_TRUE(registry.Register("probe_1", token));

    // Removal is the worker's job, not Cancel's, so a second cancel of a still
    // in-flight operation is honoured rather than silently dropped.
    EXPECT_TRUE(registry.Cancel("probe_1"));
    EXPECT_TRUE(registry.Cancel("probe_1"));
    EXPECT_EQ(token->AbortCallCount(), 2);
    EXPECT_EQ(registry.Size(), 1u);

    EXPECT_TRUE(registry.Remove("probe_1"));
    EXPECT_FALSE(registry.Cancel("probe_1"));
    EXPECT_EQ(token->AbortCallCount(), 2);
}

TEST(AsyncOperationRegistry, CancelAllSignalsEveryLiveToken) {
    Registry registry;
    auto first = std::make_shared<FakeAbortToken>();
    auto second = std::make_shared<FakeAbortToken>();
    ASSERT_TRUE(registry.Register("probe_1", first));
    ASSERT_TRUE(registry.Register("probe_2", second));

    // Shutdown path: nobody knows the ids, so every in-flight probe has to be
    // reachable in one call.
    EXPECT_EQ(registry.CancelAll(), 2u);
    EXPECT_TRUE(first->is_aborting());
    EXPECT_TRUE(second->is_aborting());

    // The workers still own removal, and their Remove must keep working after
    // a CancelAll - that is the whole reason CancelAll does not erase.
    EXPECT_EQ(registry.Size(), 2u);
    EXPECT_TRUE(registry.Remove("probe_1"));
    EXPECT_TRUE(registry.Remove("probe_2"));
    EXPECT_EQ(registry.Size(), 0u);
}

TEST(AsyncOperationRegistry, CancelAllOnEmptyRegistryReportsZero) {
    Registry registry;
    // Quitting while idle must not be a special case at the call site.
    EXPECT_EQ(registry.CancelAll(), 0u);
    EXPECT_EQ(registry.Size(), 0u);
}

TEST(AsyncOperationRegistry, CancelAllLeavesEntriesAddressableById) {
    AsyncOperationRegistry<CountingAbortToken> registry;
    auto token = std::make_shared<CountingAbortToken>();
    ASSERT_TRUE(registry.Register("probe_1", token));

    EXPECT_EQ(registry.CancelAll(), 1u);
    EXPECT_TRUE(registry.Contains("probe_1"));

    // A page cancelling the same operation right after shutdown started still
    // finds it, instead of getting the "already finished" answer for an
    // operation that is in fact still running.
    EXPECT_TRUE(registry.Cancel("probe_1"));
    EXPECT_EQ(token->AbortCallCount(), 2);

    ASSERT_TRUE(registry.Remove("probe_1"));
    EXPECT_EQ(registry.CancelAll(), 0u);
    EXPECT_EQ(token->AbortCallCount(), 2);
}

TEST(AsyncOperationRegistry, DuplicateIdIsRejectedAndKeepsFirstToken) {
    Registry registry;
    auto first = std::make_shared<FakeAbortToken>();
    auto second = std::make_shared<FakeAbortToken>();

    ASSERT_TRUE(registry.Register("probe_1", first));
    EXPECT_FALSE(registry.Register("probe_1", second));
    EXPECT_EQ(registry.Size(), 1u);

    // The stored token must still be the first one; otherwise a colliding id
    // would hand the caller a cancel handle for someone else's work.
    ASSERT_TRUE(registry.Cancel("probe_1"));
    EXPECT_TRUE(first->is_aborting());
    EXPECT_FALSE(second->is_aborting());
}

TEST(AsyncOperationRegistry, RejectsEmptyIdAndNullToken) {
    Registry registry;
    EXPECT_FALSE(registry.Register("", std::make_shared<FakeAbortToken>()));
    EXPECT_FALSE(registry.Register("probe_1", nullptr));
    EXPECT_EQ(registry.Size(), 0u);
}

TEST(AsyncOperationRegistry, RemoveReportsWhetherAnythingWasRemoved) {
    Registry registry;
    ASSERT_TRUE(registry.Register("probe_1", std::make_shared<FakeAbortToken>()));
    EXPECT_TRUE(registry.Remove("probe_1"));
    EXPECT_FALSE(registry.Remove("probe_1"));
    EXPECT_EQ(registry.Size(), 0u);
}

TEST(AsyncOperationRegistry, TokenOutlivesRemovalWhileWorkerHoldsIt) {
    Registry registry;
    auto token = std::make_shared<FakeAbortToken>();
    std::weak_ptr<FakeAbortToken> observer = token;

    ASSERT_TRUE(registry.Register("probe_1", token));
    ASSERT_TRUE(registry.Remove("probe_1"));

    // Shared ownership is the whole reason for shared_ptr here: the worker's
    // copy must keep the token alive after the registry drops it.
    EXPECT_FALSE(observer.expired());
    token.reset();
    EXPECT_TRUE(observer.expired());
}

TEST(AsyncOperationRegistry, ConcurrentRegisterCancelRemoveStaysConsistent) {
    Registry registry;
    constexpr int kThreads = 8;
    constexpr int kPerThread = 200;

    std::atomic<int> registered{0};
    std::atomic<int> cancelled{0};
    std::atomic<int> tokenNotSignalled{0};

    // Assertions are tallied into atomics and checked on the main thread:
    // GoogleTest's EXPECT_* macros are not documented thread-safe on Windows.
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&registry, &registered, &cancelled,
                              &tokenNotSignalled, t]() {
            for (int i = 0; i < kPerThread; ++i) {
                const std::string id =
                    "probe_" + std::to_string(t) + "_" + std::to_string(i);
                auto token = std::make_shared<FakeAbortToken>();
                if (registry.Register(id, token)) {
                    registered.fetch_add(1);
                    if (registry.Cancel(id)) {
                        cancelled.fetch_add(1);
                        if (!token->is_aborting()) {
                            tokenNotSignalled.fetch_add(1);
                        }
                    }
                    registry.Remove(id);
                }
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }

    EXPECT_EQ(registered.load(), kThreads * kPerThread);
    EXPECT_EQ(cancelled.load(), kThreads * kPerThread);
    // A successful Cancel must always have reached the token it found.
    EXPECT_EQ(tokenNotSignalled.load(), 0);
    // Every operation removed itself, so nothing may be left holding a token.
    EXPECT_EQ(registry.Size(), 0u);
}

TEST(AsyncOperationRegistry, ConcurrentCancelOfOneOperationNeverLosesTheToken) {
    AsyncOperationRegistry<CountingAbortToken> registry;
    auto token = std::make_shared<CountingAbortToken>();
    ASSERT_TRUE(registry.Register("probe_shared", token));

    constexpr int kThreads = 8;
    std::atomic<int> successes{0};
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&registry, &successes]() {
            if (registry.Cancel("probe_shared")) {
                successes.fetch_add(1);
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }

    // Cancel releases the lock before touching the token, so concurrent
    // cancels must all still find a live entry and all reach abort().
    EXPECT_EQ(successes.load(), kThreads);
    EXPECT_EQ(token->AbortCallCount(), kThreads);
    registry.Remove("probe_shared");
}

TEST(AsyncOperationRegistry, CancelAllRacesWorkerRemovalWithoutLosingEntries) {
    Registry registry;
    constexpr int kThreads = 8;
    constexpr int kPerThread = 200;

    // Shutdown is exactly this race: CancelAll walks the map while workers are
    // still registering and removing. Tokens are copied under the lock and
    // signalled outside it, so a token the sweep found must stay alive even if
    // its worker removes it in the same instant. The sweeper runs until the
    // workers are done rather than a fixed number of times, so the overlap
    // does not depend on thread start-up timing.
    std::atomic<int> registerFailures{0};
    std::atomic<bool> workersDone{false};

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&registry, &registerFailures, t]() {
            for (int i = 0; i < kPerThread; ++i) {
                const std::string id =
                    "probe_" + std::to_string(t) + "_" + std::to_string(i);
                auto token = std::make_shared<FakeAbortToken>();
                if (!registry.Register(id, token)) {
                    registerFailures.fetch_add(1);
                    continue;
                }
                registry.Remove(id);
            }
        });
    }

    std::thread sweeper([&registry, &workersDone]() {
        while (!workersDone.load()) {
            registry.CancelAll();
            // Yield so the sweep does not starve the workers out of the lock;
            // without it this loop can hold a core spinning on the mutex.
            std::this_thread::yield();
        }
        registry.CancelAll();
    });

    for (auto& w : workers) {
        w.join();
    }
    workersDone.store(true);
    sweeper.join();

    EXPECT_EQ(registerFailures.load(), 0);
    // CancelAll erases nothing, so what is left is decided purely by the
    // workers' own Remove calls.
    EXPECT_EQ(registry.Size(), 0u);
}

// ===========================================================================
// AsyncOperationRegistry: window scope
//
// The window dimension exists because a popup that closes mid-operation used
// to leave its worker running to the end of the queue, still touching the disk
// and still emitting at a window that no longer exists. PopupWindow::OnDestroy
// calls CancelAllForWindow for every registry that has in-flight work.
// ===========================================================================

TEST(AsyncOperationRegistryWindowScope, CancelsOnlyTheClosingWindowsOperations) {
    Registry registry;
    auto mine = std::make_shared<FakeAbortToken>();
    auto theirs = std::make_shared<FakeAbortToken>();
    ASSERT_TRUE(registry.Register("op_1", mine, "popup-a"));
    ASSERT_TRUE(registry.Register("op_2", theirs, "popup-b"));

    EXPECT_EQ(registry.CancelAllForWindow("popup-a"), 1u);
    EXPECT_TRUE(mine->is_aborting());
    // A second popup's work must survive: closing one window is not a global
    // cancel, and that distinction is the whole point of the dimension.
    EXPECT_FALSE(theirs->is_aborting());
}

TEST(AsyncOperationRegistryWindowScope, CancelsEveryOperationOfThatWindow) {
    Registry registry;
    auto first = std::make_shared<FakeAbortToken>();
    auto second = std::make_shared<FakeAbortToken>();
    auto other = std::make_shared<FakeAbortToken>();
    ASSERT_TRUE(registry.Register("op_1", first, "popup-a"));
    ASSERT_TRUE(registry.Register("op_2", second, "popup-a"));
    ASSERT_TRUE(registry.Register("op_3", other, "popup-b"));

    EXPECT_EQ(registry.CancelAllForWindow("popup-a"), 2u);
    EXPECT_TRUE(first->is_aborting());
    EXPECT_TRUE(second->is_aborting());
    EXPECT_FALSE(other->is_aborting());
}

TEST(AsyncOperationRegistryWindowScope, UnattributedOperationsAreNeverSwept) {
    Registry registry;
    auto anonymous = std::make_shared<FakeAbortToken>();
    // No windowId: a caller with no routing context to resolve at all, or an
    // instance registered through WebViewContext's windowId-less overload. Not
    // the main window - that one registers as "main"; what keeps it out of reach
    // is that the only call site of CancelAllForWindow passes a closing popup's
    // id, never "main".
    ASSERT_TRUE(registry.Register("op_1", anonymous));

    EXPECT_EQ(registry.CancelAllForWindow("popup-a"), 0u);
    EXPECT_FALSE(anonymous->is_aborting());
}

TEST(AsyncOperationRegistryWindowScope, EmptyWindowIdMatchesNothing) {
    Registry registry;
    auto anonymous = std::make_shared<FakeAbortToken>();
    auto attributed = std::make_shared<FakeAbortToken>();
    ASSERT_TRUE(registry.Register("op_1", anonymous));
    ASSERT_TRUE(registry.Register("op_2", attributed, "popup-a"));

    // A window whose id resolved to "" must not sweep the unattributed
    // entries, which would make the first closing popup cancel everything.
    EXPECT_EQ(registry.CancelAllForWindow(""), 0u);
    EXPECT_FALSE(anonymous->is_aborting());
    EXPECT_FALSE(attributed->is_aborting());
}

TEST(AsyncOperationRegistryWindowScope, UnknownWindowIdReportsZero) {
    Registry registry;
    auto token = std::make_shared<FakeAbortToken>();
    ASSERT_TRUE(registry.Register("op_1", token, "popup-a"));

    EXPECT_EQ(registry.CancelAllForWindow("popup-gone"), 0u);
    EXPECT_FALSE(token->is_aborting());
}

TEST(AsyncOperationRegistryWindowScope, SweepLeavesRemovalToTheWorker) {
    AsyncOperationRegistry<CountingAbortToken> registry;
    auto token = std::make_shared<CountingAbortToken>();
    ASSERT_TRUE(registry.Register("op_1", token, "popup-a"));

    EXPECT_EQ(registry.CancelAllForWindow("popup-a"), 1u);
    // Same contract as CancelAll: the entry stays until its worker removes it,
    // so a page that cancels by id right after the window closed still finds a
    // live operation instead of the "already finished" answer.
    EXPECT_TRUE(registry.Contains("op_1"));
    EXPECT_TRUE(registry.Cancel("op_1"));
    EXPECT_EQ(token->AbortCallCount(), 2);

    ASSERT_TRUE(registry.Remove("op_1"));
    EXPECT_EQ(registry.CancelAllForWindow("popup-a"), 0u);
}

TEST(AsyncOperationRegistryWindowScope, WindowSweepAndGlobalSweepSeeTheSameEntries) {
    Registry registry;
    auto attributed = std::make_shared<FakeAbortToken>();
    auto anonymous = std::make_shared<FakeAbortToken>();
    ASSERT_TRUE(registry.Register("op_1", attributed, "popup-a"));
    ASSERT_TRUE(registry.Register("op_2", anonymous));

    // Shutdown still has to reach the entries the window sweep cannot: that is
    // the division of labour between initquit::on_quit and OnDestroy.
    EXPECT_EQ(registry.CancelAllForWindow("popup-a"), 1u);
    EXPECT_EQ(registry.CancelAll(), 2u);
    EXPECT_TRUE(anonymous->is_aborting());
}

// ===========================================================================
// FormatAsyncOperationId
// ===========================================================================

TEST(FormatAsyncOperationId, PinsTheWireFormat) {
    EXPECT_EQ(FormatAsyncOperationId("probe", 1, 0xdeadbeefULL),
              "probe_deadbeef00000001");
}

TEST(FormatAsyncOperationId, FileOperationIdsCarryTheirOwnPrefix) {
    // file.copyAsync / moveAsync / deleteAsync all mint ids with the "fileop"
    // prefix, so an id from this family is distinguishable from a probe id in
    // a log without being guessable from one.
    EXPECT_EQ(FormatAsyncOperationId("fileop", 1, 0xdeadbeefULL),
              "fileop_deadbeef00000001");
    EXPECT_NE(FormatAsyncOperationId("fileop", 1, 7),
              FormatAsyncOperationId("probe", 1, 7));
}

TEST(FormatAsyncOperationId, TruncatesWideValuesToThirtyTwoBits) {
    // Both halves are masked to 32 bits so the id can never overflow the
    // buffer regardless of what the sequence counter or RNG hands in.
    EXPECT_EQ(FormatAsyncOperationId("probe", 0xFFFFFFFFFFULL, 0xFFFFFFFFFFULL),
              "probe_ffffffffffffffff");
}

TEST(FormatAsyncOperationId, DistinctSequencesProduceDistinctIds) {
    const std::string a = FormatAsyncOperationId("probe", 1, 7);
    const std::string b = FormatAsyncOperationId("probe", 2, 7);
    EXPECT_NE(a, b);
}

// ===========================================================================
// BatchEmitScheduler
// ===========================================================================

TEST(BatchEmitScheduler, DefaultsMatchTheEventContract) {
    BatchEmitScheduler scheduler;
    EXPECT_EQ(scheduler.FlushCount(), 64u);
    EXPECT_EQ(scheduler.FlushIntervalMs(), 100);
}

TEST(BatchEmitScheduler, NeverFlushesAnEmptyBatch) {
    BatchEmitScheduler scheduler;
    scheduler.Start(0);
    // The interval alone must not produce an event with no results in it.
    EXPECT_FALSE(scheduler.ShouldFlush(0, 0));
    EXPECT_FALSE(scheduler.ShouldFlush(0, 100000));
}

TEST(BatchEmitScheduler, FlushesOnCountBeforeTheIntervalElapses) {
    BatchEmitScheduler scheduler;
    scheduler.Start(0);
    EXPECT_FALSE(scheduler.ShouldFlush(63, 1));
    EXPECT_TRUE(scheduler.ShouldFlush(64, 1));
    EXPECT_TRUE(scheduler.ShouldFlush(65, 1));
}

TEST(BatchEmitScheduler, FlushesOnIntervalBeforeTheCountFills) {
    BatchEmitScheduler scheduler;
    scheduler.Start(1000);
    EXPECT_FALSE(scheduler.ShouldFlush(1, 1099));
    EXPECT_TRUE(scheduler.ShouldFlush(1, 1100));
}

TEST(BatchEmitScheduler, MarkFlushedRestartsTheInterval) {
    BatchEmitScheduler scheduler;
    scheduler.Start(0);
    ASSERT_TRUE(scheduler.ShouldFlush(1, 100));
    scheduler.MarkFlushed(100);
    EXPECT_FALSE(scheduler.ShouldFlush(1, 199));
    EXPECT_TRUE(scheduler.ShouldFlush(1, 200));
}

TEST(BatchEmitScheduler, ZeroAndNegativeConfigFallBackToDefaults) {
    BatchEmitScheduler zeroCount(0, 50);
    EXPECT_EQ(zeroCount.FlushCount(), 64u);
    EXPECT_EQ(zeroCount.FlushIntervalMs(), 50);

    BatchEmitScheduler negativeInterval(8, -1);
    EXPECT_EQ(negativeInterval.FlushCount(), 8u);
    EXPECT_EQ(negativeInterval.FlushIntervalMs(), 100);
}

// Drives the scheduler the way the probe worker does, to pin the resulting
// event count rather than just the per-call predicate.
namespace {

struct BatchRun {
    int events = 0;
    int emitted = 0;
};

// itemCostMs = 0 models an all-cached batch (the flooding case the batching
// rule exists for); a positive cost models per-item disk reads.
BatchRun SimulateProbeRun(size_t itemCount, int64_t itemCostMs,
                          size_t flushCount = BatchEmitScheduler::kDefaultFlushCount,
                          int64_t intervalMs = BatchEmitScheduler::kDefaultFlushIntervalMs) {
    BatchEmitScheduler scheduler(flushCount, intervalMs);
    int64_t now = 0;
    scheduler.Start(now);

    BatchRun run;
    size_t pending = 0;
    for (size_t i = 0; i < itemCount; ++i) {
        now += itemCostMs;
        ++pending;
        if (scheduler.ShouldFlush(pending, now)) {
            ++run.events;
            run.emitted += static_cast<int>(pending);
            pending = 0;
            scheduler.MarkFlushed(now);
        }
    }
    if (pending > 0) {
        // The residue after the loop must still be emitted; a scheduler that
        // never says "flush" is not the same as "nothing left to send".
        ++run.events;
        run.emitted += static_cast<int>(pending);
    }
    return run;
}

}  // namespace

TEST(BatchEmitScheduler, CachedBatchCollapsesToTheCountBound) {
    // 10000 instantly-available items: the count rule dominates and the event
    // total lands on ceil(10000 / 64).
    const BatchRun run = SimulateProbeRun(10000, 0);
    EXPECT_EQ(run.emitted, 10000);
    EXPECT_EQ(run.events, 157);
}

TEST(BatchEmitScheduler, LastPartialBatchIsNeverDropped) {
    // 100 items at 64 per event: one full batch plus a 36-item residue.
    const BatchRun run = SimulateProbeRun(100, 0);
    EXPECT_EQ(run.emitted, 100);
    EXPECT_EQ(run.events, 2);
}

TEST(BatchEmitScheduler, ExactMultipleOfCountEmitsNoEmptyTrailingEvent) {
    const BatchRun run = SimulateProbeRun(128, 0);
    EXPECT_EQ(run.emitted, 128);
    EXPECT_EQ(run.events, 2);
}

TEST(BatchEmitScheduler, SlowItemsFlushOnTheIntervalInsteadOfStalling) {
    // 20 items at 30ms each: too few to hit 64, so the 100ms rule keeps the
    // page updated instead of leaving it silent for the whole run. Bounding
    // latency this way necessarily costs more events than the count rule
    // alone -- that is the documented "whichever comes first" trade.
    const BatchRun run = SimulateProbeRun(20, 30);
    EXPECT_EQ(run.emitted, 20);
    EXPECT_GT(run.events, 1);
    EXPECT_LE(run.events, 20);
}

TEST(BatchEmitScheduler, SingleItemRunEmitsExactlyOneEvent) {
    const BatchRun run = SimulateProbeRun(1, 0);
    EXPECT_EQ(run.emitted, 1);
    EXPECT_EQ(run.events, 1);
}

TEST(BatchEmitScheduler, EmptyRunEmitsNothing) {
    const BatchRun run = SimulateProbeRun(0, 0);
    EXPECT_EQ(run.emitted, 0);
    EXPECT_EQ(run.events, 0);
}

// ===========================================================================
// Failure classification: catch order
//
// The six real exception types cannot be constructed here, and the reason is
// not a missing SDK import library: on MSVC, PFC_DECLARE_EXCEPTION
// (pfc/primitives.h:34-42) expands to all-inline constructors over
// std::exception(const char*, int), which the CRT supplies, so those classes
// are header-only constructible. The obstacle is that the two headers are not
// self-contained - exception_io.h and abort_callback.h both go straight from
// `#pragma once` to PFC_DECLARE_EXCEPTION with zero includes - so reaching them
// means dragging in the whole pfc/SDK header chain, which tests/pch.h and
// tests/compat/fb2k_types.h deliberately stay clear of.
//
// What is reproduced instead is the exact inheritance shape read out of the SDK
// headers, so the ordering property can be exercised:
//
//   pfc::exception            == std::exception          (pfc/primitives.h:201)
//   exception_aborted          : pfc::exception          (abort_callback.h:5)
//   exception_io               : pfc::exception          (exception_io.h:7)
//   exception_io_not_found     : exception_io            (exception_io.h:9)
//   exception_io_data          : exception_io            (exception_io.h:16)
//   exception_io_data_truncation      : exception_io_data (exception_io.h:18)
//   exception_io_unsupported_format   : exception_io_data (exception_io.h:20)
//   exception_io_bad_subsong_index    : exception_io_data (exception_io.h:22)
//
// PFC_DECLARE_EXCEPTION expands to `class NAME : public BASECLASS`
// (pfc/primitives.h:35), so every relationship above is public.
//
// The same relationships are asserted against the REAL SDK types with
// static_assert in src/api/MetadataApi.cpp, next to ProbeOneTrack. That pair
// covers the type hierarchy: this file proves the catch order classifies
// correctly for this hierarchy, and the production translation unit proves the
// hierarchy is the one the SDK actually has.
//
// KNOWN GAP, not covered by anything here: the catch ORDER below is a hand
// transcription of ProbeOneTrack's, and nothing binds the two. The static
// asserts pin the inheritance relations, so a SDK reshuffle is caught at
// compile time; an edit that reorders the production handlers is not. These
// tests would stay green while the shipped code reported a cancellation as
// read-error. Closing it would need the order expressed once and consumed by
// both sides, which is not possible while the test project cannot see the SDK
// types at all. Treat "ClassifyInSpecOrder still matches ProbeOneTrack" as a
// review obligation, not as something CI verifies.
// ===========================================================================

namespace {

// Mirror hierarchy.
struct MirrorAborted : std::exception {};
struct MirrorIo : std::exception {};
struct MirrorIoNotFound : MirrorIo {};
struct MirrorIoData : MirrorIo {};
struct MirrorIoTruncation : MirrorIoData {};
struct MirrorIoUnsupportedFormat : MirrorIoData {};
struct MirrorIoBadSubsong : MirrorIoData {};

static_assert(std::is_base_of_v<std::exception, MirrorAborted>, "");
static_assert(std::is_base_of_v<MirrorIoData, MirrorIoUnsupportedFormat>, "");
static_assert(!std::is_base_of_v<MirrorIoData, MirrorIoNotFound>, "");

// Outcome of one classification, mirroring ProbeItemOutcome's two signals.
struct Classification {
    bool aborted = false;
    std::string failure;

    bool operator==(const Classification& other) const {
        return aborted == other.aborted && failure == other.failure;
    }
};

// The shipped order, transcribed from ProbeOneTrack by hand. No compiler or
// test checks that the transcription is still faithful - see the KNOWN GAP note
// above before trusting a green run here as evidence about production.
Classification ClassifyInSpecOrder(void (*thrower)()) {
    Classification result;
    try {
        thrower();
    } catch (const MirrorAborted&) {
        result.aborted = true;
    } catch (const MirrorIoNotFound&) {
        result.failure = "not-found";
    } catch (const MirrorIoUnsupportedFormat&) {
        result.failure = "unsupported-format";
    } catch (const MirrorIoData&) {
        result.failure = "read-error";
    } catch (const MirrorIo&) {
        result.failure = "read-error";
    } catch (const std::exception&) {
        result.failure = "read-error";
    }
    return result;
}

// The order a reader who did not check the inheritance chain would write:
// broadest first. Kept in the test as the regression this ordering exists to
// prevent, not as an alternative implementation.
//
// C4286 ("caught by base class on line N") is the compiler saying exactly what
// this test asserts, so it is silenced here and nowhere else.
#pragma warning(push)
#pragma warning(disable : 4286)
Classification ClassifyInBrokenOrder(void (*thrower)()) {
    Classification result;
    try {
        thrower();
    } catch (const std::exception&) {
        result.failure = "read-error";
    } catch (const MirrorAborted&) {
        result.aborted = true;
    } catch (const MirrorIoNotFound&) {
        result.failure = "not-found";
    } catch (const MirrorIoUnsupportedFormat&) {
        result.failure = "unsupported-format";
    }
    return result;
}
#pragma warning(pop)

void ThrowAborted() { throw MirrorAborted(); }
void ThrowNotFound() { throw MirrorIoNotFound(); }
void ThrowUnsupportedFormat() { throw MirrorIoUnsupportedFormat(); }
void ThrowTruncation() { throw MirrorIoTruncation(); }
void ThrowBadSubsong() { throw MirrorIoBadSubsong(); }
void ThrowGenericIo() { throw MirrorIo(); }
void ThrowPlainStd() { throw std::exception(); }

}  // namespace

TEST(ProbeFailureClassification, AbortedIsNotAFailure) {
    const Classification c = ClassifyInSpecOrder(&ThrowAborted);
    EXPECT_TRUE(c.aborted);
    // The acceptance criterion for cancellation: no read-error may appear.
    EXPECT_EQ(c.failure, "");
}

TEST(ProbeFailureClassification, NotFoundIsItsOwnCategory) {
    EXPECT_EQ(ClassifyInSpecOrder(&ThrowNotFound),
              (Classification{false, "not-found"}));
}

TEST(ProbeFailureClassification, UnsupportedFormatIsItsOwnCategory) {
    EXPECT_EQ(ClassifyInSpecOrder(&ThrowUnsupportedFormat),
              (Classification{false, "unsupported-format"}));
}

TEST(ProbeFailureClassification, TruncationCollapsesToReadError) {
    EXPECT_EQ(ClassifyInSpecOrder(&ThrowTruncation),
              (Classification{false, "read-error"}));
}

TEST(ProbeFailureClassification, BadSubsongIndexCollapsesToReadError) {
    EXPECT_EQ(ClassifyInSpecOrder(&ThrowBadSubsong),
              (Classification{false, "read-error"}));
}

TEST(ProbeFailureClassification, GenericIoCollapsesToReadError) {
    EXPECT_EQ(ClassifyInSpecOrder(&ThrowGenericIo),
              (Classification{false, "read-error"}));
}

TEST(ProbeFailureClassification, PlainStdExceptionCollapsesToReadError) {
    EXPECT_EQ(ClassifyInSpecOrder(&ThrowPlainStd),
              (Classification{false, "read-error"}));
}

TEST(ProbeFailureClassification, ThreeCategoriesNeverCollide) {
    // "三者互不混淆" stated as an assertion rather than as prose.
    const Classification notFound = ClassifyInSpecOrder(&ThrowNotFound);
    const Classification unsupported = ClassifyInSpecOrder(&ThrowUnsupportedFormat);
    const Classification readError = ClassifyInSpecOrder(&ThrowTruncation);

    EXPECT_NE(notFound.failure, unsupported.failure);
    EXPECT_NE(notFound.failure, readError.failure);
    EXPECT_NE(unsupported.failure, readError.failure);
}

TEST(ProbeFailureClassification, BroadestFirstOrderSwallowsCancellation) {
    // This is the exact bug the ordering guards against: with
    // catch(std::exception) first, cancellation is reported as a read error
    // and the page can never tell "I stopped it" from "the file is broken".
    const Classification c = ClassifyInBrokenOrder(&ThrowAborted);
    EXPECT_FALSE(c.aborted);
    EXPECT_EQ(c.failure, "read-error");
}

TEST(ProbeFailureClassification, BroadestFirstOrderErasesEveryCategory) {
    EXPECT_EQ(ClassifyInBrokenOrder(&ThrowNotFound).failure, "read-error");
    EXPECT_EQ(ClassifyInBrokenOrder(&ThrowUnsupportedFormat).failure, "read-error");
    // Every category collapses into one, which is why the order is not a
    // stylistic choice.
}

// ===========================================================================
// file.*Async result classification
//
// The async file operation family reports one result per item, shaped
// { source, destination?, status, reason? }, where reason is a closed set of
// six values and status is derived from it. Both live in FileApi.cpp inside an
// anonymous namespace, in a translation unit this project cannot link: it
// pulls in the foobar2000 SDK, Win32 shell APIs and BridgeCore. What is
// reproduced below is the mapping rule, so the derivation can be exercised.
//
// KNOWN GAP, identical in kind to the one documented above for the probe catch
// order: the table below is a hand transcription of ApplyReason() and
// TallyResult(), and nothing binds the two. These tests would stay green while
// the shipped code reported a user-initiated cancel as a failure. Treat
// "MirrorApplyReason still matches ApplyReason" as a review obligation, not as
// something CI verifies.
// ===========================================================================

namespace {

// The six reason values of the event contract, transcribed from FileApi.cpp.
const char* const kFileOpReasons[] = {
    "already-exists", "not-found", "permission", "cross-volume", "io-error", "cancelled",
};

enum class MirrorStatus { Ok, Skipped, Failed };

// Mirror of ApplyReason(): reason decides status, nullptr means success.
MirrorStatus MirrorApplyReason(const char* reason) {
    if (reason == nullptr || std::string(reason) == "cross-volume") {
        return MirrorStatus::Ok;
    }
    if (std::string(reason) == "already-exists" || std::string(reason) == "cancelled") {
        return MirrorStatus::Skipped;
    }
    return MirrorStatus::Failed;
}

}  // namespace

TEST(FileOpResultClassification, ReasonSetIsExactlySixDistinctValues) {
    const size_t count = sizeof(kFileOpReasons) / sizeof(kFileOpReasons[0]);
    ASSERT_EQ(count, 6u);
    std::set<std::string> unique;
    for (size_t i = 0; i < count; ++i) {
        unique.insert(kFileOpReasons[i]);
    }
    // A duplicate here would mean two different outcomes are indistinguishable
    // to the page, which is what the closed set exists to prevent.
    EXPECT_EQ(unique.size(), count);
}

TEST(FileOpResultClassification, EveryReasonMapsToADefinedStatus) {
    // No reason may fall through to an accidental default: each of the six has
    // a decided status, and the three status values are all reachable.
    std::set<int> statusesSeen;
    for (const char* reason : kFileOpReasons) {
        statusesSeen.insert(static_cast<int>(MirrorApplyReason(reason)));
    }
    EXPECT_EQ(statusesSeen.size(), 3u);
}

TEST(FileOpResultClassification, SuccessIsTheAbsenceOfAReason) {
    EXPECT_EQ(MirrorApplyReason(nullptr), MirrorStatus::Ok);
}

TEST(FileOpResultClassification, CrossVolumeIsSuccessNotFailure) {
    // A move that fell back to copy-then-delete did what was asked; the reason
    // is there to explain the cost, not to report a problem. Reporting it as a
    // failure would make every cross-volume move look broken.
    EXPECT_EQ(MirrorApplyReason("cross-volume"), MirrorStatus::Ok);
}

TEST(FileOpResultClassification, NotPerformedIsSkippedRatherThanFailed) {
    // "The file was already there" and "you cancelled" are both "not done",
    // not "went wrong". Counting them as failures would show a user-initiated
    // cancel as a screen full of errors.
    EXPECT_EQ(MirrorApplyReason("already-exists"), MirrorStatus::Skipped);
    EXPECT_EQ(MirrorApplyReason("cancelled"), MirrorStatus::Skipped);
}

TEST(FileOpResultClassification, RealErrorsAreFailures) {
    EXPECT_EQ(MirrorApplyReason("not-found"), MirrorStatus::Failed);
    EXPECT_EQ(MirrorApplyReason("permission"), MirrorStatus::Failed);
    EXPECT_EQ(MirrorApplyReason("io-error"), MirrorStatus::Failed);
}

TEST(FileOpResultClassification, TheThreeCountsPartitionTheProcessedItems) {
    // The complete event carries successCount / skippedCount / failureCount and
    // nothing else, so every processed item must land in exactly one of them:
    // a page that adds them up has to get the number of items back.
    size_t ok = 0;
    size_t skipped = 0;
    size_t failed = 0;
    for (const char* reason : kFileOpReasons) {
        switch (MirrorApplyReason(reason)) {
            case MirrorStatus::Ok:      ++ok; break;
            case MirrorStatus::Skipped: ++skipped; break;
            case MirrorStatus::Failed:  ++failed; break;
        }
    }
    EXPECT_EQ(ok + skipped + failed, sizeof(kFileOpReasons) / sizeof(kFileOpReasons[0]));
    EXPECT_EQ(ok, 1u);
    EXPECT_EQ(skipped, 2u);
    EXPECT_EQ(failed, 3u);
}

// ===========================================================================
// file.cancelOp semantics
//
// cancelOp is a thin wrapper over Registry::Cancel: it reports whether the id
// was live at the time of the call and nothing else.
// ===========================================================================

TEST(FileOpCancelSemantics, UnknownAndFinishedOperationsAreIndistinguishable) {
    Registry registry;
    auto token = std::make_shared<FakeAbortToken>();
    ASSERT_TRUE(registry.Register("fileop_1", token, "popup-a"));
    ASSERT_TRUE(registry.Remove("fileop_1"));

    // Both answers are `cancelled: false`, deliberately: a page cannot tell
    // whether it lost the race by a microsecond or asked about an id that never
    // existed, and inventing a third answer would only invite it to try.
    EXPECT_FALSE(registry.Cancel("fileop_1"));
    EXPECT_FALSE(registry.Cancel("fileop_never_existed"));
    EXPECT_FALSE(token->is_aborting());
}

TEST(FileOpCancelSemantics, LiveOperationReportsCancelled) {
    Registry registry;
    auto token = std::make_shared<FakeAbortToken>();
    ASSERT_TRUE(registry.Register("fileop_1", token, "popup-a"));

    EXPECT_TRUE(registry.Cancel("fileop_1"));
    EXPECT_TRUE(token->is_aborting());
    registry.Remove("fileop_1");
}
