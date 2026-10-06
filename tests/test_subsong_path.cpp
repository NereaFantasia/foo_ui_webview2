// test_subsong_path.cpp - fb2k_paths 的子曲目路径拆分与拼接（utils/SubsongPath.h）
//
// 宿主里拆 "|subsong:N"、拼 "|subsong:N" 都经这几个函数，这里直接测产品函数。
// 读不出序号时记控制台、按拆出的路径建 handle 需要 SDK，不在本文件覆盖。
#include "pch.h"
#include <string>

#include "utils/SubsongPath.h"

using fb2k_paths::JoinSubsongPath;
using fb2k_paths::SplitLegacyTrackPath;
using fb2k_paths::SplitSubsongPath;

// ---- SplitSubsongPath ----

TEST(SubsongPath, NoMarkerKeepsWholeInput) {
    const auto r = SplitSubsongPath("D:\\Music\\a.flac");
    EXPECT_EQ(r.path, "D:\\Music\\a.flac");
    EXPECT_EQ(r.subsong, 0u);
    EXPECT_FALSE(r.hasSuffix);
    EXPECT_FALSE(r.suffixValid);
}

TEST(SubsongPath, MarkerWithIndex) {
    const auto r = SplitSubsongPath("D:\\Music\\album.cue|subsong:3");
    EXPECT_EQ(r.path, "D:\\Music\\album.cue");
    EXPECT_EQ(r.subsong, 3u);
    EXPECT_TRUE(r.hasSuffix);
    EXPECT_TRUE(r.suffixValid);
}

// 显式的 0 也是后缀：播放列表与队列据此决定直接建 handle，不走 process_locations。
TEST(SubsongPath, ExplicitZeroIsStillASuffix) {
    const auto r = SplitSubsongPath("D:\\a.flac|subsong:0");
    EXPECT_EQ(r.path, "D:\\a.flac");
    EXPECT_EQ(r.subsong, 0u);
    EXPECT_TRUE(r.hasSuffix);
    EXPECT_TRUE(r.suffixValid);
}

TEST(SubsongPath, UnreadableIndexStillStripsTheSuffix) {
    const auto r = SplitSubsongPath("D:\\a.flac|subsong:abc");
    EXPECT_EQ(r.path, "D:\\a.flac");
    EXPECT_EQ(r.subsong, 0u);
    EXPECT_TRUE(r.hasSuffix);
    EXPECT_FALSE(r.suffixValid);
}

TEST(SubsongPath, EmptyIndexIsUnreadable) {
    const auto r = SplitSubsongPath("D:\\a.flac|subsong:");
    EXPECT_EQ(r.path, "D:\\a.flac");
    EXPECT_EQ(r.subsong, 0u);
    EXPECT_TRUE(r.hasSuffix);
    EXPECT_FALSE(r.suffixValid);
}

TEST(SubsongPath, EmptyInput) {
    const auto r = SplitSubsongPath("");
    EXPECT_EQ(r.path, "");
    EXPECT_EQ(r.subsong, 0u);
    EXPECT_FALSE(r.hasSuffix);
}

TEST(SubsongPath, MarkerAloneGivesEmptyPath) {
    const auto r = SplitSubsongPath("|subsong:5");
    EXPECT_EQ(r.path, "");
    EXPECT_EQ(r.subsong, 5u);
    EXPECT_TRUE(r.hasSuffix);
}

// 截断发生在第一个标记处，后面再有标记也属于后缀。
TEST(SubsongPath, CutsAtTheFirstMarker) {
    const auto r = SplitSubsongPath("D:\\a.flac|subsong:1|subsong:2");
    EXPECT_EQ(r.path, "D:\\a.flac");
    EXPECT_EQ(r.subsong, 1u);
    EXPECT_TRUE(r.suffixValid);
}

// 路径里别处的 '|' 不是标记：压缩包内的曲目只在 "|subsong:" 处截断。
TEST(SubsongPath, OtherPipesBelongToThePath) {
    const auto r = SplitSubsongPath("archive://D:\\a.zip|/t.flac|subsong:2");
    EXPECT_EQ(r.path, "archive://D:\\a.zip|/t.flac");
    EXPECT_EQ(r.subsong, 2u);
}

// 以下几条锁住 std::stoul 的读法，它们是各调用点改用共享函数之前的行为。
TEST(SubsongPath, TrailingTextAfterDigitsIsIgnored) {
    const auto r = SplitSubsongPath("D:\\a.flac|subsong:3abc");
    EXPECT_EQ(r.subsong, 3u);
    EXPECT_TRUE(r.suffixValid);
}

TEST(SubsongPath, LeadingWhitespaceAndPlusAreAccepted) {
    EXPECT_EQ(SplitSubsongPath("a|subsong: 4").subsong, 4u);
    EXPECT_EQ(SplitSubsongPath("a|subsong:+4").subsong, 4u);
}

TEST(SubsongPath, MinusSignWrapsAround) {
    const auto r = SplitSubsongPath("a|subsong:-1");
    EXPECT_EQ(r.subsong, 4294967295u);
    EXPECT_TRUE(r.suffixValid);
}

TEST(SubsongPath, LargestIndexAndOverflow) {
    EXPECT_EQ(SplitSubsongPath("a|subsong:4294967295").subsong, 4294967295u);

    const auto overflow = SplitSubsongPath("a|subsong:4294967296");
    EXPECT_EQ(overflow.path, "a");
    EXPECT_EQ(overflow.subsong, 0u);
    EXPECT_FALSE(overflow.suffixValid);
}

