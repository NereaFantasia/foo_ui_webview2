// test_api_schema_results.cpp - 由 src/api/schema/ 生成的返回值结构体与 ToJson，以及它们共用的 ApiResult.h。
//
// ToJson 只负责形状：必填字段一定写、可选字段没有值就不写键、可空字段没有值写 null、
// 嵌套结构体与数组逐个转发、`A & Record<string, T>` 的额外键先写再被具名字段覆盖。
// 信封字段（success / error / code）不由它写，由 Result<R>::ToJson 写：成功时 success 最后写，失败时
// 只有 ApiEnvelope::MakeError 一处定义，extra 盖不掉信封。
#include "pch.h"
#include "compat/fb2k_types.h"  // console:: stub: the generated headers include ErrorEnvelope.h
#include "api/generated/DialogSchema.h"
#include "api/generated/TaskbarSchema.h"
#include "api/generated/TitleformatSchema.h"

using nlohmann::json;
namespace tf = api::titleformat;
namespace dlg = api::dialog;
namespace tb = api::taskbar;

TEST(ApiSchemaResults, RequiredFieldsAreAlwaysWrittenAndTheEnvelopeIsNot) {
    tf::EvalResult r;
    r.path = "C:\\a.flac";
    r.pattern = "%title%";
    r.result = "Song";
    // infoAvailable is required in the declaration: it is written even when left at its default.
    EXPECT_EQ(ToJson(r), (json{{"path", "C:\\a.flac"}, {"pattern", "%title%"}, {"result", "Song"}, {"infoAvailable", false}}));
    EXPECT_FALSE(ToJson(r).contains("success")) << "the envelope, not ToJson, writes success";
}

TEST(ApiSchemaResults, AbsentOptionalFieldsAreNotWritten) {
    tf::EvalBatchRow row;
    row.path = "a";
    row.success = true;
    EXPECT_EQ(ToJson(row), (json{{"path", "a"}, {"success", true}}));
    row.result = "A";
    row.infoAvailable = true;
    EXPECT_EQ(ToJson(row), (json{{"path", "a"}, {"success", true}, {"result", "A"}, {"infoAvailable", true}}));
}

TEST(ApiSchemaResults, NestedStructsAndArraysForwardThroughValue) {
    tf::EvalBatchResult r;
    r.pattern = "%title%";
    r.total = 2;
    r.successCount = 1;
    r.errorCount = 1;
    tf::EvalBatchRow ok;
    ok.path = "a";
    ok.success = true;
    ok.result = "A";
    tf::EvalBatchRow bad;
    bad.path = "b";
    bad.success = false;
    bad.error = "Failed to open file";
    r.results = {ok, bad};

    json j = ToJson(r);
    ASSERT_EQ(j["results"].size(), 2u);
    EXPECT_EQ(j["results"][0], (json{{"path", "a"}, {"success", true}, {"result", "A"}}));
    EXPECT_EQ(j["results"][1], (json{{"path", "b"}, {"success", false}, {"error", "Failed to open file"}}));
    EXPECT_EQ(j["total"], 2);
}

TEST(ApiSchemaResults, ExtraKeysAreWrittenFirstAndNamedFieldsWin) {
    tf::EvalFieldsResult r;
    r.path = "C:\\a.flac";
    r.additional = {{"title", "Song"}, {"path", "overridden by the named field"}};
    json j = ToJson(r);
    EXPECT_EQ(j["title"], "Song");
    EXPECT_EQ(j["path"], "C:\\a.flac");
    EXPECT_EQ(j.size(), 2u);
}

TEST(ApiSchemaResults, ScalarResultFieldsAreWritten) {
    dlg::ConfirmResult c;
    c.response = -1;
    EXPECT_EQ(ToJson(c), (json{{"response", -1}}));
}

TEST(ApiSchemaResults, ParamsNameTheirResultType) {
    static_assert(std::is_same_v<tf::EvalParams::Result, tf::EvalResult>);
    static_assert(std::is_same_v<tb::FlashParams::Result, void>, "a method declared as returning void has no result struct");
    static_assert(std::is_same_v<dlg::ConfirmParams::Result, dlg::ConfirmResult>);
}

TEST(ApiResultWriters, NullableWritesNullAndOptionalOmits) {
    json j = json::object();
    std::optional<std::string> none;
    api::results::Put(j, "optional", none);
    api::results::PutNullable(j, "nullable", none);
    EXPECT_FALSE(j.contains("optional"));
    EXPECT_TRUE(j.contains("nullable"));
    EXPECT_TRUE(j["nullable"].is_null());

    std::vector<std::optional<std::int64_t>> items{1, std::nullopt, 3};
    api::results::Put(j, "items", items);
    EXPECT_EQ(j["items"], (json{1, nullptr, 3}));

    std::map<std::string, std::vector<double>> map{{"a", {0.5, 1.0}}};
    api::results::Put(j, "map", map);
    EXPECT_EQ(j["map"]["a"], (json{0.5, 1.0}));
}

