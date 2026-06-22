// ConfigApi.cpp - Configuration and Preferences API
// Provides access to foobar2000 configuration settings for custom preferences UI

#include "pch.h"
#include "api/ConfigApi.h"
#include "api/BridgeCore.h"
#include "version.h"
#include <foobar2000/SDK/replaygain.h>
#include "utils/GuidUtils.h"

namespace {
    using json = nlohmann::json;
    
    using GuidUtils::GuidToString;
    using GuidUtils::StringToGuid;
    
    //==========================================================================
    // OUTPUT DEVICE APIs
    //==========================================================================
    
    // Get list of all available output devices
    json GetOutputDevices(const json& /*params*/) {
        json devices = json::array();
        
        // Get current output config to mark isCurrent
        GUID currentOutput = {}, currentDevice = {};
        try {
            auto mgr = output_manager::get();
            outputCoreConfig_t config = mgr->getCoreConfig();
            currentOutput = config.m_output;
            currentDevice = config.m_device;
        } catch (...) {}
        
        try {
            auto mgr = output_manager_v2::get();
            
            // List all devices
            mgr->listDevices([&devices, &currentOutput, &currentDevice](const char* fullName, const GUID& output, const GUID& device) {
                json dev;
                dev["name"] = fullName;
                dev["id"] = GuidToString(device);
                dev["outputId"] = GuidToString(output);
                dev["deviceId"] = GuidToString(device);
                dev["isCurrent"] = (output == currentOutput && device == currentDevice);
                devices.push_back(dev);
            });
        } catch (...) {
            // Fallback: enumerate via output_entry
            service_enum_t<output_entry> e;
            service_ptr_t<output_entry> ptr;
            while (e.next(ptr)) {
                pfc::string8 name;
                name = ptr->get_name();
                
                struct device_enum : output_device_enum_callback {
                    json& devices;
                    GUID outputGuid;
                    const char* outputName;
                    GUID currentOutput, currentDevice;
                    device_enum(json& d, GUID g, const char* n, GUID co, GUID cd) 
                        : devices(d), outputGuid(g), outputName(n), currentOutput(co), currentDevice(cd) {}
                    
                    void on_device(const GUID& deviceGuid, const char* deviceName, unsigned nameLen) override {
                        json dev;
                        pfc::string8 fullName;
                        fullName << outputName << ": " << pfc::string8(deviceName, nameLen);
                        dev["name"] = fullName.get_ptr();
                        dev["id"] = GuidToString(deviceGuid);
                        dev["outputId"] = GuidToString(outputGuid);
                        dev["deviceId"] = GuidToString(deviceGuid);
                        dev["isCurrent"] = (outputGuid == currentOutput && deviceGuid == currentDevice);
                        devices.push_back(dev);
                    }
                } callback(devices, ptr->get_guid(), name.get_ptr(), currentOutput, currentDevice);
                
                ptr->enum_devices(callback);
            }
        }
        
        return devices;
    }
    
