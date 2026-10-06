#include "pch.h"
#include "webview/WebViewEnvironment.h"
#include "core/WebViewContext.h"
#include "settings/SecurityConfig.h"
#include "prefs/PreferencesPage.h"   // webview_prefs::GetPreheatEnabled / GetCdpPort
#include "utils/PathExpansion.h"
#include "webview/UserDataMigration.h"
#include <Shlobj.h>
#include <chrono>
#include <filesystem>
#include <wrl/client.h>
#include <wrl/event.h>
#include <WebView2EnvironmentOptions.h>

using namespace Microsoft::WRL;

// ============================================
// WebViewEnvironment 实现
// ============================================

WebViewEnvironment& WebViewEnvironment::GetInstance() {
    static WebViewEnvironment instance;
    return instance;
}

// 便携版的用户数据目录放在实例自己的 profile 下。同一个用户数据目录由同一组浏览器
// 进程服务：几个实例共用一个目录时，后开的那个要么因为环境选项不同（比如启动参数里的
// CDP 端口）创建环境失败，要么页面跑在先开的实例的浏览器进程里、随它退出一起没了。
// 安装版仍用 %LOCALAPPDATA%：它的 profile 在漫游的 %APPDATA% 下，缓存不宜跟着漫游，
// 而且一个 Windows 用户只有一份安装版 profile。
namespace {

// 安装版的用户数据目录，也是便携版以前共用的那个；取不到时为空串
std::wstring SharedUserDataPath() {
    wchar_t path[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) return L"";
    return std::wstring(path) + L"\\foobar2000\\foo_ui_webview2";
}

// 便携版自己的用户数据目录；不是便携版或取不到 profile 时为空串
std::wstring PortableUserDataPath() {
    if (!core_api::is_portable_mode_enabled()) return L"";
    // 带结尾反斜杠
    const std::wstring profileDir = PathExpansion::GetProfileDirectory();
    return profileDir.size() > 1 ? profileDir + L"foo_ui_webview2" : L"";
}

// 共用目录被占着时最多等这么久再放弃迁移。只在首次迁移时等，之后目标目录已存在。
constexpr auto kSharedFolderReleaseWait = std::chrono::seconds(3);

}  // namespace

std::wstring WebViewEnvironment::GetUserDataPath() {
    std::wstring userDataPath = PortableUserDataPath();
    if (userDataPath.empty()) {
        userDataPath = SharedUserDataPath();
        if (userDataPath.empty()) return L"";
        // 已存在是常态，返回值不看
        (void)CreateDirectoryW(std::filesystem::path(userDataPath).parent_path().c_str(), nullptr);
    }
    (void)CreateDirectoryW(userDataPath.c_str(), nullptr);
    return userDataPath;
}

// 便携版第一次用自己的目录时，把共用目录里的数据复制过来，见 UserDataMigration.h。
// 自己的目录一旦存在就不再迁移；共用目录被占用时就不迁移，并在控制台写明手动做法。
void WebViewEnvironment::MigrateSharedUserData() {
    namespace fs = std::filesystem;
    const fs::path target = PortableUserDataPath();
    std::error_code ec;
    if (target.empty() || fs::exists(target, ec)) return;
    const fs::path source = SharedUserDataPath();
    if (source.empty() || !fs::is_directory(source, ec)) return;

    const auto utf8 = [](const fs::path& path) {
        return std::string(pfc::stringcvt::string_utf8_from_wide(path.c_str()).get_ptr());
    };
    if (webview_udf::WaitWhileInUse(source, kSharedFolderReleaseWait)) {
        FB2K_console_formatter()
            << "[WebView2 UI] Web storage was not copied from " << utf8(source).c_str()
            << ": another foobar2000 instance is using it. This instance starts with empty web"
               " storage in " << utf8(target).c_str()
            << ". To copy it later, close every foobar2000 instance, delete that folder and start"
               " this instance again; whatever pages saved there meanwhile is lost.";
        return;
    }

    const auto started = std::chrono::steady_clock::now();
    std::wstring error;
    const webview_udf::CopyOutcome outcome = webview_udf::CopyUserDataFolder(source, target, &error);
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    if (outcome == webview_udf::CopyOutcome::Copied) {
        FB2K_console_formatter()
            << "[WebView2 UI] Copied web storage from " << utf8(source).c_str() << " to "
            << utf8(target).c_str() << " in " << static_cast<int>(elapsedMs)
            << " ms; the original folder is kept.";
    } else if (outcome == webview_udf::CopyOutcome::Failed) {
        FB2K_console_formatter()
            << "[WebView2 UI] Could not copy web storage from " << utf8(source).c_str()
            << " (" << utf8(error).c_str() << "); this instance starts with empty web storage.";
    }
}

