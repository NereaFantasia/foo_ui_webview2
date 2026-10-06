#include "pch.h"
#include "compat/fb2k_types.h"
#include "media/ContainerInfo.h"
#include <filesystem>
#include <fstream>

TEST(MediaFixtures, EncodedMp4AndMatroskaMatchFfprobeMetadata) {
    for (const auto* extension : {"mp4", "mkv"}) {
        const auto path = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "media" /
            (std::string("av-subtitle.") + extension);
        std::ifstream input(path, std::ios::binary);
        ASSERT_TRUE(input.good()) << path.string();
        const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        const auto info = media::InspectContainer(data.size(), [&](std::uint64_t offset, std::size_t length) {
            return std::vector<std::uint8_t>(data.begin() + static_cast<std::size_t>(offset), data.begin() + static_cast<std::size_t>(offset) + length);
        });
        SCOPED_TRACE(extension);
        ASSERT_TRUE(info.recognized);
        ASSERT_EQ(info.tracks.size(), 3u);
        EXPECT_EQ(info.tracks[0].type, "video");
        EXPECT_EQ(info.tracks[0].width, 32);
        EXPECT_EQ(info.tracks[0].height, 24);
        EXPECT_EQ(info.tracks[0].frameRate, 25);
        EXPECT_TRUE(info.tracks[0].codecs.has_value());
        EXPECT_EQ(info.tracks[1].type, "audio");
        EXPECT_EQ(info.tracks[1].channels, 2);
        EXPECT_EQ(info.tracks[1].sampleRate, 48000);
        EXPECT_EQ(info.tracks[1].language, "jpn");
        EXPECT_EQ(info.tracks[1].codecs, "mp4a.40.2");
        EXPECT_EQ(info.tracks[2].type, "subtitle");
        if (std::string_view(extension) == "mkv") {
            EXPECT_NEAR(*info.duration, 0.421, 0.00001);
            ASSERT_EQ(info.attachments.size(), 1u);
            EXPECT_EQ(info.attachments[0].name, "fixture.txt");
            EXPECT_EQ(info.attachments[0].mimeType, "text/plain");
        } else EXPECT_NEAR(*info.duration, 0.4, 0.00001);
    }
}

TEST(MediaFixtures, EncodedChaptersMatchFfprobe) {
    for (const auto* extension : {"m4a", "mka"}) {
        const auto path = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "media" /
            (std::string("chapters.") + extension);
        std::ifstream input(path, std::ios::binary);
        ASSERT_TRUE(input.good()) << path.string();
        const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        const auto info = media::InspectContainer(data.size(), [&](std::uint64_t offset, std::size_t length) {
            return std::vector<std::uint8_t>(data.begin() + static_cast<std::size_t>(offset), data.begin() + static_cast<std::size_t>(offset) + length);
        });
        SCOPED_TRACE(extension);
        ASSERT_TRUE(info.recognized);
        ASSERT_TRUE(info.chapters.has_value());
        ASSERT_EQ(info.chapters->size(), 3u);
        const char* titles[] = {"Alpha", "Beta", "第三章"};
        for (std::size_t i = 0; i < 3; ++i) {
            const auto& chapter = (*info.chapters)[i];
            EXPECT_NEAR(chapter.start, i * 0.5, 0.000001);
            ASSERT_TRUE(chapter.end.has_value());
            EXPECT_NEAR(*chapter.end, i * 0.5 + 0.5, 0.000001);
            EXPECT_EQ(chapter.title, titles[i]);
            EXPECT_FALSE(chapter.edition.has_value());
        }
        // MP4 的章节文本轨不是字幕；ffprobe 同样把它归为 data。
        if (std::string_view(extension) == "m4a") {
            ASSERT_EQ(info.tracks.size(), 2u);
            EXPECT_EQ(info.tracks[1].type, "other");
        } else ASSERT_EQ(info.tracks.size(), 1u);
    }
}
