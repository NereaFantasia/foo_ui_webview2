// ============================================
// PreferencesPageBase.cpp - 偏好设置页实例的公共骨架
// ============================================

#include "pch.h"
#include "core/PreferencesPageBase.h"
#include "core/PreferencesLayout.h"
#include "core/SecurityConfig.h"
#include "utils/I18n.h"
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

// 父窗口 DPI 变化后发给子窗口的通知；旧 SDK 头没有这个定义。
#ifndef WM_DPICHANGED_AFTERPARENT
#define WM_DPICHANGED_AFTERPARENT 0x02E3
#endif

namespace webview_prefs {

namespace {

// ============================================
// DPI
// ============================================

int GetDpiForWindowSafe(HWND hwnd) {
    // Windows 10 1607+ 才有 GetDpiForWindow；更早的系统回退到设备上下文的读数。
    using GetDpiForWindowFunc = UINT (WINAPI *)(HWND);
    static GetDpiForWindowFunc pGetDpiForWindow = nullptr;
    static bool tried = false;

    if (!tried) {
        tried = true;
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            pGetDpiForWindow = reinterpret_cast<GetDpiForWindowFunc>(GetProcAddress(hUser32, "GetDpiForWindow"));
        }
    }

    if (pGetDpiForWindow && hwnd) {
        const UINT dpi = pGetDpiForWindow(hwnd);
        if (dpi > 0) return static_cast<int>(dpi);
    }

    HDC hdc = GetDC(hwnd);
    const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(hwnd, hdc);
    return dpi > 0 ? dpi : 96;
}

// ============================================
// 字体、基准单位与文字测量
// ============================================

// 对话框基准单位：与对话框管理器同一算法——X 取 52 个大小写字母的平均宽度，
// Y 取字体高度。MapDialogRect 绑定对话框创建时的字体，而本页借用宿主字体，
// 所以自行测量。
void MeasureDialogBaseUnits(HWND hwnd, HFONT font, int& baseUnitX, int& baseUnitY) {
    baseUnitX = 8;
    baseUnitY = 16;
    HDC hdc = GetDC(hwnd);
    if (!hdc) return;
    HGDIOBJ old = font ? SelectObject(hdc, font) : nullptr;
    TEXTMETRICW tm{};
    if (GetTextMetricsW(hdc, &tm) && tm.tmHeight > 0) {
        baseUnitY = static_cast<int>(tm.tmHeight);
    }
    static const wchar_t kAlphabet[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    SIZE size{};
    if (GetTextExtentPoint32W(hdc, kAlphabet, 52, &size) && size.cx > 0) {
        baseUnitX = static_cast<int>((std::max)(1L, (size.cx / 26 + 1) / 2));
    }
    if (old) SelectObject(hdc, old);
    ReleaseDC(hwnd, hdc);
}

int MeasureTextWidth(HWND hwnd, HFONT font, const std::wstring& text) {
    if (text.empty()) return 0;
    HDC hdc = GetDC(hwnd);
    if (!hdc) return 0;
    HGDIOBJ old = font ? SelectObject(hdc, font) : nullptr;
    SIZE size{};
    GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.size()), &size);
    if (old) SelectObject(hdc, old);
    ReleaseDC(hwnd, hdc);
    return static_cast<int>(size.cx);
}

// 按给定宽度折行后的文字高度；至少一行。
int MeasureWrappedTextHeight(HWND hwnd, HFONT font, const std::wstring& text, int width, int lineHeight) {
    if (text.empty() || width <= 0) return lineHeight;
    HDC hdc = GetDC(hwnd);
    if (!hdc) return lineHeight;
    HGDIOBJ old = font ? SelectObject(hdc, font) : nullptr;
    RECT rc{0, 0, width, 0};
    DrawTextW(hdc, text.c_str(), static_cast<int>(text.size()), &rc,
        DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
    if (old) SelectObject(hdc, old);
    ReleaseDC(hwnd, hdc);
    return (std::max)(lineHeight, static_cast<int>(rc.bottom - rc.top));
}

std::wstring GetControlText(HWND control) {
    if (!control) return {};
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) return {};
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<size_t>(length));
    return text;
}

