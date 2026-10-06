// test_api_schema_common.cpp - src/api/schema/common.ts 生成的共享结构体（CommonSchema.h）。
//
// 共享结构体只出现在返回值里，这里是唯一在 C++ 层编译它的地方。kFields 是声明的键集，直写 JSON 的
// 路径（媒体库查询的 WriteTrackJson 等）改用 Track 时拿它对拍自己写出的键；这里锁住 kFields 与
// ToJson 实际写出的键一致，那些对拍测试以此为基础。
#include "pch.h"
#include "compat/fb2k_types.h"  // console:: stub: the generated headers include ErrorEnvelope.h
#include "api/generated/CommonSchema.h"

#include <set>
#include <string>

using nlohmann::json;
namespace common = api::common;

namespace {

common::Track SampleTrack() {
    common::Track t;
    t.handle = "C:\\Music\\a.flac|subsong:2";
    t.path = "file://C:\\Music\\a.flac";
    t.absolutePath = "C:\\Music\\a.flac";
    t.subsong = 2;
    t.title = "Song";
    t.artist = "A, B";
    t.artists = {"A", "B"};
    t.album = "Album";
    t.albumArtist = "A";
    t.albumArtists = {"A"};
    t.genre = "Rock";
    t.date = "2019";
    t.trackNumber = 3;
    t.discNumber = 1;
    t.duration = 123.5;
    t.fileSize = 4096;
    t.bitrate = 900;
    t.sampleRate = 44100;
    t.channels = 2;
    t.codec = "FLAC";
    t.rating = 4;
    return t;
}

std::set<std::string> KeysOf(const json& j) {
    std::set<std::string> keys;
    for (auto it = j.begin(); it != j.end(); ++it) keys.insert(it.key());
    return keys;
}

}  // namespace

TEST(ApiSchemaCommon, TrackKFieldsIsExactlyTheKeySetToJsonWrites) {
    const json j = ToJson(SampleTrack());
    std::set<std::string> declared(common::Track::kFields.begin(), common::Track::kFields.end());
    EXPECT_EQ(KeysOf(j), declared);
    EXPECT_EQ(common::Track::kFields.size(), 21u);
    EXPECT_EQ(common::Track::kFields.front(), "handle");
}

TEST(ApiSchemaCommon, TrackPartialWritesOnlyTheFieldsItHolds) {
    common::TrackPartial p;
    p.title = "Song";
    p.rating = 0;
    EXPECT_EQ(ToJson(p), (json{{"title", "Song"}, {"rating", 0}}));
    // Its key set is Track's: a projection never invents a field.
    EXPECT_EQ((std::set<std::string>(common::TrackPartial::kFields.begin(), common::TrackPartial::kFields.end())),
              (std::set<std::string>(common::Track::kFields.begin(), common::Track::kFields.end())));
}
