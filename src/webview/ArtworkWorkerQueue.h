#pragma once
// ArtworkWorkerQueue — safe async artwork extraction off the UI thread.
//
// Bounded queue + single-flight + cancel-aware abort.
// Extraction runs on fb2k::inCpuWorkerThread (NOT raw std::thread).
// foobar2000 album-art SDK asserts when called from non-fb2k worker threads
// (crash failure_00000252: uBugCheck inside GetArtworkBinaryForMetadbHandle).
// Completions post via fb2k::inMainThread back to the owner thread.
// Include only after pch.h (depends on foobar2000 SDK types).

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "api/ArtworkApi.h"
#include "utils/ImageUtils.h"

namespace artwork_worker {

// Result produced by the worker; owner thread builds the WebView2 response.
struct ArtworkResult {
    int statusCode = 500;
    std::vector<uint8_t> bytes;  // non-empty only on 200
    std::string mime;
    std::string etag;
    bool is304 = false;
    bool isNegative = false;  // 404 from extractor → write L1N
    std::string negCacheKey;
    std::string cacheKey;  // for L1 write-back on 200
};

using CompletionFn = std::function<void(ArtworkResult)>;

struct WorkItem {
    std::string pathUtf8;
    std::string artworkType;
    int maxSize = 0;
    std::string cacheKey;  // empty if maxSize==0 → no single-flight
    std::string negCacheKey;
    std::string etag;
    std::string ifNoneMatchEtag;

    uint64_t navGen = 0;
    uint64_t hostGen = 0;

    std::shared_ptr<abort_callback_impl> abort;
    CompletionFn complete;
};

class ArtworkWorkerQueue {
public:
    static constexpr size_t kMaxQueueDepth = 32;

    ArtworkWorkerQueue() = default;

    ~ArtworkWorkerQueue() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        // Complete every outstanding deferral, then wait for in-flight CPU work.
        DrainAll();
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return !busy_; });
        }
    }

    // Returns false if queue is full (caller should respond 503).
    // Call from owner thread only.
    bool Submit(WorkItem item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return false;
        }

        // single-flight: same cacheKey already in-flight / queued → add follower
        if (!item.cacheKey.empty()) {
            auto fit = singleFlight_.find(item.cacheKey);
            if (fit != singleFlight_.end()) {
                fit->second.push_back(std::move(item.complete));
                return true;  // follower does not count against depth
            }
        }

        if (queue_.size() >= kMaxQueueDepth) {
            return false;
        }

        if (!item.cacheKey.empty()) {
            singleFlight_[item.cacheKey] = {};  // register leader
        }
        queue_.push_back(std::move(item));
        PumpLocked();
        return true;
    }

    // Cancel queued items that are stale (generation changed).
    // Aborts the item so Execute() returns 503; completion still runs via Dispatch.
    void CancelStale(uint64_t currentNavGen, uint64_t currentHostGen) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& item : queue_) {
            if (item.navGen != currentNavGen || item.hostGen != currentHostGen) {
                if (item.abort) {
                    item.abort->abort();
                }
            }
        }
        if (inFlightAbort_) {
            if (inFlightNavGen_ != currentNavGen || inFlightHostGen_ != currentHostGen) {
                inFlightAbort_->abort();
            }
        }
    }

    // Drain all outstanding work (on host close).
    // Every queued completion is invoked with 503 so deferrals never hang.
    void DrainAll() {
        std::vector<CompletionFn> toComplete;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& item : queue_) {
                if (item.abort) {
                    item.abort->abort();
                }
                if (item.complete) {
                    toComplete.push_back(std::move(item.complete));
                }
                // Leader was registered in singleFlight_; drop it so Dispatch
                // for an already-popped in-flight leader does not re-complete
                // followers twice after we complete them here.
                if (!item.cacheKey.empty()) {
                    singleFlight_.erase(item.cacheKey);
                }
            }
            queue_.clear();

            for (auto& entry : singleFlight_) {
                for (auto& fn : entry.second) {
                    if (fn) {
                        toComplete.push_back(std::move(fn));
                    }
                }
            }
            singleFlight_.clear();

            if (inFlightAbort_) {
                inFlightAbort_->abort();
            }
        }

        ArtworkResult cancelled;
        cancelled.statusCode = 503;
        for (auto& fn : toComplete) {
            PostCompletion(std::move(fn), cancelled);
        }
    }

