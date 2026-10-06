// test_preferences_draft.cpp - Preferences 草稿 / 提交纯逻辑覆盖
//
// 直接链接生产头 prefs_draft::* 与总览页的真实字段表 prefs_fields::overview::Table()，
// 不在测试内重写被测逻辑。断言依据是 docs/preferences/DESIGN.md 第 5 节：
//   - dirty 按差异计算，改回原值即清除；Reset 只改草稿；
//   - Apply 前全量校验，任一失败即拒绝；
//   - 冲突只看已编辑字段，外部写成草稿要写的值不算冲突，未编辑字段不参与；
//   - 影响重启的字段与本进程启动值不同即要求重启。
#include "pch.h"
#include "../src/core/PreferencesDraft.h"
#include "../src/core/PreferencesFields.h"

using namespace prefs_draft;
using prefs_fields::overview::Backdrop;
using prefs_fields::overview::Language;
using prefs_fields::overview::Template;
namespace window = prefs_fields::window;

namespace {

bool AlwaysUsable(const std::string&) { return true; }
bool NeverUsable(const std::string&) { return false; }

// 总览页字段表；模板可用性判定默认全部通过，校验用例再按需替换。
FieldTable Fields() { return prefs_fields::overview::Table(AlwaysUsable); }

Snapshot NonDefaultInitial() {
    Snapshot s = Defaults(Fields());
    s.SetString(Template, "night");
    s.SetInt(Backdrop, 3);        // Acrylic
    s.SetInt(Language, 2);        // Chinese
    return s;
}

bool Contains(const std::vector<size_t>& fields, size_t wanted) {
    return std::find(fields.begin(), fields.end(), wanted) != fields.end();
}

}  // namespace

// ============================================
// 差异与 dirty
// ============================================

TEST(PreferencesDraftDiff, IdenticalSnapshotsAreClean) {
    const Snapshot a = NonDefaultInitial();
    const Snapshot b = a;
    EXPECT_TRUE(Diff(a, b).empty());
    EXPECT_FALSE(IsDirty(a, b));
    EXPECT_TRUE(a == b);
}

TEST(PreferencesDraftDiff, EditingOneFieldMarksOnlyThatField) {
    const Snapshot initial = NonDefaultInitial();
    Snapshot draft = initial;
    draft.SetInt(Backdrop, 1);
    const std::vector<size_t> changed = Diff(initial, draft);
    ASSERT_EQ(changed.size(), 1u);
    EXPECT_EQ(changed[0], static_cast<size_t>(Backdrop));
    EXPECT_TRUE(IsDirty(initial, draft));
}

TEST(PreferencesDraftDiff, RevertingToOriginalClearsDirty) {
    const Snapshot initial = NonDefaultInitial();
    Snapshot draft = initial;
    draft.SetString(Template, "other");
    EXPECT_TRUE(IsDirty(initial, draft));
    draft.SetString(Template, initial.GetString(Template));
    EXPECT_FALSE(IsDirty(initial, draft));
}

TEST(PreferencesDraftDiff, DiffFollowsFieldOrder) {
    const Snapshot initial = NonDefaultInitial();
    const Snapshot draft = Defaults(Fields());
    const std::vector<size_t> changed = Diff(initial, draft);
    ASSERT_EQ(changed.size(), 3u);
    EXPECT_EQ(changed[0], static_cast<size_t>(Template));
    EXPECT_EQ(changed[1], static_cast<size_t>(Backdrop));
    EXPECT_EQ(changed[2], static_cast<size_t>(Language));
}

// ============================================
// 默认值与 Reset
// ============================================

TEST(PreferencesDraftDefaults, MatchConfigDefaults) {
    const Snapshot d = Defaults(Fields());
    ASSERT_EQ(d.size(), static_cast<size_t>(prefs_fields::overview::Count));
    EXPECT_EQ(d.GetString(Template), "default");
    EXPECT_EQ(d.GetInt(Backdrop), 1);   // Mica
    EXPECT_EQ(d.GetInt(Language), 0);   // Auto
}

// 窗口子页：七个开关的默认值与各配置变量一致；只有后台模式与缩略图按钮影响重启。
TEST(PreferencesWindowFields, DefaultsAndRestartFlagsMatchDesign) {
    const FieldTable fields = window::Table();
    const Snapshot d = Defaults(fields);
    ASSERT_EQ(d.size(), static_cast<size_t>(window::Count));
    EXPECT_TRUE(d.GetBool(window::RememberPosition));
    EXPECT_FALSE(d.GetBool(window::BackgroundMode));
    EXPECT_TRUE(d.GetBool(window::RestoreVisibility));
    EXPECT_FALSE(d.GetBool(window::MinimizeToTray));
    EXPECT_FALSE(d.GetBool(window::CloseToTray));
    EXPECT_TRUE(d.GetBool(window::TaskbarButtons));
    EXPECT_FALSE(d.GetBool(window::TaskbarProgress));
    for (size_t i = 0; i < fields.size(); ++i) {
        const bool restart = (i == window::BackgroundMode || i == window::TaskbarButtons);
        EXPECT_EQ(fields[i].affectsRestart, restart) << fields[i].key;
    }
    EXPECT_TRUE(Validate(fields, d).ok());
}

