// test_preferences_layout.cpp - Preferences 页面布局纯逻辑覆盖
//
// 直接链接生产头 prefs_layout::*，不在测试内重写被测逻辑。
// 断言依据是 docs/preferences/DESIGN.md 第 2.2 / 2.3 节：
//   - 控件右边缘不得超过内容区右边缘；
//   - 标签列取同组最宽标签，复选框独占内容宽度；
//   - 变窄时先折按钮、再把标签移到控件上方，不缩字体不裁文字。
#include "pch.h"
#include "../src/core/PreferencesLayout.h"
#include "../src/core/PreferencesLayoutBuilder.h"
#include <map>

using namespace prefs_layout;

namespace {

// 三档文字缩放对应的对话框基准单位（平均字符宽 / 字符高，像素）。
struct BaseUnits {
    int x;
    int y;
    const char* name;
};
const BaseUnits kBaseUnits[] = {
    {7, 15, "100%"},
    {10, 22, "150%"},
    {14, 30, "200%"},
};

// 两种语言下同组最宽标签的近似像素宽度（按 100% 字体，随基准单位缩放）。
struct LabelSet {
    int widestAt100;
    const char* name;
};
const LabelSet kLabelSets[] = {
    {96, "English"},  // "Default window backdrop"
    {70, "Chinese"},  // "默认窗口背景效果"
};

int Scale(int value, const BaseUnits& u) { return MulDivRound(value, u.x, 7); }

}  // namespace

// ============================================
// DLU 换算
// ============================================

TEST(PreferencesLayoutDlu, MatchesMapDialogRectRounding) {
    // 4 DLU == 一个平均字符宽；8 DLU == 一个字符高。
    EXPECT_EQ(DluToPxX(4, 7), 7);
    EXPECT_EQ(DluToPxY(8, 15), 15);
    // 7 DLU * 7 px / 4 == 12.25 -> 12；7 * 9 / 4 == 15.75 -> 16
    EXPECT_EQ(DluToPxX(7, 7), 12);
    EXPECT_EQ(DluToPxX(7, 9), 16);
    // .5 远离零
    EXPECT_EQ(DluToPxX(2, 7), 4);  // 3.5 -> 4
}

TEST(PreferencesLayoutDlu, NonPositiveBaseUnitFallsBackToSystemDefault) {
    EXPECT_EQ(DluToPxX(4, 0), 8);
    EXPECT_EQ(DluToPxY(8, -3), 16);
}

TEST(PreferencesLayoutDlu, MetricsScaleWithBaseUnits) {
    // Windows 头把 small 定义成宏，这里不用它作变量名。
    // 基准单位取 4 / 8 的整倍数，换算无取整，倍数关系才严格成立。
    const Metrics at100 = MakeMetrics(8, 16);
    const Metrics at200 = MakeMetrics(16, 32);
    EXPECT_EQ(at100.outerMargin, 14);
    EXPECT_EQ(at100.controlHeight, 28);
    EXPECT_EQ(at200.outerMargin, at100.outerMargin * 2);
    EXPECT_EQ(at200.controlHeight, at100.controlHeight * 2);
    EXPECT_EQ(at200.minFieldWidth, at100.minFieldWidth * 2);
    EXPECT_GT(at100.minFieldWidth, 0);
    EXPECT_GT(at100.checkHeight, 0);
    
    // 非整倍数基准下每个值各自四舍五入，允许相差 1 像素。
    const Metrics odd100 = MakeMetrics(7, 15);
    const Metrics odd200 = MakeMetrics(14, 30);
    EXPECT_LE(std::abs(odd200.outerMargin - odd100.outerMargin * 2), 1);
    EXPECT_LE(std::abs(odd200.controlHeight - odd100.controlHeight * 2), 1);
}

// ============================================
// 分组框与标签列
// ============================================

