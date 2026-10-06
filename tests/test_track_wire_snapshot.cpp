// test_track_wire_snapshot.cpp — 曲目直写序列化的纯函数层对拍
//
// WriteTrackJson 是 library.query / library.search 热路径唯一产出 tracks 元素的
// 地方，字段名或取值类型写错就是静默的契约破口。判据：json::parse(writer 产物) ==
// 用同名同值字段手搭的 nlohmann 对象，全字段的键集另与声明的 LibraryTrack（生成的
// kFields）核对。键序不比（nlohmann 对象按键名排序输出，直写按表序），键**集**与每个
// 键的值必须全等。
#include "pch.h"
#include "compat/fb2k_types.h"  // console:: stub：ErrorEnvelope.h 的 FailureHook 引用它
#include "api/TrackWireSnapshot.h"
#include "api/generated/LibrarySchema.h"

#include <initializer_list>
#include <set>
#include <iterator>
#include <limits>

using json = nlohmann::json;

namespace track_wire_test {

// 一条字段齐全、值都不取默认的快照：任何字段漏写/串位都会在对拍里暴露
TrackWireSnapshot MakeFullSnapshot() {
    TrackWireSnapshot snap;
    snap.index = 42;
    snap.handle = "C:\\music\\a.flac|subsong:3";
    snap.title = "Track Title";
    // 多值曲目：artist 恒等于 artists 按 ", " 拼接的结果（对外契约），取值侧由
    // MetaValuesRaw + JoinMetaValues 保证，此处的夹具按同一口径构造
    snap.artist = "Artist Name, Second Artist";
    snap.artists = {"Artist Name", "Second Artist"};
    snap.album = "Album Name";
    // albumArtists 与 albumArtist 同一口径；第一个值自身含 ", "，拼接串拆不回它
    snap.albumArtist = "Crosby, Stills, Nash & Young, Neil Young";
    snap.albumArtists = {"Crosby, Stills, Nash & Young", "Neil Young"};
    snap.genre = "Post-Rock";
    snap.date = "2019-04-05";
    snap.trackNumber = 7;
    snap.discNumber = 2;
    snap.duration = 253.093;
    snap.path = "file://C:\\music\\a.flac";
    snap.absolutePath = "C:\\music\\a.flac";
    snap.fileSize = 41231234;
    snap.bitrate = 1411;
    snap.sampleRate = 44100;
    snap.channels = 2;
    snap.codec = "FLAC";
    snap.subsong = 3;
    snap.rating = 5;
    return snap;
}

// 声明的 LibraryTrack 行的等价对象
json ExpectedFullDom(const TrackWireSnapshot& snap) {
    return {
        {"index", snap.index},
        {"handle", snap.handle},
        {"title", snap.title},
        {"artist", snap.artist},
        {"artists", snap.artists},
        {"album", snap.album},
        {"albumArtist", snap.albumArtist},
        {"albumArtists", snap.albumArtists},
        {"genre", snap.genre},
        {"date", snap.date},
        {"trackNumber", snap.trackNumber},
        {"discNumber", snap.discNumber},
        {"duration", snap.duration},
        {"path", snap.path},
        {"absolutePath", snap.absolutePath},
        {"fileSize", snap.fileSize},
        {"bitrate", snap.bitrate},
        {"sampleRate", snap.sampleRate},
        {"channels", snap.channels},
        {"codec", snap.codec},
        {"subsong", snap.subsong},
        {"rating", snap.rating},
    };
}

json WriteAndParse(const TrackWireSnapshot& snap) {
    std::string out;
    WriteTrackJson(out, snap);
    return json::parse(out);
}

std::string WriteProjected(const TrackWireSnapshot& snap, uint32_t mask) {
    std::string out;
    WriteTrackJsonProjected(out, snap, mask);
    return out;
}

json WriteProjectedAndParse(const TrackWireSnapshot& snap, uint32_t mask) {
    return json::parse(WriteProjected(snap, mask));
}

// 按字段名拼掩码：用例里写名字而不写位，与调用方传 fields 的形态一致
uint32_t MaskOf(std::initializer_list<const char*> names) {
    uint32_t mask = 0;
    for (const char* name : names) {
        mask |= TrackField::Lookup(name);
    }
    return mask;
}

// 解析形状已由声明的读取器查过的 fields（字符串数组），这里只剩名字的判定
TrackFieldSelection SelectFields(std::vector<std::string> names) {
    return ParseTrackFieldSelection(std::optional<std::vector<std::string>>(std::move(names)));
}

}  // namespace track_wire_test

// ============================================
// 全字段模式
// ============================================