TEST(PreferencesWindowFields, RestartOnlyWhenBackgroundModeOrButtonsLeaveStartupValue) {
    const FieldTable fields = window::Table();
    const Snapshot startup = Defaults(fields);
    Snapshot draft = startup;
    draft.SetBool(window::MinimizeToTray, true);
    draft.SetBool(window::TaskbarProgress, true);
    EXPECT_FALSE(NeedsRestart(fields, startup, draft));
    draft.SetBool(window::BackgroundMode, true);
    EXPECT_TRUE(NeedsRestart(fields, startup, draft));
    draft.SetBool(window::BackgroundMode, false);
    draft.SetBool(window::TaskbarButtons, false);
    EXPECT_TRUE(NeedsRestart(fields, startup, draft));
}

TEST(PreferencesWindowFields, DiffAndConflictsWorkOnBoolFields) {
    const FieldTable fields = window::Table();
    const Snapshot initial = Defaults(fields);
    Snapshot draft = initial;
    draft.SetBool(window::CloseToTray, true);
    const std::vector<size_t> changed = Diff(initial, draft);
    ASSERT_EQ(changed.size(), 1u);
    EXPECT_EQ(changed[0], static_cast<size_t>(window::CloseToTray));
    // 别处把同一开关改成了草稿要写的值：不算冲突；改了别的开关：未编辑字段不参与。
    Snapshot current = initial;
    current.SetBool(window::CloseToTray, true);
    current.SetBool(window::RememberPosition, false);
    EXPECT_TRUE(DetectConflicts(initial, draft, current).empty());
}

TEST(PreferencesDraftDefaults, ResetOnAlreadyDefaultPageStaysClean) {
    const Snapshot initial = Defaults(Fields());
    const Snapshot afterReset = Defaults(Fields());
    EXPECT_FALSE(IsDirty(initial, afterReset));
}

TEST(PreferencesDraftDefaults, ResetOnCustomisedPageBecomesDirtyWithoutWriting) {
    const Snapshot initial = NonDefaultInitial();
    const Snapshot afterReset = Defaults(Fields());
    // 只有草稿变了；initial 代表配置，保持不动
    EXPECT_TRUE(IsDirty(initial, afterReset));
    EXPECT_EQ(initial.GetString(Template), "night");
}

// ============================================
// 校验
// ============================================

TEST(PreferencesDraftValidate, AcceptsWellFormedDraft) {
    const FieldTable fields = Fields();
    const ValidationResult r = Validate(fields, Defaults(fields));
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(r.error, ValidationError::None);
}

TEST(PreferencesDraftValidate, RejectsInvalidTemplateNameBeforeProbingDisk) {
    bool probed = false;
    const FieldTable fields = prefs_fields::overview::Table([&](const std::string&) {
        probed = true;
        return true;
    });
    Snapshot draft = Defaults(fields);
    draft.SetString(Template, "../escape");
    const ValidationResult r = Validate(fields, draft);
    EXPECT_FALSE(r.ok());
    EXPECT_EQ(r.error, ValidationError::Invalid);
    EXPECT_EQ(r.field, static_cast<size_t>(Template));
    EXPECT_FALSE(probed);
}

TEST(PreferencesDraftValidate, RejectsEmptyTemplateName) {
    const FieldTable fields = Fields();
    Snapshot draft = Defaults(fields);
    draft.SetString(Template, "");
    EXPECT_EQ(Validate(fields, draft).error, ValidationError::Invalid);
}

TEST(PreferencesDraftValidate, RejectsMissingTemplateFolder) {
    const FieldTable fields = prefs_fields::overview::Table(NeverUsable);
    const ValidationResult r = Validate(fields, Defaults(fields));
    EXPECT_EQ(r.error, ValidationError::Missing);
    EXPECT_EQ(r.field, static_cast<size_t>(Template));
}

TEST(PreferencesDraftValidate, MissingUsabilityCallbackCountsAsMissingTemplate) {
    const FieldTable fields = prefs_fields::overview::Table(nullptr);
    const ValidationResult r = Validate(fields, Defaults(fields));
    EXPECT_EQ(r.error, ValidationError::Missing);
}