TEST(PreferencesLayoutGroup, WideGroupKeepsLabelsBeside) {
    const Metrics m = MakeMetrics(7, 15);
    const GroupPlan plan = PlanGroup(14, 600, 96, m);
    EXPECT_FALSE(plan.labelsAbove);
    EXPECT_EQ(plan.labelColumnWidth, 96);
    EXPECT_EQ(plan.contentLeft, 14 + m.groupPadX);
    EXPECT_EQ(plan.contentRight, 600 - m.groupPadX);
    EXPECT_EQ(plan.controlLeft, plan.contentLeft + 96 + m.colGap);
}

TEST(PreferencesLayoutGroup, NarrowGroupMovesLabelsAbove) {
    const Metrics m = MakeMetrics(7, 15);
    // 内容宽度减去标签与间距后不足 minFieldWidth 才上移。
    const int frameRight = 14 + m.groupPadX * 2 + 96 + m.colGap + m.minFieldWidth - 1;
    const GroupPlan plan = PlanGroup(14, frameRight, 96, m);
    EXPECT_TRUE(plan.labelsAbove);
    EXPECT_EQ(plan.labelColumnWidth, 0);
    EXPECT_EQ(plan.controlLeft, plan.contentLeft);
}

TEST(PreferencesLayoutGroup, ExactMinimumFieldWidthStaysBeside) {
    const Metrics m = MakeMetrics(7, 15);
    const int frameRight = 14 + m.groupPadX * 2 + 96 + m.colGap + m.minFieldWidth;
    const GroupPlan plan = PlanGroup(14, frameRight, 96, m);
    EXPECT_FALSE(plan.labelsAbove);
}

TEST(PreferencesLayoutGroup, NoLabelsMeansFullWidthControls) {
    const Metrics m = MakeMetrics(7, 15);
    const GroupPlan plan = PlanGroup(14, 200, 0, m);
    EXPECT_FALSE(plan.labelsAbove);
    EXPECT_EQ(plan.controlLeft, plan.contentLeft);
}

TEST(PreferencesLayoutGroup, CollapsedFrameNeverInvertsContentEdges) {
    const Metrics m = MakeMetrics(7, 15);
    const GroupPlan plan = PlanGroup(14, 14, 50, m);
    EXPECT_GE(plan.contentRight, plan.contentLeft);
    EXPECT_GE(plan.controlLeft, plan.contentLeft);
}

// ============================================
// 标签 + 控件行
// ============================================

TEST(PreferencesLayoutRow, BesideLabelIsVerticallyCentredOnControl) {
    const Metrics m = MakeMetrics(7, 15);
    const GroupPlan plan = PlanGroup(14, 600, 96, m);
    const LabelControlRow row = PlaceLabelControl(plan, 100, 80, m.controlHeight, m);
    EXPECT_EQ(row.control.top, 100);
    EXPECT_EQ(row.control.left, plan.controlLeft);
    EXPECT_EQ(row.control.right, plan.contentRight);
    EXPECT_EQ(row.label.left, plan.contentLeft);
    EXPECT_EQ(row.label.Width(), 80);
    EXPECT_GE(row.label.top, row.control.top);
    EXPECT_LE(row.label.bottom, row.control.bottom);
    EXPECT_EQ(row.bottom, row.control.bottom);
}

TEST(PreferencesLayoutRow, AboveLabelPrecedesFullWidthControl) {
    const Metrics m = MakeMetrics(7, 15);
    const GroupPlan plan = PlanGroup(14, 14 + m.groupPadX * 2 + 60, 96, m);
    ASSERT_TRUE(plan.labelsAbove);
    const LabelControlRow row = PlaceLabelControl(plan, 100, 96, m.controlHeight, m);
    EXPECT_EQ(row.label.top, 100);
    EXPECT_GE(row.control.top, row.label.bottom);
    EXPECT_EQ(row.control.left, plan.contentLeft);
    EXPECT_EQ(row.control.right, plan.contentRight);
    EXPECT_EQ(row.bottom, row.control.bottom);
}

