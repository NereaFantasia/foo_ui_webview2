#pragma once

// ============================================
// PreferencesLayout.h - Preferences 页面布局纯逻辑
//
// 把偏好设置页的尺寸计算从 HWND 与消息循环中剥离出来，让 GoogleTest 直接
// 覆盖真实生产符号。本头文件不依赖 Windows.h 与 foobar2000 SDK。
//
// 单位约定：所有输入输出都是像素。对话框单位（DLU）到像素的换算由
// DluToPxX / DluToPxY 完成，取整与 Win32 MapDialogRect 一致（MulDiv 四舍五入）。
// 基准单位（baseUnitX / baseUnitY）由调用方按当前字体测量后传入。
//
// 布局规则：
//   - 标签列宽取同组最宽标签；同组控件左边缘统一。
//   - 复选框、说明行独占分组框内容宽度，不为不存在的标签留空列。
//   - 页面变窄时先把 [字段][按钮] 中的按钮折到下一行，再把标签移到控件上方；
//     任何情况下控件右边缘不得超过内容区右边缘。
// ============================================

#include <algorithm>
#include <vector>

namespace prefs_layout {

struct Rect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;

    int Width() const { return right - left; }
    int Height() const { return bottom - top; }
};

// 全部为像素值，由 MakeMetrics 从 DLU 常量换算得到。
struct Metrics {
    int outerMargin = 0;     // 页面四周留白
    int groupPadX = 0;       // 分组框内左右留白
    int groupPadTop = 0;     // 分组框标题占用的顶部高度
    int groupPadBottom = 0;  // 分组框底部留白
    int rowGap = 0;          // 行间距
    int colGap = 0;          // 标签与控件、字段与按钮之间的间距
    int groupGap = 0;        // 分组框之间的间距
    int controlHeight = 0;   // 下拉框 / 编辑框高度
    int buttonHeight = 0;    // 按钮高度
    int checkHeight = 0;     // 复选框高度
    int textHeight = 0;      // 单行文字高度
    int minFieldWidth = 0;   // 字段低于此宽度时按钮下折 / 标签上移
};

// DLU 常量（Windows 对话框设计惯例：按钮 50x14，行距 4，边距 7）。
inline constexpr int kOuterMarginDlu = 7;
inline constexpr int kGroupPadXDlu = 8;
inline constexpr int kGroupPadTopDlu = 12;
inline constexpr int kGroupPadBottomDlu = 7;
inline constexpr int kRowGapDlu = 4;
inline constexpr int kColGapDlu = 6;
inline constexpr int kGroupGapDlu = 7;
inline constexpr int kControlHeightDlu = 14;
inline constexpr int kButtonHeightDlu = 14;
inline constexpr int kCheckHeightDlu = 10;
inline constexpr int kTextHeightDlu = 8;
inline constexpr int kMinFieldWidthDlu = 100;

// 与 MulDiv 相同的四舍五入（.5 远离零）。
inline int MulDivRound(int value, int numerator, int denominator) {
    if (denominator == 0) return value;
    const long long product = static_cast<long long>(value) * numerator;
    const long long half = denominator / 2;
    const long long rounded = product >= 0 ? (product + half) / denominator : (product - half) / denominator;
    return static_cast<int>(rounded);
}

// 水平 DLU -> 像素：4 DLU == 一个平均字符宽。
inline int DluToPxX(int dlu, int baseUnitX) {
    return MulDivRound(dlu, baseUnitX <= 0 ? 8 : baseUnitX, 4);
}

// 垂直 DLU -> 像素：8 DLU == 一个字符高。
inline int DluToPxY(int dlu, int baseUnitY) {
    return MulDivRound(dlu, baseUnitY <= 0 ? 16 : baseUnitY, 8);
}

inline Metrics MakeMetrics(int baseUnitX, int baseUnitY) {
    Metrics m;
    m.outerMargin = DluToPxX(kOuterMarginDlu, baseUnitX);
    m.groupPadX = DluToPxX(kGroupPadXDlu, baseUnitX);
    m.groupPadTop = DluToPxY(kGroupPadTopDlu, baseUnitY);
    m.groupPadBottom = DluToPxY(kGroupPadBottomDlu, baseUnitY);
    m.rowGap = DluToPxY(kRowGapDlu, baseUnitY);
    m.colGap = DluToPxX(kColGapDlu, baseUnitX);
    m.groupGap = DluToPxY(kGroupGapDlu, baseUnitY);
    m.controlHeight = DluToPxY(kControlHeightDlu, baseUnitY);
    m.buttonHeight = DluToPxY(kButtonHeightDlu, baseUnitY);
    m.checkHeight = DluToPxY(kCheckHeightDlu, baseUnitY);
    m.textHeight = DluToPxY(kTextHeightDlu, baseUnitY);
    m.minFieldWidth = DluToPxX(kMinFieldWidthDlu, baseUnitX);
    return m;
}

// ============================================
// 分组框
// ============================================

struct GroupPlan {
    int frameLeft = 0;
    int frameRight = 0;
    int contentLeft = 0;      // 内容区左边缘
    int contentRight = 0;     // 内容区右边缘（控件右边缘上限）
    int labelColumnWidth = 0; // 标签列宽（labelsAbove 时为 0）
    bool labelsAbove = false; // 标签是否移到控件上方
    int controlLeft = 0;      // 有标签的行里控件的左边缘
};

