// test_group_run_plan.cpp - 播放列表分组游程的纯值计算
// （docs/playlist-windowing/SPEC.md §4.2 / §5.2 / §10）。
//
// fb2k 侧的那一半——编译 Title Formatting 模式、分批取 info、逐行求键——不在这里，
// 它需要 SDK，本测试工程从不链接 SDK。这里覆盖的是喂进键之后的全部语义：游程合并、
// ASCII 折叠、两级嵌套的绝对索引、批边界的快照/回滚，以及 patterns 的错误矩阵。
#include "pch.h"
#include "../src/api/GroupRunPlan.h"

using fb2k_group_runs::FoldAscii;
using fb2k_group_runs::GroupRunAccumulator;
using fb2k_group_runs::kMaxPatterns;
using fb2k_group_runs::kNoPatternIndex;
using fb2k_group_runs::ParseGroupRunPatterns;
using fb2k_group_runs::PatternSelection;

namespace {

// 一级模式：按顺序喂主键，返回闭合后的累加器。
GroupRunAccumulator RunSingleLevel(const std::vector<std::string>& keys) {
    GroupRunAccumulator acc(false);
    for (size_t i = 0; i < keys.size(); i++) {
        acc.Push(i, keys[i], "");
    }
    acc.Finish();
    return acc;
}

// 两级模式：主键与子键一一对应。
GroupRunAccumulator RunTwoLevel(const std::vector<std::string>& primary,
                                const std::vector<std::string>& sub) {
    GroupRunAccumulator acc(true);
    for (size_t i = 0; i < primary.size(); i++) {
        acc.Push(i, primary[i], sub[i]);
    }
    acc.Finish();
    return acc;
}

// §10 的游程完整性：runs[0].start == 0、相邻首尾相接、count 之和等于 total。
void ExpectRunsTileTable(const GroupRunAccumulator& acc, size_t total) {
    const auto& parents = acc.parents();
    if (total == 0) {
        EXPECT_TRUE(parents.empty());
        return;
    }
    ASSERT_FALSE(parents.empty());
    EXPECT_EQ(parents.front().start, 0u);
    size_t sum = 0;
    for (size_t i = 0; i < parents.size(); i++) {
        EXPECT_GT(parents[i].count, 0u) << "空游程不应存在，下标 " << i;
        EXPECT_EQ(parents[i].start, sum) << "游程 " << i << " 与前一条不首尾相接";
        sum += parents[i].count;
    }
    EXPECT_EQ(sum, total);
}

// 两级：每个 sub 覆盖其父游程且首尾相接，start 是全表绝对索引。
void ExpectSubsTileParents(const GroupRunAccumulator& acc) {
    const auto& parents = acc.parents();
    const auto& subs = acc.subs();
    for (const auto& parent : parents) {
        ASSERT_LE(parent.subBegin, parent.subEnd);
        ASSERT_LE(parent.subEnd, subs.size());
        size_t cursor = parent.start;
        for (size_t s = parent.subBegin; s < parent.subEnd; s++) {
            EXPECT_EQ(subs[s].start, cursor) << "子游程未与父游程内前一条首尾相接";
            EXPECT_GT(subs[s].count, 0u);
            cursor += subs[s].count;
        }
        EXPECT_EQ(cursor, parent.start + parent.count) << "子游程未覆盖整个父游程";
    }
}

}  // namespace

// ============ patterns 解析（错误矩阵，SPEC §4.2） ============

TEST(GroupRunPatterns, AcceptsOneOrTwoPatterns) {
    const auto one = ParseGroupRunPatterns(nlohmann::json{{"patterns", {"%album%"}}});
    EXPECT_TRUE(one.valid);
    ASSERT_EQ(one.patterns.size(), 1u);
    EXPECT_EQ(one.patterns[0], "%album%");

    const auto two =
        ParseGroupRunPatterns(nlohmann::json{{"patterns", {"%album artist%", "%discnumber%"}}});
    EXPECT_TRUE(two.valid);
    ASSERT_EQ(two.patterns.size(), kMaxPatterns);
    EXPECT_EQ(two.patterns[1], "%discnumber%");
}