// ============================================
// [字段][按钮]
// ============================================

TEST(PreferencesLayoutFieldLine, ButtonStaysOnLineWhenFieldKeepsMinimumWidth) {
    const Metrics m = MakeMetrics(7, 15);
    const int left = 30;
    const int right = left + m.minFieldWidth + m.colGap + 60;
    const FieldLine line = PlaceFieldLine(left, right, 10, m.controlHeight, 60, m.buttonHeight, m);
    EXPECT_FALSE(line.wrapped);
    EXPECT_EQ(line.field.left, left);
    EXPECT_EQ(line.field.right, right - 60 - m.colGap);
    EXPECT_GE(line.field.Width(), m.minFieldWidth);
    EXPECT_EQ(line.button.left, right - 60);
    EXPECT_EQ(line.button.right, right);
    EXPECT_EQ(line.button.top, 10);
}

TEST(PreferencesLayoutFieldLine, ButtonWrapsBelowWhenFieldWouldShrinkTooFar) {
    const Metrics m = MakeMetrics(7, 15);
    const int left = 30;
    const int right = left + m.minFieldWidth + m.colGap + 60 - 1;
    const FieldLine line = PlaceFieldLine(left, right, 10, m.controlHeight, 60, m.buttonHeight, m);
    EXPECT_TRUE(line.wrapped);
    EXPECT_EQ(line.field.left, left);
    EXPECT_EQ(line.field.right, right);
    EXPECT_EQ(line.button.left, left);
    EXPECT_EQ(line.button.Width(), 60);
    EXPECT_GE(line.button.top, line.field.bottom + m.rowGap);
    EXPECT_EQ(line.bottom, line.button.bottom);
}

TEST(PreferencesLayoutFieldLine, OversizedButtonIsClampedToLineWidth) {
    const Metrics m = MakeMetrics(7, 15);
    const FieldLine line = PlaceFieldLine(30, 80, 10, m.controlHeight, 200, m.buttonHeight, m);
    EXPECT_TRUE(line.wrapped);
    EXPECT_LE(line.button.right, 80);
    EXPECT_LE(line.field.right, 80);
}

TEST(PreferencesLayoutFieldLine, NoButtonMeansFieldSpansLine) {
    const Metrics m = MakeMetrics(7, 15);
    const FieldLine line = PlaceFieldLine(30, 400, 10, m.controlHeight, 0, m.buttonHeight, m);
    EXPECT_FALSE(line.wrapped);
    EXPECT_EQ(line.field.left, 30);
    EXPECT_EQ(line.field.right, 400);
    EXPECT_EQ(line.bottom, 10 + m.controlHeight);
}

// ============================================
// 按钮行
// ============================================

TEST(PreferencesLayoutButtons, FitOnOneLineWhenRoomAllows) {
    const Metrics m = MakeMetrics(7, 15);
    const ButtonRow row = PlaceButtons(20, 400, 50, {120, 110}, m.buttonHeight, m);
    ASSERT_EQ(row.buttons.size(), 2u);
    EXPECT_EQ(row.buttons[0].left, 20);
    EXPECT_EQ(row.buttons[0].right, 140);
    EXPECT_EQ(row.buttons[1].left, 140 + m.colGap);
    EXPECT_EQ(row.buttons[1].top, 50);
    EXPECT_EQ(row.bottom, 50 + m.buttonHeight);
}

TEST(PreferencesLayoutButtons, SecondButtonWrapsWhenItWouldOverflow) {
    const Metrics m = MakeMetrics(7, 15);
    const ButtonRow row = PlaceButtons(20, 250, 50, {120, 120}, m.buttonHeight, m);
    ASSERT_EQ(row.buttons.size(), 2u);
    EXPECT_EQ(row.buttons[1].left, 20);
    EXPECT_EQ(row.buttons[1].top, 50 + m.buttonHeight + m.rowGap);
    EXPECT_LE(row.buttons[1].right, 250);
    EXPECT_EQ(row.bottom, row.buttons[1].bottom);
}

