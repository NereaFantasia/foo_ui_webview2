#pragma once
// MediaReadQueue.h - runs the media reads of deferred API handlers off the main thread.
// Include only after pch.h (depends on foobar2000 SDK types).

#include <algorithm>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "api/TypedApi.h"
#include "utils/SlowMediaPath.h"

namespace api {

// Opening a media file for its tags or artwork can block for seconds on slow or
// cloud storage. Such reads run here one at a time, in call order, on a single
// thread of fb2k's CPU pool, so a burst of slow reads cannot occupy the pool that
// foobar2000 and the artwork resource queue also use. Same scheme as the SDK's
// fb2k::CPUFIFO, whose header does not build with this project's include paths.
// FileApi keeps a second instance for file.read and its siblings, so a slow file
// path and a slow media file do not wait for each other.
//
// There are two such lanes. A read of a file that is slow to open (a share, a mapped
// network drive, a cloud placeholder not on disk) moves to the slow lane, so a
// stuck download holds up only other slow reads, not the local files behind it.
class MediaReadQueue {
public:
    static MediaReadQueue& Instance() {
        static MediaReadQueue queue;
        return queue;
    }

    // `paths` are the files `work` reads, as the API takes them. Without them the
    // work stays on the normal lane.
    void Add(std::function<void()> work, std::vector<std::string> paths = {}) {
        Push({std::move(work), std::move(paths)}, kNormal);
    }

private:
    struct Item {
        std::function<void()> work;
        std::vector<std::string> paths;
    };

    struct Lane {
        std::deque<Item> queue;
        bool running = false;
    };

    static constexpr size_t kNormal = 0;
    static constexpr size_t kSlow = 1;

    void Push(Item item, size_t lane) {
        {
            std::lock_guard lock(mutex_);
            lanes_[lane].queue.push_back(std::move(item));
            if (lanes_[lane].running) return;
            lanes_[lane].running = true;
        }
        try {
            fb2k::inCpuWorkerThread([this, lane] { Drain(lane); });
        } catch (...) {
            // The item stays queued and runs with the next Add.
            std::lock_guard lock(mutex_);
            lanes_[lane].running = false;
            throw;
        }
    }

    void Drain(size_t lane) noexcept {
        for (;;) {
            Item item;
            {
                std::lock_guard lock(mutex_);
                if (lanes_[lane].queue.empty()) {
                    lanes_[lane].running = false;
                    return;
                }
                item = std::move(lanes_[lane].queue.front());
                lanes_[lane].queue.pop_front();
            }
            // The probe may touch a dead volume, which is why it runs here and not where
            // the call arrives on the main thread.
            if (lane == kNormal && std::ranges::any_of(item.paths, slow_media_path::IsSlow)) {
                item.paths.clear();
                try {
                    Push(std::move(item), kSlow);
                } catch (...) {
                    // Same as in Add: the item stays queued for the next slow read.
                }
                continue;
            }
            try {
                item.work();
            } catch (...) {
                // AnswerFromMediaRead already turns exceptions into an answer.
            }
        }
    }

    std::mutex mutex_;
    Lane lanes_[2];
};

// The files a handler's parameters name, for the queue to tell slow ones from fast.
template <class P>
std::vector<std::string> MediaPathsOf(const P& params) {
    if constexpr (requires { params.paths; }) {
        return params.paths;
    } else if constexpr (requires { params.path; }) {
        return {params.path};
    } else if constexpr (requires { params.directory; }) {
        return {params.directory};
    } else {
        return {};
    }
}

// Runs `work` on the media read queue. `work` answers through `responder` itself or
// hands the call on; an exception it lets through answers INTERNAL_ERROR, as on the
// main thread. `paths` are the files it reads (see MediaReadQueue::Add).
template <class F>
void RunOnMediaRead(const DeferredResponder& responder, F work, std::vector<std::string> paths = {}) {
    MediaReadQueue::Instance().Add(
        [responder, work = std::move(work)]() mutable {
            try {
                work();
            } catch (const std::exception& e) {
                responder.SendError(e.what(), ApiErrorCode::INTERNAL_ERROR);
            } catch (...) {
                responder.SendError("Unknown internal error", ApiErrorCode::INTERNAL_ERROR);
            }
        },
        std::move(paths));
}

// Runs `work` on the media read queue and answers with the api::Result it returns.
template <class F>
void AnswerFromMediaRead(const DeferredResponder& responder, F work, std::vector<std::string> paths = {}) {
    RunOnMediaRead(
        responder, [responder, work = std::move(work)]() mutable { Send(responder, work()); }, std::move(paths));
}

// Deferred registration of a handler whose whole body reads media.
template <class P, class R, Result<R> (*Handler)(const P&)>
void OnMediaRead(const P& params, const DeferredResponder& responder) {
    AnswerFromMediaRead(responder, [params] { return Handler(params); }, MediaPathsOf(params));
}

}  // namespace api
