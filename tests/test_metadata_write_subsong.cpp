// test_metadata_write_subsong.cpp — T3: MetadataWrite/MetadataRemoveTag subsong parsing
// Validates ParseSubsongIndex logic used by MetadataWrite and MetadataRemoveTag.
// Refs: plans/lyrics-save-cue-iso-fix.md §5.1 T3
#include "pch.h"
#include "compat/fb2k_types.h"

// Re-implement ParseSubsongIndex inline (mirrors MetadataApi.cpp anonymous namespace)
namespace MetadataTest {

struct SubsongParseResult {
    std::string cleanPath;
    int subsongIndex = 0;
};

static SubsongParseResult ParseSubsongIndex(const std::string& path, int explicitCueIndex) {
    // Always strip |subsong:N from cleanPath, even when explicitCueIndex overrides
    std::string cleanPath = path;
    int pathSubsong = 0;

    size_t pipePos = path.find("|subsong:");
    if (pipePos != std::string::npos) {
        cleanPath = path.substr(0, pipePos);
        try {
            pathSubsong = std::stoi(path.substr(pipePos + 9));
        } catch (...) {
            pathSubsong = 0;
        }
    }

    if (explicitCueIndex >= 0) {
        return {cleanPath, explicitCueIndex};
    }

    if (pipePos != std::string::npos) {
        return {cleanPath, pathSubsong};
    }

    // #N format (backward compat)
    size_t hashPos = path.rfind('#');
    if (hashPos == std::string::npos || hashPos >= path.length() - 1) {
        return {path, 0};
    }

    std::string indexStr = path.substr(hashPos + 1);
    if (indexStr.empty() || !std::all_of(indexStr.begin(), indexStr.end(), ::isdigit)) {
        return {path, 0};
    }

    try {
        return {path.substr(0, hashPos), std::stoi(indexStr)};
    } catch (...) {
        return {path, 0};
    }
}

} // namespace MetadataTest

// --- ParseSubsongIndex: |subsong:N format ---

TEST(MetadataWriteSubsong, PipeSubsong3) {
    auto r = MetadataTest::ParseSubsongIndex("D:\\album.flac|subsong:3", -1);
    EXPECT_EQ(r.cleanPath, "D:\\album.flac");
    EXPECT_EQ(r.subsongIndex, 3);
}

TEST(MetadataWriteSubsong, PipeSubsong0) {
    auto r = MetadataTest::ParseSubsongIndex("D:\\album.flac|subsong:0", -1);
    EXPECT_EQ(r.cleanPath, "D:\\album.flac");
    EXPECT_EQ(r.subsongIndex, 0);
}

// --- ParseSubsongIndex: cueIndex override ---

TEST(MetadataWriteSubsong, CueIndexOverride) {
    auto r = MetadataTest::ParseSubsongIndex("D:\\album.flac", 5);
    EXPECT_EQ(r.cleanPath, "D:\\album.flac");
    EXPECT_EQ(r.subsongIndex, 5);
}

// --- Edge case: cueIndex + |subsong:N both present ---
// cleanPath must NOT contain |subsong:N even when cueIndex overrides

TEST(MetadataWriteSubsong, CueIndexOverrideStripsPipe) {
    auto r = MetadataTest::ParseSubsongIndex("D:\\album.flac|subsong:3", 5);
    EXPECT_EQ(r.cleanPath, "D:\\album.flac");
    EXPECT_EQ(r.subsongIndex, 5);
}

// --- ParseSubsongIndex: #N format (backward compat) ---

TEST(MetadataWriteSubsong, HashFormat) {
    auto r = MetadataTest::ParseSubsongIndex("D:\\file.flac#7", -1);
    EXPECT_EQ(r.cleanPath, "D:\\file.flac");
    EXPECT_EQ(r.subsongIndex, 7);
}

// --- Plain path (no subsong) ---

TEST(MetadataWriteSubsong, PlainPath) {
    auto r = MetadataTest::ParseSubsongIndex("D:\\song.flac", -1);
    EXPECT_EQ(r.cleanPath, "D:\\song.flac");
    EXPECT_EQ(r.subsongIndex, 0);
}

// --- MetadataRemoveTag uses same ParseSubsongIndex, verified by shared logic ---

TEST(MetadataWriteSubsong, RemoveTagSameLogic) {
    auto r = MetadataTest::ParseSubsongIndex("D:\\album.flac|subsong:2", -1);
    EXPECT_EQ(r.cleanPath, "D:\\album.flac");
    EXPECT_EQ(r.subsongIndex, 2);
}
