#pragma once
// SlowMediaPath.h - whether opening a media file can block for long (SDK-free).
// Include after the precompiled header, which brings in Windows.h.

#include <string>
#include <string_view>

#include "utils/MediaContainerPath.h"
#include "utils/SubsongPath.h"

namespace slow_media_path {

// A network share, including the \\?\UNC\ spelling. No I/O.
inline bool IsUncPath(std::string_view path) {
    if (path.starts_with(R"(\\?\UNC\)")) return true;
    return path.starts_with(R"(\\)") && !path.starts_with(R"(\\?\)") && !path.starts_with(R"(\\.\)");
}

// Reading such a file first downloads it: a cloud placeholder or an offline-tiered file.
inline bool AttributesNeedRecall(DWORD attrs) {
    constexpr DWORD kRecall =
        FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS | FILE_ATTRIBUTE_RECALL_ON_OPEN | FILE_ATTRIBUTE_OFFLINE;
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & kRecall) != 0;
}

// The path of the file an API path reads, without file:// and |subsong:N, or the container
// of an archive entry. Empty for anything that is not a drive-letter or share path.
inline std::string LocalFilePath(std::string_view apiPath) {
    const std::string physical = fb2k_paths::SplitSubsongPath(media_container_path::Unwrap(apiPath)).path;
    const bool driveRooted =
        physical.size() >= 3 && ((physical[0] >= 'A' && physical[0] <= 'Z') || (physical[0] >= 'a' && physical[0] <= 'z')) &&
        physical[1] == ':' && (physical[2] == '\\' || physical[2] == '/');
    return driveRooted || IsUncPath(physical) ? physical : std::string();
}

// A share or a mapped network drive. The drive type comes from the mount table, without a
// network round trip.
inline bool IsNetworkPath(const std::string& localPath) {
    if (IsUncPath(localPath)) return true;
    const wchar_t root[] = {static_cast<wchar_t>(static_cast<unsigned char>(localPath[0])), L':', L'\\', L'\0'};
    return GetDriveTypeW(root) == DRIVE_REMOTE;
}

// Asks the file system for the attributes of a local path; no file data is read.
inline bool PathNeedsRecall(const std::string& localPath) {
    const int length = MultiByteToWideChar(CP_UTF8, 0, localPath.data(), static_cast<int>(localPath.size()), nullptr, 0);
    if (length <= 0) return false;
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, localPath.data(), static_cast<int>(localPath.size()), wide.data(), length);
    return AttributesNeedRecall(GetFileAttributesW(wide.c_str()));
}

// True for a share, a mapped network drive, or a file that is not on disk yet; reading it
// can take seconds. The attribute probe can itself stall on a dead volume, so call this
// on a worker thread, never on the main thread.
inline bool IsSlow(std::string_view apiPath) {
    const std::string path = LocalFilePath(apiPath);
    if (path.empty()) return false;
    return IsNetworkPath(path) || PathNeedsRecall(path);
}

// True for a file that exists only in the cloud: opening it would download it. Network
// paths are never reported, since probing one can stall. Same thread rule as IsSlow.
inline bool IsOnlineOnly(std::string_view apiPath) {
    const std::string path = LocalFilePath(apiPath);
    return !path.empty() && !IsNetworkPath(path) && PathNeedsRecall(path);
}

}  // namespace slow_media_path
