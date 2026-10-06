#pragma once
#include "pch.h"

#include <cstdint>
#include <memory>
#include <string>

// ============================================
// SharedPcmBuffer — 一块 WebView2 共享缓冲的所有权包装（docs/audio-pcm/SPEC.md D6）
//
// COM 对象归 WebView 的 UI 线程：建、投递、关闭、释放都只在主线程做。
// 写端线程只拿 Data() 的裸指针与 Size()。Close() 之后先前取得的地址失效，
// 所以有写端在用时不能 Close()，也不能析构。
// ============================================
class SharedPcmBuffer {
public:
    // 调用方页面的运行时能不能用共享缓冲：环境能 QI 到 ICoreWebView2Environment12、
    // webview 能 QI 到 ICoreWebView2_17。
    static bool IsSupported(ICoreWebView2Environment* environment, ICoreWebView2* webview);

    // 建一块 bytes 字节的共享缓冲。失败返回空，*hr 给出原因（32 位进程找不到连续地址时
    // 多为 E_OUTOFMEMORY）。
    static std::unique_ptr<SharedPcmBuffer> Create(ICoreWebView2Environment* environment, std::uint64_t bytes,
                                                   HRESULT* hr = nullptr);

    // 没 Detach() 的会先 Close() 再释放；只能在主线程、且没有写端时发生。
    ~SharedPcmBuffer();

    SharedPcmBuffer(const SharedPcmBuffer&) = delete;
    SharedPcmBuffer& operator=(const SharedPcmBuffer&) = delete;

    // 宿主侧视图的地址；Close() 或 Detach() 之后为空。
    std::uint8_t* Data() const { return data_; }
    std::uint64_t Size() const { return size_; }

    // 以只读方式投给 webview 的主框架；additionalDataJson 为空时页面的 additionalData 是
    // undefined。每投一次页面多一个独立视图，Close() 不影响已经投出的视图。
    HRESULT PostReadOnly(ICoreWebView2* webview, const std::wstring& additionalDataJson);

    // 释放宿主侧视图。
    void Close();

    // 放弃 COM 引用而不 Close()、不 Release：退出时还有 worker 在写的缓冲用它故意泄漏，
    // 免得静态表在 DLL 卸载时（持有 loader lock）调 COM（D15）。
    void Detach();

private:
    SharedPcmBuffer(wil::com_ptr<ICoreWebView2SharedBuffer> buffer, std::uint8_t* data, std::uint64_t size)
        : buffer_(std::move(buffer)), data_(data), size_(size) {}

    wil::com_ptr<ICoreWebView2SharedBuffer> buffer_;
    std::uint8_t* data_ = nullptr;
    std::uint64_t size_ = 0;
};