TEST(TrackWireSnapshot, FullFieldSetMatchesDomSemantics) {
    const auto snap = track_wire_test::MakeFullSnapshot();

    json parsed;
    ASSERT_NO_THROW(parsed = track_wire_test::WriteAndParse(snap));
    EXPECT_EQ(parsed, track_wire_test::ExpectedFullDom(snap));
}

TEST(TrackWireSnapshot, FullFieldSetHasExactlyTwentyTwoKeys) {
    const auto snap = track_wire_test::MakeFullSnapshot();
    const json parsed = track_wire_test::WriteAndParse(snap);

    ASSERT_TRUE(parsed.is_object());
    EXPECT_EQ(parsed.size(), 22u);

    // 键名逐个钉住：拼错一个字符 = 主题侧字段凭空消失
    for (const char* key : {"index", "handle", "title", "artist", "artists", "album",
                            "albumArtist", "albumArtists", "genre", "date", "trackNumber",
                            "discNumber", "duration", "path", "absolutePath", "fileSize",
                            "bitrate", "sampleRate", "channels", "codec", "subsong", "rating"}) {
        EXPECT_TRUE(parsed.contains(key)) << "missing key: " << key;
    }

    // artists 是数组形态，且 join(", ") 还原成 artist
    ASSERT_TRUE(parsed["artists"].is_array());
    EXPECT_EQ(parsed["artists"].get<std::vector<std::string>>(), snap.artists);
    EXPECT_EQ(parsed["artist"].get<std::string>(), "Artist Name, Second Artist");

    // albumArtists 保住每个值的边界：第一个值含 ", " 也原样是第一个元素
    ASSERT_TRUE(parsed["albumArtists"].is_array());
    EXPECT_EQ(parsed["albumArtists"].get<std::vector<std::string>>(), snap.albumArtists);
    EXPECT_EQ(parsed["albumArtists"][0], "Crosby, Stills, Nash & Young");
}

TEST(TrackWireSnapshot, FullFieldSetEqualsDeclaredLibraryTrackKeys) {
    // 直写器绕过生成的 ToJson，键集只能靠这里对着声明核对：声明加了键而直写器没跟上
    // （或反之）在这里断。
    const json parsed = track_wire_test::WriteAndParse(track_wire_test::MakeFullSnapshot());
    std::set<std::string> written;
    for (const auto& [key, value] : parsed.items()) written.insert(key);
    std::set<std::string> declared;
    for (std::string_view key : api::library::LibraryTrack::kFields) declared.emplace(key);
    EXPECT_EQ(written, declared);
}

TEST(TrackWireSnapshot, DefaultSnapshotMatchesDomSemantics) {
    // 全默认值（空串 + 0）也要能对拍：默认值分支同样进 wire
    const TrackWireSnapshot snap;

    json parsed;
    ASSERT_NO_THROW(parsed = track_wire_test::WriteAndParse(snap));
    EXPECT_EQ(parsed, track_wire_test::ExpectedFullDom(snap));
    EXPECT_EQ(parsed["duration"], json(0.0));
    EXPECT_TRUE(parsed["duration"].is_number_float());  // 整数值也保持浮点形态
}

// ============================================
// info 容器无效的行
// ============================================

TEST(TrackWireSnapshot, RowWithoutInfoEmitsEveryKeyWithInfoFieldsDefaulted) {
    // 取不到 info 的行照样出全部键：取自句柄的字段是真值，取自 info 的归零
    auto snap = track_wire_test::MakeFullSnapshot();
    ResetInfoFields(snap);
    snap.rating = 0;

    const json parsed = track_wire_test::WriteAndParse(snap);
    ASSERT_TRUE(parsed.is_object());
    EXPECT_EQ(parsed.size(), 22u);
    EXPECT_EQ(parsed, track_wire_test::ExpectedFullDom(snap));

    EXPECT_EQ(parsed["index"], 42);
    EXPECT_EQ(parsed["handle"], "C:\\music\\a.flac|subsong:3");
    EXPECT_EQ(parsed["path"], "file://C:\\music\\a.flac");
    EXPECT_EQ(parsed["absolutePath"], "C:\\music\\a.flac");
    EXPECT_EQ(parsed["fileSize"], 41231234);
    EXPECT_EQ(parsed["subsong"], 3);
    EXPECT_EQ(parsed["artists"], json::array());
    EXPECT_EQ(parsed["albumArtists"], json::array());
    for (const char* key : {"title", "artist", "album", "albumArtist", "genre", "date", "codec"}) {
        EXPECT_EQ(parsed[key], "") << "not defaulted: " << key;
    }
    for (const char* key : {"trackNumber", "discNumber", "bitrate", "sampleRate", "channels",
                            "rating"}) {
        EXPECT_EQ(parsed[key], 0) << "not defaulted: " << key;
    }
    EXPECT_EQ(parsed["duration"], json(0.0));
}

