/**
 * WebViewHostInput.cpp - WebViewHost 的光标、鼠标输入与缩放
 *
 * Visual Hosting 模式下的光标同步与隐藏、鼠标输入转发、缩放比例与默认缩放偏好、
 * app-region 命中查询，以及拖放转发用到的 CompositionController3 与当前页面 origin。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "webview/OriginPolicy.h"
#include "prefs/PreferencesPage.h"   // webview_prefs::GetDefaultZoomPercent
#include "prefs/PreferencesZoom.h"
#include <wrl/event.h>

using namespace Microsoft::WRL;

void WebViewHost::SetupCursorHandling() {
    if (!compositionController_) return;
    
    // 注册光标变化事件
    HRESULT hr = compositionController_->add_CursorChanged(
        Callback<ICoreWebView2CursorChangedEventHandler>(
            [this](ICoreWebView2CompositionController* sender, IUnknown* /*args*/) {
                if (!parentHwnd_) return S_OK;
                
                // CompositionController 模式下 Chromium 不会处理 WM_SETCURSOR,
                // 必须由父窗口在每次 WM_SETCURSOR 主动 SetCursor 才能保证光标始终
                // 反映 Chromium 当前希望显示的状态。
                //
                // 历史方案曾把光标写入 GCLP_HCURSOR 让 DefWindowProc 接管恢复,
                // 但会在 alt+tab 等合成 WM_SETCURSOR 时残留旧光标(问题 B);
                // 同时 Chromium 在 CSS `cursor: none` ↔ default 切换时不触发
                // CursorChanged(实测仅在 hover 元素的 cursor 类型变化时触发),
                // 也不会把"隐藏"语义反映在 get_Cursor() 返回值里。
                //
                // 因此本回调只做即时同步: 把 Chromium 当前光标设为系统当前光标,
                // 并把它缓存到 currentCursor_, 供 MainWindow/PopupWindow 在
                // WM_SETCURSOR 时主动调用 ApplyCurrentCursor 使用。
                // CSS `cursor: none` 通过显式 cursor.setHidden(true) API 由前端
                // 主动通知 (见 CursorApi)。
                HCURSOR cursor = nullptr;
                if (SUCCEEDED(sender->get_Cursor(&cursor))) {
                    currentCursor_ = cursor;
                    if (!cursorHidden_) {
                        ::SetCursor(cursor);
                    }
                }
                return S_OK;
            }
        ).Get(),
        &cursorChangedToken_
    );
    
    if (SUCCEEDED(hr)) {
        LOG("✓ Cursor change handler registered");
    }
}

bool WebViewHost::ApplyCurrentCursor() {
    // 没有 CompositionController 说明走的是标准 Controller 模式,
    // 那种模式下 WebView2 自己拥有子 HWND 处理 WM_SETCURSOR, 父窗口不应介入。
    if (!compositionController_) {
        return false;
    }

    if (cursorHidden_) {
        // 显式隐藏: 任何来源的 WM_SETCURSOR 都强制隐藏。
        ::SetCursor(nullptr);
        return true;
    }

    if (currentCursor_) {
        ::SetCursor(currentCursor_);
        return true;
    }

    // CursorChanged 还未触发过 (例如 WebView 刚就绪、鼠标尚未进入客户区)。
    // 由调用方 fallback 到 DefWindowProc, 让系统使用类光标 (默认箭头)。
    return false;
}

