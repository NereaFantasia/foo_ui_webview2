// test_waveform_cache_key.cpp - 整轨波形缓存键的长度与格式
// （docs/audio-visualization/SPEC.md D8、§10 A0.1）。
//
// 重点是长路径：旧实现拼进 512 字节栈缓冲，超长路径要么 fast-fail 终止进程，
// 要么退化成空串让不同曲目串波形。键里不再有 method / scale：缓存条目存的是
// 一次解码的三组原始窗口值，任何请求形态都由同一条目现算。
#include "pch.h"
#include "../src/utils/WaveformCacheKey.h"

using waveform_cache::MakeKey;

namespace {

std::string Repeat(const std::string& unit, size_t times) {
    std::string out;
    out.reserve(unit.size() * times);
    for (size_t i = 0; i < times; ++i) out += unit;
    return out;
}

}  // namespace

// 普通路径：五个字段按 `|` 拼接，数字按十进制原样写出。覆盖负的 resolution
// 和 32 位放不下的 fileSize / modifiedTime。
TEST(WaveformCacheKey, JoinsFiveFieldsWithPipes) {
    EXPECT_EQ(MakeKey("E:\\OST\\album\\01 - track.flac", 0, 1000, 41234567ULL, 1726900000ULL),
              "E:\\OST\\album\\01 - track.flac|0|1000|41234567|1726900000");
    EXPECT_EQ(MakeKey("E:\\音乐\\专辑\\第一首.flac", 3, 2048, 0ULL, 0ULL),
              "E:\\音乐\\专辑\\第一首.flac|3|2048|0|0");
    EXPECT_EQ(MakeKey("", 4294967295u, -1, 18446744073709551615ULL, 18446744073709551615ULL),
              "|4294967295|-1|18446744073709551615|18446744073709551615");
    EXPECT_EQ(MakeKey("file://E:\\a.mp3", 1, 0, 4294967296ULL, 4294967297ULL),
              "file://E:\\a.mp3|1|0|4294967296|4294967297");
}

// 400 个汉字、UTF-8 超过 1 KB 的路径：键里完整带着路径，不截断、不为空。
// 判据要的是「至少 300 个汉字且超过 1 KB」，汉字在 UTF-8 下 3 字节，
// 300 个只有 900 字节，取 400 个才同时满足两条。
TEST(WaveformCacheKey, LongUtf8PathSurvivesIntact) {
    const std::string longName = Repeat("曲", 400);
    ASSERT_EQ(longName.size(), 1200u) << "汉字应为 3 字节 UTF-8，否则源文件编码不对";
    const std::string path = "E:\\音乐\\" + longName + ".flac";
    ASSERT_GT(path.size(), 1024u);

    const std::string key = MakeKey(path, 0, 1000, 123ULL, 456ULL);
    EXPECT_FALSE(key.empty());
    EXPECT_EQ(key.compare(0, path.size(), path), 0);
    EXPECT_EQ(key, path + "|0|1000|123|456");
}

// 只差末字的两条长路径必须得到不同的键——旧实现在这里要么崩、要么两者都是空串。
TEST(WaveformCacheKey, LongPathsDifferingInLastCharGetDistinctKeys) {
    const std::string stem = "E:\\音乐\\" + Repeat("曲", 399);
    const std::string a = stem + "甲.flac";
    const std::string b = stem + "乙.flac";
    ASSERT_GT(a.size(), 1024u);

    EXPECT_NE(MakeKey(a, 0, 1000, 1ULL, 2ULL), MakeKey(b, 0, 1000, 1ULL, 2ULL));
}

// 其余字段各自参与区分：任一不同，键就不同。
TEST(WaveformCacheKey, EveryFieldParticipates) {
    const std::string path = "E:\\OST\\a.flac";
    const std::string base = MakeKey(path, 0, 1000, 10ULL, 20ULL);
    EXPECT_NE(base, MakeKey(path, 1, 1000, 10ULL, 20ULL));
    EXPECT_NE(base, MakeKey(path, 0, 1001, 10ULL, 20ULL));
    EXPECT_NE(base, MakeKey(path, 0, 1000, 11ULL, 20ULL));
    EXPECT_NE(base, MakeKey(path, 0, 1000, 10ULL, 21ULL));
}
