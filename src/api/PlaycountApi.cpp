// PlaycountApi.cpp - Playcount Statistics API
// Provides access to foo_playcount data (play count, ratings, timestamps)

#include "pch.h"
#include "api/PlaycountApi.h"
#include "api/BridgeCore.h"

namespace {
    using json = nlohmann::json;

    // Helper to split string by delimiter
    std::vector<std::string> SplitString(const std::string& str, const std::string& delimiter) {
        std::vector<std::string> parts;
        size_t start = 0;
        size_t end = str.find(delimiter);
        
        while (end != std::string::npos) {
            parts.push_back(str.substr(start, end - start));
            start = end + delimiter.length();
            end = str.find(delimiter, start);
        }
        parts.push_back(str.substr(start));
        
        return parts;
    }

    // Helper to parse integer from string, returns 0 if invalid
    int ParseInt(const std::string& str) {
        if (str.empty() || str == "?") return 0;
        try {
            return std::stoi(str);
        } catch (...) {
            return 0;
        }
    }

    //==========================================================================
    // playcount.get - Get playback statistics for files
    //==========================================================================
    json PlaycountGet(const json& params) {
        if (!params.contains("paths") || !params["paths"].is_array()) {
            return {{"success", false}, {"error", "paths array is required"}};
        }

        try {
            const auto& paths = params["paths"];
            json results = json::array();
            auto mdb = metadb::get();

            // 缓存编译后的 titleformat 脚本，避免每次调用重复编译
            static titleformat_object::ptr tf;
            if (!tf.is_valid()) {
                static_api_ptr_t<titleformat_compiler>()->compile_safe(tf,
                    "%play_count%|||%first_played%|||%last_played%|||%added%|||%rating%");
            }

            // 移到循环外，避免 N 次重复服务查找
            auto libMgr = library_manager::get();

            for (const auto& pathItem : paths) {
                if (!pathItem.is_string()) continue;

                std::string path = pathItem.get<std::string>();
                std::string originalPath = path;  // 保存原始路径用于返回
                
                // 解析 CUE subsong index (|subsong:N 格式)
                int subsongIndex = 0;
                size_t pipePos = path.find("|subsong:");
                if (pipePos != std::string::npos) {
                    std::string indexStr = path.substr(pipePos + 9);
                    try {
                        subsongIndex = std::stoi(indexStr);
                        path = path.substr(0, pipePos);  // 移除 |subsong:N 部分
                    } catch (...) {
                        subsongIndex = 0;
                    }
                }
                
                pfc::string8 canonicalPath;
                filesystem::g_get_canonical_path(path.c_str(), canonicalPath);

                // O(log n) handle creation (uses canonical path + subsong index)
                metadb_handle_ptr handle = mdb->handle_create(canonicalPath.c_str(), subsongIndex);

                if (!handle.is_valid()) {
                    results.push_back({
                        {"path", path},
                        {"success", false},
                        {"error", "Failed to open file"}
                    });
                    continue;
                }

                // 移到 is_valid() 检查之后，修复潜在 UB
                bool foundInLibrary = libMgr->is_item_in_library(handle);

                // Format the title to get playcount data
                pfc::string8 formatted;
                handle->format_title(nullptr, formatted, tf, nullptr);

                // Parse the result: play_count|||first_played|||last_played|||added|||rating
                std::vector<std::string> parts = SplitString(formatted.c_str(), "|||");

                json item = {
                    {"path", originalPath},  // 返回原始路径（含 |subsong:N）
                    {"success", true}
                };

                // Parse each field
                if (parts.size() >= 1) {
                    int playCount = ParseInt(parts[0]);
                    item["playCount"] = playCount;
                }

                if (parts.size() >= 2 && !parts[1].empty() && parts[1] != "?") {
                    item["firstPlayed"] = parts[1];
                }

                if (parts.size() >= 3 && !parts[2].empty() && parts[2] != "?") {
                    item["lastPlayed"] = parts[2];
                }

                if (parts.size() >= 4 && !parts[3].empty() && parts[3] != "?") {
                    item["added"] = parts[3];
                }

                if (parts.size() >= 5) {
                    int rating = ParseInt(parts[4]);
                    if (rating > 0) {
                        item["rating"] = rating;
                    }
                }

                // Add flag to indicate if data is from library
                item["inLibrary"] = foundInLibrary;

                results.push_back(item);
            }

            return {
                {"success", true},
                {"count", results.size()},
                {"results", results}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // playcount.getBatch - Get playback statistics for multiple files efficiently
    //==========================================================================
    json PlaycountGetBatch(const json& params) {
        // Same as get, but optimized for batch operations
        return PlaycountGet(params);
    }

    //==========================================================================
    // playcount.set - Set playcount/rating (if supported)
    //==========================================================================
    json PlaycountSet(const json& params) {
        if (!params.contains("path") || !params["path"].is_string()) {
            return {{"success", false}, {"error", "path is required"}};
        }

        std::string path = params["path"].get<std::string>();
        
        // Note: foo_playcount doesn't have a direct API to set values.
        // Rating can be set via context menu or the rating API we already have.
        // This is a placeholder for future implementation if foo_playcount exposes API.
        
        return {
            {"success", false},
            {"error", "Direct playcount modification not supported. Use rating.set for ratings."}
        };
    }

    //==========================================================================
    // playcount.getStats - Get overall library statistics
    //==========================================================================
    json PlaycountGetStats(const json& /*params*/) {
        try {
            auto libMgr = library_manager::get();
            metadb_handle_list allItems;
            libMgr->get_all_items(allItems);

            // 合并 titleformat 脚本为单次调用，每轨只需一次 format_title
            static titleformat_object::ptr tfMerged;
            if (!tfMerged.is_valid()) {
                static_api_ptr_t<titleformat_compiler>()->compile_safe(tfMerged, "%play_count%|||%rating%");
            }

            int totalTracks = 0;
            int playedTracks = 0;
            int ratedTracks = 0;
            int totalPlayCount = 0;
            int maxPlayCount = 0;
            int ratingSum = 0;

            for (size_t i = 0; i < allItems.get_count(); i++) {
                totalTracks++;

                pfc::string8 formatted;
                allItems[i]->format_title(nullptr, formatted, tfMerged, nullptr);
                
                // Parse: "play_count|||rating"
                std::vector<std::string> parts = SplitString(formatted.c_str(), "|||");
                int playCount = parts.size() >= 1 ? ParseInt(parts[0]) : 0;

                if (playCount > 0) {
                    playedTracks++;
                    totalPlayCount += playCount;
                    if (playCount > maxPlayCount) {
                        maxPlayCount = playCount;
                    }
                }

                int rating = parts.size() >= 2 ? ParseInt(parts[1]) : 0;

                if (rating > 0) {
                    ratedTracks++;
                    ratingSum += rating;
                }
            }

            double avgRating = ratedTracks > 0 ? (double)ratingSum / ratedTracks : 0.0;
            double avgPlayCount = playedTracks > 0 ? (double)totalPlayCount / playedTracks : 0.0;

            return {
                {"success", true},
                {"totalTracks", totalTracks},
                {"playedTracks", playedTracks},
                {"unplayedTracks", totalTracks - playedTracks},
                {"ratedTracks", ratedTracks},
                {"totalPlayCount", totalPlayCount},
                {"maxPlayCount", maxPlayCount},
                {"averagePlayCount", avgPlayCount},
                {"averageRating", avgRating}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

} // namespace

void RegisterPlaycountApi() {
    auto& bridge = BridgeCore::GetInstance();

    // playcount.get - Get playback statistics for files
    bridge.RegisterApi("playcount.get", PlaycountGet, {{"paths", SecurityLevel::MediaRead, true}});

    // playcount.getBatch - Batch get (same as get, for API consistency)
    bridge.RegisterApi("playcount.getBatch", PlaycountGetBatch, {{"paths", SecurityLevel::MediaRead, true}});

    // playcount.set - Placeholder for future implementation
    bridge.RegisterApi("playcount.set", PlaycountSet, {{"path", SecurityLevel::MediaWrite}});

    // playcount.getStats - Get overall library statistics
    bridge.RegisterApi("playcount.getStats", PlaycountGetStats);

    console::print("[WebView2 UI] Playcount API registered (4 APIs)");
}