TEST(TrackWireSnapshot, RowWithoutInfoKeepsHandleAndPaths) {
    // 实际调用方在此分支只填取自句柄的几项，其余留默认
    TrackWireSnapshot snap;
    snap.index = 9;
    snap.handle = "D:\\音乐\\损坏.flac";
    snap.path = "file://D:\\音乐\\损坏.flac";
    snap.absolutePath = "D:\\音乐\\损坏.flac";

    const json parsed = track_wire_test::WriteAndParse(snap);
    EXPECT_EQ(parsed["index"], 9);
    EXPECT_EQ(parsed["handle"], "D:\\音乐\\损坏.flac");
    EXPECT_EQ(parsed["path"], "file://D:\\音乐\\损坏.flac");
    EXPECT_EQ(parsed["absolutePath"], "D:\\音乐\\损坏.flac");
    EXPECT_EQ(parsed["title"], "");
    EXPECT_EQ(parsed["duration"], json(0.0));
    EXPECT_EQ(parsed["rating"], 0);
}

// ============================================
// 字符串面：中文 / 转义 / 控制字符 / 代理对
// ============================================

TEST(TrackWireSnapshot, ChineseAndEscapedFieldValues) {
    auto snap = track_wire_test::MakeFullSnapshot();
    snap.title = "无损 \"测试\" 曲目\t第一首";
    snap.artist = "反斜杠\\艺人, 引号\"艺人";
    snap.artists = {"反斜杠\\艺人", "引号\"艺人"};  // 数组元素与单串字段同一套转义
    snap.album = "专辑\n换行";
    snap.albumArtist = "群星";
    snap.genre = "古典";
    snap.date = "2024年";
    snap.path = "file://E:\\音乐\\专辑\\曲目.flac";
    snap.absolutePath = "E:\\音乐\\专辑\\曲目.flac";
    snap.codec = "FLAC \xF0\x9F\x8E\xB5";  // 四字节 UTF-8（UTF-16 需代理对）

    json parsed;
    ASSERT_NO_THROW(parsed = track_wire_test::WriteAndParse(snap));
    EXPECT_EQ(parsed, track_wire_test::ExpectedFullDom(snap));
    EXPECT_EQ(parsed["title"].get<std::string>(), snap.title);
    EXPECT_EQ(parsed["codec"].get<std::string>(), snap.codec);
}

TEST(TrackWireSnapshot, ControlCharactersInTagValues) {
    // 标签里混入控制字符时必须转义掉，否则产出的是页面侧解析失败的坏 JSON
    auto snap = track_wire_test::MakeFullSnapshot();
    snap.title = std::string("a\x01"
                             "b\x1f"
                             "c");
    snap.genre = std::string("g\x00h", 3);  // 含嵌入 NUL

    const std::string raw = [&] {
        std::string out;
        WriteTrackJson(out, snap);
        return out;
    }();
    EXPECT_NE(raw.find("\\u0001"), std::string::npos);
    EXPECT_NE(raw.find("\\u001f"), std::string::npos);
    EXPECT_NE(raw.find("\\u0000"), std::string::npos);

    json parsed;
    ASSERT_NO_THROW(parsed = json::parse(raw));
    EXPECT_EQ(parsed, track_wire_test::ExpectedFullDom(snap));
}

TEST(TrackWireSnapshot, VeryLongTagValue) {
    auto snap = track_wire_test::MakeFullSnapshot();
    std::string longTitle;
    longTitle.reserve(200000);
    while (longTitle.size() < 200000) {
        longTitle.append("很长的标题-");
    }
    snap.title = longTitle;

    json parsed;
    ASSERT_NO_THROW(parsed = track_wire_test::WriteAndParse(snap));
    EXPECT_EQ(parsed["title"].get<std::string>(), longTitle);
    EXPECT_EQ(parsed, track_wire_test::ExpectedFullDom(snap));
}

// ============================================
// 数值面：duration / fileSize / 极值
// ============================================

TEST(TrackWireSnapshot, DurationTypicalAndBoundaryValues) {
    for (double duration : {0.0, 0.001, 1.5, 253.093, 3600.0, 86399.999,
                            std::numeric_limits<double>::min(),
                            std::numeric_limits<double>::max()}) {
        auto snap = track_wire_test::MakeFullSnapshot();
        snap.duration = duration;

        json parsed;
        ASSERT_NO_THROW(parsed = track_wire_test::WriteAndParse(snap));
        EXPECT_EQ(parsed["duration"].get<double>(), duration);
        EXPECT_EQ(parsed, track_wire_test::ExpectedFullDom(snap));
    }
}

