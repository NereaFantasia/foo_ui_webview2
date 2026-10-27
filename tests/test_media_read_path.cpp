#include "pch.h"
#include "utils/MediaReadPath.h"
#include "utils/PathProtocolScheme.h"
#include <stdexcept>

namespace {
std::string Unpack(const std::string& container, const std::string& entry = "song.flac") {
    return "unpack://zip|" + std::to_string(container.size()) + "|" + container + "|" + entry;
}

auto NoNativeResolution() {
    return [](const std::string&, std::string&) {
        ADD_FAILURE() << "native resolution must not run";
        return false;
    };
}
}

TEST(MediaReadPath, PortableArchiveUsesItsPhysicalContainerWithoutChangingTrackIdentity) {
    const std::string container = "file-relative://..\\..\\OST\\专辑.zip";
    const std::string input = Unpack(container, "disc/02.flac") + "|subsong:3";
    const auto before = input;
    unsigned calls = 0;
    const auto result = media_read_path::Resolve(input, [&](const std::string& path, std::string& native) {
        ++calls;
        EXPECT_EQ(path, container);
        native = "E:\\OST\\专辑.zip";
        return true;
    });
    ASSERT_EQ(result.error, nullptr);
    EXPECT_EQ(result.path, "E:\\OST\\专辑.zip|subsong:3");
    EXPECT_EQ(calls, 1u);
    EXPECT_EQ(input, before);
    const auto track = fb2k_paths::SplitSubsongPath(input);
    EXPECT_EQ(track.path, Unpack(container, "disc/02.flac"));
    EXPECT_EQ(track.subsong, 3u);
}

TEST(MediaReadPath, NakedAndNestedPortablePathsShareTheResolver) {
    const std::string relative = "file-relative://..\\Music\\album.zip";
    for (const auto& input : {relative, Unpack(relative), Unpack(Unpack(relative, "inner.zip"))}) {
        unsigned calls = 0;
        const auto result = media_read_path::Resolve(input, [&](const std::string& path, std::string& native) {
            ++calls;
            EXPECT_EQ(path, relative);
            native = "E:\\Music\\album.zip";
            return true;
        });
        ASSERT_EQ(result.error, nullptr) << input;
        EXPECT_EQ(result.path, "E:\\Music\\album.zip");
        EXPECT_EQ(calls, 1u);
    }
}

TEST(MediaReadPath, NativeContainersKeepTraversalAndDeviceTextForTheSafetyPipeline) {
    for (const auto& physical : {"E:\\Music\\a.zip", "C:\\Windows\\a.zip", "E:\\..\\a.zip",
                                 "\\\\nas\\music\\a.zip", "Z:\\a.zip", "\\\\?\\C:\\Windows\\a.zip"}) {
        const auto result = media_read_path::Resolve(Unpack(std::string("file://") + physical), NoNativeResolution());
        ASSERT_EQ(result.error, nullptr);
        EXPECT_EQ(result.path, physical);
    }
    const auto result = media_read_path::Resolve(Unpack("file://E:\\..\\a.zip"), NoNativeResolution());
    EXPECT_TRUE(media_read_path::HasTraversal(result.path));
}

TEST(MediaReadPath, ArchiveEntriesAreNotLocalDirectories) {
    const auto result = media_read_path::Resolve(Unpack("file://E:\\a.zip", "../disc/../../song.flac"), NoNativeResolution());
    ASSERT_EQ(result.error, nullptr);
    EXPECT_EQ(result.path, "E:\\a.zip");
    EXPECT_FALSE(media_read_path::HasTraversal(result.path));
    EXPECT_EQ(media_read_path::Resolve("archive://E:\\a.zip|/song.flac", NoNativeResolution()).path, "E:\\a.zip");
}

