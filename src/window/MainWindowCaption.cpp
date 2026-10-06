// MainWindowCaption.cpp — 主窗口的非客户区委派、DPI 变更、标题栏与拖拽区、标题栏按钮度量、系统菜单与尺寸约束
#include "pch.h"
#include "window/MainWindow.h"
#include "window/WindowGeometryMath.h"
#include "webview/WebViewHost.h"
#include "api/EventEmit.h"
#include "api/generated/WindowSchema.h"

// ============================================
// 客户区扩展到标题栏 - 核心实现
// ============================================

LRESULT MainWindow::OnNcCalcSize(WPARAM wParam, LPARAM lParam) {
    // 使用标准窗口行为，保留系统标题栏和边框
    return DefWindowProcW(hwnd_, WM_NCCALCSIZE, wParam, lParam);
}

LRESULT MainWindow::OnNcHitTest(int screenX, int screenY) {
    // 使用标准窗口行为，让系统处理标题栏和边框
    return DefWindowProcW(hwnd_, WM_NCHITTEST, 0, MAKELPARAM(screenX, screenY));
}

void MainWindow::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
    // 更新 DPI 相关尺寸
    UpdateDpiDependentSizes();
    
    // 应用新的窗口大小
    RECT* suggested = reinterpret_cast<RECT*>(lParam);
    SetWindowPos(hwnd_, nullptr, 
        suggested->left, suggested->top,
        suggested->right - suggested->left,
        suggested->bottom - suggested->top,
        SWP_NOZORDER | SWP_NOACTIVATE);
    
    // DPI 提高后，系统建议的尺寸可能已低于新 DPI 下的物理最小值。
    // WM_GETMINMAXINFO 只约束后续的拖拽/程序化改尺寸，不会追认这次
    // 系统主动施加的尺寸，故此处主动 revalidate（Q9-c）。
    RevalidateSizeAgainstConstraints();
    
    // 通知 WebView DPI 变化
    if (webView_ && webView_->IsReady()) {
        int dpi = HIWORD(wParam);
        double dpiScale = dpi / 96.0;
        
        // 1. 发送 DPI 变化事件
        api::window::DpiChangedPayload dpiChanged;
        dpiChanged.dpi = dpi;
        dpiChanged.dpiScale = dpiScale;
        dpiChanged.titlebarHeight = titlebarHeight_;
        dpiChanged.captionButtonWidth = captionButtonWidth_;
        dpiChanged.captionButtonsWidth = captionButtonWidth_ * 3;
        api::emit::Post<api::window::events::DpiChanged>(*webView_, dpiChanged);
        
        // 2. 注入 CSS 变量更新脚本
        std::wstring cssUpdateScript = L"(function() {"
            L"const root = document.documentElement;"
            L"root.style.setProperty('--dpi', '" + std::to_wstring(dpi) + L"');"
            L"root.style.setProperty('--dpi-scale', '" + std::to_wstring(dpiScale) + L"');"
            L"root.style.setProperty('--titlebar-height', '" + std::to_wstring(titlebarHeight_) + L"px');"
            L"root.style.setProperty('--caption-button-width', '" + std::to_wstring(captionButtonWidth_) + L"px');"
            L"console.log('[DPI] CSS variables updated: dpi=' + " + std::to_wstring(dpi) + L");"
            L"})();";
        webView_->ExecuteScript(cssUpdateScript, nullptr);
        
        // 3. 刷新 WebView 渲染以防止模糊
        if (!isMinimized_) {
            RECT clientRect;
            GetClientRect(hwnd_, &clientRect);
            webView_->Resize(clientRect);
        }
        
        FB2K_console_print("[WebView2 UI] DPI changed to ", dpi, " (scale: ", dpiScale, ")");
    }
}


// ============================================
// 标题栏区域管理
// ============================================

void MainWindow::SetTitlebarHeight(int height) {
    if (height > 0 && height != titlebarHeight_) {
        titlebarHeight_ = height;
        ExtendFrameIntoClientArea();
        
        // 触发重新布局
        RECT rect;
        GetClientRect(hwnd_, &rect);
        OnSize(rect.right, rect.bottom);
    }
}

void MainWindow::SetDragRegions(const std::vector<TitlebarDragRegion>& regions) {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    dragRegions_ = regions;
}

void MainWindow::ClearDragRegions() {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    dragRegions_.clear();
}

void MainWindow::SetNoDragRegions(const std::vector<TitlebarDragRegion>& regions) {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    noDragRegions_ = regions;
}

void MainWindow::AddNoDragRegion(const TitlebarDragRegion& region) {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    noDragRegions_.push_back(region);
}

void MainWindow::ClearNoDragRegions() {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    noDragRegions_.clear();
}

int MainWindow::GetCaptionButtonWidth() const {
    return captionButtonWidth_;
}

int MainWindow::GetCaptionButtonsWidth() const {
    return captionButtonWidth_ * 3;  // 最小化、最大化、关闭
}

