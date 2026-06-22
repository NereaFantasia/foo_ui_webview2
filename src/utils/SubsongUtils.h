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

namespace SubsongUtils {

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
                FB2K_console_formatter() << "foo_ui_webview2: invalid subsong suffix in path: " << path.c_str();
            }
        }
        return { filePath, subsongIndex };
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

        console::printf("lyrics.sidecar: audioPath=%s, subsong=%u, sidecar=%ls",
                        audioPath.c_str(), static_cast<unsigned>(subsong), p.c_str());

        return p.wstring();
    }

} // namespace SubsongUtils