TEST(PreferencesDraftValidate, RejectsEnumValuesOutsideRange) {
    const FieldTable fields = Fields();
    Snapshot draft = Defaults(fields);
    draft.SetInt(Backdrop, 5);
    EXPECT_EQ(Validate(fields, draft).error, ValidationError::OutOfRange);
    draft.SetInt(Backdrop, -1);
    EXPECT_EQ(Validate(fields, draft).error, ValidationError::OutOfRange);
    draft.SetInt(Backdrop, 4);
    draft.SetInt(Language, 3);
    const ValidationResult r = Validate(fields, draft);
    EXPECT_EQ(r.error, ValidationError::OutOfRange);
    EXPECT_EQ(r.field, static_cast<size_t>(Language));
}

// 枚举范围校验只由 FieldSpec::enumCount 驱动：大于 0 才查 [0, enumCount)，0 表示不设上限。
TEST(PreferencesDraftValidate, IntRangeFollowsEnumCountOnly) {
    FieldTable fields(2);
    fields[0] = FieldSpec{"bounded", Kind::Int, Value::Int(0), 3, false, nullptr};
    fields[1] = FieldSpec{"unbounded", Kind::Int, Value::Int(0), 0, false, nullptr};
    Snapshot draft = Defaults(fields);

    for (int value = 0; value < 3; ++value) {
        draft.SetInt(0, value);
        EXPECT_TRUE(Validate(fields, draft).ok()) << "value " << value;
    }
    draft.SetInt(0, 3);
    ValidationResult r = Validate(fields, draft);
    EXPECT_EQ(r.error, ValidationError::OutOfRange);
    EXPECT_EQ(r.field, 0u);
    draft.SetInt(0, -1);
    EXPECT_EQ(Validate(fields, draft).error, ValidationError::OutOfRange);

    draft.SetInt(0, 1);
    draft.SetInt(1, 100000);
    EXPECT_TRUE(Validate(fields, draft).ok());
    draft.SetInt(1, -100000);
    EXPECT_TRUE(Validate(fields, draft).ok());
}

TEST(PreferencesDraftValidate, TemplateNameRuleMatchesCreateTemplate) {
    EXPECT_TRUE(IsValidTemplateName("default"));
    EXPECT_TRUE(IsValidTemplateName("My_Theme-2"));
    EXPECT_FALSE(IsValidTemplateName(""));
    EXPECT_FALSE(IsValidTemplateName("with space"));
    EXPECT_FALSE(IsValidTemplateName("dot.name"));
    EXPECT_FALSE(IsValidTemplateName("slash/name"));
    // 非 ASCII 字节为负值：必须走 unsigned char 才不是未定义行为，且判为非法
    EXPECT_FALSE(IsValidTemplateName("\xE4\xB8\xAD"));
}

// ============================================
// 外部写入冲突
// ============================================

TEST(PreferencesDraftConflicts, NoConflictWhenConfigUnchanged) {
    const Snapshot initial = NonDefaultInitial();
    Snapshot draft = initial;
    draft.SetInt(Backdrop, 1);
    EXPECT_TRUE(DetectConflicts(initial, draft, initial).empty());
}

TEST(PreferencesDraftConflicts, ExternalWriteOfSameValueIsNotConflict) {
    const Snapshot initial = NonDefaultInitial();
    Snapshot draft = initial;
    draft.SetInt(Backdrop, 1);
    Snapshot current = initial;
    current.SetInt(Backdrop, 1);  // 别人已经写成了草稿要写的值
    EXPECT_TRUE(DetectConflicts(initial, draft, current).empty());
}

TEST(PreferencesDraftConflicts, ExternalWriteOfDifferentValueOnEditedFieldIsConflict) {
    const Snapshot initial = NonDefaultInitial();
    Snapshot draft = initial;
    draft.SetInt(Backdrop, 1);
    Snapshot current = initial;
    current.SetInt(Backdrop, 4);
    const std::vector<size_t> conflicts = DetectConflicts(initial, draft, current);
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0], static_cast<size_t>(Backdrop));
}

TEST(PreferencesDraftConflicts, StringFieldConflictComparesValues) {
    const Snapshot initial = NonDefaultInitial();   // template = night
    Snapshot draft = initial;
    draft.SetString(Template, "mine");
    Snapshot current = initial;
    current.SetString(Template, "theirs");
    const std::vector<size_t> conflicts = DetectConflicts(initial, draft, current);
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0], static_cast<size_t>(Template));
    // 外部写成草稿要写的名字，或根本没动，都不是冲突。
    current.SetString(Template, "mine");
    EXPECT_TRUE(DetectConflicts(initial, draft, current).empty());
    current.SetString(Template, "night");
    EXPECT_TRUE(DetectConflicts(initial, draft, current).empty());
}

