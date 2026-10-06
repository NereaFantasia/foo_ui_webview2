// test_api_schema.cpp - 由 src/api/schema/ 生成的参数解析器，以及它们共用的 ApiParams.h。
//
// 生成的 FromJson 是 API 参数的唯一入口：必填、类型、空串、未知键都在这里拒绝，handler 不再
// 各自手写检查。titleformat 覆盖必填字符串、字符串数组与映射；dialog 覆盖缺省值、枚举、
// 整数与嵌套对象数组；taskbar 覆盖可空字符串、浮点与整数的取值范围、嵌套对象里的必填键；
// playlist 覆盖整数数组逐项的取值范围；Reader 里都没用到的约束（最少元素数）单独测。
#include "pch.h"
#include "compat/fb2k_types.h"  // console:: stub: the generated headers include ErrorEnvelope.h
#include "api/generated/DialogSchema.h"
#include "api/generated/PlaylistSchema.h"
#include "api/generated/TaskbarSchema.h"
#include "api/generated/TitleformatSchema.h"

using nlohmann::json;
namespace tf = api::titleformat;
namespace dlg = api::dialog;
namespace tb = api::taskbar;

namespace {

template <class P>
std::string ParseError(const json& j) {
    P out;
    std::string error;
    EXPECT_FALSE(FromJson(j, out, error)) << j.dump();
    return error;
}

}  // namespace

TEST(ApiSchemaTitleformat, EvalAcceptsBothRequiredStrings) {
    tf::EvalParams p;
    std::string error;
    ASSERT_TRUE(FromJson(json{{"path", "C:\\a.flac"}, {"pattern", "%title%"}}, p, error)) << error;
    EXPECT_EQ(p.path, "C:\\a.flac");
    EXPECT_EQ(p.pattern, "%title%");
}

TEST(ApiSchemaTitleformat, EvalPathMayBeOmittedButNotEmpty) {
    // 缺省或 null 表示对正在播放的曲目求值；空串仍是错，沿用 minLength 1 的 "is required" 消息。
    tf::EvalParams omitted;
    std::string error;
    ASSERT_TRUE(FromJson(json{{"pattern", "%title%"}}, omitted, error)) << error;
    EXPECT_FALSE(omitted.path.has_value());
    tf::EvalParams null;
    ASSERT_TRUE(FromJson(json{{"path", nullptr}, {"pattern", "%title%"}}, null, error)) << error;
    EXPECT_FALSE(null.path.has_value());
    EXPECT_EQ(ParseError<tf::EvalParams>(json{{"path", ""}, {"pattern", "%title%"}}), "path is required");
    EXPECT_EQ(ParseError<tf::EvalParams>(json{{"path", "a"}}), "pattern is required");
}

TEST(ApiSchemaTitleformat, WrongTypesNameTheField) {
    EXPECT_EQ(ParseError<tf::EvalParams>(json{{"path", 1}, {"pattern", "x"}}), "path must be a string");
    EXPECT_EQ(ParseError<tf::EvalBatchParams>(json{{"paths", "a"}, {"pattern", "x"}}), "paths must be an array");
    EXPECT_EQ(ParseError<tf::EvalBatchParams>(json{{"paths", {"a", 2}}, {"pattern", "x"}}), "paths[1] must be a string");
    EXPECT_EQ(ParseError<tf::EvalFieldsParams>(json{{"path", "a"}, {"fields", {"x"}}}), "fields must be an object");
    EXPECT_EQ(ParseError<tf::EvalFieldsParams>(json{{"path", "a"}, {"fields", {{"year", 2}}}}), "fields.year must be a string");
}

TEST(ApiSchemaTitleformat, UnknownKeysAreRejectedButBridgeKeysPass) {
    EXPECT_EQ(ParseError<tf::EvalParams>(json{{"path", "a"}, {"patern", "x"}}), "unknown parameter 'patern'");
    EXPECT_EQ(ParseError<tf::GetBuiltinFieldsParams>(json{{"x", 1}}), "unknown parameter 'x'");

    // BridgeCore 把 _callerHwnd 写进每次调用的 params，下划线开头的键留给桥接层。
    tf::GetBuiltinFieldsParams none;
    std::string error;
    EXPECT_TRUE(FromJson(json{{"_callerHwnd", 123}}, none, error)) << error;
    tf::EvalParams p;
    EXPECT_TRUE(FromJson(json{{"path", "a"}, {"pattern", "x"}, {"_callerHwnd", 1}}, p, error)) << error;
}

