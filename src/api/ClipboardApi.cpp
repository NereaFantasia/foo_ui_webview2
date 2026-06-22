// ClipboardApi.cpp - Clipboard API
// Provides clipboard read/write operations

#include "pch.h"
#include "api/ClipboardApi.h"
#include "api/BridgeCore.h"
#include <shellapi.h>
#include <shlobj.h>

namespace {
    using json = nlohmann::json;
    
    // Get main window handle
    HWND GetMainWindowHandle() {
        HWND hwnd = core_api::get_main_window();
        if (hwnd) return hwnd;
        return GetActiveWindow();
    }
    
    //==========================================================================
    // clipboard.read - Read clipboard content
    //==========================================================================
    json ClipboardRead(const json& /*params*/) {
        json result;
        result["hasText"] = false;
        result["hasImage"] = false;
        result["hasFiles"] = false;
        result["text"] = "";
        
        HWND hwnd = GetMainWindowHandle();
        if (!OpenClipboard(hwnd)) {
            return {{"success", false}, {"error", "Failed to open clipboard"}};
        }
        
        // Check for text
        if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
                if (pszText) {
                    result["text"] = WideToUtf8(pszText);
                    result["hasText"] = true;
                    GlobalUnlock(hData);
                }
            }
        } else if (IsClipboardFormatAvailable(CF_TEXT)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                char* pszText = static_cast<char*>(GlobalLock(hData));
                if (pszText) {
                    result["text"] = pszText;
                    result["hasText"] = true;
                    GlobalUnlock(hData);
                }
            }
        }
        
        // Check for files
        if (IsClipboardFormatAvailable(CF_HDROP)) {
            HANDLE hData = GetClipboardData(CF_HDROP);
            if (hData) {
                HDROP hDrop = static_cast<HDROP>(hData);
                UINT fileCount = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
                if (fileCount > 0) {
                    result["hasFiles"] = true;
                    json files = json::array();
                    
                    for (UINT i = 0; i < fileCount; i++) {
                        wchar_t filePath[MAX_PATH];
                        if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
                            files.push_back(WideToUtf8(filePath));
                        }
                    }
                    
                    result["files"] = files;
                }
            }
        }
        
        // Check for image
        if (IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_BITMAP)) {
            result["hasImage"] = true;
        }
        
        CloseClipboard();
        
        result["success"] = true;
        return result;
    }
    
    //==========================================================================
    // clipboard.write - Write text to clipboard
    //==========================================================================
    json ClipboardWrite(const json& params) {
        std::string text = params.value("text", "");
        
        if (text.empty()) {
            return {{"success", false}, {"error", "text is required"}};
        }
        
        HWND hwnd = GetMainWindowHandle();
        if (!OpenClipboard(hwnd)) {
            return {{"success", false}, {"error", "Failed to open clipboard"}};
        }
        
        EmptyClipboard();
        
        // Convert to wide string
        std::wstring wtext = Utf8ToWide(text);
        size_t size = (wtext.size() + 1) * sizeof(wchar_t);
        
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!hMem) {
            CloseClipboard();
            return {{"success", false}, {"error", "Failed to allocate memory"}};
        }
        
        wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
        if (!pMem) {
            GlobalFree(hMem);
            CloseClipboard();
            return {{"success", false}, {"error", "Failed to lock memory"}};
        }
        memcpy(pMem, wtext.c_str(), size);
        GlobalUnlock(hMem);
        
        if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
            GlobalFree(hMem);
            CloseClipboard();
            return {{"success", false}, {"error", "Failed to set clipboard data"}};
        }
        
        CloseClipboard();
        
        return {{"success", true}};
    }
    
    //==========================================================================
    // clipboard.writeHTML - Write HTML to clipboard
    //==========================================================================
    json ClipboardWriteHTML(const json& params) {
        std::string html = params.value("html", "");
        std::string plainText = params.value("plainText", "");
        
        if (html.empty()) {
            return {{"success", false}, {"error", "html is required"}};
        }
        
        HWND hwnd = GetMainWindowHandle();
        if (!OpenClipboard(hwnd)) {
            return {{"success", false}, {"error", "Failed to open clipboard"}};
        }
        
        EmptyClipboard();
        
        // Register HTML format
        static UINT cfHTML = RegisterClipboardFormatW(L"HTML Format");
        
        // Build CF_HTML format
        // Format: https://docs.microsoft.com/en-us/windows/win32/dataxchg/html-clipboard-format
        std::string header = 
            "Version:0.9\r\n"
            "StartHTML:XXXXXXXXXX\r\n"
            "EndHTML:XXXXXXXXXX\r\n"
            "StartFragment:XXXXXXXXXX\r\n"
            "EndFragment:XXXXXXXXXX\r\n";
        
        std::string prefix = 
            "<!DOCTYPE html>\r\n"
            "<html>\r\n"
            "<body>\r\n"
            "<!--StartFragment-->";
        
        std::string suffix = 
            "<!--EndFragment-->\r\n"
            "</body>\r\n"
            "</html>";
        
        // Calculate positions
        size_t startHTML = header.length();
        size_t startFragment = startHTML + prefix.length();
        size_t endFragment = startFragment + html.length();
        size_t endHTML = endFragment + suffix.length();
        
        // Format position values (10 digits)
        char headerFormatted[256];
        sprintf_s(headerFormatted,
            "Version:0.9\r\n"
            "StartHTML:%010zu\r\n"
            "EndHTML:%010zu\r\n"
            "StartFragment:%010zu\r\n"
            "EndFragment:%010zu\r\n",
            startHTML, endHTML, startFragment, endFragment);
        
        std::string fullHTML = std::string(headerFormatted) + prefix + html + suffix;
        
        // Set HTML format
        bool htmlOk = false;
        size_t htmlSize = fullHTML.size() + 1;
        HGLOBAL hMemHTML = GlobalAlloc(GMEM_MOVEABLE, htmlSize);
        if (hMemHTML) {
            char* pMem = static_cast<char*>(GlobalLock(hMemHTML));
            if (pMem) {
                memcpy(pMem, fullHTML.c_str(), htmlSize);
                GlobalUnlock(hMemHTML);
                if (SetClipboardData(cfHTML, hMemHTML)) {
                    htmlOk = true;
                } else {
                    GlobalFree(hMemHTML);  // Ownership not transferred on failure
                }
            } else {
                GlobalFree(hMemHTML);
            }
        }
        
        // Also set plain text
        bool textOk = false;
        std::string textToSet = plainText.empty() ? html : plainText;
        std::wstring wtext = Utf8ToWide(textToSet);
        size_t textSize = (wtext.size() + 1) * sizeof(wchar_t);
        
        HGLOBAL hMemText = GlobalAlloc(GMEM_MOVEABLE, textSize);
        if (hMemText) {
            wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMemText));
            if (pMem) {
                memcpy(pMem, wtext.c_str(), textSize);
                GlobalUnlock(hMemText);
                if (SetClipboardData(CF_UNICODETEXT, hMemText)) {
                    textOk = true;
                } else {
                    GlobalFree(hMemText);  // Ownership not transferred on failure
                }
            } else {
                GlobalFree(hMemText);
            }
        }
        
        CloseClipboard();
        
        if (!htmlOk && !textOk) {
            return {{"success", false}, {"error", "Failed to write any clipboard format"}};
        }
        return {{"success", true}, {"htmlWritten", htmlOk}, {"textWritten", textOk}};
    }
    
    //==========================================================================
    // clipboard.writeFiles - Write file list to clipboard
    //==========================================================================
    json ClipboardWriteFiles(const json& params) {
        if (!params.contains("paths") || !params["paths"].is_array()) {
            return {{"success", false}, {"error", "paths array is required"}};
        }
        
        std::vector<std::wstring> files;
        for (const auto& path : params["paths"]) {
            files.push_back(Utf8ToWide(path.get<std::string>()));
        }
        
        if (files.empty()) {
            return {{"success", false}, {"error", "paths array is empty"}};
        }
        
        HWND hwnd = GetMainWindowHandle();
        if (!OpenClipboard(hwnd)) {
            return {{"success", false}, {"error", "Failed to open clipboard"}};
        }
        
        EmptyClipboard();
        
        // Calculate total size needed for DROPFILES structure
        size_t totalSize = sizeof(DROPFILES);
        for (const auto& file : files) {
            totalSize += (file.size() + 1) * sizeof(wchar_t);
        }
        totalSize += sizeof(wchar_t);  // Double null terminator
        
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, totalSize);
        if (!hMem) {
            CloseClipboard();
            return {{"success", false}, {"error", "Failed to allocate memory"}};
        }
        
        DROPFILES* pDropFiles = static_cast<DROPFILES*>(GlobalLock(hMem));
        if (pDropFiles) {
            pDropFiles->pFiles = sizeof(DROPFILES);
            pDropFiles->fWide = TRUE;
            
            wchar_t* pData = reinterpret_cast<wchar_t*>(reinterpret_cast<char*>(pDropFiles) + sizeof(DROPFILES));
            
            for (const auto& file : files) {
                memcpy(pData, file.c_str(), (file.size() + 1) * sizeof(wchar_t));
                pData += file.size() + 1;
            }
            *pData = L'\0';  // Double null terminator
            
            GlobalUnlock(hMem);
        }
        
        if (!SetClipboardData(CF_HDROP, hMem)) {
            GlobalFree(hMem);
            CloseClipboard();
            return {{"success", false}, {"error", "Failed to set clipboard data"}};
        }
        
        CloseClipboard();
        
        return {{"success", true}, {"fileCount", files.size()}};
    }
    
} // anonymous namespace

//==========================================================================
// Register Clipboard API
//==========================================================================
void RegisterClipboardApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // clipboard.read - Read clipboard content
    bridge.RegisterApi("clipboard.read", ClipboardRead);
    
    // clipboard.write - Write text to clipboard
    bridge.RegisterApi("clipboard.write", ClipboardWrite);
    
    // clipboard.writeHTML - Write HTML to clipboard
    bridge.RegisterApi("clipboard.writeHTML", ClipboardWriteHTML);
    
    // clipboard.writeFiles - Write file list to clipboard
    bridge.RegisterApi("clipboard.writeFiles", ClipboardWriteFiles, {{"paths", SecurityLevel::Read, true}});
    
    LOG("Clipboard API registered (4 APIs)");
}
