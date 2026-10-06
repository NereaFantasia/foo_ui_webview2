// WaveformTaskQueue.h - bookkeeping for full-track waveform decode jobs
//
/* No foobar2000 SDK or Win32 dependency. Requests for the same decode key are
 * merged into one job, each request keeping its own taskId; at most
 * `maxConcurrent` jobs decode at once and queued jobs start in submission
 * order. The queue only returns decisions - which jobs to start, which to
 * abort, whether to write the cache, which taskIds get which terminal event -
 * and the host carries them out, so the rules run in tests without a decoder.
 *
 * Every entry point is meant to be called from one thread (the host calls it
 * from the main thread only); there is no locking.
 */
#pragma once

#include "api/WaveformAccumulator.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace fb2k_waveform {

// The two identities the host resolves for a caller. The window handle is
// carried as an integer so this unit needs no Win32 types.
struct CallerIdentity {
    std::uintptr_t hwnd = 0;
    std::string windowId;
};

enum class CancelVerdict {
    // One of the registered identities is set and equals the canceller's.
    Owner,
    // Nothing was registered (a caller without routing context): accepted
    // unchecked, and the host logs it.
    Unattributed,
    Denied,
};

CancelVerdict JudgeCancel(const CallerIdentity& registered, const CallerIdentity& canceller);

// One request waiting on a job. `path` and `render` are the request's own, so
// merged requests still get answers in their own shape.
struct Waiter {
    std::string taskId;
    std::string path;
    RenderOptions render;
    CallerIdentity owner;
};

enum class JobState {
    Queued,
    Decoding,
    // Abort has been signalled but the worker has not returned yet. The job
    // keeps its slot until then, because opening a decoder on a slow disk can
    // block for seconds, and it accepts no new waiters.
    Aborting,
};

enum class JobOutcome { Succeeded, Failed, Aborted };

class WaveformTaskQueue {
public:
    explicit WaveformTaskQueue(std::size_t maxConcurrent = 2);

    struct SubmitResult {
        std::uint64_t jobId = 0;
        // true when the waiter joined a job that was already queued or decoding.
        bool merged = false;
        // Jobs to hand to a worker now, in order.
        std::vector<std::uint64_t> start;
    };
    // The taskId must be unique among live waiters; the host generates it.
    SubmitResult Submit(const std::string& decodeKey, Waiter waiter);

    struct CancelResult {
        bool cancelled = false;
        CancelVerdict verdict = CancelVerdict::Denied;
        // The removed waiter, owed exactly one CANCELLED failure event. Only
        // meaningful when `cancelled` is true.
        Waiter waiter;
        // Jobs left without waiters while decoding: signal their abort.
        std::vector<std::uint64_t> abort;
        // Queued jobs left without waiters, removed from the queue; the host
        // drops whatever it keeps per job.
        std::vector<std::uint64_t> discarded;
    };
    // An unknown or finished taskId and a denied one both answer cancelled
    // false; the caller cannot tell them apart.
    CancelResult Cancel(const std::string& taskId, const CallerIdentity& canceller);

    struct FinishResult {
        // A succeeded decode is cached even when every waiter is gone.
        bool writeCache = false;
        // Waiters owed a terminal event: ready for Succeeded, a failure with
        // the job's error for Failed, CANCELLED for Aborted.
        std::vector<Waiter> notify;
        std::vector<std::uint64_t> start;
    };
    // Called once per started job when its worker returns. An unknown jobId
    // yields an empty result.
    FinishResult Finish(std::uint64_t jobId, JobOutcome outcome);

    struct DropResult {
        std::vector<std::uint64_t> abort;
        std::vector<std::uint64_t> discarded;
        // Waiters removed without an event: their page is going away.
        std::size_t removedWaiters = 0;
    };
    // Removes every waiter registered with this non-empty windowId (a closing
    // popup). An empty windowId matches nothing. Removing waiters never frees a
    // slot, so nothing starts here.
    DropResult DropWindow(const std::string& windowId);
    // Removes every waiter, aborts every decoding job and discards the queue
    // (host shutdown). The queue stays closed: a later Submit is recorded but
    // never started, so the host must refuse requests from then on.
    DropResult DropAll();

    std::size_t ActiveCount() const;  // decoding + aborting
    std::size_t QueuedCount() const;
    bool HasJob(std::uint64_t jobId) const;
    JobState StateOf(std::uint64_t jobId) const;  // Queued for an unknown job
    std::size_t WaiterCount(std::uint64_t jobId) const;

private:
    struct Job {
        std::string key;
        JobState state = JobState::Queued;
        std::vector<Waiter> waiters;
    };

    // Removes the waiter at `index` of `jobId`; a job left empty is discarded
    // when queued (reported in `discarded`) or moved to Aborting (reported in
    // `abort`) when decoding.
    void RemoveWaiter(std::uint64_t jobId, std::size_t index, std::vector<std::uint64_t>& abort,
                      std::vector<std::uint64_t>& discarded);
    std::vector<std::uint64_t> Pump();

    std::size_t maxConcurrent_;
    std::uint64_t nextJobId_ = 1;
    bool closed_ = false;
    std::unordered_map<std::uint64_t, Job> jobs_;
    std::deque<std::uint64_t> queued_;
    // Decode key -> the job that still accepts waiters (queued or decoding).
    std::unordered_map<std::string, std::uint64_t> openByKey_;
    std::unordered_map<std::string, std::uint64_t> jobByTask_;
};

}  // namespace fb2k_waveform