TEST(ApiSchemaTitleformat, ParamsMustBeAnObject) {
    EXPECT_EQ(ParseError<tf::EvalParams>(json::array()), "params must be an object");
    EXPECT_EQ(ParseError<tf::GetBuiltinFieldsParams>(json("x")), "params must be an object");
}

TEST(ApiSchemaTitleformat, FieldsMapKeepsEveryEntry) {
    tf::EvalFieldsBatchParams p;
    std::string error;
    ASSERT_TRUE(FromJson(json{{"paths", {"a", "b"}}, {"fields", {{"year", "$year(%date%)"}, {"artist", "%artist%"}}}}, p, error)) << error;
    EXPECT_EQ(p.paths, (std::vector<std::string>{"a", "b"}));
    ASSERT_EQ(p.fields.size(), 2u);
    EXPECT_EQ(p.fields.at("year"), "$year(%date%)");
    // 空映射是合法输入，handler 对它返回零行。
    ASSERT_TRUE(FromJson(json{{"paths", json::array()}, {"fields", json::object()}}, p, error)) << error;
    EXPECT_TRUE(p.fields.empty());
}

TEST(ApiSchemaTitleformat, PathParamsCarryTheDeclaredLevels) {
    ASSERT_EQ(tf::EvalParams::kPathParams.size(), 1u);
    EXPECT_STREQ(tf::EvalParams::kPathParams[0].key, "path");
    EXPECT_EQ(tf::EvalParams::kPathParams[0].access, api::params::PathAccess::MediaRead);
    EXPECT_FALSE(tf::EvalParams::kPathParams[0].isArray);
    ASSERT_EQ(tf::EvalBatchParams::kPathParams.size(), 1u);
    EXPECT_TRUE(tf::EvalBatchParams::kPathParams[0].isArray);
    EXPECT_EQ(tf::GetBuiltinFieldsParams::kPathParams.size(), 0u);
    EXPECT_STREQ(tf::EvalFieldsParams::kMethod, "titleformat.evalFields");
}

// ---- Reader 里 titleformat 没用到的约束 ----

TEST(ApiParamsReader, RangeOneOfAndMinItems) {
    std::string error;
    const json j{{"limit", 0}, {"mode", "loud"}, {"ids", json::array()}};
    api::params::Reader r(j, error);

    std::int64_t limit = 100;
    ASSERT_TRUE(r.Optional("limit", limit));
    EXPECT_FALSE(r.Range("limit", limit, std::int64_t{1}, std::nullopt));
    EXPECT_EQ(error, "limit is out of range");

    std::string mode;
    ASSERT_TRUE(r.Required("mode", mode));
    EXPECT_FALSE(r.OneOf("mode", mode, {"quiet", "normal"}));
    EXPECT_EQ(error, "mode has an unsupported value 'loud'");

    std::vector<std::string> ids;
    ASSERT_TRUE(r.Required("ids", ids));
    EXPECT_FALSE(r.MinItems("ids", ids, 1));
    EXPECT_EQ(error, "ids must have at least 1 item(s)");
}

TEST(ApiParamsReader, ItemsRangeChecksEveryItemAndNamesTheFirstBadOne) {
    std::string error;
    const json j = json::object();
    api::params::Reader r(j, error);

    EXPECT_FALSE(r.ItemsRange("rows", std::vector<std::int64_t>{0, 3, -1, -2}, std::int64_t{0}, std::nullopt));
    EXPECT_EQ(error, "rows[2] is out of range");
    EXPECT_FALSE(r.ItemsRange("rows", std::vector<std::int64_t>{1, 9}, std::nullopt, std::int64_t{8}));
    EXPECT_EQ(error, "rows[1] is out of range");
    EXPECT_FALSE(r.ItemsRange("gains", std::vector<double>{0.5, 1.5}, 0.0, 1.0));
    EXPECT_EQ(error, "gains[1] is out of range");

    // 端点本身在范围内；空数组与缺省的可选数组没有可查的项。
    EXPECT_TRUE(r.ItemsRange("rows", std::vector<std::int64_t>{0, 8}, std::int64_t{0}, std::int64_t{8}));
    EXPECT_TRUE(r.ItemsRange("rows", std::vector<std::int64_t>{}, std::int64_t{0}, std::int64_t{0}));
    const std::optional<std::vector<std::int64_t>> absent;
    EXPECT_TRUE(r.ItemsRange("rows", absent, std::int64_t{0}, std::nullopt));
    const std::optional<std::vector<std::int64_t>> present{{2, -5}};
    EXPECT_FALSE(r.ItemsRange("rows", present, std::int64_t{0}, std::nullopt));
    EXPECT_EQ(error, "rows[1] is out of range");
}