TEST(PreferencesLayoutButtons, SingleOversizedButtonIsClampedNotDropped) {
    const Metrics m = MakeMetrics(7, 15);
    const ButtonRow row = PlaceButtons(20, 100, 50, {300}, m.buttonHeight, m);
    ASSERT_EQ(row.buttons.size(), 1u);
    EXPECT_EQ(row.buttons[0].left, 20);
    EXPECT_EQ(row.buttons[0].right, 100);
}

TEST(PreferencesLayoutButtons, EmptyRowHasZeroHeight) {
    const Metrics m = MakeMetrics(7, 15);
    const ButtonRow row = PlaceButtons(20, 100, 50, {}, m.buttonHeight, m);
    EXPECT_TRUE(row.buttons.empty());
    EXPECT_EQ(row.bottom, 50);
}

// ============================================
// 全宽控件与分组框收尾
// ============================================

TEST(PreferencesLayoutFullWidth, CheckBoxSpansContentWidthWithoutLabelColumn) {
    const Metrics m = MakeMetrics(7, 15);
    const GroupPlan plan = PlanGroup(14, 600, 96, m);
    const Rect check = PlaceFullWidth(plan, 40, m.checkHeight);
    EXPECT_EQ(check.left, plan.contentLeft);
    EXPECT_EQ(check.right, plan.contentRight);
    EXPECT_EQ(check.Height(), m.checkHeight);
}

TEST(PreferencesLayoutFullWidth, GroupFrameEnclosesContent) {
    const Metrics m = MakeMetrics(7, 15);
    const GroupPlan plan = PlanGroup(14, 600, 96, m);
    const int contentTop = GroupContentTop(20, m);
    const Rect frame = FinishGroup(plan, 20, contentTop + 100, m);
    EXPECT_EQ(frame.top, 20);
    EXPECT_EQ(frame.left, 14);
    EXPECT_EQ(frame.right, 600);
    EXPECT_EQ(frame.bottom, contentTop + 100 + m.groupPadBottom);
    EXPECT_GE(contentTop, frame.top + m.groupPadTop);
}

// ============================================
// 三档宽度 x 两种语言 x 三档缩放：任何组合控件都不越界
// ============================================

TEST(PreferencesLayoutMatrix, EveryControlStaysInsideContentArea) {
    // 页面宽度按 100% 下的三档：宿主默认、变窄、最窄。
    const int kPageWidthsAt100[] = {580, 400, 300};
    const int kButtonAt100 = 62;  // "Manage..." 一类按钮
    
    for (const BaseUnits& units : kBaseUnits) {
        const Metrics m = MakeMetrics(units.x, units.y);
        for (const LabelSet& labels : kLabelSets) {
            const int widest = Scale(labels.widestAt100, units);
            for (int widthAt100 : kPageWidthsAt100) {
                const int pageWidth = Scale(widthAt100, units);
                const Rect page{0, 0, pageWidth, 100000};
                const int frameLeft = m.outerMargin;
                const int frameRight = pageWidth - m.outerMargin;
                const GroupPlan plan = PlanGroup(frameLeft, frameRight, widest, m);
                const Rect content{plan.contentLeft, 0, plan.contentRight, 100000};
                
                int y = GroupContentTop(m.outerMargin, m);
                const LabelControlRow row = PlaceLabelControl(plan, y, widest, m.controlHeight, m);
                const FieldLine line = PlaceFieldLine(row.control.left, row.control.right, row.control.top,
                    m.controlHeight, Scale(kButtonAt100, units), m.buttonHeight, m);
                const Rect check = PlaceFullWidth(plan, line.bottom + m.rowGap, m.checkHeight);
                const ButtonRow buttons = PlaceButtons(plan.contentLeft, plan.contentRight, check.bottom + m.rowGap,
                    {Scale(140, units), Scale(110, units)}, m.buttonHeight, m);
                const Rect frame = FinishGroup(plan, m.outerMargin, buttons.bottom, m);
                
                SCOPED_TRACE(std::string(units.name) + " / " + labels.name + " / width " + std::to_string(widthAt100));
                EXPECT_TRUE(RectWithin(row.label, content));
                EXPECT_TRUE(RectWithin(line.field, content));
                EXPECT_TRUE(RectWithin(line.button, content));
                EXPECT_TRUE(RectWithin(check, content));
                for (const Rect& b : buttons.buttons) EXPECT_TRUE(RectWithin(b, content));
                EXPECT_TRUE(RectWithin(frame, page));
                EXPECT_GE(line.field.Width(), (std::min)(m.minFieldWidth, content.Width()));
            }
        }
    }
}

