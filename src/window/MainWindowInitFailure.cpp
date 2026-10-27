// MainWindowInitFailure.cpp — WebView 创建失败后主窗口的原生兜底：错误对话框与右键菜单
//
// 这两样只在 WebView 不可用时出现，所以只能用系统控件：主窗口带 WS_EX_NOREDIRECTIONBITMAP，
// GDI 在客户区画不出东西；自绘菜单与托盘图标也都依赖 WebView。
#include "pch.h"
#include "window/MainWindow.h"
#include "webview/WebViewEnvironment.h"
#include "webview/WebViewHost.h"
#include "utils/I18n.h"
#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>

namespace {
    constexpr int kCopyDiagnosticsButton = 101;
    constexpr int kOpenPreferencesButton = 102;
    constexpr int kOpenConsoleButton = 103;
    constexpr wchar_t kRuntimeDownloadUrl[] = L"https://go.microsoft.com/fwlink/p/?LinkId=2124703";

    enum FallbackMenuCommand : UINT {
        kMenuPreferences = 1,
        kMenuConsole,
        kMenuRetry,
        kMenuDetails,
        kMenuExit,
    };

    const wchar_t* HintFor(const WebViewHost::InitFailure& failure) {
        if (failure.stage && strcmp(failure.stage, "runtime") == 0) {
            return TR("WebView2 Runtime was not found. <a href=\"https://go.microsoft.com/fwlink/p/?LinkId=2124703\">Install WebView2 Runtime</a>, then restart foobar2000.",
                      "未找到 WebView2 运行时。请<a href=\"https://go.microsoft.com/fwlink/p/?LinkId=2124703\">安装 WebView2 运行时</a>，然后重启 foobar2000。");
        }
        // 微软文档给的原因是同一数据目录已有实例在运行、且环境参数不同。宿主与 WebView2 进程的
        // DPI 感知不一致也会报这个码（运行时的调试输出会写明），有用户报告清掉 exe 上的高 DPI
        // 替代后恢复。文档明确这个码不该原样重试。
        if (failure.hr == HRESULT_FROM_WIN32(ERROR_INVALID_STATE)) {
            return TR("WebView2 refused to start because another WebView2 process is using the same data folder "
                      "with different settings. Close any other foobar2000 that has this component, then restart "
                      "foobar2000. If it still fails, check whether a compatibility mode or a high DPI override is "
                      "set in foobar2000.exe Properties > Compatibility (including \"Change settings for all "
                      "users\").",
                      "WebView2 拒绝启动：另一个 WebView2 进程正以不同的设置使用同一数据目录。"
                      "请关闭其他运行了本组件的 foobar2000 实例，然后重启 foobar2000。若仍启动失败，"
                      "请检查 foobar2000.exe「属性 → 兼容性」中是否启用了兼容模式或高 DPI 缩放替代"
                      "（包括「更改所有用户的设置」）。");
        }
        if (failure.hr == E_ACCESSDENIED || failure.hr == E_UNEXPECTED) {
            return TR("WebView2 could not access its data folder. Check the permissions of the userData folder "
                      "listed in the diagnostics.",
                      "WebView2 无法访问数据目录。请检查诊断信息中 userData 所指目录的访问权限。");
        }
        return TR("WebView2 failed to start. Copy the diagnostics and include them when you report the problem.",
                  "WebView2 启动失败。报告问题时请附上复制的诊断信息。");
    }

    HRESULT CALLBACK InitFailureDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM, LONG_PTR refData) {
        if (msg == TDN_HYPERLINK_CLICKED) {
            // 提示文本里只有下载 Runtime 这一个链接，打开固定地址，不用回调传来的 href
            ShellExecuteW(hwnd, L"open", kRuntimeDownloadUrl, nullptr, nullptr, SW_SHOWNORMAL);
        } else if (msg == TDN_BUTTON_CLICKED && wParam == kCopyDiagnosticsButton) {
            uSetClipboardString(reinterpret_cast<const char*>(refData));
            return S_FALSE;  // 复制后不关闭对话框
        }
        return S_OK;
    }
}

bool MainWindow::HasWebViewInitFailed() const {
    return !webView_ || webView_->LastInitFailure().stage != nullptr;
}