TEST(GroupRunPatterns, RejectsMissingKey) {
    const auto sel = ParseGroupRunPatterns(nlohmann::json::object());
    EXPECT_FALSE(sel.valid);
    EXPECT_FALSE(sel.errorMessage.empty());
    EXPECT_EQ(sel.errorIndex, kNoPatternIndex);
}

TEST(GroupRunPatterns, RejectsNonArrayIncludingExplicitNull) {
    for (const nlohmann::json params : {nlohmann::json{{"patterns", nullptr}},
                                        nlohmann::json{{"patterns", "%album%"}},
                                        nlohmann::json{{"patterns", 7}}}) {
        const auto sel = ParseGroupRunPatterns(params);
        EXPECT_FALSE(sel.valid) << params.dump();
        EXPECT_EQ(sel.errorIndex, kNoPatternIndex);
    }
}

TEST(GroupRunPatterns, RejectsWrongLength) {
    const auto empty = ParseGroupRunPatterns(nlohmann::json{{"patterns", nlohmann::json::array()}});
    EXPECT_FALSE(empty.valid);
    EXPECT_EQ(empty.errorIndex, kNoPatternIndex);

    const auto tooMany = ParseGroupRunPatterns(nlohmann::json{{"patterns", {"a", "b", "c"}}});
    EXPECT_FALSE(tooMany.valid);
    EXPECT_EQ(tooMany.errorIndex, kNoPatternIndex);
}

TEST(GroupRunPatterns, ReportsOffendingIndexForNonStringOrEmpty) {
    const auto nonString = ParseGroupRunPatterns(nlohmann::json{{"patterns", {"%album%", 3}}});
    EXPECT_FALSE(nonString.valid);
    EXPECT_EQ(nonString.errorIndex, 1u);
    EXPECT_TRUE(nonString.patterns.empty()) << "判负时不得留下半份 patterns";

    const auto emptyString = ParseGroupRunPatterns(nlohmann::json{{"patterns", {""}}});
    EXPECT_FALSE(emptyString.valid);
    EXPECT_EQ(emptyString.errorIndex, 0u);
}

// ============ ASCII 折叠（DEC-5） ============

TEST(GroupRunFold, FoldsAsciiLowercaseOnly) {
    EXPECT_EQ(FoldAscii("abcXYZ"), "ABCXYZ");
    EXPECT_EQ(FoldAscii("a1-z_"), "A1-Z_");
}

TEST(GroupRunFold, LeavesNonAsciiBytesUntouched) {
    // UTF-8 多字节序列每个字节恒 >= 0x80，ASCII 折叠不会修改这些字节（DEC-5）：
    // 非 ASCII 的大小写差异不并组。
    const std::string cyrillic = "\xd0\xb0\xd0\xb1";  // "аб"
    EXPECT_EQ(FoldAscii(cyrillic), cyrillic);
    const std::string mixed = "\xe8\xaa\x98 mix";  // "誘 mix"
    EXPECT_EQ(FoldAscii(mixed), "\xe8\xaa\x98 MIX");
}

// ============ 一级游程 ============

TEST(GroupRunAccumulatorTest, EmptyInputProducesNoRuns) {
    const auto acc = RunSingleLevel({});
    EXPECT_TRUE(acc.parents().empty());
    ExpectRunsTileTable(acc, 0);
}

