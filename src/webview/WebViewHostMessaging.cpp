/**
 * WebViewHostMessaging.cpp - WebViewHost 的页面消息与导航
 *
 * 注册 WebMessageReceived、NavigationStarting/Completed 与 NewWindowRequested，移除 artwork
 * 生命周期记账的三个事件，以及 Navigate、ExecuteScript、PostMessage 等对页面的调用。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "api/ErrorEnvelope.h"
#include "webview/dnd/DndTrace.h"
#include "window/WindowBehaviorTrace.h"
#include <wrl/event.h>

using namespace Microsoft::WRL;

namespace {
    // 拖放取证（webview_dnd_trace.log）里的一条导航或新窗口请求。data: 页面的 URI
    // 是整页内容，只留开头。Args 为 NavigationStarting 或 NewWindowRequested 的参数，
    // 两者都有 get_Uri 与 get_IsUserInitiated。
    template <class Args>
    void TraceNavigationRequest(const char* what, HWND host, Args* args) noexcept {
        try {
            wil::unique_cotaskmem_string uri;
            BOOL userInitiated = FALSE;
            args->get_Uri(&uri);
            args->get_IsUserInitiated(&userInitiated);
            std::string text = uri ? pfc::stringcvt::string_utf8_from_wide(uri.get()).get_ptr()
                                   : std::string();
            constexpr size_t kMaxUri = 300;
            if (text.size() > kMaxUri) {
                text.resize(kMaxUri);
                text += "...";
            }
            std::ostringstream line;
            line << "[dnd] " << what << " host=0x" << std::hex
                 << reinterpret_cast<uintptr_t>(host) << std::dec
                 << " userInitiated=" << (userInitiated ? 1 : 0) << " uri=" << text;
            fb2k_dnd::DndTrace(line.str());
        } catch (...) {
            // Diagnostics must never disturb navigation.
        }
    }
}

void WebViewHost::RegisterMessageHandler() {
    // 监听来自 JavaScript 的消息
    webview_->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                // 安全: Origin 验证门禁。顶层文档与发消息的文档都要可信。
                wil::unique_cotaskmem_string messageSource;
                if (!IsOriginAllowed(sender) || FAILED(args->get_Source(&messageSource)) || !messageSource ||
                    !IsTrustedOrigin(messageSource.get())) {
                    RejectUntrustedMessage(args);
                    return S_OK;
                }
                
                if (wil::unique_cotaskmem_string message;
                    SUCCEEDED(args->get_WebMessageAsJson(&message)) && message && messageHandler_) {
                    messageHandler_(message.get());
                }
                
                return S_OK;
            }
        ).Get(),
        &webMessageReceivedToken_
    );
    
    // 导航完成事件
    const HRESULT navigationStartingHr = webview_->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                if (UINT64 id = 0; args && SUCCEEDED(args->get_NavigationId(&id))) {
                    latestNavigationId_ = id;
                }
                // 记录导航请求；文件 drop 是否触发导航，由拖放取证日志判断。
                if (fb2k_dnd::DndTraceEnabled() && args) {
                    TraceNavigationRequest("nav.starting", parentHwnd_, args);
                }
                artworkLifecycle_.OnNavigationStarting();
                ClearMediaRequests();
                // Abort queued/in-flight work that captured the previous generation.
                if (artworkWorkerQueue_) {
                    artworkWorkerQueue_->CancelStale(
                        artworkLifecycle_.NavigationGeneration(),
                        artworkLifecycle_.HostGeneration());
                }
                if (navigationStartingCallback_) {
                    navigationStartingCallback_();
                }
                return S_OK;
            }
        ).Get(),
        &artworkNavigationStartingToken_
    );
    artworkLifecycle_.RecordTokenAdd(
        artwork_request::TokenKind::NavigationStarting,
        SUCCEEDED(navigationStartingHr));

    const HRESULT navigationCompletedHr = webview_->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this](ICoreWebView2* /*sender*/, ICoreWebView2NavigationCompletedEventArgs* args) {
                artworkLifecycle_.OnNavigationCompleted();
                BOOL success;
                args->get_IsSuccess(&success);
                
                // 被顶掉的导航报的错误码与连不上的取值重叠，所以按导航 ID 判：
                // 失败的这次已不是最近开始的那次，就是被后来的导航取消的。
                UINT64 completedId = 0;
                args->get_NavigationId(&completedId);
                lastNavigationSuperseded_ = !success && completedId != latestNavigationId_;
                if (success) {
                    LOG("Navigation completed successfully");
                } else {
                    COREWEBVIEW2_WEB_ERROR_STATUS status;
                    args->get_WebErrorStatus(&status);
                    LOG("Navigation failed, error status: ", (int)status);
                }

                if (navigationCompletedCallback_) {
                    navigationCompletedCallback_(success != FALSE);
                }
                
                return S_OK;
            }
        ).Get(),
        &navigationCompletedToken_
    );
    artworkLifecycle_.RecordTokenAdd(
        artwork_request::TokenKind::NavigationCompleted,
        SUCCEEDED(navigationCompletedHr));

    // 页面没有接住的文件放下，WebView2 的默认做法是为那个文件发 NewWindowRequested，
    // 不处理就自己开一个窗口显示它（音频文件是 Chromium 自带的媒体页）。那个窗口不归
    // 宿主管，也不是页面要的，所以 file: 地址的新窗口一律拦下；拖进来的文件照常经
    // dnd:drop 与页面自己的 drop 处理。其他地址保持默认行为。处理函数不捕获 this，
    // 随 webview_ 释放。
    {
        const HWND host = parentHwnd_;
        EventRegistrationToken newWindowToken{};
        webview_->add_NewWindowRequested(
            Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                [host](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                    if (!args) {
                        return S_OK;
                    }
                    wil::unique_cotaskmem_string uri;
                    const bool isFile = SUCCEEDED(args->get_Uri(&uri)) && uri &&
                                        _wcsnicmp(uri.get(), L"file:", 5) == 0;
                    if (fb2k_dnd::DndTraceEnabled()) {
                        TraceNavigationRequest(isFile ? "newWindow.blocked" : "newWindow.requested",
                                               host, args);
                    }
                    if (isFile) {
                        args->put_Handled(TRUE);
                    }
                    return S_OK;
                }
            ).Get(),
            &newWindowToken
        );
    }
    
    LOG("Message handlers registered");
}