RECT MainWindow::GetCaptionButtonsRect() const {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    
    int buttonsWidth = captionButtonWidth_ * 3;
    
    return RECT{
        rc.right - buttonsWidth,  // left
        0,                        // top
        rc.right,                 // right
        titlebarHeight_           // bottom
    };
}

bool MainWindow::IsPointInDragRegion(int clientX, int clientY) const {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    for (const auto& region : dragRegions_) {
        if (clientX >= region.x && clientX < region.x + region.width &&
            clientY >= region.y && clientY < region.y + region.height) {
            return true;
        }
    }
    return false;
}

bool MainWindow::IsPointInNoDragRegion(int clientX, int clientY) const {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    for (const auto& region : noDragRegions_) {
        if (clientX >= region.x && clientX < region.x + region.width &&
            clientY >= region.y && clientY < region.y + region.height) {
            return true;
        }
    }
    return false;
}

void MainWindow::ShowSystemMenu(int screenX, int screenY) {
    // 获取系统菜单
    HMENU sysMenu = GetSystemMenu(hwnd_, FALSE);
    if (!sysMenu) return;
    
    // 根据窗口状态更新菜单项
    UINT enableRestore = isMaximized_ ? MF_ENABLED : MF_GRAYED;
    UINT enableMaximize = isMaximized_ ? MF_GRAYED : MF_ENABLED;
    UINT enableMove = isMaximized_ ? MF_GRAYED : MF_ENABLED;
    UINT enableSize = isMaximized_ ? MF_GRAYED : MF_ENABLED;
    
    EnableMenuItem(sysMenu, SC_RESTORE, MF_BYCOMMAND | enableRestore);
    EnableMenuItem(sysMenu, SC_MAXIMIZE, MF_BYCOMMAND | enableMaximize);
    EnableMenuItem(sysMenu, SC_MOVE, MF_BYCOMMAND | enableMove);
    EnableMenuItem(sysMenu, SC_SIZE, MF_BYCOMMAND | enableSize);
    
    // 设置默认项（还原或最大化）
    SetMenuDefaultItem(sysMenu, isMaximized_ ? SC_RESTORE : SC_MAXIMIZE, FALSE);
    
    // 显示菜单并跟踪选择
    UINT cmd = TrackPopupMenu(sysMenu,
        TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
        screenX, screenY, 0, hwnd_, nullptr);
    
    // 执行选中的命令
    if (cmd != 0) {
        PostMessageW(hwnd_, WM_SYSCOMMAND, cmd, 0);
    }
}

// ============================================
// 窗口尺寸限制方法
// ============================================

void MainWindow::SetMinSizeDip(int widthDip, int heightDip) {
    minWidthDip_ = widthDip > 0 ? widthDip : 1;
    minHeightDip_ = heightDip > 0 ? heightDip : 1;

    // DPI 提高后，当前窗口尺寸可能已低于新的物理下限。系统不会主动纠正
    // 已存在的窗口尺寸（WM_GETMINMAXINFO 只约束后续的拖拽/程序化改尺寸），
    // 故此处主动 revalidate 一次，使新约束立即生效而不是等到下次 resize。
    RevalidateSizeAgainstConstraints();
}

void MainWindow::SetMaxSizeDip(int widthDip, int heightDip) {
    maxWidthDip_ = widthDip >= 0 ? widthDip : 0;
    maxHeightDip_ = heightDip >= 0 ? heightDip : 0;

    RevalidateSizeAgainstConstraints();
}

void MainWindow::RevalidateSizeAgainstConstraints() {
    if (!hwnd_ || !IsWindow(hwnd_)) return;
    // 最大化/全屏由系统或全屏逻辑掌管尺寸，不在此干预。
    if (isMaximized_ || isFullscreen_ || IsZoomed(hwnd_)) return;

    // 最小化时 GetWindowRect 返回的是 iconic 位置，对其调 SetWindowPos 会
    // 污染还原态几何。改为登记待办，等恢复后的 WM_SIZE 再执行，使约束不丢失。
    if (isMinimized_ || IsIconic(hwnd_)) {
        pendingConstraintRevalidate_ = true;
        return;
    }

    RECT rc{};
    if (!GetWindowRect(hwnd_, &rc)) return;

    const int curW = rc.right - rc.left;
    const int curH = rc.bottom - rc.top;
    const int dpi = GetDpiForWindow(hwnd_);

    const auto clamped = window_geometry::ClampSize(
        curW, curH,
        window_geometry::DipToPhysical(minWidthDip_, dpi),
        window_geometry::DipToPhysical(minHeightDip_, dpi),
        window_geometry::DipToPhysical(maxWidthDip_, dpi),
        window_geometry::DipToPhysical(maxHeightDip_, dpi));

    if (!clamped.clamped) return;

    SetWindowPos(hwnd_, nullptr, 0, 0, clamped.width, clamped.height,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}
