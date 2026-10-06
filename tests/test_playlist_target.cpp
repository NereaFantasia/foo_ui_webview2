// test_playlist_target.cpp - api::ResolvePlaylistTarget：按序号或 GUID 指定目标播放列表。
//
// 直接测产品头 src/api/PlaylistTarget.h，列表清单用 MockPlaylistService 模拟：
// 重排、删除都是直接改 mock.playlists，GUID 跟着列表条目走。
#include "pch.h"
#include "mocks/MockPlaylistService.h"
#include "../src/api/PlaylistTarget.h"
#include <cctype>

namespace {

// 各列表各用一个不同的 GUID，Data1 取 id 便于读断言。
GUID MakeGuid(unsigned long id) {
    GUID g{};
    g.Data1 = id;
    g.Data2 = 0x1111;
    g.Data3 = 0x2222;
    for (int i = 0; i < 8; i++) g.Data4[i] = static_cast<unsigned char>(0x30 + i);
    return g;
}

class PlaylistTargetTest : public ::testing::Test {
protected:
    MockPlaylistService mock;

    void Add(const std::string& name, unsigned long id) {
        MockPlaylistService::MockPlaylist pl;
        pl.name = name;
        pl.guid = MakeGuid(id);
        mock.playlists.push_back(pl);
    }

    std::string GuidText(unsigned long id) const { return GuidUtils::GuidToString(MakeGuid(id)); }

    void SetUp() override {
        Add("Rock", 1);
        Add("Jazz", 2);
        Add("Rock", 3);  // 与 0 号同名
        mock.activePlaylist = 1;
    }
};

TEST_F(PlaylistTargetTest, GuidResolvesToTheCurrentIndexOfThatPlaylist) {
    size_t index = SIZE_MAX;
    auto failure = api::ResolvePlaylistTarget(mock, std::nullopt, GuidText(3), index);
    ASSERT_FALSE(failure.has_value());
    EXPECT_EQ(index, 2u);
}

// 同名列表之间互换位置：按名字加排位认会认错，GUID 跟着列表走。
TEST_F(PlaylistTargetTest, GuidFollowsThePlaylistWhenSameNamedPlaylistsSwap) {
    std::swap(mock.playlists[0], mock.playlists[2]);
    size_t index = SIZE_MAX;
    ASSERT_FALSE(api::ResolvePlaylistTarget(mock, std::nullopt, GuidText(3), index).has_value());
    EXPECT_EQ(index, 0u);
    ASSERT_FALSE(api::ResolvePlaylistTarget(mock, std::nullopt, GuidText(1), index).has_value());
    EXPECT_EQ(index, 2u);
}

TEST_F(PlaylistTargetTest, GuidFollowsThePlaylistWhenAnotherIsRemovedBeforeIt) {
    mock.playlists.erase(mock.playlists.begin());
    size_t index = SIZE_MAX;
    ASSERT_FALSE(api::ResolvePlaylistTarget(mock, std::nullopt, GuidText(2), index).has_value());
    EXPECT_EQ(index, 0u);
}

// 目标被删的同时另一张改成了同名：不能退回同名列表、原序号或活动列表。
TEST_F(PlaylistTargetTest, RemovedTargetFailsWithNotFoundAndNeverFallsBack) {
    mock.playlists.erase(mock.playlists.begin() + 2);
    mock.playlists[1].name = "Rock";
    size_t index = 7;
    auto failure = api::ResolvePlaylistTarget(mock, std::nullopt, GuidText(3), index);
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure->code, ApiErrorCode::NOT_FOUND);
    EXPECT_EQ(failure->extra.at("details").at("playlistGuid"), GuidText(3));
}

TEST_F(PlaylistTargetTest, LowercaseGuidIsAccepted) {
    std::string lower = GuidText(2);
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    size_t index = SIZE_MAX;
    ASSERT_FALSE(api::ResolvePlaylistTarget(mock, std::nullopt, lower, index).has_value());
    EXPECT_EQ(index, 1u);
}

TEST_F(PlaylistTargetTest, BothIndexAndGuidFailWithInvalidParams) {
    size_t index = SIZE_MAX;
    auto failure = api::ResolvePlaylistTarget(mock, std::int64_t{0}, GuidText(1), index);
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure->code, ApiErrorCode::INVALID_PARAMS);
}

TEST_F(PlaylistTargetTest, MalformedGuidFailsWithInvalidParams) {
    for (const char* text : {"", "Rock", "12345678-1111-2222-3031-323334353637",
                             "{12345678-1111-2222-3031-323334353637}x"}) {
        size_t index = SIZE_MAX;
        auto failure = api::ResolvePlaylistTarget(mock, std::nullopt, std::string(text), index);
        ASSERT_TRUE(failure.has_value()) << text;
        EXPECT_EQ(failure->code, ApiErrorCode::INVALID_PARAMS) << text;
        EXPECT_EQ(failure->extra.at("details").at("playlistGuid"), text);
    }
}

TEST_F(PlaylistTargetTest, IndexKeepsItsFailureCodes) {
    size_t index = SIZE_MAX;
    ASSERT_FALSE(api::ResolvePlaylistTarget(mock, std::int64_t{2}, std::nullopt, index).has_value());
    EXPECT_EQ(index, 2u);

    auto failure = api::ResolvePlaylistTarget(mock, std::int64_t{3}, std::nullopt, index,
                                              api::PlaylistOmitted::UseActive, "Invalid playlist");
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure->code, ApiErrorCode::INVALID_INDEX);
    EXPECT_EQ(failure->error, "Invalid playlist");
}

