// ReplayGainApi.cpp - ReplayGain Settings API
// Provides control over foobar2000's ReplayGain settings

#include "pch.h"
#include "api/ReplayGainApi.h"
#include "api/TypedApi.h"
#include "api/generated/ReplaygainSchema.h"
#include "utils/SubsongUtils.h"
#include <foobar2000/SDK/replaygain.h>

namespace {
    namespace rg = api::replaygain;

    // Mode strings for API. t_replaygain_config defines exactly the four source modes and the
    // four processing modes below; the declaration's enums list the same four, so anything else
    // (impossible through the foobar2000 UI) is reported as "none" rather than as a value the
    // declaration does not know.
    const char* GetModeString(t_uint32 mode) {
        switch (mode) {
            case t_replaygain_config::source_mode_none: return "none";
            case t_replaygain_config::source_mode_track: return "track";
            case t_replaygain_config::source_mode_album: return "album";
            case t_replaygain_config::source_mode_byPlaybackOrder: return "auto";
            default: return "none";
        }
    }

    t_uint32 ParseModeString(const std::string& mode) {
        if (mode == "none") return t_replaygain_config::source_mode_none;
        if (mode == "track") return t_replaygain_config::source_mode_track;
        if (mode == "album") return t_replaygain_config::source_mode_album;
        if (mode == "auto" || mode == "byPlaybackOrder") return t_replaygain_config::source_mode_byPlaybackOrder;
        return t_replaygain_config::source_mode_none;
    }

    // Processing mode strings
    const char* GetProcessingString(t_uint32 mode) {
        switch (mode) {
            case t_replaygain_config::processing_mode_none: return "none";
            case t_replaygain_config::processing_mode_gain: return "gain";
            case t_replaygain_config::processing_mode_gain_and_peak: return "gain_and_peak";
            case t_replaygain_config::processing_mode_peak: return "peak";
            default: return "none";
        }
    }

    t_uint32 ParseProcessingString(const std::string& mode) {
        if (mode == "none") return t_replaygain_config::processing_mode_none;
        if (mode == "gain") return t_replaygain_config::processing_mode_gain;
        if (mode == "gain_and_peak") return t_replaygain_config::processing_mode_gain_and_peak;
        if (mode == "peak") return t_replaygain_config::processing_mode_peak;
        return t_replaygain_config::processing_mode_none;
    }

