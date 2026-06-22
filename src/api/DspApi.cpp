// DspApi.cpp - DSP Chain Management API
// Provides control over foobar2000's DSP processing chain

#include "pch.h"
#include "api/DspApi.h"
#include "api/BridgeCore.h"

#ifdef FOOBAR2000_HAVE_DSP
#include <foobar2000/SDK/dsp.h>
#include <foobar2000/SDK/dsp_manager.h>
#endif
#include "utils/GuidUtils.h"

namespace {
    using json = nlohmann::json;
    using GuidUtils::GuidToString;
    using GuidUtils::StringToGuid;


#ifdef FOOBAR2000_HAVE_DSP
    //==========================================================================
    // dsp.getChain - Get current DSP chain configuration
    //==========================================================================
    json DspGetChain(const json& /*params*/) {
        try {
            auto dsp_mgr = dsp_config_manager::get();
            dsp_chain_config_impl chain;
            dsp_mgr->get_core_settings(chain);

            json dsps = json::array();
            for (size_t i = 0; i < chain.get_count(); i++) {
                const dsp_preset& preset = chain.get_item(i);
                GUID owner = preset.get_owner();
                
                pfc::string8 name;
                dsp_entry::g_name_from_guid(name, owner);
                
                dsps.push_back({
                    {"index", i},
                    {"guid", GuidToString(owner)},
                    {"name", name.get_ptr()}
                });
            }

            // Get selected preset (if available)
            json result = {{"dsps", dsps}};
            
            // Try to get active preset name from v2 API
            try {
                auto dsp_mgr_v2 = dsp_config_manager_v2::get();
                size_t selected = dsp_mgr_v2->get_selected_preset();
                if (selected != pfc::infinite_size && selected < dsp_mgr_v2->get_preset_count()) {
                    pfc::string8 presetName;
                    dsp_mgr_v2->get_preset_name(selected, presetName);
                    result["activePreset"] = presetName.get_ptr();
                    result["activePresetIndex"] = selected;
                }
            } catch (...) {
                result["activePreset"] = nullptr;
            }

            return result;
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // dsp.getPresets - Get available DSP presets
    //==========================================================================
    json DspGetPresets(const json& /*params*/) {
        try {
            auto dsp_mgr_v2 = dsp_config_manager_v2::get();
            
            size_t count = dsp_mgr_v2->get_preset_count();
            size_t selected = dsp_mgr_v2->get_selected_preset();
            
            json presets = json::array();
            for (size_t i = 0; i < count; i++) {
                pfc::string8 name;
                dsp_mgr_v2->get_preset_name(i, name);
                presets.push_back({
                    {"index", i},
                    {"name", name.get_ptr()},
                    {"active", i == selected}
                });
            }

            return {
                {"presets", presets},
                {"count", count},
                {"selectedIndex", selected}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // dsp.applyPreset - Apply a DSP preset by name or index
    //==========================================================================
    json DspApplyPreset(const json& params) {
        try {
            auto dsp_mgr_v2 = dsp_config_manager_v2::get();
            
            size_t targetIndex = pfc::infinite_size;
            
            // Find by index
            if (params.contains("index") && params["index"].is_number()) {
                targetIndex = params["index"].get<size_t>();
            }
            // Find by name
            else if (params.contains("name") && params["name"].is_string()) {
                std::string targetName = params["name"].get<std::string>();
                size_t count = dsp_mgr_v2->get_preset_count();
                
                for (size_t i = 0; i < count; i++) {
                    pfc::string8 name;
                    dsp_mgr_v2->get_preset_name(i, name);
                    if (targetName == name.get_ptr()) {
                        targetIndex = i;
                        break;
                    }
                }
                
                if (targetIndex == pfc::infinite_size) {
                    return {{"success", false}, {"error", "Preset not found: " + targetName}};
                }
            } else {
                return {{"success", false}, {"error", "name or index parameter required"}};
            }

            if (targetIndex >= dsp_mgr_v2->get_preset_count()) {
                return {{"success", false}, {"error", "Invalid preset index"}};
            }

            // Apply preset
            dsp_mgr_v2->select_preset(targetIndex);
            
            pfc::string8 appliedName;
            dsp_mgr_v2->get_preset_name(targetIndex, appliedName);

            return {
                {"success", true},
                {"appliedPreset", appliedName.get_ptr()},
                {"appliedIndex", targetIndex}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // dsp.getAvailable - Get list of available DSP processors
    //==========================================================================
    json DspGetAvailable(const json& /*params*/) {
        try {
            json dsps = json::array();
            
            service_enum_t<dsp_entry> e;
            dsp_entry::ptr ptr;
            
            while (e.next(ptr)) {
                pfc::string8 name;
                ptr->get_name(name);
                
                bool hasConfig = ptr->have_config_popup();
                
                // Try to get default preset
                dsp_preset_impl defaultPreset;
                bool hasDefault = ptr->get_default_preset(defaultPreset);
                
                dsps.push_back({
                    {"guid", GuidToString(ptr->get_guid())},
                    {"name", name.get_ptr()},
                    {"hasConfig", hasConfig}
                });
            }

            return {
                {"dsps", dsps},
                {"count", dsps.size()}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // dsp.addDsp - Add a DSP to the chain
    //==========================================================================
    json DspAddDsp(const json& params) {
        std::string guidStr = params.value("guid", "");
        int position = params.value("position", -1);

        if (guidStr.empty()) {
            return {{"success", false}, {"error", "guid is required"}};
        }

        GUID guid;
        if (!StringToGuid(guidStr, guid)) {
            return {{"success", false}, {"error", "Invalid GUID format"}};
        }

        try {
            // Get default preset for this DSP
            dsp_preset_impl preset;
            if (!dsp_entry::g_get_default_preset(preset, guid)) {
                return {{"success", false}, {"error", "DSP not found or no default preset"}};
            }

            // Get current chain
            auto dsp_mgr = dsp_config_manager::get();
            dsp_chain_config_impl chain;
            dsp_mgr->get_core_settings(chain);

            // Insert at position
            if (position < 0 || position >= (int)chain.get_count()) {
                chain.insert_item(preset, chain.get_count());
            } else {
                chain.insert_item(preset, position);
            }

            // Apply new chain
            dsp_mgr->set_core_settings(chain);

            pfc::string8 name;
            dsp_entry::g_name_from_guid(name, guid);

            return {
                {"success", true},
                {"addedDsp", name.get_ptr()},
                {"position", position < 0 ? (int)chain.get_count() - 1 : position}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // dsp.removeDsp - Remove a DSP from the chain
    //==========================================================================
    json DspRemoveDsp(const json& params) {
        if (!params.contains("index") || !params["index"].is_number()) {
            return {{"success", false}, {"error", "index is required"}};
        }

        size_t index = params["index"].get<size_t>();

        try {
            auto dsp_mgr = dsp_config_manager::get();
            dsp_chain_config_impl chain;
            dsp_mgr->get_core_settings(chain);

            if (index >= chain.get_count()) {
                return {{"success", false}, {"error", "Index out of range"}};
            }

            // Get name before removing
            const dsp_preset& preset = chain.get_item(index);
            pfc::string8 name;
            dsp_entry::g_name_from_guid(name, preset.get_owner());

            // Remove
            chain.remove_item(index);

            // Apply
            dsp_mgr->set_core_settings(chain);

            return {
                {"success", true},
                {"removedDsp", name.get_ptr()},
                {"removedIndex", index}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // dsp.moveDsp - Move a DSP within the chain
    //==========================================================================
    json DspMoveDsp(const json& params) {
        if (!params.contains("from") || !params["from"].is_number()) {
            return {{"success", false}, {"error", "from index is required"}};
        }
        if (!params.contains("to") || !params["to"].is_number()) {
            return {{"success", false}, {"error", "to index is required"}};
        }

        size_t from = params["from"].get<size_t>();
        size_t to = params["to"].get<size_t>();

        try {
            auto dsp_mgr = dsp_config_manager::get();
            dsp_chain_config_impl chain;
            dsp_mgr->get_core_settings(chain);

            if (from >= chain.get_count() || to >= chain.get_count()) {
                return {{"success", false}, {"error", "Index out of range"}};
            }

            if (from == to) {
                return {{"success", true}, {"message", "No change needed"}};
            }

            // Get the preset to move
            dsp_preset_impl preset;
            preset.copy(chain.get_item(from));
            pfc::string8 name;
            dsp_entry::g_name_from_guid(name, preset.get_owner());

            // Remove from original position
            chain.remove_item(from);

            // Adjust target position: removing from earlier index shifts later items left by 1
            size_t actualTo = (from < to) ? to - 1 : to;

            // Insert at new position
            chain.insert_item(preset, actualTo);

            // Apply
            dsp_mgr->set_core_settings(chain);

            return {
                {"success", true},
                {"movedDsp", name.get_ptr()},
                {"from", from},
                {"to", actualTo}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // dsp.setChain - Set complete DSP chain (advanced)
    //==========================================================================
    json DspSetChain(const json& params) {
        if (!params.contains("dsps") || !params["dsps"].is_array()) {
            return {{"success", false}, {"error", "dsps array is required"}};
        }

        try {
            dsp_chain_config_impl newChain;

            for (const auto& item : params["dsps"]) {
                std::string guidStr = item.value("guid", "");
                if (guidStr.empty()) continue;

                GUID guid;
                if (!StringToGuid(guidStr, guid)) continue;

                dsp_preset_impl preset;
                if (dsp_entry::g_get_default_preset(preset, guid)) {
                    newChain.insert_item(preset, newChain.get_count());
                }
            }

            auto dsp_mgr = dsp_config_manager::get();
            dsp_mgr->set_core_settings(newChain);

            return {
                {"success", true},
                {"count", newChain.get_count()}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

#else
    // Fallback for non-DSP builds
    json DspNotAvailable(const json& /*params*/) {
        return {{"success", false}, {"error", "DSP API not available in this build"}};
    }
#endif

} // anonymous namespace

//==========================================================================
// Register DSP API
//==========================================================================
void RegisterDspApi() {
    auto& bridge = BridgeCore::GetInstance();

#ifdef FOOBAR2000_HAVE_DSP
    // dsp.getChain - Get current DSP chain configuration
    bridge.RegisterApi("dsp.getChain", DspGetChain);

    // dsp.getPresets - Get available DSP presets
    bridge.RegisterApi("dsp.getPresets", DspGetPresets);

    // dsp.applyPreset - Apply a DSP preset
    bridge.RegisterApi("dsp.applyPreset", DspApplyPreset);

    // dsp.getAvailable - Get list of available DSP processors
    bridge.RegisterApi("dsp.getAvailable", DspGetAvailable);

    // dsp.addDsp - Add a DSP to the chain
    bridge.RegisterApi("dsp.addDsp", DspAddDsp);

    // dsp.removeDsp - Remove a DSP from the chain
    bridge.RegisterApi("dsp.removeDsp", DspRemoveDsp);

    // dsp.moveDsp - Move a DSP within the chain
    bridge.RegisterApi("dsp.moveDsp", DspMoveDsp);

    // dsp.setChain - Set complete DSP chain
    bridge.RegisterApi("dsp.setChain", DspSetChain);

    LOG("DSP API registered (8 APIs)");
#else
    bridge.RegisterApi("dsp.getChain", DspNotAvailable);
    bridge.RegisterApi("dsp.getPresets", DspNotAvailable);
    bridge.RegisterApi("dsp.applyPreset", DspNotAvailable);
    bridge.RegisterApi("dsp.getAvailable", DspNotAvailable);
    bridge.RegisterApi("dsp.addDsp", DspNotAvailable);
    bridge.RegisterApi("dsp.removeDsp", DspNotAvailable);
    bridge.RegisterApi("dsp.moveDsp", DspNotAvailable);
    bridge.RegisterApi("dsp.setChain", DspNotAvailable);

    LOG("DSP API registered (stub - DSP not available)");
#endif
}