TEST(GroupRunAccumulatorTest, MergesAdjacentEqualKeysAndTilesTable) {
    const auto acc = RunSingleLevel({"A", "A", "A", "B", "C", "C"});
    ExpectRunsTileTable(acc, 6);

    const auto& runs = acc.parents();
    ASSERT_EQ(runs.size(), 3u);
    EXPECT_EQ(runs[0].key, "A");
    EXPECT_EQ(runs[0].start, 0u);
    EXPECT_EQ(runs[0].count, 3u);
    EXPECT_EQ(runs[1].key, "B");
    EXPECT_EQ(runs[1].start, 3u);
    EXPECT_EQ(runs[1].count, 1u);
    EXPECT_EQ(runs[2].start, 4u);
    EXPECT_EQ(runs[2].count, 2u);
}

TEST(GroupRunAccumulatorTest, DoesNotMergeNonAdjacentEqualKeys) {
    // 顺序游程，不排序（SPEC §2.2）：同名但不相邻的行各成一组。
    const auto acc = RunSingleLevel({"A", "B", "A"});
    ASSERT_EQ(acc.parents().size(), 3u);
    ExpectRunsTileTable(acc, 3);
}

TEST(GroupRunAccumulatorTest, MergesCaseInsensitivelyKeepingFirstSpelling) {
    // §10 钉的判据：abc / ABC 相邻合并为一组，key 取首项原文。
    const auto acc = RunSingleLevel({"abc", "ABC", "AbC"});
    ASSERT_EQ(acc.parents().size(), 1u);
    EXPECT_EQ(acc.parents()[0].key, "abc");
    EXPECT_EQ(acc.parents()[0].count, 3u);
}

TEST(GroupRunAccumulatorTest, EmptyKeyIsAnOrdinaryKey) {
    // DEC-5：空串与 "?" 不特殊处理，模式可用 $if2 指定缺省值。
    const auto acc = RunSingleLevel({"", "", "X"});
    ASSERT_EQ(acc.parents().size(), 2u);
    EXPECT_EQ(acc.parents()[0].key, "");
    EXPECT_EQ(acc.parents()[0].count, 2u);
    ExpectRunsTileTable(acc, 3);
}

TEST(GroupRunAccumulatorTest, SingleLevelEmitsNoSubRuns) {
    const auto acc = RunSingleLevel({"A", "B"});
    EXPECT_TRUE(acc.subs().empty());
    EXPECT_FALSE(acc.twoLevel());
}

// ============ 两级游程 ============

TEST(GroupRunAccumulatorTest, SubRunStartsAreAbsoluteNotParentRelative) {
    // SPEC §4.2 的鉴别用例：第二个父游程的首条 sub，start 必须是父游程起点而不是 0。
    // 第一个父游程的绝对与相对同解，没有鉴别力。
    const std::vector<std::string> primary = {"Nujabes", "Nujabes", "Nujabes",
                                              "Portishead", "Portishead"};
    const std::vector<std::string> sub = {"Disc 1", "Disc 1", "Disc 2", "Disc 1", "Disc 1"};
    const auto acc = RunTwoLevel(primary, sub);

    ExpectRunsTileTable(acc, 5);
    ExpectSubsTileParents(acc);

    const auto& parents = acc.parents();
    const auto& subs = acc.subs();
    ASSERT_EQ(parents.size(), 2u);

    ASSERT_EQ(parents[0].subEnd - parents[0].subBegin, 2u);
    EXPECT_EQ(subs[parents[0].subBegin].start, 0u);
    EXPECT_EQ(subs[parents[0].subBegin].count, 2u);
    EXPECT_EQ(subs[parents[0].subBegin + 1].start, 2u);

    ASSERT_EQ(parents[1].subEnd - parents[1].subBegin, 1u);
    EXPECT_EQ(subs[parents[1].subBegin].start, 3u) << "第二个父游程的 sub.start 必须是 3，不是 0";
    EXPECT_EQ(subs[parents[1].subBegin].count, 2u);
}

TEST(GroupRunAccumulatorTest, SubRunRestartsAcrossParentBoundaryEvenIfKeyUnchanged) {
    // 子游程必须包含在自己的父游程内：父游程一换，同名子键也要重新开一组。
    const auto acc = RunTwoLevel({"A", "B"}, {"Disc 1", "Disc 1"});
    ASSERT_EQ(acc.parents().size(), 2u);
    EXPECT_EQ(acc.subs().size(), 2u);
    ExpectSubsTileParents(acc);
}

