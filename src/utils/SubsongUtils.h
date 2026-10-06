/**
 * SubsongUtils.h - Shared subsong path parsing utility
 *
 * Extracted from PlaybackApi/AudioApi/LyricsApi to eliminate code duplication.
 * Format: "path/to/file.flac|subsong:N" -> { filePath, subsongIndex (0-based) }
 * The splitting and joining rules themselves live in the SDK-free
 * utils/SubsongPath.h; this header adds the parts that need the SDK.
 */
#pragma once
#include <string>
#include <utility>
#include <foobar2000/SDK/foobar2000.h>
#include "utils/CanonicalHandle.h"
#include "utils/SubsongPath.h"

namespace SubsongUtils {

    namespace detail {
        // CreateCanonicalHandle 与 CreateTrackHandle 注入的两个 SDK 调用。
        inline std::string CanonicalizeWithSdk(const char* in) {
            pfc::string8 out;
            filesystem::g_get_canonical_path(in, out);
            return std::string(out.get_ptr() ? out.get_ptr() : "");
        }

        inline metadb_handle_ptr HandleCreateWithSdk(const char* canonical, t_uint32 subsong) {
            return metadb::get()->handle_create(canonical, subsong);
        }
    } // namespace detail

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
    //
    // path 必须已拆掉 "|subsong:N"：后缀交给规范化会被当成文件名的一部分，建出
    // 指向不存在文件的 handle。调用方给的曲目路径先经 ParseSubsongPath，或直接用
    // 下文的 CreateTrackHandle。
    // canonicalPath 收到交给 handle_create 的规范化结果。
    inline metadb_handle_ptr CreateCanonicalHandle(const char* path, t_uint32 subsong,
                                                   std::string& canonicalPath) {
        return fb2k_paths::CreateCanonicalHandleWith(
            path, subsong, detail::CanonicalizeWithSdk, detail::HandleCreateWithSdk, canonicalPath);
    }

    inline metadb_handle_ptr CreateCanonicalHandle(const char* path, t_uint32 subsong) {
        std::string canonicalPath;
        return CreateCanonicalHandle(path, subsong, canonicalPath);
    }

    inline metadb_handle_ptr CreateCanonicalHandle(const std::string& path, t_uint32 subsong) {
        return CreateCanonicalHandle(path.c_str(), subsong);
    }

    // 按调用方路径建出的 handle 取信息时，缓存缺失或不全就得读文件：get_info_ref() 只给
    // 缓存，而且 SDK 注明它总是返回非空，fb2k 没载入过的文件（不在任何播放列表或媒体库里）
    // 拿到的是空的占位信息，带标签也读不出来。
    // 本函数只在需要读文件时读：在调用线程上同步读盘并返回读到的信息，读不了抛出 SDK 异常。
    // 缓存齐全时返回空指针，表示照用缓存；远程或未识别的路径也返回空指针，免得在主线程上等网络。
    inline metadb_info_container::ptr ReadInfoFromFileIfUncached(const metadb_handle_ptr& handle) {
        metadb_info_container::ptr cached;
        if (handle->get_info_ref(cached) && !cached->isInfoPartial()) return nullptr;
        if (filesystem::g_is_remote_or_unrecognized(handle->get_path())) return nullptr;
        return handle->get_full_info_ref(fb2k::noAbort);
    }

    // 读文件的时机同上；不读时给缓存，远程路径的缓存可能不全甚至是占位信息。
    inline metadb_info_container::ptr GetInfoOrReadFile(const metadb_handle_ptr& handle) {
        if (auto fromFile = ReadInfoFromFileIfUncached(handle); fromFile.is_valid()) return fromFile;
        return handle->get_info_ref();
    }

    // 宿主里拆 "|subsong:N" 的入口，规则见 fb2k_paths::SplitSubsongPath。标记之后
    // 读不出序号（如 "|subsong:abc"）时序号按 0，并在控制台记一行原因；路径本身
    // 不进日志。需要知道有没有后缀的调用方用这个，只要路径与序号的用 ParseSubsongPath。
    inline void LogUnreadableSuffix(const fb2k_paths::SubsongPath& parts) {
        if (parts.hasSuffix && !parts.suffixValid) {
            FB2K_console_formatter() << "foo_ui_webview2: invalid subsong suffix in path";
        }
    }

    inline fb2k_paths::SubsongPath SplitSubsongPath(const std::string& path) {
        fb2k_paths::SubsongPath parts = fb2k_paths::SplitSubsongPath(path);
        LogUnreadableSuffix(parts);
        return parts;
    }

    inline std::pair<std::string, t_uint32> ParseSubsongPath(const std::string& path) {
        fb2k_paths::SubsongPath parts = SplitSubsongPath(path);
        return { std::move(parts.path), parts.subsong };
    }

    // 调用方给的曲目路径（可带 "|subsong:N"）建 handle：拆掉后缀，拆出的路径规范化后
    // 连同序号交给 handle_create，契约本体是 fb2k_paths::CreateTrackHandleWith。读不出
    // 序号时与 SplitSubsongPath 一样按 0 并记一行控制台。收曲目路径的端点用这个；
    // 已经拆好路径与序号的调用方用 CreateCanonicalHandle。
    inline metadb_handle_ptr CreateTrackHandle(const std::string& trackPath) {
        fb2k_paths::SubsongPath parts;
        metadb_handle_ptr track = fb2k_paths::CreateTrackHandleWith(
            trackPath, detail::CanonicalizeWithSdk, detail::HandleCreateWithSdk, parts);
        LogUnreadableSuffix(parts);
        return track;
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
