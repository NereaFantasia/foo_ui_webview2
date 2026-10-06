#pragma once

// ============================================
// PreferencesLayoutBuilder.h - 按“分组 → 行”描述铺排一页控件
//
// 页面类只说“这一组有这些行”，本类用 PreferencesLayout.h 的纯函数算出每个控件的矩形。
// 与 HWND 无关：文字宽度与折行高度由调用方注入，输出是 (控件 ID, 矩形) 列表和内容底边，
// 让 GoogleTest 能拿同一份度量对拍手写布局。坐标为页面客户区像素、已减去滚动偏移。
//
// 行与行之间加一个 rowGap，分组框之间加一个 groupGap；每组第一行、第一个分组框前不加。
// ============================================

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <vector>

#include "core/PreferencesLayout.h"

namespace prefs_layout {

struct Placement {
    int id = 0;
    Rect rect;
};

struct TextSource {
    std::function<int(int id)> textWidth;                  // 控件当前文字的宽度（像素）
    std::function<int(int id, int width)> wrappedHeight;   // 控件文字按给定宽度折行后的高度
};

class PageLayoutBuilder {
public:
    PageLayoutBuilder(const Metrics& m, int baseUnitX, int clientWidth, int scrollY, TextSource text)
        : m_(m), text_(std::move(text)) {
        frameLeft_ = m_.outerMargin;
        frameRight_ = (std::max)(frameLeft_, clientWidth - m_.outerMargin);
        y_ = m_.outerMargin - scrollY;
        // 下拉框的窗口高度包含展开列表；关闭态高度由字体决定。
        comboWindowHeight_ = m_.controlHeight * 8;
        buttonMinWidth_ = DluToPxX(50, baseUnitX);
        buttonTextPad_ = DluToPxX(12, baseUnitX);
        checkGlyphWidth_ = DluToPxX(12, baseUnitX);
    }

    // labelIds 为该组全部标签，最宽者决定标签列；为空则没有标签列。
    void BeginGroup(int groupId, std::initializer_list<int> labelIds) {
        if (anyGroup_) y_ += m_.groupGap;
        anyGroup_ = true;
        groupId_ = groupId;
        frameTop_ = y_;
        int widest = 0;
        for (int id : labelIds) widest = (std::max)(widest, TextWidth(id));
        plan_ = PlanGroup(frameLeft_, frameRight_, widest, m_);
        cy_ = GroupContentTop(frameTop_, m_);
        firstRow_ = true;
    }

    void EndGroup() {
        const Rect frame = FinishGroup(plan_, frameTop_, cy_, m_);
        Place(groupId_, frame);
        if (!hasFirstFrame_) {
            hasFirstFrame_ = true;
            firstFrame_ = frame;
            firstGroupId_ = groupId_;
        }
        y_ = frame.bottom;
    }

    // 标签 + 一个控件；controlWindowHeight 给下拉框用（窗口高含展开列表），<= 0 时取控件高。
    void LabelControl(int labelId, int controlId, int controlWindowHeight = 0) {
        NextRow();
        const LabelControlRow row = PlaceLabelControl(plan_, cy_, TextWidth(labelId), m_.controlHeight, m_);
        Place(labelId, row.label);
        Place(controlId, WithHeight(row.control, controlWindowHeight));
        cy_ = row.bottom;
    }

    // 标签 + [字段][按钮]；放不下时按钮折到字段下一行（PlaceFieldLine）。
    void LabelFieldButton(int labelId, int fieldId, int buttonId, int fieldWindowHeight = 0) {
        NextRow();
        const LabelControlRow row = PlaceLabelControl(plan_, cy_, TextWidth(labelId), m_.controlHeight, m_);
        const FieldLine line = PlaceFieldLine(row.control.left, row.control.right, row.control.top,
            m_.controlHeight, ButtonWidth(buttonId), m_.buttonHeight, m_);
        Place(labelId, row.label);
        Place(fieldId, WithHeight(line.field, fieldWindowHeight));
        Place(buttonId, line.button);
        cy_ = (std::max)(row.label.bottom, line.bottom);
    }

