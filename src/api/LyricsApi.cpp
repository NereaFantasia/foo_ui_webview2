// LyricsApi.cpp - Lyrics API
// Provides lyrics read/write operations

#include "pch.h"
#include "api/LyricsApi.h"
#include "api/LyricsFilePolicy.h"
#include "api/BridgeCore.h"
#include "api/MediaReadQueue.h"
#include "api/MetadataApi.h"
#include "api/TypedApi.h"
#include "api/generated/LyricsSchema.h"
#include "domain/PathSecurity.h"
#include "utils/StringUtils.h"
#include "utils/SubsongUtils.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <set>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {
    using json = nlohmann::json;
    namespace lyrics = api::lyrics;

    struct LyricsCacheEntry {
        lyrics::GetResult result;
        std::chrono::steady_clock::time_point cachedAt;
    };

    std::mutex g_lyricsCacheMutex;
    std::unordered_map<std::string, LyricsCacheEntry> g_lyricsCache;
    // Bumped by InvalidateLyricsCache. A lookup that started before a save does not
    // store what it read, so the cache never keeps lyrics older than the save.
    std::uint64_t g_lyricsCacheGeneration = 0;

    constexpr auto LYRICS_CACHE_TTL = std::chrono::milliseconds(1200);
    
    //==========================================================================
    // Helper: Check if lyrics are synced (LRC format)
    //==========================================================================
    bool IsSyncedLyrics(const std::string& lyrics) {
        // LRC format has timestamps like [00:00.00]
        return lyrics.find("[0") != std::string::npos || 
               lyrics.find("[1") != std::string::npos ||
               lyrics.find("[2") != std::string::npos;
    }

    std::string MakeCacheKey(const std::string& path, const std::string& source) {
        return json::array({path, source}).dump();
    }

    bool TryGetCachedLyricsResult(const std::string& path, const std::string& source, lyrics::GetResult& result) {
        std::lock_guard<std::mutex> lock(g_lyricsCacheMutex);
        auto it = g_lyricsCache.find(MakeCacheKey(path, source));
        if (it == g_lyricsCache.end()) return false;

        if ((std::chrono::steady_clock::now() - it->second.cachedAt) > LYRICS_CACHE_TTL) {
            g_lyricsCache.erase(it);
            return false;
        }

        result = it->second.result;
        return true;
    }

    std::uint64_t LyricsCacheGeneration() {
        std::lock_guard<std::mutex> lock(g_lyricsCacheMutex);
        return g_lyricsCacheGeneration;
    }

    // `generation` is LyricsCacheGeneration() from before the lookup read anything.
    void StoreCachedLyricsResult(const std::string& path, const std::string& source, const lyrics::GetResult& result,
                                 std::uint64_t generation) {
        std::lock_guard<std::mutex> lock(g_lyricsCacheMutex);
        if (generation != g_lyricsCacheGeneration) return;
        g_lyricsCache[MakeCacheKey(path, source)] = { result, std::chrono::steady_clock::now() };
    }

    // Superseded by SubsongUtils::TryResolveNativeMediaPath, which decides the same
    // question and hands back the converted path in one step. Kept for reference:
    // note that the fallback below treats any path without "://" as a filesystem
    // path, so a relative path such as "foo/bar.flac" was accepted and then
    // resolved against the process working directory (the foobar2000 install
    // folder). The replacement rejects that form outright.
    bool IsFilesystemPath(const std::string& path) {
        if (path.empty()) return false;
        if (path.rfind("\\\\?\\", 0) == 0 || path.rfind("\\\\", 0) == 0) {
            return true;
        }

        if (path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) &&
            path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
            return true;
        }

        if (path.rfind("file://", 0) == 0) {
            return true;
        }

        return path.find("://") == std::string::npos;
    }

    using SubsongUtils::ParseSubsongPath;

    // Classify path kind for hook evidence
    const char* ClassifyPathKind(const std::string& path) {
        if (path.find("file-relative://") == 0) return "file-relative://";
        if (path.find("file://") == 0) return "file://";
        if (path.find(fb2k_paths::kSubsongMarker) != std::string::npos) return "path|subsong";
        return "native";
    }

    metadb_handle_ptr ResolveTrackHandle(const std::string& path, const char* api = nullptr) {
        metadb_handle_ptr track;
        auto pc = playback_control::get();

        if (path.empty()) {
            pc->get_now_playing(track);
            if (api) {
                LOG("[PATH_HOOK] api=%s usedNowPlayingShortcut=true resultKind=%s",
                    api, track.is_valid() ? "handle-valid" : "handle-invalid");
            }
            return track;
        }

        // Check if path matches now-playing (compare against canonical)
        metadb_handle_ptr nowPlaying;
        if (pc->get_now_playing(nowPlaying) && nowPlaying.is_valid()) {
            // Canonicalize input for comparison
            auto [filePath, subsong] = ParseSubsongPath(path);
            pfc::string8 canonicalInput;
            filesystem::g_get_canonical_path(filePath.c_str(), canonicalInput);
            if (canonicalInput == nowPlaying->get_path() && subsong == nowPlaying->get_subsong_index()) {
                if (api) {
                    LOG("[PATH_HOOK] api=%s inputPath=%s pathKind=%s canonicalPath=%s sink=now-playing usedNowPlayingShortcut=true resultKind=handle-valid",
                        api, path.c_str(), ClassifyPathKind(path), canonicalInput.c_str());
                }
                return nowPlaying;
            }
        }

        // Canonicalize path before handle_create
        auto [filePath, subsong] = ParseSubsongPath(path);
        std::string canonicalPath;
        track = SubsongUtils::CreateCanonicalHandle(filePath.c_str(), subsong, canonicalPath);

        if (api) {
            LOG("[PATH_HOOK] api=%s inputPath=%s pathKind=%s canonicalPath=%s sink=handle_create usedNowPlayingShortcut=false resultKind=%s",
                api, path.c_str(), ClassifyPathKind(path), canonicalPath.c_str(),
                track.is_valid() ? "handle-valid" : "handle-invalid");
        }
        return track;
    }

    bool HasAnyLyricsTag(const file_info& info) {
        for (const char* tag : lyrics_file_policy::kKnownLyricsTags) {
            if (info.meta_exists(tag)) return true;
        }
        return false;
    }

    // Lyrics lookups use the track's tags (embedded lyrics, "<artist> - <title>"
    // naming). Info that cannot be read (missing file, unsupported format) counts
    // as no tags, so the lookup still goes on to the sidecar files.
    metadb_info_container::ptr TryGetTrackInfo(const metadb_handle_ptr& track) {
        if (!track.is_valid()) return nullptr;
        try {
            return SubsongUtils::GetInfoOrReadFile(track);
        } catch (const std::exception&) {
            return nullptr;
        }
    }

    bool TryReadEmbeddedLyrics(const metadb_info_container::ptr& infoContainer, lyrics::GetResult& result,
                               const std::string& type) {
        if (!infoContainer.is_valid()) return false;
        const file_info& info = infoContainer->info();
        for (const char* tag : lyrics_file_policy::kKnownLyricsTags) {
            const char* value = info.meta_get(tag, 0);
            if (!value || value[0] == '\0') continue;
            const bool synced = IsSyncedLyrics(value);
            if ((type == "synced" && !synced) || (type == "unsynced" && synced)) continue;
            result.available = true;
            result.source = "embedded";
            if (type != "any") result.tagName = tag;
            result.lyrics = StringUtils::SafeUtf8(value);
            result.synced = synced;
            return true;
        }
        return false;
    }

    constexpr const char* kInvalidFilenameError =
        "Invalid filename: expected a single Windows filename without reserved names or trailing spaces/dots";

    bool IsValidLyricsFilename(const std::string& filename) {
        // Utf8ToWide treats NUL as the end of input, so reject it before conversion.
        if (filename.find('\0') != std::string::npos) return false;
        const auto wide = Utf8ToWide(filename);
        return (filename.empty() || !wide.empty()) && lyrics_file_policy::IsValidFilename(wide);
    }

    bool IsWholeFileSingleTrack(const std::string& filePath, t_uint32 targetSubsong) {
        if (targetSubsong != 0) return false;
        try {
            pfc::string8 canonical;
            filesystem::g_get_canonical_path(filePath.c_str(), canonical);
            service_ptr_t<input_info_reader> reader;
            abort_callback_dummy abort;
            input_entry::g_open_for_info_read(reader, nullptr, canonical.get_ptr(), abort);
            const t_uint32 count = reader->get_subsong_count();
            const std::optional<std::uint32_t> first = count == 1
                ? std::optional<std::uint32_t>(reader->get_subsong(0)) : std::nullopt;
            return lyrics_file_policy::IsWholeFileSingleTrack(targetSubsong, count, first);
        } catch (const std::exception&) {
            return false;
        }
    }

    // Tags of the track at `path`, or null when they cannot be read. Main thread only:
    // ResolveTrackHandle asks for the playing track.
    metadb_info_container::ptr TryGetTrackInfoForPath(const std::string& path) {
        try {
            return TryGetTrackInfo(ResolveTrackHandle(path));
        } catch (const std::exception&) {
            return nullptr;
        }
    }

    // Without `filename` the names come from the track's tags (`infoContainer`, null when
    // unreadable), and the media file is opened to count its subsongs.
    std::vector<fs::path> BuildLyricsFileCandidates(const std::string& nativeAudioPath,
                                                   const std::string& filename, const std::string& format,
                                                   const metadb_info_container::ptr& infoContainer) {
        const auto [filePath, subsong] = ParseSubsongPath(nativeAudioPath);
        lyrics_file_policy::FileContext context;
        context.audioPath = Utf8ToWide(filePath);
        if (filename.empty()) {
            context.wholeFileSingleTrack = IsWholeFileSingleTrack(filePath, subsong);
            if (infoContainer.is_valid()) {
                const file_info& info = infoContainer->info();
                if (const char* artist = info.meta_get("ARTIST", 0)) context.artist = Utf8ToWide(artist);
                if (const char* title = info.meta_get("TITLE", 0)) context.title = Utf8ToWide(title);
            }
        }
        return lyrics_file_policy::BuildFileCandidates(context, Utf8ToWide(filename), format);
    }

    bool CanReadLyricsFile(const fs::path& candidate) {
        std::wstring pathError;
        return PathSecurity::Instance().ValidateMediaAccess(candidate.wstring(), pathError);
    }

    //==========================================================================
    // lyrics.get - Get lyrics for a track
    //==========================================================================
    // Hands a lookup back to the main thread, where the candidate files' permission
    // check has to run; exceptions answer INTERNAL_ERROR like the other hops.
    template <class F>
    void ContinueOnMainThread(const DeferredResponder& responder, F work) {
        fb2k::inMainThread([responder, work = std::move(work)]() mutable {
            try {
                work();
            } catch (const std::exception& e) {
                responder.SendError(e.what(), ApiErrorCode::INTERNAL_ERROR);
            } catch (...) {
                responder.SendError("Unknown internal error", ApiErrorCode::INTERNAL_ERROR);
            }
        });
    }

    // Candidates that exist on disk, in order. The permission check comes after on the
    // main thread; a file it rejects is dropped without anything about it being reported.
    std::vector<fs::path> ExistingLyricsFiles(const std::string& nativePath, const std::string& filename,
                                              const std::string& format,
                                              const metadb_info_container::ptr& info) {
        std::vector<fs::path> existing;
        for (auto& candidate : BuildLyricsFileCandidates(nativePath, filename, format, info)) {
            try {
                if (fs::exists(candidate)) existing.push_back(std::move(candidate));
            } catch (const std::exception&) {}
        }
        return existing;
    }

    void KeepReadableLyricsFiles(std::vector<fs::path>& candidates) {
        std::erase_if(candidates, [](const fs::path& candidate) { return !CanReadLyricsFile(candidate); });
    }

    // One lyrics.get call as it moves between the main thread and the media read queue.
    struct LyricsGetLookup {
        std::string path;
        std::string cacheKey;
        std::string type;
        std::string format;
        std::string filename;
        bool searchEmbedded = false;
        bool searchFiles = false;
        std::string nativePath;  // set when searchFiles
        metadb_handle_ptr track;  // the tags come from it; invalid when they are not needed
        std::uint64_t cacheGeneration = 0;
        std::vector<fs::path> candidates;
        lyrics::GetResult result;
    };

    void FinishLyricsGet(const LyricsGetLookup& lookup, const DeferredResponder& responder) {
        StoreCachedLyricsResult(lookup.path, lookup.cacheKey, lookup.result, lookup.cacheGeneration);
        api::Send(responder, api::Result<lyrics::GetResult>(lookup.result));
    }

    // The first candidate that reads and matches `type` wins.
    void ReadLyricsFile(LyricsGetLookup& lookup) {
        for (const auto& candidate : lookup.candidates) {
            try {
                std::ifstream file(candidate, std::ios::in);
                if (!file.is_open()) continue;
                std::stringstream buffer;
                buffer << file.rdbuf();
                const std::string text = buffer.str();
                if (text.empty() || file.bad()) continue;
                const bool synced = IsSyncedLyrics(text);
                if ((lookup.type == "synced" && !synced) || (lookup.type == "unsynced" && synced)) continue;
                lookup.result.available = true;
                lookup.result.source = "file";
                lookup.result.sourcePath = WideToUtf8(candidate.wstring());
                lookup.result.lyrics = StringUtils::SafeUtf8(text);
                lookup.result.synced = synced;
                return;
            } catch (const std::exception&) {}
        }
    }

    // Media read queue: tags, embedded lyrics and the candidate files on disk.
    void FindLyrics(const std::shared_ptr<LyricsGetLookup>& lookup, const DeferredResponder& responder) {
        const metadb_info_container::ptr info = TryGetTrackInfo(lookup->track);
        if (lookup->searchEmbedded) {
            try {
                if (TryReadEmbeddedLyrics(info, lookup->result, lookup->type)) {
                    FinishLyricsGet(*lookup, responder);
                    return;
                }
            } catch (const std::exception&) {}
        }
        if (lookup->searchFiles) {
            lookup->candidates =
                ExistingLyricsFiles(lookup->nativePath, lookup->filename, lookup->format, info);
        }
        if (lookup->candidates.empty()) {
            FinishLyricsGet(*lookup, responder);
            return;
        }
        ContinueOnMainThread(responder, [lookup, responder] {
            KeepReadableLyricsFiles(lookup->candidates);
            if (lookup->candidates.empty()) {
                FinishLyricsGet(*lookup, responder);
                return;
            }
            api::RunOnMediaRead(responder, [lookup, responder] {
                ReadLyricsFile(*lookup);
                FinishLyricsGet(*lookup, responder);
            });
        });
    }

    void LyricsGet(const lyrics::GetParams& p, const DeferredResponder& responder) {
        using Answer = api::Result<lyrics::GetResult>;
        auto lookup = std::make_shared<LyricsGetLookup>();
        lookup->filename = p.filename.value_or("");
        if (!IsValidLyricsFilename(lookup->filename)) {
            api::Send(responder, Answer(api::Fail(kInvalidFilenameError, ApiErrorCode::INVALID_PARAMS)));
            return;
        }
        lookup->path = p.path.value_or("");
        if (lookup->path.empty()) {
            metadb_handle_ptr track;
            if (!playback_control::get()->get_now_playing(track) || !track.is_valid()) {
                api::Send(responder, Answer(api::Fail("No track playing and no path specified",
                                                      ApiErrorCode::NO_ACTIVE_ITEM)));
                return;
            }
            lookup->path = fb2k_paths::JoinSubsongPath(track->get_path(), track->get_subsong_index());
        }

        lookup->cacheKey = json::array({p.source, p.type, p.format, lookup->filename}).dump();
        lyrics::GetResult cached;
        if (TryGetCachedLyricsResult(lookup->path, lookup->cacheKey, cached)) {
            api::Send(responder, Answer(std::move(cached)));
            return;
        }
        lookup->cacheGeneration = LyricsCacheGeneration();
        lookup->type = p.type;
        lookup->format = p.format;
        lookup->result.available = false;
        lookup->result.path = lookup->path;

        // ResolveTrackHandle and TryResolveNativeMediaPath are main-thread only.
        lookup->searchEmbedded = p.source == "embedded" || p.source == "any";
        lookup->searchFiles = (p.source == "file" || p.source == "any") &&
                              SubsongUtils::TryResolveNativeMediaPath(lookup->path, lookup->nativePath);
        try {
            if (lookup->searchEmbedded) {
                lookup->track = ResolveTrackHandle(lookup->path, "lyrics.get");
            } else if (lookup->searchFiles && lookup->filename.empty()) {
                lookup->track = ResolveTrackHandle(lookup->nativePath);
            }
        } catch (const std::exception&) {}

        api::RunOnMediaRead(responder, [lookup, responder] { FindLyrics(lookup, responder); });
    }

    //==========================================================================
    // One target's outcome. A failure carries the code the single-target call
    // fails with; in the multi-target result it becomes that target's envelope.
    //==========================================================================
    struct TargetOutcome {
        bool ok = false;
        std::string savedTo;
        std::string error;
        std::string code;
        // The tag write receipt of the embedded target, kept whole because a
        // multi-target result reports it as that target's envelope.
        json receipt;
    };

    TargetOutcome TargetFailure(std::string error, const char* code) {
        TargetOutcome out;
        out.error = std::move(error);
        out.code = code;
        return out;
    }

    json OutcomeToEnvelope(const TargetOutcome& outcome) {
        if (!outcome.receipt.is_null()) return outcome.receipt;
        if (outcome.ok) return json{{"success", true}, {"savedTo", outcome.savedTo}};
        return ApiEnvelope::MakeError(outcome.error, outcome.code.c_str());
    }

    //==========================================================================
    // Helper: Write lyrics to a file
    //==========================================================================
    TargetOutcome WriteLyricsFile(const std::wstring& lrcPath, const std::string& lyricsText,
                                  const std::wstring& contextAudioPath = L"") {
        std::wstring pathError;
        if (!PathSecurity::Instance().ValidateMediaWriteAccess(lrcPath, pathError, contextAudioPath)) {
            // PathSecurity's own message already starts with "Write access denied".
            return TargetFailure(WideToUtf8(pathError), ApiErrorCode::PERMISSION_DENIED);
        }

        try {
            std::ofstream file(lrcPath, std::ios::out);
            if (!file.is_open()) {
                return TargetFailure("Failed to create lyrics file", ApiErrorCode::OPERATION_FAILED);
            }
            file << lyricsText;
            file.close();
            if (!file) {
                return TargetFailure("Failed to write or close lyrics file", ApiErrorCode::OPERATION_FAILED);
            }
            InvalidateLyricsCache();
            TargetOutcome out;
            out.ok = true;
            out.savedTo = WideToUtf8(lrcPath);
            return out;
        } catch (const std::exception& e) {
            return TargetFailure(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    // The embedded target hands the tag to metadata.write and keeps its receipt.
    // A failed receipt without a code gets OPERATION_FAILED so every envelope in
    // `results` carries one.
    TargetOutcome WriteEmbeddedLyrics(const std::string& path, const std::string& tagName,
                                      const std::string& lyricsText) {
        json writeParams;
        writeParams["path"] = path;
        writeParams["tags"] = { {tagName, lyricsText} };
        json receipt = MetadataWriteTags(writeParams);
        TargetOutcome out;
        out.ok = receipt.value("success", false);
        if (!out.ok) {
            out.error = receipt.value("error", std::string("Unknown error"));
            out.code = receipt.value("code", std::string(ApiErrorCode::OPERATION_FAILED));
            receipt["code"] = out.code;
        }
        out.receipt = std::move(receipt);
        return out;
    }

    TargetOutcome WriteFileTarget(const std::string& path, const std::string& filename,
                                  const std::string& format, const std::string& lyricsText,
                                  const std::wstring& contextAudioPath) {
        std::string nativePath;
        if (!SubsongUtils::TryResolveNativeMediaPath(path, nativePath)) {
            return TargetFailure("Cannot write external lyrics for a non-local track", ApiErrorCode::INVALID_PATH);
        }
        const auto candidates = BuildLyricsFileCandidates(
            nativePath, filename, format,
            filename.empty() ? TryGetTrackInfoForPath(nativePath) : metadb_info_container::ptr());
        if (candidates.empty()) {
            return TargetFailure("Cannot derive a lyrics filename; specify filename", ApiErrorCode::INVALID_PARAMS);
        }
        return WriteLyricsFile(candidates.front().wstring(), lyricsText, contextAudioPath);
    }

    //==========================================================================
    // lyrics.save - Save lyrics to file, embedded tag, or both
    //==========================================================================
    api::Result<lyrics::SaveResult> LyricsSave(const lyrics::SaveParams& p) {
        const std::string filename = p.filename.value_or("");
        if (!IsValidLyricsFilename(filename)) return api::Fail(kInvalidFilenameError, ApiErrorCode::INVALID_PARAMS);

        // Security context handed to the write gate. Its same-directory trust rule
        // takes the parent of this value; a "file://D:\..." string makes that throw
        // and the exception is swallowed, so the rule never fires. Give it the
        // native form whenever there is one.
        std::string nativeAudioPath;
        const std::wstring contextAudioPath = Utf8ToWide(
            SubsongUtils::TryResolveNativeMediaPath(p.path, nativeAudioPath) ? nativeAudioPath : p.path);

        // Targets: "all" expands to file and embedded; duplicates collapse.
        static const std::set<std::string> validTargets = {"file", "embedded"};
        std::set<std::string> targets;
        for (const std::string& t : p.target.value_or(std::vector<std::string>{"file"})) {
            if (t == "all") {
                targets = validTargets;
                continue;
            }
            if (!validTargets.contains(t)) {
                return api::Fail("Invalid target: " + t, ApiErrorCode::INVALID_PARAMS);
            }
            targets.insert(t);
        }

        auto write = [&](const std::string& t) -> TargetOutcome {
            if (t == "file") return WriteFileTarget(p.path, filename, p.format, p.lyrics, contextAudioPath);
            return WriteEmbeddedLyrics(p.path, p.tagName, p.lyrics);
        };

        // Single target: flat result
        if (targets.size() == 1) {
            const TargetOutcome outcome = write(*targets.begin());
            if (!outcome.ok) {
                json::object_t extra;
                if (!outcome.receipt.is_null()) {
                    for (const auto& [key, value] : outcome.receipt.items()) {
                        if (key != "success" && key != "error" && key != "code") extra[key] = value;
                    }
                }
                return api::Fail(outcome.error, outcome.code, std::move(extra));
            }
            lyrics::SaveResult result;
            if (outcome.receipt.is_null()) {
                result.savedTo = outcome.savedTo;
                return result;
            }
            const json& r = outcome.receipt;
            if (r.contains("dispatched")) result.dispatched = r["dispatched"].get<bool>();
            if (r.contains("path")) result.path = r["path"].get<std::string>();
            if (r.contains("handlePath")) result.handlePath = r["handlePath"].get<std::string>();
            if (r.contains("subsong")) result.subsong = r["subsong"].get<std::int64_t>();
            if (r.contains("tagsApplied") && r["tagsApplied"].is_object()) {
                std::map<std::string, std::string> applied;
                for (const auto& [key, value] : r["tagsApplied"].items()) {
                    applied[key] = value.is_string() ? value.get<std::string>() : value.dump();
                }
                result.tagsApplied = std::move(applied);
            }
            if (r.contains("tagsSet")) result.tagsSet = r["tagsSet"].get<std::int64_t>();
            if (r.contains("tagsRemoved")) result.tagsRemoved = r["tagsRemoved"].get<std::int64_t>();
            if (r.contains("note")) result.note = r["note"].get<std::string>();
            return result;
        }

        // Multiple targets: every target's own envelope under its name
        std::map<std::string, json> results;
        bool anySuccess = false;
        for (const std::string& t : targets) {
            const TargetOutcome outcome = write(t);
            results[t] = OutcomeToEnvelope(outcome);
            if (outcome.ok) anySuccess = true;
        }

        if (!anySuccess) {
            return api::Fail("No target could be written", ApiErrorCode::OPERATION_FAILED,
                             {{"results", json(results)}});
        }
        lyrics::SaveResult result;
        result.results = std::move(results);
        return result;
    }

    //==========================================================================
    // lyrics.exists - Check if lyrics exist for a track
    //==========================================================================
    void LyricsExists(const lyrics::ExistsParams& p, const DeferredResponder& responder) {
        using Answer = api::Result<lyrics::ExistsResult>;
        std::string filename = p.filename.value_or("");
        if (!IsValidLyricsFilename(filename)) {
            api::Send(responder, Answer(api::Fail(kInvalidFilenameError, ApiErrorCode::INVALID_PARAMS)));
            return;
        }

        // ResolveTrackHandle and TryResolveNativeMediaPath are main-thread only.
        metadb_handle_ptr track;
        try {
            track = ResolveTrackHandle(p.path, "lyrics.exists");
        } catch (const std::exception&) {}
        std::string nativePath;
        if (!SubsongUtils::TryResolveNativeMediaPath(p.path, nativePath)) nativePath.clear();

        api::RunOnMediaRead(responder, [responder, track, nativePath = std::move(nativePath),
                                        filename = std::move(filename)] {
            lyrics::ExistsResult result;
            result.exists = false;
            const metadb_info_container::ptr info = TryGetTrackInfo(track);
            if (info.is_valid() && HasAnyLyricsTag(info->info())) {
                result.sources.emplace_back("embedded");
                result.exists = true;
            }
            std::vector<fs::path> files;
            if (!nativePath.empty()) files = ExistingLyricsFiles(nativePath, filename, "any", info);
            if (files.empty()) {
                api::Send(responder, Answer(std::move(result)));
                return;
            }
            ContinueOnMainThread(responder, [responder, files = std::move(files), result = std::move(result)]() mutable {
                KeepReadableLyricsFiles(files);
                for (const auto& file : files) {
                    result.sources.push_back("file:" + WideToUtf8(file.filename().wstring()));
                    result.exists = true;
                }
                api::Send(responder, Answer(std::move(result)));
            });
        });
    }

} // anonymous namespace

void InvalidateLyricsCache() {
    std::lock_guard<std::mutex> lock(g_lyricsCacheMutex);
    g_lyricsCache.clear();
    ++g_lyricsCacheGeneration;
}

//==========================================================================
// Register Lyrics API
//==========================================================================
void RegisterLyricsApi() {
    // The path security levels come from the declaration (kPathParams). get and exists
    // read tags and sidecar files on the media read queue; save stays synchronous.
    api::RegisterApiDeferred("lyrics.get", LyricsGet);
    api::RegisterApi("lyrics.save", LyricsSave);
    api::RegisterApiDeferred("lyrics.exists", LyricsExists);

    LOG("Lyrics API registered (3 APIs)");
}
