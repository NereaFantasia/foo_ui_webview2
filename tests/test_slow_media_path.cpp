#include "pch.h"
#include "utils/SlowMediaPath.h"
#include <filesystem>
#include <fstream>

using namespace slow_media_path;

TEST(SlowMediaPath, UncPathsAreNetworkShares) {
    EXPECT_TRUE(IsUncPath(R"(\\nas\music\a.flac)"));
    EXPECT_TRUE(IsUncPath(R"(\\?\UNC\nas\music\a.flac)"));
    EXPECT_FALSE(IsUncPath(R"(\\?\C:\music\a.flac)"));
    EXPECT_FALSE(IsUncPath(R"(\\.\PhysicalDrive0)"));
    EXPECT_FALSE(IsUncPath(R"(C:\music\a.flac)"));
    EXPECT_FALSE(IsUncPath(""));
}

TEST(SlowMediaPath, RecallAttributesMarkFilesNotOnDisk) {
    EXPECT_TRUE(AttributesNeedRecall(FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS));
    EXPECT_TRUE(AttributesNeedRecall(FILE_ATTRIBUTE_RECALL_ON_OPEN));
    EXPECT_TRUE(AttributesNeedRecall(FILE_ATTRIBUTE_OFFLINE));
    EXPECT_FALSE(AttributesNeedRecall(FILE_ATTRIBUTE_ARCHIVE));
    // A hydrated OneDrive file keeps its reparse point but loses the recall flag.
    EXPECT_FALSE(AttributesNeedRecall(FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_REPARSE_POINT));
    EXPECT_FALSE(AttributesNeedRecall(INVALID_FILE_ATTRIBUTES));
}

TEST(SlowMediaPath, LocalFilePathStripsTheApiDecorations) {
    EXPECT_EQ(LocalFilePath(R"(file://D:\Music\a.flac)"), R"(D:\Music\a.flac)");
    EXPECT_EQ(LocalFilePath(R"(D:\Music\a.flac|subsong:3)"), R"(D:\Music\a.flac)");
    EXPECT_EQ(LocalFilePath(R"(file://D:\Music\disc.cue|subsong:2)"), R"(D:\Music\disc.cue)");
    EXPECT_EQ(LocalFilePath(R"(file://\\nas\music\a.flac)"), R"(\\nas\music\a.flac)");
    EXPECT_EQ(LocalFilePath("unpack://zip|14|D:\\Music\\a.zip|song.flac"), R"(D:\Music\a.zip)");
}

TEST(SlowMediaPath, LocalFilePathIgnoresWhatIsNotAFile) {
    EXPECT_EQ(LocalFilePath("http://example.com/a.flac"), "");
    EXPECT_EQ(LocalFilePath(R"(file-relative://..\music\a.flac)"), "");
    EXPECT_EQ(LocalFilePath("cdda://1"), "");
    EXPECT_EQ(LocalFilePath(R"(\\?\C:\music\a.flac)"), "");
    EXPECT_EQ(LocalFilePath(""), "");
}

TEST(SlowMediaPath, ASharePathIsSlowWithoutAnyProbe) {
    // Unroutable address: a probe would hang, so this returning means none was made.
    EXPECT_TRUE(IsSlow(R"(\\10.255.255.1\share\a.flac)"));
    EXPECT_TRUE(IsSlow(R"(file://\\10.255.255.1\share\a.flac|subsong:2)"));
}

TEST(SlowMediaPath, AnOrdinaryLocalFileIsNotSlow) {
    const auto file = std::filesystem::temp_directory_path() / "foo_ui_webview2_slow_media_path.tmp";
    { std::ofstream(file) << "x"; }
    EXPECT_FALSE(IsSlow(file.string()));
    EXPECT_FALSE(IsSlow("file://" + file.string()));
    std::filesystem::remove(file);
    // A file that is not there has no recall flag either.
    EXPECT_FALSE(IsSlow(file.string()));
}

TEST(SlowMediaPath, OnlyAFileOnLocalDiskCanBeOnlineOnly) {
    // Not probed: a share could stall, and these have no recall attributes anyway.
    EXPECT_FALSE(IsOnlineOnly(R"(\\10.255.255.1\share\a.flac)"));
    EXPECT_FALSE(IsOnlineOnly("http://example.com/a.flac"));
    EXPECT_FALSE(IsOnlineOnly(""));

    const auto file = std::filesystem::temp_directory_path() / "foo_ui_webview2_online_only.tmp";
    { std::ofstream(file) << "x"; }
    EXPECT_FALSE(IsOnlineOnly(file.string()));
    EXPECT_FALSE(IsOnlineOnly("file://" + file.string() + "|subsong:2"));
    std::filesystem::remove(file);
}

TEST(SlowMediaPath, NonFilePathsAreNotSlow) {
    EXPECT_FALSE(IsSlow("http://example.com/a.flac"));
    EXPECT_FALSE(IsSlow(""));
}