TEST(GroupRunAccumulatorTest, SubRunsAlsoMergeCaseInsensitively) {
    const auto acc = RunTwoLevel({"A", "A"}, {"disc 1", "DISC 1"});
    ASSERT_EQ(acc.parents().size(), 1u);
    ASSERT_EQ(acc.subs().size(), 1u);
    EXPECT_EQ(acc.subs()[0].key, "disc 1");
    EXPECT_EQ(acc.subs()[0].count, 2u);
}

TEST(GroupRunAccumulatorTest, WorstCaseOneRunPerRowStillTiles) {
    // SPEC §4.2 登记的上界：模式产出近乎每行一组时 runs 长度等于 total。
    std::vector<std::string> keys;
    for (size_t i = 0; i < 64; i++) {
        keys.push_back(std::to_string(i));
    }
    const auto acc = RunSingleLevel(keys);
    EXPECT_EQ(acc.parents().size(), 64u);
    ExpectRunsTileTable(acc, 64);
}

// ============ 批边界快照与回滚（SPEC §5.2） ============

TEST(GroupRunSnapshot, RollbackRestoresStateSoRetryDoesNotDoubleCount) {
    // 失败批次重算前必须回滚，否则已并入的行会被二次合并，count 之和不再等于 total。
    const std::vector<std::string> keys = {"A", "A", "B", "B", "C"};

    GroupRunAccumulator acc(false);
    acc.Push(0, keys[0], "");
    acc.Push(1, keys[1], "");

    const auto snapshot = acc.Capture();
    // 「失败的一批」：喂进去再回滚
    acc.Push(2, keys[2], "");
    acc.Push(3, keys[3], "");
    acc.Restore(snapshot);

    // 重算该批，再跑完剩下的
    acc.Push(2, keys[2], "");
    acc.Push(3, keys[3], "");
    acc.Push(4, keys[4], "");
    acc.Finish();

    ExpectRunsTileTable(acc, 5);
    ASSERT_EQ(acc.parents().size(), 3u);
    EXPECT_EQ(acc.parents()[0].count, 2u);
    EXPECT_EQ(acc.parents()[1].count, 2u);
    EXPECT_EQ(acc.parents()[2].count, 1u);
}

TEST(GroupRunSnapshot, RollbackAcrossAClosedParentRun) {
    // 快照点落在游程中间、失败批里父游程发生了闭合：回滚要把已闭合的那条也撤掉。
    GroupRunAccumulator acc(false);
    acc.Push(0, "A", "");

    const auto snapshot = acc.Capture();
    acc.Push(1, "B", "");  // 闭合 A，开 B
    acc.Push(2, "C", "");  // 闭合 B，开 C
    EXPECT_EQ(acc.parents().size(), 2u);

    acc.Restore(snapshot);
    EXPECT_TRUE(acc.parents().empty()) << "回滚必须撤掉失败批里闭合的父游程";

    acc.Push(1, "B", "");
    acc.Push(2, "C", "");
    acc.Finish();

    ExpectRunsTileTable(acc, 3);
    ASSERT_EQ(acc.parents().size(), 3u);
    EXPECT_EQ(acc.parents()[0].key, "A");
}

TEST(GroupRunSnapshot, RollbackAlsoUnwindsSubRuns) {
    GroupRunAccumulator acc(true);
    acc.Push(0, "A", "Disc 1");

    const auto snapshot = acc.Capture();
    acc.Push(1, "A", "Disc 2");
    acc.Push(2, "B", "Disc 1");
    EXPECT_FALSE(acc.subs().empty());

    acc.Restore(snapshot);
    EXPECT_TRUE(acc.parents().empty());
    EXPECT_TRUE(acc.subs().empty()) << "回滚必须撤掉失败批里闭合的子游程";

    acc.Push(1, "A", "Disc 2");
    acc.Push(2, "B", "Disc 1");
    acc.Finish();

    ExpectRunsTileTable(acc, 3);
    ExpectSubsTileParents(acc);
    ASSERT_EQ(acc.parents().size(), 2u);
    EXPECT_EQ(acc.parents()[0].count, 2u);
    EXPECT_EQ(acc.subs().size(), 3u);
}

