#pragma once
// MediaReadQueue.h - runs the media reads of deferred API handlers off the main thread.
// Include only after pch.h (depends on foobar2000 SDK types).

#include <deque>
#include <functional>
#include <mutex>

#include "api/TypedApi.h"

namespace api {

// Opening a media file for its tags or artwork can block for seconds on slow or
// cloud storage. Such reads run here one at a time, in call order, on a single
// thread of fb2k's CPU pool, so a burst of slow reads cannot occupy the pool that
// foobar2000 and the artwork resource queue also use. Same scheme as the SDK's
// fb2k::CPUFIFO, whose header does not build with this project's include paths.
// FileApi keeps a second instance for file.read and its siblings, so a slow file
// path and a slow media file do not wait for each other.
class MediaReadQueue {
public:
    static MediaReadQueue& Instance() {
        static MediaReadQueue queue;
        return queue;
    }

    void Add(std::function<void()> work) {
        {
            std::lock_guard lock(mutex_);
            queue_.push_back(std::move(work));
            if (running_) return;
            running_ = true;
        }
        try {
            fb2k::inCpuWorkerThread([this] { Drain(); });
        } catch (...) {
            // The item stays queued and runs with the next Add.
            std::lock_guard lock(mutex_);
            running_ = false;
            throw;
        }
    }

private:
    void Drain() noexcept {
        for (;;) {
            std::function<void()> work;
            {
                std::lock_guard lock(mutex_);
                if (queue_.empty()) {
                    running_ = false;
                    return;
                }
                work = std::move(queue_.front());
                queue_.pop_front();
            }
            try {
                work();
            } catch (...) {
                // AnswerFromMediaRead already turns exceptions into an answer.
            }
        }
    }

    std::mutex mutex_;
    std::deque<std::function<void()>> queue_;
    bool running_ = false;
};

// Runs `work` on the media read queue. `work` answers through `responder` itself or
// hands the call on; an exception it lets through answers INTERNAL_ERROR, as on the
// main thread.
template <class F>
void RunOnMediaRead(const DeferredResponder& responder, F work) {
    MediaReadQueue::Instance().Add([responder, work = std::move(work)]() mutable {
        try {
            work();
        } catch (const std::exception& e) {
            responder.SendError(e.what(), ApiErrorCode::INTERNAL_ERROR);
        } catch (...) {
            responder.SendError("Unknown internal error", ApiErrorCode::INTERNAL_ERROR);
        }
    });
}

// Runs `work` on the media read queue and answers with the api::Result it returns.
template <class F>
void AnswerFromMediaRead(const DeferredResponder& responder, F work) {
    RunOnMediaRead(responder, [responder, work = std::move(work)]() mutable { Send(responder, work()); });
}

// Deferred registration of a handler whose whole body reads media.
template <class P, class R, Result<R> (*Handler)(const P&)>
void OnMediaRead(const P& params, const DeferredResponder& responder) {
    AnswerFromMediaRead(responder, [params] { return Handler(params); });
}

}  // namespace api