private:
    // Must hold mutex_.
    void PumpLocked() {
        if (!running_ || busy_ || queue_.empty()) {
            return;
        }

        WorkItem item = std::move(queue_.front());
        queue_.pop_front();
        busy_ = true;
        inFlightAbort_ = item.abort;
        inFlightNavGen_ = item.navGen;
        inFlightHostGen_ = item.hostGen;

        // Run extraction on foobar2000 CPU worker — raw std::thread causes uBugCheck.
        fb2k::inCpuWorkerThread([this, item = std::move(item)]() mutable {
            ArtworkResult result = Execute(item);
            Dispatch(item, std::move(result));

            {
                std::lock_guard<std::mutex> lock(mutex_);
                inFlightAbort_ = nullptr;
                inFlightNavGen_ = 0;
                inFlightHostGen_ = 0;
                busy_ = false;
                cv_.notify_all();
                PumpLocked();
            }
        });
    }

    ArtworkResult Execute(const WorkItem& item) {
        ArtworkResult result;
        result.etag = item.etag;
        result.negCacheKey = item.negCacheKey;
        result.cacheKey = item.cacheKey;

        if (item.abort && item.abort->is_aborting()) {
            result.statusCode = 503;
            return result;
        }

        // 304 fast path (ETag pre-computed on owner thread)
        if (!item.ifNoneMatchEtag.empty() && item.ifNoneMatchEtag == item.etag) {
            result.statusCode = 304;
            result.is304 = true;
            return result;
        }

        try {
            artwork_internal::BinaryArtwork artwork;
            const bool found = artwork_internal::GetArtworkBinaryForPath(
                item.pathUtf8, item.artworkType, artwork);

            if (item.abort && item.abort->is_aborting()) {
                result.statusCode = 503;
                return result;
            }

            if (!found || artwork.bytes.empty()) {
                result.statusCode = 404;
                result.isNegative = true;
                return result;
            }

            if (item.maxSize > 0) {
                int outW = 0, outH = 0;
                const char* outMime = nullptr;
                auto resized = ResizeImageWIC(
                    artwork.bytes.data(),
                    artwork.bytes.size(),
                    item.maxSize,
                    outW,
                    outH,
                    outMime);

                if (!resized.empty()) {
                    result.bytes = std::move(resized);
                    result.mime = outMime ? outMime : "image/jpeg";
                } else {
                    result.bytes = std::move(artwork.bytes);
                    result.mime = artwork.mimeType;
                }
            } else {
                result.bytes = std::move(artwork.bytes);
                result.mime = artwork.mimeType;
            }
            result.statusCode = 200;
        } catch (const exception_aborted&) {
            result.statusCode = 503;
        } catch (const std::exception& ex) {
            LOG("[ArtworkWorker] exception: ", ex.what());
            result.statusCode = 500;
        } catch (...) {
            LOG("[ArtworkWorker] unknown exception");
            result.statusCode = 500;
        }

        return result;
    }

    void Dispatch(const WorkItem& leader, ArtworkResult result) {
        std::vector<CompletionFn> followers;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!leader.cacheKey.empty()) {
                auto fit = singleFlight_.find(leader.cacheKey);
                if (fit != singleFlight_.end()) {
                    followers = std::move(fit->second);
                    singleFlight_.erase(fit);
                }
            }
        }

        PostCompletion(leader.complete, result);
        for (auto& fn : followers) {
            PostCompletion(std::move(fn), result);
        }
    }

    static void PostCompletion(CompletionFn fn, ArtworkResult r) {
        if (!fn) {
            return;
        }
        try {
            fb2k::inMainThread([fn = std::move(fn), r = std::move(r)]() mutable {
                if (fn) {
                    fn(std::move(r));
                }
            });
        } catch (...) {
            // Host teardown may reject main-thread posts; still complete deferral.
            fn(std::move(r));
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<WorkItem> queue_;
    bool running_ = true;
    bool busy_ = false;

    std::unordered_map<std::string, std::vector<CompletionFn>> singleFlight_;
    std::shared_ptr<abort_callback_impl> inFlightAbort_;
    uint64_t inFlightNavGen_ = 0;
    uint64_t inFlightHostGen_ = 0;
};

}  // namespace artwork_worker
