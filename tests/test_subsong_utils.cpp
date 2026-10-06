// test_subsong_utils.cpp — Subsong 路径解析测试
// Uses compat shim instead of full foobar2000 SDK
#include "pch.h"
#include "compat/fb2k_types.h"

// Re-implement the parsing logic inline to avoid SDK header dependency.
// This mirrors SubsongUtils::ParseSubsongPath exactly.
namespace SubsongUtils_Test {
    inline std::pair<std::string, t_uint32> ParseSubsongPath(const std::string& path) {
        std::string filePath = path;
        t_uint32 subsongIndex = 0;
        size_t pos = path.find("|subsong:");
        if (pos != std::string::npos) {
            filePath = path.substr(0, pos);
            try {
                subsongIndex = static_cast<t_uint32>(std::stoul(path.substr(pos + 9)));
            } catch (...) {
                subsongIndex = 0;
            }
        }
        return { filePath, subsongIndex };
    }
}

TEST(SubsongUtils, NoSubsong) {
    auto [path, idx] = SubsongUtils_Test::ParseSubsongPath("C:\\music\\file.flac");
    EXPECT_EQ(path, "C:\\music\\file.flac");
    EXPECT_EQ(idx, 0u);
}

TEST(SubsongUtils, WithSubsong) {
    auto [path, idx] = SubsongUtils_Test::ParseSubsongPath("C:\\music\\file.flac|subsong:3");
    EXPECT_EQ(path, "C:\\music\\file.flac");
    EXPECT_EQ(idx, 3u);
}

TEST(SubsongUtils, SubsongZero) {
    auto [path, idx] = SubsongUtils_Test::ParseSubsongPath("/path/to/file.cue|subsong:0");
    EXPECT_EQ(path, "/path/to/file.cue");
    EXPECT_EQ(idx, 0u);
}

TEST(SubsongUtils, InvalidSubsongNumber) {
    auto [path, idx] = SubsongUtils_Test::ParseSubsongPath("file.mp3|subsong:abc");
    EXPECT_EQ(path, "file.mp3");
    EXPECT_EQ(idx, 0u);  // falls back to 0
}

TEST(SubsongUtils, EmptyPath) {
    auto [path, idx] = SubsongUtils_Test::ParseSubsongPath("");
    EXPECT_EQ(path, "");
    EXPECT_EQ(idx, 0u);
}

TEST(SubsongUtils, LargeSubsongIndex) {
    auto [path, idx] = SubsongUtils_Test::ParseSubsongPath("file.ape|subsong:999");
    EXPECT_EQ(path, "file.ape");
    EXPECT_EQ(idx, 999u);
}

// ============================================================================
// MakeSidecarPath tests — re-implements MakeSidecarPath inline to avoid a
// full SDK dependency.
// ============================================================================
#include <filesystem>

namespace SubsongUtils_Test {
    // Simple ASCII-only widen (sufficient for test paths)
    inline std::wstring SimpleWiden(const std::string& s) {
        return std::wstring(s.begin(), s.end());
    }

    inline std::wstring MakeSidecarPath(const std::string& audioPath, const std::wstring& ext) {
        auto [filePath, subsong] = ParseSubsongPath(audioPath);
        std::wstring widePath = SimpleWiden(filePath);

        namespace fs = std::filesystem;
        fs::path p(widePath);

        if (subsong > 0) {
            int trackNum = static_cast<int>(subsong) + 1;
            wchar_t suffix[16];
            if (trackNum < 100)
                swprintf(suffix, 16, L".%02d", trackNum);
            else
                swprintf(suffix, 16, L".%d", trackNum);
            std::wstring newFilename = p.stem().wstring() + suffix + std::wstring(ext);
            p = p.parent_path() / newFilename;
        } else {
            p.replace_extension(ext);
        }
        return p.wstring();
    }
}

