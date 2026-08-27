// test_json_writer.cpp — 直写 JSON 原语的边界用例与 nlohmann 等价性对拍
//
// 等价性判据（spec §8.3 单测层）：json::parse(writer 产物) == 同值 nlohmann 对象。
// 字符串面额外断言字节级与 dump() 全等 —— 转义策略是照 nlohmann 3.11.3 的
// dump_escaped 抄的（仅 `"` `\` 与 U+0000–U+001F 转义、\u 小写十六进制、非 ASCII
// 原样透传），字节漂移即等价性破口，直接钉住比只比语义更早发现问题。
// 浮点面只比语义：writer 走 std::to_chars 最短往返，与 Grisu2 的字节形态不强求相同。
#include "pch.h"
#include "utils/JsonWriter.h"

#include <cstring>
#include <limits>
#include <vector>

using json = nlohmann::json;

namespace json_writer_test {

// 字符串：writer 产物必须与 dump() 逐字节相同，且 parse 回来语义相等
void ExpectStringMatchesDump(const std::string& input) {
    std::string out;
    JsonWriter::AppendJsonString(out, input);

    const json expected = input;
    EXPECT_EQ(out, expected.dump());

    json parsed;
    ASSERT_NO_THROW(parsed = json::parse(out));
    EXPECT_EQ(parsed, expected);
}

// 字符串数组：元素转义与单串同源，故同样按字节对齐 dump()
void ExpectStringArrayMatchesDump(const std::vector<std::string>& values) {
    std::string out;
    JsonWriter::AppendJsonStringArray(out, values);

    const json expected = values;
    EXPECT_EQ(out, expected.dump());

    json parsed;
    ASSERT_NO_THROW(parsed = json::parse(out));
    EXPECT_EQ(parsed, expected);
}

uint64_t BitPattern(double value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// 浮点：parse 回来必须位模式全等（±0 的符号位也要保住）
void ExpectNumberRoundTrip(double value) {
    std::string out;
    JsonWriter::AppendJsonNumber(out, value);

    json parsed;
    ASSERT_NO_THROW(parsed = json::parse(out));
    ASSERT_TRUE(parsed.is_number()) << "out=" << out;
    EXPECT_EQ(BitPattern(parsed.get<double>()), BitPattern(value)) << "out=" << out;
    EXPECT_EQ(parsed, json(value)) << "out=" << out;
}

}  // namespace json_writer_test

// ============================================
// AppendJsonString — 转义面
// ============================================

TEST(JsonWriter, StringEmptyIsQuotedPair) {
    std::string out;
    JsonWriter::AppendJsonString(out, "");
    EXPECT_EQ(out, "\"\"");
}

TEST(JsonWriter, StringPlainAsciiUnescaped) {
    std::string out;
    JsonWriter::AppendJsonString(out, "C:/music/track.flac");
    EXPECT_EQ(out, "\"C:/music/track.flac\"");  // `/` 不转义（与 dump 一致）
    json_writer_test::ExpectStringMatchesDump("C:/music/track.flac");
}

TEST(JsonWriter, StringQuoteAndBackslash) {
    std::string out;
    JsonWriter::AppendJsonString(out, R"(C:\music\a "b".flac)");
    EXPECT_EQ(out, R"("C:\\music\\a \"b\".flac")");
    json_writer_test::ExpectStringMatchesDump(R"(C:\music\a "b".flac)");
}

TEST(JsonWriter, StringShortEscapes) {
    std::string out;
    JsonWriter::AppendJsonString(out, "a\bb\fc\nd\re\tf");
    EXPECT_EQ(out, R"("a\bb\fc\nd\re\tf")");
    json_writer_test::ExpectStringMatchesDump("a\bb\fc\nd\re\tf");
}

TEST(JsonWriter, StringAllThirtyTwoControlCharacters) {
    // U+0000–U+001F 全覆盖，含嵌入 NUL（故此处必须用带长度的构造）
    std::string input;
    for (int i = 0; i <= 0x1F; ++i) {
        input.push_back(static_cast<char>(i));
    }
    ASSERT_EQ(input.size(), 32u);

    std::string out;
    JsonWriter::AppendJsonString(out, input);

    // 五个短转义 + 27 个 \u00xx（小写十六进制）
    EXPECT_NE(out.find("\\u0000"), std::string::npos);
    EXPECT_NE(out.find("\\u001f"), std::string::npos);
    EXPECT_EQ(out.find("\\u001F"), std::string::npos);  // 大写即与 dump 字节漂移
    EXPECT_NE(out.find("\\b"), std::string::npos);
    EXPECT_NE(out.find("\\t"), std::string::npos);
    EXPECT_NE(out.find("\\n"), std::string::npos);
    EXPECT_NE(out.find("\\f"), std::string::npos);
    EXPECT_NE(out.find("\\r"), std::string::npos);
    EXPECT_EQ(out.size(), 2u + 5u * 2u + 27u * 6u);

    json_writer_test::ExpectStringMatchesDump(input);
}

TEST(JsonWriter, StringDelNotEscaped) {
    // dump 只转义 <= 0x1F，DEL(0x7F) 原样输出
    const std::string input = std::string("a") + static_cast<char>(0x7F) + "b";
    std::string out;
    JsonWriter::AppendJsonString(out, input);
    EXPECT_EQ(out.size(), input.size() + 2u);
    json_writer_test::ExpectStringMatchesDump(input);
}

TEST(JsonWriter, StringUtf8ChinesePassthrough) {
    std::string out;
    JsonWriter::AppendJsonString(out, "无损 · 播放列表");
    EXPECT_EQ(out, "\"无损 · 播放列表\"");  // 非 ASCII 不作 \u 转义
    json_writer_test::ExpectStringMatchesDump("无损 · 播放列表");
}

TEST(JsonWriter, StringUtf8SurrogatePairEmoji) {
    // U+1F3B5 MUSICAL NOTE：UTF-8 四字节，UTF-16 需代理对
    const std::string emoji = "\xF0\x9F\x8E\xB5";
    std::string out;
    JsonWriter::AppendJsonString(out, "track " + emoji);
    EXPECT_EQ(out, "\"track " + emoji + "\"");
    json_writer_test::ExpectStringMatchesDump("track " + emoji);
    json_writer_test::ExpectStringMatchesDump(emoji + emoji);
}

TEST(JsonWriter, StringUtf8MixedWithEscapes) {
    const std::string input = "标题\t\"引号\"\\反斜杠\n\xF0\x9F\x8E\xB5";
    json_writer_test::ExpectStringMatchesDump(input);
}

TEST(JsonWriter, StringVeryLongOverOneMegabyte) {
    // 超长串：区间成批 append 的边界，且覆盖需转义字符落在末尾的情形
    std::string input;
    input.reserve(1024u * 1024u + 8u);
    while (input.size() < 1024u * 1024u) {
        input.append("abcdefgh");
    }
    input.append("\"\\\n");
    ASSERT_GT(input.size(), 1024u * 1024u);

    std::string out;
    JsonWriter::AppendJsonString(out, input);
    EXPECT_EQ(out.size(), input.size() + 2u + 3u);  // 三个转义各多一字节
    EXPECT_EQ(out, json(input).dump());
}

TEST(JsonWriter, StringAppendsWithoutClearingBuffer) {
    // 契约：所有原语都是"追加"，不得清空调用方缓冲
    std::string out = "{\"title\":";
    JsonWriter::AppendJsonString(out, "abc");
    out.push_back('}');
    EXPECT_EQ(out, R"({"title":"abc"})");
}

// ============================================
// AppendJsonNumber — 浮点面
// ============================================

TEST(JsonWriter, NumberNonFiniteBecomesNull) {
    // nlohmann dump 对 NaN/±Inf 输出字面 null，直写必须一致
    for (double value : {std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity()}) {
        std::string out;
        JsonWriter::AppendJsonNumber(out, value);
        EXPECT_EQ(out, "null");
        EXPECT_TRUE(json::parse(out).is_null());
        EXPECT_EQ(out, json(value).dump());
    }
}

TEST(JsonWriter, NumberZeroAndNegativeZero) {
    std::string out;
    JsonWriter::AppendJsonNumber(out, 0.0);
    EXPECT_EQ(out, "0.0");

    out.clear();
    JsonWriter::AppendJsonNumber(out, -0.0);
    EXPECT_EQ(out, "-0.0");  // 符号位不得丢

    json_writer_test::ExpectNumberRoundTrip(0.0);
    json_writer_test::ExpectNumberRoundTrip(-0.0);
}

TEST(JsonWriter, NumberIntegralValuesKeepFloatForm) {
    // 整数值补 ".0"（与 dump 的 Grisu2 同形），避免与 DOM 产物出现整型/浮点差异
    std::string out;
    JsonWriter::AppendJsonNumber(out, 5.0);
    EXPECT_EQ(out, "5.0");

    out.clear();
    JsonWriter::AppendJsonNumber(out, -42.0);
    EXPECT_EQ(out, "-42.0");
}

TEST(JsonWriter, NumberShortestRoundTripForms) {
    std::string out;
    JsonWriter::AppendJsonNumber(out, 0.8);
    EXPECT_EQ(out, "0.8");  // 定长 %.17g 会写成 0.80000000000000004

    out.clear();
    JsonWriter::AppendJsonNumber(out, 0.1);
    EXPECT_EQ(out, "0.1");
}

TEST(JsonWriter, NumberBoundaryValuesRoundTrip) {
    const double cases[] = {
        0.0,
        -0.0,
        1.0,
        -1.0,
        0.1,
        0.5,
        0.8,
        1.0 / 3.0,
        2.5e-1,
        123456789.0,
        1e21,
        1e-7,
        3.14159265358979311599796346854,
        std::numeric_limits<double>::max(),
        -std::numeric_limits<double>::max(),
        std::numeric_limits<double>::min(),
        std::numeric_limits<double>::denorm_min(),
        std::numeric_limits<double>::epsilon(),
        4503599627370497.0,   // 2^52 + 1，double 可精确表示的整数上界附近
        9007199254740993.0,   // 2^53 + 1，落到最近可表示值
    };
    for (double value : cases) {
        json_writer_test::ExpectNumberRoundTrip(value);
    }
}

TEST(JsonWriter, NumberPrecisionSweepRoundTrip) {
    // 递推出一批不规则尾数，逐个查往返（精度丢失会在此暴露）
    double value = 1.0;
    for (int i = 0; i < 64; ++i) {
        json_writer_test::ExpectNumberRoundTrip(value);
        json_writer_test::ExpectNumberRoundTrip(-value);
        value = value * 3.7 + 0.13;
    }
}

TEST(JsonWriter, NumberTypicalTrackFieldValues) {
    // 热路径实际会写的量级：duration / bitrate 等
    for (double value : {0.0, 1.5, 253.093, 320.0, 44100.0, 1411.2}) {
        json_writer_test::ExpectNumberRoundTrip(value);
    }
}

// ============================================
// AppendJsonInt / AppendJsonBool
// ============================================

TEST(JsonWriter, IntBoundaryValues) {
    struct Case { int64_t value; const char* text; };
    const Case cases[] = {
        {0, "0"},
        {1, "1"},
        {-1, "-1"},
        {2147483647LL, "2147483647"},
        {-2147483648LL, "-2147483648"},
        {9223372036854775807LL, "9223372036854775807"},
        {(-9223372036854775807LL - 1), "-9223372036854775808"},
    };
    for (const auto& item : cases) {
        std::string out;
        JsonWriter::AppendJsonInt(out, item.value);
        EXPECT_EQ(out, item.text);
        EXPECT_EQ(out, json(item.value).dump());
        EXPECT_EQ(json::parse(out).get<int64_t>(), item.value);
    }
}

TEST(JsonWriter, BoolLiterals) {
    std::string out;
    JsonWriter::AppendJsonBool(out, true);
    JsonWriter::AppendJsonBool(out, false);
    EXPECT_EQ(out, "truefalse");
    EXPECT_TRUE(json::parse("true").get<bool>());
    EXPECT_FALSE(json::parse("false").get<bool>());
}

// ============================================
// AppendJsonStringArray — artists 等多值字段的数组形态
// ============================================

TEST(JsonWriter, StringArrayEmptyIsBracketPair) {
    std::string out;
    JsonWriter::AppendJsonStringArray(out, {});
    EXPECT_EQ(out, "[]");
    json_writer_test::ExpectStringArrayMatchesDump({});
}

TEST(JsonWriter, StringArraySingleAndMultipleElements) {
    std::string out;
    JsonWriter::AppendJsonStringArray(out, {"Artist Name"});
    EXPECT_EQ(out, R"(["Artist Name"])");  // 单元素不带逗号

    out.clear();
    JsonWriter::AppendJsonStringArray(out, {"A", "B", "C"});
    EXPECT_EQ(out, R"(["A","B","C"])");  // 分隔符与 dump 一致：逗号后无空格
    json_writer_test::ExpectStringArrayMatchesDump({"A", "B", "C"});
}

TEST(JsonWriter, StringArrayKeepsEmptyAndDuplicateElements) {
    // 取值侧（MetaValuesRaw）不去重、不丢空值，写出侧不得替它做取舍
    json_writer_test::ExpectStringArrayMatchesDump({"A", "A"});
    json_writer_test::ExpectStringArrayMatchesDump({"", "B", ""});
}

TEST(JsonWriter, StringArrayElementsShareStringEscaping) {
    std::string out;
    JsonWriter::AppendJsonStringArray(out, {R"(反斜杠\艺人)", "引号\"艺人", "换行\n艺人"});
    EXPECT_EQ(out, R"(["反斜杠\\艺人","引号\"艺人","换行\n艺人"])");
    json_writer_test::ExpectStringArrayMatchesDump(
        {R"(反斜杠\艺人)", "引号\"艺人", "换行\n艺人", std::string("控制符\x01")});
}

