#pragma once
// ============================================
// AlbumIdentity.h - 媒体库专辑身份的纯值层
// ============================================
//
// 一首曲目归入哪张专辑，由 (专辑名, 专辑艺术家) 这一对值决定：专辑名取 album
// 首值，专辑艺术家取 album artist 首值，没有这个字段时取 artist 首值，两个都
// 没有时为空串。library.getAlbums 的分组、getAlbumTracks 的查找、getStats 的
// totalAlbums 与 getArtists 的 albums 都按这一口径，规则只写在这里。
//
// 本文件零 fb2k 依赖，可在 tests 工程离线单测；取值模板只依赖 meta_get(…, 0)。
// 分组始终保留首值的原始字节；身份包含非法 UTF-8 时，
// 用可逆的 albumId 查找，显示文字不参与身份判定。
#include "utils/StringUtils.h"

#include <optional>
#include <string>
#include <string_view>

// 两个成员都不为空指针，指向调用方传入的字符串（或静态的空串）。
struct AlbumIdentity {
    const char* name;
    const char* albumArtist;
};

// 三个参数是各字段的首值，字段不存在时为 nullptr。album 缺失或为空串的曲目
// 不属于任何专辑。回落只看 album artist 字段在不在：字段在而首值是空串时，
// 专辑艺术家就是空串。album artist 在的时候 artist 不会被读，调用方可以省掉
// 这次字段查找，传 nullptr。
inline std::optional<AlbumIdentity> ResolveAlbumIdentity(const char* album,
                                                         const char* albumArtist,
                                                         const char* artist) {
    if (album == nullptr || *album == '\0') return std::nullopt;
    const char* by = albumArtist != nullptr ? albumArtist : artist;
    return AlbumIdentity{album, by != nullptr ? by : ""};
}

// 专辑键：专辑名、一个 NUL、专辑艺术家。取自标签的两段是 C 字符串，不含 NUL，
// 所以不同的专辑不会拼出同一个键；调用方传来的值若含 NUL，拼出的键至少有两个
// NUL，不会与任何专辑的键相等。
inline std::string AlbumKey(std::string_view name, std::string_view albumArtist) {
    std::string key;
    key.reserve(name.size() + 1 + albumArtist.size());
    key.append(name);
    key.push_back('\0');
    key.append(albumArtist);
    return key;
}

inline std::string AlbumKey(const AlbumIdentity& identity) {
    return AlbumKey(identity.name, identity.albumArtist);
}

// 字段存在但首值为空时不回落；从多值标签取身份只看首值。
template <typename Info>
inline std::optional<AlbumIdentity> ResolveAlbumIdentity(const Info& info) {
    const char* album = info.meta_get("album", 0);
    const char* albumArtist = info.meta_get("album artist", 0);
    const char* artist = albumArtist ? nullptr : info.meta_get("artist", 0);
    return ResolveAlbumIdentity(album, albumArtist, artist);
}

// 正常身份按已有名称查找；非法字节用十六进制保留，避免显示替换字符造成撞键。
inline std::optional<std::string> AlbumId(std::string_view name, std::string_view albumArtist) {
    if (name.empty() || (StringUtils::IsValidUtf8(name) && StringUtils::IsValidUtf8(albumArtist))) {
        return std::nullopt;
    }
    const std::string key = AlbumKey(name, albumArtist);
    constexpr char digits[] = "0123456789abcdef";
    std::string id = "album1:";
    id.reserve(id.size() + key.size() * 2);
    for (const char value : key) {
        const auto byte = static_cast<unsigned char>(value);
        id.push_back(digits[byte >> 4]);
        id.push_back(digits[byte & 0x0f]);
    }
    return id;
}

inline std::optional<std::string> AlbumId(const AlbumIdentity& identity) {
    return AlbumId(identity.name, identity.albumArtist);
}

template <typename Info>
inline std::optional<std::string> AlbumIdOf(const Info& info) {
    const auto identity = ResolveAlbumIdentity(info);
    return identity ? AlbumId(*identity) : std::nullopt;
}

// 返回原始 AlbumKey。拒绝非规范编码，防止畸形标识意外退回按显示名称查询。
inline std::optional<std::string> DecodeAlbumId(std::string_view id) {
    constexpr std::string_view prefix = "album1:";
    if (!id.starts_with(prefix)) return std::nullopt;
    id.remove_prefix(prefix.size());
    if (id.size() < 4 || id.size() % 2 != 0) return std::nullopt;

    const auto hexValue = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        return -1;
    };
    std::string key;
    key.reserve(id.size() / 2);
    for (size_t i = 0; i < id.size(); i += 2) {
        const int high = hexValue(id[i]);
        const int low = hexValue(id[i + 1]);
        if (high < 0 || low < 0) return std::nullopt;
        key.push_back(static_cast<char>((high << 4) | low));
    }
    const size_t separator = key.find('\0');
    if (separator == 0 || separator == std::string::npos ||
        key.find('\0', separator + 1) != std::string::npos) {
        return std::nullopt;
    }
    return key;
}

inline std::optional<std::string> ResolveAlbumLookupKey(
    std::string_view name, std::string_view albumArtist,
    const std::optional<std::string>& albumId) {
    return albumId ? DecodeAlbumId(*albumId) : std::optional<std::string>(AlbumKey(name, albumArtist));
}
