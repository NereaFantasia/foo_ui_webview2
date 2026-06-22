// ConsoleApi.cpp - Console/Logging API
// Provides logging to foobar2000 console and file-based logging

#include "pch.h"
#include "api/ConsoleApi.h"
#include "api/BridgeCore.h"
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
    // Helper: Format log message from params
    //==========================================================================
    std::string FormatLogMessage(const json& params) {
        if (params.contains("message")) {
            if (params["message"].is_string()) {
                return params["message"].get<std::string>();
            } else {
                return params["message"].dump();
            }
        } else if (params.contains("args") && params["args"].is_array()) {
            std::ostringstream oss;
            bool first = true;
            for (const auto& arg : params["args"]) {
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
    
    //==========================================================================
    // Helper: Get log file path
    //==========================================================================
    std::wstring GetLogFilePath() {
        if (g_logFilePath.empty()) {
            // Get profile directory
            pfc::string8 profilePath;
            filesystem::g_get_display_path("profile://", profilePath);
            
            // Use proper UTF-8 → UTF-16 conversion (profilePath is UTF-8)
            std::wstring widePath = Utf8ToWide(std::string(profilePath.get_ptr(), profilePath.get_length()));
            widePath += L"\\webview_ui.log";
            g_logFilePath = widePath;
        }
        return g_logFilePath;
    }
    
    //==========================================================================
    // console.log - Log message to foobar2000 console
    //==========================================================================
    json ConsoleLog(const json& params) {
        std::string message = FormatLogMessage(params);
        
        if (message.empty()) {
            return {{"success", false}, {"error", "message is required"}};
        }
        
        WriteToConsole("", message);
        return {{"success", true}};
    }
    
    //==========================================================================
    // console.warn - Log warning to foobar2000 console
    //==========================================================================
    json ConsoleWarn(const json& params) {
        std::string message = FormatLogMessage(params);
        
        if (message.empty()) {
            return {{"success", false}, {"error", "message is required"}};
        }
        
        WriteToConsole("WARN", message);
        return {{"success", true}};
    }
    
    //==========================================================================
    // console.error - Log error to foobar2000 console
    //==========================================================================
    json ConsoleError(const json& params) {
        std::string message = FormatLogMessage(params);
        
        if (message.empty()) {
            return {{"success", false}, {"error", "message is required"}};
        }
        
        WriteToConsole("ERROR", message);
        return {{"success", true}};
    }
    
    //==========================================================================
    // log.write - Write to log file
    //==========================================================================
    json LogWrite(const json& params) {
        std::string message = FormatLogMessage(params);
        std::string level = params.value("level", "info");
        bool append = params.value("append", true);
        bool timestamp = params.value("timestamp", true);
        
        if (message.empty()) {
            return {{"success", false}, {"error", "message is required"}};
        }
        
        try {
            std::lock_guard<std::mutex> lock(g_logMutex);
            
            std::wstring logPath = GetLogFilePath();
            
            // Custom file path support
            if (params.contains("file") && params["file"].is_string()) {
                std::string customFile = params["file"].get<std::string>();
                // Validate it's in profile directory
                pfc::string8 profilePath;
                filesystem::g_get_display_path("profile://", profilePath);
                
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
                if (customFile.find("..") == std::string::npos && 
                    customFile.find('/') == std::string::npos &&
                    customFile.find('\\') == std::string::npos &&
                    hasValidExtension &&
                    !isReserved) {
                    logPath = widePath + L"\\" + customFileW;
                }
            }
            
            std::ofstream file(logPath, append ? std::ios::app : std::ios::trunc);
            if (!file.is_open()) {
                return {{"success", false}, {"error", "Failed to open log file"}};
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
            
            return {
                {"success", true},
                {"path", pfc::stringcvt::string_utf8_from_wide(logPath.c_str()).get_ptr()}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }
    
    //==========================================================================
    // log.read - Read log file contents
    //==========================================================================
    json LogRead(const json& params) {
        int lines = params.value("lines", 100);  // Last N lines
        if (lines < 0) {
            return {{"success", false}, {"error", "lines must be non-negative"}};
        }
        
        try {
            std::lock_guard<std::mutex> lock(g_logMutex);
            
            std::wstring logPath = GetLogFilePath();
            
            std::ifstream file(logPath);
            if (!file.is_open()) {
                return {
                    {"success", true},
                    {"content", ""},
                    {"lineCount", 0}
                };
            }
            
            // Read all lines
            std::vector<std::string> allLines;
            std::string line;
            while (std::getline(file, line)) {
                allLines.push_back(line);
            }
            file.close();
            
            // Get last N lines
            int startIdx = std::max(0, (int)allLines.size() - lines);
            std::ostringstream oss;
            json linesArray = json::array();
            for (int i = startIdx; i < allLines.size(); i++) {
                if (i > startIdx) oss << "\n";
                oss << allLines[i];
                linesArray.push_back(allLines[i]);
            }
            
            return {
                {"success", true},
                {"content", oss.str()},
                {"lines", linesArray},
                {"lineCount", (int)(allLines.size() - startIdx)},
                {"totalLines", (int)allLines.size()}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }
    
    //==========================================================================
    // log.clear - Clear log file
    //==========================================================================
    json LogClear(const json& /*params*/) {
        try {
            std::lock_guard<std::mutex> lock(g_logMutex);
            
            std::wstring logPath = GetLogFilePath();
            std::ofstream file(logPath, std::ios::trunc);
            file.close();
            
            return {{"success", true}};
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }
    
} // anonymous namespace

//==========================================================================
// Register Console/Logging API
//==========================================================================
void RegisterConsoleApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // console.log - Standard log
    bridge.RegisterApi("console.log", ConsoleLog);
    
    // console.warn - Warning log
    bridge.RegisterApi("console.warn", ConsoleWarn);
    
    // console.error - Error log
    bridge.RegisterApi("console.error", ConsoleError);
    
    // log.write - Write to file
    bridge.RegisterApi("log.write", LogWrite);
    
    // log.read - Read log file
    bridge.RegisterApi("log.read", LogRead);
    
    // log.clear - Clear log file
    bridge.RegisterApi("log.clear", LogClear);
    
    LOG("Console API registered (6 APIs)");
}