TEST(TrackWireSnapshot, DurationNonFiniteBecomesNull) {
    // 损坏文件可能报出非有限时长。DOM 里 NaN 原样存着，只在 dump 时才落 null，所以
    // 期望值要显式写 nullptr —— 直写产物与「DOM 走完 dump 再 parse」的结果对齐。
    for (double duration : {std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::infinity(),
                            -std::numeric_limits<double>::infinity()}) {
        auto snap = track_wire_test::MakeFullSnapshot();
        snap.duration = duration;

        json parsed;
        ASSERT_NO_THROW(parsed = track_wire_test::WriteAndParse(snap));
        EXPECT_TRUE(parsed["duration"].is_null());

        json expected = track_wire_test::ExpectedFullDom(snap);
        EXPECT_EQ(json::parse(expected.dump()), parsed);  // dump 侧同样落 null
        expected["duration"] = nullptr;
        EXPECT_EQ(parsed, expected);
    }
}

TEST(TrackWireSnapshot, IntegerFieldExtremes) {
    auto snap = track_wire_test::MakeFullSnapshot();
    snap.index = 165305;                                    // 十六万曲库的末位下标
    snap.fileSize = 9223372036854775807LL;                  // int64 上界
    snap.trackNumber = std::numeric_limits<int>::max();
    snap.discNumber = std::numeric_limits<int>::min();
    snap.bitrate = 0;
    snap.sampleRate = 384000;
    snap.channels = 8;
    snap.subsong = 4294967295u;                             // uint32 上界
    snap.rating = 5;

    json parsed;
    ASSERT_NO_THROW(parsed = track_wire_test::WriteAndParse(snap));
    EXPECT_EQ(parsed, track_wire_test::ExpectedFullDom(snap));
    EXPECT_EQ(parsed["fileSize"].get<int64_t>(), snap.fileSize);
    EXPECT_EQ(parsed["subsong"].get<uint32_t>(), snap.subsong);
    EXPECT_EQ(parsed["discNumber"].get<int>(), snap.discNumber);
}

TEST(TrackWireSnapshot, NegativeIntegerFieldsRoundTrip) {
    // trackNumber 等来自 atoi，标签写成负数时会原样带上
    auto snap = track_wire_test::MakeFullSnapshot();
    snap.trackNumber = -3;
    snap.discNumber = -1;
    snap.fileSize = -1;

    const json parsed = track_wire_test::WriteAndParse(snap);
    EXPECT_EQ(parsed["trackNumber"].get<int>(), -3);
    EXPECT_EQ(parsed["discNumber"].get<int>(), -1);
    EXPECT_EQ(parsed["fileSize"].get<int64_t>(), -1);
    EXPECT_EQ(parsed, track_wire_test::ExpectedFullDom(snap));
}

// ============================================
// 追加语义与数组拼装
// ============================================

TEST(TrackWireSnapshot, WriteAppendsWithoutClearingBuffer) {
    std::string out = "[";
    WriteTrackJson(out, track_wire_test::MakeFullSnapshot());
    out.push_back(']');

    json parsed;
    ASSERT_NO_THROW(parsed = json::parse(out));
    ASSERT_TRUE(parsed.is_array());
    ASSERT_EQ(parsed.size(), 1u);
    EXPECT_EQ(parsed[0], track_wire_test::ExpectedFullDom(track_wire_test::MakeFullSnapshot()));
}

TEST(TrackWireSnapshot, ArrayOfMixedRowsMatchesDom) {
    // 热路径的实际形态：正常行与取不到 info 的行混在一个数组里（handle 无效的行由
    // 序列化侧跳过，不进数组）
    TrackWireSnapshot full = track_wire_test::MakeFullSnapshot();
    full.index = 0;

    TrackWireSnapshot broken;
    broken.index = 1;
    broken.handle = "C:\\music\\损坏.mp3";
    broken.path = "file://C:\\music\\损坏.mp3";
    broken.absolutePath = "C:\\music\\损坏.mp3";

    std::string out = "[";
    WriteTrackJson(out, full);
    out += ",";
    WriteTrackJson(out, broken);
    out.push_back(']');

    json parsed;
    ASSERT_NO_THROW(parsed = json::parse(out));
    ASSERT_EQ(parsed.size(), 2u);
    EXPECT_EQ(parsed[0], track_wire_test::ExpectedFullDom(full));
    EXPECT_EQ(parsed[1], track_wire_test::ExpectedFullDom(broken));
    EXPECT_EQ(parsed[1].size(), 22u);
}