// 控件获得焦点的通知码因窗口类而异，且数值互相重叠（BN_SETFOCUS 与 CBN_EDITUPDATE 都是 6），
// 所以按控件的窗口类名判定。
bool IsSetFocusNotification(HWND control, WORD code) {
    wchar_t className[32]{};
    if (!GetClassNameW(control, className, static_cast<int>(std::size(className)))) return false;
    if (_wcsicmp(className, L"Button") == 0) return code == BN_SETFOCUS;
    if (_wcsicmp(className, L"ComboBox") == 0) return code == CBN_SETFOCUS;
    if (_wcsicmp(className, L"Edit") == 0) return code == EN_SETFOCUS;
    return false;
}

}  // namespace

// ============================================
// 构造、窗口创建与销毁
// ============================================

PreferencesPageBase::PreferencesPageBase(preferences_page_callback::ptr callback, prefs_draft::FieldTable fields)
    : callback_(std::move(callback)), fields_(std::move(fields)) {
}

PreferencesPageBase::~PreferencesPageBase() {
    // SDK 约定宿主先销毁页面窗口再释放实例；WM_NCDESTROY 已把 hwnd_ 清空。
    // 这里只处理宿主没有销毁窗口的异常路径：派生部分此时已析构，先断开 DWLP_USER，
    // 销毁期间的消息不再进入本对象，也就不会再发生虚调用。
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, DWLP_USER, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (ownedFont_) {
        DeleteObject(ownedFont_);
        ownedFont_ = nullptr;
    }
}