TEST(PreferencesLayoutMatrix, NarrowingWrapsButtonBeforeMovingLabelsAbove) {
    const Metrics m = MakeMetrics(7, 15);
    const int widest = 96;
    const int button = 62;
    bool sawBesideUnwrapped = false;
    bool sawBesideWrapped = false;
    bool sawAbove = false;
    bool aboveSeen = false;
    
    // 从宽到窄逐像素收缩：状态只能沿 并排+同行 -> 并排+按钮下折 -> 标签上移 单向推进。
    for (int pageWidth = 700; pageWidth >= 120; --pageWidth) {
        const GroupPlan plan = PlanGroup(m.outerMargin, pageWidth - m.outerMargin, widest, m);
        const LabelControlRow row = PlaceLabelControl(plan, 0, widest, m.controlHeight, m);
        const FieldLine line = PlaceFieldLine(row.control.left, row.control.right, row.control.top,
            m.controlHeight, button, m.buttonHeight, m);
        if (plan.labelsAbove) {
            aboveSeen = true;
            sawAbove = true;
        } else {
            EXPECT_FALSE(aboveSeen) << "labels moved back beside controls while narrowing at " << pageWidth;
            if (line.wrapped) {
                sawBesideWrapped = true;
            } else {
                EXPECT_FALSE(sawBesideWrapped) << "button un-wrapped while narrowing at " << pageWidth;
                sawBesideUnwrapped = true;
            }
        }
    }
    EXPECT_TRUE(sawBesideUnwrapped);
    EXPECT_TRUE(sawBesideWrapped);
    EXPECT_TRUE(sawAbove);
}

// ============================================
// 描述驱动铺排与手写铺排对拍：总览页四组
// ============================================

namespace {

// 对拍场景取「窗口行为」组尚在总览页时的四组布局（ID 与当时的 ControlIds 同值），
// 因为它覆盖了 builder 的全部六种行；生产页面后来的分组变化不影响这份基线。
enum OverviewIds {
    kGroupTemplate = 1000, kStaticTemplate, kComboTemplate, kBtnManage, kStaticPath, kEditPath, kBtnOpenFolder,
    kGroupWindow = 1010, kChkStartWithFoobar, kChkRememberPosition, kStaticRestoreNote,
    kGroupAppearance = 1020, kStaticLanguage, kComboLanguage, kStaticBackdrop, kComboBackdrop, kStaticLanguageNote,
    kGroupTools = 1050, kStaticDevServer, kStaticDevServerStatus, kBtnAdvanced, kBtnShowApiList, kStaticToolsNote,
};

// 文字度量的替身：宽度按控件给定的 100% 像素值随基准单位缩放；折行高度按宽度整除得到行数。
struct FakeText {
    int baseUnitX = 7;
    std::map<int, int> widthAt100 = {
        {kStaticTemplate, 50}, {kBtnManage, 62}, {kStaticPath, 90}, {kBtnOpenFolder, 48},
        {kStaticLanguage, 110}, {kStaticLanguageNote, 620}, {kStaticBackdrop, 140},
        {kChkRememberPosition, 200}, {kChkStartWithFoobar, 520}, {kStaticRestoreNote, 700},
        {kStaticDevServer, 120}, {kBtnAdvanced, 150}, {kBtnShowApiList, 110}, {kStaticToolsNote, 840},
    };

