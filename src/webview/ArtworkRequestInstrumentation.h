#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace artwork_instrumentation {

struct RequestContext {
    std::string_view route;
    uint64_t callbackThreadId = 0;
    uint64_t ownerThreadId = 0;
    uint64_t navigationGeneration = 0;
    uint64_t hostGeneration = 0;
    std::string_view path;
    std::string_view artworkType;
    uint32_t normalizedMaxSize = 0;
};

struct InvocationCounts {
    uint32_t sourceResolveCount = 0;
    uint32_t extractorCount = 0;
    uint32_t resizeCount = 0;
};

struct StageDurations {
    uint64_t callbackMs = 0;
    uint64_t sourceResolveMs = 0;
    uint64_t extractMs = 0;
    uint64_t resizeMs = 0;
    uint64_t responseMs = 0;
    uint64_t totalMs = 0;
};

struct RequestRecord {
    uint64_t requestId = 0;
    std::string route;
    uint64_t callbackThreadId = 0;
    uint64_t ownerThreadId = 0;
    uint64_t navigationGeneration = 0;
    uint64_t hostGeneration = 0;
    std::string pathHash;
    std::string artworkType;
    uint32_t normalizedMaxSize = 0;
    std::string permissionResult;
    InvocationCounts counts;
    StageDurations durations;
    std::string status;
    std::string terminalReason;
};

struct AggregateCounters {
    uint64_t recordedRequests = 0;
    uint64_t permissionDeniedRequests = 0;
    uint64_t sourceResolveInvocations = 0;
    uint64_t extractorInvocations = 0;
    uint64_t resizeInvocations = 0;
    uint64_t droppedRecords = 0;
};

struct Snapshot {
    AggregateCounters counters;
    std::vector<RequestRecord> records;
};

class ArtworkRequestInstrumentation {
public:
    class Request {
    public:
        Request() = default;
        Request(const Request&) = delete;
        Request& operator=(const Request&) = delete;
        Request(Request&&) = delete;
        Request& operator=(Request&&) = delete;

        bool IsActive() const noexcept { return owner_ != nullptr; }
        uint64_t RequestId() const noexcept { return record_.requestId; }

        RequestRecord& Data() noexcept { return record_; }
        const RequestRecord& Data() const noexcept { return record_; }

        void SetPermissionResult(std::string_view result) {
            if (IsActive()) record_.permissionResult.assign(result);
        }

        void RecordSourceResolve(uint64_t elapsedMs = 0) noexcept {
            if (!IsActive()) return;
            ++record_.counts.sourceResolveCount;
            record_.durations.sourceResolveMs += elapsedMs;
        }

        void RecordExtractor(uint64_t elapsedMs = 0) noexcept {
            if (!IsActive()) return;
            ++record_.counts.extractorCount;
            record_.durations.extractMs += elapsedMs;
        }

        void RecordResize(uint64_t elapsedMs = 0) noexcept {
            if (!IsActive()) return;
            ++record_.counts.resizeCount;
            record_.durations.resizeMs += elapsedMs;
        }

        bool Complete(std::string_view status, std::string_view terminalReason) {
            if (!IsActive()) return false;
            record_.status.assign(status);
            record_.terminalReason.assign(terminalReason);
            ArtworkRequestInstrumentation* owner = std::exchange(owner_, nullptr);
            owner->Publish(std::move(record_));
            return true;
        }

    private:
        friend class ArtworkRequestInstrumentation;

        Request(ArtworkRequestInstrumentation* owner, RequestRecord record)
            : owner_(owner), record_(std::move(record)) {}

        ArtworkRequestInstrumentation* owner_ = nullptr;
        RequestRecord record_;
    };

    explicit ArtworkRequestInstrumentation(size_t retainedRecordLimit = 256) noexcept
        : retainedRecordLimit_(retainedRecordLimit) {}

    bool IsEnabled() const noexcept { return enabled_.load(); }
    void SetEnabled(bool enabled) noexcept { enabled_.store(enabled); }

    Request Begin(const RequestContext& context) {
        if (!IsEnabled()) return {};

        RequestRecord record;
        record.requestId = nextRequestId_.fetch_add(1);
        record.route.assign(context.route);
        record.callbackThreadId = context.callbackThreadId;
        record.ownerThreadId = context.ownerThreadId;
        record.navigationGeneration = context.navigationGeneration;
        record.hostGeneration = context.hostGeneration;
        record.pathHash = StablePathHash(context.path);
        record.artworkType.assign(context.artworkType);
        record.normalizedMaxSize = context.normalizedMaxSize;
        return Request(this, std::move(record));
    }

    AggregateCounters GetAggregateCounters() const {
        std::lock_guard lock(recordsMutex_);
        return counters_;
    }

    Snapshot GetSnapshot() const {
        Snapshot result;
        std::lock_guard lock(recordsMutex_);
        result.counters = counters_;
        result.records.assign(records_.begin(), records_.end());
        return result;
    }

    void Clear() {
        std::lock_guard lock(recordsMutex_);
        records_.clear();
        counters_ = {};
    }

    static std::string StablePathHash(std::string_view path) {
        constexpr uint64_t kOffsetBasis = 14695981039346656037ULL;
        constexpr uint64_t kPrime = 1099511628211ULL;
        uint64_t hash = kOffsetBasis;
        for (const unsigned char byte : path) {
            hash ^= byte;
            hash *= kPrime;
        }

        constexpr char kHex[] = "0123456789abcdef";
        std::string result(16, '0');
        for (size_t i = 0; i < result.size(); ++i) {
            const size_t shift = (result.size() - i - 1) * 4;
            result[i] = kHex[(hash >> shift) & 0x0f];
        }
        return result;
    }

private:
    void Publish(RequestRecord record) {
        std::lock_guard lock(recordsMutex_);
        ++counters_.recordedRequests;
        if (record.permissionResult == "denied") {
            ++counters_.permissionDeniedRequests;
        }
        counters_.sourceResolveInvocations += record.counts.sourceResolveCount;
        counters_.extractorInvocations += record.counts.extractorCount;
        counters_.resizeInvocations += record.counts.resizeCount;

        if (retainedRecordLimit_ == 0) {
            ++counters_.droppedRecords;
            return;
        }
        if (records_.size() == retainedRecordLimit_) {
            records_.pop_front();
            ++counters_.droppedRecords;
        }
        records_.push_back(std::move(record));
    }

    inline static std::atomic<uint64_t> nextRequestId_{1};

    const size_t retainedRecordLimit_;
    std::atomic<bool> enabled_{false};
    mutable std::mutex recordsMutex_;
    std::deque<RequestRecord> records_;
    AggregateCounters counters_;
};

}  // namespace artwork_instrumentation