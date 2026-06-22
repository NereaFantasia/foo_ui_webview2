// ShellApi.cpp - Shell/Process API
// Provides shell operations and external process launching

#include "pch.h"
#include "api/ShellApi.h"
#include "api/BridgeCore.h"
#include "utils/PathSecurity.h"  // 安全: 统一路径验证
#include <ShlObj.h>
#include <shellapi.h>
#include <filesystem>

namespace {
    using json = nlohmann::json;
    namespace fs = std::filesystem;

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
    
    //==========================================================================
    // shell.showInExplorer - Show file in Windows Explorer
    // 安全: 添加路径验证
    //==========================================================================
    json ShellShowInExplorer(const json& params) {
        std::string pathStr = params.value("path", "");
        
        if (pathStr.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }
        
        std::wstring path = Utf8ToWide(pathStr);
        
        // Use SHOpenFolderAndSelectItems for proper file selection
        PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(path.c_str());
        if (pidl) {
            HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
            ILFree(pidl);
            
            if (SUCCEEDED(hr)) {
                return {{"success", true}};
            }
        }
        
    // Fallback: use explorer /select,path
        std::wstring cmd = L"/select,\"" + path + L"\"";
        HINSTANCE result = ::ShellExecuteW(nullptr, L"open", L"explorer.exe", 
            cmd.c_str(), nullptr, SW_SHOWNORMAL);
        
        if (reinterpret_cast<intptr_t>(result) > 32) {
            return {{"success", true}};
        }
        
        return {{"success", false}, {"error", "Failed to open Explorer"}};
    }
    
