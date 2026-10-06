// test_album_identity.cpp — 专辑身份的口径锁
//
// library.getAlbums 的分组、getAlbumTracks 的查找、getStats 的 totalAlbums 与
// getArtists 的 albums 都按 ResolveAlbumIdentity 与 AlbumKey 认专辑。调用方拿
// getAlbums 行里的 name、albumArtist 回来查曲目，所以这里的每条规则都是对外
// 契约：改了任何一条，已经取到的行就查不回自己的曲目。
#include "pch.h"
#include "api/AlbumIdentity.h"
#include <algorithm>

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

}  // namespace album_identity_test
