#pragma once

#include "api/AlbumIdentity.h"
#include "utils/StringUtils.h"

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

// 专辑聚合保留原始标签；输出行的文字处理集中在此处，便于复用和离线验证。
template <typename Row, typename Album>
inline void FillAlbumRowText(Row& row, const Album& album, bool canonicalIdentity = true) {
    row.albumId = canonicalIdentity ? AlbumId(album.name, album.albumArtist) : std::nullopt;
    row.name = StringUtils::SafeUtf8(album.name);
    row.artist = StringUtils::SafeUtf8(album.albumArtist.empty() ? album.artist : album.albumArtist);
    row.albumArtist = StringUtils::SafeUtf8(album.albumArtist);
    row.year = StringUtils::SafeUtf8(album.year);
    row.genre = StringUtils::SafeUtf8(album.genre);
    row.label = StringUtils::SafeUtf8(album.label);
    row.firstTrackPath = StringUtils::SafeUtf8(album.firstTrackPath);
}

template <typename Row>
inline void FillArtistAlbumRefText(Row& row, std::string_view name, std::string_view albumArtist) {
    row.albumId = AlbumId(name, albumArtist);
    row.name = StringUtils::SafeUtf8(name);
    row.artist = StringUtils::SafeUtf8(albumArtist);
}

template <typename Row>
inline void FillAlbumTrackRef(Row& row, std::int64_t trackNumber, std::string_view path,
                              std::string_view absolutePath, std::uint32_t subsong) {
    row.trackNumber = trackNumber;
    row.path = StringUtils::SafeUtf8(path);
    row.absolutePath = StringUtils::SafeUtf8(absolutePath);
    row.subsong = subsong;
}

// 筛选与用户看见的文字一致，分组键仍由调用方保留原始字节。
inline bool AlbumTextMatchesQuery(std::string_view name, std::string_view artist,
                                  std::string_view query) {
    const auto lower = [](std::string text) {
        for (char& ch : text) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return text;
    };
    const std::string loweredQuery = lower(std::string(query));
    return lower(StringUtils::SafeUtf8(name)).find(loweredQuery) != std::string::npos ||
           lower(StringUtils::SafeUtf8(artist)).find(loweredQuery) != std::string::npos;
}