    //==========================================================================
    // shell.openWith - Open file with default application
    // 安全: 黑名单模式 - 禁止可执行文件
    // 安全: 添加路径验证
    //==========================================================================
    json ShellOpenWith(const json& params) {
        std::string pathStr = params.value("path", "");
        
        if (pathStr.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }
        
        std::wstring path = Utf8ToWide(pathStr);
        
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
                    return {
                        {"success", false}, 
                        {"error", "Executable files cannot be opened for security reasons"}
                    };
                }
            }
        }
        
        // 通过黑名单检查，让 Windows 决定用什么程序打开
        HINSTANCE result = ::ShellExecuteW(nullptr, L"open", path.c_str(), 
            nullptr, nullptr, SW_SHOWNORMAL);
        
        if (reinterpret_cast<intptr_t>(result) > 32) {
            return {{"success", true}};
        }
        
        // Get error message
        std::string shellErrorMsg;
        switch (reinterpret_cast<intptr_t>(result)) {
            case SE_ERR_FNF:
                shellErrorMsg = "File not found";
                break;
            case SE_ERR_PNF:
                shellErrorMsg = "Path not found";
                break;
            case SE_ERR_ACCESSDENIED:
                shellErrorMsg = "Access denied";
                break;
            case SE_ERR_NOASSOC:
                shellErrorMsg = "No application associated with this file type";
                break;
            default:
                shellErrorMsg = "ShellExecute failed with code " + std::to_string(reinterpret_cast<intptr_t>(result));
        }
        
        return {{"success", false}, {"error", shellErrorMsg}};
    }
    
    //==========================================================================
    // shell.openExternal - Open URL in default browser
    //==========================================================================
    json ShellOpenExternal(const json& params) {
        std::string url = params.value("url", "");
        
        if (url.empty()) {
            return {{"success", false}, {"error", "url is required"}};
        }
        
        // Security check: only allow http/https URLs
        if (!url.starts_with("http://") && !url.starts_with("https://") && !url.starts_with("mailto:")) {
            return {{"success", false}, {"error", "Only http://, https://, and mailto: URLs are allowed"}};
        }
        
        std::wstring wurl = Utf8ToWide(url);
        
        HINSTANCE result = ::ShellExecuteW(nullptr, L"open", wurl.c_str(), 
            nullptr, nullptr, SW_SHOWNORMAL);
        
        if (reinterpret_cast<intptr_t>(result) > 32) {
            return {{"success", true}};
        }
        
        return {{"success", false}, {"error", "Failed to open URL"}};
    }
    
    //==========================================================================
    // shell.exec - Execute command (cwd 路径校验, 不限制命令)
    //==========================================================================
    json ShellExec(const json& params) {
        std::string command = params.value("command", "");
        bool hidden = params.value("hidden", true);
        std::string cwd = params.value("cwd", "");
        
        if (command.empty()) {
            return {{"success", false}, {"error", "command is required"}};
        }
        
        // 不限制可执行命令: 主题来自用户自己或可信来源, 信任边界等同于安装一个
        // foobar2000 组件。命令名白名单 (cmd/powershell/node 等解释器) 既挡不住
        // 恶意主题 (可经 cmd /c、powershell -Command 任意执行), 又绊住正常主题的
        // 直接调用, 属于无效的 security theater。真正的 fail-safe 护栏是 cwd 的
        // 路径校验 (见下) 与 FileApi 的 MediaWrite 路径黑名单。
        
        // Build command line
        std::vector<std::wstring> argStrings;
        if (params.contains("args") && params["args"].is_array()) {
            for (const auto& arg : params["args"]) {
                argStrings.push_back(Utf8ToWide(arg.get<std::string>()));
            }
        }
        
        std::wstring cmdLine = Utf8ToWide(command);
        for (const auto& arg : argStrings) {
            cmdLine += L" " + QuoteArg(arg);
        }
        
        std::wstring wcwd = cwd.empty() ? L"" : Utf8ToWide(cwd);
        
        // 安全校验: cwd 路径必须通过 PathSecurity 验证
        if (!wcwd.empty()) {
            std::wstring cwdError;
            if (!PathSecurity::Instance().ValidatePath(wcwd, cwdError)) {
                return {{"success", false}, {"error", "Invalid cwd: " + WideToUtf8(cwdError)}};
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
            return {
                {"success", false}, 
                {"error", "CreateProcess failed with error " + std::to_string(error)}
            };
        }
        
        // Don't wait for process - return immediately
        // For async execution, we just launch and forget
        
        // Close handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        return {
            {"success", true},
            {"processId", pi.dwProcessId}
        };
    }

    //==========================================================================
    // shell.spawn - Execute process using structured parameters
    // 设计目标: 避免 cmd /c start 的假成功，支持短等待拿到早退退出码
    // 安全: 不限制可执行文件, 绝对路径/cwd 走 PathSecurity 校验
    //==========================================================================
    json ShellSpawn(const json& params) {
        std::string executableUtf8 = params.value("executable", "");
        bool hidden = params.value("hidden", true);
        std::string cwdUtf8 = params.value("cwd", "");
        int waitForExitMs = params.value("waitForExitMs", 0);

        if (executableUtf8.empty()) {
            return {{"success", false}, {"error", "executable is required"}};
        }
        if (waitForExitMs < 0) waitForExitMs = 0;

        std::wstring executable = TrimOuterQuotes(TrimSpace(Utf8ToWide(executableUtf8)));
        if (executable.empty()) {
            return {{"success", false}, {"error", "executable is empty after trim"}};
        }

        // 不限制可执行文件: 见 ShellExec 说明。信任主题作者前提下, 可执行白名单是
        // 无效的 security theater。真正的 fail-safe 护栏是下方绝对路径与 cwd 的
        // PathSecurity 校验, 以及 FileApi 的 MediaWrite 路径黑名单。

        // 绝对路径可执行文件需要通过路径安全校验
        if (IsAbsolutePath(executable)) {
            std::wstring errorMsg;
            if (!PathSecurity::Instance().ValidatePath(executable, errorMsg)) {
                return {{"success", false}, {"error", WideToUtf8(L"Access denied: " + errorMsg)}};
            }
        }

        std::wstring cwd = cwdUtf8.empty() ? L"" : TrimOuterQuotes(TrimSpace(Utf8ToWide(cwdUtf8)));
        if (!cwd.empty()) {
            std::wstring errorMsg;
            if (!PathSecurity::Instance().ValidatePath(cwd, errorMsg)) {
                return {{"success", false}, {"error", WideToUtf8(L"Invalid cwd: " + errorMsg)}};
            }
            // 检查 cwd 目录是否实际存在，避免 CreateProcess error 267
            DWORD attrs = GetFileAttributesW(cwd.c_str());
            if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                return {{"success", false}, {"error", "cwd directory does not exist: " + cwdUtf8}};
            }
        }

        std::vector<std::wstring> argStrings;
        if (params.contains("args") && params["args"].is_array()) {
            for (const auto& arg : params["args"]) {
                argStrings.push_back(Utf8ToWide(arg.get<std::string>()));
            }
        }

        // 命令行格式: "exe" "arg1" "arg2"
        std::wstring cmdLine = QuoteArg(executable);
        for (const auto& arg : argStrings) {
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
            return {
                {"success", false},
                {"error", "CreateProcess failed with error " + std::to_string(error)}
            };
        }

        json result = {
            {"success", true},
            {"processId", pi.dwProcessId}
        };

        if (waitForExitMs > 0) {
            DWORD waitRet = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(waitForExitMs));
            if (waitRet == WAIT_OBJECT_0) {
                DWORD exitCode = 0;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                result["exited"] = true;
                result["exitCode"] = static_cast<int>(exitCode);
                if (exitCode != 0) {
                    result["success"] = false;
                    result["error"] = "Process exited early with non-zero exit code";
                }
            } else if (waitRet == WAIT_TIMEOUT) {
                result["exited"] = false;
            } else {
                result["success"] = false;
                result["error"] = "WaitForSingleObject failed";
            }
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return result;
    }
    
} // anonymous namespace

//==========================================================================
// Register Shell API
//==========================================================================
void RegisterShellApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // shell.showInExplorer - Show file in Explorer
    bridge.RegisterApi("shell.showInExplorer", ShellShowInExplorer, {{"path", SecurityLevel::Read}});
    
    // shell.openWith - Open with default application ( 黑名单模式)
    bridge.RegisterApi("shell.openWith", ShellOpenWith, {{"path", SecurityLevel::Read}});
    
    // shell.openExternal - Open URL in browser
    bridge.RegisterApi("shell.openExternal", ShellOpenExternal);
    
    // shell.exec - Execute command (无命令白名单, cwd 走 PathSecurity 校验)
    bridge.RegisterApi("shell.exec", ShellExec);

    // shell.spawn - Structured process launch (无可执行白名单, 绝对路径/cwd 走 PathSecurity 校验)
    bridge.RegisterApi("shell.spawn", ShellSpawn);
    
    LOG("Shell API registered (5 APIs)");
}