    //==========================================================================
    // replaygain.getSettings - Get all ReplayGain settings
    //==========================================================================
    api::Result<rg::GetSettingsResult> ReplayGainGetSettings(const rg::GetSettingsParams& /*params*/) {
        try {
            auto rg_mgr = replaygain_manager::get();
            t_replaygain_config config = rg_mgr->get_core_settings();

            rg::GetSettingsResult result;
            result.sourceMode = GetModeString(config.m_source_mode);
            result.processingMode = GetProcessingString(config.m_processing_mode);
            result.preampWithRg = config.m_preamp_with_rg;
            result.preampWithoutRg = config.m_preamp_without_rg;
            result.active = config.is_active();
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // replaygain.getMode - Get current ReplayGain mode
    //==========================================================================
    api::Result<rg::GetModeResult> ReplayGainGetMode(const rg::GetModeParams& /*params*/) {
        try {
            auto rg_mgr = replaygain_manager::get();
            t_replaygain_config config = rg_mgr->get_core_settings();

            rg::GetModeResult result;
            result.sourceMode = GetModeString(config.m_source_mode);
            result.processingMode = GetProcessingString(config.m_processing_mode);
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // replaygain.setMode - Set ReplayGain mode
    //==========================================================================
    api::Result<rg::SetModeResult> ReplayGainSetMode(const rg::SetModeParams& params) {
        try {
            auto rg_mgr = replaygain_manager::get();
            t_replaygain_config config = rg_mgr->get_core_settings();

            bool changed = false;

            // Set source mode
            if (params.sourceMode) {
                t_uint32 newMode = ParseModeString(*params.sourceMode);
                if (config.m_source_mode != newMode) {
                    config.m_source_mode = newMode;
                    changed = true;
                }
            }

            // Set processing mode
            if (params.processingMode) {
                t_uint32 newMode = ParseProcessingString(*params.processingMode);
                if (config.m_processing_mode != newMode) {
                    config.m_processing_mode = newMode;
                    changed = true;
                }
            }

            if (changed) {
                rg_mgr->set_core_settings(config);
            }

            rg::SetModeResult result;
            result.sourceMode = GetModeString(config.m_source_mode);
            result.processingMode = GetProcessingString(config.m_processing_mode);
            result.changed = changed;
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // replaygain.getPreamp - Get preamp values
    //==========================================================================
    api::Result<rg::GetPreampResult> ReplayGainGetPreamp(const rg::GetPreampParams& /*params*/) {
        try {
            auto rg_mgr = replaygain_manager::get();
            t_replaygain_config config = rg_mgr->get_core_settings();

            rg::GetPreampResult result;
            result.withRg = config.m_preamp_with_rg;
            result.withoutRg = config.m_preamp_without_rg;
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // replaygain.setPreamp - Set preamp values
    // The declaration limits both values to -24..+24 dB; the parser refuses anything outside.
    //==========================================================================
    api::Result<rg::SetPreampResult> ReplayGainSetPreamp(const rg::SetPreampParams& params) {
        try {
            auto rg_mgr = replaygain_manager::get();
            t_replaygain_config config = rg_mgr->get_core_settings();

            bool changed = false;

            if (params.withRg) {
                const float value = static_cast<float>(*params.withRg);
                if (config.m_preamp_with_rg != value) {
                    config.m_preamp_with_rg = value;
                    changed = true;
                }
            }

            if (params.withoutRg) {
                const float value = static_cast<float>(*params.withoutRg);
                if (config.m_preamp_without_rg != value) {
                    config.m_preamp_without_rg = value;
                    changed = true;
                }
            }

            if (changed) {
                rg_mgr->set_core_settings(config);
            }

            rg::SetPreampResult result;
            result.withRg = config.m_preamp_with_rg;
            result.withoutRg = config.m_preamp_without_rg;
            result.changed = changed;
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // replaygain.get - Get ReplayGain info from specific files
    //==========================================================================
    api::Result<rg::GetResult> ReplayGainGet(const rg::GetParams& params) {
        try {
            rg::GetResult result;
            result.results.reserve(params.paths.size());

            for (const std::string& path : params.paths) {
                // 直读与回退读同一首：拆出的序号既交给 get_info，也随 handle 进缓存查询。
                const fb2k_paths::SubsongPath parts = SubsongUtils::SplitSubsongPath(path);
                std::string canonicalPath;
                const metadb_handle_ptr handle =
                    SubsongUtils::CreateCanonicalHandle(parts.path.c_str(), parts.subsong, canonicalPath);

                file_info_impl info;
                bool gotInfo = false;

                // Try reading directly from file (more reliable)
                try {
                    input_info_reader::ptr reader;
                    input_entry::g_open_for_info_read(reader, nullptr, canonicalPath.c_str(), fb2k::noAbort);
                    if (reader.is_valid()) {
                        reader->get_info(parts.subsong, info, fb2k::noAbort);
                        gotInfo = true;
                    }
                } catch (...) {
                    // Fallback to cached info
                    if (handle.is_valid()) {
                        gotInfo = handle->get_info(info);
                    }
                }

                rg::ReplayGainTrackInfo row;
                row.path = path;
                if (!gotInfo) {
                    row.success = false;
                    row.error = "Failed to get track info";
                    result.results.push_back(std::move(row));
                    continue;
                }

                replaygain_info rgInfo = info.get_replaygain();
                row.success = true;

                if (rgInfo.is_track_gain_present()) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.2f dB", rgInfo.m_track_gain);
                    row.trackGain = buf;
                    row.trackGainRaw = rgInfo.m_track_gain;
                }
                if (rgInfo.is_track_peak_present()) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.6f", rgInfo.m_track_peak);
                    row.trackPeak = buf;
                    row.trackPeakRaw = rgInfo.m_track_peak;
                }
                if (rgInfo.is_album_gain_present()) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.2f dB", rgInfo.m_album_gain);
                    row.albumGain = buf;
                    row.albumGainRaw = rgInfo.m_album_gain;
                }
                if (rgInfo.is_album_peak_present()) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.6f", rgInfo.m_album_peak);
                    row.albumPeak = buf;
                    row.albumPeakRaw = rgInfo.m_album_peak;
                }

                row.hasReplayGain = rgInfo.is_track_gain_present() || rgInfo.is_album_gain_present();
                result.results.push_back(std::move(row));
            }

            result.count = static_cast<std::int64_t>(result.results.size());
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // replaygain.clear - Remove ReplayGain info from files
    //==========================================================================
    class RGClearFilter : public file_info_filter {
    public:
        bool apply_filter(metadb_handle_ptr p_location, t_filestats p_stats,
                          file_info& p_info) override {
            replaygain_info rgInfo;
            rgInfo.reset();  // Reset to default (no RG info)
            p_info.set_replaygain(rgInfo);
            return true;
        }
    };

    api::Result<rg::ClearResult> ReplayGainClear(const rg::ClearParams& params) {
        try {
            metadb_handle_list handles;
            int foundCount = 0;

            for (const std::string& path : params.paths) {
                const metadb_handle_ptr handle = SubsongUtils::CreateTrackHandle(path);
                if (handle.is_valid()) {
                    handles.add_item(handle);
                    foundCount++;
                }
            }

            if (handles.get_count() == 0) {
                return api::Fail("No valid files found", ApiErrorCode::NOT_FOUND);
            }

            // Apply the clear filter
            service_ptr_t<file_info_filter> filter =
                fb2k::service_new<RGClearFilter>();

            auto io = metadb_io_v2::get();
            // 静默：op_flag_silent (fb2k 2.0+) + op_flag_delay_ui (fb2k 1.x fallback) +
            // op_flag_no_errors,完全抑制进度/错误对话框。
            io->update_info_async(handles, filter, core_api::get_main_window(),
                                  metadb_io_v2::op_flag_no_errors |
                                      metadb_io_v2::op_flag_delay_ui |
                                      metadb_io_v2::op_flag_silent,
                                  nullptr);

            rg::ClearResult result;
            result.clearedCount = foundCount;
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // replaygain.scan - Scan ReplayGain using context menu
    // Note: Uses context menu approach as direct scanner API is complex
    //==========================================================================
    api::Result<rg::ScanResult> ReplayGainScan(const rg::ScanParams& params) {
        const std::string& mode = params.mode;

        try {
            metadb_handle_list handles;

            // Try to find handles from library first (more reliable for Unicode)
            auto lib = library_manager::get();
            metadb_handle_list libItems;
            if (lib.is_valid()) {
                lib->get_all_items(libItems);
            }

            for (const std::string& path : params.paths) {
                const fb2k_paths::SubsongPath parts = SubsongUtils::SplitSubsongPath(path);
                std::string canonicalPath;
                const metadb_handle_ptr created =
                    SubsongUtils::CreateCanonicalHandle(parts.path.c_str(), parts.subsong, canonicalPath);

                metadb_handle_ptr handle;

                // Try library first. 同一文件的多首子曲目共用一个路径，还要比序号，
                // 否则 CUE 里的哪一首都会落到媒体库里最先列出的那首。
                for (t_size i = 0; i < libItems.get_count(); i++) {
                    if (libItems[i]->get_subsong_index() == parts.subsong &&
                        metadb::path_compare(libItems[i]->get_path(), canonicalPath.c_str()) == 0) {
                        handle = libItems[i];
                        break;
                    }
                }

                // Fallback to the handle built from the canonical path
                if (!handle.is_valid()) {
                    handle = created;
                }

                if (handle.is_valid()) {
                    handles.add_item(handle);
                }
            }

            if (handles.get_count() == 0) {
                return api::Fail("No valid files found", ApiErrorCode::NOT_FOUND);
            }

            // Use context menu to trigger ReplayGain scan
            // Path: "ReplayGain/Scan per-file track gain" or "ReplayGain/Scan selection as a single album"
            service_ptr_t<contextmenu_manager> cmm;
            contextmenu_manager::g_create(cmm);
            if (!cmm.is_valid()) {
                return api::Fail("Failed to create context menu manager", ApiErrorCode::OPERATION_FAILED);
            }

            cmm->init_context(handles, 0);
            contextmenu_node* root = cmm->get_root();

            if (!root || root->get_type() != contextmenu_item_node::TYPE_POPUP) {
                return api::Fail("Failed to get context menu", ApiErrorCode::OPERATION_FAILED);
            }

            // Search for ReplayGain / 播放增益 menu
            contextmenu_node* rgMenu = nullptr;
            t_size childCount = root->get_num_children();
            for (t_size i = 0; i < childCount; i++) {
                contextmenu_node* child = root->get_child(i);
                if (!child || !child->get_name()) continue;

                std::string name = child->get_name();
                // Match "ReplayGain" or "播放增益"
                if (name == "ReplayGain" || name.find("播放增益") != std::string::npos ||
                    name.find("\xE6\x92\xAD\xE6\x94\xBE\xE5\xA2\x9E\xE7\x9B\x8A") != std::string::npos) {
                    rgMenu = child;
                    break;
                }
            }

            if (!rgMenu || rgMenu->get_type() != contextmenu_item_node::TYPE_POPUP) {
                return api::Fail("ReplayGain menu not found", ApiErrorCode::NOT_SUPPORTED);
            }

            // Search for scan command
            contextmenu_node* scanCmd = nullptr;
            t_size rgChildCount = rgMenu->get_num_children();
            for (t_size i = 0; i < rgChildCount; i++) {
                contextmenu_node* child = rgMenu->get_child(i);
                if (!child || !child->get_name()) continue;

                std::string name = child->get_name();

                if (mode == "album") {
                    // "Scan selection as a single album" / "扫描选定内容作为专辑"
                    if (name.find("album") != std::string::npos ||
                        name.find("专辑") != std::string::npos ||
                        name.find("\xE4\xB8\x93\xE8\xBE\x91") != std::string::npos) {
                        scanCmd = child;
                        break;
                    }
                } else {
                    // "Scan per-file track gain" / "扫描每个文件的音轨增益"
                    if (name.find("per-file") != std::string::npos ||
                        name.find("track") != std::string::npos ||
                        name.find("音轨") != std::string::npos ||
                        name.find("\xE9\x9F\xB3\xE8\xBD\xA8") != std::string::npos) {
                        scanCmd = child;
                        break;
                    }
                }
            }

            // If no specific match, use first scan command
            if (!scanCmd) {
                for (t_size i = 0; i < rgChildCount; i++) {
                    contextmenu_node* child = rgMenu->get_child(i);
                    if (child && child->get_type() == contextmenu_item_node::TYPE_COMMAND) {
                        std::string name = child->get_name() ? child->get_name() : "";
                        if (name.find("Scan") != std::string::npos ||
                            name.find("扫描") != std::string::npos ||
                            name.find("\xE6\x89\xAB\xE6\x8F\x8F") != std::string::npos) {
                            scanCmd = child;
                            break;
                        }
                    }
                }
            }

            if (!scanCmd) {
                return api::Fail("Scan command not found in ReplayGain menu", ApiErrorCode::NOT_SUPPORTED);
            }

            // Execute scan command
            scanCmd->execute();

            rg::ScanResult result;
            result.scannedCount = static_cast<std::int64_t>(handles.get_count());
            result.mode = mode;
            result.note = "Scan started. Results will be written to files automatically.";
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

} // anonymous namespace

//==========================================================================
// Register ReplayGain API
//==========================================================================
void RegisterReplayGainApi() {
    // The path security levels of get / scan (MediaRead) and clear (MediaWrite) come from the
    // declaration's @security tags.
    api::RegisterApi("replaygain.getSettings", ReplayGainGetSettings);
    api::RegisterApi("replaygain.getMode", ReplayGainGetMode);
    api::RegisterApi("replaygain.setMode", ReplayGainSetMode);
    api::RegisterApi("replaygain.getPreamp", ReplayGainGetPreamp);
    api::RegisterApi("replaygain.setPreamp", ReplayGainSetPreamp);
    api::RegisterApi("replaygain.get", ReplayGainGet);
    api::RegisterApi("replaygain.clear", ReplayGainClear);
    api::RegisterApi("replaygain.scan", ReplayGainScan);

    LOG("ReplayGain API registered (8 APIs)");
}
