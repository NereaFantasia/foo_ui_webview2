// test_track_wire_snapshot.cpp — 曲目直写序列化的纯函数层对拍
//
// WriteTrackJson 是 library.query / library.search 热路径唯一产出 tracks 元素的
// 地方，字段名或取值类型写错就是静默的契约破口。判据（spec §8.3 单测层）：
// json::parse(writer 产物) == 用同名同值字段手搭的 nlohmann 对象 —— 与 DOM 版
// BuildTrackJsonFromSnapshot 的输出逐字段对齐。键序不比（nlohmann 对象按键名
// 排序输出，直写按声明序），键**集**与每个键的值必须全等。
#include "pch.h"
#include "api/TrackWireSnapshot.h"

#include <initializer_list>
#include <iterator>
#include <limits>

using json = nlohmann::json;

namespace track_wire_test {

// 一条字段齐全、值都不取默认的快照：任何字段漏写/串位都会在对拍里暴露
TrackWireSnapshot MakeFullSnapshot() {
    TrackWireSnapshot snap;
    snap.index = 42;
    snap.title = "Track Title";
    // 多值曲目：artist 恒等于 artists 按 ", " 拼接的结果（对外契约），取值侧由
    // MetaValuesRaw + JoinMetaValues 保证，此处的夹具按同一口径构造
    snap.artist = "Artist Name, Second Artist";
    snap.artists = {"Artist Name", "Second Artist"};
    snap.album = "Album Name";
    snap.albumArtist = "Album Artist";
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
    snap.hasInfo = true;
    return snap;
}

// DOM 侧的等价对象 —— 字段名与取值类型照 BuildTrackJsonFromSnapshot 的全字段分支
json ExpectedFullDom(const TrackWireSnapshot& snap) {
    return {
        {"index", snap.index},
        {"title", snap.title},
        {"artist", snap.artist},
        {"artists", snap.artists},
        {"album", snap.album},
        {"albumArtist", snap.albumArtist},
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

// DOM 侧 fallback 分支（info 容器无效）：只有 8 键
json ExpectedFallbackDom(const TrackWireSnapshot& snap) {
    return {
        {"index", snap.index},
        {"title", snap.title},
        {"artist", snap.artist},
        {"album", snap.album},
        {"duration", snap.duration},
        {"path", snap.path},
        {"absolutePath", snap.absolutePath},
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

// 把 fields 的值挂进一份典型 params 再解析（其余键存在与否不影响判定）
TrackFieldSelection SelectFields(json fieldsValue) {
    json params = {{"query", "genre IS Rock"}, {"limit", 100}};
    params["fields"] = std::move(fieldsValue);
    return ParseTrackFieldSelection(params);
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

TEST(TrackWireSnapshot, FullFieldSetHasExactlyTwentyKeys) {
    const auto snap = track_wire_test::MakeFullSnapshot();
    const json parsed = track_wire_test::WriteAndParse(snap);

    ASSERT_TRUE(parsed.is_object());
    EXPECT_EQ(parsed.size(), 20u);

    // 键名逐个钉住：拼错一个字符 = 主题侧字段凭空消失
    for (const char* key : {"index", "title", "artist", "artists", "album", "albumArtist",
                            "genre", "date", "trackNumber", "discNumber", "duration", "path",
                            "absolutePath", "fileSize", "bitrate", "sampleRate",
                            "channels", "codec", "subsong", "rating"}) {
        EXPECT_TRUE(parsed.contains(key)) << "missing key: " << key;
    }

    // artists 是数组形态，且 join(", ") 还原成 artist
    ASSERT_TRUE(parsed["artists"].is_array());
    EXPECT_EQ(parsed["artists"].get<std::vector<std::string>>(), snap.artists);
    EXPECT_EQ(parsed["artist"].get<std::string>(), "Artist Name, Second Artist");
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
// fallback 分支（info 容器无效）
// ============================================

TEST(TrackWireSnapshot, FallbackEmitsEightKeysOnly) {
    auto snap = track_wire_test::MakeFullSnapshot();
    snap.hasInfo = false;

    const json parsed = track_wire_test::WriteAndParse(snap);
    ASSERT_TRUE(parsed.is_object());
    EXPECT_EQ(parsed.size(), 8u);
    EXPECT_EQ(parsed, track_wire_test::ExpectedFallbackDom(snap));

    // 全字段专属的键一个都不能漏出来。artists 在此列：fallback 分支任何标签都取不到，
    // artist 恒为 ""，再出一个恒空数组是纯噪声，键集因此仍是既有的 8 键
    for (const char* key : {"artists", "albumArtist", "genre", "date", "trackNumber",
                            "discNumber", "fileSize", "bitrate", "sampleRate", "channels",
                            "codec", "subsong"}) {
        EXPECT_FALSE(parsed.contains(key)) << "unexpected key: " << key;
    }
}

TEST(TrackWireSnapshot, FallbackKeepsIndexPathAndRating) {
    // 实际调用方在此分支只填这几项，其余留默认
    TrackWireSnapshot snap;
    snap.hasInfo = false;
    snap.index = 9;
    snap.path = "file://D:\\音乐\\损坏.flac";
    snap.absolutePath = "D:\\音乐\\损坏.flac";

    const json parsed = track_wire_test::WriteAndParse(snap);
    EXPECT_EQ(parsed["index"], 9);
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
    // 热路径的实际形态：正常行 + fallback 行 + null 行（handle 无效）混在一个数组里
    TrackWireSnapshot full = track_wire_test::MakeFullSnapshot();
    full.index = 0;

    TrackWireSnapshot broken;
    broken.hasInfo = false;
    broken.index = 1;
    broken.path = "file://C:\\music\\损坏.mp3";
    broken.absolutePath = "C:\\music\\损坏.mp3";

    std::string out = "[";
    WriteTrackJson(out, full);
    out += ",null,";
    WriteTrackJson(out, broken);
    out.push_back(']');

    json parsed;
    ASSERT_NO_THROW(parsed = json::parse(out));
    ASSERT_EQ(parsed.size(), 3u);
    EXPECT_EQ(parsed[0], track_wire_test::ExpectedFullDom(full));
    EXPECT_TRUE(parsed[1].is_null());
    EXPECT_EQ(parsed[2], track_wire_test::ExpectedFallbackDom(broken));
    EXPECT_EQ(parsed[2].size(), 8u);
}

TEST(TrackWireSnapshot, SnapshotReuseAcrossRowsLeavesNoResidue) {
    // 序列化侧循环外复用同一个 snapshot 对象（省分配），fallback 行不得把上一行的
    // 全字段值带出来，全字段行也不得残留上一行的 fallback 状态
    TrackWireSnapshot snap = track_wire_test::MakeFullSnapshot();

    std::string first;
    WriteTrackJson(first, snap);

    snap.hasInfo = false;
    snap.title.clear();
    snap.artist.clear();
    snap.album.clear();
    snap.duration = 0.0;
    snap.rating = 0;
    std::string second;
    WriteTrackJson(second, snap);
    EXPECT_EQ(json::parse(second).size(), 8u);
    EXPECT_EQ(json::parse(second)["title"], "");

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

TEST(TrackFieldSelection, OmittedFieldsKeepsFullFieldBehaviour) {
    const json params = {{"query", "genre IS Rock"}, {"limit", 100}};
    const TrackFieldSelection selection = ParseTrackFieldSelection(params);

    EXPECT_TRUE(selection.valid);
    EXPECT_FALSE(selection.projected);  // 走全字段 writer，形状不变
    EXPECT_EQ(selection.mask, TrackField::kAll);
    EXPECT_TRUE(selection.errorMessage.empty());
    EXPECT_TRUE(selection.unknownFields.empty());
}

TEST(TrackFieldSelection, ExplicitNullIsRejected) {
    // 显式 null 与"缺键"不同：前者是调用方写错，不能当成"要全字段"
    const TrackFieldSelection selection = track_wire_test::SelectFields(json(nullptr));

    EXPECT_FALSE(selection.valid);
    EXPECT_FALSE(selection.errorMessage.empty());
    EXPECT_TRUE(selection.unknownFields.empty());
}

TEST(TrackFieldSelection, NonArrayShapesAreRejected) {
    for (const json& value : {json("absolutePath"), json(42), json(true),
                              json{{"absolutePath", true}}}) {
        const TrackFieldSelection selection = track_wire_test::SelectFields(value);
        EXPECT_FALSE(selection.valid) << "accepted non-array: " << value.dump();
        EXPECT_FALSE(selection.errorMessage.empty());
    }
}

TEST(TrackFieldSelection, EmptyArrayIsRejected) {
    // 空数组不等于"全字段"：那个语义由"缺键"表达，空数组只能是调用方算错了
    const TrackFieldSelection selection = track_wire_test::SelectFields(json::array());

    EXPECT_FALSE(selection.valid);
    EXPECT_FALSE(selection.errorMessage.empty());
    EXPECT_TRUE(selection.unknownFields.empty());
}

TEST(TrackFieldSelection, NonStringElementIsRejected) {
    for (const json& element : {json(1), json(nullptr), json(true), json::array({"title"}),
                               json{{"name", "title"}}}) {
        json fields = json::array({"title"});
        fields.push_back(element);
        const TrackFieldSelection selection = track_wire_test::SelectFields(fields);

        EXPECT_FALSE(selection.valid) << "accepted element: " << element.dump();
        EXPECT_FALSE(selection.errorMessage.empty());
        // 元素类型错时不报未知名：那会把"类型错"说成"拼写错"
        EXPECT_TRUE(selection.unknownFields.empty());
    }
}

TEST(TrackFieldSelection, UnknownNameIsRejectedAndReported) {
    const TrackFieldSelection selection =
        track_wire_test::SelectFields(json::array({"absolutePath", "nosuchfield"}));

    EXPECT_FALSE(selection.valid);
    EXPECT_FALSE(selection.errorMessage.empty());
    ASSERT_EQ(selection.unknownFields.size(), 1u);
    EXPECT_EQ(selection.unknownFields[0], "nosuchfield");
}

TEST(TrackFieldSelection, AllUnknownNamesAreReportedOnceEach) {
    // 一次报全：调用方改一遍就能对，不用逐个试
    const TrackFieldSelection selection = track_wire_test::SelectFields(
        json::array({"bogus", "title", "alsoBogus", "bogus"}));

    EXPECT_FALSE(selection.valid);
    ASSERT_EQ(selection.unknownFields.size(), 2u);
    EXPECT_EQ(selection.unknownFields[0], "bogus");
    EXPECT_EQ(selection.unknownFields[1], "alsoBogus");
}

TEST(TrackFieldSelection, NameMatchingIsCaseSensitive) {
    // 大小写不符按未知名处理（静默接受等于让拼写错悄悄少一个字段）
    for (const char* name : {"Title", "TITLE", "absolutepath", "AbsolutePath", "Rating"}) {
        const TrackFieldSelection selection = track_wire_test::SelectFields(json::array({name}));
        EXPECT_FALSE(selection.valid) << "accepted case mismatch: " << name;
        ASSERT_EQ(selection.unknownFields.size(), 1u);
        EXPECT_EQ(selection.unknownFields[0], name);
    }
}

TEST(TrackFieldSelection, DuplicateNamesAreDeduplicated) {
    const TrackFieldSelection selection = track_wire_test::SelectFields(
        json::array({"absolutePath", "album", "absolutePath", "album", "absolutePath"}));

    EXPECT_TRUE(selection.valid);
    EXPECT_TRUE(selection.projected);
    EXPECT_EQ(selection.mask, track_wire_test::MaskOf({"absolutePath", "album"}));
    EXPECT_EQ(TrackField::Count(selection.mask), 2u);
}

TEST(TrackFieldSelection, AllNineteenNamesParseToFullMask) {
    json fields = json::array();
    for (const auto& entry : TrackField::kTable) {
        fields.push_back(entry.name);
    }
    const TrackFieldSelection selection = track_wire_test::SelectFields(fields);

    EXPECT_TRUE(selection.valid);
    EXPECT_TRUE(selection.projected);  // 显式全字段仍是投影模式（损坏条目形状不同）
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
    EXPECT_EQ(projected.size(), 20u);

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
        track_wire_test::SelectFields(json::array({"album", "album", "index", "album"}));
    ASSERT_TRUE(duplicated.valid);

    EXPECT_EQ(track_wire_test::WriteProjected(snap, duplicated.mask),
              track_wire_test::WriteProjected(snap, track_wire_test::MaskOf({"index", "album"})));
    EXPECT_EQ(track_wire_test::WriteProjectedAndParse(snap, duplicated.mask).size(), 2u);
}

TEST(TrackWireProjection, BrokenRowStillEmitsEveryRequestedKey) {
    // 投影下损坏条目（info 容器无效）出全部请求键，fallback 不产出的字段取类型默认。
    // 与全字段模式的 8 键 fallback 不同：那里键会直接消失。
    TrackWireSnapshot snap = track_wire_test::MakeFullSnapshot();
    snap.hasInfo = false;
    snap.title.clear();
    snap.artist.clear();
    snap.album.clear();
    snap.duration = 0.0;
    snap.rating = 0;
    ResetFieldsAbsentFromFallback(snap);

    const json parsed = track_wire_test::WriteProjectedAndParse(snap, TrackField::kAll);
    ASSERT_EQ(parsed.size(), 20u);

    // fallback 自己产出的真值
    EXPECT_EQ(parsed["index"], snap.index);
    EXPECT_EQ(parsed["path"].get<std::string>(), snap.path);
    EXPECT_EQ(parsed["absolutePath"].get<std::string>(), snap.absolutePath);
    // 其余按类型默认填充；artists 的类型默认是空数组，不是空串
    EXPECT_EQ(parsed["artists"], json::array());
    for (const char* key : {"title", "artist", "album", "albumArtist", "genre", "date", "codec"}) {
        EXPECT_EQ(parsed[key], "") << "not defaulted: " << key;
    }
    for (const char* key : {"trackNumber", "discNumber", "fileSize", "bitrate", "sampleRate",
                            "channels", "subsong", "rating"}) {
        EXPECT_EQ(parsed[key], 0) << "not defaulted: " << key;
    }
    EXPECT_EQ(parsed["duration"], json(0.0));
    EXPECT_TRUE(parsed["duration"].is_number_float());
}

TEST(TrackWireProjection, BrokenRowEmitsRequestedSubsetOnly) {
    TrackWireSnapshot snap;
    snap.hasInfo = false;
    snap.index = 7;
    snap.path = "file://D:\\音乐\\损坏.flac";
    snap.absolutePath = "D:\\音乐\\损坏.flac";
    ResetFieldsAbsentFromFallback(snap);

    const uint32_t mask = track_wire_test::MaskOf({"absolutePath", "genre", "bitrate"});
    const json parsed = track_wire_test::WriteProjectedAndParse(snap, mask);

    ASSERT_EQ(parsed.size(), 3u);
    EXPECT_EQ(parsed["absolutePath"].get<std::string>(), snap.absolutePath);
    EXPECT_EQ(parsed["genre"], "");
    EXPECT_EQ(parsed["bitrate"], 0);
}

TEST(TrackWireProjection, ProjectionIgnoresHasInfoFlag) {
    // 投影写法不看 hasInfo：键集只由掩码决定，归零由调用方负责
    auto snap = track_wire_test::MakeFullSnapshot();
    const uint32_t mask = track_wire_test::MaskOf({"codec", "bitrate"});
    const std::string withInfo = track_wire_test::WriteProjected(snap, mask);

    snap.hasInfo = false;
    EXPECT_EQ(track_wire_test::WriteProjected(snap, mask), withInfo);
}

TEST(TrackWireProjection, ResetLeavesNoResidueFromPreviousRow) {
    // 序列化侧的快照对象在行循环外复用：损坏行必须先归零，否则上一行的
    // genre/codec/fileSize 会原样出现在这一行，成为无法从输出反查的串值
    TrackWireSnapshot snap = track_wire_test::MakeFullSnapshot();
    const uint32_t mask = track_wire_test::MaskOf(
        {"artists", "genre", "codec", "fileSize", "subsong", "trackNumber"});

    const json previous = track_wire_test::WriteProjectedAndParse(snap, mask);
    ASSERT_EQ(previous["genre"], "Post-Rock");
    ASSERT_EQ(previous["artists"].size(), 2u);

    snap.hasInfo = false;
    ResetFieldsAbsentFromFallback(snap);
    const json current = track_wire_test::WriteProjectedAndParse(snap, mask);

    EXPECT_EQ(current["artists"], json::array());
    EXPECT_EQ(current["genre"], "");
    EXPECT_EQ(current["codec"], "");
    EXPECT_EQ(current["fileSize"], 0);
    EXPECT_EQ(current["subsong"], 0);
    EXPECT_EQ(current["trackNumber"], 0);
}

TEST(TrackWireProjection, ResetKeepsFallbackOwnFields) {
    // 归零只碰 fallback 不产出的 12 键，index/path/absolutePath/rating 是真值
    TrackWireSnapshot snap = track_wire_test::MakeFullSnapshot();
    ResetFieldsAbsentFromFallback(snap);

    EXPECT_EQ(snap.index, 42u);
    EXPECT_EQ(snap.path, "file://C:\\music\\a.flac");
    EXPECT_EQ(snap.absolutePath, "C:\\music\\a.flac");
    EXPECT_EQ(snap.rating, 5);
    EXPECT_EQ(snap.title, "Track Title");  // fallback 侧由调用方自己清
    EXPECT_DOUBLE_EQ(snap.duration, 253.093);
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