TEST(TrackWireSnapshot, SnapshotReuseAcrossRowsLeavesNoResidue) {
    // 序列化侧循环外复用同一个 snapshot 对象（省分配），取不到 info 的行不得把上一行的
    // 标签值带出来
    TrackWireSnapshot snap = track_wire_test::MakeFullSnapshot();

    std::string first;
    WriteTrackJson(first, snap);

    ResetInfoFields(snap);
    snap.rating = 0;
    std::string second;
    WriteTrackJson(second, snap);
    EXPECT_EQ(json::parse(second).size(), 22u);
    EXPECT_EQ(json::parse(second)["title"], "");
    EXPECT_EQ(json::parse(second)["genre"], "");
    EXPECT_EQ(json::parse(second)["albumArtists"], json::array());

    snap = track_wire_test::MakeFullSnapshot();
    std::string third;
    WriteTrackJson(third, snap);
    EXPECT_EQ(third, first);
}

// ============================================
// fields 白名单与掩码解析（spec §3.1 校验表）
// ============================================

TEST(TrackFieldSelection, WhitelistEqualsFullFieldOutputKeys) {
    // 白名单 = 全字段输出的键集。两处各写一份必然漂移，此处对拍钉住：新增字段时
    // 只改了表没改 writer（或反之）会在这里断。
    const json full = track_wire_test::WriteAndParse(track_wire_test::MakeFullSnapshot());
    ASSERT_EQ(full.size(), std::size(TrackField::kTable));
    for (const auto& entry : TrackField::kTable) {
        EXPECT_TRUE(full.contains(entry.name)) << "whitelist name not in output: " << entry.name;
    }
    EXPECT_EQ(TrackField::Count(TrackField::kAll), std::size(TrackField::kTable));
}

TEST(TrackFieldSelection, EachWhitelistNameMapsToDistinctBit) {
    uint32_t seen = 0;
    for (const auto& entry : TrackField::kTable) {
        const uint32_t bit = TrackField::Lookup(entry.name);
        EXPECT_EQ(bit, entry.bit);
        EXPECT_NE(bit, 0u) << "name not resolvable: " << entry.name;
        EXPECT_EQ(seen & bit, 0u) << "bit reused by: " << entry.name;
        seen |= bit;
    }
    EXPECT_EQ(seen, TrackField::kAll);
}

TEST(TrackFieldSelection, DeclaredFieldsNulloptKeepsFullFieldBehaviour) {
    // 按声明读入的重载：省略与 null 都是 nullopt，等于要全部键
    const TrackFieldSelection selection =
        ParseTrackFieldSelection(std::optional<std::vector<std::string>>{});

    EXPECT_TRUE(selection.valid);
    EXPECT_FALSE(selection.projected);
    EXPECT_EQ(selection.mask, TrackField::kAll);
}

TEST(TrackFieldSelection, DeclaredFieldsReportEveryUnknownNameOnce) {
    const TrackFieldSelection selection = ParseTrackFieldSelection(
        std::optional<std::vector<std::string>>({"handle", "bogus", "title", "bogus", "Title"}));

    EXPECT_FALSE(selection.valid);
    EXPECT_EQ(selection.errorMessage, "fields contains unknown field names");
    EXPECT_EQ(selection.unknownFields, (std::vector<std::string>{"bogus", "Title"}));
}

TEST(TrackFieldSelection, DeclaredFieldsProjectTheNamedKeys) {
    const TrackFieldSelection selection = ParseTrackFieldSelection(
        std::optional<std::vector<std::string>>({"handle", "album", "handle"}));

    EXPECT_TRUE(selection.valid);
    EXPECT_TRUE(selection.projected);
    EXPECT_EQ(selection.mask, TrackField::kHandle | TrackField::kAlbum);
}

TEST(TrackFieldSelection, UnknownNameIsRejectedAndReported) {
    const TrackFieldSelection selection =
        track_wire_test::SelectFields({"absolutePath", "nosuchfield"});

    EXPECT_FALSE(selection.valid);
    EXPECT_FALSE(selection.errorMessage.empty());
    ASSERT_EQ(selection.unknownFields.size(), 1u);
    EXPECT_EQ(selection.unknownFields[0], "nosuchfield");
}

TEST(TrackFieldSelection, AllUnknownNamesAreReportedOnceEach) {
    // 一次报全：调用方改一遍就能对，不用逐个试
    const TrackFieldSelection selection = track_wire_test::SelectFields(
        {"bogus", "title", "alsoBogus", "bogus"});

    EXPECT_FALSE(selection.valid);
    ASSERT_EQ(selection.unknownFields.size(), 2u);
    EXPECT_EQ(selection.unknownFields[0], "bogus");
    EXPECT_EQ(selection.unknownFields[1], "alsoBogus");
}

