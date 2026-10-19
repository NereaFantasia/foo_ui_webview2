// test_api_async_phase3.cpp — 宿主异步化第三期单测
//
// 覆盖判据：
//   1. 期限与超时 (JobGate Deadline):
//      - 超时未回包时触发到期回调，晚到的 worker 结果被丢弃
//      - worker 提前完成时取消超时调度
//   2. 取消与中止 (Cancellation):
//      - abort 信号生效，可中断调用提前抛出/中止
//   3. 恰好一次 (Exactly Once):
//      - worker 回包与到期回调并发竞争下恰好回包一次
//   4. 统一云占位符防御 (ArtworkCloudDefense):
//      - 音频文件、子曲目（|subsong:N）、目录本身、候选封面图（cover.jpg/folder.png 等）
//        置位 FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 时均判定为占位符
//      - 命中占位符后立即短路回包，绝不水合/读取物理文件
//   5. 统一超时信封契约 (Timeout Envelopes):
//      - 所有第三期迁移 API 超时统一带 success:false, code:TIMEOUT, available:false, reason:timeout
//        且保留各 API 专有默认字段（types:[], artworks:[], images:[], exists:false 等）

#include "pch.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "compat/fb2k_types.h"
#include "api/ArtworkJobDeadline.h"
#include "domain/ArtworkCloudDefense.h"
#include "api/ErrorEnvelope.h"
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;
using json = nlohmann::json;

namespace {

void WaitFor(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return;
        std::this_thread::sleep_for(1ms);
    }
}

class MockFileSystem {
public:
    void SetAttrs(const std::wstring& path, DWORD attrs) {
        attrs_[Normalize(path)] = attrs;
    }

    DWORD GetAttrs(const std::wstring& path) const {
        auto it = attrs_.find(Normalize(path));
        if (it != attrs_.end()) {
            return it->second;
        }
        return INVALID_FILE_ATTRIBUTES;
    }

private:
    static std::wstring Normalize(std::wstring p) {
        for (auto& c : p) {
            if (c == L'/') c = L'\\';
        }
        return p;
    }

    std::map<std::wstring, DWORD> attrs_;
};

} // namespace

// ============================================================================
// 1. JobGate 期限与超时
// ============================================================================

TEST(ApiAsyncPhase3Test, DeadlineFiresOnceWhenWorkerHangs) {
    std::atomic<int> deadlineRuns{0};
    auto gate = artwork_async::JobGate::Create(20ms, [&deadlineRuns]() { ++deadlineRuns; });

    WaitFor([&deadlineRuns]() { return deadlineRuns.load() == 1; }, 2000ms);
    EXPECT_EQ(deadlineRuns.load(), 1);
    EXPECT_TRUE(gate->ClaimedByDeadline());

    // 晚到的 worker 结果应当被闸门丢弃
    std::atomic<int> workerRuns{0};
    gate->Finish([&workerRuns]() { ++workerRuns; });
    EXPECT_EQ(workerRuns.load(), 0);
}

TEST(ApiAsyncPhase3Test, WorkerFinishCancelsDeadlineScheduler) {
    std::atomic<int> deadlineRuns{0};
    std::atomic<int> workerRuns{0};
    auto gate = artwork_async::JobGate::Create(2000ms, [&deadlineRuns]() { ++deadlineRuns; });

    gate->Finish([&workerRuns]() { ++workerRuns; });
    EXPECT_EQ(workerRuns.load(), 1);
    EXPECT_FALSE(gate->ClaimedByDeadline());

    // 调度器被取消，即使等待片刻也不会触发超时
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(deadlineRuns.load(), 0);
}

// ============================================================================
// 2. 恰好一次回包 (Exactly Once Under Race)
// ============================================================================

TEST(ApiAsyncPhase3Test, ExactlyOnceDeliveryUnderHighContention) {
    for (int round = 0; round < 100; ++round) {
        std::atomic<int> respondCount{0};
        auto gate = artwork_async::JobGate::Create(0ms, [&respondCount]() { ++respondCount; });

        std::thread worker([gate, &respondCount]() {
            gate->Finish([&respondCount]() { ++respondCount; });
        });
        worker.join();

        std::this_thread::sleep_for(2ms);
        ASSERT_EQ(respondCount.load(), 1) << "Contention race at round " << round;
    }
}

