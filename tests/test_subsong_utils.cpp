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
// T1: MakeSidecarPath tests (plans/lyrics-save-cue-iso-fix.md §5.1 T1)
// Re-implements MakeSidecarPath inline to avoid full SDK dependency.
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
