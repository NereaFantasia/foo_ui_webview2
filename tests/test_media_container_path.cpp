#include "pch.h"
#include "utils/MediaContainerPath.h"
#include "utils/PathTraversalSegments.h"

namespace {
std::string UnpackPath(const std::string& container, const std::string& entry = "01.flac") {
    return "unpack://zip|" + std::to_string(container.size()) + "|" + container + "|" + entry;
}
}

TEST(MediaContainerPath, UnpackResolvesTheContainerInsteadOfTheFormatName) {
    EXPECT_EQ(media_container_path::Unwrap(UnpackPath("file://D:\\Music\\album.zip")),
              "D:\\Music\\album.zip");
}

TEST(MediaContainerPath, LengthCountsUtf8BytesAndNestedContainersAreUnwrapped) {
    const std::string archive = "file://D:\\音乐\\专辑.zip";
    EXPECT_EQ(media_container_path::Unwrap(UnpackPath(archive)), "D:\\音乐\\专辑.zip");
    EXPECT_EQ(media_container_path::Unwrap(UnpackPath(UnpackPath(archive, "inner.zip"))),
              "D:\\音乐\\专辑.zip");
}

TEST(MediaContainerPath, BrokenLengthsAndMissingSeparatorsAreRejected) {
    for (const auto* path : {"unpack://zip|0||a", "unpack://zip|-1|x|a", "unpack://zip|x|x|a",
                            "unpack://zip|99999999999999999999999999|x|a", "unpack://zip|9|x|a",
                            "unpack://zip|1|xa", "unpack://|1|x|a", "unpack://zip"}) {
        EXPECT_TRUE(media_container_path::Unwrap(path).empty()) << path;
    }
}

TEST(MediaContainerPath, LocalAndLegacyPathsKeepTheirSecurityRelevantText) {
    EXPECT_EQ(media_container_path::Unwrap("FILE://D:\\Music\\a.flac|subsong:2"),
              "D:\\Music\\a.flac|subsong:2");
    EXPECT_EQ(media_container_path::Unwrap("archive://D:\\Music\\album.zip|/a.flac"),
              "D:\\Music\\album.zip");
    EXPECT_EQ(media_container_path::Unwrap("D:\\Music\\a.flac"), "D:\\Music\\a.flac");
    EXPECT_EQ(media_container_path::Unwrap(UnpackPath("file://D:\\..\\album.zip")),
              "D:\\..\\album.zip");
    EXPECT_EQ(media_container_path::Unwrap("file://D:\\a.flac|subsong:..\\..\\Windows"),
              "D:\\a.flac|subsong:..\\..\\Windows");
    EXPECT_TRUE(path_traversal::ContainsTraversalSegment(L"D:\\a.flac|subsong:..\\..\\Windows"));
}
