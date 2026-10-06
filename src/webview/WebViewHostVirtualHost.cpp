/**
 * WebViewHostVirtualHost.cpp - WebViewHost 的虚拟主机映射
 *
 * 设置与清除虚拟主机到本地目录的映射，并为映射目录下的 .js、.css 响应补上 charset=utf-8。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include <wrl/event.h>
#include <objidl.h>    // IStream, CreateStreamOnHGlobal
#include <vector>
#include <algorithm>

using namespace Microsoft::WRL;

HRESULT WebViewHost::SetVirtualHostMapping(const std::wstring& hostName, const std::wstring& folderPath) {
    if (!webview_) return E_FAIL;
    
    // 获取 ICoreWebView2_3 接口（支持虚拟主机映射）
    wil::com_ptr<ICoreWebView2_3> webview3;
    HRESULT hr = webview_->QueryInterface(IID_PPV_ARGS(&webview3));
    if (FAILED(hr) || !webview3) {
        LOG("Failed to get ICoreWebView2_3 interface for virtual host mapping");
        return hr;
    }
    
    // 设置虚拟主机映射
    hr = webview3->SetVirtualHostNameToFolderMapping(
        hostName.c_str(),
        folderPath.c_str(),
        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW
    );
    
    if (SUCCEEDED(hr)) {
        LOG("Virtual host mapping set: https://", 
            pfc::stringcvt::string_utf8_from_wide(hostName.c_str()).get_ptr(),
            "/ -> ", 
            pfc::stringcvt::string_utf8_from_wide(folderPath.c_str()).get_ptr());
        
        // 保存映射并注册 charset 修复
        virtualHostName_ = hostName;
        virtualHostFolderPath_ = folderPath;
        SetupCharsetFixForVirtualHost();
    } else {
        LOG("Failed to set virtual host mapping, HRESULT: ", pfc::format_hex((uint32_t)hr));
    }
    
    return hr;
}

HRESULT WebViewHost::ClearVirtualHostMapping(const std::wstring& hostName) {
    if (!webview_) return E_FAIL;
    
    // 获取 ICoreWebView2_3 接口
    wil::com_ptr<ICoreWebView2_3> webview3;
    HRESULT hr = webview_->QueryInterface(IID_PPV_ARGS(&webview3));
    if (SUCCEEDED(hr) && webview3) {
        hr = webview3->ClearVirtualHostNameToFolderMapping(hostName.c_str());
        if (SUCCEEDED(hr)) {
            LOG("Virtual host mapping cleared: ", 
                pfc::stringcvt::string_utf8_from_wide(hostName.c_str()).get_ptr());
        }
    }
    return hr;
}

//==========================================================================
// 修复虚拟主机映射的 .js/.css 文件 charset
// WebView2 的 SetVirtualHostNameToFolderMapping 不设置 Content-Type charset，
// 中文 Windows 上 .js/.css 内的 UTF-8 中文会被错误按 GBK 解析导致乱码。
// 通过 WebResourceRequested 拦截这些请求，自行读取文件并附带 charset=utf-8 响应。
//==========================================================================
void WebViewHost::SetupCharsetFixForVirtualHost() {
    if (charsetFixRegistered_ || !webview_ || virtualHostName_.empty()) return;
    charsetFixRegistered_ = true;

    // 注册过滤器: https://<virtualHost>/[.js] 和 [.css]
    wil::com_ptr<ICoreWebView2_2> webview2;
    HRESULT hr = webview_->QueryInterface(IID_PPV_ARGS(&webview2));
    if (FAILED(hr) || !webview2) {
        LOG("Failed to get ICoreWebView2_2 for charset fix filter");
        return;
    }

    std::wstring jsFilter = L"https://" + virtualHostName_ + L"/*.js";
    std::wstring cssFilter = L"https://" + virtualHostName_ + L"/*.css";

    webview2->AddWebResourceRequestedFilter(jsFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_SCRIPT);
    webview2->AddWebResourceRequestedFilter(cssFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_STYLESHEET);

    webview_->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this](ICoreWebView2* /*sender*/, ICoreWebView2WebResourceRequestedEventArgs* args) {
                // 纵深防御：顶层 catch-all，防止 bad_alloc/length_error 等异常逃逸 COM 回调边界触发 terminate()。
                try {
                wil::com_ptr<ICoreWebView2WebResourceRequest> request;
                if (FAILED(args->get_Request(&request)) || !request) return S_OK;

                wil::unique_cotaskmem_string uri;
                if (FAILED(request->get_Uri(&uri)) || !uri) return S_OK;
                std::wstring uriStr(uri.get());

                // 只处理虚拟主机上的 .js/.css 请求
                std::wstring prefix = L"https://" + virtualHostName_ + L"/";
                if (uriStr.find(prefix) != 0) return S_OK;

                // 提取相对路径
                std::wstring relativePath = uriStr.substr(prefix.length());

                // 去除 query string
                auto qpos = relativePath.find(L'?');
                if (qpos != std::wstring::npos) relativePath = relativePath.substr(0, qpos);

                // URL decode
                // WebView2 虚拟主机不做复杂 URL 编码，直接 %20 替换即可
                for (size_t p = 0; (p = relativePath.find(L"%20", p)) != std::wstring::npos; ) {
                    relativePath.replace(p, 3, L" ");
                    ++p;  // advance past the replacement
                }

                // 将 / 转为系统分隔符
                std::replace(relativePath.begin(), relativePath.end(), L'/', L'\\');

                // 确定 MIME 类型
                std::wstring contentType;
                if (relativePath.ends_with(L".js")) {
                    contentType = L"text/javascript; charset=utf-8";
                } else if (relativePath.ends_with(L".css")) {
                    contentType = L"text/css; charset=utf-8";
                } else {
                    return S_OK; // 非 js/css，放行给默认处理
                }

                // 构造本地文件路径
                std::wstring localPath = virtualHostFolderPath_ + L"\\" + relativePath;

                // 安全校验: 确保路径在映射目录内（防路径遍历）
                std::wstring canonicalPathBuf(MAX_PATH, L'\0');
                std::wstring canonicalBaseBuf(MAX_PATH, L'\0');
                if (!GetFullPathNameW(localPath.c_str(), MAX_PATH, canonicalPathBuf.data(), nullptr) ||
                    !GetFullPathNameW(virtualHostFolderPath_.c_str(), MAX_PATH, canonicalBaseBuf.data(), nullptr)) {
                    return S_OK;
                }
                canonicalPathBuf.resize(wcslen(canonicalPathBuf.c_str()));
                canonicalBaseBuf.resize(wcslen(canonicalBaseBuf.c_str()));
                // 确保规范化路径以映射目录为前缀
                if (canonicalPathBuf.find(canonicalBaseBuf) != 0) {
                    LOG("Charset fix: blocked path traversal attempt");
                    return S_OK;
                }

                // 读取文件
                HANDLE hFile = CreateFileW(localPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hFile == INVALID_HANDLE_VALUE) {
                    return S_OK; // 文件不存在，放行给默认 404 处理
                }

                LARGE_INTEGER fileSize;
                GetFileSizeEx(hFile, &fileSize);
                std::vector<uint8_t> fileData(static_cast<size_t>(fileSize.QuadPart));
                DWORD bytesRead = 0;
                ReadFile(hFile, fileData.data(), static_cast<DWORD>(fileData.size()), &bytesRead, nullptr);
                CloseHandle(hFile);

                // 创建 IStream
                IStream* stream = SHCreateMemStream(fileData.data(), static_cast<UINT>(bytesRead));
                if (!stream) return S_OK;

                // 构造带正确 charset 的响应
                std::wstring headers = L"Content-Type: " + contentType + L"\r\n"
                    L"Cache-Control: no-cache\r\n"
                    L"Access-Control-Allow-Origin: *\r\n";

                wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                if (SUCCEEDED(environment_->CreateWebResourceResponse(
                        stream, 200, L"OK", headers.c_str(), &response))) {
                    args->put_Response(response.get());
                }

                stream->Release();
                return S_OK;
                } catch (...) {
                    return S_OK;  // 异常逃逸即放行默认处理，绝不让异常穿透 COM 回调边界
                }
            }
        ).Get(),
        &charsetResourceRequestedToken_
    );

    LOG("Charset fix: WebResourceRequested handler registered for virtual host JS/CSS");
}
