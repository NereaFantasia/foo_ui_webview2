// HttpApi.cpp - HTTP Request API
// Provides HTTP GET/POST/HEAD requests and file download (ASYNC VERSION)

#include "pch.h"
#include "api/HttpApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/ErrorEnvelope.h"
#include "core/WebViewContext.h"
#include "utils/PathExpansion.h"
#include <winhttp.h>
#include <ws2tcpip.h>
#include <thread>
#include <filesystem>
#include <fstream>
#include <regex>
#include <atomic>
#include <mutex>
#include <random>
#include <unordered_map>
#include "utils/Base64.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

namespace fs = std::filesystem;

// 安全: 引入安全配置
namespace security_config {
    extern bool IsLocalNetworkAccessAllowed();
    extern bool IsInsecureTlsAllowed();
}

namespace {
    using json = nlohmann::json;

    // noexcept dispatcher used by every async lambda below to deliver
    // an HTTP response/event JSON back to the calling window. Any exception
    // raised by SendEventTo / EmitEvent / BridgeCore::EmitEvent (json copy,
    // std::bad_alloc, etc.) is swallowed and logged instead of propagating
    // into the fb2k main-thread callback runner or std::thread context,
    // where it would trigger std::terminate. Resolves bugprone-exception-escape.
    inline void DispatchHttpEventSafely(HWND callerHwnd,
                                        const std::string& callerWindowId,
                                        const char* eventName,
                                        const json& payload) noexcept {
        try {
            auto& wvc = WebViewContext::GetInstance();
            bool sent = false;
            if (!callerWindowId.empty()) {
                sent = wvc.SendEventTo(callerWindowId, eventName, payload);
            }
            if (!sent && callerHwnd) {
                if (auto* bridge = wvc.GetBridge(callerHwnd)) {
                    bridge->EmitEvent(eventName, payload);
                    sent = true;
                }
            }
            if (!sent) {
                BridgeCore::GetInstance().EmitEvent(eventName, payload);
            }
        } catch (const std::exception& e) {
            console::printf("[HTTP] DispatchHttpEventSafely(%s) failed: %s", eventName, e.what());
        } catch (...) {
            console::printf("[HTTP] DispatchHttpEventSafely(%s) failed: unknown exception", eventName);
        }
    }
    
    // User agent string
    constexpr const wchar_t* USER_AGENT = L"foo_ui_webview2/1.0 (WinHTTP)";
    
    // FIX-2: 响应体大小限制 (100MB)
    constexpr size_t MAX_RESPONSE_SIZE = 100ULL * 1024 * 1024;
    
    // FIX-4: 下载文件大小限制 (500MB)
    constexpr size_t MAX_DOWNLOAD_SIZE = 500ULL * 1024 * 1024;
    
    
    // 请求扩展选项
    struct RequestOptions {
        std::string redirect = "follow";    // follow | error | manual
        std::string responseType = "text";  // text | base64 | arraybuffer | binary
        bool insecureTls = false;           // 允许自签 / 无效证书 (需全局开关同时 ON)
        std::shared_ptr<std::atomic<bool>> cancelToken;
    };
    
    //==========================================================================
    // 异步请求管理器
    //==========================================================================
    class AsyncRequestManager {
    public:
        // GAP_702: 并发请求上限
        static constexpr int MAX_CONCURRENT_REQUESTS = 10;

        static AsyncRequestManager& GetInstance() {
            static AsyncRequestManager instance;
            return instance;
        }
        
        // 生成唯一请求ID (B-9: 随机化防止可预测性)
        std::string GenerateRequestId() {
            static std::mt19937 rng(std::random_device{}());
            static std::uniform_int_distribution<uint32_t> dist;
            char buf[32];
            snprintf(buf, sizeof(buf), "http_%08x%08llx", dist(rng), static_cast<unsigned long long>(++requestCounter_));
            return buf;
        }
        
        // 在后台线程执行 HTTP 请求并通过事件返回结果
        // callerHwnd / callerWindowId 在调用时捕获，用于路由到正确实例
        void ExecuteAsync(
            const std::string& requestId,
            const std::string& method,
            const std::string& url,
            const std::map<std::string, std::string>& headers,
            const std::string& body,
            int timeout,
            HWND callerHwnd = nullptr,
            const std::string& callerWindowId = "",
            RequestOptions options = {}
        ) {
            // GAP_702: 并发请求数检查
            if (activeRequests_.load() >= MAX_CONCURRENT_REQUESTS) {
                json errorResult = {
                    {"requestId", requestId},
                    {"success", false},
                    {"error", "Too many concurrent requests"},
                    {"code", ApiErrorCode::OPERATION_FAILED}
                };
                fb2k::inMainThread([errorResult, callerHwnd, callerWindowId]() noexcept {
                    DispatchHttpEventSafely(callerHwnd, callerWindowId, "http:response", errorResult);
                });
                return;
            }

            console::printf("[HTTP Async] Starting async request %s for: %s", requestId.c_str(), url.c_str());
            
            // 注册取消令牌
            auto cancelToken = RegisterCancelToken(requestId);
            options.cancelToken = cancelToken;
            RegisterWindowOwner(requestId, callerWindowId);
            
            // GAP_702: increment 在线程创建前（主线程）
            ++activeRequests_;
            
            std::thread([this, requestId, method, url, headers, body, timeout, callerHwnd, callerWindowId, options]() noexcept {
                // Outer noexcept guard: catches any exception that escapes the
                // inner try/catch handler (e.g. json copy in catch block,
                // FailureHook::LogAsync, fb2k::inMainThread copy capture) so
                // that std::thread does not call std::terminate.
                try {
                console::printf("[HTTP Async] Background thread started for %s", requestId.c_str());
                try {
                    json result = PerformHttpRequestInternal(method, url, headers, body, timeout, options);
                    result["requestId"] = requestId;

                    // B-8: 异步 HEAD 补充 contentLength
                    if (method == "HEAD" && result.value("success", false) && result.contains("headers")) {
                        auto& respHeaders = result["headers"];
                        if (respHeaders.contains("Content-Length")) {
                            try {
                                result["contentLength"] = std::stoll(respHeaders["Content-Length"].get<std::string>());
                            } catch (...) {}
                        }
                    }
                    
                    console::printf("[HTTP Async] Request %s completed, emitting event", requestId.c_str());
                    
                    // 通过事件发送结果，路由到调用者实例
                    fb2k::inMainThread([result, callerHwnd, callerWindowId]() noexcept {
                        DispatchHttpEventSafely(callerHwnd, callerWindowId, "http:response", result);
                    });
                } catch (const std::exception& e) {
                    console::printf("[HTTP Async] Request %s failed: %s", requestId.c_str(), e.what());
                    json errorResult = {
                        {"requestId", requestId},
                        {"success", false},
                        {"error", e.what()},
                        {"code", ApiErrorCode::OPERATION_FAILED}
                    };
                    FailureHook::LogAsync("http:response", ApiErrorCode::OPERATION_FAILED,
                                          e.what(), requestId.c_str());
                    fb2k::inMainThread([errorResult, callerHwnd, callerWindowId]() noexcept {
                        DispatchHttpEventSafely(callerHwnd, callerWindowId, "http:response", errorResult);
                    });
                }
                // 清理取消令牌
                UnregisterCancelToken(requestId);
                UnregisterWindowOwner(requestId);
                // GAP_702: decrement 在线程结束时
                --activeRequests_;
                } catch (...) {
                    console::printf("[HTTP Async] Request %s outer guard caught unknown exception", requestId.c_str());
                }
            }).detach();
            
            console::printf("[HTTP Async] Async request %s dispatched (returning immediately)", requestId.c_str());
        }
        