void PreferencesPageBase::CreatePageWindow(HWND parent) {
    // 初始快照与草稿同源。
    initial_ = ReadSnapshotFromConfig();
    draft_ = initial_;

    // 页面本身是一个无控件的子对话框：DS_CONTROL 让宿主的对话框消息循环把
    // Tab / 助记键导航穿透到本页；控件在 WM_INITDIALOG 里按运行期文案创建，
    // 尺寸完全由宿主给的父窗口决定，不再自设下限。
    RECT rcParent{};
    GetClientRect(parent, &rcParent);

    struct alignas(DWORD) {
        DLGTEMPLATE tmpl;
        WORD menuArray[1];
        WORD classArray[1];
        WORD titleArray[1];
    } dlg = {};
    dlg.tmpl.style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL | DS_CONTROL;
    dlg.tmpl.dwExtendedStyle = WS_EX_CONTROLPARENT;
    dlg.tmpl.cdit = 0;
    dlg.tmpl.x = 0;
    dlg.tmpl.y = 0;
    dlg.tmpl.cx = 0;
    dlg.tmpl.cy = 0;

    hwnd_ = CreateDialogIndirectParamW(
        core_api::get_my_instance(),
        &dlg.tmpl,
        parent,
        DialogProc,
        reinterpret_cast<LPARAM>(this));

    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, 0, 0,
            rcParent.right - rcParent.left, rcParent.bottom - rcParent.top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

// ============================================
// preferences_page_instance
// ============================================

t_uint32 PreferencesPageBase::get_state() {
    t_uint32 state = preferences_state::dark_mode_supported | preferences_state::resettable;
    if (prefs_draft::IsDirty(initial_, draft_)) {
        state |= preferences_state::changed;
    }
    // 影响重启的字段与本进程启动值不同就要求重启。Applied 之后 draft_ == initial_，
    // 只要仍与启动值不同，标志继续保留。
    if (prefs_draft::NeedsRestart(fields_, StartupSnapshot(), draft_)) {
        state |= preferences_state::needs_restart;
    }
    return state;
}

fb2k::hwnd_t PreferencesPageBase::get_wnd() {
    return hwnd_;
}

void PreferencesPageBase::reset() {
    // 只把草稿设为默认值并刷新控件：不写配置、不建目录、不导航；已全部为默认值时保持 clean。
    draft_ = prefs_draft::Defaults(fields_);
    SyncControlsFromDraft();
    UpdateState();
}

void PreferencesPageBase::apply() {
    service_ptr_t<PreferencesPageBase> keepAlive(this);

    // 1. 读回当前配置，再做全量校验：任一失败就在第一个 setter 之前停下，配置不变。
    const prefs_draft::Snapshot current = ReadSnapshotFromConfig();
    const prefs_draft::ValidationResult validation = prefs_draft::Validate(fields_, draft_);
    if (!validation.ok()) {
        const std::wstring message = ValidationMessage(validation);
        ShowApplyError(message.c_str());
        return;
    }

    // 2. 以当前配置为准判定冲突：只看已编辑字段，外部写成别的值就拒绝，不用旧快照覆盖别人。
    const std::vector<size_t> edited = prefs_draft::Diff(initial_, draft_);
    const std::vector<size_t> conflicts = prefs_draft::DetectConflicts(initial_, draft_, current);
    if (!conflicts.empty()) {
        std::wstring message = TR(
            "These settings were changed elsewhere while this page was open. Nothing was saved.\n"
            "Review the new values, then change and apply again:\n",
            "以下设置在本页打开期间被其他地方修改，本次没有保存。\n"
            "请查看新值后再修改并应用：\n");
        for (size_t field : conflicts) {
            message += L"\n- ";
            message += FieldDisplayName(field);
        }
        // 基线换成当前值：草稿保留，dirty 相对新基线重新计算，用户看得到自己还要不要改。
        Rebase(current);
        SyncControlsFromDraft();
        UpdateState();
        ShowApplyError(message.c_str());
        return;
    }

    // 3. 只写实际改动的字段；哪个 setter 没写成就停下、读回真实配置并报告，不宣称原子性。
    for (size_t field : edited) {
        if (!WriteField(field, draft_)) {
            Rebase(ReadSnapshotFromConfig());
            SyncControlsFromDraft();
            UpdateState();
            std::wstring message = TR("Failed to save: ", "保存失败：");
            message += FieldDisplayName(field);
            message += TR("\nSettings written before this one are kept; the rest were not saved.",
                          "\n此前已写入的设置保留，其余未保存。");
            ShowApplyError(message.c_str());
            return;
        }
    }

    // 4. 刷新快照与 dirty，再通知宿主。
    initial_ = ReadSnapshotFromConfig();
    draft_ = initial_;
    UpdateState();
    console::printf("[WebView2 UI] Preferences applied (%u field(s))", static_cast<unsigned>(edited.size()));

    // 5. 运行时刷新交给派生页；导航是异步的，配置已保存与导航完成是两回事。
    AfterApply(edited);
}

std::wstring PreferencesPageBase::ValidationMessage(const prefs_draft::ValidationResult& result) const {
    switch (result.error) {
    case prefs_draft::ValidationError::OutOfRange:
        return TR("A setting has a value outside the allowed range. Nothing was saved.",
                  "有设置的取值超出允许范围，本次没有保存。");
    case prefs_draft::ValidationError::Invalid:
    case prefs_draft::ValidationError::Missing: {
        std::wstring message = TR("The value of \"", "“");
        message += FieldDisplayName(result.field);
        message += TR("\" is not valid. Nothing was saved.", "”的取值无效，本次没有保存。");
        return message;
    }
    case prefs_draft::ValidationError::None:
        break;
    }
    return {};
}

INT_PTR PreferencesPageBase::OnMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)hwnd;
    (void)msg;
    (void)wParam;
    (void)lParam;
    return FALSE;
}

void PreferencesPageBase::Rebase(const prefs_draft::Snapshot& snapshot) {
    initial_ = snapshot;
}

void PreferencesPageBase::UpdateState() {
    if (callback_.is_valid()) {
        callback_->on_state_changed();
    }
}