TEST_F(PlaylistTargetTest, OmittedUsesTheActivePlaylistOrRefuses) {
    size_t index = SIZE_MAX;
    ASSERT_FALSE(api::ResolvePlaylistTarget(mock, std::nullopt, std::nullopt, index).has_value());
    EXPECT_EQ(index, 1u);

    auto refused = api::ResolvePlaylistTarget(mock, std::nullopt, std::nullopt, index,
                                              api::PlaylistOmitted::Refuse);
    ASSERT_TRUE(refused.has_value());
    EXPECT_EQ(refused->code, ApiErrorCode::INVALID_PARAMS);

    mock.activePlaylist = SIZE_MAX;
    auto none = api::ResolvePlaylistTarget(mock, std::nullopt, std::nullopt, index);
    ASSERT_TRUE(none.has_value());
    EXPECT_EQ(none->code, ApiErrorCode::NO_ACTIVE_ITEM);
}

// 路径解析期间列表被拖动：按解析前的序号钉住，写入前找回同一张列表的新序号。
TEST_F(PlaylistTargetTest, PinnedPlaylistFollowsTheTargetWhenPlaylistsMove) {
    const api::PinnedPlaylist pin(mock, 2);
    std::swap(mock.playlists[0], mock.playlists[2]);
    EXPECT_EQ(pin.Locate(mock), std::optional<size_t>(0u));

    mock.playlists.erase(mock.playlists.begin() + 1);
    EXPECT_EQ(pin.Locate(mock), std::optional<size_t>(0u));
}

// 目标被删掉、原序号上换成了另一张列表：不能把曲目写进那张列表。
TEST_F(PlaylistTargetTest, PinnedPlaylistReportsARemovedTargetInsteadOfReusingItsIndex) {
    const api::PinnedPlaylist pin(mock, 1);
    mock.playlists.erase(mock.playlists.begin() + 1);
    ASSERT_EQ(mock.playlists.size(), 2u);
    EXPECT_EQ(pin.Locate(mock), std::nullopt);
}

// 结果里的 guid 原样作为 playlistGuid 传回，要能找回同一个列表。
TEST_F(PlaylistTargetTest, ReportedGuidRoundTripsAsPlaylistGuid) {
    const std::string reported = api::PlaylistGuidOf(mock, 2);
    EXPECT_EQ(reported, GuidText(3));
    size_t index = SIZE_MAX;
    ASSERT_FALSE(api::ResolvePlaylistTarget(mock, std::nullopt, reported, index).has_value());
    EXPECT_EQ(index, 2u);
}

// 页面按读到的清单 [1, 2, 3] 排出 [3, 1, 2]，调用前 0 号与 2 号已互换：GUID 仍排出页面要的顺序，
// 换成序号是它们现在的位置。
TEST_F(PlaylistTargetTest, GuidOrderUsesTheCurrentIndexOfEachPlaylist) {
    std::swap(mock.playlists[0], mock.playlists[2]);  // 现在是 [3, 2, 1]
    std::vector<size_t> order;
    auto failure = api::ResolvePlaylistGuidOrder(mock, {GuidText(3), GuidText(1), GuidText(2)}, order);
    ASSERT_FALSE(failure.has_value());
    EXPECT_EQ(order, (std::vector<size_t>{0, 2, 1}));
}

TEST_F(PlaylistTargetTest, GuidOrderOfTheWrongLengthFailsWithInvalidParams) {
    std::vector<size_t> order;
    auto failure = api::ResolvePlaylistGuidOrder(mock, {GuidText(1), GuidText(2)}, order);
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure->code, ApiErrorCode::INVALID_PARAMS);
    EXPECT_EQ(failure->extra.at("expected"), 3);
    EXPECT_EQ(failure->extra.at("got"), 2);
}

TEST_F(PlaylistTargetTest, GuidOrderNamingAPlaylistTwiceFailsWithInvalidParams) {
    std::vector<size_t> order;
    auto failure = api::ResolvePlaylistGuidOrder(mock, {GuidText(1), GuidText(2), GuidText(1)}, order);
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure->code, ApiErrorCode::INVALID_PARAMS);
    EXPECT_EQ(failure->extra.at("details").at("index"), 2);
}

// 删一张又加一张，个数不变：被删的那张的 GUID 报 NOT_FOUND，不会把新列表排进它的位置。
TEST_F(PlaylistTargetTest, GuidOrderWithARemovedPlaylistFailsWithNotFound) {
    mock.playlists.erase(mock.playlists.begin() + 1);
    Add("Blues", 4);
    std::vector<size_t> order;
    auto failure = api::ResolvePlaylistGuidOrder(mock, {GuidText(1), GuidText(2), GuidText(3)}, order);
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure->code, ApiErrorCode::NOT_FOUND);
    EXPECT_EQ(failure->extra.at("details").at("index"), 1);
    EXPECT_EQ(failure->extra.at("details").at("playlistGuid"), GuidText(2));
}

TEST_F(PlaylistTargetTest, GuidOrderWithAMalformedGuidFailsWithInvalidParams) {
    std::vector<size_t> order;
    auto failure = api::ResolvePlaylistGuidOrder(mock, {GuidText(1), "not-a-guid", GuidText(3)}, order);
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure->code, ApiErrorCode::INVALID_PARAMS);
    EXPECT_EQ(failure->extra.at("details").at("index"), 1);
}

}  // namespace
