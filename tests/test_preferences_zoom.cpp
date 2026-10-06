// test_preferences_zoom.cpp - 默认内容缩放的换算与性能子页字段表
//
// 直接链接生产头 prefs_zoom::* 与 prefs_fields::performance::Table()，不在测试内重写被测逻辑。
// 断言依据是 docs/preferences/DESIGN.md 第 11.4 节：百分比 50–200、步进 25、默认 100，
// WebView2 因子 = 百分比 / 100；只有预热开关影响重启。
#include "pch.h"
#include "../src/prefs/PreferencesFields.h"
#include "../src/prefs/PreferencesZoom.h"

using namespace prefs_zoom;

// ============================================
// 取值范围
// ============================================

TEST(PreferencesZoom, ConstantsMatchDesignRange) {
    EXPECT_EQ(kMinPercent, 50);
    EXPECT_EQ(kMaxPercent, 200);
    EXPECT_EQ(kStepPercent, 25);
    EXPECT_EQ(kDefaultPercent, 100);
    EXPECT_EQ(kOptionCount, 7);
}

TEST(PreferencesZoom, ValidPercentIsExactlyADropdownOption) {
    EXPECT_TRUE(IsValidPercent(50));
    EXPECT_TRUE(IsValidPercent(75));
    EXPECT_TRUE(IsValidPercent(100));
    EXPECT_TRUE(IsValidPercent(125));
    EXPECT_TRUE(IsValidPercent(175));
    EXPECT_TRUE(IsValidPercent(200));
    EXPECT_FALSE(IsValidPercent(40));
    EXPECT_FALSE(IsValidPercent(210));
    EXPECT_FALSE(IsValidPercent(105));   // 在范围内但不在步进上
    EXPECT_FALSE(IsValidPercent(110));   // 步进 10 时代的取值，现在不是下拉项
    EXPECT_FALSE(IsValidPercent(0));
    EXPECT_FALSE(IsValidPercent(-100));
}

// ============================================
// 清洗存储值
// ============================================

TEST(PreferencesZoom, SanitizeClampsToRange) {
    EXPECT_EQ(SanitizePercent(0), kMinPercent);
    EXPECT_EQ(SanitizePercent(49), kMinPercent);
    EXPECT_EQ(SanitizePercent(-30), kMinPercent);
    EXPECT_EQ(SanitizePercent(201), kMaxPercent);
    EXPECT_EQ(SanitizePercent(1000), kMaxPercent);
}

TEST(PreferencesZoom, SanitizeRoundsToNearestStep) {
    EXPECT_EQ(SanitizePercent(104), 100);
    EXPECT_EQ(SanitizePercent(112), 100);
    EXPECT_EQ(SanitizePercent(113), 125);   // 正中间 112.5 向上
    EXPECT_EQ(SanitizePercent(110), 100);   // 步进 10 时代存下的值落到最近的档位
    EXPECT_EQ(SanitizePercent(94), 100);
    EXPECT_EQ(SanitizePercent(87), 75);
    EXPECT_EQ(SanitizePercent(199), 200);
    EXPECT_EQ(SanitizePercent(51), 50);
}

TEST(PreferencesZoom, SanitizeLeavesValidOptionsUnchanged) {
    for (int index = 0; index < kOptionCount; ++index) {
        const int percent = PercentAt(index);
        EXPECT_EQ(SanitizePercent(percent), percent);
    }
}

// ============================================
// 因子与下拉下标
// ============================================

TEST(PreferencesZoom, FactorIsPercentOverHundredAfterSanitizing) {
    EXPECT_DOUBLE_EQ(FactorFromPercent(100), 1.0);
    EXPECT_DOUBLE_EQ(FactorFromPercent(150), 1.5);
    EXPECT_DOUBLE_EQ(FactorFromPercent(50), 0.5);
    EXPECT_DOUBLE_EQ(FactorFromPercent(200), 2.0);
    EXPECT_DOUBLE_EQ(FactorFromPercent(30), 0.5);    // 越界先钳到 50
    EXPECT_DOUBLE_EQ(FactorFromPercent(104), 1.0);   // 先对齐到步进
}

TEST(PreferencesZoom, IndexAndPercentRoundTripOverAllOptions) {
    for (int index = 0; index < kOptionCount; ++index) {
        const int percent = PercentAt(index);
        EXPECT_TRUE(IsValidPercent(percent)) << percent;
        EXPECT_EQ(IndexOf(percent), index) << percent;
    }
    EXPECT_EQ(PercentAt(0), kMinPercent);
    EXPECT_EQ(PercentAt(kOptionCount - 1), kMaxPercent);
    EXPECT_EQ(IndexOf(kDefaultPercent), 2);   // 50 / 75 / 100
}

