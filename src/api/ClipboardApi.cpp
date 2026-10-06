// ClipboardApi.cpp - Clipboard API
// Provides clipboard read/write operations

#include "pch.h"
#include "api/ClipboardApi.h"
#include "api/TypedApi.h"
#include "api/generated/ClipboardSchema.h"
#include <shellapi.h>
#include <shlobj.h>

namespace {
    namespace cb = api::clipboard;

    // Get main window handle
    HWND GetMainWindowHandle() {
        HWND hwnd = core_api::get_main_window();
        if (hwnd) return hwnd;
        return GetActiveWindow();
    }

    //==========================================================================
    // clipboard.read - Read clipboard content
    //==========================================================================
    api::Result<cb::ReadResult> ClipboardRead(const cb::ReadParams& /*params*/) {
        cb::ReadResult result;

        HWND hwnd = GetMainWindowHandle();
        if (!OpenClipboard(hwnd)) {
            return api::Fail("Failed to open clipboard", ApiErrorCode::OPERATION_FAILED);
        }

        // Check for text
        if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
                if (pszText) {
                    result.text = WideToUtf8(pszText);
                    result.hasText = true;
                    GlobalUnlock(hData);
                }
            }
        } else if (IsClipboardFormatAvailable(CF_TEXT)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                char* pszText = static_cast<char*>(GlobalLock(hData));
                if (pszText) {
                    result.text = pszText;
                    result.hasText = true;
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
                    result.hasFiles = true;
                    std::vector<std::string> files;

                    for (UINT i = 0; i < fileCount; i++) {
                        wchar_t filePath[MAX_PATH];
                        if (DragQueryFileW(hDrop, i, filePath, MAX_PATH)) {
                            files.push_back(WideToUtf8(filePath));
                        }
                    }

                    result.files = std::move(files);
                }
            }
        }

        // Check for image
        if (IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_BITMAP)) {
            result.hasImage = true;
        }

        CloseClipboard();

        return result;
    }

    //==========================================================================
    // clipboard.write - Write text to clipboard
    //==========================================================================
    api::Result<void> ClipboardWrite(const cb::WriteParams& params) {
        HWND hwnd = GetMainWindowHandle();
        if (!OpenClipboard(hwnd)) {
            return api::Fail("Failed to open clipboard", ApiErrorCode::OPERATION_FAILED);
        }

        EmptyClipboard();

        // Convert to wide string
        std::wstring wtext = Utf8ToWide(params.text);
        size_t size = (wtext.size() + 1) * sizeof(wchar_t);

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!hMem) {
            CloseClipboard();
            return api::Fail("Failed to allocate memory", ApiErrorCode::OPERATION_FAILED);
        }

        wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
        if (!pMem) {
            GlobalFree(hMem);
            CloseClipboard();
            return api::Fail("Failed to lock memory", ApiErrorCode::OPERATION_FAILED);
        }
        memcpy(pMem, wtext.c_str(), size);
        GlobalUnlock(hMem);

        if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
            GlobalFree(hMem);
            CloseClipboard();
            return api::Fail("Failed to set clipboard data", ApiErrorCode::OPERATION_FAILED);
        }

        CloseClipboard();

        return api::Ok();
    }

    //==========================================================================
    // clipboard.writeHTML - Write HTML to clipboard
    //==========================================================================
    api::Result<cb::WriteHTMLResult> ClipboardWriteHTML(const cb::WriteHTMLParams& params) {
        const std::string& html = params.html;
        const std::string plainText = params.plainText.value_or("");

        HWND hwnd = GetMainWindowHandle();
        if (!OpenClipboard(hwnd)) {
            return api::Fail("Failed to open clipboard", ApiErrorCode::OPERATION_FAILED);
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
        const std::string& textToSet = plainText.empty() ? html : plainText;
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
            return api::Fail("Failed to write any clipboard format", ApiErrorCode::OPERATION_FAILED);
        }
        cb::WriteHTMLResult result;
        result.htmlWritten = htmlOk;
        result.textWritten = textOk;
        return result;
    }

    //==========================================================================
    // clipboard.writeFiles - Write file list to clipboard
    //==========================================================================
    api::Result<cb::WriteFilesResult> ClipboardWriteFiles(const cb::WriteFilesParams& params) {
        // The declaration requires at least one path; the parser has already enforced it.
        std::vector<std::wstring> files;
        files.reserve(params.paths.size());
        for (const auto& path : params.paths) {
            files.push_back(Utf8ToWide(path));
        }

        HWND hwnd = GetMainWindowHandle();
        if (!OpenClipboard(hwnd)) {
            return api::Fail("Failed to open clipboard", ApiErrorCode::OPERATION_FAILED);
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
            return api::Fail("Failed to allocate memory", ApiErrorCode::OPERATION_FAILED);
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
            return api::Fail("Failed to set clipboard data", ApiErrorCode::OPERATION_FAILED);
        }

        CloseClipboard();

        cb::WriteFilesResult result;
        result.fileCount = static_cast<std::int64_t>(files.size());
        return result;
    }

} // anonymous namespace

//==========================================================================
// Register Clipboard API
//==========================================================================
void RegisterClipboardApi() {
    // The path security level of writeFiles (Read) comes from the declaration's @security tag.
    api::RegisterApi("clipboard.read", ClipboardRead);
    api::RegisterApi("clipboard.write", ClipboardWrite);
    api::RegisterApi("clipboard.writeHTML", ClipboardWriteHTML);
    api::RegisterApi("clipboard.writeFiles", ClipboardWriteFiles);

    LOG("Clipboard API registered (4 APIs)");
}
