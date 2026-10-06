// Entries that path validation skipped under skipInvalid are removed from the params the
// handler receives, so a rejected path never reaches it.
#include "pch.h"
#include "api/SkippedPathFilter.h"

using json = nlohmann::json;

TEST(SkippedPathFilter, NothingSkippedLeavesParamsAsTheyWere) {
    const json params = {{"paths", {"a", "b"}}, {"startIndex", 1}};
    EXPECT_EQ(api::WithoutSkippedEntries(params, {}), params);
    EXPECT_EQ(api::WithoutSkippedEntries(params, {{"paths", {}}}), params);
}

TEST(SkippedPathFilter, RemovesTheSkippedIndicesAndKeepsTheRestInOrder) {
    const json params = {{"paths", {"ok1", "denied", "ok2", "denied2"}}, {"startIndex", 1}, {"clear", true}};
    const json out = api::WithoutSkippedEntries(params, {{"paths", {1, 3}}});
    EXPECT_EQ(out["paths"], json::array({"ok1", "ok2"}));
    EXPECT_EQ(out["startIndex"], 1);
    EXPECT_EQ(out["clear"], true);
    EXPECT_EQ(params["paths"].size(), 4u) << "the input is not modified";
}

TEST(SkippedPathFilter, SkippingEveryEntryLeavesAnEmptyArray) {
    const json params = {{"paths", {"x", "y"}}};
    EXPECT_EQ(api::WithoutSkippedEntries(params, {{"paths", {0, 1}}})["paths"], json::array());
}

TEST(SkippedPathFilter, NonStringEntriesAreRemovedToo) {
    const json params = {{"paths", {"ok", 42, nullptr}}};
    EXPECT_EQ(api::WithoutSkippedEntries(params, {{"paths", {1, 2}}})["paths"], json::array({"ok"}));
}

TEST(SkippedPathFilter, EachKeyIsFilteredOnItsOwn) {
    const json params = {{"a", {"1", "2", "3"}}, {"b", {"x", "y"}}};
    const json out = api::WithoutSkippedEntries(params, {{"a", {0}}, {"b", {1}}});
    EXPECT_EQ(out["a"], json::array({"2", "3"}));
    EXPECT_EQ(out["b"], json::array({"x"}));
}

TEST(SkippedPathFilter, AMissingOrNonArrayKeyIsLeftAlone) {
    const json params = {{"paths", "not-an-array"}};
    EXPECT_EQ(api::WithoutSkippedEntries(params, {{"paths", {0}}, {"absent", {0}}}), params);
}
