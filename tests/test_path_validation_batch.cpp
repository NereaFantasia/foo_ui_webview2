#include "pch.h"
#include "compat/fb2k_types.h"
#include "api/PathValidationBatch.h"
#include "api/TypedApi.h"
#include "api/generated/FileSchema.h"
#include "api/generated/PlaybackSchema.h"
#include "api/generated/PlaylistSchema.h"

// 直接验证同步与 deferred 共用的规格转换、校验汇总及参数过滤 helper。
// 校验器替身只提供判定结果；不覆盖 BridgeCore 实际派发、CallerContext 解析或 Win32 路径授权。

using json = nlohmann::json;

namespace {
struct Spec { std::string paramKey; };
struct Verdict {
    bool success = true;
    std::string errorMsg;
    std::vector<size_t> skippedIndices;
    bool shapeError = false;
};
}

TEST(PathValidationBatch, RejectsBeforeDispatchAndPreservesErrorCategory) {
    for (bool shape : {false, true}) {
        unsigned calls = 0;
        auto checked = api::CheckPathSpecs(json{{"path", "denied"}}, std::vector<Spec>{{"path"}, {"later"}},
            [&](const json&, const Spec&) { ++calls; return Verdict{false, "denied", {}, shape}; });
        EXPECT_EQ(calls, 1u);
        ASSERT_TRUE(checked.error.has_value());
        EXPECT_EQ(checked.error->shapeError, shape);
        EXPECT_EQ(checked.error->message, "denied");
    }
}

TEST(PathValidationBatch, AggregatesEverySkippedArrayAndLeavesInputUntouched) {
    const json input = {{"a", {"yes", "no"}}, {"b", {"no", "yes", "no"}}, {"_callerHwnd", 17}};
    auto checked = api::CheckPathSpecs(input, std::vector<Spec>{{"a"}, {"b"}},
        [](const json&, const Spec& s) { return Verdict{true, {}, s.paramKey == "a" ? std::vector<size_t>{1} : std::vector<size_t>{0, 2}, false}; });
    EXPECT_FALSE(checked.error);
    EXPECT_EQ(checked.skippedCount, 3u);
    const auto filtered = api::WithoutSkippedEntries(input, checked.skipped);
    EXPECT_EQ(filtered.at("a"), json::array({"yes"}));
    EXPECT_EQ(filtered.at("b"), json::array({"yes"}));
    EXPECT_EQ(filtered.at("_callerHwnd"), 17);
    EXPECT_EQ(input.at("a").size(), 2u);
}

TEST(PathValidationBatch, AnnotationOnlyAppliesToSuccessfulObjects) {
    json successful = {{"success", true}, {"value", 1}};
    api::AddSkippedPathCount(successful, 2);
    EXPECT_EQ(successful.at("skippedPaths"), 2);
    for (auto result : {json{{"success", false}}, json{{"value", 1}}, json::array({1}), json(nullptr)}) {
        const json original = result;
        api::AddSkippedPathCount(result, 2);
        EXPECT_EQ(result, original);
    }
    json noSkips = {{"success", true}};
    api::AddSkippedPathCount(noSkips, 0);
    EXPECT_FALSE(noSkips.contains("skippedPaths"));
}

TEST(TypedPathSecuritySpecs, ScalarMediaPathKeepsItsAccessLevel) {
    const auto specs = api::PathSecuritySpecsOf<api::playback::PlayPathParams>();

    ASSERT_EQ(specs.size(), 1u);
    EXPECT_EQ(specs[0].paramKey, "path");
    EXPECT_EQ(specs[0].level, SecurityLevel::MediaRead);
    EXPECT_FALSE(specs[0].isArray);
    EXPECT_FALSE(specs[0].skipInvalid);
    EXPECT_TRUE(specs[0].nestedKey.empty());
    EXPECT_FALSE(specs[0].stringElements);
}

TEST(TypedPathSecuritySpecs, StringArrayKeepsSkipInvalid) {
    const auto specs = api::PathSecuritySpecsOf<api::playback::PlayPathsParams>();

    ASSERT_EQ(specs.size(), 1u);
    EXPECT_EQ(specs[0].paramKey, "paths");
    EXPECT_EQ(specs[0].level, SecurityLevel::MediaRead);
    EXPECT_TRUE(specs[0].isArray);
    EXPECT_TRUE(specs[0].skipInvalid);
    EXPECT_TRUE(specs[0].nestedKey.empty());
    EXPECT_FALSE(specs[0].stringElements);
}

