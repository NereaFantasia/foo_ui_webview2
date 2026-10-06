#include "pch.h"
#include "media/MediaFile.h"
#include <fstream>
#include <filesystem>
#include <wil/resource.h>

class MediaFileTest : public ::testing::Test {
protected:
    std::wstring path;
    void SetUp() override {
        wchar_t directory[MAX_PATH]{}, file[MAX_PATH]{};
        ASSERT_NE(GetTempPathW(MAX_PATH, directory), 0u);
        ASSERT_NE(GetTempFileNameW(directory, L"mda", 0, file), 0u);
        path = file;
    }
    void TearDown() override { DeleteFileW(path.c_str()); }
    void Write(const std::string& data) {
        std::ofstream file(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
};

TEST_F(MediaFileTest, ReadsSegmentsAndChecksIdentityOnTheSameHandle) {
    Write("0123456789");
    media::MediaFile file(path);
    EXPECT_EQ(file.Identity().size, 10u);
    EXPECT_EQ(file.Read(3, 3), (std::vector<std::uint8_t>{'3', '4', '5'}));
    EXPECT_FALSE(file.FinalPath().empty());
    EXPECT_THROW(file.Read(9, 2), media::FileError);
    EXPECT_THROW(file.Read(0, 16u * 1024u * 1024u + 1), media::FileError);
    const auto identity = file.Identity();
    Write("changed");
    EXPECT_NE(file.Identity(), identity);
}
TEST_F(MediaFileTest, MissingFilesAndSharingConflictsAreReported) {
    EXPECT_THROW(media::MediaFile(path + L".missing"), media::FileError);
    wil::unique_hfile exclusive(CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr));
    ASSERT_TRUE(exclusive);
    try {
        media::MediaFile file(path);
        FAIL() << "exclusive file opened";
    } catch (const media::FileError& error) {
        EXPECT_EQ(error.code, ERROR_SHARING_VIOLATION);
    }
}
TEST_F(MediaFileTest, SniffsEbmlDocTypeInsteadOfSearchingArbitraryBytes) {
    Write(std::string("\x1a\x45\xdf\xa3\x87\x42\x82\x84", 8) + "webm");
    EXPECT_EQ(media::MediaFile(path).MimeType(), "video/webm");
    Write(std::string("\x1a\x45\xdf\xa3\x87\x42\x83\x84", 8) + "webm");
    EXPECT_EQ(media::MediaFile(path).MimeType(), "application/octet-stream");
}

TEST_F(MediaFileTest, EbmlSniffAcceptsWideSizesAndSkipsUnknownElements) {
    const std::string signature("\x1a\x45\xdf\xa3", 4);
    for (unsigned width = 1; width <= 8; ++width) {
        std::string size(width, '\0');
        size[0] = static_cast<char>(1u << (8 - width));
        size[width - 1] = static_cast<char>(static_cast<unsigned char>(size[width - 1]) | 14);
        Write(signature + size + std::string("\xec\x80\x42\x82\x40\x08", 6) + "matroska");
        EXPECT_EQ(media::MediaFile(path).MimeType(), "video/x-matroska") << width;
    }
}

TEST_F(MediaFileTest, EbmlSniffRejectsEveryTruncatedPrefixAndInvalidVint) {
    const std::string complete = std::string("\x1a\x45\xdf\xa3\x40\x08\x42\x82\x40\x04", 10) + "webm";
    for (std::size_t length = 4; length < complete.size(); ++length) {
        Write(complete.substr(0, length));
        EXPECT_EQ(media::MediaFile(path).MimeType(), "application/octet-stream") << length;
    }
    const std::string signature("\x1a\x45\xdf\xa3", 4);
    for (const auto& suffix : {std::string("\0", 1), std::string("\x82\x42\0", 3),
        std::string("\x85\x08\0\0\0\0", 6), std::string("\x82\xec\x84", 3)}) {
        Write(signature + suffix);
        EXPECT_EQ(media::MediaFile(path).MimeType(), "application/octet-stream");
    }
}

TEST_F(MediaFileTest, EbmlSniffDoesNotFollowDocTypeBeyondTheDeclaredHeaderOrReadBudget) {
    Write(std::string("\x1a\x45\xdf\xa3\x80\x42\x82\x84", 8) + "webm");
    EXPECT_EQ(media::MediaFile(path).MimeType(), "application/octet-stream");
    Write(std::string("\x1a\x45\xdf\xa3\x50\x07\xec\x4f\xfd", 9) + std::string(4093, 'x') +
        std::string("\x42\x82\x84", 3) + "webm");
    EXPECT_EQ(media::MediaFile(path).MimeType(), "application/octet-stream");
}
