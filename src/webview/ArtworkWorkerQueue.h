#pragma once
// ============================================================================
// ArtworkWorkerQueue — safe async artwork extraction off the UI thread.
//
// 改进（2026-10-20 fix/artwork-resource-async）：
//   1. 8s 有界超时（ArtworkJobDeadline / JobGate），超时直接回 404 并中断提取，
//      保证响应恰好一次，绝不挂死或阻滞后续请求；
//   2. 云占位符防御（ArtworkCloudDefense）：工作线程内二次防御，绝不水合/下载；
//   3. 并发提取（kMaxConcurrentWorkers = 4）与队列扩容（kMaxQueueDepth = 1024），
//      解决 612 专辑墙瞬间打满深度 32 导致数百个 503 拒绝与长时间排队卡顿；
//   4. 销毁守卫（DrainAll + activeWorkers_ == 0 同步等待），保证无 UAF。
// ============================================================================

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "api/ArtworkApi.h"
#include "api/ArtworkJobDeadline.h"
#include "domain/ArtworkCloudDefense.h"
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
    std::string cacheKey;     // for L1 write-back on 200
    std::string fingerprint;  // source fingerprint captured with the request; stored on L1 write-back
};

using CompletionFn = std::function<void(ArtworkResult)>;

struct WorkItem {
    std::string pathUtf8;
    std::string artworkType;
    int maxSize = 0;
    std::string cacheKey;     // empty if maxSize==0 → no single-flight
    std::string negCacheKey;
    std::string fingerprint;
    std::string etag;
    std::string ifNoneMatchEtag;

    uint64_t navGen = 0;
    uint64_t hostGen = 0;

    std::shared_ptr<abort_callback_impl> abort;
    CompletionFn complete;
};

class ArtworkWorkerQueue {
public:
    static constexpr size_t kMaxQueueDepth = 1024;
    static constexpr size_t kMaxConcurrentWorkers = 4;
    static constexpr std::chrono::milliseconds kArtworkDeadline{8000};

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
            cv_.wait(lock, [this]() { return activeWorkers_ == 0; });
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
        for (auto& pair : inFlight_) {
            if (pair.second.navGen != currentNavGen || pair.second.hostGen != currentHostGen) {
                if (pair.second.abort) {
                    pair.second.abort->abort();
                }
            }
        }
    }

    // Drain all outstanding work (on host close).
    // Every queued completion is invoked with 503 so deferrals never hang.
    void DrainAll() {
        std::vector<CompletionFn> toComplete;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
            for (auto& item : queue_) {
                if (item.abort) {
                    item.abort->abort();
                }
                if (item.complete) {
                    toComplete.push_back(std::move(item.complete));
                }
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

            for (auto& pair : inFlight_) {
                if (pair.second.abort) {
                    pair.second.abort->abort();
                }
                if (pair.second.complete) {
                    toComplete.push_back(std::move(pair.second.complete));
                }
            }
            inFlight_.clear();
        }

        ArtworkResult cancelled;
        cancelled.statusCode = 503;
        for (auto& fn : toComplete) {
            PostCompletion(std::move(fn), cancelled);
        }
    }

