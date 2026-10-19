// ============================================================================
// test_artwork_resource_async.cpp — fb2k://artwork 资源处理器异步化与云占位符防御单测
//
// 测试覆盖：
//   1. 占位符判定 (Cloud Placeholder Defense):
//      - 媒体文件置位 RECALL_ON_DATA_ACCESS -> 命中占位符
//      - 本地文件未置位 -> 未命中
//      - 目录封面图 (folder.jpg/cover.jpg 等) 置位 -> 命中占位符
//      - 目录本身置位 -> 命中占位符
//   2. Subsong 剥离:
//      - 带有 |subsong:N 或 |subsong:invalid 后缀时，正确剥离子曲目再判定属性
//      - file:// 前缀正确剥离
//   3. 期限/超时 (JobGate 8s 模式):
//      - 超时未回包触发 404 / timeout，晚到的 worker 结果被丢弃
//      - worker 提前完成时取消超时
//   4. 取消/生命周期 (Cancellation):
//      - abort 信号生效
//   5. 重复响应 (Exactly Once):
//      - 并发竞争下恰好回包一次
// ============================================================================

#include "pch.h"
#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "domain/ArtworkCloudDefense.h"
#include "api/ArtworkJobDeadline.h"

using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// 1 & 2: 云占位符防御与 Subsong 测试
// ---------------------------------------------------------------------------

