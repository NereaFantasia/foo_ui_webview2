// PathExpansion.cpp — 共享路径变量展开实现
// 统一全项目的 %profile% / %component% / %music% / %APPDATA% / %TEMP% 展开

#include "pch.h"
#include "utils/PathExpansion.h"
#include "api/BridgeCore.h"   // Utf8ToWide
#include <ShlObj.h>

namespace PathExpansion {

std::wstring GetProfileDirectory() {
    static std::wstring profileDir = [] {
        pfc::string8 path;
        filesystem::g_get_display_path(core_api::get_profile_path(), path);
        std::wstring dir = Utf8ToWide(path.get_ptr());
        if (!dir.empty() && dir.back() != L'\\')
            dir += L'\\';
        return dir;
    }();
    return profileDir;
}

std::wstring GetComponentDirectory() {
    static std::wstring componentDir = [] {
        std::wstring modulePath(MAX_PATH, L'\0');
        DWORD len = GetModuleFileNameW(core_api::get_my_instance(), modulePath.data(), MAX_PATH);
        modulePath.resize(len);
        if (auto pos = modulePath.find_last_of(L'\\'); pos != std::wstring::npos)
            modulePath = modulePath.substr(0, pos + 1);
        return modulePath;
    }();
    return componentDir;
}

std::wstring GetMusicDirectory() {
    static std::wstring musicDir = [] {
        std::wstring dir;
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Music, 0, nullptr, &path))) {
            dir = path;
            CoTaskMemFree(path);
            if (!dir.empty() && dir.back() != L'\\')
                dir += L'\\';
        }
        return dir;
    }();
    return musicDir;
}

std::wstring GetAppDataDirectory() {
    static std::wstring appDataDir = [] {
        std::wstring dir;
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
            dir = path;
            CoTaskMemFree(path);
            if (!dir.empty() && dir.back() != L'\\')
                dir += L'\\';
        }
        return dir;
    }();
    return appDataDir;
}

std::wstring GetTempDirectory() {
    static std::wstring tempDir = [] {
        std::wstring path(MAX_PATH, L'\0');
        DWORD len = GetTempPathW(MAX_PATH, path.data());
        path.resize(len);
        return path;
    }();
    return tempDir;
}

// Replace all occurrences of a placeholder in a string
static void ReplaceAllOccurrences(std::wstring& str, const std::wstring& from, const std::wstring& to) {
    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::wstring::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }
}

// wide 版本
std::wstring Expand(const std::wstring& path) {
    std::wstring result = path;

    ReplaceAllOccurrences(result, L"%profile%", GetProfileDirectory());
    ReplaceAllOccurrences(result, L"%component%", GetComponentDirectory());
    ReplaceAllOccurrences(result, L"%music%", GetMusicDirectory());

    // Case-insensitive: try uppercase first, then lowercase
    if (result.find(L"%APPDATA%") != std::wstring::npos)
        ReplaceAllOccurrences(result, L"%APPDATA%", GetAppDataDirectory());
    else
        ReplaceAllOccurrences(result, L"%appdata%", GetAppDataDirectory());

    if (result.find(L"%TEMP%") != std::wstring::npos)
        ReplaceAllOccurrences(result, L"%TEMP%", GetTempDirectory());
    else
        ReplaceAllOccurrences(result, L"%temp%", GetTempDirectory());

    return result;
}

// UTF-8 便捷版本
std::wstring Expand(const std::string& pathUtf8) {
    return Expand(Utf8ToWide(pathUtf8));
}

} // namespace PathExpansion