TEST(SubsongUtils, MakeSidecarPath_NoSubsong) {
    auto r = SubsongUtils_Test::MakeSidecarPath("C:\\a\\file.flac", L".lrc");
    EXPECT_EQ(r, L"C:\\a\\file.lrc");
}

TEST(SubsongUtils, MakeSidecarPath_SubsongZero) {
    auto r = SubsongUtils_Test::MakeSidecarPath("C:\\a\\file.flac|subsong:0", L".lrc");
    EXPECT_EQ(r, L"C:\\a\\file.lrc");
}

TEST(SubsongUtils, MakeSidecarPath_SubsongOne) {
    auto r = SubsongUtils_Test::MakeSidecarPath("C:\\a\\file.flac|subsong:1", L".lrc");
    EXPECT_EQ(r, L"C:\\a\\file.02.lrc");
}

TEST(SubsongUtils, MakeSidecarPath_ISO) {
    auto r = SubsongUtils_Test::MakeSidecarPath("D:\\SACD.iso|subsong:9", L".lrc");
    EXPECT_EQ(r, L"D:\\SACD.10.lrc");
}

TEST(SubsongUtils, MakeSidecarPath_Overflow) {
    auto r = SubsongUtils_Test::MakeSidecarPath("D:\\disc|subsong:99", L".lrc");
    EXPECT_EQ(r, L"D:\\disc.100.lrc");
}

TEST(SubsongUtils, MakeSidecarPath_Txt) {
    auto r = SubsongUtils_Test::MakeSidecarPath("C:\\a\\file.cue|subsong:2", L".txt");
    EXPECT_EQ(r, L"C:\\a\\file.03.txt");
}

TEST(SubsongUtils, MakeSidecarPath_CueSubsongThree) {
    auto r = SubsongUtils_Test::MakeSidecarPath("D:\\album.flac|subsong:2", L".lrc");
    EXPECT_EQ(r, L"D:\\album.03.lrc");
}

TEST(SubsongUtils, MakeSidecarPath_SubsongZeroExplicit) {
    auto r = SubsongUtils_Test::MakeSidecarPath("D:\\album.flac|subsong:0", L".lrc");
    EXPECT_EQ(r, L"D:\\album.lrc");
}

// ============================================================================
// TryMakeNativeMediaPath tests — re-implements the helper inline for the same
// reason as above. Paths stay ASCII-only: SimpleWiden truncates bytes rather
// than decoding UTF-8, so non-ASCII cases belong in live verification.
// ============================================================================

namespace SubsongUtils_Test {
    inline bool TryMakeNativeMediaPath(const std::string& in, std::string& out) {
        out.clear();
        if (in.empty()) return false;

        std::string body = in;
        if (in.rfind("file://", 0) == 0) {
            body = in.substr(7);
        } else if (in.find("://") != std::string::npos) {
            return false;
        }

        const std::string base = ParseSubsongPath(body).first;

        const bool driveRooted =
            base.size() >= 3 &&
            ((base[0] >= 'A' && base[0] <= 'Z') || (base[0] >= 'a' && base[0] <= 'z')) &&
            base[1] == ':' && (base[2] == '\\' || base[2] == '/');
        const bool unc = base.size() >= 2 && base[0] == '\\' && base[1] == '\\';

        if (!driveRooted && !unc) return false;

        out = body;
        return true;
    }
}

TEST(SubsongUtils, NativePath_StripsFileScheme) {
    std::string out;
    EXPECT_TRUE(SubsongUtils_Test::TryMakeNativeMediaPath("file://D:\\Music\\song.flac", out));
    EXPECT_EQ(out, "D:\\Music\\song.flac");
}

TEST(SubsongUtils, NativePath_AlreadyNativeIsIdentity) {
    std::string out;
    EXPECT_TRUE(SubsongUtils_Test::TryMakeNativeMediaPath("D:\\Music\\song.flac", out));
    EXPECT_EQ(out, "D:\\Music\\song.flac");
}

