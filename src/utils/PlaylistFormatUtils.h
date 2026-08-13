#pragma once

#include <array>
#include <string>
#include <string_view>
#include <unordered_set>

namespace PlaylistFormatUtils {

using ExtensionSet = std::unordered_set<std::string>;

inline constexpr std::array<std::string_view, 8> kBuiltInExtensions = {
    ".pls", ".m3u", ".m3u8", ".asx", ".wpl", ".xspf", ".fpl", ".cue",
};

constexpr char ToLowerAscii(char value) noexcept {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value + ('a' - 'A'))
        : value;
}

constexpr bool EqualsAsciiIgnoreCase(
    std::string_view lhs,
    std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (ToLowerAscii(lhs[i]) != ToLowerAscii(rhs[i])) {
            return false;
        }
    }
    return true;
}

constexpr bool IsLocalPathScheme(std::string_view scheme) noexcept {
    return EqualsAsciiIgnoreCase(scheme, "file") ||
        EqualsAsciiIgnoreCase(scheme, "file-relative") ||
        EqualsAsciiIgnoreCase(scheme, "archive") ||
        EqualsAsciiIgnoreCase(scheme, "unpack");
}

inline std::string NormalizeExtension(std::string_view extension) {
    if (extension.empty()) {
        return {};
    }

    std::string normalized;
    normalized.reserve(extension.size() + (extension.front() == '.' ? 0u : 1u));
    if (extension.front() != '.') {
        normalized.push_back('.');
    }
    for (const char value : extension) {
        normalized.push_back(ToLowerAscii(value));
    }
    return normalized;
}

inline std::string_view ExtractExtension(std::string_view location) noexcept {
    size_t suffixPos = std::string_view::npos;
    const size_t schemePos = location.find("://");
    if (schemePos != std::string_view::npos &&
        !IsLocalPathScheme(location.substr(0, schemePos))) {
        suffixPos = location.find('?', schemePos + 3);
        const size_t fragmentPos = location.find('#', schemePos + 3);
        if (fragmentPos != std::string_view::npos &&
            (suffixPos == std::string_view::npos || fragmentPos < suffixPos)) {
            suffixPos = fragmentPos;
        }
    }
    const std::string_view path = location.substr(0, suffixPos);
    const size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string_view::npos) {
        return {};
    }

    const size_t separatorPos = path.find_last_of("/\\");
    if (separatorPos != std::string_view::npos && dotPos < separatorPos) {
        return {};
    }

    return path.substr(dotPos);
}

inline bool ExtensionEquals(std::string_view lhs, std::string_view rhs) noexcept {
    if (!lhs.empty() && lhs.front() == '.') {
        lhs.remove_prefix(1);
    }
    if (!rhs.empty() && rhs.front() == '.') {
        rhs.remove_prefix(1);
    }
    if (lhs.size() != rhs.size() || lhs.empty()) {
        return false;
    }

    for (size_t i = 0; i < lhs.size(); ++i) {
        if (ToLowerAscii(lhs[i]) != ToLowerAscii(rhs[i])) {
            return false;
        }
    }
    return true;
}

inline bool LooksLikePlaylistWrapper(
    std::string_view location,
    const ExtensionSet& registeredExtensions) {
    const std::string_view extension = ExtractExtension(location);
    if (extension.empty()) {
        return false;
    }

    for (const std::string_view builtIn : kBuiltInExtensions) {
        if (ExtensionEquals(extension, builtIn)) {
            return true;
        }
    }

    for (const std::string& registered : registeredExtensions) {
        if (ExtensionEquals(registered, extension)) {
            return true;
        }
    }
    return false;
}

bool LooksLikePlaylistWrapper(std::string_view location);

} // namespace PlaylistFormatUtils
