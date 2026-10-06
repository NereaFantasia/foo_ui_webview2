/**
 * WebViewHostOrigin.cpp - WebViewHost 的来源校验与文档戳
 *
 * 判定页面来源是否可信（内置虚拟主机与宿主登记的 origin，规则见 OriginPolicy.h），以及异步结果
 * 投递前核对发起请求的文档是否还在的 DocumentStamp。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "settings/SecurityConfig.h"
#include "webview/OriginPolicy.h"
#include <vector>
#include <atomic>

//==========================================================================
// Origin 验证: 只允许受信任的来源调用 Bridge API，规则见 origin_policy
//==========================================================================

void WebViewHost::AddTrustedOrigin(const std::wstring& urlOrOrigin) {
    std::wstring origin = origin_policy::OriginOf(urlOrOrigin);
    if (origin.empty()) {
        LOG("AddTrustedOrigin: cannot parse origin from input, ignored");
        return;
    }

    std::lock_guard<std::mutex> lock(extraTrustedOriginsMutex_);
    for (const auto& existing : extraTrustedOrigins_) {
        if (existing == origin) return;  // 去重
    }
    LOG("AddTrustedOrigin: ", pfc::stringcvt::string_utf8_from_wide(origin.c_str()).get_ptr());
    extraTrustedOrigins_.push_back(std::move(origin));
}

bool WebViewHost::IsOriginAllowed(ICoreWebView2* webview) {
    std::wstring origin;
    if (IsSourceTrusted(webview, origin)) return true;
    if (!origin.empty()) {
        LOG("\xe2\x9a\xa0\xef\xb8\x8f Blocked message from untrusted origin: ",
            pfc::stringcvt::string_utf8_from_wide(origin.c_str()).get_ptr());
    }
    return false;
}

bool WebViewHost::IsSourceTrusted(ICoreWebView2* webview, std::wstring& origin) {
    origin.clear();
    if (!webview) return false;

    wil::unique_cotaskmem_string source;
    if (FAILED(webview->get_Source(&source)) || !source) {
        return false;
    }

    origin = source.get();

    return IsTrustedOrigin(origin);
}

// 开发服务器不按当前设置放行，而是在宿主导航过去时登记（WebViewPanel::LoadFrontendPage、
// PopupWindow::NavigateToPage）：页面调 setDevServerConfig 改了地址，自己不会因此失去信任。
bool WebViewHost::IsTrustedOrigin(const std::wstring& url) {
    std::lock_guard<std::mutex> lock(extraTrustedOriginsMutex_);
    return origin_policy::IsTrustedPage(url, extraTrustedOrigins_);
}

std::wstring WebViewHost::CurrentDevServerOrigin() {
    const char* url = security_config::GetDevServerUrl();
    return origin_policy::DevServerOrigin(security_config::UseDevServer(), url ? url : "");
}

std::uint64_t WebViewHost::NextInstanceSerial() {
    static std::atomic<std::uint64_t> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

WebViewHost::DocumentStamp WebViewHost::CaptureDocument() const {
    return {instanceSerial_, artworkLifecycle_.NavigationGeneration(), artworkLifecycle_.HostGeneration()};
}

bool WebViewHost::IsCurrentDocument(const DocumentStamp& stamp) const {
    return stamp.hostSerial == instanceSerial_ && artworkLifecycle_.IsAlive() && !artworkLifecycle_.IsClosing() &&
           stamp.navigationGeneration == artworkLifecycle_.NavigationGeneration() &&
           stamp.hostGeneration == artworkLifecycle_.HostGeneration();
}

bool WebViewHost::IsCurrentDocumentTrusted() {
    std::wstring origin;
    return IsSourceTrusted(webview_.get(), origin);
}