TEST(ApiParamsReader, OptionalKeepsDefaultsAndNullMeansAbsent) {
    std::string error;
    const json j{{"count", nullptr}};
    api::params::Reader r(j, error);
    std::int64_t count = 100;
    ASSERT_TRUE(r.Optional("count", count));
    EXPECT_EQ(count, 100);
    std::optional<std::string> name = std::string("stale");
    ASSERT_TRUE(r.Optional("name", name));
    EXPECT_FALSE(name.has_value());
}

TEST(ApiParamsReader, IntegersRejectFractionsAndHugeUnsigned) {
    std::string error;
    std::int64_t n = 0;
    const json fraction{{"n", 1.5}};
    api::params::Reader r1(fraction, error);
    EXPECT_FALSE(r1.Required("n", n));
    EXPECT_EQ(error, "n must be an integer");
    const json huge{{"n", 18446744073709551615ull}};
    api::params::Reader r2(huge, error);
    EXPECT_FALSE(r2.Required("n", n));
    EXPECT_EQ(error, "n is out of range");
}

// ---- dialog：缺省值、枚举、整数、嵌套对象数组 ----

TEST(ApiSchemaDialog, EmptyParamsTakeTheDeclaredDefaults) {
    dlg::ConfirmParams p;
    std::string error;
    ASSERT_TRUE(FromJson(json::object(), p, error)) << error;
    EXPECT_FALSE(p.title.has_value());  // 标题缺省值随界面语言变化，由 handler 决定
    EXPECT_EQ(p.message, "");
    EXPECT_EQ(p.type, "question");
    EXPECT_EQ(p.defaultButton, 0);
    EXPECT_FALSE(p.buttons.has_value());
}

TEST(ApiSchemaDialog, TypeMustBeOneOfTheDeclaredIcons) {
    EXPECT_EQ(ParseError<dlg::ConfirmParams>(json{{"type", "warn"}}), "type has an unsupported value 'warn'");
    dlg::ConfirmParams p;
    std::string error;
    ASSERT_TRUE(FromJson(json{{"type", "error"}, {"defaultButton", 1}, {"buttons", json::array({"Yes", "No"})}}, p, error)) << error;
    EXPECT_EQ(p.type, "error");
    EXPECT_EQ(p.defaultButton, 1);
    EXPECT_EQ(p.buttons->size(), 2u);
    EXPECT_EQ(ParseError<dlg::ConfirmParams>(json{{"defaultButton", 1.5}}), "defaultButton must be an integer");
}

TEST(ApiSchemaDialog, FiltersParseIntoTheSharedStruct) {
    dlg::OpenFileParams p;
    std::string error;
    const json j{{"multiple", true},
                 {"filters", json::array({json{{"name", "Audio"}, {"extensions", json::array({"flac", "mp3"})}},
                                          json{{"extensions", json::array({"*"})}}})}};
    ASSERT_TRUE(FromJson(j, p, error)) << error;
    EXPECT_TRUE(p.multiple);
    EXPECT_EQ(p.defaultPath, "");
    ASSERT_EQ(p.filters->size(), 2u);
    EXPECT_EQ((*p.filters)[0].name, "Audio");
    EXPECT_EQ((*p.filters)[0].extensions->at(1), "mp3");
    EXPECT_FALSE((*p.filters)[1].name.has_value());

    // saveFile 用同一个 FileFilter，两个方法共享一份结构体。
    dlg::SaveFileParams s;
    ASSERT_TRUE(FromJson(json{{"filters", json::array({json{{"name", "Playlist"}}})}}, s, error)) << error;
    static_assert(std::is_same_v<decltype(s.filters), decltype(p.filters)>);
}

