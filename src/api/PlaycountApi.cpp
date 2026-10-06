// PlaycountApi.cpp - Playcount Statistics API
// Provides access to foo_playcount data (play count, ratings, timestamps)

#include "pch.h"
#include "api/PlaycountApi.h"
#include "api/BridgeCore.h"
#include "api/TypedApi.h"
#include "api/generated/PlaycountSchema.h"
#include "utils/SubsongUtils.h"

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
    // playcount.get / playcount.getBatch - Get playback statistics for files
    // Both methods have the same params and result shape; R is their own
    // generated result type.
    //==========================================================================
    template <class R>
    api::Result<R> ReadPlaycounts(const std::vector<std::string>& paths) {
        try {
            R out;

            // 缓存编译后的 titleformat 脚本，避免每次调用重复编译
            static titleformat_object::ptr tf;
            if (!tf.is_valid()) {
                static_api_ptr_t<titleformat_compiler>()->compile_safe(tf,
                    "%play_count%|||%first_played%|||%last_played%|||%added%|||%rating%");
            }

            // 移到循环外，避免 N 次重复服务查找
            auto libMgr = library_manager::get();

            for (const std::string& requested : paths) {
                // 拆 |subsong:N、规范化、建 handle 走共享函数；读不出序号时截断、按 0 并记一行控制台。
                metadb_handle_ptr handle = SubsongUtils::CreateTrackHandle(requested);

                api::playcount::PlaycountRow row;
                // 成功行与失败行都回请求原样的路径，调用方按 path 对行。
                row.path = requested;
                if (!handle.is_valid()) {
                    row.error = "Failed to open file";
                    out.results.push_back(std::move(row));
                    continue;
                }

                // 移到 is_valid() 检查之后，修复潜在 UB
                bool foundInLibrary = libMgr->is_item_in_library(handle);

                // Format the title to get playcount data
                pfc::string8 formatted;
                handle->format_title(nullptr, formatted, tf, nullptr);

                // Parse the result: play_count|||first_played|||last_played|||added|||rating
                std::vector<std::string> parts = SplitString(formatted.c_str(), "|||");

                row.success = true;

                // Parse each field
                if (parts.size() >= 1) {
                    row.playCount = ParseInt(parts[0]);
                }

                if (parts.size() >= 2 && !parts[1].empty() && parts[1] != "?") {
                    row.firstPlayed = parts[1];
                }

                if (parts.size() >= 3 && !parts[2].empty() && parts[2] != "?") {
                    row.lastPlayed = parts[2];
                }

                if (parts.size() >= 4 && !parts[3].empty() && parts[3] != "?") {
                    row.added = parts[3];
                }

                if (parts.size() >= 5) {
                    int rating = ParseInt(parts[4]);
                    if (rating > 0) {
                        row.rating = rating;
                    }
                }

                // Add flag to indicate if data is from library
                row.inLibrary = foundInLibrary;

                out.results.push_back(std::move(row));
            }

            out.count = static_cast<std::int64_t>(out.results.size());
            return out;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    api::Result<api::playcount::GetResult> PlaycountGet(const api::playcount::GetParams& p) {
        return ReadPlaycounts<api::playcount::GetResult>(p.paths);
    }

    api::Result<api::playcount::GetBatchResult> PlaycountGetBatch(const api::playcount::GetBatchParams& p) {
        return ReadPlaycounts<api::playcount::GetBatchResult>(p.paths);
    }

    //==========================================================================
    // playcount.set - Placeholder: foo_playcount has no API to change values
    // Rating can be set via context menu or the rating API we already have.
    //==========================================================================
    api::Result<void> PlaycountSet(const api::playcount::SetParams& /*params*/) {
        return api::Fail("Direct playcount modification not supported. Use rating.set for ratings.",
                         ApiErrorCode::NOT_SUPPORTED);
    }

    //==========================================================================
    // playcount.getStats - Get overall library statistics
    //==========================================================================
    api::Result<api::playcount::GetStatsResult> PlaycountGetStats(const api::playcount::GetStatsParams& /*params*/) {
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

            api::playcount::GetStatsResult out;
            out.totalTracks = totalTracks;
            out.playedTracks = playedTracks;
            out.unplayedTracks = totalTracks - playedTracks;
            out.ratedTracks = ratedTracks;
            out.totalPlayCount = totalPlayCount;
            out.maxPlayCount = maxPlayCount;
            out.averagePlayCount = playedTracks > 0 ? (double)totalPlayCount / playedTracks : 0.0;
            out.averageRating = ratedTracks > 0 ? (double)ratingSum / ratedTracks : 0.0;
            return out;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

} // namespace

// Parameters, results and path security levels come from src/api/schema/playcount.ts through the generated types.
void RegisterPlaycountApi() {
    api::RegisterApi("playcount.get", PlaycountGet);
    api::RegisterApi("playcount.getBatch", PlaycountGetBatch);
    api::RegisterApi("playcount.set", PlaycountSet);
    api::RegisterApi("playcount.getStats", PlaycountGetStats);

    console::print("[WebView2 UI] Playcount API registered (4 APIs)");
}
