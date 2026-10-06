/**
 * SubsongUtils.h - Shared subsong path parsing utility
 *
 * Extracted from PlaybackApi/AudioApi/LyricsApi to eliminate code duplication.
 * Format: "path/to/file.flac|subsong:N" -> { filePath, subsongIndex (0-based) }
 */
#pragma once
#include <string>
#include <utility>
#include <foobar2000/SDK/foobar2000.h>
#include "utils/CanonicalHandle.h"

namespace SubsongUtils {

    // 规范化后再建 handle。metadb::handle_create 不做路径规范化——SDK 的约定是
    // 调用方自己做（metadb.cpp 的 handle_create_replace_path 先
    // g_get_canonical_path 再转 canonical 版本）。跳过这一步的站点会与经
    // process_locations 解析出的同一文件产生两种 metadb 身份，去重与存在性
    // 判定随之失效。全仓所有"路径 → handle"必须经此函数，不得再手写
    // handle_create(path)；也不得用 process_locations 代替规范化，它是会弹
    // 模态进度框的解析器，不是规范化实现。
    //
    // 契约本体是 SDK-free 的 fb2k_paths::CreateCanonicalHandleWith（单测在那里
    // 用 fake 验证"先规范化、再以规范化结果建 handle、subsong 透传"）；这里
    // 只注入真实的 SDK 调用。
    inline metadb_handle_ptr CreateCanonicalHandle(const char* path, t_uint32 subsong) {
        return fb2k_paths::CreateCanonicalHandleWith(
            path, subsong,
            [](const char* in) {
                pfc::string8 out;
                filesystem::g_get_canonical_path(in, out);
                return std::string(out.get_ptr() ? out.get_ptr() : "");
            },
            [](const char* canonical, t_uint32 s) { return metadb::get()->handle_create(canonical, s); });
    }

    inline metadb_handle_ptr CreateCanonicalHandle(const std::string& path, t_uint32 subsong) {
        return CreateCanonicalHandle(path.c_str(), subsong);
    }

    inline std::pair<std::string, t_uint32> ParseSubsongPath(const std::string& path) {
        std::string filePath = path;
        t_uint32 subsongIndex = 0;
        size_t pos = path.find("|subsong:");
        if (pos != std::string::npos) {
            filePath = path.substr(0, pos);
            try {
                subsongIndex = static_cast<t_uint32>(std::stoul(path.substr(pos + 9)));
            } catch (...) {
                // Invalid subsong suffix (e.g. "|subsong:abc")  fall back to 0 with warning
                subsongIndex = 0;
                // Report the failure reason only; the path itself must not be logged.
                FB2K_console_formatter() << "foo_ui_webview2: invalid subsong suffix in path";
            }
        }
        return { filePath, subsongIndex };
    }

    // Normalize a media path into the native form that std::filesystem / Win32 file
    // APIs accept. fb2k's logical path for a local file is "file://D:\dir\x.flac"
    // (two slashes, native backslashes, not escaped); handing that to a file API
    // produces a path containing ':', which is a reserved Win32 filename character.
    //
    // Returns false when the input is not a local filesystem path — the caller must
    // then give up on file IO rather than concatenating and trying anyway. Rejected
    // forms include other protocols (archive://, unpack://, file-relative://, http://),
    // standard three-slash file URIs ("file:///D:/x.flac", unsupported by design) and
    // relative paths.
    //
    // A "|subsong:N" suffix is carried through verbatim; MakeSidecarPath's per-track
    // naming depends on it. The shape check applies to the path in front of it.
    inline bool TryMakeNativeMediaPath(const std::string& in, std::string& out) {
        out.clear();
        if (in.empty()) return false;

        std::string body = in;
        if (in.rfind("file://", 0) == 0) {
            // Byte-for-byte the same rule as the SDK's extract_native_path: drop 7 chars.
            body = in.substr(7);
        } else if (in.find("://") != std::string::npos) {
            return false;
        }

        const std::string base = ParseSubsongPath(body).first;

        const bool driveRooted =
            base.size() >= 3 &&
            ((base[0] >= 'A' && base[0] <= 'Z') || (base[0] >= 'a' && base[0] <= 'z')) &&
            base[1] == ':' && (base[2] == '\\' || base[2] == '/');
        const bool unc = base.size() >= 2 && base[0] == '\\' && base[1] == '\\';

        if (!driveRooted && !unc) return false;

        out = body;
        return true;
    }


    // Same contract as TryMakeNativeMediaPath, plus the one form that only
    // foobar2000 itself can interpret: "file-relative://". A portable install
    // emits it for media sitting on the same volume as the program, and the base
    // directory it is relative to is fb2k's, not something a string rule can know.
    //
    // The pure-string rules run first, so native and "file://" inputs never reach
    // the SDK. Other protocols stay rejected on purpose: archive:// and unpack://
    // resolve to the container file, not to the track, so a sidecar placed beside
    // the resolved path would belong to the wrong file.
    //
    // Main thread only. For non-file:// input g_get_native_path forwards to
    // filesystem_v3::getNativePath, whose implementation may come from a
    // third-party component and carries no threading guarantee.
    inline bool TryResolveNativeMediaPath(const std::string& in, std::string& out) {
        if (TryMakeNativeMediaPath(in, out)) return true;

        out.clear();
        if (in.rfind("file-relative://", 0) != 0) return false;

        const std::string base = ParseSubsongPath(in).first;
        const std::string suffix = in.substr(base.size());

        pfc::string8 resolved;
        try {
            if (!filesystem::g_get_native_path(base.c_str(), resolved)) return false;
        } catch (...) {
            return false;
        }

        const std::string resolvedStr = resolved.get_ptr() ? resolved.get_ptr() : "";
        if (resolvedStr.empty()) return false;

        // Put the resolved value through the same shape check, so an unexpected
        // result cannot slip through as a filesystem path.
        std::string checked;
        if (!TryMakeNativeMediaPath(resolvedStr, checked)) return false;

        out = checked + suffix;
        return true;
    }


    // Generate per-track sidecar path from audio path with optional |subsong:N suffix.
    // - subsong == 0: no numeric suffix (backward compatible with single-track files)
    // - subsong >= 1: append .NN before ext (NN = subsong+1, zero-padded min 2 digits)
    // audioPath: raw path, may contain "|subsong:N"
    // ext: target extension with leading '.' (e.g. L".lrc", L".txt")
    inline std::wstring MakeSidecarPath(const std::string& audioPath, const std::wstring& ext) {
        auto [filePath, subsong] = ParseSubsongPath(audioPath);

        // UTF-8 to wide conversion (MultiByteToWideChar via pch.h <windows.h>)
        std::wstring widePath;
        if (!filePath.empty()) {
            int len = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(),
                                          static_cast<int>(filePath.size()), nullptr, 0);
            if (len > 0) {
                widePath.resize(len);
                MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(),
                                    static_cast<int>(filePath.size()), widePath.data(), len);
            }
        }

        std::filesystem::path p(widePath);

        if (subsong > 0) {
            int trackNum = static_cast<int>(subsong) + 1;
            wchar_t suffix[16];
            if (trackNum < 100)
                swprintf_s(suffix, L".%02d", trackNum);
            else
                swprintf_s(suffix, L".%d", trackNum);
            std::wstring newFilename = p.stem().wstring() + suffix + std::wstring(ext);
            p = p.parent_path() / newFilename;
        } else {
            p.replace_extension(ext);
        }

        return p.wstring();
    }

} // namespace SubsongUtils
