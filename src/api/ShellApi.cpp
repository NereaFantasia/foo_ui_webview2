// ShellApi.cpp - Shell/Process API
// Provides shell operations and external process launching

#include "pch.h"
#include "api/ShellApi.h"
#include "api/TypedApi.h"
#include "api/generated/ShellSchema.h"
#include "domain/PathSecurity.h"  // 安全: 统一路径验证
#include <ShlObj.h>
#include <shellapi.h>

namespace {
    namespace sh = api::shell;

    std::wstring TrimSpace(const std::wstring& text) {
        size_t start = 0;
        while (start < text.size() && iswspace(text[start])) {
            start++;
        }
        size_t end = text.size();
        while (end > start && iswspace(text[end - 1])) {
            end--;
        }
        return text.substr(start, end - start);
    }

    std::wstring TrimOuterQuotes(const std::wstring& text) {
        if (text.size() >= 2 && text.front() == L'"' && text.back() == L'"') {
            return text.substr(1, text.size() - 2);
        }
        return text;
    }

    bool IsAbsolutePath(const std::wstring& path) {
        if (path.length() >= 2 && path[1] == L':') return true;
        if (path.length() >= 2 && path[0] == L'\\' && path[1] == L'\\') return true;
        return false;
    }

    std::wstring QuoteArg(const std::wstring& arg) {
        std::wstring escaped;
        escaped.reserve(arg.size() + 8);
        for (wchar_t ch : arg) {
            if (ch == L'"') escaped.push_back(L'\\');
            escaped.push_back(ch);
        }
        return L"\"" + escaped + L"\"";
    }

    std::vector<std::wstring> WideArgs(const std::optional<std::vector<std::string>>& args) {
        std::vector<std::wstring> out;
        if (args) {
            out.reserve(args->size());
            for (const auto& arg : *args) out.push_back(Utf8ToWide(arg));
        }
        return out;
    }

    //==========================================================================
    // shell.showInExplorer - Show file in Windows Explorer
    // 安全: 路径校验由声明的 @security Read 完成
    //==========================================================================
    api::Result<void> ShellShowInExplorer(const sh::ShowInExplorerParams& params) {
        std::wstring path = Utf8ToWide(params.path);

        // Use SHOpenFolderAndSelectItems for proper file selection
        PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(path.c_str());
        if (pidl) {
            HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
            ILFree(pidl);

            if (SUCCEEDED(hr)) {
                return api::Ok();
            }
        }

        // Fallback: use explorer /select,path
        std::wstring cmd = L"/select,\"" + path + L"\"";
        HINSTANCE result = ::ShellExecuteW(nullptr, L"open", L"explorer.exe",
            cmd.c_str(), nullptr, SW_SHOWNORMAL);

        if (reinterpret_cast<intptr_t>(result) > 32) {
            return api::Ok();
        }

        return api::Fail("Failed to open Explorer", ApiErrorCode::OPERATION_FAILED);
    }

    //==========================================================================
    // shell.openWith - Open file with default application
    // 安全: 黑名单模式 - 禁止可执行文件
    // 安全: 路径校验由声明的 @security Read 完成
    //==========================================================================
    api::Result<void> ShellOpenWith(const sh::OpenWithParams& params) {
        std::wstring path = Utf8ToWide(params.path);

        // 安全: 可执行文件黑名单 (核心防线)
        static const std::vector<std::wstring> dangerousExtensions = {
            // 可执行程序
            L".exe", L".com", L".cmd", L".bat", L".ps1", L".vbs", L".vbe",
            L".js", L".jse", L".wsf", L".wsh", L".msc",
            // 脚本和安装包
            L".scr", L".pif", L".hta", L".cpl",
            L".msi", L".msp", L".msu",
            // 动态库
            L".dll", L".ocx", L".sys", L".drv",
            // 快捷方式 (可指向危险程序)
            L".lnk", L".url",
            // 其他危险格式
            L".reg", L".inf",
            L".jar", L".application"
        };

        // 转小写比较
        std::wstring lowerPath = path;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);

        // 提取扩展名
        size_t dotPos = lowerPath.rfind(L'.');
        if (dotPos != std::wstring::npos) {
            std::wstring ext = lowerPath.substr(dotPos);
            for (const auto& dangerous : dangerousExtensions) {
                if (ext == dangerous) {
                    return api::Fail("Executable files cannot be opened for security reasons",
                                     ApiErrorCode::PERMISSION_DENIED);
                }
            }
        }

        // 通过黑名单检查，让 Windows 决定用什么程序打开
        HINSTANCE result = ::ShellExecuteW(nullptr, L"open", path.c_str(),
            nullptr, nullptr, SW_SHOWNORMAL);

        if (reinterpret_cast<intptr_t>(result) > 32) {
            return api::Ok();
        }

