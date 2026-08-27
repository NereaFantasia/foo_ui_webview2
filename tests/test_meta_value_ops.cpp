// test_meta_value_ops.cpp — 多值标签纯值层的语义锁
//
// JoinMetaValues / DedupPreservingOrder 是多值 tag 显示串与身份键的唯一拼接
// 与去重实现（MetaAccess.h 的多值分支强制走这里），两者语义刻意不同且各有
// 对外契约依赖：Join 不去重不丢空 —— 显示串必须与 metadata.read 的全值数组
// join 后逐字节一致；Dedup 保序去重并丢空 —— 身份键计数与聚合 API 既有的
// strlen>0 过滤同口径。此处把这组差异锁死，防止后续"顺手统一"。
#include "pch.h"
#include "api/MetaValueOps.h"

namespace meta_value_ops_test {

TEST(JoinMetaValues, EmptyListReturnsEmptyString) {
    EXPECT_EQ(JoinMetaValues({}, ", "), "");
}

TEST(JoinMetaValues, SingleElementHasNoSeparator) {
    EXPECT_EQ(JoinMetaValues({"Artist A"}, ", "), "Artist A");
}

TEST(JoinMetaValues, MultipleElementsJoinInOriginalOrder) {
    EXPECT_EQ(JoinMetaValues({"B", "A", "C"}, ", "), "B, A, C");
}

TEST(JoinMetaValues, SeparatorAppearsOnlyBetweenElements) {
    EXPECT_EQ(JoinMetaValues({"A", "B"}, " / "), "A / B");
}

TEST(JoinMetaValues, KeepsDuplicates) {
    // 契约：不去重 —— metadata.read 的数组同样不去重，两侧 join 后须逐字节一致
    EXPECT_EQ(JoinMetaValues({"A", "A"}, ", "), "A, A");
}

TEST(JoinMetaValues, KeepsEmptyValues) {
    // 契约：不丢空值，空元素在拼接结果里保留位置
    EXPECT_EQ(JoinMetaValues({"", "B"}, ", "), ", B");
    EXPECT_EQ(JoinMetaValues({"A", ""}, ", "), "A, ");
}

TEST(DedupPreservingOrder, RemovesDuplicatesKeepingFirstOccurrence) {
    EXPECT_EQ(DedupPreservingOrder({"B", "A", "B", "C", "A"}),
              (std::vector<std::string>{"B", "A", "C"}));
}

TEST(DedupPreservingOrder, DropsEmptyValues) {
    EXPECT_EQ(DedupPreservingOrder({"", "A", "", "B"}),
              (std::vector<std::string>{"A", "B"}));
}

TEST(DedupPreservingOrder, AllEmptyYieldsEmptyList) {
    EXPECT_EQ(DedupPreservingOrder({"", ""}), std::vector<std::string>{});
}

TEST(DedupPreservingOrder, ValuesCompareByteExact) {
    // fb2k 的字段名查找大小写不敏感，但字段值是字节精确的，大小写不同不算重复
    EXPECT_EQ(DedupPreservingOrder({"a", "A"}),
              (std::vector<std::string>{"a", "A"}));
}

}  // namespace meta_value_ops_test
