#pragma once
// ============================================================================
// ArtworkCloudDefense.h — 云占位符防御（Cloud Placeholder Defense）
//
// 目标媒体文件或目录封面图置位 FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 时：
//   - 判定为云占位符（OneDrive / 云盘未下载文件）；
//   - 立即返回空图 / 404 占位响应，严格禁止读文件与水合下载；
//   - 支持带有 |subsong:N 后缀的路径，先剥离子曲目后缀再判定 Win32 属性。
//
// SDK-free 设计：单元测试与运行时均可直接包含。
// ============================================================================

#include <windows.h>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "utils/SubsongPath.h"

#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
#endif

namespace artwork_defense {

inline bool IsPlaceholderAttributes(DWORD attrs) noexcept {
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    return (attrs & FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS) != 0;
}

inline std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring wide(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wide.data(), len);
    return wide;
}

/**
 * 核心判定逻辑（支持注入属性获取函数，用于单元测试）。
 */
inline bool IsCloudPlaceholderArtworkTargetCustom(
    const std::string& rawPath,
    const std::function<DWORD(const std::wstring&)>& getFileAttrs)
{
    if (rawPath.empty() || !getFileAttrs) return false;

    // 1. 规范化路径：去掉 file:// 前缀
    std::string nativePath = rawPath;
    if (nativePath.rfind("file://", 0) == 0) {
        nativePath = nativePath.substr(7);
    }

    // 2. 剥离 |subsong:N 后缀，得到物理承载文件路径
    const auto subsongParts = fb2k_paths::SplitSubsongPath(nativePath);
    const std::string& filePath = subsongParts.path;
    if (filePath.empty()) return false;

    const std::wstring wideFilePath = Utf8ToWide(filePath);

    // 3. 判定媒体文件本体
    const DWORD targetAttrs = getFileAttrs(wideFilePath);
    if (IsPlaceholderAttributes(targetAttrs)) {
        return true;
    }

    // 4. 判定所属目录及目录内常用封面图
    const size_t lastSlash = filePath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        const std::wstring wdir = Utf8ToWide(filePath.substr(0, lastSlash));

        // 目录本身是占位符
        const DWORD dirAttrs = getFileAttrs(wdir);
        if (IsPlaceholderAttributes(dirAttrs)) {
            return true;
        }

        // 目录封面图候选列表（cover/folder/front/album + jpg/png）
        static const wchar_t* const kCoverFiles[] = {
            L"cover.jpg", L"cover.png", L"folder.jpg", L"folder.png",
            L"front.jpg", L"front.png", L"album.jpg", L"album.png",
        };
        for (const auto* coverFile : kCoverFiles) {
            const std::wstring fullCoverPath = wdir + L"\\" + coverFile;
            const DWORD cattrs = getFileAttrs(fullCoverPath);
            if (cattrs != INVALID_FILE_ATTRIBUTES && !(cattrs & FILE_ATTRIBUTE_DIRECTORY)) {
                if (IsPlaceholderAttributes(cattrs)) {
                    return true;
                }
            }
        }
    }

    return false;
}

/**
 * 生产环境入口：使用真实 Win32 GetFileAttributesW
 */
inline bool IsCloudPlaceholderArtworkTarget(const std::string& rawPath) {
    return IsCloudPlaceholderArtworkTargetCustom(rawPath, [](const std::wstring& path) {
        return GetFileAttributesW(path.c_str());
    });
}

} // namespace artwork_defense
