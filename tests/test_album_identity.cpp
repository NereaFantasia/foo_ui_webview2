// test_album_identity.cpp — 专辑身份的口径锁
//
// library.getAlbums 的分组、getAlbumTracks 的查找、getStats 的 totalAlbums 与
// getArtists 的 albums 都按 ResolveAlbumIdentity 与 AlbumKey 认专辑。调用方用
// 正常的 name、albumArtist，或坏标签对应的 albumId 查曲目；规则变化会影响
// 已经取到的专辑能否查回自己的成员。
#include "pch.h"
#include "api/AlbumIdentity.h"
#include <algorithm>
#include <map>

namespace album_identity_test {

TEST(ResolveAlbumIdentity, MissingAlbumBelongsToNoAlbum) {
    EXPECT_FALSE(ResolveAlbumIdentity(nullptr, "Album Artist", "Artist").has_value());
}

TEST(ResolveAlbumIdentity, EmptyAlbumBelongsToNoAlbum) {
    EXPECT_FALSE(ResolveAlbumIdentity("", "Album Artist", "Artist").has_value());
}

TEST(ResolveAlbumIdentity, AlbumArtistWinsOverArtist) {
    const auto identity = ResolveAlbumIdentity("Album", "Album Artist", "Artist");
    ASSERT_TRUE(identity.has_value());
    EXPECT_STREQ(identity->name, "Album");
    EXPECT_STREQ(identity->albumArtist, "Album Artist");
}

TEST(ResolveAlbumIdentity, ArtistStandsInWhenAlbumArtistIsAbsent) {
    // 没标专辑艺术家的合辑因此按每首的首位艺术家分成几张
    const auto identity = ResolveAlbumIdentity("Album", nullptr, "Artist");
    ASSERT_TRUE(identity.has_value());
    EXPECT_STREQ(identity->albumArtist, "Artist");
}

TEST(ResolveAlbumIdentity, EmptyAlbumArtistDoesNotFallBackToArtist) {
    // 回落只看字段在不在：字段在而值为空串时，专辑艺术家就是空串
    const auto identity = ResolveAlbumIdentity("Album", "", "Artist");
    ASSERT_TRUE(identity.has_value());
    EXPECT_STREQ(identity->albumArtist, "");
}

TEST(ResolveAlbumIdentity, NeitherTagGivesAnEmptyAlbumArtist) {
    const auto identity = ResolveAlbumIdentity("Album", nullptr, nullptr);
    ASSERT_TRUE(identity.has_value());
    ASSERT_NE(identity->albumArtist, nullptr);
    EXPECT_STREQ(identity->albumArtist, "");
}

TEST(AlbumKey, SeparatesNameAndAlbumArtistWithOneNul) {
    EXPECT_EQ(AlbumKey("Album", "Artist"), std::string("Album\0Artist", 12));
    EXPECT_EQ(AlbumKey("Album", ""), std::string("Album\0", 6));
}

TEST(AlbumKey, IdentityOverloadMatchesTheTwoPartForm) {
    const auto identity = ResolveAlbumIdentity("Album", nullptr, "Artist");
    ASSERT_TRUE(identity.has_value());
    EXPECT_EQ(AlbumKey(*identity), AlbumKey("Album", "Artist"));
}

TEST(AlbumKey, SplittingTheSameBytesDifferentlyGivesDifferentKeys) {
    // 取自标签的两段不含 NUL，拆法不同的两对值不会撞键
    EXPECT_NE(AlbumKey("AB", "C"), AlbumKey("A", "BC"));
}

TEST(AlbumKey, ValuesWithNulNeverEqualAKeyBuiltFromTags) {
    // 调用方传来的值含 NUL 时键里至少有两个 NUL，不会命中任何专辑
    const std::string fromCaller = AlbumKey("Album", std::string_view("Artist\0X", 8));
    EXPECT_NE(fromCaller, AlbumKey("Album", "Artist"));
    EXPECT_EQ(std::count(fromCaller.begin(), fromCaller.end(), '\0'), 2);
}

TEST(AlbumId, ValidNamesDoNotNeedAnIdentifier) {
    EXPECT_FALSE(AlbumId("中文专辑", "艺术家"));
    EXPECT_FALSE(AlbumId("\xEF\xBF\xBD", "Artist"));
    EXPECT_FALSE(AlbumId("", "\xB9"));
}

TEST(AlbumId, InvalidRawIdentitiesRemainDistinctAndReversible) {
    const auto first = AlbumId("\xB9", "Artist");
    const auto second = AlbumId("\xBA", "Artist");
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(*first, "album1:b900417274697374");
    EXPECT_NE(*first, *second);
    EXPECT_EQ(DecodeAlbumId(*first), AlbumKey("\xB9", "Artist"));
    EXPECT_EQ(DecodeAlbumId(*second), AlbumKey("\xBA", "Artist"));
    EXPECT_EQ(DecodeAlbumId(*AlbumId("Album", "\xB9")), AlbumKey("Album", "\xB9"));
}

TEST(AlbumId, DecoderRejectsMalformedRepresentations) {
    for (const auto* value : {"", "album1:", "album2:b900", "Album1:b900",
                              "album1:b90", "album1:B900", "album1:bx00",
                              "album1:00", "album1:0041", "album1:41",
                              "album1:410000", "album1:4100 ", "album1:410a"}) {
        EXPECT_FALSE(DecodeAlbumId(value)) << value;
    }
    EXPECT_EQ(DecodeAlbumId("album1:4100"), AlbumKey("A", ""));
}

TEST(AlbumId, OnlyMissingIdentifierUsesTheNameLookup) {
    EXPECT_EQ(ResolveAlbumLookupKey("Album", "Artist", std::nullopt),
              AlbumKey("Album", "Artist"));
    EXPECT_FALSE(ResolveAlbumLookupKey("Album", "Artist", std::string{}));
    EXPECT_FALSE(ResolveAlbumLookupKey("Album", "Artist", "bad-id"));
    EXPECT_EQ(ResolveAlbumLookupKey("", "wrong", "album1:b900"), AlbumKey("\xB9", ""));
    const std::map<std::string, int> index{{AlbumKey("Album", "Artist"), 1}};
    const auto missing = ResolveAlbumLookupKey("Album", "Artist", "album1:4100");
    ASSERT_TRUE(missing);
    EXPECT_EQ(index.count(*missing), 0u);
}

struct FirstValueInfo {
    std::map<std::string, std::vector<std::string>> tags;