TEST(PreferencesZoom, OutOfRangeIndexFallsBackToDefault) {
    EXPECT_EQ(PercentAt(-1), kDefaultPercent);    // CB_GETCURSEL 没有选中项时返回 -1
    EXPECT_EQ(PercentAt(kOptionCount), kDefaultPercent);
    EXPECT_EQ(IndexOf(-5), 0);                     // 越界百分比先钳到范围再取下标
    EXPECT_EQ(IndexOf(999), kOptionCount - 1);
}

// ============================================
// 性能子页字段表
// ============================================

namespace performance = prefs_fields::performance;

TEST(PreferencesPerformanceFields, DefaultsAndRestartFlagsMatchDesign) {
    const prefs_draft::FieldTable fields = performance::Table();
    const prefs_draft::Snapshot d = prefs_draft::Defaults(fields);
    ASSERT_EQ(d.size(), static_cast<size_t>(performance::Count));
    EXPECT_TRUE(d.GetBool(performance::Preheat));
    EXPECT_TRUE(d.GetBool(performance::DeepSuspend));
    EXPECT_EQ(d.GetInt(performance::ZoomPercent), kDefaultPercent);
    EXPECT_TRUE(prefs_draft::Validate(fields, d).ok());
    // 预热在启动期读取；深度挂起与缩放都在运行期消费。
    for (size_t i = 0; i < fields.size(); ++i) {
        EXPECT_EQ(fields[i].affectsRestart, i == performance::Preheat) << fields[i].key;
    }
}

TEST(PreferencesPerformanceFields, ZoomOutsideDropdownOptionsIsOutOfRange) {
    const prefs_draft::FieldTable fields = performance::Table();
    prefs_draft::Snapshot draft = prefs_draft::Defaults(fields);

    draft.SetInt(performance::ZoomPercent, 150);
    EXPECT_TRUE(prefs_draft::Validate(fields, draft).ok());

    for (int bad : {105, 110, 40, 210, 0}) {
        draft.SetInt(performance::ZoomPercent, bad);
        const prefs_draft::ValidationResult r = prefs_draft::Validate(fields, draft);
        EXPECT_EQ(r.error, prefs_draft::ValidationError::OutOfRange) << bad;
        EXPECT_EQ(r.field, static_cast<size_t>(performance::ZoomPercent)) << bad;
    }
}

TEST(PreferencesPerformanceFields, RestartOnlyWhenPreheatLeavesStartupValue) {
    const prefs_draft::FieldTable fields = performance::Table();
    const prefs_draft::Snapshot startup = prefs_draft::Defaults(fields);
    prefs_draft::Snapshot draft = startup;

    draft.SetBool(performance::DeepSuspend, false);
    draft.SetInt(performance::ZoomPercent, 125);
    EXPECT_FALSE(prefs_draft::NeedsRestart(fields, startup, draft));

    draft.SetBool(performance::Preheat, false);
    EXPECT_TRUE(prefs_draft::NeedsRestart(fields, startup, draft));
    draft.SetBool(performance::Preheat, true);
    EXPECT_FALSE(prefs_draft::NeedsRestart(fields, startup, draft));
}

TEST(PreferencesPerformanceFields, ZoomEditIsDetectedAsDirtyAndConflict) {
    const prefs_draft::FieldTable fields = performance::Table();
    const prefs_draft::Snapshot initial = prefs_draft::Defaults(fields);
    prefs_draft::Snapshot draft = initial;
    draft.SetInt(performance::ZoomPercent, 125);
    EXPECT_TRUE(prefs_draft::IsDirty(initial, draft));
    const std::vector<size_t> changed = prefs_draft::Diff(initial, draft);
    ASSERT_EQ(changed.size(), 1u);
    EXPECT_EQ(changed[0], static_cast<size_t>(performance::ZoomPercent));

    prefs_draft::Snapshot current = initial;
    current.SetInt(performance::ZoomPercent, 175);   // 别处改成了第三个值
    const std::vector<size_t> conflicts = prefs_draft::DetectConflicts(initial, draft, current);
    ASSERT_EQ(conflicts.size(), 1u);
    EXPECT_EQ(conflicts[0], static_cast<size_t>(performance::ZoomPercent));
}