        // 同步执行 HTTP 请求 (用于内部调用或下载)
        static json PerformHttpRequestInternal(
            const std::string& method,
            const std::string& url,
            const std::map<std::string, std::string>& headers,
            const std::string& body,
            int timeout,
            const RequestOptions& options = {}
        );
        
        // 请求取消支持
        std::shared_ptr<std::atomic<bool>> RegisterCancelToken(const std::string& requestId) {
            auto token = std::make_shared<std::atomic<bool>>(false);
            std::lock_guard<std::mutex> lock(cancelMutex_);
            cancelTokens_[requestId] = token;
            return token;
        }
        
        void UnregisterCancelToken(const std::string& requestId) {
            std::lock_guard<std::mutex> lock(cancelMutex_);
            cancelTokens_.erase(requestId);
        }
        
        bool CancelRequest(const std::string& requestId) {
            std::lock_guard<std::mutex> lock(cancelMutex_);
            auto it = cancelTokens_.find(requestId);
            if (it != cancelTokens_.end()) {
                it->second->store(true);
                return true;
            }
            return false;
        }

        // 取消指定窗口的所有异步请求 (popup 关闭时调用)
        int CancelAllForWindow(const std::string& windowId) {
            if (windowId.empty()) return 0;
            std::lock_guard<std::mutex> lock(cancelMutex_);
            int count = 0;
            for (auto it = windowOwners_.begin(); it != windowOwners_.end(); ) {
                if (it->second == windowId) {
                    auto tokenIt = cancelTokens_.find(it->first);
                    if (tokenIt != cancelTokens_.end()) {
                        tokenIt->second->store(true);
                    }
                    it = windowOwners_.erase(it);
                    ++count;
                } else {
                    ++it;
                }
            }
            if (count > 0) {
                console::printf("[HTTP] Cancelled %d pending requests for window %s", count, windowId.c_str());
            }
            return count;
        }

        // 注册请求所属窗口
        void RegisterWindowOwner(const std::string& requestId, const std::string& windowId) {
            if (!windowId.empty()) {
                std::lock_guard<std::mutex> lock(cancelMutex_);
                windowOwners_[requestId] = windowId;
            }
        }

        void UnregisterWindowOwner(const std::string& requestId) {
            std::lock_guard<std::mutex> lock(cancelMutex_);
            windowOwners_.erase(requestId);
        }
        
        // GAP_701: download 异步路径用的活跃计数访问
        int GetActiveCount() const { return activeRequests_.load(); }
        void IncrementActive() { ++activeRequests_; }
        void DecrementActive() { --activeRequests_; }
        
    private:
        std::atomic<uint64_t> requestCounter_{0};
        std::atomic<int> activeRequests_{0};  // GAP_702: 活跃并发计数
        std::mutex cancelMutex_;
        std::unordered_map<std::string, std::shared_ptr<std::atomic<bool>>> cancelTokens_;
        std::unordered_map<std::string, std::string> windowOwners_;  // requestId -> windowId
    };
    
    //==========================================================================
    // 安全: SSRF 防护 - 检查是否为内网地址
    //==========================================================================
    bool IsLocalNetworkUrl(const std::string& url) {
        std::string lowerUrl = url;
        std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);
        
        // localhost
        if (lowerUrl.find("://localhost") != std::string::npos ||
            lowerUrl.find("://localhost:") != std::string::npos) {
            return true;
        }
        
        // 127.x.x.x
        if (lowerUrl.find("://127.") != std::string::npos) {
            return true;
        }
        
        // 10.x.x.x
        if (lowerUrl.find("://10.") != std::string::npos) {
            return true;
        }
        
        // 172.16-31.x.x
        std::regex r172(R"(://172\.(1[6-9]|2[0-9]|3[0-1])\.)");
        if (std::regex_search(lowerUrl, r172)) {
            return true;
        }
        
        // 192.168.x.x
        if (lowerUrl.find("://192.168.") != std::string::npos) {
            return true;
        }
        
        // 169.254.x.x (link-local)
        if (lowerUrl.find("://169.254.") != std::string::npos) {
            return true;
        }
        
        // ::1, [::1] (IPv6 localhost)
        if (lowerUrl.find("://[::1]") != std::string::npos ||
            lowerUrl.find("://::1") != std::string::npos) {
            return true;
        }
        
        // FIX-6: 0.0.0.0 (Windows 上等同 localhost)
        if (lowerUrl.find("://0.0.0.0") != std::string::npos) {
            return true;
        }
        
        // FIX-6: [::ffff:x.x.x.x] (IPv6 映射的 IPv4 地址)
        if (lowerUrl.find("://[::ffff:") != std::string::npos) {
            return true;
        }
        
        // FIX-6: fc00::/fd00:: (IPv6 私有地址)
        if (lowerUrl.find("://[fc") != std::string::npos ||
            lowerUrl.find("://[fd") != std::string::npos) {
            return true;
        }
        
        // FIX-6: [::] (IPv6 unspecified, 等同 0.0.0.0)
        if (lowerUrl.find("://[::]") != std::string::npos) {
            return true;
        }
        