TEST(SubsongUtils, NativePath_KeepsSubsongSuffix) {
    std::string out;
    EXPECT_TRUE(SubsongUtils_Test::TryMakeNativeMediaPath("file://D:\\Music\\song.flac|subsong:2", out));
    EXPECT_EQ(out, "D:\\Music\\song.flac|subsong:2");
}

TEST(SubsongUtils, NativePath_RejectsThreeSlashUri) {
    std::string out;
    EXPECT_FALSE(SubsongUtils_Test::TryMakeNativeMediaPath("file:///D:/Music/song.flac", out));
}

TEST(SubsongUtils, NativePath_RejectsArchive) {
    std::string out;
    EXPECT_FALSE(SubsongUtils_Test::TryMakeNativeMediaPath("archive://D:\\a.zip|/t.flac", out));
}

TEST(SubsongUtils, NativePath_RejectsUnpack) {
    std::string out;
    EXPECT_FALSE(SubsongUtils_Test::TryMakeNativeMediaPath("unpack://zip|0|D:\\a.zip|t.flac", out));
}

TEST(SubsongUtils, NativePath_RejectsHttp) {
    std::string out;
    EXPECT_FALSE(SubsongUtils_Test::TryMakeNativeMediaPath("http://example.com/song.flac", out));
}

TEST(SubsongUtils, NativePath_RejectsFileRelative) {
    // LyricsApi and ArtworkApi both branch on this form, so it is a real input.
    std::string out;
    EXPECT_FALSE(SubsongUtils_Test::TryMakeNativeMediaPath("file-relative://song.flac", out));
}

TEST(SubsongUtils, NativePath_RejectsRelative) {
    std::string out;
    EXPECT_FALSE(SubsongUtils_Test::TryMakeNativeMediaPath("relative/song.flac", out));
}

TEST(SubsongUtils, NativePath_AcceptsUnc) {
    std::string out;
    EXPECT_TRUE(SubsongUtils_Test::TryMakeNativeMediaPath("\\\\NAS\\share\\song.flac", out));
    EXPECT_EQ(out, "\\\\NAS\\share\\song.flac");
}

TEST(SubsongUtils, NativePath_AcceptsFileSchemeUnc) {
    // get_path() emits this form for network paths.
    std::string out;
    EXPECT_TRUE(SubsongUtils_Test::TryMakeNativeMediaPath("file://\\\\NAS\\share\\song.flac", out));
    EXPECT_EQ(out, "\\\\NAS\\share\\song.flac");
}

TEST(SubsongUtils, NativePath_RejectsEmpty) {
    std::string out;
    EXPECT_FALSE(SubsongUtils_Test::TryMakeNativeMediaPath("", out));
}

TEST(SubsongUtils, NativePath_SchemeIsCaseSensitive) {
    // Matches the SDK's strncmp against a lowercase "file://".
    std::string out;
    EXPECT_FALSE(SubsongUtils_Test::TryMakeNativeMediaPath("FILE://D:\\Music\\song.flac", out));
}

TEST(SubsongUtils, NativePath_FeedsMakeSidecarPath) {
    std::string out;
    ASSERT_TRUE(SubsongUtils_Test::TryMakeNativeMediaPath("file://D:\\Music\\song.flac", out));
    EXPECT_EQ(SubsongUtils_Test::MakeSidecarPath(out, L".lrc"), L"D:\\Music\\song.lrc");
}

TEST(SubsongUtils, NativePath_FeedsMakeSidecarPathWithSubsong) {
    std::string out;
    ASSERT_TRUE(SubsongUtils_Test::TryMakeNativeMediaPath("file://D:\\Music\\album.flac|subsong:2", out));
    EXPECT_EQ(SubsongUtils_Test::MakeSidecarPath(out, L".lrc"), L"D:\\Music\\album.03.lrc");
}