// 按分组框边框与最宽标签决定标签列。widestLabel 为该组所有标签文字的最大宽度。
inline GroupPlan PlanGroup(int frameLeft, int frameRight, int widestLabel, const Metrics& m) {
    GroupPlan plan;
    plan.frameLeft = frameLeft;
    plan.frameRight = frameRight;
    plan.contentLeft = frameLeft + m.groupPadX;
    plan.contentRight = (std::max)(plan.contentLeft, frameRight - m.groupPadX);

    const int contentWidth = plan.contentRight - plan.contentLeft;
    if (widestLabel <= 0) {
        plan.labelsAbove = false;
        plan.labelColumnWidth = 0;
        plan.controlLeft = plan.contentLeft;
        return plan;
    }

    const int remaining = contentWidth - widestLabel - m.colGap;
    plan.labelsAbove = remaining < m.minFieldWidth;
    plan.labelColumnWidth = plan.labelsAbove ? 0 : widestLabel;
    plan.controlLeft = plan.labelsAbove ? plan.contentLeft : plan.contentLeft + widestLabel + m.colGap;
    return plan;
}

// ============================================
// 行
// ============================================

struct LabelControlRow {
    Rect label;
    Rect control;
    int bottom = 0;
};

// 标签 + 单个控件。控件占满标签列之后的全部宽度。
// 标签与控件并排时标签在控件高度内垂直居中；标签上移时控件紧跟标签下方。
inline LabelControlRow PlaceLabelControl(const GroupPlan& plan, int top, int labelWidth, int controlHeight,
                                         const Metrics& m) {
    LabelControlRow row;
    if (plan.labelsAbove) {
        row.label = {plan.contentLeft, top, plan.contentLeft + labelWidth, top + m.textHeight};
        const int controlTop = row.label.bottom + m.rowGap / 2;
        row.control = {plan.controlLeft, controlTop, plan.contentRight, controlTop + controlHeight};
    } else {
        const int labelTop = top + (std::max)(0, (controlHeight - m.textHeight) / 2);
        row.label = {plan.contentLeft, labelTop, plan.contentLeft + labelWidth, labelTop + m.textHeight};
        row.control = {plan.controlLeft, top, plan.contentRight, top + controlHeight};
    }
    row.bottom = (std::max)(row.label.bottom, row.control.bottom);
    return row;
}

struct FieldLine {
    Rect field;
    Rect button;
    bool wrapped = false; // 按钮是否折到字段下一行
    int bottom = 0;
};

// [字段][按钮]：放得下就同排、按钮靠右；放不下时字段占满整行，按钮左对齐折到下一行。
// buttonWidth <= 0 表示没有按钮。
inline FieldLine PlaceFieldLine(int left, int right, int top, int fieldHeight, int buttonWidth, int buttonHeight,
                                const Metrics& m) {
    FieldLine line;
    right = (std::max)(left, right);
    if (buttonWidth <= 0) {
        line.field = {left, top, right, top + fieldHeight};
        line.bottom = line.field.bottom;
        return line;
    }

    const int available = right - left;
    if (available >= m.minFieldWidth + m.colGap + buttonWidth) {
        line.field = {left, top, right - buttonWidth - m.colGap, top + fieldHeight};
        line.button = {right - buttonWidth, top, right, top + buttonHeight};
        line.bottom = (std::max)(line.field.bottom, line.button.bottom);
        return line;
    }

    line.wrapped = true;
    line.field = {left, top, right, top + fieldHeight};
    const int buttonTop = line.field.bottom + m.rowGap;
    const int clampedWidth = (std::min)(buttonWidth, available);
    line.button = {left, buttonTop, left + clampedWidth, buttonTop + buttonHeight};
    line.bottom = line.button.bottom;
    return line;
}

struct ButtonRow {
    std::vector<Rect> buttons;
    int bottom = 0;
};

// 一排按钮，左对齐；超出右边缘的按钮折到下一行。单个按钮宽于整行时被钳到行宽。
inline ButtonRow PlaceButtons(int left, int right, int top, const std::vector<int>& widths, int buttonHeight,
                              const Metrics& m) {
    ButtonRow row;
    right = (std::max)(left, right);
    int x = left;
    int y = top;
    bool anyOnLine = false;
    for (int width : widths) {
        const int clamped = (std::min)((std::max)(width, 0), right - left);
        if (anyOnLine && x + clamped > right) {
            x = left;
            y += buttonHeight + m.rowGap;
            anyOnLine = false;
        }
        row.buttons.push_back({x, y, x + clamped, y + buttonHeight});
        x += clamped + m.colGap;
        anyOnLine = true;
    }
    row.bottom = widths.empty() ? top : y + buttonHeight;
    return row;
}

// 占满内容宽度的单个控件（复选框、说明文字）。
inline Rect PlaceFullWidth(const GroupPlan& plan, int top, int height) {
    return {plan.contentLeft, top, plan.contentRight, top + height};
}

// 分组框边框：给定内容底部得到边框矩形。
inline Rect FinishGroup(const GroupPlan& plan, int frameTop, int contentBottom, const Metrics& m) {
    return {plan.frameLeft, frameTop, plan.frameRight, contentBottom + m.groupPadBottom};
}

// 内容区第一行的 top：分组框顶边之下留出标题高度。
inline int GroupContentTop(int frameTop, const Metrics& m) {
    return frameTop + m.groupPadTop;
}

// ============================================
// 校验
// ============================================

// inner 是否完全落在 outer 之内（边界相等视为落在内部）。
inline bool RectWithin(const Rect& inner, const Rect& outer) {
    return inner.left >= outer.left && inner.top >= outer.top && inner.right <= outer.right &&
           inner.bottom <= outer.bottom;
}

}  // namespace prefs_layout