TEST(PreferencesDraftConflicts, UneditedFieldsNeverConflict) {
    const Snapshot initial = NonDefaultInitial();
    Snapshot draft = initial;
    draft.SetInt(Backdrop, 1);  // 只编辑了背景
    Snapshot current = initial;
    current.SetString(Template, "changed-elsewhere");
    current.SetInt(Language, 1);
    EXPECT_TRUE(DetectConflicts(initial, draft, current).empty());
}

TEST(PreferencesDraftConflicts, ReportsEveryConflictingEditedField) {
    const Snapshot initial = NonDefaultInitial();
    Snapshot draft = initial;
    draft.SetString(Template, "mine");
    draft.SetInt(Language, 0);
    Snapshot current = initial;
    current.SetString(Template, "theirs");
    current.SetInt(Language, 1);
    const std::vector<size_t> conflicts = DetectConflicts(initial, draft, current);
    EXPECT_EQ(conflicts.size(), 2u);
    EXPECT_TRUE(Contains(conflicts, Template));
    EXPECT_TRUE(Contains(conflicts, Language));
}

// ============================================
// 重启判定
// ============================================

TEST(PreferencesDraftRestart, OnlyRestartFieldsCompareAgainstStartup) {
    const FieldTable fields = Fields();
    const Snapshot startup = Defaults(fields);   // 启动时语言 = Auto
    Snapshot draft = startup;
    EXPECT_FALSE(NeedsRestart(fields, startup, draft));

    // 背景效果即时生效，改它不要求重启。
    draft.SetInt(Backdrop, 3);
    EXPECT_FALSE(NeedsRestart(fields, startup, draft));

    // 语言与启动值不同即要求重启；改回启动值就不再要求。
    draft.SetInt(Language, 2);
    EXPECT_TRUE(NeedsRestart(fields, startup, draft));
    draft.SetInt(Language, 0);
    EXPECT_FALSE(NeedsRestart(fields, startup, draft));
}

// ============================================
// 空字段表
// ============================================

TEST(PreferencesDraftFieldTable, EmptyTableIsCleanValidAndConflictFree) {
    const FieldTable empty;
    const Snapshot s = Defaults(empty);
    EXPECT_EQ(s.size(), 0u);
    EXPECT_TRUE(Diff(s, s).empty());
    EXPECT_FALSE(IsDirty(s, s));
    EXPECT_TRUE(Validate(empty, s).ok());
    EXPECT_TRUE(DetectConflicts(s, s, s).empty());
    EXPECT_FALSE(NeedsRestart(empty, s, s));
}

// ============================================
// 存储值清洗
// ============================================

TEST(PreferencesDraftSanitize, OutOfRangeStoredValueShowsAsFallback) {
    EXPECT_EQ(SanitizeEnum(7, 5, 1), 1);
    EXPECT_EQ(SanitizeEnum(-2, 5, 1), 1);
    EXPECT_EQ(SanitizeEnum(4, 5, 1), 4);
    EXPECT_EQ(SanitizeEnum(0, 3, 0), 0);
}

// ============================================
// setter 失败后的快照回读
// ============================================

// apply() 某个字段写失败时，页面把基线换成读回的真实配置、草稿保持不动：
// 已写成的字段不再 dirty，写失败和还没轮到的字段继续 dirty，用户可直接再按 Apply。
TEST(PreferencesDraftWriteFailure, RebaseOnReadBackKeepsOnlyUnwrittenFieldsDirty) {
    const Snapshot initial = NonDefaultInitial();
    Snapshot draft = initial;
    draft.SetString(Template, "mine");   // 第一个字段写成
    draft.SetInt(Backdrop, 1);           // 第二个字段写失败
    draft.SetInt(Language, 0);           // 没轮到
    Snapshot readBack = initial;
    readBack.SetString(Template, "mine");
    const std::vector<size_t> remaining = Diff(readBack, draft);
    ASSERT_EQ(remaining.size(), 2u);
    EXPECT_EQ(remaining[0], static_cast<size_t>(Backdrop));
    EXPECT_EQ(remaining[1], static_cast<size_t>(Language));
    EXPECT_TRUE(IsDirty(readBack, draft));
    // 读回的基线就是当前配置，再按 Apply 不会被判成冲突。
    EXPECT_TRUE(DetectConflicts(readBack, draft, readBack).empty());
}

TEST(PreferencesDraftWriteFailure, RebaseAfterEverythingWrittenIsClean) {
    const Snapshot initial = NonDefaultInitial();
    Snapshot draft = initial;
    draft.SetInt(Backdrop, 1);
    const Snapshot readBack = draft;   // 全部写成后读回
    EXPECT_FALSE(IsDirty(readBack, draft));
    EXPECT_TRUE(Diff(readBack, draft).empty());
}