TEST(TrackFieldSelection, NameMatchingIsCaseSensitive) {
    // 大小写不符按未知名处理（静默接受等于让拼写错悄悄少一个字段）
    for (const char* name : {"Title", "TITLE", "absolutepath", "AbsolutePath", "Rating"}) {
        const TrackFieldSelection selection = track_wire_test::SelectFields({name});
        EXPECT_FALSE(selection.valid) << "accepted case mismatch: " << name;
        ASSERT_EQ(selection.unknownFields.size(), 1u);
        EXPECT_EQ(selection.unknownFields[0], name);
    }
}

TEST(TrackFieldSelection, DuplicateNamesAreDeduplicated) {
    const TrackFieldSelection selection = track_wire_test::SelectFields(
        {"absolutePath", "album", "absolutePath", "album", "absolutePath"});

    EXPECT_TRUE(selection.valid);
    EXPECT_TRUE(selection.projected);
    EXPECT_EQ(selection.mask, track_wire_test::MaskOf({"absolutePath", "album"}));
    EXPECT_EQ(TrackField::Count(selection.mask), 2u);
}

TEST(TrackFieldSelection, AllWhitelistNamesParseToFullMask) {
    std::vector<std::string> fields;
    for (const auto& entry : TrackField::kTable) {
        fields.emplace_back(entry.name);
    }
    const TrackFieldSelection selection = track_wire_test::SelectFields(fields);

    EXPECT_TRUE(selection.valid);
    EXPECT_TRUE(selection.projected);  // 显式列全仍记作投影，产物与省略 fields 相同
    EXPECT_EQ(selection.mask, TrackField::kAll);
}

// ============================================
// fields 投影输出
// ============================================

TEST(TrackWireProjection, SingleAbsolutePathFieldOnly) {
    // spec §3.3 的头号用例：数万命中只要路径
    const auto snap = track_wire_test::MakeFullSnapshot();
    const json parsed =
        track_wire_test::WriteProjectedAndParse(snap, track_wire_test::MaskOf({"absolutePath"}));

    ASSERT_TRUE(parsed.is_object());
    ASSERT_EQ(parsed.size(), 1u);
    EXPECT_EQ(parsed["absolutePath"].get<std::string>(), snap.absolutePath);
    EXPECT_FALSE(parsed.contains("index"));
    EXPECT_FALSE(parsed.contains("path"));
    EXPECT_FALSE(parsed.contains("rating"));
}

TEST(TrackWireProjection, MultiFieldCombinationEmitsExactlyRequestedKeys) {
    const auto snap = track_wire_test::MakeFullSnapshot();
    const uint32_t mask =
        track_wire_test::MaskOf({"index", "album", "duration", "absolutePath", "rating"});
    const json parsed = track_wire_test::WriteProjectedAndParse(snap, mask);

    ASSERT_EQ(parsed.size(), 5u);
    EXPECT_EQ(parsed["index"], snap.index);
    EXPECT_EQ(parsed["album"].get<std::string>(), snap.album);
    EXPECT_EQ(parsed["duration"], json(snap.duration));
    EXPECT_EQ(parsed["absolutePath"].get<std::string>(), snap.absolutePath);
    EXPECT_EQ(parsed["rating"], snap.rating);

    // 未请求的一个都不许附带
    for (const auto& entry : TrackField::kTable) {
        if (mask & entry.bit) continue;
        EXPECT_FALSE(parsed.contains(entry.name)) << "unrequested key present: " << entry.name;
    }
}

TEST(TrackWireProjection, FullMaskMatchesFullFieldWriter) {
    const auto snap = track_wire_test::MakeFullSnapshot();

    const json projected = track_wire_test::WriteProjectedAndParse(snap, TrackField::kAll);
    EXPECT_EQ(projected, track_wire_test::ExpectedFullDom(snap));  // 语义等价（判据）
    EXPECT_EQ(projected.size(), 22u);

    // 字节相同不是对外契约，但两支同序同原语时它成立 —— 拿它当"两支没漂移"的哨兵
    std::string full;
    WriteTrackJson(full, snap);
    EXPECT_EQ(track_wire_test::WriteProjected(snap, TrackField::kAll), full);
}

TEST(TrackWireProjection, KeyOrderIsStableAcrossWrites) {
    const auto snap = track_wire_test::MakeFullSnapshot();
    for (uint32_t mask : {track_wire_test::MaskOf({"absolutePath"}),
                          track_wire_test::MaskOf({"rating", "index", "codec"}),
                          track_wire_test::MaskOf({"title", "artist", "album", "duration"}),
                          TrackField::kAll}) {
        const std::string first = track_wire_test::WriteProjected(snap, mask);
        const std::string second = track_wire_test::WriteProjected(snap, mask);
        EXPECT_EQ(first, second);
    }
}

