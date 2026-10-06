/**
 * WebViewHostArtwork.cpp - WebViewHost 的封面协议
 *
 * fb2k://artwork、artwork:// 与同源 /fb2k-artwork/ 路由的 WebResourceRequested 处理，
 * 以及进程级的缩放封面缓存与负缓存。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "webview/ArtworkWorkerQueue.h"
#include "webview/ScaledArtworkCache.h"
#include "api/ArtworkApi.h"
#include "api/ArtworkRequestParser.h"
#include "utils/ArtworkCacheKey.h"
#include "utils/ImageUtils.h"
#include "domain/PathSecurity.h"
#include <wrl/event.h>
#include <objidl.h>
#include <vector>
#include <functional>  // std::hash for ETag generation
#include <unordered_map>  // scaled artwork cache
#include <mutex>

using namespace Microsoft::WRL;

// ==========================================================================
// Scaled artwork response cache
// key: resolved external artwork path(s) when available, otherwise request path.
// This keeps embedded artwork isolated per track while allowing tracks that
// share the same folder image to reuse the same resized JPEG entry.
// ==========================================================================
namespace {
    struct NegativeCacheEntry {
        std::chrono::steady_clock::time_point timestamp;
    };

    static constexpr size_t MAX_SCALED_CACHE = 500;
    static constexpr size_t MAX_SCALED_CACHE_BYTES = 64ull * 1024 * 1024;
    static constexpr int SCALED_CACHE_TTL_SEC = 300;  // entries without a source fingerprint only
    static constexpr int NEGATIVE_CACHE_TTL_SEC = 30;

    // Positive cache locks internally; both the async completion callback and the
    // synchronous fallback write through it so one eviction policy applies.
    artwork_cache::ScaledArtworkCache s_scaledArtworkCache{artwork_cache::ScaledArtworkLimits{
        MAX_SCALED_CACHE, MAX_SCALED_CACHE_BYTES, std::chrono::seconds(SCALED_CACHE_TTL_SEC)}};

    std::unordered_map<std::string, NegativeCacheEntry> s_artworkNegativeCache;
    std::mutex s_artworkNegativeCacheMutex;
}

// Compute a source-content fingerprint from filesystem metadata.
// Returns an empty string when no source paths are accessible.
static std::string ComputeSourceFingerprint(const std::vector<std::string>& sourcePaths) {
    std::string data;
    data.reserve(sourcePaths.size() * 64);
    for (const auto& p : sourcePaths) {
        const std::wstring wp = pfc::stringcvt::string_wide_from_utf8(p.c_str()).get_ptr();
        WIN32_FILE_ATTRIBUTE_DATA attrs = {};
        if (GetFileAttributesExW(wp.c_str(), GetFileExInfoStandard, &attrs)) {
            const uint64_t mtime =
                (static_cast<uint64_t>(attrs.ftLastWriteTime.dwHighDateTime) << 32) |
                static_cast<uint64_t>(attrs.ftLastWriteTime.dwLowDateTime);
            const uint64_t fsize =
                (static_cast<uint64_t>(attrs.nFileSizeHigh) << 32) |
                static_cast<uint64_t>(attrs.nFileSizeLow);
            data += p;
            data += ':';
            data += std::to_string(mtime);
            data += ':';
            data += std::to_string(fsize);
            data += ';';
        } else {
            // Stat failed: include path so a later successful stat produces a different hash.
            data += p;
            data += ":x;";
        }
    }
    return data;
}

// Serve a scaled-cache hit as a 200 response; false on miss so the caller
// continues to the extractor. The current source fingerprint decides freshness:
// an entry written for a different mtime/size of the same cover file is a miss.
// Shared bytes come out of the cache lock; the response is built outside it.
static bool TryRespondFromScaledCache(
    ICoreWebView2Environment* environment,
    ICoreWebView2WebResourceRequestedEventArgs* args,
    const std::string& cacheKey,
    const std::string& sourceFingerprint,
    const std::string& etag)
{
    if (cacheKey.empty() || !environment || !args) return false;
    const auto hit = s_scaledArtworkCache.Get(
        cacheKey, sourceFingerprint, std::chrono::steady_clock::now());
    if (!hit) return false;
    const std::vector<uint8_t>& hitBytes = *hit->bytes;
    IStream* cacheStream = SHCreateMemStream(
        hitBytes.data(), static_cast<UINT>(hitBytes.size()));
    if (!cacheStream) return false;
    const std::wstring mimeTypeW(hit->mimeType.begin(), hit->mimeType.end());
    const std::wstring etagW(etag.begin(), etag.end());
    std::wstring headers = L"Content-Type: " + mimeTypeW + L"\r\n";
    headers += L"Cache-Control: public, max-age=86400\r\n";
    headers += L"ETag: " + etagW + L"\r\n";
    headers += L"Access-Control-Allow-Origin: *\r\n";
    wil::com_ptr<ICoreWebView2WebResourceResponse> response;
    if (SUCCEEDED(environment->CreateWebResourceResponse(
            cacheStream, 200, L"OK", headers.c_str(), &response))) {
        args->put_Response(response.get());
        LOG("Scaled artwork cache hit: ", hitBytes.size(), " bytes");
    }
    cacheStream->Release();
    return true;
}

// ==========================================================================
// Custom protocol handler
// fb2k://artwork, artwork://, https://foo-ui-webview2.local/fb2k-artwork/*
// ==========================================================================
void WebViewHost::SetupCustomProtocol() {
    if (!webview_) {
        LOG("SetupCustomProtocol: webview_ is null");
        return;
    }
    
    // 获取 ICoreWebView2_2 接口以添加 WebResourceRequested 过滤器
    wil::com_ptr<ICoreWebView2_2> webview2;
    HRESULT hr = webview_->QueryInterface(IID_PPV_ARGS(&webview2));
    if (FAILED(hr) || !webview2) {
        LOG("Failed to get ICoreWebView2_2 for custom protocol, HRESULT: ", pfc::format_hex((uint32_t)hr));
        return;
    }
    
    // 添加 URI 过滤器 - fb2k://artwork/* 和 artwork://*
    std::wstring fb2kArtworkFilter(L"fb2k://artwork/*");
    hr = webview2->AddWebResourceRequestedFilter(fb2kArtworkFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    if (FAILED(hr)) {
        LOG("Failed to add fb2k://artwork filter, HRESULT: ", pfc::format_hex((uint32_t)hr));
    }
    
    std::wstring artworkFilter(L"artwork://*");
    hr = webview2->AddWebResourceRequestedFilter(artworkFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    if (FAILED(hr)) {
        LOG("Failed to add artwork:// filter, HRESULT: ", pfc::format_hex((uint32_t)hr));
    }
    
    // 添加同源 HTTPS 路由过滤器 - 用于 Canvas CORS 支持
    std::wstring sameOriginArtworkFilter(L"https://foo-ui-webview2.local/fb2k-artwork/*");
    hr = webview2->AddWebResourceRequestedFilter(sameOriginArtworkFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    if (FAILED(hr)) {
        LOG("Failed to add same-origin artwork filter, HRESULT: ", pfc::format_hex((uint32_t)hr));
    } else {
        LOG("✓ Added same-origin artwork route filter");
    }
    
    // 注册请求处理程序
    hr = webview_->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this](ICoreWebView2* /*sender*/, ICoreWebView2WebResourceRequestedEventArgs* args) {
                // 纵深防御：顶层 catch-all，防止 bad_alloc/length_error 等异常逃逸 COM 回调边界触发 terminate()。
                try {
                wil::com_ptr<ICoreWebView2WebResourceRequest> request;
                if (FAILED(args->get_Request(&request)) || !request) {
                    return S_OK;
                }
                
                wil::unique_cotaskmem_string uri;
                if (FAILED(request->get_Uri(&uri)) || !uri) {
                    return S_OK;
                }
                
                std::wstring uriStr(uri.get());
                
                // 检查是否是我们的自定义协议或同源路由
                std::wstring fb2kPrefix(L"fb2k://artwork/");
                std::wstring artworkPrefix(L"artwork://");
                bool isFb2kArtwork = (uriStr.find(fb2kPrefix) == 0);
                bool isArtworkScheme = (uriStr.find(artworkPrefix) == 0);
                std::wstring sameOriginPrefix(L"https://foo-ui-webview2.local/fb2k-artwork/");
                bool isSameOriginArtwork = (uriStr.find(sameOriginPrefix) == 0);
                
                if (!isFb2kArtwork && !isArtworkScheme && !isSameOriginArtwork) {
                    return S_OK;  // 不是我们的协议
                }
                
                // 处理 OPTIONS 预检请求 (CORS preflight)
                wil::unique_cotaskmem_string method;
                if (FAILED(request->get_Method(&method)) || !method) {
                    return S_OK;
                }
                const std::wstring methodStr(method.get());
                if (methodStr == L"OPTIONS") {
                        wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                        std::wstring corsHeaders = 
                            L"Access-Control-Allow-Origin: *\r\n"
                            L"Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                            L"Access-Control-Allow-Headers: *\r\n"
                            L"Access-Control-Max-Age: 86400\r\n";
                        if (SUCCEEDED(environment_->CreateWebResourceResponse(
                                nullptr, 204, L"No Content", corsHeaders.c_str(), &response))) {
                            args->put_Response(response.get());
                            LOG("OPTIONS preflight response sent");
                        }
                        return S_OK;
                }
                if (methodStr != L"GET") {
                    wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                    if (SUCCEEDED(environment_->CreateWebResourceResponse(
                            nullptr, 405, L"Method Not Allowed",
                            L"Allow: GET, OPTIONS\r\nAccess-Control-Allow-Origin: *\r\n",
                            &response))) {
                        args->put_Response(response.get());
                    }
                    return S_OK;
                }

                const std::string uriUtf8 =
                    pfc::stringcvt::string_utf8_from_wide(uriStr.c_str()).get_ptr();
                const auto parsed = artwork_request::Parse(uriUtf8);
                if (!parsed.ok()) {
                    wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                    const int status = parsed.httpStatus();
                    const wchar_t* reason = status == 414 ? L"URI Too Long" : L"Bad Request";
                    if (SUCCEEDED(environment_->CreateWebResourceResponse(
                            nullptr, status, reason, L"Access-Control-Allow-Origin: *\r\n",
                            &response))) {
                        args->put_Response(response.get());
                    }
                    return S_OK;
                }

                const std::string& pathUtf8 = parsed.request.path;
                const std::string& artworkType = parsed.request.type;
                const int maxSize = parsed.request.maxSize;
                const auto lifecycleCapture = artworkLifecycle_.CaptureRequest();
                if (!lifecycleCapture.has_value()) {
                    return S_OK;
                }
                auto instrumentation = artworkInstrumentation_.Begin({
                    isFb2kArtwork ? "fb2k" : (isArtworkScheme ? "artwork" : "same-origin"),
                    GetCurrentThreadId(), ownerThreadId_,
                    lifecycleCapture->navigationGeneration,
                    lifecycleCapture->hostGeneration,
                    pathUtf8, artworkType, static_cast<uint32_t>(maxSize),
                });
                const std::wstring pathWide =
                    pfc::stringcvt::string_wide_from_utf8(pathUtf8.c_str()).get_ptr();
                std::wstring permissionError;
                if (!PathSecurity::Instance().ValidateMediaAccess(pathWide, permissionError)) {
                    instrumentation.SetPermissionResult("denied");
                    instrumentation.Complete("denied", "permission-denied");
                    artworkLifecycle_.Complete(*lifecycleCapture);
                    wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                    if (SUCCEEDED(environment_->CreateWebResourceResponse(
                            nullptr, 403, L"Forbidden", L"Access-Control-Allow-Origin: *\r\n",
                            &response))) {
                        args->put_Response(response.get());
                    }
                    LOG("Artwork resource denied by MediaRead");
                    return S_OK;
                }
                instrumentation.SetPermissionResult("allowed");

                // === Source-aware ETag: resolve source paths unconditionally ===
                instrumentation.RecordSourceResolve();
                std::vector<std::string> artworkSourcePaths;
                artwork_internal::TryGetArtworkSourcePathsForPath(
                    pathUtf8, artworkType, artworkSourcePaths);
                const std::string sourceFingerprint =
                    ComputeSourceFingerprint(artworkSourcePaths);
                const std::string& fingerprintBase =
                    sourceFingerprint.empty() ? pathUtf8 : sourceFingerprint;
                const size_t contentHash = std::hash<std::string>{}(
                    fingerprintBase + artworkType + std::to_string(maxSize));
                char etagBuffer[32];
                snprintf(etagBuffer, sizeof(etagBuffer), "\"%zx\"", contentHash);
                const std::string etag(etagBuffer);
                LOG("Artwork resource authorized: type=", artworkType.c_str(),
                    ", maxSize=", maxSize, ", sources=",
                    static_cast<int>(artworkSourcePaths.size()));

                // === Negative cache check (avoids extractor on known-missing artwork) ===
                const std::string negCacheKey =
                    artworkType + ":" +
                    (artworkSourcePaths.empty() ? pathUtf8 : artworkSourcePaths[0]);
                {
                    std::lock_guard lock(s_artworkNegativeCacheMutex);
                    auto nit = s_artworkNegativeCache.find(negCacheKey);
                    if (nit != s_artworkNegativeCache.end()) {
                        const auto age =
                            std::chrono::steady_clock::now() - nit->second.timestamp;
                        if (std::chrono::duration_cast<std::chrono::seconds>(age).count()
                                < NEGATIVE_CACHE_TTL_SEC) {
                            instrumentation.Complete("missing", "negative-cache-hit");
                            artworkLifecycle_.Complete(*lifecycleCapture);
                            wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                            if (SUCCEEDED(environment_->CreateWebResourceResponse(
                                    nullptr, 404, L"Not Found",
                                    L"Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\n",
                                    &response))) {
                                args->put_Response(response.get());
                            }
                            return S_OK;
                        }
                        s_artworkNegativeCache.erase(nit);  // expired entry
                    }
                }

                // === If-None-Match against source-aware ETag (after source resolve) ===
                {
                    wil::com_ptr<ICoreWebView2HttpRequestHeaders> reqHdrs;
                    if (wil::unique_cotaskmem_string ifNoneMatch;
                        SUCCEEDED(request->get_Headers(&reqHdrs)) && reqHdrs &&
                        SUCCEEDED(reqHdrs->GetHeader(L"If-None-Match", &ifNoneMatch)) &&
                        ifNoneMatch) {
                        const std::string ifNoneMatchStr(
                            pfc::stringcvt::string_utf8_from_wide(ifNoneMatch.get()).get_ptr());
                        if (ifNoneMatchStr == etag) {
                            const std::wstring etagW(etag.begin(), etag.end());
                            const std::wstring headers304 =
                                L"ETag: " + etagW + L"\r\n"
                                L"Cache-Control: no-cache\r\n"
                                L"Access-Control-Allow-Origin: *\r\n";
                            wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                            if (SUCCEEDED(environment_->CreateWebResourceResponse(
                                    nullptr, 304, L"Not Modified",
                                    headers304.c_str(), &response))) {
                                args->put_Response(response.get());
                                LOG("Artwork 304 Not Modified (source-aware ETag)");
                            }
                            instrumentation.Complete("not-modified", "etag-match");
                            artworkLifecycle_.Complete(*lifecycleCapture);
                            return S_OK;
                        }
                    }
                }

                // === Scaled artwork cache lookup ===
                std::string cacheKey;
                if (maxSize > 0) {
                    cacheKey = BuildScaledArtworkCacheKey(
                        pathUtf8, artworkType, maxSize, artworkSourcePaths);
                }

                if (maxSize > 0 && TryRespondFromScaledCache(
                        environment_.get(), args, cacheKey, sourceFingerprint, etag)) {
                    instrumentation.Complete("success", "scaled-cache-hit");
                    artworkLifecycle_.Complete(*lifecycleCapture);
                    return S_OK;
                }

                // === Async cache miss: GetDeferral + worker queue ===
                wil::com_ptr<ICoreWebView2Deferral> deferral;
                if (SUCCEEDED(args->GetDeferral(&deferral)) && deferral) {
                    // Capture everything needed for owner-thread completion.
                    // artworkHostAlive_ guards against WebViewHost destruction.
                    auto aliveFlag   = artworkHostAlive_;
                    auto envCapture  = environment_;   // COM AddRef
                    wil::com_ptr<ICoreWebView2WebResourceRequestedEventArgs> argsCapture = args;
                    auto lifecycle   = &artworkLifecycle_;
                    auto capture     = *lifecycleCapture;
                    const std::string etagCopy      = etag;
                    const std::string cacheKeyCopy  = cacheKey;

                    // Read If-None-Match once more for the async path.
                    std::string ifNoneMatchEtag;
                    {
                        wil::com_ptr<ICoreWebView2HttpRequestHeaders> reqHdrs2;
                        if (SUCCEEDED(request->get_Headers(&reqHdrs2)) && reqHdrs2) {
                            if (wil::unique_cotaskmem_string inm;
                                SUCCEEDED(reqHdrs2->GetHeader(L"If-None-Match", &inm)) && inm)
                                ifNoneMatchEtag = pfc::stringcvt::string_utf8_from_wide(
                                    inm.get()).get_ptr();
                        }
                    }

                    artwork_worker::WorkItem workItem;
                    workItem.pathUtf8        = pathUtf8;
                    workItem.artworkType     = artworkType;
                    workItem.maxSize         = maxSize;
                    workItem.cacheKey        = cacheKey;
                    workItem.negCacheKey     = negCacheKey;
                    workItem.fingerprint     = sourceFingerprint;
                    workItem.etag            = etag;
                    workItem.ifNoneMatchEtag = std::move(ifNoneMatchEtag);
                    workItem.navGen          = lifecycleCapture->navigationGeneration;
                    workItem.hostGen         = lifecycleCapture->hostGeneration;
                    workItem.abort           = std::make_shared<abort_callback_impl>();
                    // Capture deferral by copy (wil::com_ptr is refcounted). Do NOT move it:
                    // Submit(std::move(workItem)) always takes ownership of the WorkItem, even when
                    // it returns false for queue-full; the outer deferral must remain usable for 503.
                    workItem.complete = [
                        aliveFlag, envCapture, argsCapture = std::move(argsCapture),
                        deferral, lifecycle, capture,
                        etagCopy, cacheKeyCopy
                    ](artwork_worker::ArtworkResult result) mutable {
                        // Runs on fb2k main thread (== WebView owner thread).
                        if (!aliveFlag->load(std::memory_order_acquire)) {
                            if (deferral) deferral->Complete();
                            return;
                        }
                        const auto completionStatus = lifecycle->Complete(capture);
                        if (completionStatus != artwork_request::CompletionResult::Accepted) {
                            if (deferral) deferral->Complete();
                            return;
                        }

                        wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                        if (result.statusCode == 200 && !result.bytes.empty()) {
                            // One shared buffer feeds both the cache entry and the response stream.
                            auto sharedBytes = std::make_shared<const std::vector<uint8_t>>(
                                std::move(result.bytes));
                            if (!result.cacheKey.empty()) {
                                s_scaledArtworkCache.Put(
                                    result.cacheKey, sharedBytes, result.mime, result.fingerprint,
                                    std::chrono::steady_clock::now());
                            }
                            IStream* stream = SHCreateMemStream(
                                sharedBytes->data(),
                                static_cast<UINT>(sharedBytes->size()));
                            if (stream) {
                                const std::wstring mimeW(
                                    result.mime.begin(), result.mime.end());
                                const std::wstring etagW(
                                    result.etag.begin(), result.etag.end());
                                std::wstring h = L"Content-Type: " + mimeW + L"\r\n";
                                h += L"Cache-Control: public, max-age=86400\r\n";
                                h += L"ETag: " + etagW + L"\r\n";
                                h += L"Access-Control-Allow-Origin: *\r\n";
                                envCapture->CreateWebResourceResponse(
                                    stream, 200, L"OK", h.c_str(), &response);
                                stream->Release();
                            }
                        } else if (result.statusCode == 304 || result.is304) {
                            const std::wstring etagW(
                                result.etag.begin(), result.etag.end());
                            const std::wstring h304 =
                                L"ETag: " + etagW + L"\r\n"
                                L"Cache-Control: no-cache\r\n"
                                L"Access-Control-Allow-Origin: *\r\n";
                            envCapture->CreateWebResourceResponse(
                                nullptr, 304, L"Not Modified", h304.c_str(), &response);
                        } else if (result.statusCode == 404) {
                            if (result.isNegative && !result.negCacheKey.empty()) {
                                std::lock_guard lock(s_artworkNegativeCacheMutex);
                                s_artworkNegativeCache[result.negCacheKey] =
                                    NegativeCacheEntry{std::chrono::steady_clock::now()};
                            }
                            envCapture->CreateWebResourceResponse(
                                nullptr, 404, L"Not Found",
                                L"Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\n",
                                &response);
                        } else {
                            // 503 (queue-full/cancelled) or 500
                            const int sc = result.statusCode;
                            const wchar_t* reason = sc == 503
                                ? L"Service Unavailable" : L"Internal Server Error";
                            const std::wstring h5xx =
                                sc == 503
                                ? L"Retry-After: 5\r\nAccess-Control-Allow-Origin: *\r\n"
                                : L"Access-Control-Allow-Origin: *\r\n";
                            envCapture->CreateWebResourceResponse(
                                nullptr, sc, reason, h5xx.c_str(), &response);
                        }

                        if (response && argsCapture) {
                            argsCapture->put_Response(response.get());
                        }
                        if (deferral) deferral->Complete();
                        LOG("[ArtworkWorker] async complete: status=", result.statusCode);
                    };

                    if (artworkWorkerQueue_ &&
                        artworkWorkerQueue_->Submit(std::move(workItem))) {
                        instrumentation.RecordExtractor();
                        instrumentation.Complete("async", "worker-queued");
                        return S_OK;  // deferral will be completed by worker
                    }

                    // Queue full / worker unavailable — respond 503 and complete deferral.
                    // workItem (and its completion lambda) was destroyed inside Submit; use the
                    // outer refcounted deferral / args / lifecycle still on this stack.
                    wil::com_ptr<ICoreWebView2WebResourceResponse> response503;
                    if (SUCCEEDED(environment_->CreateWebResourceResponse(
                            nullptr, 503, L"Service Unavailable",
                            L"Retry-After: 5\r\nAccess-Control-Allow-Origin: *\r\n",
                            &response503))) {
                        args->put_Response(response503.get());
                    }
                    instrumentation.Complete("rejected", "worker-queue-full");
                    artworkLifecycle_.Complete(*lifecycleCapture);
                    if (deferral) deferral->Complete();
                    return S_OK;
                }

                // No deferral — fall through to inline sync extraction below.
                // === Sync fallback (GetDeferral failed or async path unavailable) ===
                instrumentation.RecordExtractor();
                artwork_internal::BinaryArtwork artwork;
                bool found = artwork_internal::GetArtworkBinaryForPath(pathUtf8, artworkType, artwork);
                
                if (!found || artwork.bytes.empty()) {
                    // Write negative cache so repeated requests skip the extractor.
                    {
                        std::lock_guard lock(s_artworkNegativeCacheMutex);
                        s_artworkNegativeCache[negCacheKey] =
                            NegativeCacheEntry{std::chrono::steady_clock::now()};
                    }
                    instrumentation.Complete("missing", "artwork-not-found");
                    artworkLifecycle_.Complete(*lifecycleCapture);
                    wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                    if (SUCCEEDED(environment_->CreateWebResourceResponse(
                            nullptr, 404, L"Not Found",
                            L"Cache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\n",
                            &response))) {
                        args->put_Response(response.get());
                    }
                    return S_OK;
                }
                
                // Resize if maxSize is specified
                const uint8_t* finalData = artwork.bytes.data();
                size_t finalSize = artwork.bytes.size();
                std::string finalMimeType = artwork.mimeType;
                std::vector<uint8_t> resizedData;
                
                if (maxSize > 0) {
                    instrumentation.RecordResize();
                    int outWidth = 0, outHeight = 0;
                    const char* outMimeType = nullptr;
                    
                    resizedData = ResizeImageWIC(
                        artwork.bytes.data(),
                        artwork.bytes.size(),
                        maxSize,
                        outWidth,
                        outHeight,
                        outMimeType
                    );
                    
                    if (!resizedData.empty()) {
                        finalData = resizedData.data();
                        finalSize = resizedData.size();
                        finalMimeType = outMimeType;
                        LOG("Image resized to ", outWidth, "x", outHeight, ", ", finalSize, " bytes");

                        // Same cache and eviction policy as the async completion path.
                        s_scaledArtworkCache.Put(
                            cacheKey,
                            std::make_shared<const std::vector<uint8_t>>(resizedData),
                            finalMimeType, sourceFingerprint,
                            std::chrono::steady_clock::now());
                    } else if (outWidth > 0) {
                        LOG("Image already ", outWidth, "x", outHeight, ", no resize needed");
                    }
                }
                
                // Build 200 response with artwork data
                IStream* stream = SHCreateMemStream(
                    finalData,
                    static_cast<UINT>(finalSize)
                );
                if (stream) {
                    std::wstring mimeTypeW(finalMimeType.begin(), finalMimeType.end());
                    std::wstring etagW(etag.begin(), etag.end());
                    
                    std::wstring headers = L"Content-Type: " + mimeTypeW + L"\r\n";
                    headers += L"Cache-Control: public, max-age=86400\r\n";  // no immutable
                    headers += L"ETag: " + etagW + L"\r\n";
                    headers += L"Access-Control-Allow-Origin: *\r\n";

                    wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                    if (SUCCEEDED(environment_->CreateWebResourceResponse(
                            stream, 200, L"OK", headers.c_str(), &response))) {
                        args->put_Response(response.get());
                        LOG("Artwork response sent: ", finalSize, " bytes, type: ", finalMimeType.c_str());
                    }
                    
                    stream->Release();
                }

                instrumentation.Complete("success", "response-created");
                artworkLifecycle_.Complete(*lifecycleCapture);
                
                return S_OK;
                } catch (...) {
                    wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                    if (environment_ && SUCCEEDED(environment_->CreateWebResourceResponse(
                            nullptr, 500, L"Internal Server Error",
                            L"Access-Control-Allow-Origin: *\r\n", &response))) {
                        args->put_Response(response.get());
                    }
                    return S_OK;
                }
            }
        ).Get(),
        &artworkResourceRequestedToken_
    );
    
    if (SUCCEEDED(hr)) {
        artworkLifecycle_.RecordTokenAdd(
            artwork_request::TokenKind::WebResourceRequested, true);
        LOG("Custom protocol handlers registered (fb2k://artwork, artwork://)");
    } else {
        LOG("Failed to register WebResourceRequested handler, HRESULT: ", pfc::format_hex((uint32_t)hr));
    }
}
