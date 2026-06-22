#include "pch.h"
#include "api/TitleformatApi.h"
#include "api/BridgeCore.h"
#include <algorithm>

// ============================================
// Helper Functions
// ============================================

// Unique separator for merging multiple titleformat fields.
// Uses a sequence extremely unlikely to appear in real metadata.
static constexpr const char* kFieldSeparator = "\x01\x1F\x01";

// Evaluate a single titleformat pattern for a track
static std::string EvalPattern(const metadb_handle_ptr& track, const titleformat_object::ptr& script) {
    if (!track.is_valid() || !script.is_valid()) return "";
    
    pfc::string8 result;
    track->format_title(nullptr, result, script, nullptr);
    return result.get_ptr();
}

// Create metadb_handle from path
static metadb_handle_ptr GetHandleFromPath(const std::string& path) {
    if (path.empty()) return nullptr;
    
    pfc::string8 canonicalPath;
    filesystem::g_get_canonical_path(path.c_str(), canonicalPath);
    
    auto mdb = metadb::get();
    return mdb->handle_create(canonicalPath.c_str(), 0);
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
// params: { path: string, pattern: string }
// Returns: { success: true, result: string }
//==========================================================================
json TitleformatEval(const json& params) {
    std::string path = params.value("path", "");
    std::string pattern = params.value("pattern", "");
    
    if (path.empty()) {
        return {{"success", false}, {"error", "path is required"}};
    }
    if (pattern.empty()) {
        return {{"success", false}, {"error", "pattern is required"}};
    }
    
    try {
        // Compile pattern
        static_api_ptr_t<titleformat_compiler> compiler;
        titleformat_object::ptr script;
        
        if (!compiler->compile(script, pattern.c_str())) {
            return {{"success", false}, {"error", "Invalid titleformat pattern"}};
        }
        
        // Get track handle
        metadb_handle_ptr handle = GetHandleFromPath(path);
        if (!handle.is_valid()) {
            return {{"success", false}, {"error", "Failed to open file"}, {"path", path}};
        }
        
        // Evaluate
        std::string result = EvalPattern(handle, script);
        
        return {
            {"success", true},
            {"path", path},
            {"pattern", pattern},
            {"result", result}
        };
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", e.what()}};
    }
}

//==========================================================================
// titleformat.evalBatch - Evaluate a single pattern for multiple files
// params: { paths: string[], pattern: string }
// Returns: { success: true, results: [{ path, result }] }
//==========================================================================
json TitleformatEvalBatch(const json& params) {
    if (!params.contains("paths") || !params["paths"].is_array()) {
        return {{"success", false}, {"error", "paths array is required"}};
    }
    std::string pattern = params.value("pattern", "");
    
    if (pattern.empty()) {
        return {{"success", false}, {"error", "pattern is required"}};
    }
    
    try {
        // Compile pattern once (reuse for all files)
        static_api_ptr_t<titleformat_compiler> compiler;
        titleformat_object::ptr script;
        
        if (!compiler->compile(script, pattern.c_str())) {
            return {{"success", false}, {"error", "Invalid titleformat pattern"}};
        }
        
        const auto& paths = params["paths"];
        json results = json::array();
        int successCount = 0;
        int errorCount = 0;
        auto mdb = metadb::get();
        
        for (const auto& pathItem : paths) {
            if (!pathItem.is_string()) {
                results.push_back({{"success", false}, {"error", "invalid path type"}});
                errorCount++;
                continue;
            }
            
            std::string path = pathItem.get<std::string>();
            
            pfc::string8 canonicalPath;
            filesystem::g_get_canonical_path(path.c_str(), canonicalPath);
            
            metadb_handle_ptr handle = mdb->handle_create(canonicalPath.c_str(), 0);
            
            if (!handle.is_valid()) {
                results.push_back({{"path", path}, {"success", false}, {"error", "Failed to open file"}});
                errorCount++;
                continue;
            }
            
            std::string result = EvalPattern(handle, script);
            results.push_back({{"path", path}, {"success", true}, {"result", result}});
            successCount++;
        }
        
        return {
            {"success", true},
            {"pattern", pattern},
            {"total", paths.size()},
            {"successCount", successCount},
            {"errorCount", errorCount},
            {"results", results}
        };
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", e.what()}};
    }
}

//==========================================================================
// titleformat.evalFields - Evaluate multiple fields for a single file
// params: { path: string, fields: { fieldName: pattern, ... } }
// Returns: { success: true, fieldName: value, ... }
//==========================================================================
json TitleformatEvalFields(const json& params) {
    std::string path = params.value("path", "");
    
    if (path.empty()) {
        return {{"success", false}, {"error", "path is required"}};
    }
    if (!params.contains("fields") || !params["fields"].is_object()) {
        return {{"success", false}, {"error", "fields object is required"}};
    }
    
    try {
        // Get track handle
        metadb_handle_ptr handle = GetHandleFromPath(path);
        if (!handle.is_valid()) {
            return {{"success", false}, {"error", "Failed to open file"}, {"path", path}};
        }
        
        const auto& fields = params["fields"];
        static_api_ptr_t<titleformat_compiler> compiler;
        
        // 合并所有字段为单个 titleformat 脚本
        std::vector<std::string> fieldNames;
        std::string mergedPattern;
        
        for (auto& [fieldName, patternValue] : fields.items()) {
            if (!patternValue.is_string()) continue;
            std::string pattern = patternValue.get<std::string>();
            if (!mergedPattern.empty()) {
                mergedPattern += kFieldSeparator;
            }
            mergedPattern += pattern;
            fieldNames.push_back(fieldName);
        }
        
        json result;
        result["success"] = true;
        result["path"] = path;
        
        if (!fieldNames.empty()) {
            titleformat_object::ptr mergedScript;
            if (compiler->compile(mergedScript, mergedPattern.c_str())) {
                pfc::string8 formatted;
                handle->format_title(nullptr, formatted, mergedScript, nullptr);
                std::vector<std::string> parts = SplitString(formatted.c_str(), kFieldSeparator);
                for (size_t i = 0; i < fieldNames.size(); i++) {
                    result[fieldNames[i]] = (i < parts.size()) ? parts[i] : "";
                }
            }
        }
        
        // 对于非字符串字段，设为 null
        for (auto& [fieldName, patternValue] : fields.items()) {
            if (!patternValue.is_string()) {
                result[fieldName] = nullptr;
            }
        }
        
        return result;
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", e.what()}};
    }
}

//==========================================================================
// titleformat.evalFieldsBatch - Evaluate multiple fields for multiple files
// params: { paths: string[], fields: { fieldName: pattern, ... } }
// Returns: { success: true, results: [{ path, success, fieldName: value, ... }] }
//==========================================================================
json TitleformatEvalFieldsBatch(const json& params) {
    if (!params.contains("paths") || !params["paths"].is_array()) {
        return {{"success", false}, {"error", "paths array is required"}};
    }
    if (!params.contains("fields") || !params["fields"].is_object()) {
        return {{"success", false}, {"error", "fields object is required"}};
    }
    
    try {
        const auto& paths = params["paths"];
        const auto& fields = params["fields"];
        static_api_ptr_t<titleformat_compiler> compiler;
        auto mdb = metadb::get();
        
        // 合并所有字段模式为单个 ||| 分隔的 titleformat 脚本
        // 100 路径 × 10 字段: 从 1000 次 format_title() 减少到 100 次 (~10× 提速)
        std::vector<std::string> fieldNames;
        std::string mergedPattern;
        
        for (auto& [fieldName, patternValue] : fields.items()) {
            if (!patternValue.is_string()) continue;
            std::string pattern = patternValue.get<std::string>();
            if (!mergedPattern.empty()) {
                mergedPattern += kFieldSeparator;
            }
            mergedPattern += pattern;
            fieldNames.push_back(fieldName);
        }
        
        if (fieldNames.empty()) {
            return {{"success", true}, {"total", 0}, {"successCount", 0}, {"errorCount", 0}, {"results", json::array()}};
        }
        
        // 编译合并后的脚本（只编译一次）
        titleformat_object::ptr mergedScript;
        if (!compiler->compile(mergedScript, mergedPattern.c_str())) {
            return {{"success", false}, {"error", "Failed to compile merged pattern"}};
        }
        
        json results = json::array();
        int successCount = 0;
        int errorCount = 0;
        
        for (const auto& pathItem : paths) {
            if (!pathItem.is_string()) {
                results.push_back({{"success", false}, {"error", "invalid path type"}});
                errorCount++;
                continue;
            }
            
            std::string path = pathItem.get<std::string>();
            
            pfc::string8 canonicalPath;
            filesystem::g_get_canonical_path(path.c_str(), canonicalPath);
            
            metadb_handle_ptr handle = mdb->handle_create(canonicalPath.c_str(), 0);
            
            if (!handle.is_valid()) {
                results.push_back({{"path", path}, {"success", false}, {"error", "Failed to open file"}});
                errorCount++;
                continue;
            }
            
            // 单次 format_title 调用获取所有字段
            pfc::string8 formatted;
            handle->format_title(nullptr, formatted, mergedScript, nullptr);
            
            // Split 并映射回字段名
            std::vector<std::string> parts = SplitString(formatted.c_str(), kFieldSeparator);
            
            json item;
            item["path"] = path;
            item["success"] = true;
            
            for (size_t i = 0; i < fieldNames.size(); i++) {
                item[fieldNames[i]] = (i < parts.size()) ? parts[i] : "";
            }
            
            results.push_back(item);
            successCount++;
        }
        
        return {
            {"success", true},
            {"total", paths.size()},
            {"successCount", successCount},
            {"errorCount", errorCount},
            {"results", results}
        };
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", e.what()}};
    }
}

//==========================================================================
// titleformat.getBuiltinFields - Get list of common titleformat fields
// Returns built-in field reference for frontend use
//==========================================================================
json TitleformatGetBuiltinFields(const json& /*params*/) {
    return {
        {"success", true},
        {"fields", {
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
        }}
    };
}

// ============================================
// API Registration
// ============================================

void RegisterTitleformatApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    bridge.RegisterApi("titleformat.eval", TitleformatEval, {{"path", SecurityLevel::MediaRead}});
    bridge.RegisterApi("titleformat.evalBatch", TitleformatEvalBatch, {{"paths", SecurityLevel::MediaRead, true}});
    bridge.RegisterApi("titleformat.evalFields", TitleformatEvalFields, {{"path", SecurityLevel::MediaRead}});
    bridge.RegisterApi("titleformat.evalFieldsBatch", TitleformatEvalFieldsBatch, {{"paths", SecurityLevel::MediaRead, true}});
    bridge.RegisterApi("titleformat.getBuiltinFields", TitleformatGetBuiltinFields);
}
