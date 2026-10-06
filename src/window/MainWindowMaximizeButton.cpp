// MainWindowMaximizeButton.cpp - 页面自绘的最大化键以 HTMAXBUTTON 回答命中测试。
//
// Windows 11 只对 WM_NCHITTEST 在光标下答 HTMAXBUTTON 的窗口弹出贴靠布局浮层。
// 答了它，按钮上的鼠标输入就变成页面收不到的非客户区消息，所以落在按钮上的输入
// 一律当作普通鼠标输入转给 WebView：页面照常显示悬停与按下样式，照常由自己的点击
// 处理切换最大化。宿主自己不做最大化，一次点击不会生效两次。
#include "pch.h"
#include "window/MainWindow.h"
#include "webview/WebViewHost.h"

#include <windowsx.h>

void MainWindow::SetMaximizeButtonRegion(
    const std::optional<maximize_button_region::Rect>& region) {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    maximizeButtonRegion_ = region;
}

// 矩形属于报告它的那个页面。新页面不再报告时，旧矩形会一直把那块区域当作最大化键，
// 吞掉新页面在那里的点击，所以换页即作废，由新页面重新报告。
void MainWindow::OnTopLevelNavigationStarting() {
    SetMaximizeButtonRegion(std::nullopt);
}

bool MainWindow::IsPointInMaximizeButton(int clientX, int clientY) const {
    {
        std::lock_guard<std::mutex> lock(regionsMutex_);
        if (!maximizeButtonRegion_ ||
            !maximize_button_region::Contains(*maximizeButtonRegion_, clientX, clientY)) {
            return false;
        }
    }
    maximize_button_region::Conditions conditions;
    conditions.snapLayoutsSupported = maximize_button_region::SnapLayoutsSupported();
    conditions.frameless = frameless_;
    conditions.resizable = resizable_;
    conditions.fullscreen = isFullscreen_;
    conditions.webViewForwarding =
        webView_ && webView_->IsReady() && webView_->GetCompositionController() != nullptr;
    return maximize_button_region::ShouldAnswerMaximizeButton(conditions);
}

bool MainWindow::HandleMaximizeButtonMessage(UINT msg, WPARAM wParam, LPARAM lParam,
                                             LRESULT& result) {
    switch (msg) {
    case WM_NCMOUSEMOVE:
    case WM_NCMOUSELEAVE:
    case WM_MOUSELEAVE:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCLBUTTONUP:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
        break;
    default:
        return false;
    }
    if (!webView_) {
        return false;
    }

    const auto toClient = [this](LPARAM screenPos) {
        POINT pt = {GET_X_LPARAM(screenPos), GET_Y_LPARAM(screenPos)};
        ScreenToClient(hwnd_, &pt);
        return pt;
    };
    // 光标当前位置的命中码。每次重新问，不缓存：页面随时可能挪动矩形。
    const auto cursorHit = [this]() -> LRESULT {
        POINT cursor = {};
        if (!GetCursorPos(&cursor) || WindowFromPoint(cursor) != hwnd_) {
            return HTNOWHERE;
        }
        return SendMessageW(hwnd_, WM_NCHITTEST, 0, MAKELPARAM(cursor.x, cursor.y));
    };

    switch (msg) {
    case WM_NCMOUSEMOVE: {
        if (wParam != HTMAXBUTTON) {
            return false;
        }
        // 每次移动都重新挂上：点击时的抓取与释放可能取消跟踪，漏掉 WM_NCMOUSELEAVE
        // 会让页面的悬停态一直亮着。
        TRACKMOUSEEVENT tme = {sizeof(tme)};
        tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
        webView_->SendNonClientMouseInput(COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE, toClient(lParam),
                                          false);
        // 继续交给默认处理，与 Windows Terminal 等自绘标题栏的做法一致；
        // 它不画任何东西，非客户区面积为 0。
        return false;
    }

    case WM_NCMOUSELEAVE:
        // 按下时抓取鼠标，Windows 也会发这条；此时鼠标归这次按下，不算离开。
        if (!webView_->IsCapturingMouse()) {
            const LRESULT hit = cursorHit();
            // 移回客户区时，紧接着的 WM_MOUSEMOVE 会更新页面；移到别处则要告诉页面
            // 鼠标离开了按钮。
            if (hit != HTCLIENT && hit != HTMAXBUTTON) {
                webView_->SendNonClientMouseInput(COREWEBVIEW2_MOUSE_EVENT_KIND_LEAVE, POINT{},
                                                  false);
            }
        }
        // DwmDefWindowProc 与 DefWindowProc 也要看到它。
        return false;

    case WM_MOUSELEAVE:
        // 光标从页面移到了按钮上：对页面来说鼠标并没有离开。
        if (!webView_->IsCapturingMouse() && cursorHit() == HTMAXBUTTON) {
            webView_->ResetClientMouseTracking();
            result = 0;
            return true;
        }
        return false;

    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
        if (wParam != HTMAXBUTTON) {
            return false;
        }
        // 不交给 DefWindowProc：它会自己跟踪按钮，在页面上画出经典样式的按下按钮，
        // 松开时还会自己发 SC_MAXIMIZE。抓取鼠标后，松开以 WM_LBUTTONUP 经客户区
        // 路径到达页面，页面据此产生 click。
        webView_->SendNonClientMouseInput(msg == WM_NCLBUTTONDOWN
                                              ? COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN
                                              : COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOUBLE_CLICK,
                                          toClient(lParam), true);
        result = 0;
        return true;

    case WM_NCLBUTTONUP:
        // 只在没有抓取时到达：它结束的那次按下从未交给页面，吞掉即可。
        if (wParam != HTMAXBUTTON) {
            return false;
        }
        result = 0;
        return true;

    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
        // 右键照常交给页面，按钮上不弹系统菜单。
        if (wParam != HTMAXBUTTON) {
            return false;
        }
        webView_->SendNonClientMouseInput(msg == WM_NCRBUTTONDOWN
                                              ? COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOWN
                                              : COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_UP,
                                          toClient(lParam), false);
        result = 0;
        return true;

    default:
        return false;
    }
}