bool WebViewHost::SetCursorHidden(bool hidden) {
    if (cursorHidden_ == hidden) {
        return false;
    }
    cursorHidden_ = hidden;

    // 立即生效: 模拟一次 WM_SETCURSOR, 让光标状态在不等下一次鼠标事件的
    // 情况下也能切换。GetCursorPos + WindowFromPoint 用来确认当前鼠标
    // 是否还在自己的客户区, 避免在跨窗口时误改其他窗口的光标。
    if (!parentHwnd_) {
        return true;
    }
    POINT pt;
    if (!GetCursorPos(&pt)) {
        return true;
    }
    HWND hover = WindowFromPoint(pt);
    if (hover != parentHwnd_) {
        return true;
    }
    if (hidden) {
        ::SetCursor(nullptr);
    } else if (currentCursor_) {
        ::SetCursor(currentCursor_);
    } else {
        ::SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    return true;
}

bool WebViewHost::HandleMouseMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    // 如果没有 CompositionController，不处理
    if (!compositionController_) {
        return false;
    }
    
    POINT point;
    POINTSTOPOINT(point, lParam);
    
    // 滚轮消息是屏幕坐标，需要转换为客户端坐标
    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
        ScreenToClient(parentHwnd_, &point);
    }
    
    // ============================================
    // 边缘检测：不转发边缘区域的鼠标事件
    // 让 WM_NCHITTEST 处理窗口边缘调整大小
    // ============================================
    RECT clientRect;
    GetClientRect(parentHwnd_, &clientRect);
    const int EDGE_WIDTH = 6;  // 边缘检测宽度（像素）
    
    bool isOnEdge = (point.x < EDGE_WIDTH || 
                     point.x >= clientRect.right - EDGE_WIDTH ||
                     point.y < EDGE_WIDTH || 
                     point.y >= clientRect.bottom - EDGE_WIDTH);
    
    // 鼠标离开消息也需要处理
    if (message == WM_MOUSELEAVE) {
        isTrackingMouse_ = false;
        // 仍然转发给 WebView
    }
    // 边缘区域不转发（除非正在捕获鼠标）
    else if (isOnEdge && !isCapturingMouse_) {
        return false;  // 交给 DefWindowProc 处理
    }
    
    // ============================================
    // 鼠标捕获管理
    // ============================================
    if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN) {
        SetCapture(parentHwnd_);
        isCapturingMouse_ = true;
    }
    else if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP) {
        ReleaseCapture();
        isCapturingMouse_ = false;
    }
    
    // 跟踪鼠标离开
    if (!isTrackingMouse_ && message == WM_MOUSEMOVE) {
        TRACKMOUSEEVENT tme = { sizeof(tme) };
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = parentHwnd_;
        TrackMouseEvent(&tme);
        isTrackingMouse_ = true;
    }
    
    // ============================================
    // 转发给 WebView
    // ============================================
    COREWEBVIEW2_MOUSE_EVENT_KIND eventKind;
    switch (message) {
        case WM_MOUSEMOVE:      eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE; break;
        case WM_LBUTTONDOWN:    eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN; break;
        case WM_LBUTTONUP:      eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP; break;
        case WM_LBUTTONDBLCLK:  eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOUBLE_CLICK; break;
        case WM_RBUTTONDOWN:    eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOWN; break;
        case WM_RBUTTONUP:      eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_UP; break;
        case WM_RBUTTONDBLCLK:  eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOUBLE_CLICK; break;
        case WM_MBUTTONDOWN:    eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOWN; break;
        case WM_MBUTTONUP:      eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_UP; break;
        case WM_MBUTTONDBLCLK:  eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOUBLE_CLICK; break;
        case WM_MOUSEWHEEL:     eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_WHEEL; break;
        case WM_MOUSEHWHEEL:    eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_HORIZONTAL_WHEEL; break;
        case WM_XBUTTONDOWN:    eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOWN; break;
        case WM_XBUTTONUP:      eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_UP; break;
        case WM_XBUTTONDBLCLK:  eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOUBLE_CLICK; break;
        case WM_MOUSELEAVE:     eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEAVE; break;
        default:
            return false;  // 不认识的消息
    }
    
    // 虚拟键状态
    COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS virtualKeys = COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE;
    if (wParam & MK_CONTROL) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_CONTROL);
    if (wParam & MK_SHIFT)   virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_SHIFT);
    if (wParam & MK_LBUTTON) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_LEFT_BUTTON);
    if (wParam & MK_MBUTTON) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_MIDDLE_BUTTON);
    if (wParam & MK_RBUTTON) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_RIGHT_BUTTON);
    if (wParam & MK_XBUTTON1) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON1);
    if (wParam & MK_XBUTTON2) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON2);
    
    // 鼠标滚轮数据
    UINT32 mouseData = 0;
    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
        mouseData = GET_WHEEL_DELTA_WPARAM(wParam);
    }
    else if (message == WM_XBUTTONDOWN || message == WM_XBUTTONUP || message == WM_XBUTTONDBLCLK) {
        mouseData = GET_XBUTTON_WPARAM(wParam);
    }
    
    // 发送鼠标输入
    HRESULT hr = compositionController_->SendMouseInput(eventKind, virtualKeys, mouseData, point);
    
    return SUCCEEDED(hr);
}

bool WebViewHost::SendNonClientMouseInput(COREWEBVIEW2_MOUSE_EVENT_KIND kind, POINT clientPt,
                                          bool beginCapture) {
    if (!compositionController_) {
        return false;
    }

    // GetKeyState 反映的是本消息被取出时的状态，与客户区消息 wParam 里的 MK_* 同一时刻。
    auto down = [](int vk) { return (::GetKeyState(vk) & 0x8000) != 0; };
    int keys = COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE;
    if (down(VK_CONTROL))  keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_CONTROL;
    if (down(VK_SHIFT))    keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_SHIFT;
    if (down(VK_LBUTTON))  keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_LEFT_BUTTON;
    if (down(VK_RBUTTON))  keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_RIGHT_BUTTON;
    if (down(VK_MBUTTON))  keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_MIDDLE_BUTTON;
    if (down(VK_XBUTTON1)) keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON1;
    if (down(VK_XBUTTON2)) keys |= COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON2;

    if (beginCapture) {
        SetCapture(parentHwnd_);
        isCapturingMouse_ = true;
    }
    if (kind == COREWEBVIEW2_MOUSE_EVENT_KIND_LEAVE) {
        isTrackingMouse_ = false;
    }

    const HRESULT hr = compositionController_->SendMouseInput(
        kind, static_cast<COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS>(keys), 0, clientPt);
    return SUCCEEDED(hr);
}
// 临时文件缺失时的后备实现

