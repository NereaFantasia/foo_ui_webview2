// AsyncOperationRegistry.h - cancellation bookkeeping and batch-emit pacing
// for handlers that dispatch work to a worker thread and report back by event.
//
// This is the second such registry in the codebase, not the first. HttpApi.cpp
// already carries the same shape: RegisterCancelToken / UnregisterCancelToken /
// CancelRequest at :211-231 over a `cancelMutex_` plus a
// `std::unordered_map<std::string, std::shared_ptr<std::atomic<bool>>>` at
// :277-278. Anyone about to write a third one should reuse this file instead.
//
// The two are not merged because the token types are not interchangeable, and
// the difference is forced by the SDK rather than chosen:
//
//   - HttpApi owns its wait loop, so a `shared_ptr<std::atomic<bool>>` that the
//     request thread polls is sufficient.
//   - Metadata probing hands the token to the SDK, which takes an
//     `abort_callback&` (`metadb_handle::get_full_info_ref`), so the token must
//     BE an `abort_callback_impl`. An atomic<bool> cannot be passed there, and
//     `abort_callback_impl` is non-copyable, which is why ownership is shared.
//
// Retrofitting HttpApi onto this template would therefore mean either giving it
// an SDK dependency it does not need or adding a second token concept here; the
// registry is parameterised on the token type so a future third caller can pick
// whichever one its callee demands.
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fb2k_api {

// Registry of in-flight async operations, keyed by the operationId handed back
// to the page, holding the abort token the worker polls.
//
// Standard library only: no foobar2000 SDK, Win32 or core_api dependency. The
// token type is a template parameter rather than `abort_callback_impl` so the
// class is constructible in unit tests, where a stub exposing `abort()` stands
// in for the SDK type. Production instantiates it with the SDK type directly.
//
// Ownership: the token is shared, never owned solely by the registry. The SDK's
// `abort_callback_impl` is externally triggerable but non-copyable
// (abort_callback.h:82-83), so registry and worker must hold the same object;
// `shared_ptr` also means Cancel() cannot race the worker into a freed token.
//
// Lifetime: Register on dispatch, Remove when the worker finishes. An entry
// left behind would keep its token alive forever, so the worker must Remove on
// every exit path including the cancelled and thrown ones.
//
// Each entry also records which window dispatched it, so CancelAllForWindow()
// can stop the work a closing popup started. That id is a plain string rather
// than an HWND because that is what both ends already hold: callers resolve it
// through CallerContext, and the teardown path has PopupWindow::GetWindowId().
//
// Cancel() reports whether the id was live *at the time of the call*. An
// unknown id yields false, which is also what a completed-and-removed operation
// yields - the two are deliberately indistinguishable to the caller, since a
// page that cancels a probe cannot tell whether it lost the race by a
// microsecond or by a minute.
template <typename AbortToken>
class AsyncOperationRegistry {
public:
    using TokenPtr = std::shared_ptr<AbortToken>;

    AsyncOperationRegistry() = default;

    AsyncOperationRegistry(const AsyncOperationRegistry&) = delete;
    AsyncOperationRegistry& operator=(const AsyncOperationRegistry&) = delete;

    // Records token under id. Returns false and stores nothing when id is
    // already present or token is null; a false here means the caller's id
    // generator collided and the operation must not be dispatched.
    //
    // windowId attributes the entry to the window that dispatched it, so that a
    // closing popup can stop the work it started. Empty means "not
    // attributable": CancelAllForWindow("") matches nothing, which leaves such
    // an entry reachable only through Cancel(id) and CancelAll().
    //
    // Both callers pass CallerContext::FromParams(params).windowId - the file
    // operation registry and the probe registry resolve it the same way. Two
    // properties of that resolution are easy to misread:
    //
    //   - Its last-resort fallback (CallerContext.cpp:49-55) does not yield an
    //     empty id, it yields the FIRST registered instance's windowId. That
    //     branch is also unreachable for a bridge message, whose _callerHwnd is
    //     by construction the calling instance's own registered hwnd, so the
    //     direct lookup (:23-30) answers first. The fallback is left as it is on
    //     purpose: an operation's events are routed through the same resolution,
    //     so "the window that receives the events is the one whose close cancels
    //     the work" holds whichever branch answered.
    //   - Empty ids do not come out of that resolution either. They arise where
    //     there is no routing context to resolve at all (a direct C++ caller that
    //     passes no _callerHwnd; never a page message, since the one entry point
    //     for those, WebViewPanel.cpp:368, always hands BridgeCore the calling
    //     panel's own hwnd), or on the registration side, where
    //     WebViewPanel::CompleteWebViewInit selects WebViewContext's
    //     windowId-less RegisterInstance overload if windowId_ was never set.
    //     All four current hosts set it before that point (main window, popups,
    //     DUI and CUI panels), so no live path produces an empty id today - the
    //     rule above guards the next host, it is not describing current
    //     behaviour.
    bool Register(const std::string& id, TokenPtr token,
                  const std::string& windowId = std::string()) {
        if (id.empty() || !token) {
            return false;
        }
        std::lock_guard lock(mutex_);
        return operations_.emplace(id, Entry{std::move(token), windowId}).second;
    }

    // Signals the token registered under id. Returns false when id is not
    // currently registered.
    //
    // The token is signalled after the lock is released: abort() belongs to the
    // caller-supplied type, so calling it under our mutex would put foreign
    // code inside our critical section. Holding a reference across the release
    // is what keeps the token alive while the worker may be tearing down.
    bool Cancel(const std::string& id) {
        TokenPtr token;
        {
            std::lock_guard lock(mutex_);
            auto it = operations_.find(id);
            if (it == operations_.end()) {
                return false;
            }
            token = it->second.token;
        }
        token->abort();
        return true;
    }

    // Signals every token registered at the time of the call, and returns how
    // many were signalled. Exists for component shutdown: a worker only notices
    // cancellation between items, so a queue nobody aborts is walked to its end
    // while the process waits on it.
    //
    // Entries are deliberately NOT erased here. Removal is the worker's job per
    // the Lifetime note above, and it still runs on every exit path; erasing
    // from under it would make Size()/Contains() report an idle registry while
    // workers are demonstrably still running, and would race a concurrent
    // Register for the same id into an operation nobody can cancel afterwards.
    // Nothing leaks either way - the tokens are shared, and the workers drop
    // their own copies as they finish.
    //
    // Tokens are collected under the lock and signalled after releasing it, for
    // the same reason as Cancel(): abort() is caller-supplied code and must not
    // run inside our critical section. The copies keep every token alive across
    // the release even if its worker finishes and Removes in between.
    size_t CancelAll() {
        std::vector<TokenPtr> tokens;
        {
            std::lock_guard lock(mutex_);
            tokens.reserve(operations_.size());
            for (const auto& entry : operations_) {
                tokens.push_back(entry.second.token);
            }
        }
        for (const auto& token : tokens) {
            token->abort();
        }
        return tokens.size();
    }

    // Signals every token registered under windowId at the time of the call,
    // and returns how many were signalled. Exists for window teardown: a popup
    // closed mid-operation leaves a worker that keeps touching the disk and
    // reporting to a window that is already gone.
    //
    // An empty windowId matches nothing, including the entries registered
    // without one. Matching those would let the first popup that closes cancel
    // every unattributed operation in the process.
    //
    // Entries are not erased and tokens are signalled outside the lock, for the
    // same reasons as CancelAll() above.
    size_t CancelAllForWindow(const std::string& windowId) {
        if (windowId.empty()) {
            return 0;
        }
        std::vector<TokenPtr> tokens;
        {
            std::lock_guard lock(mutex_);
            for (const auto& entry : operations_) {
                if (entry.second.windowId == windowId) {
                    tokens.push_back(entry.second.token);
                }
            }
        }
        for (const auto& token : tokens) {
            token->abort();
        }
        return tokens.size();
    }

    // Drops the entry for id. Returns whether anything was removed, so a
    // double-Remove is observable in tests but harmless in production.
    bool Remove(const std::string& id) {
        std::lock_guard lock(mutex_);
        return operations_.erase(id) > 0;
    }

    bool Contains(const std::string& id) const {
        std::lock_guard lock(mutex_);
        return operations_.find(id) != operations_.end();
    }

    size_t Size() const {
        std::lock_guard lock(mutex_);
        return operations_.size();
    }

private:
    // One live operation: the token its worker polls, plus the window that
    // dispatched it (empty when the caller could not be attributed to one).
    struct Entry {
        TokenPtr token;
        std::string windowId;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> operations_;
};

// Builds the operationId string. Split out as a free function so the wire
// format is pinned by a test rather than by whatever the call site happens to
// print. `randomBits` is a parameter, not something this function draws, which
// keeps it deterministic; callers supply a value from their own generator so an
// id cannot be guessed by a page that only watched the sequence advance.
std::string FormatAsyncOperationId(const std::string& prefix,
                                   uint64_t sequence,
                                   uint64_t randomBits);

// Decides when a producer loop should emit its accumulated batch.
//
// Emitting one event per produced item floods the bridge - the same reason the
// drag-drop design refuses a per-DragOver event. Emitting only at the end
// leaves the page with no progress signal at all. This paces between the two:
// flush once `flushCount` items have piled up, or once `flushIntervalMs` has
// passed since the last flush, whichever comes first.
//
// The pending items live in the caller, not here; this class is policy only.
// Time is passed in rather than read, so the policy is testable without
// sleeping. Callers must still emit any residue after their loop ends - a
// scheduler that never says "flush" is not the same as "nothing left to send".
class BatchEmitScheduler {
public:
    // 64 items is large enough that a cached-metadata batch collapses into a
    // handful of events, small enough that one event stays a reasonable JSON
    // payload. 100ms keeps the progress bar moving when each item costs a disk
    // read, which is the case this whole code path exists for.
    static constexpr size_t kDefaultFlushCount = 64;
    static constexpr int64_t kDefaultFlushIntervalMs = 100;

    explicit BatchEmitScheduler(size_t flushCount = kDefaultFlushCount,
                                int64_t flushIntervalMs = kDefaultFlushIntervalMs);

    // Marks the start of the run; the interval is measured from here.
    void Start(int64_t nowMs);

    // Whether `pendingCount` items should be emitted now. Always false for an
    // empty batch: the interval must never produce an empty event.
    bool ShouldFlush(size_t pendingCount, int64_t nowMs) const;

    // Records that a flush actually happened, restarting the interval.
    void MarkFlushed(int64_t nowMs);

    size_t FlushCount() const { return flushCount_; }
    int64_t FlushIntervalMs() const { return flushIntervalMs_; }

private:
    size_t flushCount_;
    int64_t flushIntervalMs_;
    int64_t lastFlushMs_ = 0;
};

}  // namespace fb2k_api
