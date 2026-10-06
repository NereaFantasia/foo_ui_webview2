#pragma once

#include <array>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lyrics_file_policy {

inline constexpr std::array<const char*, 5> kKnownLyricsTags = {
    "LYRICS", "UNSYNCED LYRICS", "UNSYNCEDLYRICS", "SYNCEDLYRICS", "SYNCED LYRICS"
};

inline bool IsWholeFileSingleTrack(std::uint32_t targetSubsong,
                                  std::optional<std::uint32_t> subsongCount,
                                  std::optional<std::uint32_t> firstSubsong) {
    return targetSubsong == 0 && subsongCount == 1 && firstSubsong == 0;
}

inline bool IsValidFilename(std::wstring_view filename) {
    if (filename.empty()) return true;
    if (filename.back() == L' ' || filename.back() == L'.') return false;
    constexpr std::wstring_view invalid = L"\\/:*?\"<>|";
    for (wchar_t c : filename) {
        if (c < 32 || invalid.find(c) != std::wstring_view::npos) return false;
    }

    std::wstring base(filename.substr(0, filename.find(L'.')));
    while (!base.empty() && base.back() == L' ') base.pop_back();
    for (wchar_t& c : base) {
        if (c >= L'a' && c <= L'z') c -= L'a' - L'A';
    }
    if (base == L"CON" || base == L"PRN" || base == L"AUX" || base == L"NUL" ||
        base == L"CONIN$" || base == L"CONOUT$") return false;
    if (base.size() == 4 && (base.starts_with(L"COM") || base.starts_with(L"LPT"))) {
        const wchar_t digit = base.back();
        if ((digit >= L'1' && digit <= L'9') || digit == L'\u00b9' ||
            digit == L'\u00b2' || digit == L'\u00b3') return false;
    }
    return true;
}

struct FileContext {
    std::filesystem::path audioPath;
    bool wholeFileSingleTrack = false;
    std::wstring artist;
    std::wstring title;
};

inline std::vector<std::filesystem::path> BuildFileCandidates(
    const FileContext& track, std::wstring_view filename, std::string_view format) {
    std::vector<std::filesystem::path> candidates;
    const auto directory = track.audioPath.parent_path();
    if (directory.empty() || !IsValidFilename(filename)) return candidates;
    if (!filename.empty()) {
        candidates.push_back(directory / filename);
        return candidates;
    }

    const auto sanitize = [](std::wstring value) {
        for (wchar_t& c : value) {
            if (c < 32 || std::wstring_view(L"\\/:*?\"<>|").find(c) != std::wstring_view::npos) c = L'_';
        }
        return value;
    };
    const std::wstring taggedStem = track.artist.empty() || track.title.empty()
        ? L"" : sanitize(track.artist) + L" - " + sanitize(track.title);
    const auto append = [&](const std::filesystem::path& candidate) {
        if (IsValidFilename(candidate.filename().wstring()) &&
            std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) {
            candidates.push_back(candidate);
        }
    };
    for (const auto* extension : { L".lrc", L".txt" }) {
        if ((format == "lrc" && std::wstring_view(extension) != L".lrc") ||
            (format == "txt" && std::wstring_view(extension) != L".txt")) continue;
        if (track.wholeFileSingleTrack) {
            auto named = track.audioPath;
            append(named.replace_extension(extension));
        }
        if (!taggedStem.empty()) append(directory / (taggedStem + extension));
    }
    return candidates;
}

} // namespace lyrics_file_policy
