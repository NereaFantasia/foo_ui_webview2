// ConsoleApi.cpp - Console/Logging API
// Provides logging to foobar2000 console and file-based logging

#include "pch.h"
#include "api/ConsoleApi.h"
#include "api/BridgeCore.h"
#include "api/TypedApi.h"
#include "api/generated/ConsoleSchema.h"
#include "api/generated/LogSchema.h"
#include "utils/PathTraversalSegments.h"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace {
    using json = nlohmann::json;

    // Log file mutex and path
    static std::mutex g_logMutex;
    static std::wstring g_logFilePath;

    //==========================================================================
    // Helper: Get current timestamp string
    //==========================================================================
    std::string GetTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm_buf;
        localtime_s(&tm_buf, &time);

        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    //==========================================================================
    // Helper: Format log message from the declared message / args
    // message wins over args; a string is written as it is, any other JSON
    // value as its JSON text. An empty result means "no message".
    //==========================================================================
    std::string FormatLogMessage(const std::optional<json>& message,
                                 const std::optional<std::vector<json>>& args) {
        if (message) {
            return message->is_string() ? message->get<std::string>() : message->dump();
        }
        if (args) {
            std::ostringstream oss;
            bool first = true;
            for (const auto& arg : *args) {
                if (!first) oss << " ";
                first = false;

                if (arg.is_string()) {
                    oss << arg.get<std::string>();
                } else {
                    oss << arg.dump();
                }
            }
            return oss.str();
        }
        return "";
    }

    //==========================================================================
    // Helper: Write to foobar2000 console
    //==========================================================================
    void WriteToConsole(const std::string& level, const std::string& message) {
        std::string prefix = "[WebView]";
        if (!level.empty() && level != "log") {
            prefix += "[" + level + "]";
        }

        std::string fullMessage = prefix + " " + message;
        console::print(fullMessage.c_str());
    }

    // console.log / warn / error share this: the three params types are the same shape.
    template <class P>
    api::Result<void> ConsoleWrite(const char* level, const P& p) {
        std::string message = FormatLogMessage(p.message, p.args);
        if (message.empty()) {
            return api::Fail("message is required", ApiErrorCode::INVALID_PARAMS);
        }
        WriteToConsole(level, message);
        return api::Ok();
    }

    //==========================================================================
    // Helper: Get log file path
    //==========================================================================
    std::wstring GetLogFilePath() {
        if (g_logFilePath.empty()) {
            // 必须走 core_api::get_profile_path()：字面量 "profile://" 不是可解析
            // 的路径，g_get_display_path 拿它解不出 profile 目录，拼出来的路径打不开，
            // 于是 log.write 全部失败、log.read 又把打不开报成空文件。
            // 同一仓库的 WebViewHost::GetProfileLogPath 是这条的正确写法。
            pfc::string8 profilePath;
            filesystem::g_get_display_path(core_api::get_profile_path(), profilePath);

            // Use proper UTF-8 → UTF-16 conversion (profilePath is UTF-8)
            std::wstring widePath = Utf8ToWide(std::string(profilePath.get_ptr(), profilePath.get_length()));
            widePath += L"\\webview_ui.log";
            g_logFilePath = widePath;
        }
        return g_logFilePath;
    }

    //==========================================================================
    // console.log / console.warn / console.error - Log to foobar2000 console
    //==========================================================================
    api::Result<void> ConsoleLog(const api::console::LogParams& p) {
        return ConsoleWrite("", p);
    }

    api::Result<void> ConsoleWarn(const api::console::WarnParams& p) {
        return ConsoleWrite("WARN", p);
    }

    api::Result<void> ConsoleError(const api::console::ErrorParams& p) {
        return ConsoleWrite("ERROR", p);
    }

    //==========================================================================
    // log.write - Write to log file
    //==========================================================================
    api::Result<api::log::WriteResult> LogWrite(const api::log::WriteParams& p) {
        std::string message = FormatLogMessage(p.message, p.args);
        const std::string& level = p.level;
        bool append = p.append;
        bool timestamp = p.timestamp;

        if (message.empty()) {
            return api::Fail("message is required", ApiErrorCode::INVALID_PARAMS);
        }

        try {
            std::lock_guard<std::mutex> lock(g_logMutex);

            std::wstring logPath = GetLogFilePath();

            // Custom file path support
            if (p.file) {
                const std::string& customFile = *p.file;
                // Validate it's in profile directory
                // 与 GetLogFilePath 同一约束：必须用 core_api::get_profile_path()，
                // 字面量 "profile://" 解不出 profile 目录。
                pfc::string8 profilePath;
                filesystem::g_get_display_path(core_api::get_profile_path(), profilePath);

                // Use proper UTF-8 → UTF-16 conversion (profilePath is UTF-8)
                std::wstring widePath = Utf8ToWide(std::string(profilePath.get_ptr(), profilePath.get_length()));
                std::wstring customFileW = Utf8ToWide(customFile);

                // 安全校验: 禁止路径遍历 + 仅允许 .log/.txt 扩展名 + 过滤 Windows 保留设备名
                bool hasValidExtension = false;
                if (customFile.size() >= 4) {
                    std::string ext = customFile.substr(customFile.size() - 4);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    hasValidExtension = (ext == ".log" || ext == ".txt");
                }
                // 过滤 Windows 保留设备名 (CON, PRN, AUX, NUL, COM1-9, LPT1-9)
                std::string baseName = customFile;
                auto dotPos = baseName.rfind('.');
                if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);
                std::transform(baseName.begin(), baseName.end(), baseName.begin(), ::toupper);
                static const std::vector<std::string> reservedNames = {
                    "CON", "PRN", "AUX", "NUL",
                    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
                };
                bool isReserved = false;
                for (const auto& rn : reservedNames) {
                    if (baseName == rn) { isReserved = true; break; }
                }
                if (path_traversal::IsPlainFilename(customFile) &&
                    hasValidExtension &&
                    !isReserved) {
                    logPath = widePath + L"\\" + customFileW;
                }
            }

            std::ofstream file(logPath, append ? std::ios::app : std::ios::trunc);
            if (!file.is_open()) {
                return api::Fail("Failed to open log file", ApiErrorCode::OPERATION_FAILED);
            }

            if (timestamp) {
                file << "[" << GetTimestamp() << "]";
            }

            // Level prefix
            std::string upperLevel = level;
            std::transform(upperLevel.begin(), upperLevel.end(), upperLevel.begin(), ::toupper);
            file << "[" << upperLevel << "] ";

            file << message << '\n';
            file.close();

            api::log::WriteResult out;
            out.path = pfc::stringcvt::string_utf8_from_wide(logPath.c_str()).get_ptr();
            return out;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // log.read - Read log file contents
    // The generated parser has already rejected a negative line count.
    //==========================================================================
    api::Result<api::log::ReadResult> LogRead(const api::log::ReadParams& p) {
        const std::int64_t lines = p.lines;  // Last N lines

        try {
            std::lock_guard<std::mutex> lock(g_logMutex);

            std::wstring logPath = GetLogFilePath();

            api::log::ReadResult out;
            std::ifstream file(logPath);
            if (!file.is_open()) {
                // A missing file reads as empty, without lines / totalLines.
                return out;
            }

            // Read all lines
            std::vector<std::string> allLines;
            std::string line;
            while (std::getline(file, line)) {
                allLines.push_back(line);
            }
            file.close();

            // Get last N lines
            const std::int64_t total = static_cast<std::int64_t>(allLines.size());
            const std::int64_t startIdx = std::max<std::int64_t>(0, total - lines);
            std::ostringstream oss;
            std::vector<std::string> tail;
            for (std::int64_t i = startIdx; i < total; i++) {
                if (i > startIdx) oss << "\n";
                oss << allLines[static_cast<size_t>(i)];
                tail.push_back(allLines[static_cast<size_t>(i)]);
            }

            out.content = oss.str();
            out.lineCount = total - startIdx;
            out.lines = std::move(tail);
            out.totalLines = total;
            return out;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // log.clear - Clear log file
    //==========================================================================
    api::Result<void> LogClear(const api::log::ClearParams& /*params*/) {
        try {
            std::lock_guard<std::mutex> lock(g_logMutex);

            std::wstring logPath = GetLogFilePath();
            std::ofstream file(logPath, std::ios::trunc);
            file.close();

            return api::Ok();
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

} // anonymous namespace

//==========================================================================
// Register Console/Logging API
//==========================================================================
// Parameters and results come from src/api/schema/console.ts and log.ts through the generated types.
void RegisterConsoleApi() {
    api::RegisterApi("console.log", ConsoleLog);
    api::RegisterApi("console.warn", ConsoleWarn);
    api::RegisterApi("console.error", ConsoleError);
    api::RegisterApi("log.write", LogWrite);
    api::RegisterApi("log.read", LogRead);
    api::RegisterApi("log.clear", LogClear);

    LOG("Console API registered (6 APIs)");
}
