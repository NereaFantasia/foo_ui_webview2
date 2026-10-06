#include "pch.h"
#include "api/TitleformatApi.h"
#include "api/BridgeCore.h"
#include "api/TypedApi.h"
#include "api/ErrorEnvelope.h"
#include "api/generated/TitleformatSchema.h"
#include "utils/SubsongUtils.h"
#include <algorithm>

// ============================================
// Helper Functions
// ============================================

// Unique separator for merging multiple titleformat fields.
// Uses a sequence extremely unlikely to appear in real metadata.
static constexpr const char* kFieldSeparator = "\x01\x1F\x01";

// Render one track. metadb_handle::format_title reads only foobar2000's cache, where a local
// file foobar2000 has not loaded (in no playlist or library) has a placeholder file_info, so
// such files are rendered from the info read out of the file instead. Returns false when the
// output still came from a placeholder file_info: a remote path with nothing cached, or a file
// that cannot be read. Tag-derived text is untrustworthy then. The file is read for the
// handle's own subsong (metadb_handle::get_full_info_ref), so the handle must carry it.
static bool FormatTrack(const metadb_handle_ptr& track, const titleformat_object::ptr& script,
                        pfc::string_base& out) {
    try {
        if (auto fromFile = SubsongUtils::ReadInfoFromFileIfUncached(track); fromFile.is_valid()) {
            track->format_title_from_external_info(fromFile->info(), nullptr, out, script, nullptr);
            return true;
        }
    } catch (const std::exception&) {
        // An unreadable file is rendered from the placeholder below and reported as false.
    }
    return track->format_title(nullptr, out, script, nullptr);
}

// Evaluate a single titleformat pattern for a track. outInfoAvailable (optional) receives
// FormatTrack's result; it is also false when the track or script is invalid, so it is always
// written before the function returns.
static std::string EvalPattern(const metadb_handle_ptr& track, const titleformat_object::ptr& script,
                               bool* outInfoAvailable = nullptr) {
    if (outInfoAvailable) *outInfoAvailable = false;
    if (!track.is_valid() || !script.is_valid()) return "";

    pfc::string8 result;
    const bool infoAvailable = FormatTrack(track, script, result);
    if (outInfoAvailable) *outInfoAvailable = infoAvailable;
    return result.get_ptr();
}

// Create metadb_handle from a track path; a "|subsong:N" suffix selects the subsong, so a CUE
// track is evaluated as itself rather than as the first track of its sheet.
static metadb_handle_ptr GetHandleFromPath(const std::string& path) {
    if (path.empty()) return nullptr;
    return SubsongUtils::CreateTrackHandle(path);
}

// Split string by delimiter
static std::vector<std::string> SplitString(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    result.push_back(str.substr(start));
    
    return result;
}

// ============================================
// API Implementations
// ============================================

