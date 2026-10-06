#include "pch.h"
#include "media/MediaFile.h"
#include "media/MediaRange.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <wil/resource.h>

namespace media {
namespace {
void CheckRunning() {
    if (detail::ioStopping.load(std::memory_order_acquire)) throw FileError("stopping", ERROR_OPERATION_ABORTED);
}
std::optional<std::uint64_t> ReadPrefixVint(const std::vector<std::uint8_t>& bytes, std::size_t& cursor, bool identifier) {
    if (cursor >= bytes.size() || bytes[cursor] == 0) return std::nullopt;
    unsigned width = 1;
    std::uint8_t mask = 0x80;
    while (!(bytes[cursor] & mask)) { ++width; mask >>= 1; }
    if (width > (identifier ? 4u : 8u) || width > bytes.size() - cursor) return std::nullopt;
    std::uint64_t value = identifier ? bytes[cursor] : bytes[cursor] & (mask - 1);
    for (unsigned i = 1; i < width; ++i) value = (value << 8) | bytes[cursor + i];
    cursor += width;
    return value;
}
std::string EbmlMime(const std::vector<std::uint8_t>& bytes) {
    std::size_t cursor = 4;
    const auto headerSize = ReadPrefixVint(bytes, cursor, false);
    if (!headerSize.has_value() || *headerSize > bytes.size() - cursor) return "application/octet-stream";
    const auto end = cursor + static_cast<std::size_t>(*headerSize);
    while (cursor < end) {
        const auto id = ReadPrefixVint(bytes, cursor, true);
        const auto size = ReadPrefixVint(bytes, cursor, false);
        if (!id.has_value() || !size.has_value() || cursor > end || *size > end - cursor) break;
        if (*id == 0x4282) {
            const std::string_view docType(reinterpret_cast<const char*>(bytes.data() + cursor), static_cast<std::size_t>(*size));
            if (docType == "webm") return "video/webm";
            if (docType == "matroska") return "video/x-matroska";
            break;
        }
        cursor += static_cast<std::size_t>(*size);
    }
    return "application/octet-stream";
}
}

FileError::FileError(std::string reason, unsigned long code)
    : std::runtime_error(reason + " (" + std::to_string(code) + ")"), reason(std::move(reason)), code(code) {}

struct MediaFile::Handle { wil::unique_hfile value; };

MediaFile::MediaFile(const std::wstring& path) : handle_(std::make_unique<Handle>()) {
    // 标签写入可能短暂独占文件。这里只重试共享/锁冲突，不把其他错误拖到截止时间。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    for (;;) {
        CheckRunning();
        handle_->value.reset(CreateFileW(path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr));
        if (handle_->value) break;
        const auto error = GetLastError();
        if ((error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION) ||
            std::chrono::steady_clock::now() >= deadline) throw FileError("open", error);
        Sleep(20);
    }
    const auto identity = Identity();
    // 大小会进入 JSON number，不能超出 JavaScript 可精确表示的整数范围。
    if (identity.size > 9007199254740991ull) throw FileError("size-limit", ERROR_FILE_TOO_LARGE);
}

MediaFile::~MediaFile() = default;

FileIdentity MediaFile::Identity() const {
    CheckRunning();
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(handle_->value.get(), &info)) throw FileError("stat", GetLastError());
    if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) throw FileError("directory", ERROR_DIRECTORY);
    return {(static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow,
            (static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32) | info.ftLastWriteTime.dwLowDateTime};
}

std::wstring MediaFile::FinalPath() const {
    CheckRunning();
    const DWORD required = GetFinalPathNameByHandleW(handle_->value.get(), nullptr, 0, FILE_NAME_NORMALIZED);
    if (!required || required > 32768) throw FileError("final-path", GetLastError());
    std::wstring path(required, L'\0');
    const DWORD written = GetFinalPathNameByHandleW(handle_->value.get(), path.data(), required, FILE_NAME_NORMALIZED);
    if (!written || written >= required) throw FileError("final-path", GetLastError());
    path.resize(written);
    if (path.starts_with(L"\\\\?\\UNC\\")) path = L"\\\\" + path.substr(8);
    else if (path.starts_with(L"\\\\?\\")) path.erase(0, 4);
    return path;
}

std::vector<std::uint8_t> MediaFile::Read(std::uint64_t offset, std::size_t length) const {
    // 必须读满请求长度；短读意味着文件可能变动，不能把残缺数据当作成功的媒体段。
    CheckRunning();
    if (length > 16u * 1024u * 1024u || offset > static_cast<std::uint64_t>(INT64_MAX))
        throw FileError("read-limit", ERROR_FILE_TOO_LARGE);
    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(handle_->value.get(), position, nullptr, FILE_BEGIN)) throw FileError("seek", GetLastError());
    std::vector<std::uint8_t> data(length);
    std::size_t total = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (total < length) {
        CheckRunning();
        DWORD read = 0;
        if (!ReadFile(handle_->value.get(), data.data() + total, static_cast<DWORD>(length - total), &read, nullptr)) {
            const auto error = GetLastError();
            if ((error == ERROR_SHARING_VIOLATION || error == ERROR_LOCK_VIOLATION) &&
                std::chrono::steady_clock::now() < deadline) { Sleep(20); continue; }
            throw FileError("read", error);
        }
        if (!read) throw FileError("short-read", ERROR_HANDLE_EOF);
        total += read;
    }
    return data;
}

std::string MediaFile::MimeType() const {
    const auto size = Identity().size;
    const auto bytes = Read(0, static_cast<size_t>(std::min<std::uint64_t>(size, 4096)));
    const std::string_view data(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    if (data.size() >= 12 && data.substr(4, 4) == "ftyp") {
        const auto brand = data.substr(8, 4);
        if (brand == "qt  ") return "video/quicktime";
        if (brand == "M4A " || brand == "M4B ") return "audio/mp4";
        return "video/mp4";
    }
    if (data.size() >= 4 && data.substr(0, 4) == std::string_view("\x1a\x45\xdf\xa3", 4)) {
        return EbmlMime(bytes);
    }
    if (data.starts_with("fLaC")) return "audio/flac";
    if (data.starts_with("OggS")) return "audio/ogg";
    if (data.size() >= 12 && data.starts_with("RIFF") && data.substr(8, 4) == "WAVE") return "audio/wav";
    if (data.starts_with("ID3")) return "audio/mpeg";
    if (bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff) return "image/jpeg";
    if (data.starts_with(std::string_view("\x89PNG\r\n\x1a\n", 8))) return "image/png";
    if (data.starts_with("WEBVTT")) return "text/vtt";
    return "application/octet-stream";
}

}  // namespace media