private:
    struct InFlightInfo {
        uint64_t id;
        std::string cacheKey;
        std::shared_ptr<abort_callback_impl> abort;
        std::shared_ptr<artwork_async::JobGate> gate;
        uint64_t navGen;
        uint64_t hostGen;
        CompletionFn complete;
    };

    // Must hold mutex_.
    void PumpLocked() {
        if (!running_) {
            return;
        }

        while (activeWorkers_ < kMaxConcurrentWorkers && !queue_.empty()) {
            WorkItem item = std::move(queue_.front());
            queue_.pop_front();
            ++activeWorkers_;

            const uint64_t itemId = nextItemId_++;
            auto abort = item.abort ? item.abort : std::make_shared<abort_callback_impl>();
            item.abort = abort;

            const std::string cacheKey = item.cacheKey;
            const std::string negKey = item.negCacheKey;

            // 创建 JobGate 带有界 8s 期限
            auto gate = artwork_async::JobGate::Create(
                kArtworkDeadline,
                [this, itemId, cacheKey, abort, negKey]() {
                    // 到期回调由 DeadlineScheduler 线程触发
                    abort->abort();

                    ArtworkResult timeoutResult;
                    timeoutResult.statusCode = 404;  // 超时返回 404 占位
                    timeoutResult.isNegative = false;
                    timeoutResult.negCacheKey = negKey;
                    timeoutResult.cacheKey = cacheKey;

                    OnDeadlineExpired(itemId, cacheKey, std::move(timeoutResult));
                }
            );

            inFlight_[itemId] = InFlightInfo{
                itemId, cacheKey, abort, gate,
                item.navGen, item.hostGen, std::move(item.complete)
            };

            // Run extraction on foobar2000 CPU worker — raw std::thread causes uBugCheck.
            fb2k::inCpuWorkerThread([this, itemId, item = std::move(item), gate]() mutable {
                ArtworkResult result = Execute(item);

                // JobGate 保证：worker 结果与到期超时恰好一个回包，另一侧静默丢弃
                gate->Finish([this, itemId, cacheKey = item.cacheKey, result = std::move(result)]() mutable {
                    OnWorkerFinish(itemId, cacheKey, std::move(result));
                });

                OnWorkerThreadExit(itemId);
            });
        }
    }

    void OnWorkerFinish(uint64_t itemId, const std::string& cacheKey, ArtworkResult result) {
        CompletionFn leaderComplete;
        std::vector<CompletionFn> followers;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = inFlight_.find(itemId);
            if (it != inFlight_.end()) {
                leaderComplete = std::move(it->second.complete);
                inFlight_.erase(it);
            }
            if (!cacheKey.empty()) {
                auto fit = singleFlight_.find(cacheKey);
                if (fit != singleFlight_.end()) {
                    followers = std::move(fit->second);
                    singleFlight_.erase(fit);
                }
            }
        }

        PostCompletion(std::move(leaderComplete), result);
        for (auto& fn : followers) {
            PostCompletion(std::move(fn), result);
        }
    }

    void OnDeadlineExpired(uint64_t itemId, const std::string& cacheKey, ArtworkResult result) {
        CompletionFn leaderComplete;
        std::vector<CompletionFn> followers;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = inFlight_.find(itemId);
            if (it != inFlight_.end()) {
                leaderComplete = std::move(it->second.complete);
                inFlight_.erase(it);
            }
            if (!cacheKey.empty()) {
                auto fit = singleFlight_.find(cacheKey);
                if (fit != singleFlight_.end()) {
                    followers = std::move(fit->second);
                    singleFlight_.erase(fit);
                }
            }
        }

        PostCompletion(std::move(leaderComplete), result);
        for (auto& fn : followers) {
            PostCompletion(std::move(fn), result);
        }
    }

    void OnWorkerThreadExit(uint64_t itemId) {
        std::lock_guard<std::mutex> lock(mutex_);
        inFlight_.erase(itemId);
        --activeWorkers_;
        cv_.notify_all();
        PumpLocked();
    }

    ArtworkResult Execute(const WorkItem& item) {
        ArtworkResult result;
        result.etag = item.etag;
        result.negCacheKey = item.negCacheKey;
        result.cacheKey = item.cacheKey;
        result.fingerprint = item.fingerprint;

        if (item.abort && item.abort->is_aborting()) {
            result.statusCode = 503;
            return result;
        }

        // 云占位符防御：工作线程内二次防御，绝不水合/下载
        if (artwork_defense::IsCloudPlaceholderArtworkTarget(item.pathUtf8)) {
            result.statusCode = 404;
            result.isNegative = true;
            return result;
        }

        // 304 fast path (ETag pre-computed on owner thread)
        if (!item.ifNoneMatchEtag.empty() && !item.etag.empty() && item.ifNoneMatchEtag == item.etag) {
            result.statusCode = 304;
            result.is304 = true;
            return result;
        }

        try {
            abort_callback& abort = item.abort
                ? static_cast<abort_callback&>(*item.abort)
                : static_cast<abort_callback&>(fb2k::noAbort);
            artwork_internal::BinaryArtwork artwork;
            const bool found = artwork_internal::GetArtworkBinaryForPath(
                item.pathUtf8, item.artworkType, artwork, abort);

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

            // 计算返回数据的真实 ETag
            if (!result.bytes.empty()) {
                const size_t contentHash = std::hash<std::string_view>{}(
                    std::string_view(reinterpret_cast<const char*>(result.bytes.data()), result.bytes.size()));
                char etagBuf[32];
                snprintf(etagBuf, sizeof(etagBuf), "\"%zx\"", contentHash);
                result.etag = etagBuf;

                if (!item.ifNoneMatchEtag.empty() && item.ifNoneMatchEtag == result.etag) {
                    result.statusCode = 304;
                    result.is304 = true;
                    return result;
                }
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
    size_t activeWorkers_ = 0;

    std::unordered_map<std::string, std::vector<CompletionFn>> singleFlight_;
    uint64_t nextItemId_ = 1;
    std::unordered_map<uint64_t, InFlightInfo> inFlight_;
};

}  // namespace artwork_worker
