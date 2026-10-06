#include "pch.h"
#include "webview/WebViewHost.h"
#include "webview/OriginPolicy.h"
#include "core/WebViewContext.h"
#include "media/MediaFile.h"
#include "media/MediaHttp.h"
#include "media/MediaRange.h"
#include "media/MediaService.h"
#include <wrl/event.h>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

// 请求参数和 deferral 均留在 WebView 所属线程。worker 只携带 weak_ptr，回主线程后才访问表。
struct MediaRouteState {
    struct Pending {
        ComPtr<ICoreWebView2WebResourceRequestedEventArgs> args;
        ComPtr<ICoreWebView2Deferral> deferral;
        std::wstring origin;
    };
    std::uint64_t nextId = 0;
    std::unordered_map<std::uint64_t, Pending> pending;
};

namespace {
media::DocumentOwner Owner(const WebViewHost::DocumentStamp& stamp) {
    return {stamp.hostSerial, stamp.navigationGeneration, stamp.hostGeneration};
}
std::wstring Header(ICoreWebView2WebResourceRequest* request, const wchar_t* name) {
    ComPtr<ICoreWebView2HttpRequestHeaders> headers;
    wil::unique_cotaskmem_string value;
    if (FAILED(request->get_Headers(&headers)) || !headers || FAILED(headers->GetHeader(name, &value)) || !value) return {};
    return value.get();
}
const wchar_t* Reason(int status) {
    switch (status) {
    case 200: return L"OK";
    case 204: return L"No Content";
    case 206: return L"Partial Content";
    case 403: return L"Forbidden";
    case 405: return L"Method Not Allowed";
    case 410: return L"Gone";
    case 416: return L"Range Not Satisfiable";
    case 503: return L"Service Unavailable";
    default: return L"Internal Server Error";
    }
}
void Respond(ICoreWebView2Environment* environment, ICoreWebView2WebResourceRequestedEventArgs* args,
             int status, const std::wstring& origin, const media::MediaToken* token = nullptr,
             const media::ByteRange* range = nullptr, const std::vector<std::uint8_t>& body = {}, bool head = false) {
    if (!environment || !args) return;
    std::wstring headers = L"Cache-Control: no-store\r\nVary: Origin\r\nX-Content-Type-Options: nosniff\r\n";
    // 已获准的来源也必须能读取错误状态，否则浏览器只会暴露笼统的 CORS 失败。
    if (!origin.empty()) {
        headers += L"Access-Control-Allow-Origin: " + origin + L"\r\n";
        headers += L"Access-Control-Expose-Headers: Content-Range, Accept-Ranges, Content-Length, ETag\r\n";
    }
    if (status == 204) headers += L"Access-Control-Allow-Methods: GET, HEAD, OPTIONS\r\nAccess-Control-Allow-Headers: Range\r\n";
    if (status == 405) headers += L"Allow: GET, HEAD, OPTIONS\r\n";
    if (token) {
        headers += L"ETag: \"" + std::to_wstring(token->identity.mtime) + L"-" + std::to_wstring(token->identity.size) + L"\"\r\n";
        headers += L"Accept-Ranges: bytes\r\nContent-Type: " + media::Wide(token->mimeType) + L"\r\n";
        if (status == 416) headers += L"Content-Range: bytes */" + std::to_wstring(token->identity.size) + L"\r\n";
        if (status == 206 && range) headers += L"Content-Range: bytes " + std::to_wstring(range->offset) + L"-" +
            std::to_wstring(range->offset + range->length - 1) + L"/" + std::to_wstring(token->identity.size) + L"\r\n";
    }
    const auto length = head && token && status == 200 ? token->identity.size : body.size();
    headers += L"Content-Length: " + std::to_wstring(length) + L"\r\n";
    ComPtr<IStream> stream;
    if (!body.empty()) {
        stream.Attach(SHCreateMemStream(body.data(), static_cast<UINT>(body.size())));
        if (!stream) {
            Respond(environment, args, 503, origin);
            return;
        }
    }
    ComPtr<ICoreWebView2WebResourceResponse> response;
    if (SUCCEEDED(environment->CreateWebResourceResponse(stream.Get(), status, Reason(status), headers.c_str(), &response)))
        args->put_Response(response.Get());
}
struct ReadResult {
    int status = 500;
    std::wstring finalPath;
    std::vector<std::uint8_t> bytes;
};
ReadResult ReadSegment(const media::MediaToken& token, const media::ByteRange& range, bool head) {
    ReadResult result;
    try {
        media::MediaFile file(token.path);
        result.finalPath = file.FinalPath();
        if (file.Identity() != token.identity ||
            CompareStringOrdinal(result.finalPath.c_str(), -1, token.path.c_str(), -1, TRUE) != CSTR_EQUAL) {
            result.status = 410;
            return result;
        }
        // HEAD 和非法 Range 也要核对当前文件；文件已变化时先返回 410，不能沿用旧元数据。
        if (!head && range.status != 416) result.bytes = file.Read(range.offset, static_cast<std::size_t>(range.length));
        if (file.Identity() != token.identity) { result.bytes.clear(); result.status = 410; return result; }
        result.status = head ? 200 : range.status;
    } catch (const media::FileError& error) {
        if (error.code == ERROR_FILE_NOT_FOUND || error.code == ERROR_PATH_NOT_FOUND || error.code == ERROR_HANDLE_EOF) result.status = 410;
        else if (error.code == ERROR_SHARING_VIOLATION || error.code == ERROR_LOCK_VIOLATION || error.code == ERROR_OPERATION_ABORTED) result.status = 503;
    } catch (...) {
        result.status = 500;
        result.bytes.clear();
    }
    return result;
}

struct ReadOptions {
    media::ByteRange range;
    bool head = false;
};

std::optional<ReadOptions> PrepareRead(ICoreWebView2Environment* environment,
    ICoreWebView2WebResourceRequestedEventArgs* args, ICoreWebView2WebResourceRequest* request,
    const std::wstring& cors, const media::MediaToken& token) {
    wil::unique_cotaskmem_string method;
    if (FAILED(request->get_Method(&method)) || !method) { Respond(environment, args, 500, cors); return std::nullopt; }
    const std::wstring verb(method.get());
    if (verb == L"OPTIONS") {
        const bool allowed = !cors.empty() && media::ValidPreflight(Header(request, L"Access-Control-Request-Method"),
                                                                  Header(request, L"Access-Control-Request-Headers"));
        Respond(environment, args, allowed ? 204 : 403, cors);
        return std::nullopt;
    }
    if (verb != L"GET" && verb != L"HEAD") { Respond(environment, args, 405, cors); return std::nullopt; }
    const bool head = verb == L"HEAD";
    const auto range = head ? media::ByteRange{200, 0, 0} : media::SelectRange(media::Utf8(Header(request, L"Range")), token.identity.size);
    return ReadOptions{range, head};
}

template<class Completion>
void CompletePending(const std::weak_ptr<MediaRouteState>& weak, HWND hwnd, const WebViewHost::DocumentStamp& stamp,
                     std::uint64_t id, int failureStatus, Completion completion) noexcept {
    try {
        const auto state = weak.lock();
        if (!state) return;
        const auto found = state->pending.find(id);
        if (found == state->pending.end()) return;
        auto pending = std::move(found->second);
        state->pending.erase(found);
        const auto complete = wil::scope_exit([&pending] { pending.deferral->Complete(); });
        WebViewHost* host = nullptr;
        try {
            host = WebViewContext::GetInstance().GetWebViewHost(hwnd);
            if (!host || !host->IsCurrentDocument(stamp)) return;
            completion(*host, pending);
        } catch (...) {
            try {
                if (host && host->IsCurrentDocument(stamp)) Respond(host->GetEnvironment(), pending.args.Get(), failureStatus, pending.origin);
            } catch (...) {
                // complete 仍负责释放请求；不能再次尝试分配错误响应。
            }
        }
    } catch (...) {
        // 尚未取出的 pending 项留给导航或关闭清理，避免异常越过主线程任务边界。
    }
}

// 跨线程只传自有数据；宿主和 COM 请求在完成回调中重新取得。
struct ReadContext {
    HWND hwnd;
    WebViewHost::DocumentStamp stamp;
    std::uint64_t id;
    std::string key;
    media::MediaToken token;
    ReadOptions options;
};

template<class Authorize>
void CompleteRead(const std::weak_ptr<MediaRouteState>& weak, const ReadContext& context,
                  const ReadResult& result, Authorize authorize) noexcept {
    CompletePending(weak, context.hwnd, context.stamp, context.id, 500,
        [&context, &result, authorize](WebViewHost& host, const MediaRouteState::Pending& pending) {
            // IO 期间可能导航、撤权或淘汰令牌，不能只依赖开始读取时的检查。
            auto status = result.status;
            const auto& token = context.token;
            if (!authorize(host, token.origin) || !media::Tokens().Find(context.key, Owner(context.stamp)) ||
                !media::CanRead(token.path) || (!result.finalPath.empty() && !media::CanRead(result.finalPath))) status = 403;
            if (status == 410) media::Tokens().Revoke(context.key);
            if (status == 200 || status == 206) media::Tokens().Touch(context.key);
            static const std::vector<std::uint8_t> empty;
            Respond(host.GetEnvironment(), pending.args.Get(), status, pending.origin, &token, &context.options.range,
                    status == 200 || status == 206 ? result.bytes : empty, context.options.head);
        });
}

void RollbackPending(MediaRouteState& state, std::uint64_t id, ICoreWebView2Environment* environment,
    ICoreWebView2WebResourceRequestedEventArgs* args, const std::wstring& cors,
    ICoreWebView2Deferral* deferral, bool responded) noexcept {
    if (!responded) {
        try { Respond(environment, args, 500, cors); }
        catch (...) {
            // 响应失败后继续撤销 pending 项并完成 deferral。
        }
    }
    state.pending.erase(id);
    deferral->Complete();
}

template<class Authorize>
void QueueRead(const std::shared_ptr<MediaRouteState>& state, ICoreWebView2Environment* environment,
    ICoreWebView2WebResourceRequestedEventArgs* args, const std::wstring& cors, ReadContext context, Authorize authorize) {
    if (!state || state->pending.size() >= 16) { Respond(environment, args, 503, cors); return; }
    ComPtr<ICoreWebView2Deferral> deferral;
    if (FAILED(args->GetDeferral(&deferral)) || !deferral) { Respond(environment, args, 503, cors); return; }
    // 入队成功才把完成责任交给 pending 表；此前的异常和队满路径由局部 guard 完成 deferral。
    bool accepted = false;
    bool responded = false;
    ++state->nextId;
    const auto id = state->nextId;
    const auto finishOnFailure = wil::scope_exit([&accepted, &state, id, environment, args, &cors, &deferral, &responded] {
        if (!accepted) RollbackPending(*state, id, environment, args, cors, deferral.Get(), responded);
    });
    try {
        state->pending.try_emplace(id, args, deferral, cors);
        const std::weak_ptr<MediaRouteState> weak = state;
        context.id = id;
        const bool queued = media::QueueMediaIo([weak, context, authorize] {
            auto result = ReadSegment(context.token, context.options.range, context.options.head);
            fb2k::inMainThread([weak, context, authorize, completed = std::move(result)] {
                CompleteRead(weak, context, completed, authorize);
            });
        }, [weak, hwnd = context.hwnd, stamp = context.stamp, id] {
            CompletePending(weak, hwnd, stamp, id, 503, [](const WebViewHost& host, const MediaRouteState::Pending& pending) {
                Respond(host.GetEnvironment(), pending.args.Get(), 503, pending.origin);
            });
        });
        accepted = queued;
        if (!queued) {
            auto pending = std::move(state->pending.at(id));
            state->pending.erase(id);
            Respond(environment, pending.args.Get(), 503, cors);
            responded = true;
        }
    } catch (...) {
        // finishOnFailure 仍在主线程，负责尝试错误响应并完成尚未移交的 deferral。
    }
}
}

