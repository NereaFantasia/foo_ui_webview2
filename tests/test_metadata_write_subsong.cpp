// test_metadata_write_subsong.cpp — MetadataWrite/MetadataRemoveTag subsong parsing
// Validates the ParseSubsongIndex logic used by MetadataWrite and MetadataRemoveTag.
#include "pch.h"
#include "compat/fb2k_types.h"
#include "utils/SubsongPath.h"

// MetadataApi.cpp's ParseSubsongIndex forwards to fb2k_paths::SplitLegacyTrackPath
// and narrows the index to int; this adapter does the same, so the cases below
// exercise the product function.
namespace MetadataTest {

struct SubsongParseResult {
    std::string cleanPath;
    int subsongIndex = 0;
};

static SubsongParseResult ParseSubsongIndex(const std::string& path, int explicitCueIndex) {
    fb2k_paths::LegacyTrackPath r = fb2k_paths::SplitLegacyTrackPath(path, explicitCueIndex);
    return {r.path, static_cast<int>(r.subsong)};
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