void WebViewEnvironment::Preheat() {
    bool shouldCreate = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (ready_ || creating_) {
            return;  // Already ready or creating
        }
        
        creating_ = true;  // Set flag while holding lock
        shouldCreate = true;
    }
    // Lock released here - important to avoid deadlock if callback is synchronous
    
    if (shouldCreate) {
        console::print("[WebView2 UI] Preheating WebView2 environment...");
        CreateEnvironmentInternal();
    }
}

void WebViewEnvironment::GetEnvironment(const EnvironmentCallback& callback) {
    ICoreWebView2Environment* envToReturn = nullptr;
    bool shouldStartCreating = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (ready_ && environment_) {
            // Environment is ready, will call callback outside lock
            envToReturn = environment_.get();
        } else {
            // Add to pending callbacks
            if (callback) {
                pendingCallbacks_.push_back(callback);
            }
            
            // Start creating if not already
            if (!creating_) {
                creating_ = true;  // Set flag while holding lock
                shouldStartCreating = true;
            }
        }
    }
    // Lock released here - important to avoid deadlock if callback is synchronous
    
    // Call callback outside lock to avoid deadlock
    if (envToReturn && callback) {
        callback(envToReturn);
        return;
    }
    
    if (shouldStartCreating) {
        CreateEnvironmentInternal();
    }
}