void WebViewHost::ClearMediaRequests(bool removeHandler) {
    // 导航开始即撤销，包括后来被取消的导航。迟到的 worker 结果找不到 pending 项便会丢弃。
    media::Tokens().InvalidateHost(instanceSerial_);
    if (mediaRoute_) {
        auto pending = std::move(mediaRoute_->pending);
        mediaRoute_->pending.clear();
        for (auto& [id, request] : pending) {
            try { Respond(environment_.get(), request.args.Get(), 410, request.origin); }
            catch (...) {
                // 文档撤销时仍须完成 deferral，错误响应失败不能中断后续请求的清理。
            }
            request.deferral->Complete();
        }
    }
    if (removeHandler && webview_ && mediaRequestedRegistered_) {
        webview_->remove_WebResourceRequested(mediaRequestedToken_);
        mediaRequestedRegistered_ = false;
    }
}

std::string WebViewHost::GetMediaOrigin() {
    std::wstring source;
    if (!IsSourceTrusted(webview_.get(), source)) return {};
    const auto origin = origin_policy::OriginOf(source);
    if (origin.empty()) return {};
    return media::Utf8(origin);
}

void WebViewHost::SetupMediaProtocol() {
    ClearMediaRequests(true);
    mediaRoute_ = std::make_shared<MediaRouteState>();
    if (!webview_) return;
    const auto filter = std::wstring(media::kUrlPrefix) + L"*";
    // DOCUMENT 过滤涵盖子框架的请求；缺少该接口时不签发地址，以免 iframe 请求绕过路由检查。
    const auto webview22 = webview_.try_query<ICoreWebView2_22>();
    if (!webview22 || FAILED(webview22->AddWebResourceRequestedFilterWithRequestSourceKinds(
        filter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL, COREWEBVIEW2_WEB_RESOURCE_REQUEST_SOURCE_KINDS_DOCUMENT))) return;
    const HRESULT hr = webview_->add_WebResourceRequested(
        Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                try { HandleMediaRequest(args); }
                catch (...) {
                    try {
                    ComPtr<ICoreWebView2WebResourceRequest> request;
                    std::wstring cors;
                    if (args && SUCCEEDED(args->get_Request(&request)) && request) {
                        const auto origin = Header(request.Get(), L"Origin");
                        if (!origin.empty() && origin == media::Wide(GetMediaOrigin())) cors = origin;
                    }
                    Respond(environment_.get(), args, 500, cors);
                    } catch (...) {
                        // 第二次响应失败不能作为 C++ 异常返回给 WebView2 的 COM 事件分发。
                    }
                }
                return S_OK;
            }).Get(), &mediaRequestedToken_);
    mediaRequestedRegistered_ = SUCCEEDED(hr);
}

