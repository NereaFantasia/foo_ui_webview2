#include "pch.h"

#include "../src/webview/ArtworkRequestInstrumentation.h"

#include <algorithm>
#include <atomic>
#include <ranges>
#include <thread>

namespace artwork_instrumentation_test {

using artwork_instrumentation::ArtworkRequestInstrumentation;
using artwork_instrumentation::RequestContext;

RequestContext MakeContext(std::string_view path) {
    RequestContext context;
    context.route = "app-artwork";
    context.callbackThreadId = 41;
    context.ownerThreadId = 17;
    context.navigationGeneration = 5;
    context.hostGeneration = 9;
    context.path = path;
    context.artworkType = "front";
    context.normalizedMaxSize = 300;
    return context;
}

TEST(ArtworkRequestInstrumentation, DiagnosticsAreOffByDefault) {
    ArtworkRequestInstrumentation instrumentation;

    auto request = instrumentation.Begin(MakeContext(R"(E:\private\album\track.flac)"));
    EXPECT_FALSE(request.IsActive());
    EXPECT_EQ(request.RequestId(), 0u);
    EXPECT_FALSE(request.Complete("denied", "permission-denied"));

    const auto snapshot = instrumentation.GetSnapshot();
    EXPECT_TRUE(snapshot.records.empty());
    EXPECT_EQ(snapshot.counters.recordedRequests, 0u);
    EXPECT_EQ(snapshot.counters.extractorInvocations, 0u);
}

TEST(ArtworkRequestInstrumentation, StoresOnlyStablePathHash) {
    ArtworkRequestInstrumentation instrumentation;
    instrumentation.SetEnabled(true);
    const std::string privatePath = R"(E:\Users\listener\Music\Secret Album\track.flac)";

    auto request = instrumentation.Begin(MakeContext(privatePath));
    ASSERT_TRUE(request.Complete("success", "response-created"));

    const auto snapshot = instrumentation.GetSnapshot();
    ASSERT_EQ(snapshot.records.size(), 1u);
    const auto& record = snapshot.records.front();
    EXPECT_EQ(record.pathHash, ArtworkRequestInstrumentation::StablePathHash(privatePath));
    EXPECT_EQ(record.pathHash.size(), 16u);
    EXPECT_EQ(record.pathHash.find(privatePath), std::string::npos);
    EXPECT_EQ(record.pathHash.find("Secret Album"), std::string::npos);
    EXPECT_EQ(
        ArtworkRequestInstrumentation::StablePathHash(privatePath),
        ArtworkRequestInstrumentation::StablePathHash(privatePath)
    );
    EXPECT_NE(
        ArtworkRequestInstrumentation::StablePathHash(privatePath),
        ArtworkRequestInstrumentation::StablePathHash(R"(E:\other\track.flac)")
    );
}

TEST(ArtworkRequestInstrumentation, DeniedRequestDoesNotInventExtractorWork) {
    ArtworkRequestInstrumentation instrumentation;
    instrumentation.SetEnabled(true);

    auto request = instrumentation.Begin(MakeContext(R"(E:\blocked\track.flac)"));
    request.SetPermissionResult("denied");
    request.Data().durations.callbackMs = 2;
    request.Data().durations.totalMs = 2;
    ASSERT_TRUE(request.Complete("denied", "permission-denied"));

    const auto snapshot = instrumentation.GetSnapshot();
    ASSERT_EQ(snapshot.records.size(), 1u);
    EXPECT_EQ(snapshot.records.front().counts.extractorCount, 0u);
    EXPECT_EQ(snapshot.records.front().counts.sourceResolveCount, 0u);
    EXPECT_EQ(snapshot.records.front().counts.resizeCount, 0u);
    EXPECT_EQ(snapshot.counters.permissionDeniedRequests, 1u);
    EXPECT_EQ(snapshot.counters.extractorInvocations, 0u);
}

TEST(ArtworkRequestInstrumentation, CapturesCountsThreadsAndGenerations) {
    ArtworkRequestInstrumentation instrumentation;
    instrumentation.SetEnabled(true);

    auto request = instrumentation.Begin(MakeContext(R"(E:\music\track.flac)"));
    request.SetPermissionResult("allowed");
    request.RecordSourceResolve(3);
    request.RecordExtractor(11);
    request.RecordExtractor(7);
    request.RecordResize(5);
    request.Data().durations.callbackMs = 1;
    request.Data().durations.responseMs = 2;
    request.Data().durations.totalMs = 29;
    ASSERT_TRUE(request.Complete("success", "response-created"));

    const auto snapshot = instrumentation.GetSnapshot();
    ASSERT_EQ(snapshot.records.size(), 1u);
    const auto& record = snapshot.records.front();
    EXPECT_EQ(record.callbackThreadId, 41u);
    EXPECT_EQ(record.ownerThreadId, 17u);
    EXPECT_EQ(record.navigationGeneration, 5u);
    EXPECT_EQ(record.hostGeneration, 9u);
    EXPECT_EQ(record.counts.sourceResolveCount, 1u);
    EXPECT_EQ(record.counts.extractorCount, 2u);
    EXPECT_EQ(record.counts.resizeCount, 1u);
    EXPECT_EQ(record.durations.sourceResolveMs, 3u);
    EXPECT_EQ(record.durations.extractMs, 18u);
    EXPECT_EQ(record.durations.resizeMs, 5u);
    EXPECT_EQ(snapshot.counters.sourceResolveInvocations, 1u);
    EXPECT_EQ(snapshot.counters.extractorInvocations, 2u);
    EXPECT_EQ(snapshot.counters.resizeInvocations, 1u);
}

TEST(ArtworkRequestInstrumentation, ConcurrentRequestIdsAreUnique) {
    ArtworkRequestInstrumentation instrumentation(2048);
    instrumentation.SetEnabled(true);
    constexpr int kThreadCount = 8;
    constexpr int kRequestsPerThread = 128;
    std::atomic start{false};
    std::vector<std::jthread> workers;
    workers.reserve(kThreadCount);

    for (int threadIndex = 0; threadIndex < kThreadCount; ++threadIndex) {
        workers.emplace_back([&, threadIndex]() {
            while (!start.load()) {
                std::this_thread::yield();
            }
            for (int requestIndex = 0; requestIndex < kRequestsPerThread; ++requestIndex) {
                RequestContext context = MakeContext("concurrent-path");
                context.callbackThreadId = static_cast<uint64_t>(threadIndex + 1);
                auto request = instrumentation.Begin(context);
                request.SetPermissionResult("allowed");
                EXPECT_TRUE(request.Complete("success", "response-created"));
            }
        });
    }

    start.store(true);
    for (auto& worker : workers) worker.join();

    const auto snapshot = instrumentation.GetSnapshot();
    ASSERT_EQ(snapshot.records.size(), static_cast<size_t>(kThreadCount * kRequestsPerThread));
    EXPECT_EQ(snapshot.counters.recordedRequests, snapshot.records.size());
    std::vector<uint64_t> requestIds;
    requestIds.reserve(snapshot.records.size());
    for (const auto& record : snapshot.records) requestIds.push_back(record.requestId);
    std::ranges::sort(requestIds);
    EXPECT_GT(requestIds.front(), 0u);
    EXPECT_EQ(std::ranges::adjacent_find(requestIds), requestIds.end());
}

}  // namespace artwork_instrumentation_test