void MainWindow::ShowInitFailureDialog() {
    if (initFailureDialogOpen_ || !hwnd_ || !IsWindow(hwnd_)) return;

    // webView_ 为空说明连异步创建都没能发起
    WebViewHost::InitFailure failure = webView_ ? webView_->LastInitFailure() : WebViewHost::InitFailure{};
    if (!failure.stage) failure = {"start", E_FAIL};

    const std::string diagnostics = std::string("stage=") + failure.stage +
        "\nhr=0x" + pfc::format_hex((uint32_t)failure.hr, 8).get_ptr() +
        "\n" + WebViewHost::DescribeInitContext(hwnd_, '\n');
    const std::wstring details = pfc::stringcvt::string_wide_from_utf8(diagnostics.c_str()).get_ptr();

    const TASKDIALOG_BUTTON buttons[] = {
        {kCopyDiagnosticsButton, TR("Copy diagnostics", "复制诊断信息")},
        {kOpenPreferencesButton, TR("Open Preferences", "打开首选项")},
        {kOpenConsoleButton, TR("Open Console", "打开控制台")},
        // 不用 TDCBF_CLOSE_BUTTON：系统按钮跟随 Windows 界面语言，会和组件语言混在一起
        {IDCANCEL, TR("Close", "关闭")},
    };
    TASKDIALOGCONFIG config = {};
    config.cbSize = sizeof(config);
    config.hwndParent = hwnd_;
    config.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = L"foobar2000";
    config.pszMainIcon = TD_ERROR_ICON;
    config.pszMainInstruction = TR("The WebView2 UI could not be loaded", "WebView2 UI 加载失败");
    config.pszContent = HintFor(failure);
    config.pszExpandedInformation = details.c_str();
    config.pszCollapsedControlText = TR("Show diagnostics", "显示诊断信息");
    config.pszExpandedControlText = TR("Hide diagnostics", "隐藏诊断信息");
    config.pButtons = buttons;
    config.cButtons = ARRAYSIZE(buttons);
    config.nDefaultButton = kCopyDiagnosticsButton;
    config.pfCallback = InitFailureDialogProc;
    config.lpCallbackData = reinterpret_cast<LONG_PTR>(diagnostics.c_str());

    initFailureDialogOpen_ = true;
    int clicked = 0;
    const HRESULT hr = TaskDialogIndirect(&config, &clicked, nullptr, nullptr);
    initFailureDialogOpen_ = false;

    if (FAILED(hr)) {
        const std::wstring text = std::wstring(HintFor(failure)) + L"\n\n" + details;
        MessageBoxW(hwnd_, text.c_str(), L"foobar2000", MB_OK | MB_ICONERROR);
        return;
    }
    if (clicked == kOpenPreferencesButton) {
        standard_commands::main_preferences();
    } else if (clicked == kOpenConsoleButton) {
        standard_commands::main_show_console();
    }
}

void MainWindow::ShowFallbackContextMenu(LPARAM lParam) {
    POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (lParam == -1) GetCursorPos(&pt);  // Shift+F10 / 菜单键触发时没有坐标

    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    // 正常加载过程中也会走到这里；那时重试会打断进行中的创建，所以只在失败后可用
    const UINT failedOnly = HasWebViewInitFailed() ? MF_ENABLED : MF_GRAYED;
    AppendMenuW(menu, MF_STRING, kMenuPreferences, TR("Preferences", "首选项"));
    AppendMenuW(menu, MF_STRING, kMenuConsole, TR("Console", "控制台"));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | failedOnly, kMenuRetry, TR("Reload interface", "重新加载界面"));
    AppendMenuW(menu, MF_STRING | failedOnly, kMenuDetails, TR("Error details...", "错误详情…"));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, TR("Exit foobar2000", "退出 foobar2000"));

    const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);

    switch (cmd) {
        case kMenuPreferences: standard_commands::main_preferences(); break;
        case kMenuConsole: standard_commands::main_show_console(); break;
        case kMenuRetry:
            // 文档建议重试时从新环境开始：环境本身可能就是失败的一方
            WebViewEnvironment::GetInstance().Invalidate("user-retry");
            RebuildWebViewAfterHealthFailure("user-retry");
            break;
        case kMenuDetails: ShowInitFailureDialog(); break;
        case kMenuExit: standard_commands::main_exit(); break;
        default: break;
    }
}
