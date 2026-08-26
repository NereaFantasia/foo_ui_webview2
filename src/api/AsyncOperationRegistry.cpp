// AsyncOperationRegistry.cpp
#include "pch.h"
#include "api/AsyncOperationRegistry.h"

#include <cstdio>

namespace fb2k_api {

std::string FormatAsyncOperationId(const std::string& prefix,
                                   uint64_t sequence,
                                   uint64_t randomBits) {
    char buf[80];
    // Fixed-width hex on both halves so ids sort and truncate predictably and
    // never grow past the buffer regardless of the values handed in.
    std::snprintf(buf, sizeof(buf), "%s_%08llx%08llx",
                  prefix.c_str(),
                  static_cast<unsigned long long>(randomBits & 0xFFFFFFFFULL),
                  static_cast<unsigned long long>(sequence & 0xFFFFFFFFULL));
    return buf;
}

BatchEmitScheduler::BatchEmitScheduler(size_t flushCount, int64_t flushIntervalMs)
    : flushCount_(flushCount == 0 ? kDefaultFlushCount : flushCount),
      flushIntervalMs_(flushIntervalMs < 0 ? kDefaultFlushIntervalMs : flushIntervalMs) {}

void BatchEmitScheduler::Start(int64_t nowMs) {
    lastFlushMs_ = nowMs;
}

bool BatchEmitScheduler::ShouldFlush(size_t pendingCount, int64_t nowMs) const {
    if (pendingCount == 0) {
        return false;
    }
    if (pendingCount >= flushCount_) {
        return true;
    }
    return (nowMs - lastFlushMs_) >= flushIntervalMs_;
}

void BatchEmitScheduler::MarkFlushed(int64_t nowMs) {
    lastFlushMs_ = nowMs;
}

}  // namespace fb2k_api
