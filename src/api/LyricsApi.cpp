// LyricsApi.cpp - Lyrics API
// Provides lyrics read/write operations

#include "pch.h"
#include "api/LyricsApi.h"
#include "api/BridgeCore.h"
#include "api/MetadataApi.h"
#include "utils/PathExpansion.h"
#include "utils/PathSecurity.h"
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

    struct LyricsCacheEntry {
        json result;
        std::chrono::steady_clock::time_point cachedAt;
    };

    std::mutex g_lyricsCacheMutex;
    std::unordered_map<std::string, LyricsCacheEntry> g_lyricsCache;

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
        return source + "|" + path;
    }

    bool TryGetCachedLyricsResult(const std::string& path, const std::string& source, json& result) {
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

    void StoreCachedLyricsResult(const std::string& path, const std::string& source, const json& result) {
        std::lock_guard<std::mutex> lock(g_lyricsCacheMutex);
        g_lyricsCache[MakeCacheKey(path, source)] = { result, std::chrono::steady_clock::now() };
    }

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
        if (path.find("|subsong:") != std::string::npos) return "path|subsong";
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
        pfc::string8 canonicalPath;
        filesystem::g_get_canonical_path(filePath.c_str(), canonicalPath);

        auto mdb = metadb::get();
        track = mdb->handle_create(canonicalPath.c_str(), subsong);

        if (api) {
            LOG("[PATH_HOOK] api=%s inputPath=%s pathKind=%s canonicalPath=%s sink=handle_create usedNowPlayingShortcut=false resultKind=%s",
                api, path.c_str(), ClassifyPathKind(path), canonicalPath.c_str(),
                track.is_valid() ? "handle-valid" : "handle-invalid");
        }
        return track;
    }

    // Well-known lyrics tag names (order = priority)
    static const char* const kLyricsTags[] = {
        "LYRICS",
        "UNSYNCED LYRICS",
        "UNSYNCEDLYRICS",
        "SYNCEDLYRICS",
        "SYNCED LYRICS",
    };

    // Fallback: scan all meta fields for any name containing "lyric" (case-insensitive)
    const char* FindLyricsFieldFallback(const file_info& info) {
        for (t_size i = 0, n = info.meta_get_count(); i < n; ++i) {
            const char* name = info.meta_enum_name(i);
            if (!name) continue;
            // case-insensitive substring match for "lyric"
            for (const char* p = name; *p; ++p) {
                if ((p[0] == 'l' || p[0] == 'L') &&
                    (p[1] == 'y' || p[1] == 'Y') &&
                    (p[2] == 'r' || p[2] == 'R') &&
                    (p[3] == 'i' || p[3] == 'I') &&
                    (p[4] == 'c' || p[4] == 'C')) {
                    const char* val = info.meta_enum_value(i, 0);
                    if (val && val[0] != '\0') return val;
                    break;
                }
            }
        }
        return nullptr;
    }

    // Try to read lyrics from any known tag, with fallback scan
    const char* FindEmbeddedLyrics(const file_info& info) {
        for (const char* tag : kLyricsTags) {
            const char* val = info.meta_get(tag, 0);
            if (val && val[0] != '\0') return val;
        }
        return FindLyricsFieldFallback(info);
    }

    // Check if any known lyrics tag exists (without reading value)
    bool HasAnyLyricsTag(const file_info& info) {
        for (const char* tag : kLyricsTags) {
            if (info.meta_get(tag, 0)) return true;
        }
        return FindLyricsFieldFallback(info) != nullptr;
    }

    bool TryReadEmbeddedLyrics(metadb_handle_ptr track, json& result, const std::string& type = "any") {
        if (!track.is_valid()) return false;

        metadb_info_container::ptr infoContainer = track->get_info_ref();
        if (!infoContainer.is_valid()) return false;

        const file_info& info = infoContainer->info();

        if (type == "any") {
            const char* lyrics = FindEmbeddedLyrics(info);
            if (!lyrics) return false;

            result["available"] = true;
            result["source"] = "embedded";
            result["lyrics"] = lyrics;
            result["synced"] = IsSyncedLyrics(lyrics);
            return true;
        }

        // Type-filtered search: try known tags, check content matches requested type
        for (const char* tag : kLyricsTags) {
            const char* val = info.meta_get(tag, 0);
            if (!val || val[0] == '\0') continue;
            bool synced = IsSyncedLyrics(val);
            if ((type == "synced" && synced) || (type == "unsynced" && !synced)) {
                result["available"] = true;
                result["source"] = "embedded";
                result["tagName"] = tag;
                result["lyrics"] = val;
                result["synced"] = synced;
                return true;
            }
        }

        // Fallback scan for type-filtered
        const char* fallback = FindLyricsFieldFallback(info);
        if (fallback) {
            bool synced = IsSyncedLyrics(fallback);
            if ((type == "synced" && synced) || (type == "unsynced" && !synced)) {
                result["available"] = true;
                result["source"] = "embedded";
                result["lyrics"] = fallback;
                result["synced"] = synced;
                return true;
            }
        }

        return false;
    }
    
    //==========================================================================
    // Helper: Get lyrics file path from audio file path (subsong-aware)
    //==========================================================================
    std::wstring GetLyricsFilePath(const std::string& audioPath, const std::wstring& ext = L".lrc") {
        return SubsongUtils::MakeSidecarPath(audioPath, ext);
    }

    // Shared/legacy path (no subsong suffix) for backward-compatible fallback reads
    std::wstring GetSharedLyricsFilePath(const std::string& audioPath, const std::wstring& ext = L".lrc") {
        auto [filePath, subsong] = SubsongUtils::ParseSubsongPath(audioPath);
        fs::path p(Utf8ToWide(filePath));
        return p.replace_extension(ext).wstring();
    }
    
    //==========================================================================
    // lyrics.get - Get lyrics for a track
    //==========================================================================
    json LyricsGet(const json& params) {
        std::string path = params.value("path", "");
        std::string source = params.value("source", "any");    // embedded, file, any
        std::string type = params.value("type", "any");        // synced, unsynced, any
        std::string format = params.value("format", "any");    // lrc, txt, any

        if (path.empty()) {
            // Get current playing track
            auto pc = playback_control::get();
            metadb_handle_ptr track;
            if (!pc->get_now_playing(track)) {
                return {{"success", false}, {"error", "No track playing and no path specified"}};
            }
            path = track->get_path();
        }

        // Cache key includes new params
        std::string cacheKey = source + "|" + type + "|" + format;
        json cached;
        if (TryGetCachedLyricsResult(path, cacheKey, cached)) {
            return cached;
        }

        json result;
        result["success"] = true;
        result["available"] = false;
        result["path"] = path;

        // Try embedded lyrics first (if source is 'embedded' or 'any')
        if (source == "embedded" || source == "any") {
            try {
                auto track = ResolveTrackHandle(path, "lyrics.get");
                if (TryReadEmbeddedLyrics(track, result, type)) {
                    StoreCachedLyricsResult(path, cacheKey, result);
                    return result;
                }
            } catch (...) {}
        }

        // Try external lyrics files (if source is 'file' or 'any')
        if ((source == "file" || source == "any") && IsFilesystemPath(path)) {

            // Helper lambda: try reading a lyrics file by extension
            auto tryReadFile = [&](const std::wstring& ext) -> bool {
                std::wstring filePath = GetLyricsFilePath(path, ext);

                std::wstring pathError;
                if (!PathSecurity::Instance().ValidateMediaAccess(filePath, pathError)) {
                    return false;
                }

                if (!fs::exists(filePath)) return false;

                try {
                    std::ifstream file(filePath, std::ios::in);
                    if (!file.is_open()) return false;

                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::string lyrics = buffer.str();
                    file.close();

                    if (lyrics.empty()) return false;

                    bool synced = IsSyncedLyrics(lyrics);
                    // Apply type filter for file source too
                    if (type == "synced" && !synced) return false;
                    if (type == "unsynced" && synced) return false;

                    result["available"] = true;
                    result["source"] = "file";
                    result["sourcePath"] = WideToUtf8(filePath);
                    result["lyrics"] = lyrics;
                    result["synced"] = synced;
                    return true;
                } catch (...) {
                    return false;
                }
            };

            // Try per-track .lrc first, then shared fallback (if format is "lrc" or "any")
            if (format == "lrc" || format == "any") {
                if (tryReadFile(L".lrc")) {
                    StoreCachedLyricsResult(path, cacheKey, result);
                    return result;
                }
                // Shared fallback for backward compat (M7: existing album.lrc)
                std::wstring sharedLrc = GetSharedLyricsFilePath(path, L".lrc");
                std::wstring perTrackLrc = GetLyricsFilePath(path, L".lrc");
                if (sharedLrc != perTrackLrc && fs::exists(sharedLrc)) {
                    std::wstring pathError;
                    if (PathSecurity::Instance().ValidateMediaAccess(sharedLrc, pathError)) {
                        try {
                            std::ifstream file(sharedLrc, std::ios::in);
                            if (file.is_open()) {
                                std::stringstream buffer;
                                buffer << file.rdbuf();
                                std::string lyrics = buffer.str();
                                file.close();
                                if (!lyrics.empty()) {
                                    bool synced = IsSyncedLyrics(lyrics);
                                    if ((type == "synced" && !synced) || (type == "unsynced" && synced)) {
                                        // type mismatch, skip
                                    } else {
                                        result["available"] = true;
                                        result["source"] = "file";
                                        result["sourcePath"] = WideToUtf8(sharedLrc);
                                        result["lyrics"] = lyrics;
                                        result["synced"] = synced;
                                        StoreCachedLyricsResult(path, cacheKey, result);
                                        return result;
                                    }
                                }
                            }
                        } catch (...) {}
                    }
                }
            }

            // Try .txt (if format is "txt" or "any")
            if (format == "txt" || format == "any") {
                if (tryReadFile(L".txt")) {
                    StoreCachedLyricsResult(path, cacheKey, result);
                    return result;
                }
            }
        }

        StoreCachedLyricsResult(path, cacheKey, result);
        return result;
    }
    
    //==========================================================================
    // Helper: Validate filename has no path separators or traversal
    //==========================================================================
    bool ContainsPathTraversal(const std::string& filename) {
        return filename.find(".\\") != std::string::npos ||
               filename.find("./") != std::string::npos ||
               filename.find("..") != std::string::npos ||
               filename.find('/') != std::string::npos ||
               filename.find('\\') != std::string::npos;
    }

    //==========================================================================
    // Helper: Resolve lyrics output path from audio path + optional filename + format
    //==========================================================================
    std::wstring ResolveLyricsOutputPath(const std::string& audioPath, const std::string& filename,
                                         const std::string& format = "lrc") {
        if (!filename.empty()) {
            // Explicit filename: strip subsong from path to get directory
            auto [filePath, subsong] = SubsongUtils::ParseSubsongPath(audioPath);
            fs::path dir = fs::path(Utf8ToWide(filePath)).parent_path();
            return (dir / Utf8ToWide(filename)).wstring();
        }
        std::wstring ext = (format == "txt") ? L".txt" : L".lrc";
        return SubsongUtils::MakeSidecarPath(audioPath, ext);
    }

    //==========================================================================
    // Helper: Resolve %profile%\lyrics\<stem>.<ext> path for config mode
    //==========================================================================
    std::wstring ResolveConfigLyricsPath(const std::string& audioPath, const std::string& filename,
                                         const std::string& format = "lrc") {
        std::wstring profileDir = PathExpansion::GetProfileDirectory();
        std::wstring lyricsDir = profileDir + L"lyrics\\";
        // Ensure directory exists
        fs::create_directories(lyricsDir);

        if (!filename.empty()) {
            return lyricsDir + Utf8ToWide(filename);
        }
        // Use MakeSidecarPath for per-track naming in config dir
        std::wstring ext = (format == "txt") ? L".txt" : L".lrc";
        std::wstring sidecar = SubsongUtils::MakeSidecarPath(audioPath, ext);
        // Extract just the filename from the sidecar path
        return lyricsDir + fs::path(sidecar).filename().wstring();
    }

    //==========================================================================
    // Helper: Write lyrics to a file (config dir uses different security check)
    //==========================================================================
    json WriteLyricsFile(const std::wstring& lrcPath, const std::string& lyrics,
                         const std::wstring& contextAudioPath = L"") {
        std::wstring pathError;
        if (!PathSecurity::Instance().ValidateMediaWriteAccess(lrcPath, pathError, contextAudioPath)) {
            return {{"success", false}, {"error", "Write access denied: " + WideToUtf8(pathError)}};
        }

        try {
            std::ofstream file(lrcPath, std::ios::out);
            if (!file.is_open()) {
                return {{"success", false}, {"error", "Failed to create lyrics file"}};
            }
            file << lyrics;
            file.close();
            return {{"success", true}, {"savedTo", WideToUtf8(lrcPath)}};
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // lyrics.save - Save lyrics to file, embedded tag, config folder, or any combination
    // target: "file" | "embedded" | "config" | "all" | ["file","embedded","config"]
    //==========================================================================
    json LyricsSave(const json& params) {
        std::string path = params.value("path", "");
        std::string lyrics = params.value("lyrics", "");
        std::string filename = params.value("filename", "");
        std::string tagName = params.value("tagName", "LYRICS"); // for embedded mode
        std::string format = params.value("format", "lrc");      // lrc, txt (for file mode)
        
        if (path.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }
        
        if (lyrics.empty()) {
            return {{"success", false}, {"error", "lyrics is required"}};
        }

        // Parse target — string or array of strings
        std::set<std::string> targets;
        if (params.contains("target") && params["target"].is_array()) {
            for (auto& t : params["target"]) {
                if (t.is_string()) targets.insert(t.get<std::string>());
            }
        } else {
            std::string t = params.value("target", "file");
            if (t == "all") {
                targets = {"file", "embedded", "config"};
            } else {
                targets.insert(t);
            }
        }

        // Validate targets
        static const std::set<std::string> validTargets = {"file", "embedded", "config"};
        for (auto& t : targets) {
            if (validTargets.find(t) == validTargets.end()) {
                return {{"success", false}, {"error", "Invalid target: " + t}};
            }
        }

        // Single target: backward-compatible flat response
        if (targets.size() == 1) {
            const std::string& t = *targets.begin();

            if (t == "file") {
                if (!filename.empty() && ContainsPathTraversal(filename)) {
                    return {{"success", false}, {"error", "Invalid filename: path separators and traversal sequences not allowed"}};
                }
                return WriteLyricsFile(
                    ResolveLyricsOutputPath(path, filename, format),
                    lyrics, Utf8ToWide(path));
            }

            if (t == "embedded") {
                json writeParams;
                writeParams["path"] = path;
                writeParams["tags"] = { {tagName, lyrics} };
                return MetadataWriteTags(writeParams);
            }

            // t == "config"
            if (!filename.empty() && ContainsPathTraversal(filename)) {
                return {{"success", false}, {"error", "Invalid filename: path separators and traversal sequences not allowed"}};
            }
            return WriteLyricsFile(
                ResolveConfigLyricsPath(path, filename, format),
                lyrics, Utf8ToWide(path));
        }

        // Multiple targets: collect results
        json results = json::object();
        bool anySuccess = false;

        if (targets.count("file")) {
            if (!filename.empty() && ContainsPathTraversal(filename)) {
                results["file"] = {{"success", false}, {"error", "Invalid filename: path separators and traversal sequences not allowed"}};
            } else {
                results["file"] = WriteLyricsFile(
                    ResolveLyricsOutputPath(path, filename, format),
                    lyrics, Utf8ToWide(path));
            }
            if (results["file"].value("success", false)) anySuccess = true;
        }

        if (targets.count("embedded")) {
            json writeParams;
            writeParams["path"] = path;
            writeParams["tags"] = { {tagName, lyrics} };
            results["embedded"] = MetadataWriteTags(writeParams);
            if (results["embedded"].value("success", false)) anySuccess = true;
        }

        if (targets.count("config")) {
            if (!filename.empty() && ContainsPathTraversal(filename)) {
                results["config"] = {{"success", false}, {"error", "Invalid filename: path separators and traversal sequences not allowed"}};
            } else {
                results["config"] = WriteLyricsFile(
                    ResolveConfigLyricsPath(path, filename, format),
                    lyrics, Utf8ToWide(path));
            }
            if (results["config"].value("success", false)) anySuccess = true;
        }

        return {{"success", anySuccess}, {"results", results}};
    }
    
    //==========================================================================
    // lyrics.exists - Check if lyrics exist for a track
    //==========================================================================
    json LyricsExists(const json& params) {
        std::string path = params.value("path", "");
        
        if (path.empty()) {
            return {{"success", false}, {"error", "path is required"}};
        }
        
        json sources = json::array();
        bool exists = false;

        // Check embedded lyrics
        try {
            auto track = ResolveTrackHandle(path, "lyrics.exists");
            metadb_info_container::ptr infoContainer = track.is_valid() ? track->get_info_ref() : nullptr;
            if (infoContainer.is_valid()) {
                const file_info& info = infoContainer->info();
                if (HasAnyLyricsTag(info)) {
                    sources.push_back("embedded");
                    exists = true;
                }
            }
        } catch (...) {}

        // Check external LRC file (subsong-aware per-track path)
        if (IsFilesystemPath(path)) {
            std::wstring lrcPath = SubsongUtils::MakeSidecarPath(path, L".lrc");
            if (fs::exists(lrcPath)) {
                sources.push_back("file:" + WideToUtf8(fs::path(lrcPath).filename().wstring()));
                exists = true;
            }

            // Also check for .txt lyrics file (subsong-aware)
            std::wstring txtPath = SubsongUtils::MakeSidecarPath(path, L".txt");
            if (fs::exists(txtPath)) {
                sources.push_back("file:" + WideToUtf8(fs::path(txtPath).filename().wstring()));
                exists = true;
            }
        }
        
        return {
            {"exists", exists},
            {"sources", sources}
        };
    }
    
} // anonymous namespace

//==========================================================================
// Register Lyrics API
//==========================================================================
void RegisterLyricsApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // lyrics.get - Get lyrics for a track
    bridge.RegisterApi("lyrics.get", LyricsGet, {{"path", SecurityLevel::MediaRead}});
    
    // lyrics.save - Save lyrics
    bridge.RegisterApi("lyrics.save", LyricsSave, {{"path", SecurityLevel::MediaWrite}});
    
    // lyrics.exists - Check if lyrics exist
    bridge.RegisterApi("lyrics.exists", LyricsExists, {{"path", SecurityLevel::MediaRead}});
    
    LOG("Lyrics API registered (3 APIs)");
}