TEST(TrackWireProjection, KeyOrderFollowsTableOrderNotRequestOrder) {
    // 掩码不记调用方的书写顺序，输出恒按表序 —— 逆序请求与正序请求产物相同
    const auto snap = track_wire_test::MakeFullSnapshot();
    const std::string forward =
        track_wire_test::WriteProjected(snap, track_wire_test::MaskOf({"index", "title", "rating"}));
    const std::string reversed =
        track_wire_test::WriteProjected(snap, track_wire_test::MaskOf({"rating", "title", "index"}));

    EXPECT_EQ(forward, reversed);
    EXPECT_NE(forward.find("\"index\""), std::string::npos);
    EXPECT_LT(forward.find("\"index\""), forward.find("\"title\""));
    EXPECT_LT(forward.find("\"title\""), forward.find("\"rating\""));
}

TEST(TrackWireProjection, DuplicateRequestYieldsSameOutputAsDeduplicated) {
    const auto snap = track_wire_test::MakeFullSnapshot();
    const TrackFieldSelection duplicated =
        track_wire_test::SelectFields({"album", "album", "index", "album"});
    ASSERT_TRUE(duplicated.valid);

    EXPECT_EQ(track_wire_test::WriteProjected(snap, duplicated.mask),
              track_wire_test::WriteProjected(snap, track_wire_test::MaskOf({"index", "album"})));
    EXPECT_EQ(track_wire_test::WriteProjectedAndParse(snap, duplicated.mask).size(), 2u);
}

TEST(TrackWireProjection, BrokenRowStillEmitsEveryRequestedKey) {
    // 投影下取不到 info 的行同样出全部请求键，取自 info 的字段取类型默认
    TrackWireSnapshot snap = track_wire_test::MakeFullSnapshot();
    ResetInfoFields(snap);
    snap.rating = 0;

    const json parsed = track_wire_test::WriteProjectedAndParse(snap, TrackField::kAll);
    ASSERT_EQ(parsed.size(), 22u);

    // 取自句柄的真值
    EXPECT_EQ(parsed["index"], snap.index);
    EXPECT_EQ(parsed["handle"].get<std::string>(), snap.handle);
    EXPECT_EQ(parsed["path"].get<std::string>(), snap.path);
    EXPECT_EQ(parsed["absolutePath"].get<std::string>(), snap.absolutePath);
    EXPECT_EQ(parsed["fileSize"], snap.fileSize);
    EXPECT_EQ(parsed["subsong"], snap.subsong);
    // 其余按类型默认填充；artists、albumArtists 的类型默认是空数组，不是空串
    EXPECT_EQ(parsed["artists"], json::array());
    EXPECT_EQ(parsed["albumArtists"], json::array());
    for (const char* key : {"title", "artist", "album", "albumArtist", "genre", "date", "codec"}) {
        EXPECT_EQ(parsed[key], "") << "not defaulted: " << key;
    }
    for (const char* key : {"trackNumber", "discNumber", "bitrate", "sampleRate", "channels",
                            "rating"}) {
        EXPECT_EQ(parsed[key], 0) << "not defaulted: " << key;
    }
    EXPECT_EQ(parsed["duration"], json(0.0));
    EXPECT_TRUE(parsed["duration"].is_number_float());
}

TEST(TrackWireProjection, BrokenRowEmitsRequestedSubsetOnly) {
    TrackWireSnapshot snap;
    snap.index = 7;
    snap.path = "file://D:\\音乐\\损坏.flac";
    snap.absolutePath = "D:\\音乐\\损坏.flac";
    ResetInfoFields(snap);

    const uint32_t mask = track_wire_test::MaskOf({"absolutePath", "genre", "bitrate"});
    const json parsed = track_wire_test::WriteProjectedAndParse(snap, mask);

    ASSERT_EQ(parsed.size(), 3u);
    EXPECT_EQ(parsed["absolutePath"].get<std::string>(), snap.absolutePath);
    EXPECT_EQ(parsed["genre"], "");
    EXPECT_EQ(parsed["bitrate"], 0);
}

TEST(TrackWireProjection, ResetLeavesNoResidueFromPreviousRow) {
    // 序列化侧的快照对象在行循环外复用：取不到 info 的行必须先归零，否则上一行的
    // genre、codec 等会原样出现在这一行，成为无法从输出反查的串值
    TrackWireSnapshot snap = track_wire_test::MakeFullSnapshot();
    const uint32_t mask = track_wire_test::MaskOf(
        {"artists", "albumArtists", "genre", "codec", "bitrate", "duration", "trackNumber"});

    const json previous = track_wire_test::WriteProjectedAndParse(snap, mask);
    ASSERT_EQ(previous["genre"], "Post-Rock");
    ASSERT_EQ(previous["artists"].size(), 2u);
    ASSERT_EQ(previous["albumArtists"].size(), 2u);

    ResetInfoFields(snap);
    const json current = track_wire_test::WriteProjectedAndParse(snap, mask);

    EXPECT_EQ(current["artists"], json::array());
    EXPECT_EQ(current["albumArtists"], json::array());
    EXPECT_EQ(current["genre"], "");
    EXPECT_EQ(current["codec"], "");
    EXPECT_EQ(current["bitrate"], 0);
    EXPECT_EQ(current["duration"], json(0.0));
    EXPECT_EQ(current["trackNumber"], 0);
}