void WebViewHost::HandleMediaRequest(ICoreWebView2WebResourceRequestedEventArgs* args) {
    ComPtr<ICoreWebView2WebResourceRequest> request;
    wil::unique_cotaskmem_string uri;
    if (!args || FAILED(args->get_Request(&request)) || !request || FAILED(request->get_Uri(&uri)) || !uri) return;
    const std::wstring url(uri.get());
    if (!url.starts_with(media::kUrlPrefix)) return;
    const auto stamp = CaptureDocument();
    const std::string key = media::Utf8(url.substr(std::wcslen(media::kUrlPrefix)));
    const auto token = media::Tokens().Find(key, Owner(stamp));
    std::wstring source;
    const auto origin = Header(request.Get(), L"Origin");
    const auto referer = origin_policy::OriginOf(Header(request.Get(), L"Referer"));
    const bool trusted = IsSourceTrusted(webview_.get(), source);
    const auto currentOrigin = origin_policy::OriginOf(source);
    if (!trusted || !IsCurrentDocument(stamp) ||
        !IsTrustedOrigin(currentOrigin) || !media::AuthorizeRequestOrigin(currentOrigin, origin, referer)) {
        Respond(environment_.get(), args, 403, {});
        return;
    }
    // 来源可信不等于持有令牌：还要核对文档身份、签发来源和当前路径权限。
    const std::wstring cors = origin.empty() ? std::wstring() : currentOrigin;
    if (!token || token->origin != media::Utf8(currentOrigin)) { Respond(environment_.get(), args, 403, cors); return; }
    if (!media::CanRead(token->path)) { Respond(environment_.get(), args, 403, cors); return; }
    const auto options = PrepareRead(environment_.get(), args, request.Get(), cors, *token);
    if (!options) return;
    const auto authorize = [](WebViewHost& host, std::string_view expectedOrigin) {
        std::wstring currentSource;
        return host.IsSourceTrusted(host.webview_.get(), currentSource) &&
            media::Utf8(origin_policy::OriginOf(currentSource)) == expectedOrigin;
    };
    QueueRead(mediaRoute_, environment_.get(), args, cors, {parentHwnd_, stamp, 0, key, *token, *options}, authorize);
}
