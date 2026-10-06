#pragma once

// ============================================
// PreferencesPageBase.h - 偏好设置页实例的公共骨架
//
// 每个页面（总览页与各子页）都是一个原生子对话框：字体与基准单位取自宿主，控件按
// PreferencesLayoutBuilder 的“分组 → 行”描述铺排，高度不足时纵向滚动，颜色交给
// CCoreDarkModeHooks；设置走草稿模型，Apply 才写配置。本类持有这些与具体设置无关的部分，
// 派生页只声明字段表、创建控件、描述布局、读写配置和 Apply 之后的运行时刷新。
//
// 生命周期：最派生类的构造函数末尾调用 CreatePageWindow(parent)。创建窗口时同步收到
// WM_INITDIALOG，里面的 CreateControls / LayoutContent 是虚调用，只有从最派生类的构造函数体
// 里发起（此时虚表已指向最派生类）才会落到派生实现；在基类构造函数里调用会落到纯虚函数。
// 宿主先销毁窗口再释放实例（WM_NCDESTROY 断开关联），析构只处理宿主没销毁窗口的异常路径。
// ============================================

#include <string>
#include <vector>
#include <foobar2000/SDK/coreDarkMode.h>

#include "core/PreferencesDraft.h"
#include "core/PreferencesLayoutBuilder.h"

class preferences_page_callback;

namespace webview_prefs {

class PreferencesPageBase : public preferences_page_instance {
public:
    // preferences_page_instance
    t_uint32 get_state() override;
    fb2k::hwnd_t get_wnd() override;
    void apply() override;
    void reset() override;

protected:
    PreferencesPageBase(preferences_page_callback::ptr callback, prefs_draft::FieldTable fields);
    // service_base 的析构不是虚函数（不能写 override）。实例由 service_impl_t<派生类>::service_release
    // 以派生类型 delete，析构链本身完整；声明为虚只是让任何经基类指针的 delete 不成为未定义行为。
    virtual ~PreferencesPageBase();

    // 派生类构造函数末尾调用：读初始快照、创建子对话框（触发 CreateControls 与首次布局）。
    void CreatePageWindow(HWND parent);

    // ---- 派生类必须实现 ----
    // 从配置读一份快照；未知存储值按默认值清洗。
    virtual prefs_draft::Snapshot ReadSnapshotFromConfig() const = 0;
    // 本进程启动时影响重启的字段的取值；与草稿不同即报告 needs_restart。
    virtual prefs_draft::Snapshot StartupSnapshot() const = 0;
    // 写一个字段并读回比对；返回 false 表示没写成。
    virtual bool WriteField(size_t field, const prefs_draft::Snapshot& value) = 0;
    // 在 WM_INITDIALOG 里创建全部控件（文案按运行期语言求值一次）。
    virtual void CreateControls(HWND hwnd) = 0;
    // 用 builder 描述各分组与行；基类负责铺排、滚动与重绘。
    virtual void LayoutContent(prefs_layout::PageLayoutBuilder& builder) = 0;
    // 把草稿写回控件（Reset / 冲突 / 写失败后都会调用）。
    virtual void SyncControlsFromDraft() = 0;
    // 字段在错误框里的名字。
    virtual std::wstring FieldDisplayName(size_t field) const = 0;

    // ---- 派生类可覆写 ----
    // 校验失败的提示；默认按错误类型给通用文案。
    virtual std::wstring ValidationMessage(const prefs_draft::ValidationResult& result) const;
    // Apply 写成之后的运行时刷新；changed 为已写字段的下标。
    virtual void AfterApply(const std::vector<size_t>& changed) { (void)changed; }
    // 基类没处理的消息（WM_COMMAND 等）；返回 TRUE 表示已处理。
    virtual INT_PTR OnMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // ---- 派生类可用 ----
    HWND hwnd() const { return hwnd_; }
    HFONT font() const { return font_; }
    int baseUnitX() const { return baseUnitX_; }
    int baseUnitY() const { return baseUnitY_; }
    const prefs_draft::FieldTable& fields() const { return fields_; }
    prefs_draft::Snapshot& draft() { return draft_; }
    const prefs_draft::Snapshot& draft() const { return draft_; }
    const prefs_draft::Snapshot& initial() const { return initial_; }
    // 把基线换成 snapshot，草稿保留；冲突与写失败路径用。
    void Rebase(const prefs_draft::Snapshot& snapshot);
    // 通知宿主重新查询 get_state()。
    void UpdateState();
    // 焦点移到屏外控件时把它滚入可见区。
    void EnsureChildVisible(HWND child);
    // 模态错误框；期间持有自引用，防止宿主释放实例。
    void ShowApplyError(const wchar_t* message);
    // 用本页字体量文字。
    int MeasureText(const std::wstring& text) const;
    int MeasureWrappedText(const std::wstring& text, int width) const;
    // 控件文案按本页字体的宽度 / 折行高度。
    int ControlTextWidth(int id) const;
    int ControlWrappedHeight(int id, int width) const;

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    INT_PTR HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void RefreshFontAndMetrics(HWND parent);
    void LayoutControls();
    void UpdateScrollBar();
    void ScrollTo(int y);
    void LogLayoutMetrics(int clientWidth, int clientHeight, bool placed);

    // 一次布局的关键读数（像素）。开发者工具开启时变化即写控制台，供与外部量出的控件矩形对照
    struct LayoutLog {
        int dpi = 0;
        int baseUnitX = 0;
        int baseUnitY = 0;
        int clientWidth = 0;
        int clientHeight = 0;
        int contentHeight = 0;
        int frameLeft = 0;      // 第一个分组框的左边缘
        int frameTop = 0;       // 第一个分组框的上边缘（未滚动坐标）
        int frameRight = 0;
        bool placed = false;    // 本轮 DeferWindowPos 批次是否成功提交
        bool operator==(const LayoutLog&) const = default;
    };

    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;       // 当前使用的字体；借自宿主时不归本实例释放
    HFONT ownedFont_ = nullptr;  // 宿主字体不可得时自建的回退字体
    int baseUnitX_ = 8;          // 对话框基准单位（平均字符宽，像素）
    int baseUnitY_ = 16;         // 对话框基准单位（字符高，像素）
    int scrollY_ = 0;            // 当前纵向滚动偏移（像素）
    int contentHeight_ = 0;      // 最近一次布局得到的内容总高
    int firstGroupId_ = 0;       // 布局读数里记的分组框
    LayoutLog lastLayoutLog_;    // 上一次写进控制台的布局读数
    preferences_page_callback::ptr callback_;
    fb2k::CCoreDarkModeHooks darkMode_;

    prefs_draft::FieldTable fields_;
    // 初始快照与编辑副本：控件只改 draft_，Apply 才写配置；dirty = initial_ != draft_
    prefs_draft::Snapshot initial_;
    prefs_draft::Snapshot draft_;
};

} // namespace webview_prefs