TEST(JsonWriter, StringArrayAppendsWithoutClearingBuffer) {
    std::string out = R"({"artists":)";
    JsonWriter::AppendJsonStringArray(out, {"A", "B"});
    out.push_back('}');

    json expected;
    expected["artists"] = std::vector<std::string>{"A", "B"};

    json parsed;
    ASSERT_NO_THROW(parsed = json::parse(out));
    EXPECT_EQ(parsed, expected);
}

// ============================================
// 组合：手拼对象与 DOM 产物语义等价
// ============================================

TEST(JsonWriter, HandBuiltTrackObjectMatchesDomSemantics) {
    // T3 的曲目序列化形态预演：键名手写、值走原语
    const std::string title = "无损 \"测试\"\t曲目";
    const std::string path = R"(C:\music\无损\a.flac)";

    std::string out = "{\"index\":";
    JsonWriter::AppendJsonInt(out, 7);
    out += ",\"title\":";
    JsonWriter::AppendJsonString(out, title);
    out += ",\"absolutePath\":";
    JsonWriter::AppendJsonString(out, path);
    out += ",\"duration\":";
    JsonWriter::AppendJsonNumber(out, 253.093);
    out += ",\"rating\":";
    JsonWriter::AppendJsonNumber(out, std::numeric_limits<double>::quiet_NaN());
    out += ",\"subsong\":";
    JsonWriter::AppendJsonBool(out, false);
    out += "}";

    const json expected = {
        {"index", 7},
        {"title", title},
        {"absolutePath", path},
        {"duration", 253.093},
        {"rating", nullptr},  // NaN 在两侧都落成 null
        {"subsong", false},
    };

    json parsed;
    ASSERT_NO_THROW(parsed = json::parse(out));
    EXPECT_EQ(parsed, expected);
}

TEST(JsonWriter, HandBuiltArrayMatchesDomSemantics) {
    std::string out = "[";
    for (int i = 0; i < 3; ++i) {
        if (i > 0) out.push_back(',');
        out += "{\"path\":";
        JsonWriter::AppendJsonString(out, "曲目" + std::to_string(i) + ".flac");
        out += "}";
    }
    out.push_back(']');

    const json expected = json::array({
        {{"path", "曲目0.flac"}},
        {{"path", "曲目1.flac"}},
        {{"path", "曲目2.flac"}},
    });
    EXPECT_EQ(json::parse(out), expected);
}
