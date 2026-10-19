// test_cloud_state.cpp — cloud.getStates 的判定与批次规则（纯逻辑，无 SDK / 无文件 IO）
//
// 判据来源：task-3「宿主暴露云状态只读 API」——
//   local       = exists && !(attrs & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS)
//   placeholder = exists &&  (attrs & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS)
//   paths / directory 恰好一个；paths 为 1..256 条，超过上限整次失败而非静默截断。
//
// 施测层是 header-only 的 cloud_state（src/api/CloudState.h）；Win32 属性读取与
// 目录枚举由 CloudApi.cpp 负责，在测试台 CDP 探针里另验。
#include "pch.h"
#include "api/CloudState.h"

namespace {

constexpr std::uint32_t kArchiveBit = FILE_ATTRIBUTE_ARCHIVE;
constexpr std::uint32_t kDirectoryBit = FILE_ATTRIBUTE_DIRECTORY;

}  // namespace

TEST(CloudStateClassify, NonexistentPathIsNeitherLocalNorPlaceholder) {
    const cloud_state::PathState state = cloud_state::Classify(false, 0);
    EXPECT_FALSE(state.exists);
    EXPECT_FALSE(state.local);
    EXPECT_FALSE(state.placeholder);
    EXPECT_EQ(state.attrs, 0u);
}

TEST(CloudStateClassify, ExistingFileWithoutCloudBitsIsLocal) {
    const cloud_state::PathState state = cloud_state::Classify(true, kArchiveBit);
    EXPECT_TRUE(state.exists);
    EXPECT_TRUE(state.local);
    EXPECT_FALSE(state.placeholder);
    EXPECT_EQ(state.attrs, kArchiveBit);  // 原始属性位原样带出，供调用方看目录位等
}

TEST(CloudStateClassify, RecallOnDataAccessIsPlaceholderAndNotLocal) {
    const std::uint32_t attrs = kArchiveBit | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS;
    const cloud_state::PathState state = cloud_state::Classify(true, attrs);
    EXPECT_TRUE(state.exists);
    EXPECT_FALSE(state.local);
    EXPECT_TRUE(state.placeholder);
    EXPECT_EQ(state.attrs, attrs);
}

TEST(CloudStateClassify, OtherCloudAndDirectoryBitsDoNotMakePlaceholder) {
    // 红线只看 RECALL_ON_DATA_ACCESS：OFFLINE / RECALL_ON_OPEN / DIRECTORY 都不是
    // "读取会水合内容"的判据，不能拿来当 placeholder。
    const std::uint32_t attrs = kDirectoryBit | FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_RECALL_ON_OPEN;
    const cloud_state::PathState state = cloud_state::Classify(true, attrs);
    EXPECT_TRUE(state.exists);
    EXPECT_TRUE(state.local);
    EXPECT_FALSE(state.placeholder);
    EXPECT_EQ(state.attrs, attrs);
}

TEST(CloudStateClassify, ZeroAttributesExistingEntryIsLocal) {
    const cloud_state::PathState state = cloud_state::Classify(true, 0u);
    EXPECT_TRUE(state.exists);
    EXPECT_TRUE(state.local);
    EXPECT_FALSE(state.placeholder);
    EXPECT_EQ(state.attrs, 0u);
}

TEST(CloudStateSelection, ExactlyOneOfPathsOrDirectory) {
    using cloud_state::SelectionError;
    EXPECT_EQ(cloud_state::ValidateSelection(false, false, 0), SelectionError::NeitherGiven);
    EXPECT_EQ(cloud_state::ValidateSelection(true, true, 3), SelectionError::BothGiven);
    EXPECT_EQ(cloud_state::ValidateSelection(true, false, 1), SelectionError::None);
    EXPECT_EQ(cloud_state::ValidateSelection(false, true, 0), SelectionError::None);
}

TEST(CloudStateSelection, PathBatchMustBeNonEmptyAndBounded) {
    using cloud_state::SelectionError;
    EXPECT_EQ(cloud_state::ValidateSelection(true, false, 0), SelectionError::EmptyPaths);
    EXPECT_EQ(cloud_state::ValidateSelection(true, false, cloud_state::kMaxBatchPaths), SelectionError::None);
    EXPECT_EQ(cloud_state::ValidateSelection(true, false, cloud_state::kMaxBatchPaths + 1),
              SelectionError::TooManyPaths);
}

TEST(CloudStateSelection, MaxBatchIs256) {
    EXPECT_EQ(cloud_state::kMaxBatchPaths, 256u);
}

TEST(CloudStateSelection, ErrorNamesAreStable) {
    EXPECT_EQ(cloud_state::SelectionErrorName(cloud_state::SelectionError::None), "none");
    EXPECT_EQ(cloud_state::SelectionErrorName(cloud_state::SelectionError::BothGiven), "both-given");
    EXPECT_EQ(cloud_state::SelectionErrorName(cloud_state::SelectionError::NeitherGiven), "neither-given");
    EXPECT_EQ(cloud_state::SelectionErrorName(cloud_state::SelectionError::EmptyPaths), "empty-paths");
    EXPECT_EQ(cloud_state::SelectionErrorName(cloud_state::SelectionError::TooManyPaths), "too-many-paths");
}