TEST(TypedPathSecuritySpecs, ObjectArrayKeepsDistinctNestedKeysAndAccessLevels) {
    const auto specs = api::PathSecuritySpecsOf<api::file::CopyAsyncParams>();

    ASSERT_EQ(specs.size(), 2u);
    EXPECT_EQ(specs[0].nestedKey, "source");
    EXPECT_EQ(specs[0].level, SecurityLevel::Read);
    EXPECT_EQ(specs[1].nestedKey, "destination");
    EXPECT_EQ(specs[1].level, SecurityLevel::FileWrite);
    for (const auto& spec : specs) {
        EXPECT_EQ(spec.paramKey, "items");
        EXPECT_TRUE(spec.isArray);
        EXPECT_FALSE(spec.skipInvalid);
        EXPECT_FALSE(spec.stringElements);
    }
}

TEST(TypedPathSecuritySpecs, HandleArrayKeepsStringElementSupport) {
    const auto specs = api::PathSecuritySpecsOf<api::playlist::InsertTracksParams>();

    ASSERT_EQ(specs.size(), 1u);
    EXPECT_EQ(specs[0].paramKey, "handles");
    EXPECT_EQ(specs[0].level, SecurityLevel::MediaRead);
    EXPECT_TRUE(specs[0].isArray);
    EXPECT_FALSE(specs[0].skipInvalid);
    EXPECT_EQ(specs[0].nestedKey, "path");
    EXPECT_TRUE(specs[0].stringElements);
}

TEST(TypedPathSecuritySpecs, MethodWithoutPathsHasNoSecuritySpecs) {
    EXPECT_TRUE(api::PathSecuritySpecsOf<api::playback::PlayParams>().empty());
}

TEST(PathValidationBatch, NoSpecsNeverInvokesTheValidator) {
    const json input = {{"_callerHwnd", 17}};
    const auto specs = api::PathSecuritySpecsOf<api::playback::PlayParams>();
    unsigned calls = 0;

    const auto checked = api::CheckPathSpecs(input, specs,
        [&](const json&, const PathSecuritySpec&) {
            ++calls;
            return ValidationResult{false, "unexpected validation", {}, false};
        });

    EXPECT_EQ(calls, 0u);
    EXPECT_FALSE(checked.error.has_value());
    EXPECT_EQ(checked.skippedCount, 0u);
    EXPECT_TRUE(checked.skipped.empty());
    EXPECT_EQ(api::WithoutSkippedEntries(input, checked.skipped), input);
}

TEST(PathValidationBatch, FailureAfterSkippedPathsStopsBeforeTheNextSpec) {
    const json input = {{"paths", {"allowed", "denied"}}, {"source", "bad"}, {"destination", "later"}};
    const auto originals = input;
    auto specs = api::PathSecuritySpecsOf<api::playback::PlayPathsParams>();
    const auto copySpecs = api::PathSecuritySpecsOf<api::file::CopyParams>();
    specs.insert(specs.end(), copySpecs.begin(), copySpecs.end());

    for (const bool shapeError : {false, true}) {
        SCOPED_TRACE(shapeError);
        std::vector<std::string> validated;
        const auto checked = api::CheckPathSpecs(input, specs,
            [&](const json& raw, const PathSecuritySpec& spec) {
                EXPECT_EQ(raw, originals);
                validated.push_back(spec.paramKey);
                if (spec.paramKey == "paths") return ValidationResult{true, {}, {1}, false};
                return ValidationResult{false, "source rejected", {}, shapeError};
            });

        EXPECT_EQ(validated, (std::vector<std::string>{"paths", "source"}));
        ASSERT_TRUE(checked.error.has_value());
        EXPECT_EQ(checked.error->message, "source rejected");
        EXPECT_EQ(checked.error->shapeError, shapeError);
        EXPECT_EQ(input, originals);
    }
}