// 非 ASCII 路径按字节原样保留。
TEST(SubsongPath, NonAsciiPathIsKeptByteForByte) {
    const std::string path = "D:\\\xE9\x9F\xB3\xE4\xB9\x90\\\xE4\xB8\x93\xE8\xBE\x91.cue";
    const auto r = SplitSubsongPath(path + "|subsong:7");
    EXPECT_EQ(r.path, path);
    EXPECT_EQ(r.subsong, 7u);
}

// ---- JoinSubsongPath ----

TEST(SubsongPath, JoinAppendsSuffixForNonZeroSubsong) {
    EXPECT_EQ(JoinSubsongPath("D:\\album.cue", 3), "D:\\album.cue|subsong:3");
}

TEST(SubsongPath, JoinLeavesSubsongZeroBare) {
    EXPECT_EQ(JoinSubsongPath("D:\\a.flac", 0), "D:\\a.flac");
}

// 没有路径时不拼出一个只有后缀的键。
TEST(SubsongPath, JoinLeavesEmptyPathEmpty) {
    EXPECT_EQ(JoinSubsongPath("", 2), "");
}

TEST(SubsongPath, JoinThenSplitRoundTrips) {
    for (const uint32_t subsong : {1u, 2u, 99u, 4294967295u}) {
        const auto r = SplitSubsongPath(JoinSubsongPath("D:\\album.cue", subsong));
        EXPECT_EQ(r.path, "D:\\album.cue");
        EXPECT_EQ(r.subsong, subsong);
        EXPECT_TRUE(r.suffixValid);
    }
}

// ---- SplitLegacyTrackPath（metadata.* 与 rating.* 的写法） ----

TEST(SubsongPath, LegacyPipeSuffix) {
    const auto r = SplitLegacyTrackPath("D:\\album.flac|subsong:3", -1);
    EXPECT_EQ(r.path, "D:\\album.flac");
    EXPECT_EQ(r.subsong, 3u);
}

TEST(SubsongPath, LegacyCueIndexWins) {
    const auto r = SplitLegacyTrackPath("D:\\album.flac", 5);
    EXPECT_EQ(r.path, "D:\\album.flac");
    EXPECT_EQ(r.subsong, 5u);
}

TEST(SubsongPath, LegacyCueIndexWinsButSuffixIsStillStripped) {
    const auto r = SplitLegacyTrackPath("D:\\album.flac|subsong:3", 5);
    EXPECT_EQ(r.path, "D:\\album.flac");
    EXPECT_EQ(r.subsong, 5u);
}

TEST(SubsongPath, LegacyCueIndexZeroIsHonoured) {
    const auto r = SplitLegacyTrackPath("D:\\album.flac|subsong:3", 0);
    EXPECT_EQ(r.subsong, 0u);
}

TEST(SubsongPath, LegacyHashIndex) {
    const auto r = SplitLegacyTrackPath("D:\\file.flac#7", -1);
    EXPECT_EQ(r.path, "D:\\file.flac");
    EXPECT_EQ(r.subsong, 7u);
}

// cueIndex 或 "|subsong:N" 生效时不再拆 "#N"。
TEST(SubsongPath, LegacyHashIsKeptWhenCueIndexWins) {
    const auto r = SplitLegacyTrackPath("D:\\file.flac#7", 2);
    EXPECT_EQ(r.path, "D:\\file.flac#7");
    EXPECT_EQ(r.subsong, 2u);
}

TEST(SubsongPath, LegacyHashIsKeptWhenPipeSuffixWins) {
    const auto r = SplitLegacyTrackPath("D:\\a#5.flac|subsong:2", -1);
    EXPECT_EQ(r.path, "D:\\a#5.flac");
    EXPECT_EQ(r.subsong, 2u);
}

// 读不出序号的 "|subsong:" 仍算后缀：剥掉、序号 0，不再往下找 "#N"。
TEST(SubsongPath, LegacyUnreadablePipeSuffixDoesNotFallThroughToHash) {
    const auto r = SplitLegacyTrackPath("D:\\a#5|subsong:x", -1);
    EXPECT_EQ(r.path, "D:\\a#5");
    EXPECT_EQ(r.subsong, 0u);
}

TEST(SubsongPath, LegacyHashNeedsDigitsOnly) {
    EXPECT_EQ(SplitLegacyTrackPath("D:\\a#7.flac", -1).path, "D:\\a#7.flac");
    EXPECT_EQ(SplitLegacyTrackPath("D:\\a#", -1).path, "D:\\a#");
    EXPECT_EQ(SplitLegacyTrackPath("D:\\a#-3", -1).path, "D:\\a#-3");
    EXPECT_EQ(SplitLegacyTrackPath("D:\\a#7.flac", -1).subsong, 0u);
}

// '#' 之后是 UTF-8 多字节字符时按非数字处理，不把字节交给按 int 取值的字符分类函数。
TEST(SubsongPath, LegacyHashFollowedByNonAsciiIsNotAnIndex) {
    const std::string path = "D:\\a#\xE6\xAD\x8C";
    const auto r = SplitLegacyTrackPath(path, -1);
    EXPECT_EQ(r.path, path);
    EXPECT_EQ(r.subsong, 0u);
}

TEST(SubsongPath, LegacyHashBeyondIntRangeIsNotAnIndex) {
    EXPECT_EQ(SplitLegacyTrackPath("D:\\a#2147483647", -1).subsong, 2147483647u);

    const auto r = SplitLegacyTrackPath("D:\\a#2147483648", -1);
    EXPECT_EQ(r.path, "D:\\a#2147483648");
    EXPECT_EQ(r.subsong, 0u);
}

TEST(SubsongPath, LegacyPlainPath) {
    const auto r = SplitLegacyTrackPath("D:\\song.flac", -1);
    EXPECT_EQ(r.path, "D:\\song.flac");
    EXPECT_EQ(r.subsong, 0u);
}