namespace api_result_test {
// 测试内合成的嵌套结构体：生成器目前没有 map<string, 结构体> 与 optional<vector<结构体>> 的实例，
// 这里验证 Value 的模板先声明后定义与 ADL 转发对这两种组合同样成立。
struct Item {
    std::string name;
};
inline json ToJson(const Item& v) {
    json j = json::object();
    api::results::Put(j, "name", v.name);
    return j;
}
}  // namespace api_result_test

TEST(ApiSchemaResults, MapsAndOptionalVectorsOfStructsForwardThroughValue) {
    std::map<std::string, api_result_test::Item> byKey{{"a", {"A"}}, {"b", {"B"}}};
    EXPECT_EQ(api::results::Value(byKey), (json{{"a", {{"name", "A"}}}, {"b", {{"name", "B"}}}}));

    std::optional<std::vector<api_result_test::Item>> list;
    json j = json::object();
    api::results::Put(j, "list", list);
    EXPECT_FALSE(j.contains("list"));
    list = std::vector<api_result_test::Item>{{"x"}};
    api::results::Put(j, "list", list);
    EXPECT_EQ(j, (json{{"list", json::array({{{"name", "x"}}})}}));
}

TEST(ApiSchemaResults, StringLiteralsAreWrittenAsStringsNotBooleans) {
    json j = json::object();
    api::results::Put(j, "k", "text");
    EXPECT_EQ(j, (json{{"k", "text"}}));
}

TEST(ApiResult, SuccessWritesTheEnvelopeLast) {
    tf::EvalFieldsResult r;
    r.path = "C:\\a.flac";
    r.infoAvailable = true;
    r.additional = {{"title", "Song"}, {"path", "overridden"}, {"success", "overridden"}};
    // D4：额外键 → 具名字段 → success。调用方取名 path / success 的字段被丢弃。
    EXPECT_EQ(api::Result<tf::EvalFieldsResult>(r).ToJson(),
              (json{{"success", true}, {"path", "C:\\a.flac"}, {"infoAvailable", true}, {"title", "Song"}}));
}

TEST(ApiResult, FailureIsTheEnvelopeAndExtraCannotOverrideIt) {
    api::Result<tf::EvalResult> failed =
        api::Fail("Failed to open file", ApiErrorCode::INVALID_PATH,
                  json{{"path", "C:\\a.flac"}, {"success", true}, {"code", "X"}, {"error", "Y"}});
    EXPECT_EQ(failed.ToJson(), (json{{"success", false},
                                     {"error", "Failed to open file"},
                                     {"code", "INVALID_PATH"},
                                     {"path", "C:\\a.flac"}}));
}

TEST(ApiResult, CodeIsOwnedSoALocalStringDoesNotDangle) {
    auto make = [] {
        std::string code = std::string("OPERATION_") + "FAILED";
        return api::Result<void>(api::Fail("boom", code));
    };
    EXPECT_EQ(make().ToJson(), (json{{"success", false}, {"error", "boom"}, {"code", "OPERATION_FAILED"}}));
}

TEST(ApiResult, VoidSuccessIsOnlyTheEnvelope) {
    EXPECT_EQ(api::Ok().ToJson(), (json{{"success", true}}));
}

TEST(ApiResult, PanelModeUnsupportedIsTheBareFailureEnvelope) {
    api::Result<void> r = api::PanelModeUnsupported("taskbar.flash");
    EXPECT_EQ(r.ToJson(), (json{{"success", false},
                                {"error", "taskbar.flash is not supported in panel mode"},
                                {"code", "PANEL_MODE_UNSUPPORTED"}}));
}

TEST(ApiResult, DialogCancelIsASuccessWithAnEmptySelection) {
    dlg::OpenFileResult cancelled;
    cancelled.canceled = true;
    EXPECT_EQ(api::Result<dlg::OpenFileResult>(cancelled).ToJson(),
              (json{{"success", true}, {"canceled", true}, {"filePaths", json::array()}}));
    api::Result<dlg::OpenFileResult> failed = api::Fail("Dialog failed", ApiErrorCode::OPERATION_FAILED);
    EXPECT_EQ(failed.ToJson(), (json{{"success", false}, {"error", "Dialog failed"}, {"code", "OPERATION_FAILED"}}));
}

TEST(ApiResult, RowFieldsKeepTheRowsOwnNamedFields) {
    tf::EvalFieldsBatchRow row;
    row.path = "a";
    row.success = true;
    row.infoAvailable = true;
    row.additional = {{"title", "T"}, {"path", "x"}, {"success", "x"}, {"infoAvailable", "x"}, {"error", "E"}};
    // 有值的具名字段胜过同名额外键；成功行没有 error，调用方取名 error 的字段留在行里。
    EXPECT_EQ(ToJson(row), (json{{"path", "a"}, {"success", true}, {"infoAvailable", true}, {"title", "T"}, {"error", "E"}}));
}