    const char* meta_get(const char* name, size_t index) const {
        const auto found = tags.find(name);
        if (found == tags.end() || index >= found->second.size()) return nullptr;
        return found->second[index].c_str();
    }
};

TEST(AlbumId, ProductionInfoAdapterUsesOnlyTheFirstIdentityValues) {
    FirstValueInfo info{{{"album", {"\xB9", "second album"}},
                         {"album artist", {"First", "\xBA"}},
                         {"artist", {"ignored", "second artist"}}}};
    EXPECT_EQ(AlbumIdOf(info), AlbumId("\xB9", "First"));
    info.tags["album"] = {"valid", "\xB9"};
    EXPECT_FALSE(AlbumIdOf(info));
}

TEST(AlbumId, ProductionInfoAdapterDistinguishesMissingAndEmptyAlbumArtist) {
    FirstValueInfo info{{{"album", {"\xB9"}}, {"artist", {"First", "Second"}}}};
    EXPECT_EQ(AlbumIdOf(info), AlbumId("\xB9", "First"));
    info.tags["album artist"] = {""};
    EXPECT_EQ(AlbumIdOf(info), AlbumId("\xB9", ""));
    info.tags.erase("artist");
    info.tags.erase("album artist");
    EXPECT_EQ(AlbumIdOf(info), AlbumId("\xB9", ""));
    info.tags.erase("album");
    EXPECT_FALSE(AlbumIdOf(info));
}

}  // namespace album_identity_test