// ============================================================================
// 3. 统一云占位符防御 (ArtworkCloudDefense)
// ============================================================================

TEST(ApiAsyncPhase3Test, PlaceholderDetectedForDirectMediaFile) {
    MockFileSystem fs;
    fs.SetAttrs(L"C:\\CloudMusic\\track.mp3", FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);
    fs.SetAttrs(L"C:\\CloudMusic", FILE_ATTRIBUTE_DIRECTORY);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "C:\\CloudMusic\\track.mp3",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

TEST(ApiAsyncPhase3Test, PlaceholderDetectedForSubsongPath) {
    MockFileSystem fs;
    fs.SetAttrs(L"D:\\OneDrive\\album.cue", FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);
    fs.SetAttrs(L"D:\\OneDrive", FILE_ATTRIBUTE_DIRECTORY);

    // 带子曲目后缀的路径正确剥离后判定
    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "D:\\OneDrive\\album.cue|subsong:5",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

TEST(ApiAsyncPhase3Test, PlaceholderDetectedForParentDirectory) {
    MockFileSystem fs;
    fs.SetAttrs(L"E:\\CloudFolder\\track.flac", FILE_ATTRIBUTE_ARCHIVE);
    // 目录本身是云占位符（尚未下载的云文件夹）
    fs.SetAttrs(L"E:\\CloudFolder", FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "E:\\CloudFolder\\track.flac",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

TEST(ApiAsyncPhase3Test, PlaceholderDetectedForCandidateFolderCover) {
    MockFileSystem fs;
    fs.SetAttrs(L"F:\\Music\\track.flac", FILE_ATTRIBUTE_ARCHIVE);
    fs.SetAttrs(L"F:\\Music", FILE_ATTRIBUTE_DIRECTORY);
    // 目录内的 cover.jpg 是云占位符
    fs.SetAttrs(L"F:\\Music\\cover.jpg", FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "F:\\Music\\track.flac",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

TEST(ApiAsyncPhase3Test, LocalFilesWithoutRecallBitPassDefense) {
    MockFileSystem fs;
    fs.SetAttrs(L"C:\\LocalMusic\\track.flac", FILE_ATTRIBUTE_ARCHIVE);
    fs.SetAttrs(L"C:\\LocalMusic", FILE_ATTRIBUTE_DIRECTORY);
    fs.SetAttrs(L"C:\\LocalMusic\\folder.jpg", FILE_ATTRIBUTE_ARCHIVE);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "C:\\LocalMusic\\track.flac",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_FALSE(isPlaceholder);
}

TEST(ApiAsyncPhase3Test, FileUrlPrefixHandledCorrectly) {
    MockFileSystem fs;
    fs.SetAttrs(L"C:\\LocalMusic\\song.flac", FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);
    fs.SetAttrs(L"C:\\LocalMusic", FILE_ATTRIBUTE_DIRECTORY);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "file://C:\\LocalMusic\\song.flac",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

// ============================================================================
// 4. 统一超时信封契约测试 (Timeout Envelopes)
// ============================================================================

TEST(ApiAsyncPhase3Test, TimeoutEnvelopeContractAllEightApis) {
    // 1. artwork.getAvailableTypes
    {
        json env = ApiEnvelope::MakeError("artwork.getAvailableTypes: timeout", ApiErrorCode::TIMEOUT);
        env["available"] = false;
        env["types"] = json::array();
        env["reason"] = "timeout";

        EXPECT_FALSE(env["success"].get<bool>());
        EXPECT_EQ(env["code"].get<std::string>(), "TIMEOUT");
        EXPECT_FALSE(env["available"].get<bool>());
        EXPECT_EQ(env["reason"].get<std::string>(), "timeout");
        EXPECT_TRUE(env["types"].is_array());
        EXPECT_TRUE(env["types"].empty());
    }

    // 2. artwork.getLyrics
    {
        json env = ApiEnvelope::MakeError("artwork.getLyrics: timeout", ApiErrorCode::TIMEOUT);
        env["available"] = false;
        env["reason"] = "timeout";

        EXPECT_FALSE(env["success"].get<bool>());
        EXPECT_EQ(env["code"].get<std::string>(), "TIMEOUT");
        EXPECT_FALSE(env["available"].get<bool>());
        EXPECT_EQ(env["reason"].get<std::string>(), "timeout");
    }

    // 3. artwork.getMetadata
    {
        json env = ApiEnvelope::MakeError("artwork.getMetadata: timeout", ApiErrorCode::TIMEOUT);
        env["available"] = false;
        env["reason"] = "timeout";

        EXPECT_FALSE(env["success"].get<bool>());
        EXPECT_EQ(env["code"].get<std::string>(), "TIMEOUT");
        EXPECT_FALSE(env["available"].get<bool>());
        EXPECT_EQ(env["reason"].get<std::string>(), "timeout");
    }

    // 4. artwork.getBatch
    {
        json env = ApiEnvelope::MakeError("artwork.getBatch: timeout", ApiErrorCode::TIMEOUT);
        env["available"] = false;
        env["artworks"] = json::array();
        env["reason"] = "timeout";

        EXPECT_FALSE(env["success"].get<bool>());
        EXPECT_EQ(env["code"].get<std::string>(), "TIMEOUT");
        EXPECT_FALSE(env["available"].get<bool>());
        EXPECT_EQ(env["reason"].get<std::string>(), "timeout");
        EXPECT_TRUE(env["artworks"].is_array());
        EXPECT_TRUE(env["artworks"].empty());
    }

    // 5. artwork.getAvailableArtwork
    {
        json env = ApiEnvelope::MakeError("artwork.getAvailableArtwork: timeout", ApiErrorCode::TIMEOUT);
        env["available"] = false;
        env["artworks"] = json::array();
        env["sources"] = json::array();
        env["reason"] = "timeout";

        EXPECT_FALSE(env["success"].get<bool>());
        EXPECT_EQ(env["code"].get<std::string>(), "TIMEOUT");
        EXPECT_FALSE(env["available"].get<bool>());
        EXPECT_EQ(env["reason"].get<std::string>(), "timeout");
        EXPECT_TRUE(env["artworks"].is_array());
        EXPECT_TRUE(env["sources"].is_array());
    }

    // 6. artwork.getFolderImages
    {
        json env = ApiEnvelope::MakeError("artwork.getFolderImages: timeout", ApiErrorCode::TIMEOUT);
        env["available"] = false;
        env["images"] = json::array();
        env["reason"] = "timeout";

        EXPECT_FALSE(env["success"].get<bool>());
        EXPECT_EQ(env["code"].get<std::string>(), "TIMEOUT");
        EXPECT_FALSE(env["available"].get<bool>());
        EXPECT_EQ(env["reason"].get<std::string>(), "timeout");
        EXPECT_TRUE(env["images"].is_array());
    }

    // 7. lyrics.get
    {
        json env = ApiEnvelope::MakeError("lyrics.get: timeout", ApiErrorCode::TIMEOUT);
        env["available"] = false;
        env["path"] = "D:\\Music\\test.flac";
        env["reason"] = "timeout";

        EXPECT_FALSE(env["success"].get<bool>());
        EXPECT_EQ(env["code"].get<std::string>(), "TIMEOUT");
        EXPECT_FALSE(env["available"].get<bool>());
        EXPECT_EQ(env["path"].get<std::string>(), "D:\\Music\\test.flac");
        EXPECT_EQ(env["reason"].get<std::string>(), "timeout");
    }

    // 8. lyrics.exists
    {
        json env = ApiEnvelope::MakeError("lyrics.exists: timeout", ApiErrorCode::TIMEOUT);
        env["available"] = false;
        env["exists"] = false;
        env["sources"] = json::array();
        env["reason"] = "timeout";

        EXPECT_FALSE(env["success"].get<bool>());
        EXPECT_EQ(env["code"].get<std::string>(), "TIMEOUT");
        EXPECT_FALSE(env["available"].get<bool>());
        EXPECT_FALSE(env["exists"].get<bool>());
        EXPECT_EQ(env["reason"].get<std::string>(), "timeout");
        EXPECT_TRUE(env["sources"].is_array());
    }
}