TEST(MediaReadPath, MissingFailedThrownOrNonNativeResolutionIsRejected) {
    const std::string input = Unpack("file-relative://..\\a.zip");
    for (const auto* native : {"", "relative\\a.zip", "E:a.zip", "file-relative://a.zip", "https://example.com/a.zip"}) {
        const auto result = media_read_path::Resolve(input, [&](const std::string&, std::string& out) {
            out = native;
            return true;
        });
        ASSERT_NE(result.error, nullptr) << native;
        EXPECT_TRUE(result.path.empty());
    }
    EXPECT_NE(media_read_path::Resolve(input, [](const std::string&, std::string& out) {
        out = "E:\\a.zip";
        return false;
    }).error, nullptr);
    EXPECT_NE(media_read_path::Resolve(input, [](const std::string&, std::string&) -> bool {
        throw std::runtime_error("unavailable");
    }).error, nullptr);
}

TEST(MediaReadPath, MalformedContainersDoNotReachTheNativeResolver) {
    for (const auto* input : {"", "unpack://zip", "unpack://zip|0||a", "unpack://zip|999|x|a",
                              "unpack://zip|-1|x|a", "unpack://zip|999999999999999999999999|x|a"}) {
        const auto result = media_read_path::Resolve(input, NoNativeResolution());
        EXPECT_NE(result.error, nullptr) << input;
    }
    EXPECT_NE(media_read_path::Resolve(Unpack("https://example.com/a.zip"), NoNativeResolution()).error, nullptr);
    EXPECT_NE(media_read_path::Resolve(std::string("E:\\a\0b", 6), NoNativeResolution()).error, nullptr);
}

TEST(MediaReadPath, SubsongTraversalCannotDisappearDuringPortableResolution) {
    for (const auto& input : {std::string("file-relative://..\\a.flac|subsong:..\\..\\Windows"),
                              Unpack("file-relative://..\\a.zip") + "|subsong:2/../Windows"}) {
        const auto result = media_read_path::Resolve(input, NoNativeResolution());
        ASSERT_NE(result.error, nullptr);
        EXPECT_STREQ(result.error, "Path traversal detected");
    }
}

TEST(MediaReadPath, UnreadableSubsongStillUsesTheExistingFallback) {
    const std::string input = "file-relative://..\\a.flac|subsong:abc";
    const auto result = media_read_path::Resolve(input, [](const std::string& path, std::string& out) {
        EXPECT_EQ(path, "file-relative://..\\a.flac");
        out = "E:\\a.flac";
        return true;
    });
    ASSERT_EQ(result.error, nullptr);
    EXPECT_EQ(result.path, "E:\\a.flac|subsong:abc");
    EXPECT_EQ(fb2k_paths::SplitSubsongPath(input).subsong, 0u);
}

TEST(MediaReadPath, ProtocolCaseDoesNotChangeThePhysicalFileName) {
    const auto result = media_read_path::Resolve("FILE-RELATIVE://..\\Music\\MiXeD.flac", [](const std::string& path, std::string& out) {
        EXPECT_EQ(path, "file-relative://..\\Music\\MiXeD.flac");
        out = "E:\\Music\\MiXeD.flac";
        return true;
    });
    ASSERT_EQ(result.error, nullptr);
    EXPECT_EQ(result.path, "E:\\Music\\MiXeD.flac");
    // 其它权限入口继续使用原分类，不跟随媒体读取入口改策略。
    EXPECT_TRUE(path_protocol::IsVirtualOrNetworkProtocol(L"file-relative://..\\Music\\MiXeD.flac"));
}

TEST(MediaReadPath, ResolvedLandingStillCarriesItsSecurityRelevantText) {
    for (const auto* native : {"C:\\Windows\\a.flac", "\\\\localhost\\C$\\Windows\\a.flac",
                               "\\\\?\\C:\\Windows\\a.flac", "E:\\Music\\..\\a.flac"}) {
        const auto result = media_read_path::Resolve("file-relative://..\\a.flac", [&](const std::string&, std::string& out) {
            out = native;
            return true;
        });
        ASSERT_EQ(result.error, nullptr);
        EXPECT_EQ(result.path, native);
    }
}