TEST(TrackWireProjection, ResetKeepsFieldsTakenFromTheHandle) {
    // 归零只碰取自 info 的字段；取自句柄的字段与调用方给的 rating 是真值
    TrackWireSnapshot snap = track_wire_test::MakeFullSnapshot();
    ResetInfoFields(snap);

    EXPECT_EQ(snap.index, 42u);
    EXPECT_EQ(snap.handle, "C:\\music\\a.flac|subsong:3");
    EXPECT_EQ(snap.path, "file://C:\\music\\a.flac");
    EXPECT_EQ(snap.absolutePath, "C:\\music\\a.flac");
    EXPECT_EQ(snap.fileSize, 41231234);
    EXPECT_EQ(snap.subsong, 3u);
    EXPECT_EQ(snap.rating, 5);
    EXPECT_EQ(snap.title, "");
    EXPECT_DOUBLE_EQ(snap.duration, 0.0);
}

TEST(TrackWireProjection, EscapingAndNonFiniteHoldUnderProjection) {
    // 投影与全字段共用同一批 JsonWriter 原语，转义/非有限浮点语义不因收窄而变
    auto snap = track_wire_test::MakeFullSnapshot();
    snap.title = "无损 \"测试\"\t曲目";
    snap.codec = "FLAC \xF0\x9F\x8E\xB5";
    snap.duration = std::numeric_limits<double>::quiet_NaN();

    const uint32_t mask = track_wire_test::MaskOf({"title", "codec", "duration"});
    json parsed;
    ASSERT_NO_THROW(parsed = track_wire_test::WriteProjectedAndParse(snap, mask));
    ASSERT_EQ(parsed.size(), 3u);
    EXPECT_EQ(parsed["title"].get<std::string>(), snap.title);
    EXPECT_EQ(parsed["codec"].get<std::string>(), snap.codec);
    EXPECT_TRUE(parsed["duration"].is_null());
}

TEST(TrackWireProjection, AppendsWithoutClearingBuffer) {
    // 数组拼装口径与全字段路径一致：不清空调用方缓冲
    std::string out = "[";
    WriteTrackJsonProjected(out, track_wire_test::MakeFullSnapshot(),
                            track_wire_test::MaskOf({"absolutePath"}));
    out += ",";
    WriteTrackJsonProjected(out, track_wire_test::MakeFullSnapshot(),
                            track_wire_test::MaskOf({"absolutePath"}));
    out.push_back(']');

    json parsed;
    ASSERT_NO_THROW(parsed = json::parse(out));
    ASSERT_TRUE(parsed.is_array());
    ASSERT_EQ(parsed.size(), 2u);
    EXPECT_EQ(parsed[0].size(), 1u);
    EXPECT_EQ(parsed[0], parsed[1]);
}

// ==========================================================================
// MakeTrackFieldsErrorBody — playlist.getTracks 与 library.* 共用的参数错误形状
// 直接验证 TrackWireSnapshot.h 的共享实现：错误码、details 条件与未知字段顺序。
// ==========================================================================

TEST(TrackFieldsErrorBody, UnknownNamesAreAllListedInDetails) {
    // 扫完再报：调用方一次就能改对，不用逐个试
    const TrackFieldSelection selection =
        track_wire_test::SelectFields({"title", "bogus", "bogus2", "bogus"});
    ASSERT_FALSE(selection.valid);
    const json body = MakeTrackFieldsErrorBody(selection);
    EXPECT_EQ(body.at("code").get<std::string>(), "INVALID_PARAMS");
    // 重复名去重后按出现序列出
    EXPECT_EQ(body.at("details").at("unknownFields"),
              json::array({"bogus", "bogus2"}));
}

TEST(TrackFieldsErrorBody, ValidSelectionIsProjectedWithRequestedBits) {
    const TrackFieldSelection selection =
        track_wire_test::SelectFields({"title", "album"});
    ASSERT_TRUE(selection.valid);
    EXPECT_TRUE(selection.projected);
    EXPECT_EQ(selection.mask, TrackField::kTitle | TrackField::kAlbum);
}