void WebViewEnvironment::CreateEnvironmentInternal() {
    // Note: creating_ flag should already be set by caller
    
    // 必须在 GetUserDataPath 创建目录之前：迁移以目标目录不存在为前提
    MigrateSharedUserData();
    auto userDataPath = GetUserDataPath();
    
    // Check WebView2 Runtime availability
    wil::unique_cotaskmem_string version;
    HRESULT checkHr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
    if (FAILED(checkHr) || !version) {
        console::print("[WebView2 UI] WebView2 Runtime not available");
        DrainPendingCallbacksOnFailure();
        return;
    }
    
    FB2K_console_formatter() << "[WebView2 UI] WebView2 Runtime version: " 
                             << pfc::stringcvt::string_utf8_from_wide(version.get()).get_ptr();
    
    // Create environment options with custom schemes
    auto envOptions = Make<CoreWebView2EnvironmentOptions>();
    
    // Register fb2k:// and artwork:// protocols
    auto schemeRegistrationFb2k = Make<CoreWebView2CustomSchemeRegistration>(L"fb2k");
    schemeRegistrationFb2k->put_TreatAsSecure(TRUE);
    schemeRegistrationFb2k->put_HasAuthorityComponent(TRUE);
    LPCWSTR allowedOriginsFb2k[] = { L"*" };
    schemeRegistrationFb2k->SetAllowedOrigins(1, allowedOriginsFb2k);
    
    auto schemeRegistrationArtwork = Make<CoreWebView2CustomSchemeRegistration>(L"artwork");
    schemeRegistrationArtwork->put_TreatAsSecure(TRUE);
    schemeRegistrationArtwork->put_HasAuthorityComponent(FALSE);
    LPCWSTR allowedOriginsArtwork[] = { L"*" };
    schemeRegistrationArtwork->SetAllowedOrigins(1, allowedOriginsArtwork);
    
    ICoreWebView2CustomSchemeRegistration* schemeRegistrations[] = {
        schemeRegistrationFb2k.Get(),
        schemeRegistrationArtwork.Get()
    };
    envOptions->SetCustomSchemeRegistrations(2, schemeRegistrations);

    // ============================================
    // 环境选项减法（关闭运行期纯开销特性）
    // SDK 辅助类 CoreWebView2EnvironmentOptions 已实现 Options1-8 全链，
    // QI 恒成功；保留 SUCCEEDED 判空仅为防御（勿改用 WRL ComPtr::As 搭配
    // wil::com_ptr，模板不匹配）。
    // ============================================

    // Options5: 关闭跟踪防护 — 本项目导航面受控（fb2k://、虚拟主机、dev server），
    // 过滤引擎对每个资源请求都是纯开销（官方文档明确关闭可提升运行时性能）
    wil::com_ptr<ICoreWebView2EnvironmentOptions5> options5;
    if (SUCCEEDED(envOptions->QueryInterface(IID_PPV_ARGS(&options5)))) {
        options5->put_EnableTrackingPrevention(FALSE);
    }

    // Options3: 崩溃转储仅本地保留（用户数据目录 CrashDumps），不向微软上报，
    // 消除崩溃上报的后台 IO/网络开销；本地转储仍可配合 webview_crash.log 诊断
    wil::com_ptr<ICoreWebView2EnvironmentOptions3> options3;
    if (SUCCEEDED(envOptions->QueryInterface(IID_PPV_ARGS(&options3)))) {
        options3->put_IsCustomCrashReportingEnabled(TRUE);
    }
    
    // ============================================
    // 浏览器命令行参数
    // ============================================
    // Visual Hosting (CompositionController + DirectComposition) 模式下，
    // Chromium 的 native window occlusion 计算会在窗口最小化/被完全遮挡时
    // 把 WebView 判定为 occluded 并挂起呈现；恢复时在 Composition 模式下
    // 运行时不一定能可靠收到“重新可见”信号，导致 DComp 表面停留空白
    // （现象：最小化→恢复后只剩空 Win32 窗口壳，且不触发 ProcessFailed）。
    // 关闭该特性可让 WebView 始终保持呈现，规避恢复空白。
    // 代价：窗口最小化时后台仍持续渲染，CPU/GPU 占用略增（音乐播放器常驻后台，可接受）。
    // --disable-features 必须保持单一 switch（逗号分隔值）：WebView2 对重复
    // switch 的语义是 last-instance-wins、仅 features 类做 union 合并，
    // 为规避运行时版本差异，一律在此处自行合并。
    //
    // SpareRendererForSitePerProcess: 防御性禁用 Chromium "备用渲染进程"预热。
    // 本组件是单源单页应用，备用 renderer 纯属内存浪费；本机基线未观测到该进程
    // （官方口径 only consulted in site-per-process mode），显式禁用以挡住任意
    // Runtime 版本/导航时机下的创建。
    std::wstring disableFeatures = L"CalculateNativeWinOcclusion,SpareRendererForSitePerProcess";
    std::wstring extraArgs;

    // CDP remote debugging port — 仅在 CDP 远程调试开关启用时开放
    // 允许 MCP 工具集 / AI 智能体通过 CDP 操控 WebView2
    if (security_config::IsCdpRemoteEnabled()) {
        // 端口来自 Developer 子页（默认 9222）；读出时已钳到 1024–65535。环境只创建一次，
        // 运行中改端口不影响本进程已开的端口。
        const int cdpPort = webview_prefs::GetCdpPort();
        // CDP 模式追加后台节流禁用，消除 hidden ≥5min 后定时器分钟级对齐的长尾
        // （IntensiveWakeUpThrottling 是 blink feature、没有独立 switch，只能并入
        // --disable-features）。
        extraArgs += L" --remote-debugging-port=" + std::to_wstring(cdpPort) +
                     L" --disable-background-timer-throttling"
                     L" --disable-renderer-backgrounding";
        disableFeatures += L",IntensiveWakeUpThrottling";
        // keep-alive 判定绑定"本进程真实开了端口"的快照，而非配置实时值
        // （运行中勾/取消 CDP 不影响已创建环境的端口状态）。
        security_config::NoteCdpPortOpenedThisProcess();
        FB2K_console_formatter() << "[WebView2 UI] CDP remote debugging enabled on port " << cdpPort;
    }

    std::wstring browserArgs = L"--disable-features=" + disableFeatures + extraArgs;

    envOptions->put_AdditionalBrowserArguments(browserArgs.c_str());
    
    // Create environment
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,                    // Browser executable path
        userDataPath.c_str(),       // User data directory
        envOptions.Get(),           // Environment options
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                std::vector<EnvironmentCallback> callbacksToInvoke;
                ICoreWebView2Environment* envToReturn = nullptr;
                bool failed = false;
                
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    
                    if (FAILED(result) || !env) {
                        FB2K_console_formatter() << "[WebView2 UI] Failed to create environment, HRESULT: 0x" 
                                                 << pfc::format_hex((uint32_t)result);
                        // 提取等待者，在锁外以 nullptr 回调
                        callbacksToInvoke = std::move(pendingCallbacks_);
                        pendingCallbacks_.clear();
                        creating_ = false;
                        failed = true;
                    } else {
                        environment_ = env;
                        ready_ = true;
                        creating_ = false;
                        envToReturn = environment_.get();
                        
                        // Move callbacks to local vector to call outside lock
                        callbacksToInvoke = std::move(pendingCallbacks_);
                        pendingCallbacks_.clear();
                    }
                }
                
                if (failed) {
                    // 通知所有等待者环境创建失败
                    for (auto& cb : callbacksToInvoke) {
                        if (cb) cb(nullptr);
                    }
                    return result;
                }
                
                console::print("[WebView2 UI] WebView2 environment preheated successfully");
                
                // Call all pending callbacks outside lock to avoid deadlock
                for (auto& cb : callbacksToInvoke) {
                    if (cb) {
                        cb(envToReturn);
                    }
                }
                
                return S_OK;
            }
        ).Get()
    );
    
    if (FAILED(hr)) {
        FB2K_console_formatter() << "[WebView2 UI] CreateCoreWebView2EnvironmentWithOptions failed: 0x" 
                                 << pfc::format_hex((uint32_t)hr);
        DrainPendingCallbacksOnFailure();
    }
}