        // ShellExecute 的错误值分别对应找不到、拒绝访问与无关联程序，各给自己的 code。
        switch (reinterpret_cast<intptr_t>(result)) {
            case SE_ERR_FNF:
                return api::Fail("File not found", ApiErrorCode::NOT_FOUND);
            case SE_ERR_PNF:
                return api::Fail("Path not found", ApiErrorCode::NOT_FOUND);
            case SE_ERR_ACCESSDENIED:
                return api::Fail("Access denied", ApiErrorCode::PERMISSION_DENIED);
            case SE_ERR_NOASSOC:
                return api::Fail("No application associated with this file type", ApiErrorCode::NOT_SUPPORTED);
            default:
                return api::Fail("ShellExecute failed with code " + std::to_string(reinterpret_cast<intptr_t>(result)),
                                 ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // shell.openExternal - Open URL in default browser
    //==========================================================================
    api::Result<void> ShellOpenExternal(const sh::OpenExternalParams& params) {
        const std::string& url = params.url;

        // Security check: only allow http/https URLs
        if (!url.starts_with("http://") && !url.starts_with("https://") && !url.starts_with("mailto:")) {
            return api::Fail("Only http://, https://, and mailto: URLs are allowed", ApiErrorCode::INVALID_PARAMS);
        }

        std::wstring wurl = Utf8ToWide(url);

        HINSTANCE result = ::ShellExecuteW(nullptr, L"open", wurl.c_str(),
            nullptr, nullptr, SW_SHOWNORMAL);

        if (reinterpret_cast<intptr_t>(result) > 32) {
            return api::Ok();
        }

        return api::Fail("Failed to open URL", ApiErrorCode::OPERATION_FAILED);
    }

    //==========================================================================
    // shell.exec - Execute command (cwd 路径校验, 不限制命令)
    //==========================================================================
    api::Result<sh::ExecResult> ShellExec(const sh::ExecParams& params) {
        const bool hidden = params.hidden;
        const std::string cwd = params.cwd.value_or("");

        // 不限制可执行命令: 主题来自用户自己或可信来源, 信任边界等同于安装一个
        // foobar2000 组件。命令名白名单 (cmd/powershell/node 等解释器) 既挡不住
        // 恶意主题 (可经 cmd /c、powershell -Command 任意执行), 又绊住正常主题的
        // 直接调用, 因此不作为安全边界。实际护栏是 cwd 的路径校验 (见下)
        // 与 FileApi 的 MediaWrite 路径黑名单。

        // Build command line
        std::wstring cmdLine = Utf8ToWide(params.command);
        for (const auto& arg : WideArgs(params.args)) {
            cmdLine += L" " + QuoteArg(arg);
        }

        std::wstring wcwd = cwd.empty() ? L"" : Utf8ToWide(cwd);

        // 安全校验: cwd 路径必须通过 PathSecurity 验证
        if (!wcwd.empty()) {
            std::wstring cwdError;
            if (!PathSecurity::Instance().ValidatePath(wcwd, cwdError)) {
                return api::Fail("Invalid cwd: " + WideToUtf8(cwdError), ApiErrorCode::INVALID_PATH);
            }
        }

        // Create process
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        if (hidden) {
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
        }

        PROCESS_INFORMATION pi = {};

        // Need a modifiable buffer for CreateProcess
        std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
        cmdBuffer.push_back(L'\0');

        BOOL success = CreateProcessW(
            nullptr,                    // Application name
            cmdBuffer.data(),           // Command line
            nullptr,                    // Process attributes
            nullptr,                    // Thread attributes
            FALSE,                      // Inherit handles
            hidden ? CREATE_NO_WINDOW : 0,  // Creation flags
            nullptr,                    // Environment
            wcwd.empty() ? nullptr : wcwd.c_str(),  // Current directory
            &si,                        // Startup info
            &pi                         // Process info
        );

        if (!success) {
            DWORD error = GetLastError();
            return api::Fail("CreateProcess failed with error " + std::to_string(error), ApiErrorCode::OPERATION_FAILED);
        }

        // Don't wait for process - return immediately
        // For async execution, we just launch and forget

        // Close handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        sh::ExecResult result;
        result.processId = pi.dwProcessId;
        return result;
    }

    //==========================================================================
    // shell.spawn - Execute process using structured parameters
    // 设计目标: 避免 cmd /c start 的假成功，支持短等待拿到早退退出码
    // 安全: 不限制可执行文件, 绝对路径/cwd 走 PathSecurity 校验
    //==========================================================================
    api::Result<sh::SpawnResult> ShellSpawn(const sh::SpawnParams& params) {
        const bool hidden = params.hidden;
        const std::string cwdUtf8 = params.cwd.value_or("");
        // 声明限定 waitForExitMs >= 0；DWORD 能装下的等待时长封顶，避免溢出。
        const DWORD waitForExitMs = params.waitForExitMs > static_cast<std::int64_t>(INFINITE - 1)
            ? INFINITE - 1
            : static_cast<DWORD>(params.waitForExitMs);

        std::wstring executable = TrimOuterQuotes(TrimSpace(Utf8ToWide(params.executable)));
        if (executable.empty()) {
            return api::Fail("executable is empty after trim", ApiErrorCode::INVALID_PARAMS);
        }

        // 不限制可执行文件: 见 ShellExec 说明。信任主题作者前提下, 可执行白名单
        // 挡不住恶意输入, 不作为安全边界。实际护栏是下方绝对路径与 cwd 的
        // PathSecurity 校验, 以及 FileApi 的 MediaWrite 路径黑名单。

        // 绝对路径可执行文件需要通过路径安全校验
        if (IsAbsolutePath(executable)) {
            std::wstring errorMsg;
            if (!PathSecurity::Instance().ValidatePath(executable, errorMsg)) {
                return api::Fail(WideToUtf8(L"Access denied: " + errorMsg), ApiErrorCode::PERMISSION_DENIED);
            }
        }

        std::wstring cwd = cwdUtf8.empty() ? L"" : TrimOuterQuotes(TrimSpace(Utf8ToWide(cwdUtf8)));
        if (!cwd.empty()) {
            std::wstring errorMsg;
            if (!PathSecurity::Instance().ValidatePath(cwd, errorMsg)) {
                return api::Fail(WideToUtf8(L"Invalid cwd: " + errorMsg), ApiErrorCode::INVALID_PATH);
            }
            // 检查 cwd 目录是否实际存在，避免 CreateProcess error 267
            DWORD attrs = GetFileAttributesW(cwd.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                return api::Fail("cwd directory does not exist: " + cwdUtf8, ApiErrorCode::NOT_FOUND);
            }
        }

        // 命令行格式: "exe" "arg1" "arg2"
        std::wstring cmdLine = QuoteArg(executable);
        for (const auto& arg : WideArgs(params.args)) {
            cmdLine += L" ";
            cmdLine += QuoteArg(arg);
        }

        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        if (hidden) {
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
        }

        PROCESS_INFORMATION pi = {};
        std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
        cmdBuffer.push_back(L'\0');

        const bool hasExplicitPath = executable.find(L'\\') != std::wstring::npos ||
                                     executable.find(L'/') != std::wstring::npos ||
                                     IsAbsolutePath(executable);
        LPCWSTR appName = hasExplicitPath ? executable.c_str() : nullptr;

        BOOL success = CreateProcessW(
            appName,
            cmdBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            hidden ? CREATE_NO_WINDOW : 0,
            nullptr,
            cwd.empty() ? nullptr : cwd.c_str(),
            &si,
            &pi
        );

        if (!success) {
            DWORD error = GetLastError();
            return api::Fail("CreateProcess failed with error " + std::to_string(error), ApiErrorCode::OPERATION_FAILED);
        }

        sh::SpawnResult result;
        result.processId = pi.dwProcessId;
        std::optional<api::Failure> failure;

        if (waitForExitMs > 0) {
            DWORD waitRet = WaitForSingleObject(pi.hProcess, waitForExitMs);
            if (waitRet == WAIT_OBJECT_0) {
                DWORD exitCode = 0;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                result.exited = true;
                result.exitCode = static_cast<int>(exitCode);
                if (exitCode != 0) {
                    // 失败信封仍带上进程 id 与退出码，调用方据此判断是启动失败还是早退。
                    failure = api::Fail("Process exited early with non-zero exit code", ApiErrorCode::OPERATION_FAILED,
                                        {{"processId", pi.dwProcessId}, {"exited", true}, {"exitCode", static_cast<int>(exitCode)}});
                }
            } else if (waitRet == WAIT_TIMEOUT) {
                result.exited = false;
            } else {
                failure = api::Fail("WaitForSingleObject failed", ApiErrorCode::OPERATION_FAILED,
                                    {{"processId", pi.dwProcessId}});
            }
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (failure) return *failure;
        return result;
    }

} // anonymous namespace

//==========================================================================
// Register Shell API
//==========================================================================
void RegisterShellApi() {
    // showInExplorer 与 openWith 的 path 安全级别（Read）来自声明的 @security 标签。
    api::RegisterApi("shell.showInExplorer", ShellShowInExplorer);
    api::RegisterApi("shell.openWith", ShellOpenWith);
    api::RegisterApi("shell.openExternal", ShellOpenExternal);
    // shell.exec - 无命令白名单, cwd 走 PathSecurity 校验
    api::RegisterApi("shell.exec", ShellExec);
    // shell.spawn - 无可执行白名单, 绝对路径/cwd 走 PathSecurity 校验
    api::RegisterApi("shell.spawn", ShellSpawn);

    LOG("Shell API registered (5 APIs)");
}