TEST(PathValidationBatch, EveryNestedSpecReceivesTheOriginalCallerParameters) {
    const json input = {
        {"items", json::array({{{"source", "read.flac"}, {"destination", "write.flac"}}})},
        {"_callerHwnd", 0x123456789ULL}
    };
    const auto specs = api::PathSecuritySpecsOf<api::file::CopyAsyncParams>();
    std::vector<std::string> validated;

    const auto checked = api::CheckPathSpecs(input, specs,
        [&](const json& raw, const PathSecuritySpec& spec) {
            EXPECT_EQ(&raw, &input);
            EXPECT_EQ(raw.at("_callerHwnd"), input.at("_callerHwnd"));
            EXPECT_EQ(raw.at("items"), input.at("items"));
            validated.push_back(spec.nestedKey);
            return ValidationResult{};
        });

    EXPECT_EQ(validated, (std::vector<std::string>{"source", "destination"}));
    EXPECT_FALSE(checked.error.has_value());
    EXPECT_EQ(checked.skippedCount, 0u);
    EXPECT_TRUE(checked.skipped.empty());
    EXPECT_EQ(api::WithoutSkippedEntries(input, checked.skipped), input);
}

TEST(PathValidationBatch, AllFilteredPathsStayEmptyForTheGeneratedParser) {
    const json input = {{"paths", {"denied-a", "denied-b"}}, {"replace", true}, {"_callerHwnd", 17}};
    const auto specs = api::PathSecuritySpecsOf<api::playback::PlayPathsParams>();
    const auto checked = api::CheckPathSpecs(input, specs,
        [](const json&, const PathSecuritySpec&) {
            return ValidationResult{true, {}, {0, 1}, false};
        });

    ASSERT_FALSE(checked.error.has_value());
    EXPECT_EQ(checked.skippedCount, 2u);
    const auto filtered = api::WithoutSkippedEntries(input, checked.skipped);
    EXPECT_EQ(filtered, (json{{"paths", json::array()}, {"replace", true}, {"_callerHwnd", 17}}));
    EXPECT_EQ(input.at("paths"), json::array({"denied-a", "denied-b"}));

    // 过滤不补回被拒绝的路径；生成解析器继续执行声明中的非空数组约束。
    api::playback::PlayPathsParams parsed;
    std::string error;
    EXPECT_FALSE(api::playback::FromJson(filtered, parsed, error));
    EXPECT_FALSE(error.empty());
}

TEST(PathValidationBatch, FullyFilteredArrayDoesNotClearOtherArrays) {
    const json input = {
        {"allDenied", {"no-a", "no-b"}}, {"mixed", {"keep", "no-c"}},
        {"allowed", {"keep-too"}}, {"_callerHwnd", 0x123456789ULL}
    };
    const auto original = input;
    std::vector<std::string> validated;
    const auto checked = api::CheckPathSpecs(input,
        std::vector<Spec>{{"allDenied"}, {"mixed"}, {"allowed"}},
        [&](const json& raw, const Spec& spec) {
            EXPECT_EQ(&raw, &input);
            EXPECT_EQ(raw, original);
            validated.push_back(spec.paramKey);
            if (spec.paramKey == "allDenied") return Verdict{true, {}, {0, 1}, false};
            if (spec.paramKey == "mixed") return Verdict{true, {}, {1}, false};
            return Verdict{};
        });

    ASSERT_FALSE(checked.error.has_value());
    EXPECT_EQ(validated, (std::vector<std::string>{"allDenied", "mixed", "allowed"}));
    ASSERT_EQ(checked.skipped.size(), 2u);
    EXPECT_EQ(checked.skippedCount, 3u);
    const auto filtered = api::WithoutSkippedEntries(input, checked.skipped);
    EXPECT_EQ(filtered.at("allDenied"), json::array());
    EXPECT_EQ(filtered.at("mixed"), json::array({"keep"}));
    EXPECT_EQ(filtered.at("allowed"), json::array({"keep-too"}));
    EXPECT_EQ(filtered.at("_callerHwnd"), input.at("_callerHwnd"));
    EXPECT_EQ(input, original);

    json result = {{"success", true}, {"kept", 2}};
    api::AddSkippedPathCount(result, checked.skippedCount);
    EXPECT_EQ(result, (json{{"success", true}, {"kept", 2}, {"skippedPaths", 3}}));
}
