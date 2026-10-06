#pragma once
// TrackRow.h - builds the shared track row (api::common::Track, declared in
// src/api/schema/common.ts) from a metadb handle.
//
// Every endpoint that returns whole track rows goes through here, so the
// field rules live in one place:
//   handle        absolutePath plus "|subsong:N" when the subsong is not 0
//   path          the path as foobar2000 stores it (file://, file-relative://, a URL)
//   absolutePath  the native path without a subsong suffix; the stored path
//                 when no native form exists (a remote URL)
//   artist / albumArtist / genre   every value joined with ", " (MetaJoined)
//   artists / albumArtists   every ARTIST / ALBUM ARTIST value in tag order
//                 (MetaValuesRaw), so that the array joined with ", " equals
//                 artist / albumArtist
//   rating        the %rating% statistic when it is 1..5, else the RATING tag
//                 clamped to 0..5 (RatingResolve.h); main thread only, because
//                 the cached titleformat script is shared
//   fileSize      get_filesize(); the SDK's "unknown" value casts to -1
//
// A handle with no cached info (a file foobar2000 never opened) yields a row
// with the identity fields filled and everything else empty or zero.
//
// A caller that builds rows off the main thread fills the identity fields and
// rating on the main thread and the metadata from an info snapshot on the
// worker; BuildTrackRow is the two halves plus rating in one go.
#include "api/MetaAccess.h"
#include "api/RatingResolve.h"
#include "api/generated/CommonSchema.h"
#include "utils/StringUtils.h"
#include "utils/SubsongUtils.h"

#include <cstdint>
#include <format>
#include <string>

// The fields that name a track: handle, path, absolutePath and subsong. Events
// that report tracks without a whole row, such as metadb:changed, take handle
// and subsong from here too, so a page can match them to the rows by handle.
// Main thread: g_get_native_path can hand a non-file:// path to a third-party
// filesystem implementation.
struct TrackIdentity {
    std::string handle;
    std::string path;
    std::string absolutePath;
    t_uint32 subsong = 0;
};

inline TrackIdentity TrackIdentityOf(const metadb_handle_ptr& track) {
    const std::string rawPath = track->get_path();
    // A stored path never carries the suffix, but a handle minted from a
    // "path|subsong:N" string may; strip it so absolutePath stays a plain path.
    auto [rawBasePath, rawPathSubsong] = SubsongUtils::ParseSubsongPath(rawPath);

    pfc::string8 nativePath;
    filesystem::g_get_native_path(rawBasePath.c_str(), nativePath);
    TrackIdentity id;
    id.absolutePath = StringUtils::SafeUtf8(nativePath.get_ptr());
    if (id.absolutePath.empty()) id.absolutePath = StringUtils::SafeUtf8(rawBasePath.c_str());

    id.subsong = track->get_subsong_index();
    if (id.subsong == 0 && rawPathSubsong > 0) id.subsong = rawPathSubsong;

    id.handle = id.absolutePath;
    if (id.subsong > 0 && !id.absolutePath.empty()) id.handle += std::format("|subsong:{}", id.subsong);
    id.path = StringUtils::SafeUtf8(rawPath.c_str());
    return id;
}

// The identity fields plus fileSize. Main thread, as TrackIdentityOf, and
// because get_filesize() reads the mutable file stats.
inline void FillTrackIdentity(api::common::Track& row, const metadb_handle_ptr& track) {
    TrackIdentity id = TrackIdentityOf(track);
    row.handle = std::move(id.handle);
    row.path = std::move(id.path);
    row.absolutePath = std::move(id.absolutePath);
    row.subsong = id.subsong;
    row.fileSize = static_cast<std::int64_t>(track->get_filesize());
}

// Tag and technical fields; reads only the info snapshot, so any thread will
// do. Leaves the identity fields and rating alone.
inline void FillTrackMetadata(api::common::Track& row, const file_info& info) {
    auto getMeta = [&](const char* name) -> std::string {
        const char* value = info.meta_get(name, 0);
        return value ? StringUtils::SafeUtf8(value) : std::string();
    };
    auto getMetaInt = [&](const char* name) -> std::int64_t {
        const char* value = info.meta_get(name, 0);
        return value ? atoi(value) : 0;
    };

    row.title = getMeta("title");
    row.artist = MetaJoined(info, "artist");
    row.artists = MetaValuesRaw(info, "artist");
    row.album = getMeta("album");
    // One lookup for both: joining MetaValuesRaw gives what MetaJoined would (MetaAccess.h).
    row.albumArtists = MetaValuesRaw(info, "album artist");
    row.albumArtist = JoinMetaValues(row.albumArtists, ", ");
    row.genre = MetaJoined(info, "genre");
    row.date = getMeta("date");
    row.trackNumber = getMetaInt("tracknumber");
    row.discNumber = getMetaInt("discnumber");
    row.duration = info.get_length();
    row.bitrate = static_cast<std::int64_t>(info.info_get_bitrate());
    row.sampleRate = static_cast<std::int64_t>(info.info_get_int("samplerate"));
    row.channels = static_cast<std::int64_t>(info.info_get_int("channels"));
    const char* codec = info.info_get("codec");
    row.codec = codec ? StringUtils::SafeUtf8(codec) : std::string();
}

// `info` may be null when the handle has no information loaded.
inline api::common::Track BuildTrackRow(const metadb_handle_ptr& track, const file_info* info) {
    api::common::Track row;
    FillTrackIdentity(row, track);

    if (info == nullptr) {
        row.duration = track->get_length();
        return row;
    }

    FillTrackMetadata(row, *info);
    row.rating = ResolveTrackRating(track, info).value;
    return row;
}

// Reads the handle's cached information itself.
inline api::common::Track BuildTrackRow(const metadb_handle_ptr& track) {
    metadb_info_container::ptr container = track->get_info_ref();
    return BuildTrackRow(track, container.is_valid() ? &container->info() : nullptr);
}
