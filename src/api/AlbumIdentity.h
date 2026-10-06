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
// 本文件零 fb2k 依赖，可在 tests 工程离线单测；从 file_info 取首值的适配在
// LibraryApi.cpp。取值用 meta_get(…, 0) 的原始字节，不做 UTF-8 清洗：键要与
// getAlbums 行里的 name、albumArtist 逐字节相等。
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
