// ReplayGainApi.cpp - ReplayGain Settings API
// Provides control over foobar2000's ReplayGain settings

#include "pch.h"
#include "api/ReplayGainApi.h"
#include "api/BridgeCore.h"
#include <foobar2000/SDK/replaygain.h>

namespace {
    using json = nlohmann::json;

    // Mode strings for API
    const char* GetModeString(t_uint32 mode) {
        switch (mode) {
            case t_replaygain_config::source_mode_none: return "none";
            case t_replaygain_config::source_mode_track: return "track";
            case t_replaygain_config::source_mode_album: return "album";
            case t_replaygain_config::source_mode_byPlaybackOrder: return "auto";
            default: return "unknown";
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
            default: return "unknown";
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
    json ReplayGainGetSettings(const json& /*params*/) {
        try {
            auto rg_mgr = replaygain_manager::get();
            t_replaygain_config config = rg_mgr->get_core_settings();

            return {
                {"sourceMode", GetModeString(config.m_source_mode)},
                {"processingMode", GetProcessingString(config.m_processing_mode)},
                {"preampWithRg", config.m_preamp_with_rg},
                {"preampWithoutRg", config.m_preamp_without_rg},
                {"active", config.is_active()}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // replaygain.getMode - Get current ReplayGain mode
    //==========================================================================
    json ReplayGainGetMode(const json& /*params*/) {
        try {
            auto rg_mgr = replaygain_manager::get();
            t_replaygain_config config = rg_mgr->get_core_settings();

            return {
                {"sourceMode", GetModeString(config.m_source_mode)},
                {"processingMode", GetProcessingString(config.m_processing_mode)}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // replaygain.setMode - Set ReplayGain mode
    //==========================================================================
    json ReplayGainSetMode(const json& params) {
        try {
            auto rg_mgr = replaygain_manager::get();
            t_replaygain_config config = rg_mgr->get_core_settings();

            bool changed = false;

            // Set source mode
            if (params.contains("sourceMode") && params["sourceMode"].is_string()) {
                std::string mode = params["sourceMode"].get<std::string>();
                t_uint32 newMode = ParseModeString(mode);
                if (config.m_source_mode != newMode) {
                    config.m_source_mode = newMode;
                    changed = true;
                }
            }

            // Set processing mode
            if (params.contains("processingMode") && params["processingMode"].is_string()) {
                std::string mode = params["processingMode"].get<std::string>();
                t_uint32 newMode = ParseProcessingString(mode);
                if (config.m_processing_mode != newMode) {
                    config.m_processing_mode = newMode;
                    changed = true;
                }
            }

            if (changed) {
                rg_mgr->set_core_settings(config);
            }

            return {
                {"success", true},
                {"sourceMode", GetModeString(config.m_source_mode)},
                {"processingMode", GetProcessingString(config.m_processing_mode)},
                {"changed", changed}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // replaygain.getPreamp - Get preamp values
    //==========================================================================
    json ReplayGainGetPreamp(const json& /*params*/) {
        try {
            auto rg_mgr = replaygain_manager::get();
            t_replaygain_config config = rg_mgr->get_core_settings();

            return {
                {"withRg", config.m_preamp_with_rg},
                {"withoutRg", config.m_preamp_without_rg}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // replaygain.setPreamp - Set preamp values
    //==========================================================================
    json ReplayGainSetPreamp(const json& params) {
        try {
            auto rg_mgr = replaygain_manager::get();
            t_replaygain_config config = rg_mgr->get_core_settings();

            bool changed = false;

            // Set preamp with RG
            if (params.contains("withRg") && params["withRg"].is_number()) {
                float value = params["withRg"].get<float>();
                // Clamp to reasonable range (-24 to +24 dB)
                value = std::max(-24.0f, std::min(24.0f, value));
                if (config.m_preamp_with_rg != value) {
                    config.m_preamp_with_rg = value;
                    changed = true;
                }
            }

            // Set preamp without RG
            if (params.contains("withoutRg") && params["withoutRg"].is_number()) {
                float value = params["withoutRg"].get<float>();
                value = std::max(-24.0f, std::min(24.0f, value));
                if (config.m_preamp_without_rg != value) {
                    config.m_preamp_without_rg = value;
                    changed = true;
                }
            }

            if (changed) {
                rg_mgr->set_core_settings(config);
            }

            return {
                {"success", true},
                {"withRg", config.m_preamp_with_rg},
                {"withoutRg", config.m_preamp_without_rg},
                {"changed", changed}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // replaygain.get - Get ReplayGain info from specific files
    //==========================================================================
    json ReplayGainGet(const json& params) {
        if (!params.contains("paths") || !params["paths"].is_array()) {
            return {{"success", false}, {"error", "paths array is required"}};
        }

        try {
            const auto& paths = params["paths"];
            json results = json::array();

            for (const auto& pathItem : paths) {
                if (!pathItem.is_string()) continue;
                
                std::string path = pathItem.get<std::string>();
                pfc::string8 canonicalPath;
                filesystem::g_get_canonical_path(path.c_str(), canonicalPath);

                file_info_impl info;
                bool gotInfo = false;

                // Try reading directly from file (more reliable)
                try {
                    input_info_reader::ptr reader;
                    input_entry::g_open_for_info_read(reader, nullptr, canonicalPath.c_str(), fb2k::noAbort);
                    if (reader.is_valid()) {
                        reader->get_info(0, info, fb2k::noAbort);
                        gotInfo = true;
                    }
                } catch (...) {
                    // Fallback to cached info
                    auto mdb = metadb::get();
                    metadb_handle_ptr handle = mdb->handle_create(canonicalPath.c_str(), 0);
                    if (handle.is_valid()) {
                        gotInfo = handle->get_info(info);
                    }
                }

                if (!gotInfo) {
                    results.push_back({
                        {"path", path},
                        {"success", false},
                        {"error", "Failed to get track info"}
                    });
                    continue;
                }

                replaygain_info rg = info.get_replaygain();
                json rgResult = {
                    {"path", path},
                    {"success", true}
                };

                // Debug: log what we found
                console::printf("replaygain.get: %s - track_gain_present=%d, album_gain_present=%d",
                    path.c_str(), rg.is_track_gain_present(), rg.is_album_gain_present());

                if (rg.is_track_gain_present()) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.2f dB", rg.m_track_gain);
                    rgResult["trackGain"] = buf;
                    rgResult["trackGainRaw"] = rg.m_track_gain;
                }
                if (rg.is_track_peak_present()) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.6f", rg.m_track_peak);
                    rgResult["trackPeak"] = buf;
                    rgResult["trackPeakRaw"] = rg.m_track_peak;
                }
                if (rg.is_album_gain_present()) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.2f dB", rg.m_album_gain);
                    rgResult["albumGain"] = buf;
                    rgResult["albumGainRaw"] = rg.m_album_gain;
                }
                if (rg.is_album_peak_present()) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.6f", rg.m_album_peak);
                    rgResult["albumPeak"] = buf;
                    rgResult["albumPeakRaw"] = rg.m_album_peak;
                }

                // Add "hasReplayGain" flag
                rgResult["hasReplayGain"] = rg.is_track_gain_present() || rg.is_album_gain_present();

                results.push_back(rgResult);
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
    // replaygain.clear - Remove ReplayGain info from files
    //==========================================================================
    class RGClearFilter : public file_info_filter {
    public:
        bool apply_filter(metadb_handle_ptr p_location, t_filestats p_stats,
                          file_info& p_info) override {
            replaygain_info rg;
            rg.reset();  // Reset to default (no RG info)
            p_info.set_replaygain(rg);
            return true;
        }
    };

    json ReplayGainClear(const json& params) {
        if (!params.contains("paths") || !params["paths"].is_array()) {
            return {{"success", false}, {"error", "paths array is required"}};
        }

        try {
            const auto& paths = params["paths"];
            metadb_handle_list handles;
            auto mdb = metadb::get();
            int foundCount = 0;

            for (const auto& pathItem : paths) {
                if (!pathItem.is_string()) continue;
                
                std::string path = pathItem.get<std::string>();
                pfc::string8 canonicalPath;
                filesystem::g_get_canonical_path(path.c_str(), canonicalPath);

                metadb_handle_ptr handle = mdb->handle_create(canonicalPath.c_str(), 0);
                if (handle.is_valid()) {
                    handles.add_item(handle);
                    foundCount++;
                }
            }

            if (handles.get_count() == 0) {
                return {{"success", false}, {"error", "No valid files found"}};
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

            return {
                {"success", true},
                {"clearedCount", foundCount}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // replaygain.scan - Scan ReplayGain using context menu
    // Note: Uses context menu approach as direct scanner API is complex
    //==========================================================================
    json ReplayGainScan(const json& params) {
        if (!params.contains("paths") || !params["paths"].is_array()) {
            return {{"success", false}, {"error", "paths array is required"}};
        }

        std::string mode = params.value("mode", "track");
        
        try {
            const auto& paths = params["paths"];
            metadb_handle_list handles;
            auto mdb = metadb::get();

            // Try to find handles from library first (more reliable for Unicode)
            auto lib = library_manager::get();
            metadb_handle_list libItems;
            if (lib.is_valid()) {
                lib->get_all_items(libItems);
            }

            for (const auto& pathItem : paths) {
                if (!pathItem.is_string()) continue;
                
                std::string path = pathItem.get<std::string>();
                pfc::string8 canonicalPath;
                filesystem::g_get_canonical_path(path.c_str(), canonicalPath);

                metadb_handle_ptr handle;

                // Try library first
                for (t_size i = 0; i < libItems.get_count(); i++) {
                    if (metadb::path_compare(libItems[i]->get_path(), canonicalPath.c_str()) == 0) {
                        handle = libItems[i];
                        break;
                    }
                }

                // Fallback to handle_create
                if (!handle.is_valid()) {
                    handle = mdb->handle_create(canonicalPath.c_str(), 0);
                }

                if (handle.is_valid()) {
                    handles.add_item(handle);
                }
            }

            if (handles.get_count() == 0) {
                return {{"success", false}, {"error", "No valid files found"}};
            }

            // Use context menu to trigger ReplayGain scan
            // Path: "ReplayGain/Scan per-file track gain" or "ReplayGain/Scan selection as a single album"
            service_ptr_t<contextmenu_manager> cmm;
            contextmenu_manager::g_create(cmm);
            if (!cmm.is_valid()) {
                return {{"success", false}, {"error", "Failed to create context menu manager"}};
            }

            cmm->init_context(handles, 0);
            contextmenu_node* root = cmm->get_root();
            
            if (!root || root->get_type() != contextmenu_item_node::TYPE_POPUP) {
                return {{"success", false}, {"error", "Failed to get context menu"}};
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
                return {{"success", false}, {"error", "ReplayGain menu not found"}};
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
                return {{"success", false}, {"error", "Scan command not found in ReplayGain menu"}};
            }

            // Execute scan command
            scanCmd->execute();

            return {
                {"success", true},
                {"scannedCount", static_cast<int>(handles.get_count())},
                {"mode", mode},
                {"note", "Scan started. Results will be written to files automatically."}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

} // anonymous namespace

//==========================================================================
// Register ReplayGain API
//==========================================================================
void RegisterReplayGainApi() {
    auto& bridge = BridgeCore::GetInstance();

    // replaygain.getSettings - Get all ReplayGain settings
    bridge.RegisterApi("replaygain.getSettings", ReplayGainGetSettings);

    // replaygain.getMode - Get current mode
    bridge.RegisterApi("replaygain.getMode", ReplayGainGetMode);

    // replaygain.setMode - Set processing mode
    bridge.RegisterApi("replaygain.setMode", ReplayGainSetMode);

    // replaygain.getPreamp - Get preamp values
    bridge.RegisterApi("replaygain.getPreamp", ReplayGainGetPreamp);

    // replaygain.setPreamp - Set preamp values
    bridge.RegisterApi("replaygain.setPreamp", ReplayGainSetPreamp);

    // replaygain.get - Get ReplayGain info from files
    bridge.RegisterApi("replaygain.get", ReplayGainGet, {{"paths", SecurityLevel::MediaRead, true}});

    // replaygain.clear - Clear ReplayGain info from files
    bridge.RegisterApi("replaygain.clear", ReplayGainClear, {{"paths", SecurityLevel::MediaWrite, true}});

    // replaygain.scan - Scan ReplayGain for files
    bridge.RegisterApi("replaygain.scan", ReplayGainScan, {{"paths", SecurityLevel::MediaRead, true}});

    LOG("ReplayGain API registered (8 APIs)");
}