void PreferencesPageBase::ShowApplyError(const wchar_t* message) {
    HWND owner = hwnd_ ? GetAncestor(hwnd_, GA_ROOT) : nullptr;
    // 模态期间宿主可能销毁本页并释放实例，先持有自引用。
    service_ptr_t<PreferencesPageBase> keepAlive(this);
    MessageBoxW(owner, message, L"WebView2 UI", MB_OK | MB_ICONERROR);
}

// ============================================
// 消息分发
// ============================================

INT_PTR CALLBACK PreferencesPageBase::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PreferencesPageBase* self = nullptr;
    if (msg == WM_INITDIALOG) {
        self = reinterpret_cast<PreferencesPageBase*>(lParam);
        SetWindowLongPtrW(hwnd, DWLP_USER, static_cast<LONG_PTR>(lParam));
    } else {
        self = reinterpret_cast<PreferencesPageBase*>(GetWindowLongPtrW(hwnd, DWLP_USER));
    }

    if (!self) return FALSE;
    return self->HandleMessage(hwnd, msg, wParam, lParam);
}

INT_PTR PreferencesPageBase::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        // CreateDialogIndirectParamW 返回之前就会收到本消息，此时 hwnd_ 尚未赋值。
        hwnd_ = hwnd;
        RefreshFontAndMetrics(GetParent(hwnd));
        CreateControls(hwnd);
        darkMode_.AddDialogWithControls(hwnd);
        LayoutControls();
        // 返回 FALSE：不让对话框管理器抢焦点，宿主决定初始焦点落点。
        return FALSE;

    case WM_NCDESTROY:
        // 宿主销毁窗口后实例可能还活一小段时间；先断开关联，析构不再碰句柄。
        SetWindowLongPtrW(hwnd, DWLP_USER, 0);
        hwnd_ = nullptr;
        return FALSE;

    case WM_SIZE:
        LayoutControls();
        return TRUE;

    case WM_DPICHANGED_AFTERPARENT:
        // 父窗口换了 DPI：重取宿主字体与基准单位，坐标从逻辑布局重新生成。
        RefreshFontAndMetrics(GetParent(hwnd));
        LayoutControls();
        return TRUE;

    case WM_VSCROLL: {
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        int target = si.nPos;
        const int line = (std::max)(1, baseUnitY_);
        switch (LOWORD(wParam)) {
        case SB_LINEUP: target -= line; break;
        case SB_LINEDOWN: target += line; break;
        case SB_PAGEUP: target -= static_cast<int>(si.nPage); break;
        case SB_PAGEDOWN: target += static_cast<int>(si.nPage); break;
        case SB_TOP: target = si.nMin; break;
        case SB_BOTTOM: target = si.nMax; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: target = si.nTrackPos; break;
        default: break;
        }
        ScrollTo(target);
        return TRUE;
    }

    case WM_MOUSEWHEEL: {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        const int lines = (std::max)(1, baseUnitY_) * 3;
        ScrollTo(scrollY_ - MulDiv(delta, lines, WHEEL_DELTA));
        return TRUE;
    }

    case WM_COMMAND: {
        // 任何控件拿到焦点都把它滚入可见区；命令本身交给派生页。
        HWND control = reinterpret_cast<HWND>(lParam);
        if (control && IsSetFocusNotification(control, HIWORD(wParam))) {
            EnsureChildVisible(control);
        }
        return OnMessage(hwnd, msg, wParam, lParam);
    }

    default:
        break;
    }

    // 未处理的消息交给对话框管理器；颜色与背景由 CCoreDarkModeHooks 接管。
    return OnMessage(hwnd, msg, wParam, lParam);
}

// ============================================
// 字体与度量
// ============================================

