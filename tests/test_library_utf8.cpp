#include "pch.h"
#include "compat/fb2k_types.h"
#include "api/AlbumIdentity.h"
#include "api/AlbumRowText.h"
#include "api/generated/LibrarySchema.h"
#include "utils/StringUtils.h"
#include <iostream>
#include <map>

// 直接验证生成的响应结构与现有序列化器，不模拟 foobar2000 的文件解码器。
// 原始标签进入 AlbumInfo 后，JSON 对象可以构建，直到 dump 才检查 UTF-8。
namespace library_utf8_test {
namespace lb = api::library;
using nlohmann::json;

lb::AlbumInfo ValidAlbum() {
    lb::AlbumInfo row;
    row.name = "中文专辑";
    row.artist = "艺术家";
    row.albumArtist = row.artist;
    row.year = "1993";
    row.genre = "流行";
    row.label = "唱片公司";
    row.trackCount = 1;
    row.discCount = 1;
    row.duration = 1;
    row.firstTrackPath = "file://C:\\Music\\sample.flac";
    return row;
}

json AlbumsBody(const lb::AlbumInfo& row) {
    lb::GetAlbumsResult result;
    result.albums = {ValidAlbum(), row};
    result.total = 2;
    return api::Result<lb::GetAlbumsResult>(result).ToJson();
}

void ExpectInvalidByte(const json& body, bool printError = false) {
    try {
        (void)body.dump();
        FAIL() << "原始非法字节应在序列化边界被拒绝";
    } catch (const json::type_error& error) {
        if (printError) std::cout << "[复现] " << error.what() << '\n';
        EXPECT_EQ(error.id, 316);
        EXPECT_STREQ(error.what(),
            "[json.exception.type_error.316] invalid UTF-8 byte at index 0: 0xB9");
    }
}

TEST(LibraryUtf8, ValidChineseAlbumSurvivesSerialization) {
    const json body = AlbumsBody(ValidAlbum());
    EXPECT_EQ(json::parse(body.dump()), body);
    EXPECT_EQ(body["albums"][0]["name"], "中文专辑");
}

TEST(LibraryUtf8, OneMalformedFieldRejectsTheWholeAlbumResponse) {
    const std::pair<const char*, std::string lb::AlbumInfo::*> fields[] = {
        {"name", &lb::AlbumInfo::name},
        {"artist", &lb::AlbumInfo::artist},
        {"albumArtist", &lb::AlbumInfo::albumArtist},
        {"year", &lb::AlbumInfo::year},
        {"genre", &lb::AlbumInfo::genre},
        {"label", &lb::AlbumInfo::label},
        {"firstTrackPath", &lb::AlbumInfo::firstTrackPath},
    };
    for (const auto& [field, member] : fields) {
        SCOPED_TRACE(field);
        auto row = ValidAlbum();
        row.*member = "\xB9";
        const json body = AlbumsBody(row);
        ASSERT_EQ(body["albums"].size(), 2u);
        ExpectInvalidByte(body, member == &lb::AlbumInfo::name);
    }
}

TEST(LibraryUtf8, NestedAlbumPathsAreAlsoStrictlyValidated) {
    auto row = ValidAlbum();
    row.firstTrackAbsolutePath = "\xB9";
    ExpectInvalidByte(AlbumsBody(row));
    row.firstTrackAbsolutePath.reset();
    lb::AlbumTrackRef track;
    track.path = "\xB9";
    row.tracks = std::vector<lb::AlbumTrackRef>{track};
    ExpectInvalidByte(AlbumsBody(row));
    track.path = "file://C:\\Music\\sample.flac";
    track.absolutePath = "\xB9";
    row.tracks = std::vector<lb::AlbumTrackRef>{track};
    ExpectInvalidByte(AlbumsBody(row));
}

TEST(LibraryUtf8, AlbumRelatedResponsesShareTheSerializationFailure) {
    auto row = ValidAlbum();
    row.genre = "\xB9";
    lb::GetArtistAlbumsResult artistAlbums;
    artistAlbums.albums = {row};
    ExpectInvalidByte(api::Result<lb::GetArtistAlbumsResult>(artistAlbums).ToJson());

    lb::GetAlbumTracksResult albumTracks;
    albumTracks.row = row;
    ExpectInvalidByte(api::Result<lb::GetAlbumTracksResult>(albumTracks).ToJson());

    lb::ArtistAlbumRef ref;
    ref.name = "\xB9";
    ref.artist = "Artist";
    lb::ArtistInfo artist;
    artist.name = "Artist";
    artist.albums = std::vector<lb::ArtistAlbumRef>{ref};
    lb::GetArtistsResult artists;
    artists.items = {artist};
    ExpectInvalidByte(api::Result<lb::GetArtistsResult>(artists).ToJson());

    lb::LibraryValueCount value;
    value.name = "\xB9";
    value.trackCount = 1;
    lb::GetFieldValuesResult values;
    values.values = {value};
    ExpectInvalidByte(api::Result<lb::GetFieldValuesResult>(values).ToJson());
}

TEST(LibraryUtf8, ExistingTextNormalizationMakesNonIdentityFieldsSerializable) {
    const std::string invalid[] = {"\xB9", "\xE6\x92", "\xC0\xAF", "\xED\xA0\x80"};
    for (const auto& bytes : invalid) {
        auto row = ValidAlbum();
        row.genre = bytes;
        EXPECT_THROW(AlbumsBody(row).dump(), json::type_error);
        row.genre = StringUtils::SafeUtf8(row.genre);
        const json body = AlbumsBody(row);
        EXPECT_EQ(json::parse(body.dump()), body);
        EXPECT_EQ(body["albums"][0]["name"], "中文专辑");
    }
}

TEST(LibraryUtf8, NormalizingOnlyTheReturnedNameCannotFindTheRawAlbumKey) {
    const std::string rawName = "\xB9";
    const auto identity = ResolveAlbumIdentity(rawName.c_str(), "Artist", nullptr);
    ASSERT_TRUE(identity.has_value());
    const std::map<std::string, int> index{{AlbumKey(*identity), 1}};
    const std::string displayedName = StringUtils::SafeUtf8(rawName);
    EXPECT_EQ(index.count(AlbumKey(displayedName, "Artist")), 0u);
    EXPECT_EQ(index.count(AlbumKey(rawName, "Artist")), 1u);
}

TEST(LibraryUtf8, ReplacementTextCannotDistinguishDifferentRawAlbumNames) {
    const std::string first = "\xB9";
    const std::string second = "\xBA";
    EXPECT_NE(AlbumKey(first, "Artist"), AlbumKey(second, "Artist"));
    EXPECT_EQ(StringUtils::SafeUtf8(first), StringUtils::SafeUtf8(second));
    EXPECT_EQ(AlbumKey(StringUtils::SafeUtf8(first), "Artist"),
              AlbumKey(StringUtils::SafeUtf8(second), "Artist"));
}

TEST(LibraryUtf8Production, AlbumRowTextKeepsValidChineseUnchanged) {
    const auto raw = ValidAlbum();
    auto row = raw;
    FillAlbumRowText(row, raw);
    EXPECT_EQ(AlbumsBody(row), AlbumsBody(raw));
}

TEST(LibraryUtf8Production, MalformedAlbumTextDoesNotRejectOtherAlbums) {
    const std::pair<const char*, std::string lb::AlbumInfo::*> fields[] = {
        {"name", &lb::AlbumInfo::name},
        {"artist", &lb::AlbumInfo::artist},
        {"albumArtist", &lb::AlbumInfo::albumArtist},
        {"year", &lb::AlbumInfo::year},
        {"genre", &lb::AlbumInfo::genre},
        {"label", &lb::AlbumInfo::label},
        {"firstTrackPath", &lb::AlbumInfo::firstTrackPath},
    };
    for (const auto& [field, member] : fields) {
        SCOPED_TRACE(field);
        auto raw = ValidAlbum();
        raw.*member = "\xB9";
        if (member == &lb::AlbumInfo::artist) raw.albumArtist.clear();
        auto row = raw;
        FillAlbumRowText(row, raw);
        const json body = AlbumsBody(row);
        EXPECT_NO_THROW((void)body.dump());
        EXPECT_EQ(body["albums"][0]["name"], "中文专辑");
        EXPECT_EQ(body["albums"][1][field].get<std::string>(), "\xEF\xBF\xBD");
    }
}

TEST(LibraryUtf8Production, DisplayCollisionsKeepCanonicalIdentityAndOriginalData) {
    std::vector<lb::AlbumInfo> rows;
    for (const char* name : {"\xB9", "\xBA", "\xEF\xBF\xBD"}) {
        auto raw = ValidAlbum();
        raw.name = name;
        raw.albumArtist = "Artist";
        auto row = raw;
        FillAlbumRowText(row, raw);
        EXPECT_EQ(raw.name, name);
        EXPECT_EQ(row.name, "\xEF\xBF\xBD");
        rows.push_back(std::move(row));
    }
    ASSERT_TRUE(rows[0].albumId);
    ASSERT_TRUE(rows[1].albumId);
    EXPECT_NE(rows[0].albumId, rows[1].albumId);
    EXPECT_FALSE(rows[2].albumId);
    EXPECT_EQ(DecodeAlbumId(*rows[0].albumId), AlbumKey("\xB9", "Artist"));
    EXPECT_EQ(DecodeAlbumId(*rows[1].albumId), AlbumKey("\xBA", "Artist"));
}

TEST(LibraryUtf8Production, ArtistAlbumReferenceSharesCanonicalIdentifier) {
    auto raw = ValidAlbum();
    raw.name = "\xB9";
    raw.albumArtist = "\xBA";
    auto row = raw;
    FillAlbumRowText(row, raw);
    lb::ArtistAlbumRef reference;
    FillArtistAlbumRefText(reference, raw.name, raw.albumArtist);
    EXPECT_EQ(reference.albumId, row.albumId);
    EXPECT_EQ(reference.name, row.name);
    EXPECT_EQ(reference.artist, row.albumArtist);
    EXPECT_NO_THROW(api::results::Value(reference).dump());

    FillAlbumRowText(row, raw, false);
    EXPECT_FALSE(row.albumId);
    EXPECT_EQ(row.name, reference.name);
}

TEST(LibraryUtf8Production, AlbumMembersKeepSubsongAndPathShape) {
    lb::AlbumTrackRef first;
    lb::AlbumTrackRef second;
    FillAlbumTrackRef(first, 1, "file://C:\\Music\\album.cue", "C:\\Music\\album.cue", 1);
    FillAlbumTrackRef(second, 2, "file://C:\\Music\\album.cue", "C:\\Music\\album.cue", 2);
    EXPECT_EQ(first.path, second.path);
    EXPECT_EQ(first.absolutePath, second.absolutePath);
    EXPECT_EQ(first.subsong, 1);
    EXPECT_EQ(second.subsong, 2);
    EXPECT_EQ(first.trackNumber, 1);
    FillAlbumTrackRef(second, 2, "\xB9", "\xBA", 2);
    EXPECT_EQ(second.path, "\xEF\xBF\xBD");
    EXPECT_EQ(second.absolutePath, "\xEF\xBF\xBD");
    EXPECT_NO_THROW(api::results::Value(second).dump());
}

TEST(LibraryUtf8Production, AlbumFilterUsesDisplayedText) {
    EXPECT_TRUE(AlbumTextMatchesQuery("\xB9", "Artist", "\xEF\xBF\xBD"));
    EXPECT_TRUE(AlbumTextMatchesQuery("Album", "\xBA", "\xEF\xBF\xBD"));
    EXPECT_TRUE(AlbumTextMatchesQuery("中文专辑", "艺术家", "中文"));
    EXPECT_TRUE(AlbumTextMatchesQuery("Album", "Artist", "ARTIST"));
    EXPECT_FALSE(AlbumTextMatchesQuery("Album", "Artist", "missing"));
}

TEST(LibraryUtf8Production, AlbumLookupKeepsOptionalNullButRejectsExplicitEmptyId) {
    json request{{"album", "Album"}, {"albumArtist", "Artist"}};
    lb::GetAlbumTracksParams params;
    std::string error;
    ASSERT_TRUE(lb::FromJson(request, params, error, "params"));
    EXPECT_EQ(ResolveAlbumLookupKey(params.album, params.albumArtist, params.albumId),
              AlbumKey("Album", "Artist"));

    request["albumId"] = nullptr;
    ASSERT_TRUE(lb::FromJson(request, params, error, "params"));
    EXPECT_FALSE(params.albumId);
    EXPECT_EQ(ResolveAlbumLookupKey(params.album, params.albumArtist, params.albumId),
              AlbumKey("Album", "Artist"));

    request["albumId"] = "";
    ASSERT_TRUE(lb::FromJson(request, params, error, "params"));
    ASSERT_TRUE(params.albumId);
    EXPECT_FALSE(ResolveAlbumLookupKey(params.album, params.albumArtist, params.albumId));
}
}  // namespace library_utf8_test