void WebViewHost::RemoveArtworkEventHandlers() noexcept {
    if (!webview_) return;

    if (artworkResourceRequestedToken_.value != 0 &&
        SUCCEEDED(webview_->remove_WebResourceRequested(artworkResourceRequestedToken_))) {
        artworkLifecycle_.RemoveToken(artwork_request::TokenKind::WebResourceRequested);
        artworkResourceRequestedToken_ = {};
    }
    if (artworkNavigationStartingToken_.value != 0 &&
        SUCCEEDED(webview_->remove_NavigationStarting(artworkNavigationStartingToken_))) {
        artworkLifecycle_.RemoveToken(artwork_request::TokenKind::NavigationStarting);
        artworkNavigationStartingToken_ = {};
    }
    if (navigationCompletedToken_.value != 0 &&
        SUCCEEDED(webview_->remove_NavigationCompleted(navigationCompletedToken_))) {
        artworkLifecycle_.RemoveToken(artwork_request::TokenKind::NavigationCompleted);
        navigationCompletedToken_ = {};
    }
}

HRESULT WebViewHost::Navigate(const std::wstring& url) {
    if (!webview_) return E_FAIL;
    LOG("Navigating to: ", pfc::stringcvt::string_utf8_from_wide(url.c_str()).get_ptr());
    window_behavior_trace::Com(parentHwnd_, "webview.Navigate");
    return webview_->Navigate(url.c_str());
}

HRESULT WebViewHost::NavigateToString(const std::wstring& html) {
    if (!webview_) return E_FAIL;
    return webview_->NavigateToString(html.c_str());
}

HRESULT WebViewHost::ExecuteScript(const std::wstring& script, ScriptCallback callback) {
    if (!webview_) return E_FAIL;
    
    return webview_->ExecuteScript(
        script.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [callback](HRESULT /*result*/, LPCWSTR resultJson) {
                if (callback && resultJson) {
                    // 异常安全边界：ExecuteScript 完成回调运行在主线程的 WebView2 COM 回调里，
                    // 回调体内（如 nlohmann::json 访问）抛出的 C++ 异常一旦逃逸到原生回调边界，
                    // 会触发 terminate() 直接崩溃整个 foobar2000 进程，故必须就地兜住。
                    // 全项目 3 处 ExecuteScript 均经此 funnel，这一处兜住整类崩溃。
                    try {
                        callback(resultJson);
                    } catch (const std::exception& e) {
                        console::printf("[WebViewHost] ExecuteScript callback threw: %s", e.what());
                    } catch (...) {
                        console::print("[WebViewHost] ExecuteScript callback threw unknown exception");
                    }
                }
                return S_OK;
            }
        ).Get()
    );
}

void WebViewHost::RejectUntrustedMessage(ICoreWebView2WebMessageReceivedEventArgs* args) {
    wil::unique_cotaskmem_string message;
    if (!webview_ || !args || FAILED(args->get_WebMessageAsJson(&message)) || !message) return;
    try {
        const json request = json::parse(pfc::stringcvt::string_utf8_from_wide(message.get()).get_ptr());
        if (!request.is_object()) return;
        const auto id = request.find("id");
        const auto method = request.find("method");
        if (id == request.end() || !(id->is_number_integer() || id->is_string())) return;
        if (method == request.end() || !method->is_string()) return;
        // 与 BridgeCore::SendError 的失败信封同形；绕过 PostMessage 的来源门禁，这条本来就是发给不可信页面的。
        const json response = {{"type", "response"},
                               {"id", *id},
                               {"error", "The page origin is not trusted"},
                               {"code", ApiErrorCode::ORIGIN_DENIED}};
        webview_->PostWebMessageAsJson(pfc::stringcvt::string_wide_from_utf8(response.dump().c_str()).get_ptr());
    } catch (const std::exception&) {
        // 不可信页面发来的不是合法 JSON：不应答
    }
}

HRESULT WebViewHost::PostMessage(const std::wstring& json) {
    if (!webview_) return E_FAIL;
    if (!IsCurrentDocumentTrusted()) return S_FALSE;
    return webview_->PostWebMessageAsJson(json.c_str());
}

HRESULT WebViewHost::PostEventMessage(const std::string& eventName, const std::wstring& json) {
    window_behavior_trace::Event(parentHwnd_, eventName);
    // 无条件可靠投递。历史上此处曾按事件名字符串分类做 hidden 期间的
    // 丢弃/合并（event_gate），已撤销：事件名空间开放（port.emit 等允许调用方传入
    // 任意名字），宿主无法从名字推断投递语义，默认合并会造成静默丢弃：
    // 带 correlation id 的异步应答（http:response / library:getAllResult /
    // audio:fullWaveform*）按名去重会让并发请求互相覆盖；
    // 不可合并的逐次事实（playback:itemPlayed 等）会丢失记录。
    // 可再生高频流的节约改由生产侧显式承担（IsPageHidden 判定，见
    // AudioApi 频谱分发与 PlaybackCallback 的 timeHighRes）。
    return PostMessage(json);
}