void WebViewEnvironment::DrainPendingCallbacksOnFailure() {
    std::vector<EnvironmentCallback> failedCallbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        creating_ = false;
        failedCallbacks = std::move(pendingCallbacks_);
        pendingCallbacks_.clear();
    }
    // 在锁外回调，避免死锁
    for (auto& cb : failedCallbacks) {
        if (cb) cb(nullptr);
    }
}

void WebViewEnvironment::Invalidate(const char* reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!ready_) {
        // 已经不处于就绪态（未创建过 / 已作废 / 正在创建），无需重复处理。
        return;
    }
    
    // 只清就绪标记。旧 environment_ COM 引用不主动释放：WebView2 运行时在
    // 浏览器进程退出后仍可能有异步清理回调访问它（理由同 Shutdown）。
    // 下一次 GetEnvironment 会走创建分支，成功后 environment_ = env 自然替换。
    ready_ = false;
    
    FB2K_console_formatter()
        << "[WebView2 UI] WebView2 environment invalidated, reason: "
        << (reason && reason[0] ? reason : "unspecified");
}

void WebViewEnvironment::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 不再主动释放 environment_ COM 引用。
    // controller_->Close() 是异步操作，WebView2 运行时在关闭后仍有后台清理任务。
    // 若在此处释放最后一个 ICoreWebView2Environment 引用，运行时的异步回调会
    // 访问已销毁对象，导致未捕获异常 → std::terminate → abort。
    // 让 COM 运行时在进程退出时自然回收。
    if (environment_) {
        console::print("[WebView2 UI] WebView2 environment marked for natural release");
    }
    
    ready_ = false;
    creating_ = false;
    pendingCallbacks_.clear();
    
    console::print("[WebView2 UI] WebView2 environment shutdown complete");
}

// ============================================
// initquit 服务 - 在启动时预热环境
// ============================================

class WebViewEnvironmentInitQuit : public initquit {
public:
    void on_init() override {
        // 预热开关在 Performance 子页；关闭时不在这里创建环境，首个 GetEnvironment() 调用会按需创建。
        // 开关改动只影响下一次启动。
        if (!webview_prefs::GetPreheatEnabled()) {
            console::print("[WebView2 UI] WebView2 environment preheat disabled; it will be created on first use");
            return;
        }
        // Use main thread callback to ensure proper COM context
        fb2k::inMainThread([]() {
            WebViewEnvironment::GetInstance().Preheat();
        });
    }
    
    void on_quit() override {
        // 显式释放 WebView2 环境的全局 COM 引用
        // 此时所有 Controller 已由 user_interface::shutdown() 关闭
        // 释放环境引用后 msedgewebview2.exe 子进程将自动退出
        WebViewEnvironment::GetInstance().Shutdown();
    }
};

// Register with high priority to start early
static initquit_factory_t<WebViewEnvironmentInitQuit> g_webview_env_factory;

void InitWebViewEnvironment() {
    WebViewEnvironment::GetInstance().Preheat();
}

void ShutdownWebViewEnvironment() {
    WebViewEnvironment::GetInstance().Shutdown();
}