TEST(ApiSchemaDialog, NestedErrorsNameTheExactPosition) {
    EXPECT_EQ(ParseError<dlg::OpenFileParams>(json{{"filters", json::array({"Audio"})}}), "filters[0] must be an object");
    EXPECT_EQ(ParseError<dlg::OpenFileParams>(
                  json{{"filters", json::array({json{{"extensions", json::array({"flac", 3})}}})}}),
              "filters[0].extensions[1] must be a string");
    EXPECT_EQ(ParseError<dlg::SaveFileParams>(json{{"filters", json::array({json{{"name", "A"}, {"label", "B"}}})}}),
              "unknown parameter 'filters[0].label'");
}

// ---- taskbar：可空字符串、取值范围、嵌套对象里的必填键 ----

TEST(ApiSchemaTaskbar, NullableIconReadsNullAsAbsent) {
    // icon 在公开契约里接受 null（清除 / 用默认图标），声明成 string | null，Reader 把 null 当没传。
    tb::SetOverlayIconParams p;
    std::string error;
    ASSERT_TRUE(FromJson(json{{"icon", nullptr}, {"description", "Paused"}}, p, error)) << error;
    EXPECT_FALSE(p.icon.has_value());
    EXPECT_EQ(p.description, "Paused");
    tb::SetOverlayIconParams q;
    ASSERT_TRUE(FromJson(json{{"icon", "AAABAAEA"}}, q, error)) << error;
    EXPECT_EQ(*q.icon, "AAABAAEA");
    EXPECT_EQ(q.description, "");
    EXPECT_EQ(ParseError<tb::SetOverlayIconParams>(json{{"icon", 1}}), "icon must be a string");
}

TEST(ApiSchemaTaskbar, ProgressStateIsAnEnumAndValueIsRanged) {
    tb::SetProgressParams p;
    std::string error;
    ASSERT_TRUE(FromJson(json::object(), p, error)) << error;
    EXPECT_EQ(p.state, "none");
    EXPECT_FALSE(p.value.has_value());
    ASSERT_TRUE(FromJson(json{{"state", "normal"}, {"value", 0.42}}, p, error)) << error;
    EXPECT_EQ(p.state, "normal");
    EXPECT_DOUBLE_EQ(*p.value, 0.42);
    ASSERT_TRUE(FromJson(json{{"value", 1}}, p, error)) << error;  // 整数也是 number
    EXPECT_DOUBLE_EQ(*p.value, 1.0);
    // 迁移前未知状态按 none 处理、越界的 value 被静默忽略；现在两者都是 INVALID_PARAMS。
    EXPECT_EQ(ParseError<tb::SetProgressParams>(json{{"state", "busy"}}), "state has an unsupported value 'busy'");
    EXPECT_EQ(ParseError<tb::SetProgressParams>(json{{"value", 1.5}}), "value is out of range");
    EXPECT_EQ(ParseError<tb::SetProgressParams>(json{{"value", "half"}}), "value must be a number");
}

TEST(ApiSchemaTaskbar, ThumbnailButtonsNeedAnIdAndTakeDefaults) {
    tb::SetThumbnailButtonsParams p;
    std::string error;
    const json j{{"buttons", json::array({json{{"id", "prev"}, {"tooltip", "Previous"}},
                                          json{{"id", "pp"}, {"icon", nullptr}, {"enabled", false}, {"dismissOnClick", true}}})}};
    ASSERT_TRUE(FromJson(j, p, error)) << error;
    ASSERT_EQ(p.buttons.size(), 2u);
    EXPECT_EQ(p.buttons[0].id, "prev");
    EXPECT_EQ(p.buttons[0].tooltip, "Previous");
    EXPECT_FALSE(p.buttons[0].icon.has_value());
    EXPECT_TRUE(p.buttons[0].enabled);
    EXPECT_TRUE(p.buttons[0].visible);
    EXPECT_FALSE(p.buttons[0].dismissOnClick);
    EXPECT_FALSE(p.buttons[1].enabled);
    EXPECT_TRUE(p.buttons[1].dismissOnClick);

    EXPECT_EQ(ParseError<tb::SetThumbnailButtonsParams>(json::object()), "buttons is required");
    EXPECT_EQ(ParseError<tb::SetThumbnailButtonsParams>(json{{"buttons", json::array({json{{"tooltip", "x"}}})}}),
              "buttons[0].id is required");
    EXPECT_EQ(ParseError<tb::SetThumbnailButtonsParams>(json{{"buttons", json::array({json{{"id", ""}}})}}),
              "buttons[0].id is required");
    EXPECT_EQ(ParseError<tb::SetThumbnailButtonsParams>(json{{"buttons", json::array({json{{"id", "a"}, {"label", "x"}}})}}),
              "unknown parameter 'buttons[0].label'");
}