TEST(GroupRunSnapshot, CaptureOnFreshAccumulatorRollsBackToEmpty) {
    GroupRunAccumulator acc(false);
    const auto snapshot = acc.Capture();
    acc.Push(0, "A", "");
    acc.Push(1, "B", "");
    acc.Restore(snapshot);

    acc.Push(0, "A", "");
    acc.Finish();
    ASSERT_EQ(acc.parents().size(), 1u);
    EXPECT_EQ(acc.parents()[0].start, 0u);
    EXPECT_EQ(acc.parents()[0].count, 1u);
}

// ============ 直写序列化（SPEC §4.2 的形状） ============

TEST(GroupRunJson, SingleLevelShapeHasNoSubKey) {
    const auto acc = RunSingleLevel({"A", "A", "B"});
    std::string out;
    fb2k_group_runs::WriteGroupRunsJson(out, 2, 3, acc);

    const nlohmann::json parsed = nlohmann::json::parse(out);
    EXPECT_EQ(parsed["success"], true);
    EXPECT_EQ(parsed["playlist"], 2);
    EXPECT_EQ(parsed["total"], 3);
    ASSERT_EQ(parsed["runs"].size(), 2u);
    EXPECT_EQ(parsed["runs"][0]["start"], 0);
    EXPECT_EQ(parsed["runs"][0]["count"], 2);
    EXPECT_EQ(parsed["runs"][0]["key"], "A");
    EXPECT_FALSE(parsed["runs"][0].contains("sub")) << "一级模式不得出 sub 键";
}

TEST(GroupRunJson, TwoLevelShapeCarriesAbsoluteSubStarts) {
    const auto acc = RunTwoLevel({"A", "A", "B"}, {"d1", "d2", "d1"});
    std::string out;
    fb2k_group_runs::WriteGroupRunsJson(out, 0, 3, acc);

    const nlohmann::json parsed = nlohmann::json::parse(out);
    ASSERT_EQ(parsed["runs"].size(), 2u);
    ASSERT_EQ(parsed["runs"][0]["sub"].size(), 2u);
    EXPECT_EQ(parsed["runs"][0]["sub"][1]["start"], 1);
    ASSERT_EQ(parsed["runs"][1]["sub"].size(), 1u);
    EXPECT_EQ(parsed["runs"][1]["sub"][0]["start"], 2) << "sub.start 是全表绝对索引";
}

TEST(GroupRunJson, EmptyRunsSerializeAsEmptyArray) {
    const auto acc = RunSingleLevel({});
    std::string out;
    fb2k_group_runs::WriteGroupRunsJson(out, 0, 0, acc);

    const nlohmann::json parsed = nlohmann::json::parse(out);
    EXPECT_EQ(parsed["total"], 0);
    EXPECT_TRUE(parsed["runs"].is_array());
    EXPECT_TRUE(parsed["runs"].empty());
}

TEST(GroupRunJson, EscapesKeysThatNeedIt) {
    // 键是用户模式产出的任意文本，直写必须走同一份转义。
    const auto acc = RunSingleLevel({"say \"hi\"\\", "say \"hi\"\\"});
    std::string out;
    fb2k_group_runs::WriteGroupRunsJson(out, 0, 2, acc);

    const nlohmann::json parsed = nlohmann::json::parse(out);
    ASSERT_EQ(parsed["runs"].size(), 1u);
    EXPECT_EQ(parsed["runs"][0]["key"], "say \"hi\"\\");
}