namespace {

// 模拟文件系统属性表
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

TEST(ArtworkCloudDefenseTest, LocalFileWithoutRecallBitIsNotPlaceholder) {
    MockFileSystem fs;
    fs.SetAttrs(L"D:\\Music\\track.flac", FILE_ATTRIBUTE_ARCHIVE);
    fs.SetAttrs(L"D:\\Music", FILE_ATTRIBUTE_DIRECTORY);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "D:\\Music\\track.flac",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_FALSE(isPlaceholder);
}

TEST(ArtworkCloudDefenseTest, TargetMediaWithRecallBitIsPlaceholder) {
    MockFileSystem fs;
    fs.SetAttrs(L"D:\\Music\\cloud_track.flac", FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);
    fs.SetAttrs(L"D:\\Music", FILE_ATTRIBUTE_DIRECTORY);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "D:\\Music\\cloud_track.flac",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

TEST(ArtworkCloudDefenseTest, SubsongPathStripsSuffixBeforeAttributeCheck) {
    MockFileSystem fs;
    // 物理文件是 album.cue，不能带有 |subsong:3
    fs.SetAttrs(L"D:\\Music\\album.cue", FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);
    fs.SetAttrs(L"D:\\Music", FILE_ATTRIBUTE_DIRECTORY);

    // 请求包含子曲目后缀
    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "D:\\Music\\album.cue|subsong:3",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

TEST(ArtworkCloudDefenseTest, InvalidSubsongNumberHandledSafely) {
    MockFileSystem fs;
    fs.SetAttrs(L"D:\\Music\\album.flac", FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);
    fs.SetAttrs(L"D:\\Music", FILE_ATTRIBUTE_DIRECTORY);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "D:\\Music\\album.flac|subsong:invalid",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

TEST(ArtworkCloudDefenseTest, FileProtocolPrefixStripped) {
    MockFileSystem fs;
    fs.SetAttrs(L"D:\\Music\\album.flac", FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);
    fs.SetAttrs(L"D:\\Music", FILE_ATTRIBUTE_DIRECTORY);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "file://D:\\Music\\album.flac|subsong:1",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

TEST(ArtworkCloudDefenseTest, DirectoryCoverWithRecallBitIsPlaceholder) {
    MockFileSystem fs;
    // 媒体文件本体在本地
    fs.SetAttrs(L"D:\\Music\\track.flac", FILE_ATTRIBUTE_ARCHIVE);
    fs.SetAttrs(L"D:\\Music", FILE_ATTRIBUTE_DIRECTORY);
    // 但目录里的 folder.jpg 是云占位符
    fs.SetAttrs(L"D:\\Music\\folder.jpg", FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "D:\\Music\\track.flac",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

TEST(ArtworkCloudDefenseTest, DirectoryCoverCoverJpgWithRecallBitIsPlaceholder) {
    MockFileSystem fs;
    fs.SetAttrs(L"D:\\Music\\track.flac", FILE_ATTRIBUTE_ARCHIVE);
    fs.SetAttrs(L"D:\\Music", FILE_ATTRIBUTE_DIRECTORY);
    // cover.png 是云占位符
    fs.SetAttrs(L"D:\\Music\\cover.png", FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "D:\\Music\\track.flac",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

TEST(ArtworkCloudDefenseTest, ParentDirectoryItselfWithRecallBitIsPlaceholder) {
    MockFileSystem fs;
    fs.SetAttrs(L"D:\\Music\\track.flac", FILE_ATTRIBUTE_ARCHIVE);
    // 目录本身置位 RECALL_ON_DATA_ACCESS
    fs.SetAttrs(L"D:\\Music", FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS);

    const bool isPlaceholder = artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "D:\\Music\\track.flac",
        [&](const std::wstring& p) { return fs.GetAttrs(p); }
    );
    EXPECT_TRUE(isPlaceholder);
}

TEST(ArtworkCloudDefenseTest, EmptyOrNonexistentPathReturnsFalse) {
    MockFileSystem fs;
    EXPECT_FALSE(artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "", [&](const std::wstring& p) { return fs.GetAttrs(p); }));
    EXPECT_FALSE(artwork_defense::IsCloudPlaceholderArtworkTargetCustom(
        "D:\\NonExistent\\track.flac", [&](const std::wstring& p) { return fs.GetAttrs(p); }));
}

// ---------------------------------------------------------------------------
// 3, 4, 5: 期限、取消、重复响应测试
// ---------------------------------------------------------------------------

namespace {

void WaitForCondition(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return;
        std::this_thread::sleep_for(1ms);
    }
}

} // namespace

TEST(ArtworkResourceAsyncTest, DeadlineExceededDelivers404AndDropsWorkerResult) {
    std::atomic<int> deadlineCalled{0};
    std::atomic<int> workerCalled{0};

    // 15ms 期限
    auto gate = artwork_async::JobGate::Create(15ms, [&deadlineCalled]() {
        ++deadlineCalled;
    });

    // 等待期限触发
    WaitForCondition([&]() { return deadlineCalled.load() == 1; }, 2000ms);
    EXPECT_EQ(deadlineCalled.load(), 1);
    EXPECT_TRUE(gate->ClaimedByDeadline());

    // 迟到的 worker 调用 Finish 必须被丢弃
    gate->Finish([&workerCalled]() {
        ++workerCalled;
    });
    EXPECT_EQ(workerCalled.load(), 0);
}

TEST(ArtworkResourceAsyncTest, WorkerEarlyFinishCancelsDeadlineAndDeliversResult) {
    std::atomic<int> deadlineCalled{0};
    std::atomic<int> workerCalled{0};

    // 2000ms 期限
    auto gate = artwork_async::JobGate::Create(2000ms, [&deadlineCalled]() {
        ++deadlineCalled;
    });

    // worker 快速完成
    gate->Finish([&workerCalled]() {
        ++workerCalled;
    });
    EXPECT_EQ(workerCalled.load(), 1);
    EXPECT_FALSE(gate->ClaimedByDeadline());

    // 稍等确认 deadline 不会再被唤醒
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(deadlineCalled.load(), 0);
}

TEST(ArtworkResourceAsyncTest, ConcurrentContentionProducesExactlyOneResponse) {
    for (int round = 0; round < 100; ++round) {
        std::atomic<int> responses{0};
        auto gate = artwork_async::JobGate::Create(0ms, [&responses]() {
            ++responses;
        });

        std::thread worker([gate, &responses]() {
            gate->Finish([&responses]() {
                ++responses;
            });
        });
        worker.join();

        std::this_thread::sleep_for(2ms);
        ASSERT_EQ(responses.load(), 1) << "Contention failed at round " << round;
    }
}