        return false;
    }
    
    //==========================================================================
    // SSRF 增强: DNS 解析后验证 IP 不是私有地址 (防 DNS rebinding / 十进制绕过)
    //==========================================================================
    bool IsResolvedAddressPrivate(const std::wstring& host) {
        // Convert wide host to narrow for getaddrinfo
        int len = WideCharToMultiByte(CP_UTF8, 0, host.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string narrowHost(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, host.c_str(), -1, narrowHost.data(), len, nullptr, nullptr);
        
        ADDRINFOA hints = {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        
        ADDRINFOA* result = nullptr;
        if (getaddrinfo(narrowHost.c_str(), nullptr, &hints, &result) != 0 || !result) {
            return false; // Can't resolve — let WinHTTP handle the error
        }
        
        bool isPrivate = false;
        for (ADDRINFOA* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            if (ptr->ai_family == AF_INET) {
                auto* addr = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
                uint32_t ip = ntohl(addr->sin_addr.s_addr);
                // 127.0.0.0/8
                if ((ip >> 24) == 127) { isPrivate = true; break; }
                // 10.0.0.0/8
                if ((ip >> 24) == 10) { isPrivate = true; break; }
                // 172.16.0.0/12
                if ((ip & 0xFFF00000) == 0xAC100000) { isPrivate = true; break; }
                // 192.168.0.0/16
                if ((ip & 0xFFFF0000) == 0xC0A80000) { isPrivate = true; break; }
                // 169.254.0.0/16 (link-local)
                if ((ip & 0xFFFF0000) == 0xA9FE0000) { isPrivate = true; break; }
                // 0.0.0.0
                if (ip == 0) { isPrivate = true; break; }
            } else if (ptr->ai_family == AF_INET6) {
                auto* addr6 = reinterpret_cast<sockaddr_in6*>(ptr->ai_addr);
                const uint8_t* b = addr6->sin6_addr.s6_addr;
                // ::1
                static const uint8_t loopback[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
                if (memcmp(b, loopback, 16) == 0) { isPrivate = true; break; }
                // :: (unspecified)
                static const uint8_t unspec[16] = {};
                if (memcmp(b, unspec, 16) == 0) { isPrivate = true; break; }
                // fc00::/7 (ULA)
                if ((b[0] & 0xFE) == 0xFC) { isPrivate = true; break; }
                // fe80::/10 (link-local)
                if (b[0] == 0xFE && (b[1] & 0xC0) == 0x80) { isPrivate = true; break; }
                // ::ffff:x.x.x.x (IPv4-mapped) — check inner IPv4
                if (b[0]==0 && b[1]==0 && b[2]==0 && b[3]==0 &&
                    b[4]==0 && b[5]==0 && b[6]==0 && b[7]==0 &&
                    b[8]==0 && b[9]==0 && b[10]==0xFF && b[11]==0xFF) {
                    uint32_t ip4 = (b[12]<<24)|(b[13]<<16)|(b[14]<<8)|b[15];
                    if ((ip4>>24)==127 || (ip4>>24)==10 ||
                        (ip4 & 0xFFF00000)==0xAC100000 ||
                        (ip4 & 0xFFFF0000)==0xC0A80000 ||
                        (ip4 & 0xFFFF0000)==0xA9FE0000 || ip4==0) {
                        isPrivate = true; break;
                    }
                }
            }
        }
        
        freeaddrinfo(result);
        return isPrivate;
    }
    
    //==========================================================================
    // GAP_704: WinHTTP error diagnostic helper
    //==========================================================================
    std::string DescribeWinHttpError(DWORD err) {
        switch (err) {
        case ERROR_WINHTTP_SECURE_FAILURE:
            return "TLS/SSL certificate verification failed";
        case ERROR_WINHTTP_CANNOT_CONNECT:
            return "Cannot connect to server";
        case ERROR_WINHTTP_TIMEOUT:
            return "Connection timed out";
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:
            return "DNS name not resolved";
        case ERROR_WINHTTP_CONNECTION_ERROR:
            return "Connection error (reset or aborted)";
        case ERROR_WINHTTP_SECURE_CERT_DATE_INVALID:
            return "TLS certificate date is invalid";
        case ERROR_WINHTTP_SECURE_CERT_CN_INVALID:
            return "TLS certificate CN mismatch";
        case ERROR_WINHTTP_SECURE_INVALID_CA:
            return "TLS certificate authority is invalid or untrusted";
        case ERROR_WINHTTP_SECURE_CERT_REV_FAILED:
            return "TLS certificate revocation check failed";
        case ERROR_WINHTTP_SECURE_CERT_REVOKED:
            return "TLS certificate has been revoked";
        default:
            return "WinHTTP error (code " + std::to_string(err) + ")";
        }
    }

    //==========================================================================
    // Helper: Parse URL into components
    //==========================================================================
    struct UrlComponents {
        std::wstring scheme;
        std::wstring host;
        INTERNET_PORT port;
        std::wstring path;
        bool isHttps;
    };
    
    bool ParseUrl(const std::string& url, UrlComponents& components) {
        std::wstring wurl = Utf8ToWide(url);
        
        URL_COMPONENTS urlComp = {};
        urlComp.dwStructSize = sizeof(urlComp);
        
        wchar_t scheme[32], host[256], path[2048];
        urlComp.lpszScheme = scheme;
        urlComp.dwSchemeLength = static_cast<DWORD>(std::size(scheme));
        urlComp.lpszHostName = host;
        urlComp.dwHostNameLength = static_cast<DWORD>(std::size(host));
        urlComp.lpszUrlPath = path;
        urlComp.dwUrlPathLength = static_cast<DWORD>(std::size(path));
        
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp)) {
            return false;
        }
        
        components.scheme = scheme;
        components.host = host;
        components.port = urlComp.nPort;
        components.path = path;
        components.isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
        
        return true;
    }
    
    //==========================================================================
    // Helper: Perform HTTP request (同步内部实现)
    // 安全: 添加 SSRF 检查
    //==========================================================================
    json AsyncRequestManager::PerformHttpRequestInternal(
        const std::string& method,
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        const std::string& body,
        int timeout,
        const RequestOptions& options
    ) {
        // GAP_700: 最大重定向跟随次数
        static constexpr int MAX_REDIRECTS = 10;

        // Open session (可跨重定向复用, AUTOMATIC_PROXY 自动读取系统代理)
        HINTERNET hSession = WinHttpOpen(USER_AGENT,
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        
        if (!hSession) {
            return {{"success", false}, {"error", "Failed to open HTTP session"}};
        }
        
        // 启用 gzip/deflate 自动解压 (Win 8.1+)
        DWORD decompFlags = WINHTTP_DECOMPRESSION_FLAG_ALL;
        WinHttpSetOption(hSession, WINHTTP_OPTION_DECOMPRESSION, &decompFlags, sizeof(decompFlags));
        
        // Set timeouts
        if (timeout > 0) {
            WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);
        }

        // GAP_700: 手动重定向循环 — 每跳都校验目标地址防止 SSRF
        std::string currentUrl = url;
        std::string currentMethod = method;
        std::string currentBody = body;
        int redirectCount = 0;
        DWORD statusCode = 0;
        HINTERNET hConnect = nullptr;
        HINTERNET hRequest = nullptr;

        while (true) {
            // 安全: SSRF 防护 (每跳都检查)
            if (IsLocalNetworkUrl(currentUrl)) {
                if (!security_config::IsLocalNetworkAccessAllowed()) {
                    WinHttpCloseHandle(hSession);
                    return {
                        {"success", false},
                        {"error", redirectCount > 0
                            ? "Redirect target is a local network address"
                            : "Access to local network is disabled. Enable in Advanced Settings if needed."}
                    };
                }
            }

            UrlComponents urlComp;
            if (!ParseUrl(currentUrl, urlComp)) {
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Invalid URL"}};
            }

            // FIX-1: 协议白名单 — 只允许 http/https
            if (_wcsicmp(urlComp.scheme.c_str(), L"http") != 0 &&
                _wcsicmp(urlComp.scheme.c_str(), L"https") != 0) {
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Only http and https protocols are allowed"}};
            }

            // SSRF 增强: DNS 解析后验证目标 IP 不是私有地址
            if (!security_config::IsLocalNetworkAccessAllowed() &&
                IsResolvedAddressPrivate(urlComp.host)) {
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Target host resolves to a private/local network address"}};
            }

            // Connect
            hConnect = WinHttpConnect(hSession, urlComp.host.c_str(),
                urlComp.port, 0);

            if (!hConnect) {
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Failed to connect"}};
            }

            // Create request
            std::wstring wmethod = Utf8ToWide(currentMethod);
            DWORD flags = urlComp.isHttps ? WINHTTP_FLAG_SECURE : 0;

            hRequest = WinHttpOpenRequest(hConnect,
                wmethod.c_str(),
                urlComp.path.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                flags);

            if (!hRequest) {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Failed to create request"}};
            }

            // TLS 证书校验双层门禁：仅全局开关 ON 且请求显式 opt-in 时跳过。
            // 任一条件不满足都保持 WinHTTP 默认严格校验。
            if (urlComp.isHttps && options.insecureTls && security_config::IsInsecureTlsAllowed()) {
                DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA
                               | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
                               | SECURITY_FLAG_IGNORE_CERT_CN_INVALID
                               | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
                console::printf("[HTTP] TLS validation skipped for %s (user opt-in)", currentUrl.c_str());
            }

            // GAP_700: 始终禁用 WinHTTP 自动重定向，由手动循环处理
            DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

            // Add headers (仅首次请求; 重定向时不重传自定义 headers 防止 Authorization 泄露)
            if (redirectCount == 0) {
                for (const auto& [key, value] : headers) {
                    // S-5: 过滤 CRLF 注入
                    if (key.find('\r') != std::string::npos || key.find('\n') != std::string::npos ||
                        value.find('\r') != std::string::npos || value.find('\n') != std::string::npos) {
                        continue;
                    }
                    std::wstring header = Utf8ToWide(key) + L": " + Utf8ToWide(value);
                    WinHttpAddRequestHeaders(hRequest, header.c_str(),
                        static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
                }
            }

            // Send request (307/308 保持原 body; 其他重定向清空 body)
            BOOL bResult = WinHttpSendRequest(hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                currentBody.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(currentBody.data()),
                static_cast<DWORD>(currentBody.size()),
                static_cast<DWORD>(currentBody.size()), 0);

            if (!bResult) {
                DWORD err = GetLastError();
                std::string errMsg = "Send failed: " + DescribeWinHttpError(err);
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", errMsg}};
            }

            // Receive response
            bResult = WinHttpReceiveResponse(hRequest, nullptr);

            if (!bResult) {
                DWORD err = GetLastError();
                std::string errMsg = "Receive failed: " + DescribeWinHttpError(err);
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", errMsg}};
            }

            // Get status code
            statusCode = 0;
            DWORD scSize = sizeof(statusCode);
            (void)WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &scSize, WINHTTP_NO_HEADER_INDEX);

            // GAP_700: 重定向处理
            bool isRedirect = (statusCode == 301 || statusCode == 302 || statusCode == 303 ||
                               statusCode == 307 || statusCode == 308);

            if (isRedirect && options.redirect == "follow") {
                if (++redirectCount > MAX_REDIRECTS) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return {{"success", false}, {"error", "Too many redirects (limit: 10)"}};
                }

                // 提取 Location header
                DWORD locSize = 0;
                (void)WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION,
                    WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &locSize, WINHTTP_NO_HEADER_INDEX);

                if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || locSize == 0) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return {{"success", false}, {"error", "Redirect without Location header"}};
                }

                std::vector<wchar_t> locBuf(locSize / sizeof(wchar_t) + 1);
                if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION,
                    WINHTTP_HEADER_NAME_BY_INDEX, locBuf.data(), &locSize, WINHTTP_NO_HEADER_INDEX)) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return {{"success", false}, {"error", "Failed to read Location header"}};
                }

                currentUrl = WideToUtf8(std::wstring(locBuf.data()));

                // 301/302/303 → method 改为 GET, body 清空 (符合浏览器行为)
                if (statusCode == 301 || statusCode == 302 || statusCode == 303) {
                    currentMethod = "GET";
                    currentBody.clear();
                }
                // 307/308 → 保持原 method 和 body

                WinHttpCloseHandle(hRequest);
                hRequest = nullptr;
                WinHttpCloseHandle(hConnect);
                hConnect = nullptr;
                continue;  // 重新循环，对新 URL 执行 SSRF 检查
            }

            if (isRedirect && options.redirect == "error") {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Redirect not allowed"}, {"status", statusCode}};
            }

            // 非重定向 或 redirect=="manual" — 退出循环，正常读取响应
            break;
        }

        // Get response headers
        json responseHeaders = json::object();
        
        // Get all headers
        DWORD size = 0;
        (void)WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size, WINHTTP_NO_HEADER_INDEX);
        
        if (size > 0) {
            std::vector<wchar_t> headerBuffer(size / sizeof(wchar_t) + 1);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                WINHTTP_HEADER_NAME_BY_INDEX, headerBuffer.data(), &size, WINHTTP_NO_HEADER_INDEX)) {
                
                std::wstring allHeaders(headerBuffer.data());
                std::wistringstream iss(allHeaders);
                std::wstring line;
                while (std::getline(iss, line)) {
                    size_t colonPos = line.find(L':');
                    if (colonPos != std::wstring::npos) {
                        std::string key = WideToUtf8(line.substr(0, colonPos));
                        std::string value = WideToUtf8(line.substr(colonPos + 1));
                        // Trim whitespace
                        while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
                            value = value.substr(1);
                        while (!value.empty() && (value.back() == '\r' || value.back() == '\n'))
                            value.pop_back();
                        responseHeaders[key] = value;
                    }
                }
            }
        }
        
        // Read body
        std::string responseBody;
        DWORD bytesAvailable = 0;
        
        do {
            // 检查取消请求
            if (options.cancelToken && options.cancelToken->load()) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Request cancelled"}, {"cancelled", true}};
            }
            
            // FIX-2: 检查响应体大小限制
            if (responseBody.size() > MAX_RESPONSE_SIZE) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Response too large (exceeds 100MB limit)"},
                        {"code", ApiErrorCode::OPERATION_FAILED}};
            }
            
            bytesAvailable = 0;
            WinHttpQueryDataAvailable(hRequest, &bytesAvailable);
            
            if (bytesAvailable > 0) {
                std::vector<char> buffer(bytesAvailable);
                DWORD bytesRead = 0;
                if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
                    responseBody.append(buffer.data(), bytesRead);
                }
            }
        } while (bytesAvailable > 0);
        
        // Cleanup
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        
        // 响应类型处理
        // arraybuffer/binary 视为 base64 别名 — 二进制响应统一以 base64 字符串传输，
        // 避免非 UTF-8 字节让 nlohmann::json 序列化失败。
        const bool wantBase64 = (options.responseType == "base64" ||
                                 options.responseType == "arraybuffer" ||
                                 options.responseType == "binary");
        json bodyValue;
        std::string actualResponseType;
        if (wantBase64) {
            bodyValue = utils::Base64Encode(reinterpret_cast<const uint8_t*>(responseBody.data()), responseBody.size());
            actualResponseType = "base64";
        } else {
            bodyValue = responseBody;
            actualResponseType = "text";
        }
        
        return {
            {"success", true},
            {"status", statusCode},
            {"headers", responseHeaders},
            {"body", bodyValue},
            {"responseType", actualResponseType}
        };
    }
    
    // FIX-5: body 为 object/array 时自动序列化
    static std::string ExtractBody(const json& params) {
        if (!params.contains("body")) return "";
        const auto& b = params["body"];
        if (b.is_string()) return b.get<std::string>();
        if (b.is_object() || b.is_array()) return b.dump();
        return "";
    }
    
    //==========================================================================
    // http.get - HTTP GET request (异步版本)
    // 返回 { requestId: "xxx" }，结果通过 http:response 事件返回
    //==========================================================================
    json HttpGet(const json& params) {
        std::string url = params.value("url", "");
        int timeout = params.value("timeout", 30000);
        bool async = params.value("async", true);
        std::string redirect = params.value("redirect", "follow");
        std::string responseType = params.value("responseType", "text");
        bool insecureTls = params.value("insecureTls", false);
        
        console::printf("[HTTP] http.get called, async=%s, url=%s", async ? "true" : "false", url.c_str());
        
        if (url.empty()) {
            return {{"success", false}, {"error", "url is required"}};
        }
        
        // Parse headers
        std::map<std::string, std::string> headers;
        if (params.contains("headers") && params["headers"].is_object()) {
            for (auto& [key, value] : params["headers"].items()) {
                headers[key] = value.get<std::string>();
            }
        }
        
        RequestOptions options;
        options.redirect = redirect;
        options.responseType = responseType;
        options.insecureTls = insecureTls;
        
        if (async) {
            console::print("[HTTP] Using ASYNC mode - returning immediately");
            auto caller = CallerContext::FromParams(params);
            auto& manager = AsyncRequestManager::GetInstance();
            std::string requestId = manager.GenerateRequestId();
            manager.ExecuteAsync(requestId, "GET", url, headers, "", timeout,
                                 caller.callerHwnd, caller.windowId, options);
            return {{"success", true}, {"requestId", requestId}, {"async", true}};
        } else {
            console::print("[HTTP] Using SYNC mode - will block until complete");
            return AsyncRequestManager::PerformHttpRequestInternal("GET", url, headers, "", timeout, options);
        }
    }
    
    //==========================================================================
    // http.post - HTTP POST request (异步版本)
    //==========================================================================
    json HttpPost(const json& params) {
        std::string url = params.value("url", "");
        std::string body = ExtractBody(params);  // FIX-5: 支持 object/array 自动序列化
        int timeout = params.value("timeout", 30000);
        bool async = params.value("async", true);
        std::string redirect = params.value("redirect", "follow");
        std::string responseType = params.value("responseType", "text");
        bool insecureTls = params.value("insecureTls", false);
        
        if (url.empty()) {
            return {{"success", false}, {"error", "url is required"}};
        }
        
        std::map<std::string, std::string> headers;
        if (params.contains("headers") && params["headers"].is_object()) {
            for (auto& [key, value] : params["headers"].items()) {
                headers[key] = value.get<std::string>();
            }
        }
        
        RequestOptions options;
        options.redirect = redirect;
        options.responseType = responseType;
        options.insecureTls = insecureTls;
        
        if (async) {
            auto caller = CallerContext::FromParams(params);
            auto& manager = AsyncRequestManager::GetInstance();
            std::string requestId = manager.GenerateRequestId();
            manager.ExecuteAsync(requestId, "POST", url, headers, body, timeout,
                                 caller.callerHwnd, caller.windowId, options);
            return {{"success", true}, {"requestId", requestId}, {"async", true}};
        } else {
            return AsyncRequestManager::PerformHttpRequestInternal("POST", url, headers, body, timeout, options);
        }
    }
    
    //==========================================================================
    // http.head - HTTP HEAD request (异步版本)
    //==========================================================================
    json HttpHead(const json& params) {
        std::string url = params.value("url", "");
        int timeout = params.value("timeout", 30000);
        bool async = params.value("async", true);
        std::string redirect = params.value("redirect", "follow");
        bool insecureTls = params.value("insecureTls", false);
        
        if (url.empty()) {
            return {{"success", false}, {"error", "url is required"}};
        }
        
        std::map<std::string, std::string> headers;
        if (params.contains("headers") && params["headers"].is_object()) {
            for (auto& [key, value] : params["headers"].items()) {
                headers[key] = value.get<std::string>();
            }
        }
        
        RequestOptions options;
        options.redirect = redirect;
        options.insecureTls = insecureTls;
        
        if (async) {
            auto caller = CallerContext::FromParams(params);
            auto& manager = AsyncRequestManager::GetInstance();
            std::string requestId = manager.GenerateRequestId();
            manager.ExecuteAsync(requestId, "HEAD", url, headers, "", timeout,
                                 caller.callerHwnd, caller.windowId, options);
            return {{"success", true}, {"requestId", requestId}, {"async", true}};
        } else {
            json result = AsyncRequestManager::PerformHttpRequestInternal("HEAD", url, headers, "", timeout, options);
            
            // For HEAD request, also extract content-length if available
            if (result.value("success", false) && result.contains("headers")) {
                auto& respHeaders = result["headers"];
                if (respHeaders.contains("Content-Length")) {
                    try {
                        result["contentLength"] = std::stoll(respHeaders["Content-Length"].get<std::string>());
                    } catch (...) {}
                }
            }
            return result;
        }
    }
    
    //==========================================================================
    // http.download 内部实现 (GAP_701: 支持取消 + 每跳 SSRF 校验)
    //==========================================================================
    static json HttpDownloadInternal(
        const std::string& url,
        const std::wstring& wsaveTo,
        int timeout,
        const std::string& redirect,
        const std::map<std::string, std::string>& headers,
        const std::shared_ptr<std::atomic<bool>>& cancelToken,
        bool insecureTls = false
    ) {
        // GAP_701: 最大重定向跟随次数 (与 PerformHttpRequestInternal 一致)
        static constexpr int MAX_REDIRECTS = 10;

        // 打开 WinHTTP 会话 (可跨重定向复用, AUTOMATIC_PROXY 自动读取系统代理)
        HINTERNET hSession = WinHttpOpen(USER_AGENT,
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        
        if (!hSession) {
            return {{"success", false}, {"error", "Failed to open HTTP session"}};
        }
        
        // 启用 gzip/deflate 自动解压
        DWORD decompFlags = WINHTTP_DECOMPRESSION_FLAG_ALL;
        WinHttpSetOption(hSession, WINHTTP_OPTION_DECOMPRESSION, &decompFlags, sizeof(decompFlags));
        
        if (timeout > 0) {
            WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);
        }

        // GAP_701: 手动重定向循环 — 每跳都校验目标地址防止 SSRF
        std::string currentUrl = url;
        int redirectCount = 0;
        DWORD statusCode = 0;
        HINTERNET hConnect = nullptr;
        HINTERNET hRequest = nullptr;

        while (true) {
            // GAP_701: 取消检查
            if (cancelToken && cancelToken->load()) {
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Download cancelled"}, {"cancelled", true}};
            }

            // SSRF 防护 (每跳都检查)
            if (IsLocalNetworkUrl(currentUrl)) {
                if (!security_config::IsLocalNetworkAccessAllowed()) {
                    WinHttpCloseHandle(hSession);
                    return {
                        {"success", false},
                        {"error", redirectCount > 0
                            ? "Redirect target is a local network address"
                            : "Access to local network is disabled. Enable in Advanced Settings if needed."}
                    };
                }
            }

            UrlComponents urlComp;
            if (!ParseUrl(currentUrl, urlComp)) {
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Invalid URL"}};
            }

            // FIX-1: 协议白名单
            if (_wcsicmp(urlComp.scheme.c_str(), L"http") != 0 &&
                _wcsicmp(urlComp.scheme.c_str(), L"https") != 0) {
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Only http and https protocols are allowed"}};
            }

            // SSRF 增强: DNS 解析后验证目标 IP 不是私有地址
            if (!security_config::IsLocalNetworkAccessAllowed() &&
                IsResolvedAddressPrivate(urlComp.host)) {
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Target host resolves to a private/local network address"}};
            }

            // 连接
            hConnect = WinHttpConnect(hSession, urlComp.host.c_str(),
                urlComp.port, 0);

            if (!hConnect) {
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Failed to connect"}};
            }

            // 创建请求 (download 始终 GET)
            DWORD flags = urlComp.isHttps ? WINHTTP_FLAG_SECURE : 0;
            hRequest = WinHttpOpenRequest(hConnect,
                L"GET",
                urlComp.path.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                flags);

            if (!hRequest) {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Failed to create request"}};
            }

            // TLS 证书校验双层门禁：仅全局开关 ON 且调用者 opt-in 时跳过。
            if (urlComp.isHttps && insecureTls && security_config::IsInsecureTlsAllowed()) {
                DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA
                               | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID
                               | SECURITY_FLAG_IGNORE_CERT_CN_INVALID
                               | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
                console::printf("[HTTP Download] TLS validation skipped for %s (user opt-in)", currentUrl.c_str());
            }

            // GAP_701: 始终禁用 WinHTTP 自动重定向，由手动循环处理
            DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

            // 添加请求头 (仅首次请求; 重定向时不重传自定义 headers 防止 Authorization 泄露)
            if (redirectCount == 0) {
                for (const auto& [key, value] : headers) {
                    // S-5: 过滤 CRLF 注入
                    if (key.find('\r') != std::string::npos || key.find('\n') != std::string::npos ||
                        value.find('\r') != std::string::npos || value.find('\n') != std::string::npos) {
                        continue;
                    }
                    std::wstring header = Utf8ToWide(key) + L": " + Utf8ToWide(value);
                    WinHttpAddRequestHeaders(hRequest, header.c_str(),
                        static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
                }
            }

            // 发送请求
            BOOL bResult = WinHttpSendRequest(hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

            if (!bResult) {
                DWORD err = GetLastError();
                std::string errMsg = "Send failed: " + DescribeWinHttpError(err);
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", errMsg}};
            }

            // 接收响应
            bResult = WinHttpReceiveResponse(hRequest, nullptr);
            if (!bResult) {
                DWORD err = GetLastError();
                std::string errMsg = "Receive failed: " + DescribeWinHttpError(err);
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", errMsg}};
            }

            // 获取状态码
            statusCode = 0;
            DWORD scSize = sizeof(statusCode);
            (void)WinHttpQueryHeaders(hRequest,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &scSize, WINHTTP_NO_HEADER_INDEX);

            // GAP_701: 重定向处理 (与 PerformHttpRequestInternal 保持一致)
            bool isRedirect = (statusCode == 301 || statusCode == 302 || statusCode == 303 ||
                               statusCode == 307 || statusCode == 308);

            if (isRedirect && redirect == "follow") {
                if (++redirectCount > MAX_REDIRECTS) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return {{"success", false}, {"error", "Too many redirects (limit: 10)"}};
                }

                // 提取 Location header
                DWORD locSize = 0;
                (void)WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION,
                    WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &locSize, WINHTTP_NO_HEADER_INDEX);

                if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || locSize == 0) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return {{"success", false}, {"error", "Redirect without Location header"}};
                }

                std::vector<wchar_t> locBuf(locSize / sizeof(wchar_t) + 1);
                if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION,
                    WINHTTP_HEADER_NAME_BY_INDEX, locBuf.data(), &locSize, WINHTTP_NO_HEADER_INDEX)) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    return {{"success", false}, {"error", "Failed to read Location header"}};
                }

                currentUrl = WideToUtf8(std::wstring(locBuf.data()));

                // download 始终 GET，无需 method 变更
                WinHttpCloseHandle(hRequest);
                hRequest = nullptr;
                WinHttpCloseHandle(hConnect);
                hConnect = nullptr;
                continue;  // 重新循环，对新 URL 执行 SSRF 检查
            }

            if (isRedirect && redirect == "error") {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Redirect not allowed"}, {"status", statusCode}};
            }

            // 非重定向 或 redirect=="manual" — 退出循环
            break;
        }

        // 打开输出文件
        std::ofstream file(wsaveTo, std::ios::binary);
        if (!file.is_open()) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return {{"success", false}, {"error", "Failed to create output file"}};
        }
        
        // FIX-4: 流式读取并直接写入文件，不在内存中积累完整 body
        size_t totalBytesWritten = 0;
        DWORD bytesAvailable = 0;
        
        do {
            // GAP_701: 取消检查 (每次读取循环)
            if (cancelToken && cancelToken->load()) {
                file.close();
                try { fs::remove(wsaveTo); } catch (...) {}
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return {{"success", false}, {"error", "Download cancelled"}, {"cancelled", true}};
            }

            bytesAvailable = 0;
            WinHttpQueryDataAvailable(hRequest, &bytesAvailable);
            
            if (bytesAvailable > 0) {
                std::vector<char> buffer(bytesAvailable);
                DWORD bytesRead = 0;
                if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
                    file.write(buffer.data(), bytesRead);
                    totalBytesWritten += bytesRead;
                    
                    // 检查下载大小限制
                    if (totalBytesWritten > MAX_DOWNLOAD_SIZE) {
                        file.close();
                        try { fs::remove(wsaveTo); } catch (...) {}
                        WinHttpCloseHandle(hRequest);
                        WinHttpCloseHandle(hConnect);
                        WinHttpCloseHandle(hSession);
                        return {{"success", false},
                                {"error", "Download too large (exceeds 500MB limit)"},
                                {"code", ApiErrorCode::OPERATION_FAILED}};
                    }
                }
            }
        } while (bytesAvailable > 0);
        
        file.close();
        
        // 清理 WinHTTP 句柄
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        
        return {
            {"success", true},
            {"status", statusCode},
            {"bytesWritten", totalBytesWritten},
            {"path", WideToUtf8(wsaveTo)}
        };
    }

    //==========================================================================
    // http.download - Download file (GAP_701: 异步取消 + 每跳 SSRF 校验)
    //==========================================================================
    json HttpDownload(const json& params) {
        std::string url = params.value("url", "");
        std::string saveTo = params.value("saveTo", "");
        int timeout = params.value("timeout", 60000);
        std::string redirect = params.value("redirect", "follow");
        bool async = params.value("async", false);  // GAP_701: download 默认同步
        bool insecureTls = params.value("insecureTls", false);
        
        if (url.empty()) {
            return {{"success", false}, {"error", "url is required"}};
        }
        
        if (saveTo.empty()) {
            return {{"success", false}, {"error", "saveTo is required"}};
        }
        
        // SSRF 防护 (初始 URL 快速检查)
        if (IsLocalNetworkUrl(url)) {
            if (!security_config::IsLocalNetworkAccessAllowed()) {
                return {
                    {"success", false},
                    {"error", "Access to local network is disabled. Enable in Advanced Settings if needed."}
                };
            }
        }
        
        // SSRF 增强: DNS 解析后验证
        if (!security_config::IsLocalNetworkAccessAllowed()) {
            UrlComponents preCheck;
            if (ParseUrl(url, preCheck) && IsResolvedAddressPrivate(preCheck.host)) {
                return {{"success", false}, {"error", "Target host resolves to a private/local network address"}};
            }
        }
        
        // FIX-3: 从 params 解析 headers
        std::map<std::string, std::string> headers;
        if (params.contains("headers") && params["headers"].is_object()) {
            for (auto& [key, value] : params["headers"].items()) {
                headers[key] = value.get<std::string>();
            }
        }
        
        // 展开路径变量 — 使用共享 PathExpansion 模块，与框架层装饰器一致
        std::wstring wsaveTo = PathExpansion::Expand(saveTo);
        
        // 确保父目录存在
        try {
            fs::path filePath(wsaveTo);
            fs::path parentDir = filePath.parent_path();
            if (!parentDir.empty() && !fs::exists(parentDir)) {
                fs::create_directories(parentDir);
            }
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", std::string("Failed to create directory: ") + e.what()}};
        }
        
        if (async) {
            // GAP_701: 异步下载模式
            auto caller = CallerContext::FromParams(params);
            auto& mgr = AsyncRequestManager::GetInstance();
            
            // GAP_702: 并发请求数检查
            if (mgr.GetActiveCount() >= AsyncRequestManager::MAX_CONCURRENT_REQUESTS) {
                return {
                    {"success", false},
                    {"error", "Too many concurrent requests"},
                    {"code", ApiErrorCode::OPERATION_FAILED}
                };
            }
            
            std::string requestId = mgr.GenerateRequestId();
            auto cancelToken = mgr.RegisterCancelToken(requestId);
            mgr.RegisterWindowOwner(requestId, caller.windowId);
            mgr.IncrementActive();
            
            HWND callerHwnd = caller.callerHwnd;
            std::string callerWindowId = caller.windowId;
            
            console::printf("[HTTP Download] Starting async download %s for: %s", requestId.c_str(), url.c_str());
            
            std::thread([url, wsaveTo, timeout, redirect, headers, cancelToken, insecureTls,
                         requestId, callerHwnd, callerWindowId]() noexcept {
                // Outer noexcept guard (same rationale as HttpRequestAsync above).
                try {
                json result;
                try {
                    result = HttpDownloadInternal(url, wsaveTo, timeout, redirect, headers, cancelToken, insecureTls);
                    result["requestId"] = requestId;
                } catch (const std::exception& e) {
                    result = {
                        {"requestId", requestId},
                        {"success", false},
                        {"error", e.what()},
                        {"code", ApiErrorCode::OPERATION_FAILED}
                    };
                }
                
                fb2k::inMainThread([result, callerHwnd, callerWindowId]() noexcept {
                    DispatchHttpEventSafely(callerHwnd, callerWindowId, "http:downloadComplete", result);
                });
                
                AsyncRequestManager::GetInstance().UnregisterCancelToken(requestId);
                AsyncRequestManager::GetInstance().UnregisterWindowOwner(requestId);
                AsyncRequestManager::GetInstance().DecrementActive();
                } catch (...) {
                    console::printf("[HTTP Download] Request %s outer guard caught unknown exception", requestId.c_str());
                }
            }).detach();
            
            return {{"success", true}, {"requestId", requestId}, {"async", true}, {"message", "Download started"}};
        } else {
            // 同步下载
            return HttpDownloadInternal(url, wsaveTo, timeout, redirect, headers, nullptr, insecureTls);
        }
    }
    
    //==========================================================================
    // http.put - HTTP PUT request
    //==========================================================================
    json HttpPut(const json& params) {
        std::string url = params.value("url", "");
        std::string body = ExtractBody(params);  // FIX-5: 支持 object/array 自动序列化
        int timeout = params.value("timeout", 30000);
        bool async = params.value("async", true);
        std::string redirect = params.value("redirect", "follow");
        std::string responseType = params.value("responseType", "text");
        bool insecureTls = params.value("insecureTls", false);
        
        if (url.empty()) {
            return {{"success", false}, {"error", "url is required"}};
        }
        
        std::map<std::string, std::string> headers;
        if (params.contains("headers") && params["headers"].is_object()) {
            for (auto& [key, value] : params["headers"].items()) {
                headers[key] = value.get<std::string>();
            }
        }
        
        RequestOptions options;
        options.redirect = redirect;
        options.responseType = responseType;
        options.insecureTls = insecureTls;
        
        if (async) {
            auto caller = CallerContext::FromParams(params);
            auto& manager = AsyncRequestManager::GetInstance();
            std::string requestId = manager.GenerateRequestId();
            manager.ExecuteAsync(requestId, "PUT", url, headers, body, timeout,
                                 caller.callerHwnd, caller.windowId, options);
            return {{"success", true}, {"requestId", requestId}, {"async", true}};
        } else {
            return AsyncRequestManager::PerformHttpRequestInternal("PUT", url, headers, body, timeout, options);
        }
    }
    
    //==========================================================================
    // http.delete - HTTP DELETE request
    //==========================================================================
    json HttpDelete(const json& params) {
        std::string url = params.value("url", "");
        std::string body = ExtractBody(params);  // FIX-5: 支持 object/array 自动序列化
        int timeout = params.value("timeout", 30000);
        bool async = params.value("async", true);
        std::string redirect = params.value("redirect", "follow");
        std::string responseType = params.value("responseType", "text");
        bool insecureTls = params.value("insecureTls", false);
        
        if (url.empty()) {
            return {{"success", false}, {"error", "url is required"}};
        }
        
        std::map<std::string, std::string> headers;
        if (params.contains("headers") && params["headers"].is_object()) {
            for (auto& [key, value] : params["headers"].items()) {
                headers[key] = value.get<std::string>();
            }
        }
        
        RequestOptions options;
        options.redirect = redirect;
        options.responseType = responseType;
        options.insecureTls = insecureTls;
        
        if (async) {
            auto caller = CallerContext::FromParams(params);
            auto& manager = AsyncRequestManager::GetInstance();
            std::string requestId = manager.GenerateRequestId();
            manager.ExecuteAsync(requestId, "DELETE", url, headers, body, timeout,
                                 caller.callerHwnd, caller.windowId, options);
            return {{"success", true}, {"requestId", requestId}, {"async", true}};
        } else {
            return AsyncRequestManager::PerformHttpRequestInternal("DELETE", url, headers, body, timeout, options);
        }
    }
    
    //==========================================================================
    // http.patch - HTTP PATCH request
    //==========================================================================
    json HttpPatch(const json& params) {
        std::string url = params.value("url", "");
        std::string body = ExtractBody(params);  // FIX-5: 支持 object/array 自动序列化
        int timeout = params.value("timeout", 30000);
        bool async = params.value("async", true);
        std::string redirect = params.value("redirect", "follow");
        std::string responseType = params.value("responseType", "text");
        bool insecureTls = params.value("insecureTls", false);
        
        if (url.empty()) {
            return {{"success", false}, {"error", "url is required"}};
        }
        
        std::map<std::string, std::string> headers;
        if (params.contains("headers") && params["headers"].is_object()) {
            for (auto& [key, value] : params["headers"].items()) {
                headers[key] = value.get<std::string>();
            }
        }
        
        RequestOptions options;
        options.redirect = redirect;
        options.responseType = responseType;
        options.insecureTls = insecureTls;
        
        if (async) {
            auto caller = CallerContext::FromParams(params);
            auto& manager = AsyncRequestManager::GetInstance();
            std::string requestId = manager.GenerateRequestId();
            manager.ExecuteAsync(requestId, "PATCH", url, headers, body, timeout,
                                 caller.callerHwnd, caller.windowId, options);
            return {{"success", true}, {"requestId", requestId}, {"async", true}};
        } else {
            return AsyncRequestManager::PerformHttpRequestInternal("PATCH", url, headers, body, timeout, options);
        }
    }
    
    //==========================================================================
    // http.abort - Cancel an async HTTP request
    //==========================================================================
    json HttpAbort(const json& params) {
        std::string requestId = params.value("requestId", "");
        
        if (requestId.empty()) {
            return {{"success", false}, {"error", "requestId is required"}};
        }
        
        auto& manager = AsyncRequestManager::GetInstance();
        bool cancelled = manager.CancelRequest(requestId);
        
        return {
            {"success", true},
            {"requestId", requestId},
            {"cancelled", cancelled}
        };
    }
    
} // anonymous namespace

//==========================================================================
// Cancel all HTTP requests for a window (called on popup close)
//==========================================================================
void CancelAllHttpRequestsForWindow(const std::string& windowId) {
    AsyncRequestManager::GetInstance().CancelAllForWindow(windowId);
}

//==========================================================================
// Register HTTP API
//==========================================================================
void RegisterHttpApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // http.get - HTTP GET request
    bridge.RegisterApi("http.get", HttpGet);
    
    // http.post - HTTP POST request
    bridge.RegisterApi("http.post", HttpPost);
    
    // http.head - HTTP HEAD request
    bridge.RegisterApi("http.head", HttpHead);
    
    // http.download - Download file
    bridge.RegisterApi("http.download", HttpDownload, {{"saveTo", SecurityLevel::Write}});
    
    // http.put - HTTP PUT request
    bridge.RegisterApi("http.put", HttpPut);
    
    // http.delete - HTTP DELETE request
    bridge.RegisterApi("http.delete", HttpDelete);
    
    // http.patch - HTTP PATCH request
    bridge.RegisterApi("http.patch", HttpPatch);
    
    // http.abort - Cancel async request
    bridge.RegisterApi("http.abort", HttpAbort);
    
    LOG("HTTP API registered (8 APIs)");
}