TEST(ApiSchemaTaskbar, UpdateButtonLeavesOmittedStateAlone) {
    tb::UpdateButtonParams p;
    std::string error;
    ASSERT_TRUE(FromJson(json{{"id", "pp"}, {"tooltip", "Pause"}}, p, error)) << error;
    EXPECT_EQ(p.id, "pp");
    EXPECT_EQ(p.tooltip, "Pause");
    EXPECT_FALSE(p.enabled.has_value());
    EXPECT_FALSE(p.visible.has_value());
    EXPECT_FALSE(p.icon.has_value());
    ASSERT_TRUE(FromJson(json{{"id", "pp"}, {"enabled", false}, {"visible", true}}, p, error)) << error;
    EXPECT_EQ(p.enabled, std::optional<bool>(false));
    EXPECT_EQ(p.visible, std::optional<bool>(true));
    EXPECT_EQ(ParseError<tb::UpdateButtonParams>(json{{"tooltip", "Pause"}}), "id is required");
    EXPECT_EQ(ParseError<tb::UpdateButtonParams>(json{{"id", ""}}), "id is required");
}

TEST(ApiSchemaTaskbar, FlashCountsAreNonNegativeIntegers) {
    tb::FlashParams p;
    std::string error;
    ASSERT_TRUE(FromJson(json::object(), p, error)) << error;
    EXPECT_EQ(p.count, 3);
    EXPECT_EQ(p.interval, 0);
    ASSERT_TRUE(FromJson(json{{"count", 0}, {"interval", 250}}, p, error)) << error;
    EXPECT_EQ(p.count, 0);
    EXPECT_EQ(p.interval, 250);
    EXPECT_EQ(ParseError<tb::FlashParams>(json{{"count", -1}}), "count is out of range");
    EXPECT_EQ(ParseError<tb::FlashParams>(json{{"interval", 2.5}}), "interval must be an integer");
}

// ---- playlist：行号数组逐项不小于 0 ----

TEST(ApiSchemaPlaylist, RowArraysRefuseNegativeRowsAndNameTheItem) {
    namespace pl = api::playlist;
    EXPECT_EQ(ParseError<pl::RemoveTracksParams>(json{{"items", {0, -1, 2}}}), "items[1] is out of range");
    EXPECT_EQ(ParseError<pl::MoveTracksParams>(json{{"items", {-3}}, {"delta", 1}}), "items[0] is out of range");
    EXPECT_EQ(ParseError<pl::SetSelectionParams>(json{{"indices", {4, 5, -1}}}), "indices[2] is out of range");
    EXPECT_EQ(ParseError<pl::ReorderParams>(json{{"newOrder", {1, -1}}}), "newOrder[1] is out of range");
    EXPECT_EQ(ParseError<pl::ReorderPlaylistsParams>(json{{"newOrder", {-1, 0}}}), "newOrder[0] is out of range");

    // 0 与超出末尾的行照常通过解析，超出末尾的行仍由 handler 按行数处理。
    pl::RemoveTracksParams remove;
    std::string error;
    ASSERT_TRUE(FromJson(json{{"items", {0, 99}}}, remove, error)) << error;
    EXPECT_EQ(remove.items, (std::vector<std::int64_t>{0, 99}));
    pl::MoveTracksParams move;
    ASSERT_TRUE(FromJson(json{{"delta", -1}}, move, error)) << error;
    EXPECT_FALSE(move.items.has_value());
    EXPECT_EQ(move.delta, -1);
}