//==========================================================================
// titleformat.eval - Evaluate a single pattern for a single file
// Params and result are declared in src/api/schema/titleformat.ts; the
// generated parser has already rejected an empty path and a missing or empty
// pattern. Without a path the playing track is evaluated.
//==========================================================================
api::Result<api::titleformat::EvalResult> TitleformatEval(const api::titleformat::EvalParams& p) {
    const std::string& pattern = p.pattern;

    try {
        // Compile pattern
        static_api_ptr_t<titleformat_compiler> compiler;
        titleformat_object::ptr script;

        if (!compiler->compile(script, pattern.c_str())) {
            return api::Fail("Invalid titleformat pattern", ApiErrorCode::INVALID_PARAMS);
        }

        api::titleformat::EvalResult out;
        out.pattern = pattern;

        if (!p.path) {
            // The playback formatter also fills the dynamic fields that path-based
            // evaluation leaves empty: %playback_time%, %isplaying%, stream titles.
            auto pc = playback_control::get();
            metadb_handle_ptr playing;
            pfc::string8 formatted;
            if (!pc->get_now_playing(playing) ||
                !pc->playback_format_title(nullptr, formatted, script, nullptr,
                                           playback_control::display_level_all)) {
                return api::Fail("No track is playing", ApiErrorCode::NO_ACTIVE_ITEM);
            }
            out.path = playing->get_path();
            out.result = formatted.get_ptr();
            out.infoAvailable = true;
            return out;
        }
        const std::string& path = *p.path;

        // Get track handle
        metadb_handle_ptr handle = GetHandleFromPath(path);
        if (!handle.is_valid()) {
            return api::Fail("Failed to open file", ApiErrorCode::INVALID_PATH, {{"path", path}});
        }

        out.path = path;
        out.result = EvalPattern(handle, script, &out.infoAvailable);
        return out;
    } catch (const std::exception& e) {
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}

//==========================================================================
// titleformat.evalBatch - Evaluate a single pattern for multiple files
// Params and result are declared in src/api/schema/titleformat.ts.
//==========================================================================
api::Result<api::titleformat::EvalBatchResult> TitleformatEvalBatch(const api::titleformat::EvalBatchParams& p) {
    const std::string& pattern = p.pattern;

    try {
        // Compile pattern once (reuse for all files)
        static_api_ptr_t<titleformat_compiler> compiler;
        titleformat_object::ptr script;

        if (!compiler->compile(script, pattern.c_str())) {
            return api::Fail("Invalid titleformat pattern", ApiErrorCode::INVALID_PARAMS);
        }

        const auto& paths = p.paths;
        api::titleformat::EvalBatchResult out;
        out.pattern = pattern;
        out.total = static_cast<std::int64_t>(paths.size());

        for (const std::string& path : paths) {
            // Not GetHandleFromPath: batch rows do not reject an empty path.
            metadb_handle_ptr handle = SubsongUtils::CreateTrackHandle(path);

            api::titleformat::EvalBatchRow row;
            row.path = path;
            if (!handle.is_valid()) {
                row.error = "Failed to open file";
                out.results.push_back(std::move(row));
                out.errorCount++;
                continue;
            }

            bool infoAvailable = false;
            row.success = true;
            row.result = EvalPattern(handle, script, &infoAvailable);
            row.infoAvailable = infoAvailable;
            out.results.push_back(std::move(row));
            out.successCount++;
        }

        return out;
    } catch (const std::exception& e) {
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}

//==========================================================================
// titleformat.evalFields - Evaluate multiple fields for a single file
// Params and result are declared in src/api/schema/titleformat.ts.
//   All fields are merged into one script, so the single infoAvailable flag
//   cannot separate "%bitrate% is untrustworthy" from "%rating% is fine".
//   It is omitted when no format_title call happened (empty fields or a
//   merged-pattern compile failure).
//==========================================================================
api::Result<api::titleformat::EvalFieldsResult> TitleformatEvalFields(const api::titleformat::EvalFieldsParams& p) {
    const std::string& path = p.path;

    try {
        // Get track handle
        metadb_handle_ptr handle = GetHandleFromPath(path);
        if (!handle.is_valid()) {
            return api::Fail("Failed to open file", ApiErrorCode::INVALID_PATH, {{"path", path}});
        }

        static_api_ptr_t<titleformat_compiler> compiler;

        // 合并所有字段为单个 titleformat 脚本
        std::vector<std::string> fieldNames;
        std::string mergedPattern;

        for (const auto& [fieldName, pattern] : p.fields) {
            if (!mergedPattern.empty()) {
                mergedPattern += kFieldSeparator;
            }
            mergedPattern += pattern;
            fieldNames.push_back(fieldName);
        }

        api::titleformat::EvalFieldsResult out;
        out.path = path;

        if (!fieldNames.empty()) {
            titleformat_object::ptr mergedScript;
            if (compiler->compile(mergedScript, mergedPattern.c_str())) {
                pfc::string8 formatted;
                out.infoAvailable = FormatTrack(handle, mergedScript, formatted);
                // 字段值进 additional：调用方取名 path / infoAvailable 的字段被同名的具名字段覆盖，
                // 取名 success 的被信封覆盖（生成的 ToJson 先写额外键）
                std::vector<std::string> parts = SplitString(formatted.c_str(), kFieldSeparator);
                for (size_t i = 0; i < fieldNames.size(); i++) {
                    out.additional[fieldNames[i]] = (i < parts.size()) ? parts[i] : "";
                }
            }
        }

        return out;
    } catch (const std::exception& e) {
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}

//==========================================================================
// titleformat.evalFieldsBatch - Evaluate multiple fields for multiple files
// Params and result are declared in src/api/schema/titleformat.ts. Each
// row's infoAvailable has the same merged-script limitation as evalFields.
//==========================================================================
api::Result<api::titleformat::EvalFieldsBatchResult> TitleformatEvalFieldsBatch(
    const api::titleformat::EvalFieldsBatchParams& p) {
    try {
        const auto& paths = p.paths;
        static_api_ptr_t<titleformat_compiler> compiler;

        // 合并所有字段模式为单个 ||| 分隔的 titleformat 脚本
        // 100 路径 × 10 字段: 从 1000 次 format_title() 减少到 100 次 (~10× 提速)
        std::vector<std::string> fieldNames;
        std::string mergedPattern;

        for (const auto& [fieldName, pattern] : p.fields) {
            if (!mergedPattern.empty()) {
                mergedPattern += kFieldSeparator;
            }
            mergedPattern += pattern;
            fieldNames.push_back(fieldName);
        }

        api::titleformat::EvalFieldsBatchResult out;
        if (fieldNames.empty()) {
            return out;
        }

        // 编译合并后的脚本（只编译一次）
        titleformat_object::ptr mergedScript;
        if (!compiler->compile(mergedScript, mergedPattern.c_str())) {
            return api::Fail("Failed to compile merged pattern", ApiErrorCode::INVALID_PARAMS);
        }

        out.total = static_cast<std::int64_t>(paths.size());

        for (const std::string& path : paths) {
            metadb_handle_ptr handle = SubsongUtils::CreateTrackHandle(path);

            api::titleformat::EvalFieldsBatchRow row;
            row.path = path;
            if (!handle.is_valid()) {
                row.error = "Failed to open file";
                out.results.push_back(std::move(row));
                out.errorCount++;
                continue;
            }

            // 单次 format_title 调用获取所有字段
            pfc::string8 formatted;
            row.success = true;
            row.infoAvailable = FormatTrack(handle, mergedScript, formatted);

            // Split 并映射回字段名；与 evalFields 相同，同名的具名字段胜过调用方字段
            std::vector<std::string> parts = SplitString(formatted.c_str(), kFieldSeparator);
            for (size_t i = 0; i < fieldNames.size(); i++) {
                row.additional[fieldNames[i]] = (i < parts.size()) ? parts[i] : "";
            }

            out.results.push_back(std::move(row));
            out.successCount++;
        }

        return out;
    } catch (const std::exception& e) {
        return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
    }
}

//==========================================================================
// titleformat.getBuiltinFields - Get list of common titleformat fields
// Returns built-in field reference for frontend use
//==========================================================================
api::Result<api::titleformat::GetBuiltinFieldsResult> TitleformatGetBuiltinFields(
    const api::titleformat::GetBuiltinFieldsParams& /*params*/) {
    api::titleformat::GetBuiltinFieldsResult out;
    out.fields = {
        // === Standard Tags ===
        {"artist", "%artist%"},
        {"album", "%album%"},
        {"title", "%title%"},
        {"albumArtist", "%album artist%"},
        {"genre", "%genre%"},
        {"date", "%date%"},
        {"year", "$year(%date%)"},
        {"trackNumber", "%tracknumber%"},
        {"discNumber", "%discnumber%"},
        {"composer", "%composer%"},
        {"performer", "%performer%"},
        {"comment", "%comment%"},
        
        // === Technical Info ===
        {"codec", "%codec%"},
        {"bitrate", "%bitrate%"},
        {"sampleRate", "%samplerate%"},
        {"channels", "%channels%"},
        {"bitDepth", "%bitspersample%"},
        {"duration", "%length%"},
        {"durationSeconds", "%length_seconds%"},
        
        // === File Info ===
        {"filename", "%filename%"},
        {"filenameExt", "%filename_ext%"},
        {"path", "%path%"},
        {"directoryPath", "%directory_path%"},
        {"fileSize", "%filesize%"},
        {"fileModified", "%file_modified%"},
        {"fileCreated", "%file_created%"},
        
        // === Playback (dynamic) ===
        {"isPlaying", "%isplaying%"},
        {"isPaused", "%ispaused%"},
        {"playbackTime", "%playback_time%"},
        
        // === foo_playcount (requires plugin) ===
        {"playCount", "%play_count%"},
        {"rating", "%rating%"},
        {"firstPlayed", "%first_played%"},
        {"lastPlayed", "%last_played%"},
        {"added", "%added%"},
        
        // === Useful Patterns ===
        {"artistOrAlbumArtist", "$if(%artist%,%artist%,%album artist%)"},
        {"displayTitle", "$if(%title%,%title%,%filename%)"},
        {"trackDisplay", "$if(%tracknumber%,$num(%tracknumber%,2). ,)%title%"}
    };
    return out;
}

// ============================================
// API Registration
// ============================================

// Path security levels come from the schema (x-security) through the generated types.
void RegisterTitleformatApi() {
    api::RegisterApi("titleformat.eval", TitleformatEval);
    api::RegisterApi("titleformat.evalBatch", TitleformatEvalBatch);
    api::RegisterApi("titleformat.evalFields", TitleformatEvalFields);
    api::RegisterApi("titleformat.evalFieldsBatch", TitleformatEvalFieldsBatch);
    api::RegisterApi("titleformat.getBuiltinFields", TitleformatGetBuiltinFields);
}