    int Width(int id) const {
        const auto it = widthAt100.find(id);
        return it == widthAt100.end() ? 0 : MulDivRound(it->second, baseUnitX, 7);
    }
    int WrappedHeight(int id, int width, int lineHeight) const {
        const int text = Width(id);
        if (text <= 0 || width <= 0) return lineHeight;
        const int lines = (text + width - 1) / width;
        return (std::max)(1, lines) * lineHeight;
    }
};

struct OverviewLayout {
    std::map<int, Rect> rects;
    int contentBottom = 0;  // 最后一个分组框的底边（滚动坐标）
    Rect firstFrame;
};

// 描述驱动改造前总览页的手写铺排（逐行沿用原 LayoutControls 的调用序列），作为对拍基线。
OverviewLayout HandwrittenOverview(const Metrics& m, int baseUnitX, int clientWidth, int scrollY, const FakeText& text) {
    const int frameLeft = m.outerMargin;
    const int frameRight = (std::max)(frameLeft, clientWidth - m.outerMargin);
    const int comboWindowHeight = m.controlHeight * 8;
    const int buttonMinWidth = DluToPxX(50, baseUnitX);
    const int buttonTextPad = DluToPxX(12, baseUnitX);
    const int checkGlyphWidth = DluToPxX(12, baseUnitX);

    OverviewLayout out;
    auto place = [&](int id, const Rect& r) { out.rects[id] = r; };
    auto textWidth = [&](int id) { return text.Width(id); };
    auto buttonWidth = [&](int id) { return (std::max)(buttonMinWidth, textWidth(id) + buttonTextPad); };
    auto wrappedHeight = [&](int id, int width) { return text.WrappedHeight(id, width, m.textHeight); };

    int y = m.outerMargin - scrollY;

    {
        const int frameTop = y;
        const int widest = (std::max)(textWidth(kStaticTemplate), textWidth(kStaticPath));
        const GroupPlan plan = PlanGroup(frameLeft, frameRight, widest, m);
        int cy = GroupContentTop(frameTop, m);

        LabelControlRow row = PlaceLabelControl(plan, cy, textWidth(kStaticTemplate), m.controlHeight, m);
        FieldLine line = PlaceFieldLine(row.control.left, row.control.right, row.control.top, m.controlHeight,
            buttonWidth(kBtnManage), m.buttonHeight, m);
        place(kStaticTemplate, row.label);
        place(kComboTemplate, {line.field.left, line.field.top, line.field.right, line.field.top + comboWindowHeight});
        place(kBtnManage, line.button);
        cy = (std::max)(row.label.bottom, line.bottom) + m.rowGap;

        row = PlaceLabelControl(plan, cy, textWidth(kStaticPath), m.controlHeight, m);
        line = PlaceFieldLine(row.control.left, row.control.right, row.control.top, m.controlHeight,
            buttonWidth(kBtnOpenFolder), m.buttonHeight, m);
        place(kStaticPath, row.label);
        place(kEditPath, line.field);
        place(kBtnOpenFolder, line.button);
        cy = (std::max)(row.label.bottom, line.bottom);

        const Rect frame = FinishGroup(plan, frameTop, cy, m);
        place(kGroupTemplate, frame);
        out.firstFrame = frame;
        y = frame.bottom + m.groupGap;
    }

    {
        const int frameTop = y;
        const int widest = (std::max)(textWidth(kStaticLanguage), textWidth(kStaticBackdrop));
        const GroupPlan plan = PlanGroup(frameLeft, frameRight, widest, m);
        int cy = GroupContentTop(frameTop, m);

        LabelControlRow row = PlaceLabelControl(plan, cy, textWidth(kStaticLanguage), m.controlHeight, m);
        place(kStaticLanguage, row.label);
        place(kComboLanguage, {row.control.left, row.control.top, row.control.right, row.control.top + comboWindowHeight});
        cy = row.bottom + m.rowGap;

        const int noteWidth = plan.contentRight - plan.contentLeft;
        const Rect note = PlaceFullWidth(plan, cy, wrappedHeight(kStaticLanguageNote, noteWidth));
        place(kStaticLanguageNote, note);
        cy = note.bottom + m.rowGap;

        row = PlaceLabelControl(plan, cy, textWidth(kStaticBackdrop), m.controlHeight, m);
        place(kStaticBackdrop, row.label);
        place(kComboBackdrop, {row.control.left, row.control.top, row.control.right, row.control.top + comboWindowHeight});
        cy = row.bottom;

        const Rect frame = FinishGroup(plan, frameTop, cy, m);
        place(kGroupAppearance, frame);
        y = frame.bottom + m.groupGap;
    }

    {
        const int frameTop = y;
        const GroupPlan plan = PlanGroup(frameLeft, frameRight, 0, m);
        int cy = GroupContentTop(frameTop, m);
        const int contentWidth = plan.contentRight - plan.contentLeft;

        auto placeCheck = [&](int id) {
            const int textHeight = wrappedHeight(id, contentWidth - checkGlyphWidth);
            const Rect r = PlaceFullWidth(plan, cy, (std::max)(m.checkHeight, textHeight));
            place(id, r);
            cy = r.bottom + m.rowGap;
        };
        placeCheck(kChkRememberPosition);
        placeCheck(kChkStartWithFoobar);

        const Rect note = PlaceFullWidth(plan, cy, wrappedHeight(kStaticRestoreNote, contentWidth));
        place(kStaticRestoreNote, note);
        cy = note.bottom;

        const Rect frame = FinishGroup(plan, frameTop, cy, m);
        place(kGroupWindow, frame);
        y = frame.bottom + m.groupGap;
    }

    {
        const int frameTop = y;
        const GroupPlan plan = PlanGroup(frameLeft, frameRight, textWidth(kStaticDevServer), m);
        int cy = GroupContentTop(frameTop, m);
        const int contentWidth = plan.contentRight - plan.contentLeft;

        LabelControlRow row = PlaceLabelControl(plan, cy, textWidth(kStaticDevServer), m.controlHeight, m);
        place(kStaticDevServer, row.label);
        const int statusTop = row.control.top + (std::max)(0, (m.controlHeight - m.textHeight) / 2);
        place(kStaticDevServerStatus, {row.control.left, statusTop, row.control.right, statusTop + m.textHeight});
        cy = row.bottom + m.rowGap;

        const ButtonRow buttons = PlaceButtons(plan.contentLeft, plan.contentRight, cy,
            {buttonWidth(kBtnAdvanced), buttonWidth(kBtnShowApiList)}, m.buttonHeight, m);
        if (buttons.buttons.size() == 2) {
            place(kBtnAdvanced, buttons.buttons[0]);
            place(kBtnShowApiList, buttons.buttons[1]);
        }
        cy = buttons.bottom + m.rowGap;

        const Rect note = PlaceFullWidth(plan, cy, wrappedHeight(kStaticToolsNote, contentWidth));
        place(kStaticToolsNote, note);
        cy = note.bottom;

        const Rect frame = FinishGroup(plan, frameTop, cy, m);
        place(kGroupTools, frame);
        y = frame.bottom;
    }

    out.contentBottom = y;
    return out;
}

// 与手写基线同一份分组与行序的声明式描述。
void DescribeOverview(PageLayoutBuilder& builder) {
    const int comboWindowHeight = builder.comboWindowHeight();

    builder.BeginGroup(kGroupTemplate, {kStaticTemplate, kStaticPath});
    builder.LabelFieldButton(kStaticTemplate, kComboTemplate, kBtnManage, comboWindowHeight);
    builder.LabelFieldButton(kStaticPath, kEditPath, kBtnOpenFolder);
    builder.EndGroup();

    builder.BeginGroup(kGroupAppearance, {kStaticLanguage, kStaticBackdrop});
    builder.LabelControl(kStaticLanguage, kComboLanguage, comboWindowHeight);
    builder.Note(kStaticLanguageNote);
    builder.LabelControl(kStaticBackdrop, kComboBackdrop, comboWindowHeight);
    builder.EndGroup();

    builder.BeginGroup(kGroupWindow, {});
    builder.CheckBox(kChkRememberPosition);
    builder.CheckBox(kChkStartWithFoobar);
    builder.Note(kStaticRestoreNote);
    builder.EndGroup();

    builder.BeginGroup(kGroupTools, {kStaticDevServer});
    builder.LabelInlineText(kStaticDevServer, kStaticDevServerStatus);
    builder.Buttons({kBtnAdvanced, kBtnShowApiList});
    builder.Note(kStaticToolsNote);
    builder.EndGroup();
}

bool SameRect(const Rect& a, const Rect& b) {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

std::string RectText(const Rect& r) {
    return "(" + std::to_string(r.left) + "," + std::to_string(r.top) + ")-(" + std::to_string(r.right) + "," +
           std::to_string(r.bottom) + ")";
}

}  // namespace

TEST(PreferencesLayoutBuilder, MatchesHandwrittenOverviewAtEveryScaleWidthAndScroll) {
    const int kPageWidthsAt100[] = {580, 400, 300};
    const int kScrollOffsets[] = {0, 37};

    for (const BaseUnits& units : kBaseUnits) {
        const Metrics m = MakeMetrics(units.x, units.y);
        FakeText text;
        text.baseUnitX = units.x;
        for (int widthAt100 : kPageWidthsAt100) {
            const int clientWidth = Scale(widthAt100, units);
            for (int scrollY : kScrollOffsets) {
                SCOPED_TRACE(std::string(units.name) + " / width " + std::to_string(widthAt100) + " / scroll " +
                             std::to_string(scrollY));
                const OverviewLayout expected = HandwrittenOverview(m, units.x, clientWidth, scrollY, text);

                TextSource source;
                source.textWidth = [&text](int id) { return text.Width(id); };
                source.wrappedHeight = [&text, &m](int id, int width) { return text.WrappedHeight(id, width, m.textHeight); };
                PageLayoutBuilder builder(m, units.x, clientWidth, scrollY, source);
                DescribeOverview(builder);

                // 每个控件恰好铺排一次，矩形逐个相同。
                const std::vector<Placement>& placements = builder.placements();
                ASSERT_EQ(placements.size(), expected.rects.size());
                std::map<int, int> seen;
                for (const Placement& p : placements) {
                    ++seen[p.id];
                    const auto it = expected.rects.find(p.id);
                    ASSERT_NE(it, expected.rects.end()) << "unexpected control " << p.id;
                    EXPECT_TRUE(SameRect(p.rect, it->second))
                        << "control " << p.id << " builder " << RectText(p.rect) << " handwritten " << RectText(it->second);
                }
                for (const auto& [id, count] : seen) EXPECT_EQ(count, 1) << "control " << id;

                EXPECT_EQ(builder.contentBottom(), expected.contentBottom);
                EXPECT_TRUE(SameRect(builder.firstFrame(), expected.firstFrame));
                EXPECT_EQ(builder.firstGroupId(), static_cast<int>(kGroupTemplate));
            }
        }
    }
}

TEST(PreferencesLayoutBuilder, EmptyDescriptionPlacesNothing) {
    const Metrics m = MakeMetrics(7, 15);
    PageLayoutBuilder builder(m, 7, 580, 0, TextSource{});
    EXPECT_TRUE(builder.placements().empty());
    // 没有分组框时内容底边就是页面顶部留白，读数里的分组框为 0。
    EXPECT_EQ(builder.contentBottom(), m.outerMargin);
    EXPECT_EQ(builder.firstGroupId(), 0);
}