    // 标签 + 一行只读文字：文字与标签基线对齐，高度按一行文字。
    void LabelInlineText(int labelId, int textId) {
        NextRow();
        const LabelControlRow row = PlaceLabelControl(plan_, cy_, TextWidth(labelId), m_.controlHeight, m_);
        Place(labelId, row.label);
        const int textTop = row.control.top + (std::max)(0, (m_.controlHeight - m_.textHeight) / 2);
        Place(textId, {row.control.left, textTop, row.control.right, textTop + m_.textHeight});
        cy_ = row.bottom;
    }

    // 复选框独占内容宽度；文字按去掉勾选框后的宽度折行。
    void CheckBox(int id) {
        NextRow();
        const int textHeight = WrappedHeight(id, ContentWidth() - checkGlyphWidth_);
        const Rect r = PlaceFullWidth(plan_, cy_, (std::max)(m_.checkHeight, textHeight));
        Place(id, r);
        cy_ = r.bottom;
    }

    // 说明文字独占内容宽度，按宽度折行。
    void Note(int id) {
        NextRow();
        const Rect r = PlaceFullWidth(plan_, cy_, WrappedHeight(id, ContentWidth()));
        Place(id, r);
        cy_ = r.bottom;
    }

    // 一排按钮，左对齐，放不下折行。
    void Buttons(std::initializer_list<int> ids) {
        NextRow();
        std::vector<int> widths;
        for (int id : ids) widths.push_back(ButtonWidth(id));
        const ButtonRow row = PlaceButtons(plan_.contentLeft, plan_.contentRight, cy_, widths, m_.buttonHeight, m_);
        size_t index = 0;
        for (int id : ids) {
            if (index < row.buttons.size()) Place(id, row.buttons[index]);
            ++index;
        }
        cy_ = row.bottom;
    }

    const std::vector<Placement>& placements() const { return placements_; }
    // 最后一个分组框的底边（滚动坐标）；调用方加回滚动偏移得到内容总高。
    int contentBottom() const { return y_; }
    // 第一个分组框的边框（滚动坐标）与控件 ID，供布局读数；没有分组框时为全零矩形与 0。
    const Rect& firstFrame() const { return firstFrame_; }
    int firstGroupId() const { return firstGroupId_; }
    int comboWindowHeight() const { return comboWindowHeight_; }
    int ButtonWidth(int id) const { return (std::max)(buttonMinWidth_, TextWidth(id) + buttonTextPad_); }

private:
    void NextRow() {
        if (!firstRow_) cy_ += m_.rowGap;
        firstRow_ = false;
    }
    int ContentWidth() const { return plan_.contentRight - plan_.contentLeft; }
    int TextWidth(int id) const { return text_.textWidth ? text_.textWidth(id) : 0; }
    int WrappedHeight(int id, int width) const {
        return text_.wrappedHeight ? text_.wrappedHeight(id, width) : m_.textHeight;
    }
    static Rect WithHeight(const Rect& r, int height) {
        return height > 0 ? Rect{r.left, r.top, r.right, r.top + height} : r;
    }
    void Place(int id, const Rect& r) { placements_.push_back({id, r}); }

    Metrics m_;
    TextSource text_;
    int frameLeft_ = 0;
    int frameRight_ = 0;
    int y_ = 0;
    int comboWindowHeight_ = 0;
    int buttonMinWidth_ = 0;
    int buttonTextPad_ = 0;
    int checkGlyphWidth_ = 0;
    bool anyGroup_ = false;
    int groupId_ = 0;
    int frameTop_ = 0;
    int cy_ = 0;
    bool firstRow_ = true;
    GroupPlan plan_;
    bool hasFirstFrame_ = false;
    Rect firstFrame_;
    int firstGroupId_ = 0;
    std::vector<Placement> placements_;
};

}  // namespace prefs_layout