void PreferencesPageBase::RefreshFontAndMetrics(HWND parent) {
    // 宿主的偏好设置对话框把字体交给页面容器；逐级向上找到第一个有字体的祖先。
    HFONT hostFont = nullptr;
    for (HWND probe = parent; probe && !hostFont; probe = GetParent(probe)) {
        hostFont = reinterpret_cast<HFONT>(SendMessageW(probe, WM_GETFONT, 0, 0));
    }

    if (hostFont) {
        font_ = hostFont;
    } else {
        // 宿主没有给字体时按当前 DPI 建一份 9pt 消息字体作回退，由本实例释放。
        if (ownedFont_) {
            DeleteObject(ownedFont_);
            ownedFont_ = nullptr;
        }
        NONCLIENTMETRICSW ncm{};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        const int dpi = GetDpiForWindowSafe(hwnd_ ? hwnd_ : parent);
        ncm.lfMessageFont.lfHeight = -MulDiv(9, dpi, 72);
        ownedFont_ = CreateFontIndirectW(&ncm.lfMessageFont);
        font_ = ownedFont_;
    }

    HWND measureWnd = hwnd_ ? hwnd_ : parent;
    MeasureDialogBaseUnits(measureWnd, font_, baseUnitX_, baseUnitY_);

    // 控件已存在（DPI 变化路径）时同步字体，布局随后由调用方重算。
    if (hwnd_) {
        for (HWND child = GetWindow(hwnd_, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        }
    }
}

int PreferencesPageBase::MeasureText(const std::wstring& text) const {
    return MeasureTextWidth(hwnd_, font_, text);
}

int PreferencesPageBase::MeasureWrappedText(const std::wstring& text, int width) const {
    const int lineHeight = prefs_layout::DluToPxY(prefs_layout::kTextHeightDlu, baseUnitY_);
    return MeasureWrappedTextHeight(hwnd_, font_, text, width, lineHeight);
}

int PreferencesPageBase::ControlTextWidth(int id) const {
    return MeasureText(GetControlText(GetDlgItem(hwnd_, id)));
}

int PreferencesPageBase::ControlWrappedHeight(int id, int width) const {
    return MeasureWrappedText(GetControlText(GetDlgItem(hwnd_, id)), width);
}

// ============================================
// 布局与滚动
// ============================================

void PreferencesPageBase::LayoutControls() {
    if (!hwnd_) return;
    using namespace prefs_layout;

    RECT rcClient{};
    GetClientRect(hwnd_, &rcClient);
    const int clientWidth = rcClient.right - rcClient.left;
    const int clientHeight = rcClient.bottom - rcClient.top;

    const Metrics m = MakeMetrics(baseUnitX_, baseUnitY_);
    TextSource text;
    text.textWidth = [this](int id) { return ControlTextWidth(id); };
    text.wrappedHeight = [this](int id, int width) { return ControlWrappedHeight(id, width); };
    PageLayoutBuilder builder(m, baseUnitX_, clientWidth, scrollY_, std::move(text));
    LayoutContent(builder);

    // SWP_NOCOPYBITS：分组框是 WS_EX_TRANSPARENT 的空心窗口，搬动时复制旧像素得到的是错的内容，
    // 统一让所有控件在随后的整页重绘里重画。
    const std::vector<Placement>& placements = builder.placements();
    HDWP hdwp = BeginDeferWindowPos(static_cast<int>(placements.size()));
    for (const Placement& placement : placements) {
        HWND h = GetDlgItem(hwnd_, placement.id);
        if (!h || !hdwp) continue;
        const Rect& r = placement.rect;
        hdwp = DeferWindowPos(hdwp, h, nullptr, r.left, r.top, (std::max)(0, r.Width()), (std::max)(0, r.Height()),
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
    }
    const bool placed = hdwp && EndDeferWindowPos(hdwp);
    firstGroupId_ = builder.firstGroupId();

    // 内容总高按未滚动坐标记录；滚动范围随之更新，滚动条出现或消失会再触发一次 WM_SIZE。
    // 末尾的外边距只在需要滚动时计入：最后一个分组框放得下就不为这段留白出滚动条（英文说明折行后
    // 内容曾只比客户区高 2 px，而滚动条一出又压窄客户区）；放不下时把它算进滚动范围，滚到底仍留边距。
    const int contentBottom = builder.contentBottom() + scrollY_;
    contentHeight_ = contentBottom <= clientHeight ? contentBottom : contentBottom + m.outerMargin;
    UpdateScrollBar();

    // 页面变高到不需要滚动时把偏移收回，避免留下一段空白。
    const int maxScroll = (std::max)(0, contentHeight_ - clientHeight);
    if (scrollY_ > maxScroll) {
        scrollY_ = maxScroll;
        LayoutControls();
        return;
    }
    // 页面带 WS_CLIPCHILDREN，只失效父窗口时透明的分组框不会跟着重画，滚动后框线就留在旧位置或被
    // 背景盖掉；RDW_ALLCHILDREN 让背景与全部控件按 Z 序重画一遍。
    RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    LogLayoutMetrics(clientWidth, clientHeight, placed);
}

// 开发者工具开启时把一次布局的关键读数写进控制台，供与外部量出的控件矩形对照；
// 读数没变（例如只是滚动）就不重复写。
void PreferencesPageBase::LogLayoutMetrics(int clientWidth, int clientHeight, bool placed) {
    if (!hwnd_ || !security_config::IsDevToolsEnabled()) return;
    RECT frame{};
    if (HWND group = firstGroupId_ ? GetDlgItem(hwnd_, firstGroupId_) : nullptr) {
        GetWindowRect(group, &frame);
        MapWindowPoints(nullptr, hwnd_, reinterpret_cast<POINT*>(&frame), 2);
    }
    const LayoutLog current{GetDpiForWindowSafe(hwnd_), baseUnitX_, baseUnitY_, clientWidth, clientHeight,
                            contentHeight_, frame.left, frame.top + scrollY_, frame.right, placed};
    if (current == lastLayoutLog_) return;
    lastLayoutLog_ = current;
    console::printf("[WebView2 UI] Preferences layout: dpi=%d base=%dx%d client=%dx%d content=%d "
                    "firstFrame=%d,%d-%d placed=%d",
        current.dpi, current.baseUnitX, current.baseUnitY, current.clientWidth, current.clientHeight,
        current.contentHeight, current.frameLeft, current.frameTop, current.frameRight, current.placed ? 1 : 0);
}

void PreferencesPageBase::UpdateScrollBar() {
    if (!hwnd_) return;
    RECT rcClient{};
    GetClientRect(hwnd_, &rcClient);
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = (std::max)(0, contentHeight_ - 1);
    si.nPage = static_cast<UINT>((std::max)(0L, rcClient.bottom - rcClient.top));
    si.nPos = scrollY_;
    SetScrollInfo(hwnd_, SB_VERT, &si, TRUE);
}

void PreferencesPageBase::ScrollTo(int y) {
    if (!hwnd_) return;
    RECT rcClient{};
    GetClientRect(hwnd_, &rcClient);
    const int pageHeight = static_cast<int>(rcClient.bottom - rcClient.top);
    const int maxScroll = (std::max)(0, contentHeight_ - pageHeight);
    const int clamped = (std::min)((std::max)(0, y), maxScroll);
    if (clamped == scrollY_) return;
    scrollY_ = clamped;
    LayoutControls();
}

void PreferencesPageBase::EnsureChildVisible(HWND child) {
    if (!hwnd_ || !child) return;
    RECT rcChild{};
    GetWindowRect(child, &rcChild);
    MapWindowPoints(nullptr, hwnd_, reinterpret_cast<POINT*>(&rcChild), 2);
    RECT rcClient{};
    GetClientRect(hwnd_, &rcClient);
    const int margin = (std::max)(1, baseUnitY_ / 2);
    if (rcChild.top < rcClient.top) {
        ScrollTo(scrollY_ + (rcChild.top - rcClient.top) - margin);
    } else if (rcChild.bottom > rcClient.bottom) {
        ScrollTo(scrollY_ + (rcChild.bottom - rcClient.bottom) + margin);
    }
}

} // namespace webview_prefs
