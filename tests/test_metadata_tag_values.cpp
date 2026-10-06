#include "pch.h"
#include "api/MetadataTagValues.h"

namespace {
using nlohmann::json;
using metadata_tags::Action;
using metadata_tags::ParseTagValue;

TEST(MetadataTagValues, NullRemovesField) {
    const auto parsed = ParseTagValue(nullptr);
    EXPECT_EQ(parsed.action, Action::Remove);
    EXPECT_TRUE(parsed.values.empty());
    EXPECT_TRUE(parsed.appliedValue.is_null());
    EXPECT_FALSE(parsed.invalidIndex.has_value());
}

TEST(MetadataTagValues, EmptyStringRemovesField) {
    const auto parsed = ParseTagValue("");
    EXPECT_EQ(parsed.action, Action::Remove);
    EXPECT_TRUE(parsed.values.empty());
    EXPECT_TRUE(parsed.appliedValue.is_null());
}

TEST(MetadataTagValues, StringPreservesOriginalTextAndScalarReceipt) {
    const std::string text = "  艺人甲; 艺人乙, Artist C\t";
    const auto parsed = ParseTagValue(text);
    EXPECT_EQ(parsed.action, Action::Set);
    EXPECT_EQ(parsed.values, (std::vector<std::string>{text}));
    ASSERT_TRUE(parsed.appliedValue.is_string());
    EXPECT_EQ(parsed.appliedValue.dump(), json(text).dump());
    EXPECT_FALSE(parsed.invalidIndex.has_value());
}

TEST(MetadataTagValues, WhitespaceStringIsNotDeletion) {
    const auto parsed = ParseTagValue(" \t ");
    EXPECT_EQ(parsed.action, Action::Set);
    EXPECT_EQ(parsed.values, (std::vector<std::string>{" \t "}));
    EXPECT_EQ(parsed.appliedValue.dump(), json(" \t ").dump());
}

TEST(MetadataTagValues, IntegerKeepsExactDecimalTextAndNumericReceipt) {
    const std::vector<std::string> numbers = {
        "0", "-2147483649", "9223372036854775807", "18446744073709551615"
    };
    for (const auto& text : numbers) {
        SCOPED_TRACE(text);
        const json value = json::parse(text);
        const auto parsed = ParseTagValue(value);
        EXPECT_EQ(parsed.action, Action::Set);
        EXPECT_EQ(parsed.values, (std::vector<std::string>{text}));
        EXPECT_TRUE(parsed.appliedValue.is_number_integer());
        EXPECT_EQ(parsed.appliedValue.dump(), value.dump());
    }
}

TEST(MetadataTagValues, FloatKeepsExistingSixDecimalTextAndNumericReceipt) {
    const auto parsed = ParseTagValue(1.25);
    EXPECT_EQ(parsed.action, Action::Set);
    EXPECT_EQ(parsed.values, (std::vector<std::string>{"1.250000"}));
    EXPECT_TRUE(parsed.appliedValue.is_number_float());
    EXPECT_EQ(parsed.appliedValue.dump(), json(1.25).dump());

    const auto parsedFraction = ParseTagValue(0.0000001);
    EXPECT_EQ(parsedFraction.values, (std::vector<std::string>{"0.000000"}));
    EXPECT_EQ(parsedFraction.appliedValue.dump(), json(0.0000001).dump());
}

TEST(MetadataTagValues, BooleanAndObjectRemainIgnored) {
    const std::vector<json> values = {true, false, json::object(), json{{"value", "Artist"}}};
    for (const auto& value : values) {
        SCOPED_TRACE(value.dump());
        const auto parsed = ParseTagValue(value);
        EXPECT_EQ(parsed.action, Action::Ignore);
        EXPECT_TRUE(parsed.values.empty());
        EXPECT_TRUE(parsed.appliedValue.is_null());
        EXPECT_FALSE(parsed.invalidIndex.has_value());
    }
}

TEST(MetadataTagValues, NonemptyArraySetsAllValuesAndKeepsArrayReceipt) {
    const auto parsed = ParseTagValue(json::array({"Artist A", "Artist B"}));
    EXPECT_EQ(parsed.action, Action::Set);
    EXPECT_EQ(parsed.values, (std::vector<std::string>{"Artist A", "Artist B"}));
    EXPECT_EQ(parsed.appliedValue.dump(), json::array({"Artist A", "Artist B"}).dump());
    EXPECT_FALSE(parsed.invalidIndex.has_value());
}

TEST(MetadataTagValues, SingleElementArrayKeepsArrayReceipt) {
    const auto parsed = ParseTagValue(json::array({"Artist A"}));
    EXPECT_EQ(parsed.action, Action::Set);
    EXPECT_EQ(parsed.values, (std::vector<std::string>{"Artist A"}));
    EXPECT_EQ(parsed.appliedValue.dump(), json::array({"Artist A"}).dump());
}

TEST(MetadataTagValues, ArrayKeepsOrderDuplicatesUnicodeDelimitersAndWhitespace) {
    const auto parsed = ParseTagValue(json::array({"乙", "甲", "乙", "A;B", "C,D", " \t "}));
    EXPECT_EQ(parsed.action, Action::Set);
    EXPECT_EQ(parsed.values, (std::vector<std::string>{"乙", "甲", "乙", "A;B", "C,D", " \t "}));
    EXPECT_EQ(parsed.appliedValue.dump(), json::array({"乙", "甲", "乙", "A;B", "C,D", " \t "}).dump());
}

TEST(MetadataTagValues, EmptyArrayRemovesFieldWithNullReceipt) {
    const auto parsed = ParseTagValue(json::array());
    EXPECT_EQ(parsed.action, Action::Remove);
    EXPECT_TRUE(parsed.values.empty());
    EXPECT_TRUE(parsed.appliedValue.is_null());
    EXPECT_FALSE(parsed.invalidIndex.has_value());
}

TEST(MetadataTagValues, ArrayRejectsNonStringAtItsZeroBasedIndex) {
    const std::vector<json> invalidValues = {
        nullptr, true, false, 7, 1.25, json::object(), json{{"name", "Artist"}}, json::array({"Artist"})
    };
    for (const auto& value : invalidValues) {
        SCOPED_TRACE(value.dump());
        const auto parsed = ParseTagValue(json::array({"Valid", value, "Later"}));
        EXPECT_EQ(parsed.action, Action::Invalid);
        ASSERT_TRUE(parsed.invalidIndex.has_value());
        EXPECT_EQ(*parsed.invalidIndex, 1u);
        EXPECT_TRUE(parsed.values.empty());
        EXPECT_TRUE(parsed.appliedValue.is_null());
    }
}

TEST(MetadataTagValues, ArrayRejectsEmptyStringWithoutPartialValues) {
    const auto parsed = ParseTagValue(json::array({"Valid", "Also valid", ""}));
    EXPECT_EQ(parsed.action, Action::Invalid);
    ASSERT_TRUE(parsed.invalidIndex.has_value());
    EXPECT_EQ(*parsed.invalidIndex, 2u);
    EXPECT_TRUE(parsed.values.empty());
    EXPECT_TRUE(parsed.appliedValue.is_null());
}

TEST(MetadataTagValues, ArrayRejectsEmbeddedNulWithoutPartialValues) {
    const std::string text("A\0B", 3);
    const auto parsed = ParseTagValue(json::array({"Valid", text}));
    EXPECT_EQ(parsed.action, Action::Invalid);
    ASSERT_TRUE(parsed.invalidIndex.has_value());
    EXPECT_EQ(*parsed.invalidIndex, 1u);
    EXPECT_TRUE(parsed.values.empty());
    EXPECT_TRUE(parsed.appliedValue.is_null());
}

TEST(MetadataTagValues, ArrayReportsFirstInvalidElementIncludingIndexZero) {
    const auto parsed = ParseTagValue(json::array({false, "", nullptr}));
    EXPECT_EQ(parsed.action, Action::Invalid);
    ASSERT_TRUE(parsed.invalidIndex.has_value());
    EXPECT_EQ(*parsed.invalidIndex, 0u);
    EXPECT_TRUE(parsed.values.empty());
    EXPECT_TRUE(parsed.appliedValue.is_null());
}

TEST(MetadataTagValues, ScalarAndArrayResultsOwnTheirStringsAndReceipts) {
    const auto scalar = [] {
        json input = "Original";
        auto parsed = ParseTagValue(input);
        input = "Changed";
        return parsed;
    }();
    EXPECT_EQ(scalar.values, (std::vector<std::string>{"Original"}));
    EXPECT_EQ(scalar.appliedValue.dump(), json("Original").dump());

    auto array = [] {
        json input = json::array({"甲", "乙"});
        auto parsed = ParseTagValue(input);
        input[0] = "Changed";
        return parsed;
    }();
    EXPECT_EQ(array.values, (std::vector<std::string>{"甲", "乙"}));
    EXPECT_EQ(array.appliedValue.dump(), json::array({"甲", "乙"}).dump());
    ASSERT_FALSE(array.values.empty());
    array.values[0] = "Different";
    EXPECT_EQ(array.appliedValue.dump(), json::array({"甲", "乙"}).dump());
}

} // namespace