// 缩放因子
HRESULT WebViewHost::SetZoomFactor(double zoomFactor) {
    if (!controller_) return E_FAIL;

    wil::com_ptr<ICoreWebView2Controller3> controller3;
    HRESULT hr = controller_->QueryInterface(IID_PPV_ARGS(&controller3));
    if (SUCCEEDED(hr) && controller3) {
        return controller3->put_ZoomFactor(zoomFactor);
    }
    return E_NOTIMPL;
}

double WebViewHost::GetZoomFactor() const {
    if (!controller_) return 1.0;

    wil::com_ptr<ICoreWebView2Controller3> controller3;
    HRESULT hr = controller_->QueryInterface(IID_PPV_ARGS(&controller3));
    if (SUCCEEDED(hr) && controller3) {
        double zoomFactor = 1.0;
        controller3->get_ZoomFactor(&zoomFactor);
        return zoomFactor;
    }
    return 1.0;
}

HRESULT WebViewHost::SetZoomForDpi(int dpi) {
    // 96 DPI = 100% = 1.0x
    // 120 DPI = 125% = 1.25x
    // 144 DPI = 150% = 1.5x
    double zoomFactor = dpi / 96.0;
    return SetZoomFactor(zoomFactor);
}

void WebViewHost::ApplyDefaultZoomPreference() {
    // WebView2 新建时就是 1.0；偏好为 100 时什么都不做，也不把主题稍后的覆盖抢在前面。
    const int percent = webview_prefs::GetDefaultZoomPercent();
    if (percent == prefs_zoom::kDefaultPercent) return;
    SetZoomFactor(prefs_zoom::FactorFromPercent(percent));
}

// CSS app-region 查询
// 需要 ICoreWebView2CompositionController4 (SDK 1.0.1185+)
int WebViewHost::GetNonClientRegionAtPoint(POINT clientPt) {
    if (!compositionController_) return 0;

    // GetNonClientRegionAtPoint 在 ICoreWebView2CompositionController4 中
    wil::com_ptr<ICoreWebView2CompositionController4> controller4;
    HRESULT hr = compositionController_->QueryInterface(IID_PPV_ARGS(&controller4));
    if (FAILED(hr) || !controller4) {
        // 仅首次失败时记录，避免日志洪泛
        static bool logged = false;
        if (!logged) {
            LOG("GetNonClientRegionAtPoint: ICoreWebView2CompositionController4 QI failed, hr=", 
                pfc::format_hex((uint32_t)hr));
            logged = true;
        }
        return 0;  // 当前运行时不支持此功能
    }

    COREWEBVIEW2_NON_CLIENT_REGION_KIND regionKind;
    hr = controller4->GetNonClientRegionAtPoint(clientPt, &regionKind);

    if (FAILED(hr)) {
        static bool logged = false;
        if (!logged) {
            LOG("GetNonClientRegionAtPoint failed, hr=", pfc::format_hex((uint32_t)hr));
            logged = true;
        }
        return 0;
    }

    // 转换为简单的整数返回值
    switch (regionKind) {
        case COREWEBVIEW2_NON_CLIENT_REGION_KIND_CAPTION:
            return 1;  // drag 区域
        case COREWEBVIEW2_NON_CLIENT_REGION_KIND_CLIENT:
            return 0;  // 客户区
        default:
            return 2;  // 其他
    }
}

// Drag-drop forwarding entry points for Visual Hosting.
// The QI result is cached on both outcomes: a runtime that lacks the interface
// would otherwise be re-queried on every DragOver, of which a single drag
// produces dozens.
ICoreWebView2CompositionController3* WebViewHost::GetCompositionController3() {
    if (compositionController3Probed_) {
        return compositionController3_.get();
    }
    if (!compositionController_) {
        return nullptr;  // not Visual Hosting, or not initialised yet
    }

    compositionController3Probed_ = true;
    HRESULT hr = compositionController_->QueryInterface(IID_PPV_ARGS(&compositionController3_));
    if (FAILED(hr) || !compositionController3_) {
        compositionController3_.reset();
        LOG("GetCompositionController3: QI failed, hr=", pfc::format_hex((uint32_t)hr));
        return nullptr;
    }
    return compositionController3_.get();
}

std::wstring WebViewHost::GetCurrentOriginNormalized() {
    if (!webview_) {
        return L"";
    }
    wil::unique_cotaskmem_string source;
    if (FAILED(webview_->get_Source(&source)) || !source) {
        return L"";
    }
    return origin_policy::OriginOf(source.get());
}