    // Get current output configuration
    json GetOutputConfig(const json& /*params*/) {
        json result;
        
        try {
            auto mgr = output_manager::get();
            outputCoreConfig_t config = mgr->getCoreConfig();
            
            result["outputId"] = GuidToString(config.m_output);
            result["deviceId"] = GuidToString(config.m_device);
            result["bufferLength"] = config.m_buffer_length;
            result["bitDepth"] = config.m_bitDepth;
            result["useDither"] = (config.m_flags & outputCoreConfig_t::flagUseDither) != 0;
            result["useFades"] = (config.m_flags & outputCoreConfig_t::flagUseFades) != 0;
            
            // Try to get device name
            auto entry = output_entry::g_find(config.m_output);
            if (entry.is_valid()) {
                result["outputName"] = entry->get_name();
                pfc::string8 deviceName;
                if (entry->get_device_name(config.m_device, deviceName)) {
                    result["deviceName"] = deviceName.get_ptr();
                }
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to get output config: ") + e.what());
        }
        
        return result;
    }
    
    // Set output device
    json SetOutputDevice(const json& params) {
        std::string outputIdStr = params.value("outputId", "");
        std::string deviceIdStr = params.value("deviceId", "");
        
        if (outputIdStr.empty() || deviceIdStr.empty()) {
            throw std::runtime_error("outputId and deviceId are required");
        }
        
        GUID outputId, deviceId;
        if (!StringToGuid(outputIdStr, outputId) || !StringToGuid(deviceIdStr, deviceId)) {
            throw std::runtime_error("Invalid GUID format");
        }
        
        try {
            auto mgr = output_manager_v2::get();
            mgr->setCoreConfigDevice(outputId, deviceId);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to set output device: ") + e.what());
        }
        
        return json{{"success", true}};
    }
    
    // Set output buffer length
    json SetOutputBuffer(const json& params) {
        // Support both 'bufferLength' (seconds) and 'milliseconds' parameter formats
        double bufferLength;
        if (params.contains("milliseconds")) {
            bufferLength = params.value("milliseconds", 0.0) / 1000.0;  // Convert ms to seconds
        } else {
            bufferLength = params.value("bufferLength", 0.0);
        }
        
        if (bufferLength < 0.05 || bufferLength > 2.0) {
            throw std::runtime_error("bufferLength must be between 0.05 and 2.0 seconds");
        }
        
        try {
            auto mgr = output_manager::get();
            outputCoreConfig_t config = mgr->getCoreConfig();
            config.m_buffer_length = bufferLength;
            
            auto mgr2 = output_manager_v2::get();
            mgr2->setCoreConfig(config);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to set buffer length: ") + e.what());
        }
        
        return json{{"success", true}};
    }
    
    //==========================================================================
    // ADVANCED CONFIG APIs
    //==========================================================================
    
    // Helper to build advconfig tree
    void BuildAdvConfigTree(const GUID& parentGuid, json& items, int depth = 0) {
        if (depth > 10) return; // Prevent infinite recursion
        
        service_enum_t<advconfig_entry> e;
        service_ptr_t<advconfig_entry> ptr;
        
        while (e.next(ptr)) {
            if (ptr->get_parent() == parentGuid) {
                json item;
                
                pfc::string8 name;
                ptr->get_name(name);
                item["name"] = name.get_ptr();
                item["guid"] = GuidToString(ptr->get_guid());
                item["sortPriority"] = ptr->get_sort_priority();
                
                // Check if it's a branch
                service_ptr_t<advconfig_branch> branch;
                if (ptr->service_query_t(branch)) {
                    item["type"] = "branch";
                    item["children"] = json::array();
                    BuildAdvConfigTree(ptr->get_guid(), item["children"], depth + 1);
                }
                // Check if it's a checkbox
                else {
                    service_ptr_t<advconfig_entry_checkbox> checkbox;
                    if (ptr->service_query_t(checkbox)) {
                        item["type"] = checkbox->is_radio() ? "radio" : "checkbox";
                        item["value"] = checkbox->get_state();
                        
                        // Try to get default value
                        service_ptr_t<advconfig_entry_checkbox_v2> cb2;
                        if (ptr->service_query_t(cb2)) {
                            item["defaultValue"] = cb2->get_default_state();
                        }
                    }
                    // Check if it's a string/integer
                    else {
                        service_ptr_t<advconfig_entry_string> stringEntry;
                        if (ptr->service_query_t(stringEntry)) {
                            pfc::string8 value;
                            stringEntry->get_state(value);
                            
                            t_uint32 flags = stringEntry->get_flags();
                            bool isInteger = (flags & advconfig_entry_string::flag_is_integer) != 0;
                            
                            item["type"] = isInteger ? "integer" : "string";
                            item["value"] = value.get_ptr();
                            item["isSigned"] = (flags & advconfig_entry_string::flag_is_signed) != 0;
                            item["isFilePath"] = (flags & advconfig_entry_string::flag_is_file_path) != 0;
                            item["isFolderPath"] = (flags & advconfig_entry_string::flag_is_folder_path) != 0;
                            
                            // Try to get default value
                            service_ptr_t<advconfig_entry_string_v2> str2;
                            if (ptr->service_query_t(str2)) {
                                pfc::string8 defaultVal;
                                str2->get_default_state(defaultVal);
                                item["defaultValue"] = defaultVal.get_ptr();
                            }
                        }
                    }
                }
                
                items.push_back(item);
            }
        }
    }
    
    // Get all advanced config entries
    json GetAdvancedConfig(const json& params) {
        std::string parentGuidStr = params.value("parentGuid", "");
        
        GUID parentGuid = advconfig_entry::guid_root;
        if (!parentGuidStr.empty()) {
            StringToGuid(parentGuidStr, parentGuid);
        }
        
        json result = json::array();
        BuildAdvConfigTree(parentGuid, result);
        return result;
    }
    
    // Get specific advanced config value
    json GetAdvancedConfigValue(const json& params) {
        std::string guidStr = params.value("guid", "");
        if (guidStr.empty()) {
            throw std::runtime_error("guid is required");
        }
        
        GUID guid;
        if (!StringToGuid(guidStr, guid)) {
            throw std::runtime_error("Invalid GUID format");
        }
        
        service_ptr_t<advconfig_entry> entry;
        if (!advconfig_entry::g_find(entry, guid)) {
            throw std::runtime_error("Config entry not found");
        }
        
        json result;
        pfc::string8 name;
        entry->get_name(name);
        result["name"] = name.get_ptr();
        result["guid"] = guidStr;
        
        // Branch (group/folder) - check first as it doesn't have a value
        service_ptr_t<advconfig_branch> branch;
        if (entry->service_query_t(branch)) {
            result["type"] = "branch";
            result["value"] = nullptr;
            return result;
        }
        
        // Checkbox
        service_ptr_t<advconfig_entry_checkbox> checkbox;
        if (entry->service_query_t(checkbox)) {
            result["type"] = checkbox->is_radio() ? "radio" : "checkbox";
            result["value"] = checkbox->get_state();
            return result;
        }
        
        // String (note: integers are also advconfig_entry_string with flag_is_integer)
        service_ptr_t<advconfig_entry_string> stringEntry;
        if (entry->service_query_t(stringEntry)) {
            pfc::string8 value;
            stringEntry->get_state(value);
            
            // Check if this is an integer type
            t_uint32 flags = stringEntry->get_flags();
            if (flags & advconfig_entry_string::flag_is_integer) {
                result["type"] = "integer";
                // Parse string as integer
                try {
                    result["value"] = std::stoll(value.get_ptr());
                } catch (...) {
                    result["value"] = 0;
                }
            } else {
                result["type"] = "string";
                result["value"] = value.get_ptr();
            }
            return result;
        }
        
        // Unknown type - return type info without value
        result["type"] = "unknown";
        result["value"] = nullptr;
        return result;
    }
    
    // Set advanced config value
    json SetAdvancedConfigValue(const json& params) {
        std::string guidStr = params.value("guid", "");
        if (guidStr.empty()) {
            throw std::runtime_error("guid is required");
        }
        
        GUID guid;
        if (!StringToGuid(guidStr, guid)) {
            throw std::runtime_error("Invalid GUID format");
        }
        
        service_ptr_t<advconfig_entry> entry;
        if (!advconfig_entry::g_find(entry, guid)) {
            throw std::runtime_error("Config entry not found");
        }
        
        // Checkbox
        service_ptr_t<advconfig_entry_checkbox> checkbox;
        if (entry->service_query_t(checkbox)) {
            if (!params.contains("value") || !params["value"].is_boolean()) {
                throw std::runtime_error("Boolean value required for checkbox");
            }
            checkbox->set_state(params["value"].get<bool>());
            return json{{"success", true}};
        }
        
        // String (integers are also advconfig_entry_string with flag_is_integer)
        service_ptr_t<advconfig_entry_string> stringEntry;
        if (entry->service_query_t(stringEntry)) {
            if (!params.contains("value")) {
                throw std::runtime_error("value is required");
            }
            
            std::string value;
            const auto& jsonVal = params["value"];
            
            // Check if this is an integer config
            t_uint32 flags = 0;
            service_ptr_t<advconfig_entry_string_v2> v2;
            if (stringEntry->service_query_t(v2)) {
                flags = v2->get_preferences_flags();
            }
            bool isIntegerConfig = (flags & advconfig_entry_string::flag_is_integer) != 0;
            
            if (jsonVal.is_number_integer()) {
                // Accept integer JSON values (especially for integer configs)
                value = std::to_string(jsonVal.get<int64_t>());
            } else if (jsonVal.is_number_float()) {
                // Accept float JSON values — truncate to integer for integer configs
                if (isIntegerConfig) {
                    value = std::to_string(static_cast<int64_t>(jsonVal.get<double>()));
                } else {
                    value = std::to_string(jsonVal.get<double>());
                }
            } else if (jsonVal.is_string()) {
                value = jsonVal.get<std::string>();
            } else {
                throw std::runtime_error(isIntegerConfig ? "Integer or string value required" : "String value required");
            }
            
            stringEntry->set_state(value.c_str(), value.length());
            return json{{"success", true}};
        }
        
        throw std::runtime_error("Unknown config entry type");
    }
    
    // Reset advanced config to default
    json ResetAdvancedConfig(const json& params) {
        std::string guidStr = params.value("guid", "");
        if (guidStr.empty()) {
            throw std::runtime_error("guid is required");
        }
        
        GUID guid;
        if (!StringToGuid(guidStr, guid)) {
            throw std::runtime_error("Invalid GUID format");
        }
        
        service_ptr_t<advconfig_entry> entry;
        if (!advconfig_entry::g_find(entry, guid)) {
            throw std::runtime_error("Config entry not found");
        }
        
        entry->reset();
        return json{{"success", true}};
    }
    
    //==========================================================================
    // PREFERENCES PAGE APIs
    //==========================================================================
    
    // Get list of all preferences pages
    json GetPreferencesPages(const json& /*params*/) {
        json pages = json::array();
        
        // Enumerate preferences pages
        service_enum_t<preferences_page> e;
        service_ptr_t<preferences_page> ptr;
        
        while (e.next(ptr)) {
            json page;
            page["name"] = ptr->get_name();
            page["guid"] = GuidToString(ptr->get_guid());
            page["parentGuid"] = GuidToString(ptr->get_parent_guid());
            
            // Get sort priority if available
            service_ptr_t<preferences_page_v2> v2;
            if (ptr->service_query_t(v2)) {
                page["sortPriority"] = v2->get_sort_priority();
            } else {
                page["sortPriority"] = 0.0;
            }
            
            pages.push_back(page);
        }
        
        // Also enumerate branches
        service_enum_t<preferences_branch> eb;
        service_ptr_t<preferences_branch> branchPtr;
        
        while (eb.next(branchPtr)) {
            json branch;
            branch["name"] = branchPtr->get_name();
            branch["guid"] = GuidToString(branchPtr->get_guid());
            branch["parentGuid"] = GuidToString(branchPtr->get_parent_guid());
            branch["isBranch"] = true;
            
            service_ptr_t<preferences_branch_v2> v2;
            if (branchPtr->service_query_t(v2)) {
                branch["sortPriority"] = v2->get_sort_priority();
            } else {
                branch["sortPriority"] = 0.0;
            }
            
            pages.push_back(branch);
        }
        
        return pages;
    }
    
    // Get standard preference page GUIDs
    json GetPreferencesStandardGuids(const json& /*params*/) {
        json guids;
        guids["root"] = GuidToString(preferences_page::guid_root);
        guids["hidden"] = GuidToString(preferences_page::guid_hidden);
        guids["tools"] = GuidToString(preferences_page::guid_tools);
        guids["core"] = GuidToString(preferences_page::guid_core);
        guids["display"] = GuidToString(preferences_page::guid_display);
        guids["playback"] = GuidToString(preferences_page::guid_playback);
        guids["visualisations"] = GuidToString(preferences_page::guid_visualisations);
        guids["input"] = GuidToString(preferences_page::guid_input);
        guids["tagWriting"] = GuidToString(preferences_page::guid_tag_writing);
        guids["mediaLibrary"] = GuidToString(preferences_page::guid_media_library);
        guids["tagging"] = GuidToString(preferences_page::guid_tagging);
        guids["output"] = GuidToString(preferences_page::guid_output);
        guids["advanced"] = GuidToString(preferences_page::guid_advanced);
        guids["components"] = GuidToString(preferences_page::guid_components);
        guids["dsp"] = GuidToString(preferences_page::guid_dsp);
        guids["shell"] = GuidToString(preferences_page::guid_shell);
        guids["keyboardShortcuts"] = GuidToString(preferences_page::guid_keyboard_shortcuts);
        return guids;
    }
    
    //==========================================================================
    // MEDIA LIBRARY APIs
    //==========================================================================
    
    // Get media library status
    json GetLibraryStatus(const json& /*params*/) {
        json result;
        
        auto lib = library_manager::get();
        result["enabled"] = lib->is_library_enabled();
        
        // Get item count
        struct counter : library_manager::enum_callback {
            size_t count =0;
            bool on_item(const metadb_handle_ptr&) override { count++; return true; }
        } c;
        lib->enum_items(c);
        result["itemCount"] = c.count;
        
        // Check if initialized (v4+)
        try {
            auto lib4 = library_manager_v4::get();
            result["initialized"] = lib4->is_initialized();
        } catch (...) {
            result["initialized"] = true; // Assume initialized for older versions
        }
        
        return result;
    }
    
    // Get library file patterns
    json GetLibraryFilePatterns(const json& /*params*/) {
        json result;
        
        try {
            auto lib = library_manager_v3::get();
            
            pfc::string8 tracksDir, tracksFormat;
            if (lib->get_new_file_pattern_tracks(tracksDir, tracksFormat)) {
                result["tracks"]["directory"] = tracksDir.get_ptr();
                result["tracks"]["format"] = tracksFormat.get_ptr();
            }
            
            pfc::string8 imagesDir, imagesFormat;
            if (lib->get_new_file_pattern_images(imagesDir, imagesFormat)) {
                result["images"]["directory"] = imagesDir.get_ptr();
                result["images"]["format"] = imagesFormat.get_ptr();
            }
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to get library patterns: ") + e.what());
        }
        
        return result;
    }
    
    // Show library preferences
    json ShowLibraryPreferences(const json& /*params*/) {
        library_manager::get()->show_preferences();
        return json{{"success", true}};
    }
    
    //==========================================================================
    // COMPONENT INFO APIs
    //==========================================================================
    
    // Get list of installed components
    json GetComponents(const json& /*params*/) {
        json components = json::array();
        
        service_enum_t<componentversion> e;
        service_ptr_t<componentversion> ptr;
        
        while (e.next(ptr)) {
            json comp;
            
            pfc::string8 name, version, about;
            ptr->get_component_name(name);
            ptr->get_component_version(version);
            
            comp["name"] = name.get_ptr();
            comp["version"] = version.get_ptr();
            
            // Try to get file name
            pfc::string8 fileName;
            ptr->get_file_name(fileName);
            if (!fileName.is_empty()) {
                comp["fileName"] = fileName.get_ptr();
                comp["filename"] = fileName.get_ptr();
            }
            
            components.push_back(comp);
        }
        
        return components;
    }
    
    // Get foobar2000 version info
    json GetVersionInfo(const json& /*params*/) {
        json result;
        result["version"] = core_version_info::g_get_version_string();
        result["foobar2000"] = core_version_info::g_get_version_string();
        result["versionFull"] = core_version_info_v2::get()->get_name();
        result["is64bit"] = (sizeof(void*) == 8);
        result["isPortable"] = core_api::is_portable_mode_enabled();
        
        // Plugin info
        result["plugin"] = {
            {"name", "foo_ui_webview2"},
            {"version", PLUGIN_VERSION_STR},
        };
        
        // Profile path
        pfc::string8 profilePath;
        filesystem::g_get_display_path(core_api::get_profile_path(), profilePath);
        result["profilePath"] = profilePath.get_ptr();
        
        return result;
    }
    
    //==========================================================================
    // DSP CONFIG APIs
    //==========================================================================
    
    // Get list of available DSP presets
    json GetDspPresets(const json& /*params*/) {
        json presets = json::array();
        
        try {
            auto dsp = dsp_config_manager_v2::get();
            t_size count = dsp->get_preset_count();
            
            for (t_size i = 0; i < count; i++) {
                pfc::string8 name;
                dsp->get_preset_name(i, name);
                
                json preset;
                preset["index"] = i;
                preset["name"] = name.get_ptr();
                presets.push_back(preset);
            }
        } catch (...) {
            // DSP manager v2 not available
        }
        
        return presets;
    }
    
    // Get active DSP preset
    json GetActiveDspPreset(const json& /*params*/) {
        json result;
        
        try {
            auto dsp = dsp_config_manager_v2::get();
            t_size active = dsp->get_selected_preset();
            
            if (active != pfc::infinite_size) {
                pfc::string8 name;
                dsp->get_preset_name(active, name);
                result["index"] = active;
                result["name"] = name.get_ptr();
                result["isActive"] = true;
            } else {
                result["index"] = nullptr;
                result["name"] = nullptr;
                result["isActive"] = false;
            }
        } catch (...) {
            result["index"] = nullptr;
            result["name"] = nullptr;
            result["isActive"] = false;
        }
        
        return result;
    }
    
    // Set active DSP preset
    json SetActiveDspPreset(const json& params) {
        if (!params.contains("index")) {
            throw std::runtime_error("index is required");
        }
        
        t_size index = params["index"].get<t_size>();
        
        try {
            auto dsp = dsp_config_manager_v2::get();
            if (index >= dsp->get_preset_count()) {
                throw std::runtime_error("Invalid preset index");
            }
            dsp->select_preset(index);
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to set DSP preset: ") + e.what());
        }
        
        return json{{"success", true}};
    }

    //==========================================================================
    // PLAYBACK FOLLOW/CURSOR CONFIG APIs
    //==========================================================================

    json GetCursorFollowPlayback(const json& /*params*/) {
        bool enabled = config_object::g_get_data_bool_simple(standard_config_objects::bool_cursor_follows_playback, false);
        return { {"enabled", enabled}, {"value", enabled} };
    }

    json SetCursorFollowPlayback(const json& params) {
        bool enabled = params.contains("enabled")
            ? params.value("enabled", false)
            : params.value("value", false);
        config_object::g_set_data_bool(standard_config_objects::bool_cursor_follows_playback, enabled);
        return { {"success", true}, {"enabled", enabled} };
    }

    json GetPlaybackFollowCursor(const json& /*params*/) {
        bool enabled = config_object::g_get_data_bool_simple(standard_config_objects::bool_playback_follows_cursor, false);
        return { {"enabled", enabled}, {"value", enabled} };
    }

    json SetPlaybackFollowCursor(const json& params) {
        bool enabled = params.contains("enabled")
            ? params.value("enabled", false)
            : params.value("value", false);
        config_object::g_set_data_bool(standard_config_objects::bool_playback_follows_cursor, enabled);
        return { {"success", true}, {"enabled", enabled} };
    }

    //==========================================================================
    // REPLAYGAIN MODE APIs
    //==========================================================================

    json GetReplaygainMode(const json& /*params*/) {
        auto mgr = replaygain_manager::get();
        t_replaygain_config cfg = mgr->get_core_settings();
        return { {"mode", static_cast<int>(cfg.m_source_mode)}, {"value", static_cast<int>(cfg.m_source_mode)} };
    }

    json SetReplaygainMode(const json& params) {
        auto mgr = replaygain_manager::get();
        t_replaygain_config cfg = mgr->get_core_settings();

        int mode = params.value("mode", -1);
        if (mode < 0 && params.contains("value")) {
            mode = params.value("value", -1);
        }
        if (mode < 0 && params.contains("sourceMode") && params["sourceMode"].is_string()) {
            std::string s = params["sourceMode"].get<std::string>();
            if (s == "track") mode = t_replaygain_config::source_mode_track;
            else if (s == "album") mode = t_replaygain_config::source_mode_album;
            else if (s == "auto" || s == "byPlaybackOrder") mode = t_replaygain_config::source_mode_byPlaybackOrder;
            else if (s == "none") mode = t_replaygain_config::source_mode_none;
            else return {{"error", "Unknown replaygain mode: " + s}, {"code", "INVALID_PARAMS"}};
        }
        if (mode < 0) mode = t_replaygain_config::source_mode_none;

        cfg.m_source_mode = static_cast<t_uint32>(mode);
        mgr->set_core_settings(cfg);

        return { {"success", true}, {"mode", mode}, {"value", mode} };
    }

} // anonymous namespace

// GUID for legacy cfg_string storage (保留用于迁移，迁移完成后不再写入)
static constexpr GUID guid_cfg_portable_config = 
    { 0xb7e8f3a7, 0x4c5d, 0x2e9f, { 0x8a, 0x1b, 0x6c, 0x7d, 0x8e, 0x9f, 0x0a, 0x2e } };
static cfg_string cfg_portable_config(guid_cfg_portable_config, "{}");

// configStore key for new SQLite-backed persistent storage (foobar2000 v2.0+)
// configStore 在写入时立即 commit（有 delay-write cache 兜底），重启和崩溃均不丢数据
static constexpr const char* kConfigStoreKey = "foo_ui_webview2.portable_config";

// Memory cache for performance
static json g_configCache;
static bool g_configCacheValid = false;

// Helper: Get config from cache or load from persistent storage
// 优先从 configStore (SQLite) 读取；若不存在则尝试从旧 cfg_string 迁移
static json& GetConfigCache() {
    if (!g_configCacheValid) {
        try {
            // 优先读取新存储 (configStore / SQLite)
            auto store = fb2k::configStore::get();
            auto valRef = store->getConfigString(kConfigStoreKey, "");
            const char* configStr = valRef.is_valid() ? valRef->c_str() : nullptr;

            if (configStr && configStr[0]) {
                g_configCache = json::parse(configStr);
            } else {
                // 新存储为空时，检查旧 cfg_string 是否有数据可迁移
                const char* legacyStr = cfg_portable_config.get();
                if (legacyStr && legacyStr[0] && strcmp(legacyStr, "{}") != 0) {
                    try {
                        g_configCache = json::parse(legacyStr);
                        // 迁移到新存储
                        store->setConfigString(kConfigStoreKey, g_configCache.dump().c_str());
                        console::printf("[ConfigApi] Migrated config from cfg_string to configStore (%zu keys)",
                                        g_configCache.size());
                    } catch (...) {
                        g_configCache = json::object();
                    }
                } else {
                    g_configCache = json::object();
                }
            }
        } catch (const std::exception& e) {
            console::printf("[ConfigApi] Failed to load config cache: %s", e.what());
            g_configCache = json::object();
        }
        g_configCacheValid = true;
    }
    return g_configCache;
}

// Helper: Save config cache to persistent storage (configStore — SQLite 立即 commit)
static void SaveConfigCache() {
    try {
        auto store = fb2k::configStore::get();
        store->setConfigString(kConfigStoreKey, g_configCache.dump().c_str());
        g_configCacheValid = true;
    } catch (const std::exception& e) {
        console::printf("[ConfigApi] Failed to save config cache: %s", e.what());
    }
}

//==========================================================================
// Register all config APIs
//==========================================================================

// ==========================================================================
// Config API handler functions
// ==========================================================================
namespace {


// ========== Persistent Config Storage APIs ==========
// These store key-value pairs in foobar2000's config system

json ConfigSet(const json& params) {
    std::string key = params.value("key", "");
    if (key.empty()) {
        return {{"success", false}, {"error", "key is required"}};
    }
    if (!params.contains("value")) {
        return {{"success", false}, {"error", "value is required"}};
    }
    
    // Get config cache and update
    json& config = GetConfigCache();
    config[key] = params["value"];
    
    // Save to persistent storage
    SaveConfigCache();
    
    return {{"success", true}, {"key", key}};
}


json ConfigGet(const json& params) {
    std::string key = params.value("key", "");
    if (key.empty()) {
        return {{"success", false}, {"error", "key is required"}};
    }
    
    // Get config cache
    json& config = GetConfigCache();
    auto it = config.find(key);
    
    if (it != config.end()) {
        return {{"success", true}, {"key", key}, {"value", it.value()}, {"found", true}};
    }
    
    // Check for default value
    if (params.contains("default")) {
        return {{"success", true}, {"key", key}, {"value", params["default"]}, {"found", false}};
    }
    
    return {{"success", true}, {"key", key}, {"value", nullptr}, {"found", false}};
}


json ConfigRemove(const json& params) {
    std::string key = params.value("key", "");
    if (key.empty()) {
        return {{"success", false}, {"error", "key is required"}};
    }
    
    // Get config cache
    json& config = GetConfigCache();
    auto it = config.find(key);
    bool existed = (it != config.end());
    
    if (existed) {
        config.erase(it);
        // Save to persistent storage
        SaveConfigCache();
    }
    
    return {{"success", true}, {"key", key}, {"existed", existed}};
}


json ConfigGetAll(const json& params) {
    // Get config cache
    json& config = GetConfigCache();
    
    return {
        {"success", true}, 
        {"items", config}, 
        {"configs", config},  // alias for test compatibility
        {"count", config.size()}
    };
}


json ConfigExport(const json& params) {
    // Get config cache
    json& config = GetConfigCache();
    
    // Return both object and a serialized JSON string for compatibility
    return {
        {"success", true}, 
        {"data", config}, 
        {"json", config.dump()},  // Serialized JSON string for tests expecting string
        {"count", config.size()}
    };
}

} // namespace

void RegisterConfigApi() {
    auto& bridge = BridgeCore::GetInstance();

    // 便携配置存储
    bridge.RegisterApi("config.set", ConfigSet);
    bridge.RegisterApi("config.get", ConfigGet);
    bridge.RegisterApi("config.remove", ConfigRemove);
    bridge.RegisterApi("config.getAll", ConfigGetAll);
    bridge.RegisterApi("config.export", ConfigExport);

    // 输出设备
    bridge.RegisterApi("config.getOutputDevices", GetOutputDevices);
    bridge.RegisterApi("config.getOutputConfig", GetOutputConfig);
    bridge.RegisterApi("config.setOutputDevice", SetOutputDevice);
    bridge.RegisterApi("config.setOutputBuffer", SetOutputBuffer);

    // 高级配置
    bridge.RegisterApi("config.getAdvancedConfig", GetAdvancedConfig);
    bridge.RegisterApi("config.getAdvancedConfigValue", GetAdvancedConfigValue);
    bridge.RegisterApi("config.setAdvancedConfigValue", SetAdvancedConfigValue);
    bridge.RegisterApi("config.resetAdvancedConfig", ResetAdvancedConfig);

    // 首选项与组件
    bridge.RegisterApi("config.getPreferencesPages", GetPreferencesPages);
    bridge.RegisterApi("config.getPreferencesStandardGuids", GetPreferencesStandardGuids);
    bridge.RegisterApi("config.getComponents", GetComponents);
    bridge.RegisterApi("config.getVersionInfo", GetVersionInfo);

    // 媒体库
    bridge.RegisterApi("config.getLibraryStatus", GetLibraryStatus);
    bridge.RegisterApi("config.getLibraryFilePatterns", GetLibraryFilePatterns);
    bridge.RegisterApi("config.showLibraryPreferences", ShowLibraryPreferences);

    // DSP
    bridge.RegisterApi("config.getDspPresets", GetDspPresets);
    bridge.RegisterApi("config.getActiveDspPreset", GetActiveDspPreset);
    bridge.RegisterApi("config.setActiveDspPreset", SetActiveDspPreset);

    // 播放行为
    bridge.RegisterApi("config.getCursorFollowPlayback", GetCursorFollowPlayback);
    bridge.RegisterApi("config.setCursorFollowPlayback", SetCursorFollowPlayback);
    bridge.RegisterApi("config.getPlaybackFollowCursor", GetPlaybackFollowCursor);
    bridge.RegisterApi("config.setPlaybackFollowCursor", SetPlaybackFollowCursor);

    // ReplayGain
    bridge.RegisterApi("config.getReplaygainMode", GetReplaygainMode);
    bridge.RegisterApi("config.setReplaygainMode", SetReplaygainMode);
}
