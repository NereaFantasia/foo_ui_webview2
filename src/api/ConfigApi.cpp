// ConfigApi.cpp - Configuration and Preferences API
// Provides access to foobar2000 configuration settings for custom preferences UI
// 形状由 src/api/schema/config.ts 声明，结构体与参数解析来自生成的 ConfigSchema.h。

#include "pch.h"
#include "api/ConfigApi.h"
#include "api/BridgeCore.h"
#include "api/TypedApi.h"
#include "api/generated/ConfigSchema.h"
#include "version.h"
#include <foobar2000/SDK/replaygain.h>
#include "utils/GuidUtils.h"

namespace {
    using json = nlohmann::json;
    // 别名不叫 config：本文件多个 handler 有名为 config 的局部变量。
    namespace cfg = api::config;

    using GuidUtils::GuidToString;
    using GuidUtils::StringToGuid;

    //==========================================================================
    // OUTPUT DEVICE APIs
    //==========================================================================

    // One row of config.getOutputDevices; `id` repeats `deviceId`.
    cfg::ConfigOutputDevice MakeOutputDevice(const char* name, const GUID& output, const GUID& device,
                                             const GUID& currentOutput, const GUID& currentDevice) {
        cfg::ConfigOutputDevice dev;
        dev.name = name;
        dev.id = GuidToString(device);
        dev.outputId = GuidToString(output);
        dev.deviceId = dev.id;
        dev.isCurrent = (output == currentOutput && device == currentDevice);
        return dev;
    }

    // Get list of all available output devices
    api::Result<cfg::GetOutputDevicesResult> GetOutputDevices(const cfg::GetOutputDevicesParams& /*params*/) {
        cfg::GetOutputDevicesResult result;
        std::vector<cfg::ConfigOutputDevice>& devices = result.devices;

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
                devices.push_back(MakeOutputDevice(fullName, output, device, currentOutput, currentDevice));
            });
        } catch (...) {
            // Fallback: enumerate via output_entry
            service_enum_t<output_entry> e;
            service_ptr_t<output_entry> ptr;
            while (e.next(ptr)) {
                pfc::string8 name;
                name = ptr->get_name();

                struct device_enum : output_device_enum_callback {
                    std::vector<cfg::ConfigOutputDevice>& devices;
                    GUID outputGuid;
                    const char* outputName;
                    GUID currentOutput, currentDevice;
                    device_enum(std::vector<cfg::ConfigOutputDevice>& d, GUID g, const char* n, GUID co, GUID cd)
                        : devices(d), outputGuid(g), outputName(n), currentOutput(co), currentDevice(cd) {}

                    void on_device(const GUID& deviceGuid, const char* deviceName, unsigned nameLen) override {
                        pfc::string8 fullName;
                        fullName << outputName << ": " << pfc::string8(deviceName, nameLen);
                        devices.push_back(MakeOutputDevice(fullName.get_ptr(), outputGuid, deviceGuid, currentOutput, currentDevice));
                    }
                } callback(devices, ptr->get_guid(), name.get_ptr(), currentOutput, currentDevice);

                ptr->enum_devices(callback);
            }
        }

        result.count = static_cast<std::int64_t>(devices.size());
        return result;
    }

    // Get current output configuration
    api::Result<cfg::GetOutputConfigResult> GetOutputConfig(const cfg::GetOutputConfigParams& /*params*/) {
        try {
            auto mgr = output_manager::get();
            outputCoreConfig_t config = mgr->getCoreConfig();

            cfg::GetOutputConfigResult result;
            result.outputId = GuidToString(config.m_output);
            result.deviceId = GuidToString(config.m_device);
            result.bufferLength = config.m_buffer_length;
            result.bitDepth = static_cast<std::int64_t>(config.m_bitDepth);
            result.useDither = (config.m_flags & outputCoreConfig_t::flagUseDither) != 0;
            result.useFades = (config.m_flags & outputCoreConfig_t::flagUseFades) != 0;

            // Try to get device name
            auto entry = output_entry::g_find(config.m_output);
            if (entry.is_valid()) {
                result.outputName = std::string(entry->get_name());
                pfc::string8 deviceName;
                if (entry->get_device_name(config.m_device, deviceName)) {
                    result.deviceName = std::string(deviceName.get_ptr());
                }
            }
            return result;
        } catch (const std::exception& e) {
            return api::Fail(std::string("Failed to get output config: ") + e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    // Set output device. The generated parser has already refused a missing or empty id.
    api::Result<void> SetOutputDevice(const cfg::SetOutputDeviceParams& params) {
        GUID outputId = {}, deviceId = {};
        if (!StringToGuid(params.outputId, outputId) || !StringToGuid(params.deviceId, deviceId)) {
            return api::Fail("Invalid GUID format", ApiErrorCode::INVALID_PARAMS);
        }

        try {
            auto mgr = output_manager_v2::get();
            mgr->setCoreConfigDevice(outputId, deviceId);
        } catch (const std::exception& e) {
            return api::Fail(std::string("Failed to set output device: ") + e.what(), ApiErrorCode::OPERATION_FAILED);
        }

        return api::Ok();
    }

    // Set output buffer length. Both keys are range-checked by the generated parser (0.05..2 s and
    // 50..2000 ms), so the seconds value here is always in range; milliseconds wins over
    // bufferLength when both are given.
    api::Result<void> SetOutputBuffer(const cfg::SetOutputBufferParams& params) {
        double bufferLength = 0.0;
        if (params.milliseconds) {
            bufferLength = *params.milliseconds / 1000.0;  // Convert ms to seconds
        } else if (params.bufferLength) {
            bufferLength = *params.bufferLength;
        } else {
            return api::Fail("bufferLength or milliseconds is required", ApiErrorCode::INVALID_PARAMS);
        }

        try {
            auto mgr = output_manager::get();
            outputCoreConfig_t config = mgr->getCoreConfig();
            config.m_buffer_length = bufferLength;

            auto mgr2 = output_manager_v2::get();
            mgr2->setCoreConfig(config);
        } catch (const std::exception& e) {
            return api::Fail(std::string("Failed to set buffer length: ") + e.what(), ApiErrorCode::OPERATION_FAILED);
        }

        return api::Ok();
    }

    //==========================================================================
    // ADVANCED CONFIG APIs
    //==========================================================================

    // Helper to build advconfig tree. Entries down to eleven levels below parentGuid are listed;
    // a branch on the eleventh level keeps an empty children list.
    void BuildAdvConfigTree(const GUID& parentGuid, std::vector<cfg::AdvancedConfigItem>& items, int depth = 0) {
        if (depth > 10) return; // Prevent infinite recursion

        service_enum_t<advconfig_entry> e;
        service_ptr_t<advconfig_entry> ptr;

        while (e.next(ptr)) {
            if (ptr->get_parent() == parentGuid) {
                cfg::AdvancedConfigItem item;

                pfc::string8 name;
                ptr->get_name(name);
                item.name = name.get_ptr();
                item.guid = GuidToString(ptr->get_guid());
                item.sortPriority = ptr->get_sort_priority();

                // Check if it's a branch
                service_ptr_t<advconfig_branch> branch;
                if (ptr->service_query_t(branch)) {
                    item.type = "branch";
                    item.children.emplace();
                    BuildAdvConfigTree(ptr->get_guid(), *item.children, depth + 1);
                }
                // Check if it's a checkbox
                else {
                    service_ptr_t<advconfig_entry_checkbox> checkbox;
                    if (ptr->service_query_t(checkbox)) {
                        item.type = checkbox->is_radio() ? "radio" : "checkbox";
                        item.value = json(checkbox->get_state());

                        // Try to get default value
                        service_ptr_t<advconfig_entry_checkbox_v2> cb2;
                        if (ptr->service_query_t(cb2)) {
                            item.defaultValue = json(cb2->get_default_state());
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

                            item.type = isInteger ? "integer" : "string";
                            item.value = json(value.get_ptr());
                            item.isSigned = (flags & advconfig_entry_string::flag_is_signed) != 0;
                            item.isFilePath = (flags & advconfig_entry_string::flag_is_file_path) != 0;
                            item.isFolderPath = (flags & advconfig_entry_string::flag_is_folder_path) != 0;

                            // Try to get default value
                            service_ptr_t<advconfig_entry_string_v2> str2;
                            if (ptr->service_query_t(str2)) {
                                pfc::string8 defaultVal;
                                str2->get_default_state(defaultVal);
                                item.defaultValue = json(defaultVal.get_ptr());
                            }
                        }
                        // Any other kind: the same name getAdvancedConfigValue reports for it
                        else {
                            item.type = "unknown";
                        }
                    }
                }

                items.push_back(std::move(item));
            }
        }
    }

    // Get all advanced config entries. An empty parentGuid means the root, as it did before the
    // declaration; a malformed one is refused instead of silently listing the root.
    api::Result<cfg::GetAdvancedConfigResult> GetAdvancedConfig(const cfg::GetAdvancedConfigParams& params) {
        GUID parentGuid = advconfig_entry::guid_root;
        if (params.parentGuid && !params.parentGuid->empty() && !StringToGuid(*params.parentGuid, parentGuid)) {
            return api::Fail("Invalid GUID format", ApiErrorCode::INVALID_PARAMS);
        }

        cfg::GetAdvancedConfigResult result;
        BuildAdvConfigTree(parentGuid, result.entries);
        result.count = static_cast<std::int64_t>(result.entries.size());
        return result;
    }

    // Get specific advanced config value
    api::Result<cfg::GetAdvancedConfigValueResult> GetAdvancedConfigValue(const cfg::GetAdvancedConfigValueParams& params) {
        GUID guid = {};
        if (!StringToGuid(params.guid, guid)) {
            return api::Fail("Invalid GUID format", ApiErrorCode::INVALID_PARAMS);
        }

        service_ptr_t<advconfig_entry> entry;
        if (!advconfig_entry::g_find(entry, guid)) {
            return api::Fail("Config entry not found", ApiErrorCode::NOT_FOUND);
        }

        cfg::GetAdvancedConfigValueResult result;
        pfc::string8 name;
        entry->get_name(name);
        result.name = name.get_ptr();
        result.guid = params.guid;

        // Branch (group/folder) - check first as it doesn't have a value
        service_ptr_t<advconfig_branch> branch;
        if (entry->service_query_t(branch)) {
            result.type = "branch";
            result.value = nullptr;
            return result;
        }

        // Checkbox
        service_ptr_t<advconfig_entry_checkbox> checkbox;
        if (entry->service_query_t(checkbox)) {
            result.type = checkbox->is_radio() ? "radio" : "checkbox";
            result.value = checkbox->get_state();
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
                result.type = "integer";
                // Parse string as integer
                try {
                    result.value = std::stoll(value.get_ptr());
                } catch (...) {
                    result.value = 0;
                }
            } else {
                result.type = "string";
                result.value = value.get_ptr();
            }
            return result;
        }

        // Unknown type - return type info without value
        result.type = "unknown";
        result.value = nullptr;
        return result;
    }

    // Set advanced config value. The generated parser has already refused a missing guid or value.
    api::Result<void> SetAdvancedConfigValue(const cfg::SetAdvancedConfigValueParams& params) {
        GUID guid = {};
        if (!StringToGuid(params.guid, guid)) {
            return api::Fail("Invalid GUID format", ApiErrorCode::INVALID_PARAMS);
        }

        service_ptr_t<advconfig_entry> entry;
        if (!advconfig_entry::g_find(entry, guid)) {
            return api::Fail("Config entry not found", ApiErrorCode::NOT_FOUND);
        }

        const json& jsonVal = params.value;

        // Checkbox
        service_ptr_t<advconfig_entry_checkbox> checkbox;
        if (entry->service_query_t(checkbox)) {
            if (!jsonVal.is_boolean()) {
                return api::Fail("Boolean value required for checkbox", ApiErrorCode::INVALID_PARAMS);
            }
            checkbox->set_state(jsonVal.get<bool>());
            return api::Ok();
        }

        // String (integers are also advconfig_entry_string with flag_is_integer)
        service_ptr_t<advconfig_entry_string> stringEntry;
        if (entry->service_query_t(stringEntry)) {
            // flag_is_integer lives in get_flags(), as the tree and getAdvancedConfigValue read it;
            // advconfig_entry_string_v2::get_preferences_flags() is the restart hint
            // (preferences_state), not these flags.
            const bool isIntegerConfig = (stringEntry->get_flags() & advconfig_entry_string::flag_is_integer) != 0;

            std::string value;
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
                return api::Fail(isIntegerConfig ? "Integer or string value required" : "String value required",
                                 ApiErrorCode::INVALID_PARAMS);
            }

            stringEntry->set_state(value.c_str(), value.length());
            return api::Ok();
        }

        // Branches and entries of any other kind hold no writable value.
        return api::Fail("Unknown config entry type", ApiErrorCode::NOT_SUPPORTED);
    }

    // Reset advanced config to default
    api::Result<void> ResetAdvancedConfig(const cfg::ResetAdvancedConfigParams& params) {
        GUID guid = {};
        if (!StringToGuid(params.guid, guid)) {
            return api::Fail("Invalid GUID format", ApiErrorCode::INVALID_PARAMS);
        }

        service_ptr_t<advconfig_entry> entry;
        if (!advconfig_entry::g_find(entry, guid)) {
            return api::Fail("Config entry not found", ApiErrorCode::NOT_FOUND);
        }

        entry->reset();
        return api::Ok();
    }

    //==========================================================================
    // PREFERENCES PAGE APIs
    //==========================================================================

    // Get list of all preferences pages
    api::Result<cfg::GetPreferencesPagesResult> GetPreferencesPages(const cfg::GetPreferencesPagesParams& /*params*/) {
        cfg::GetPreferencesPagesResult result;

        // Enumerate preferences pages
        service_enum_t<preferences_page> e;
        service_ptr_t<preferences_page> ptr;

        while (e.next(ptr)) {
            cfg::ConfigPreferencesPage page;
            page.name = ptr->get_name();
            page.guid = GuidToString(ptr->get_guid());
            page.parentGuid = GuidToString(ptr->get_parent_guid());

            // Get sort priority if available
            service_ptr_t<preferences_page_v2> v2;
            page.sortPriority = ptr->service_query_t(v2) ? v2->get_sort_priority() : 0.0;

            result.pages.push_back(std::move(page));
        }

        // Also enumerate branches
        service_enum_t<preferences_branch> eb;
        service_ptr_t<preferences_branch> branchPtr;

        while (eb.next(branchPtr)) {
            cfg::ConfigPreferencesPage branch;
            branch.name = branchPtr->get_name();
            branch.guid = GuidToString(branchPtr->get_guid());
            branch.parentGuid = GuidToString(branchPtr->get_parent_guid());
            branch.isBranch = true;

            service_ptr_t<preferences_branch_v2> v2;
            branch.sortPriority = branchPtr->service_query_t(v2) ? v2->get_sort_priority() : 0.0;

            result.pages.push_back(std::move(branch));
        }

        result.count = static_cast<std::int64_t>(result.pages.size());
        return result;
    }

    // Get standard preference page GUIDs
    api::Result<cfg::GetPreferencesStandardGuidsResult> GetPreferencesStandardGuids(const cfg::GetPreferencesStandardGuidsParams& /*params*/) {
        cfg::GetPreferencesStandardGuidsResult guids;
        guids.root = GuidToString(preferences_page::guid_root);
        guids.hidden = GuidToString(preferences_page::guid_hidden);
        guids.tools = GuidToString(preferences_page::guid_tools);
        guids.core = GuidToString(preferences_page::guid_core);
        guids.display = GuidToString(preferences_page::guid_display);
        guids.playback = GuidToString(preferences_page::guid_playback);
        guids.visualisations = GuidToString(preferences_page::guid_visualisations);
        guids.input = GuidToString(preferences_page::guid_input);
        guids.tagWriting = GuidToString(preferences_page::guid_tag_writing);
        guids.mediaLibrary = GuidToString(preferences_page::guid_media_library);
        guids.tagging = GuidToString(preferences_page::guid_tagging);
        guids.output = GuidToString(preferences_page::guid_output);
        guids.advanced = GuidToString(preferences_page::guid_advanced);
        guids.components = GuidToString(preferences_page::guid_components);
        guids.dsp = GuidToString(preferences_page::guid_dsp);
        guids.shell = GuidToString(preferences_page::guid_shell);
        guids.keyboardShortcuts = GuidToString(preferences_page::guid_keyboard_shortcuts);
        return guids;
    }

    //==========================================================================
    // MEDIA LIBRARY APIs
    //==========================================================================

    // Get media library status
    api::Result<cfg::GetLibraryStatusResult> GetLibraryStatus(const cfg::GetLibraryStatusParams& /*params*/) {
        cfg::GetLibraryStatusResult result;

        auto lib = library_manager::get();
        result.enabled = lib->is_library_enabled();

        // Get item count
        struct counter : library_manager::enum_callback {
            size_t count =0;
            bool on_item(const metadb_handle_ptr&) override { count++; return true; }
        } c;
        lib->enum_items(c);
        result.itemCount = static_cast<std::int64_t>(c.count);

        // Check if initialized (v4+)
        try {
            auto lib4 = library_manager_v4::get();
            result.initialized = lib4->is_initialized();
        } catch (...) {
            result.initialized = true; // Assume initialized for older versions
        }

        return result;
    }

    // Get library file patterns. A pattern that is not configured stays absent, so with neither
    // configured the response is the bare success envelope.
    api::Result<cfg::GetLibraryFilePatternsResult> GetLibraryFilePatterns(const cfg::GetLibraryFilePatternsParams& /*params*/) {
        try {
            auto lib = library_manager_v3::get();
            cfg::GetLibraryFilePatternsResult result;

            pfc::string8 tracksDir, tracksFormat;
            if (lib->get_new_file_pattern_tracks(tracksDir, tracksFormat)) {
                cfg::ConfigLibraryFilePattern tracks;
                tracks.directory = tracksDir.get_ptr();
                tracks.format = tracksFormat.get_ptr();
                result.tracks = std::move(tracks);
            }

            pfc::string8 imagesDir, imagesFormat;
            if (lib->get_new_file_pattern_images(imagesDir, imagesFormat)) {
                cfg::ConfigLibraryFilePattern images;
                images.directory = imagesDir.get_ptr();
                images.format = imagesFormat.get_ptr();
                result.images = std::move(images);
            }

            return result;
        } catch (const std::exception& e) {
            return api::Fail(std::string("Failed to get library patterns: ") + e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    // Show library preferences
    api::Result<void> ShowLibraryPreferences(const cfg::ShowLibraryPreferencesParams& /*params*/) {
        library_manager::get()->show_preferences();
        return api::Ok();
    }

    //==========================================================================
    // COMPONENT INFO APIs
    //==========================================================================

    // Get list of installed components
    api::Result<cfg::GetComponentsResult> GetComponents(const cfg::GetComponentsParams& /*params*/) {
        cfg::GetComponentsResult result;

        service_enum_t<componentversion> e;
        service_ptr_t<componentversion> ptr;

        while (e.next(ptr)) {
            cfg::ConfigComponentInfo comp;

            pfc::string8 name, version;
            ptr->get_component_name(name);
            ptr->get_component_version(version);

            comp.name = name.get_ptr();
            comp.version = version.get_ptr();

            // Try to get file name
            pfc::string8 fileName;
            ptr->get_file_name(fileName);
            if (!fileName.is_empty()) {
                comp.fileName = std::string(fileName.get_ptr());
                comp.filename = comp.fileName;
            }

            result.components.push_back(std::move(comp));
        }

        result.count = static_cast<std::int64_t>(result.components.size());
        return result;
    }

    // Get foobar2000 version info
    api::Result<cfg::GetVersionInfoResult> GetVersionInfo(const cfg::GetVersionInfoParams& /*params*/) {
        cfg::GetVersionInfoResult result;
        result.version = core_version_info::g_get_version_string();
        result.foobar2000 = result.version;
        result.versionFull = core_version_info_v2::get()->get_name();
        result.is64bit = (sizeof(void*) == 8);
        result.isPortable = core_api::is_portable_mode_enabled();

        // Plugin info
        result.plugin.name = "foo_ui_webview2";
        result.plugin.version = PLUGIN_VERSION_STR;

        // Profile path
        pfc::string8 profilePath;
        filesystem::g_get_display_path(core_api::get_profile_path(), profilePath);
        result.profilePath = profilePath.get_ptr();

        return result;
    }

    //==========================================================================
    // DSP CONFIG APIs
    //==========================================================================

    // Get list of available DSP presets
    api::Result<cfg::GetDspPresetsResult> GetDspPresets(const cfg::GetDspPresetsParams& /*params*/) {
        cfg::GetDspPresetsResult result;

        try {
            auto dsp = dsp_config_manager_v2::get();
            t_size count = dsp->get_preset_count();

            for (t_size i = 0; i < count; i++) {
                pfc::string8 name;
                dsp->get_preset_name(i, name);

                cfg::ConfigDspPreset preset;
                preset.index = static_cast<std::int64_t>(i);
                preset.name = name.get_ptr();
                result.presets.push_back(std::move(preset));
            }
        } catch (...) {
            // DSP manager v2 not available
        }

        result.count = static_cast<std::int64_t>(result.presets.size());
        return result;
    }

    // Get active DSP preset. A selection outside the preset list reads as nothing selected, the
    // same check dsp.getChain makes.
    api::Result<cfg::GetActiveDspPresetResult> GetActiveDspPreset(const cfg::GetActiveDspPresetParams& /*params*/) {
        cfg::GetActiveDspPresetResult result;  // index / name null, isActive false

        try {
            auto dsp = dsp_config_manager_v2::get();
            t_size active = dsp->get_selected_preset();

            if (active != pfc::infinite_size && active < dsp->get_preset_count()) {
                pfc::string8 name;
                dsp->get_preset_name(active, name);
                result.index = static_cast<std::int64_t>(active);
                result.name = std::string(name.get_ptr());
                result.isActive = true;
            }
        } catch (...) {
            result = cfg::GetActiveDspPresetResult{};
        }

        return result;
    }

    // Set active DSP preset. The generated parser has already refused a missing, negative or
    // non-integer index; one past the end of the list is refused here.
    api::Result<void> SetActiveDspPreset(const cfg::SetActiveDspPresetParams& params) {
        try {
            auto dsp = dsp_config_manager_v2::get();
            // Compared as int64 first: on Win32 a cast to t_size would wrap large values into range.
            if (params.index >= static_cast<std::int64_t>(dsp->get_preset_count())) {
                return api::Fail("Invalid preset index", ApiErrorCode::INVALID_INDEX);
            }
            dsp->select_preset(static_cast<t_size>(params.index));
        } catch (const std::exception& e) {
            return api::Fail(std::string("Failed to set DSP preset: ") + e.what(), ApiErrorCode::OPERATION_FAILED);
        }

        return api::Ok();
    }

    //==========================================================================
    // PLAYBACK FOLLOW/CURSOR CONFIG APIs
    //==========================================================================

    // The getters report the setting under `enabled` and its older name `value`.
    api::Result<cfg::GetCursorFollowPlaybackResult> GetCursorFollowPlayback(const cfg::GetCursorFollowPlaybackParams& /*params*/) {
        bool enabled = config_object::g_get_data_bool_simple(standard_config_objects::bool_cursor_follows_playback, false);
        cfg::GetCursorFollowPlaybackResult result;
        result.enabled = enabled;
        result.value = enabled;
        return result;
    }

    api::Result<cfg::SetCursorFollowPlaybackResult> SetCursorFollowPlayback(const cfg::SetCursorFollowPlaybackParams& params) {
        config_object::g_set_data_bool(standard_config_objects::bool_cursor_follows_playback, params.enabled);
        cfg::SetCursorFollowPlaybackResult result;
        result.enabled = params.enabled;
        return result;
    }

    api::Result<cfg::GetPlaybackFollowCursorResult> GetPlaybackFollowCursor(const cfg::GetPlaybackFollowCursorParams& /*params*/) {
        bool enabled = config_object::g_get_data_bool_simple(standard_config_objects::bool_playback_follows_cursor, false);
        cfg::GetPlaybackFollowCursorResult result;
        result.enabled = enabled;
        result.value = enabled;
        return result;
    }

    api::Result<cfg::SetPlaybackFollowCursorResult> SetPlaybackFollowCursor(const cfg::SetPlaybackFollowCursorParams& params) {
        config_object::g_set_data_bool(standard_config_objects::bool_playback_follows_cursor, params.enabled);
        cfg::SetPlaybackFollowCursorResult result;
        result.enabled = params.enabled;
        return result;
    }

    //==========================================================================
    // REPLAYGAIN MODE APIs
    //==========================================================================

    // The generated parser only lets the five declared names through.
    t_uint32 SourceModeFromName(const std::string& name) {
        if (name == "track") return t_replaygain_config::source_mode_track;
        if (name == "album") return t_replaygain_config::source_mode_album;
        if (name == "auto" || name == "byPlaybackOrder") return t_replaygain_config::source_mode_byPlaybackOrder;
        return t_replaygain_config::source_mode_none;
    }

    api::Result<cfg::GetReplaygainModeResult> GetReplaygainMode(const cfg::GetReplaygainModeParams& /*params*/) {
        auto mgr = replaygain_manager::get();
        t_replaygain_config settings = mgr->get_core_settings();
        cfg::GetReplaygainModeResult result;
        result.mode = static_cast<std::int64_t>(settings.m_source_mode);
        result.value = result.mode;
        return result;
    }

    // mode is range-checked (0..3) by the generated parser and wins over sourceMode.
    api::Result<cfg::SetReplaygainModeResult> SetReplaygainMode(const cfg::SetReplaygainModeParams& params) {
        t_uint32 mode = t_replaygain_config::source_mode_none;
        if (params.mode) {
            mode = static_cast<t_uint32>(*params.mode);
        } else if (params.sourceMode) {
            mode = SourceModeFromName(*params.sourceMode);
        } else {
            return api::Fail("mode or sourceMode is required", ApiErrorCode::INVALID_PARAMS);
        }

        auto mgr = replaygain_manager::get();
        t_replaygain_config settings = mgr->get_core_settings();
        settings.m_source_mode = mode;
        mgr->set_core_settings(settings);

        cfg::SetReplaygainModeResult result;
        result.mode = static_cast<std::int64_t>(mode);
        result.value = result.mode;
        return result;
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
// 优先从 configStore (SQLite) 读取；若不存在则尝试从旧 cfg_string 迁移。
// configStore 读不出、或其中的文本解析不成 JSON 对象时返回 nullptr：缓存保持未加载、什么也不写，
// 下次调用重试。以前的做法是把缓存重置成空对象，下一次写入就会用它覆盖存储里的原数据。
static json* GetConfigCache() {
    if (g_configCacheValid) return &g_configCache;

    try {
        // 优先读取新存储 (configStore / SQLite)
        auto store = fb2k::configStore::get();
        auto valRef = store->getConfigString(kConfigStoreKey, "");
        const char* configStr = valRef.is_valid() ? valRef->c_str() : nullptr;

        json loaded = json::object();
        if (configStr && configStr[0]) {
            loaded = json::parse(configStr);
            if (!loaded.is_object()) {
                console::printf("[ConfigApi] Stored config is not a JSON object; left untouched");
                return nullptr;
            }
        } else {
            // 新存储为空时，检查旧 cfg_string 是否有数据可迁移
            const char* legacyStr = cfg_portable_config.get();
            if (legacyStr && legacyStr[0] && strcmp(legacyStr, "{}") != 0) {
                // 旧数据解析不成对象时按空处理，与迁移前相同：cfg_string 从不被写入，原文仍在。
                json legacy = json::parse(legacyStr, nullptr, false);
                if (legacy.is_object()) {
                    // 迁移到新存储；写失败时整次加载失败（落到下面的 catch），下次调用重试
                    store->setConfigString(kConfigStoreKey, legacy.dump().c_str());
                    console::printf("[ConfigApi] Migrated config from cfg_string to configStore (%zu keys)",
                                    legacy.size());
                    loaded = std::move(legacy);
                }
            }
        }

        g_configCache = std::move(loaded);
        g_configCacheValid = true;
        return &g_configCache;
    } catch (const std::exception& e) {
        console::printf("[ConfigApi] Failed to load config cache: %s", e.what());
        return nullptr;
    }
}

// Helper: Save config cache to persistent storage (configStore — SQLite 立即 commit)
// 写失败时返回 false 并把内存缓存标为未加载：刚才那次改动作废，下次访问从存储重读，
// 调用方据此回 OPERATION_FAILED，而不是报成功却没存下。
static bool SaveConfigCache(std::string& error) {
    try {
        auto store = fb2k::configStore::get();
        store->setConfigString(kConfigStoreKey, g_configCache.dump().c_str());
        g_configCacheValid = true;
        return true;
    } catch (const std::exception& e) {
        console::printf("[ConfigApi] Failed to save config cache: %s", e.what());
        error = e.what();
        g_configCacheValid = false;
        return false;
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

api::Failure ConfigStoreUnreadable() {
    return api::Fail("config store is unreadable", ApiErrorCode::OPERATION_FAILED);
}

api::Failure ConfigStoreNotSaved(const std::string& error) {
    return api::Fail("Failed to save config store: " + error, ApiErrorCode::OPERATION_FAILED);
}

// The store is a JSON object; the declared result holds it as a map.
std::map<std::string, json> ConfigEntries(const json& cache) {
    std::map<std::string, json> entries;
    for (auto it = cache.begin(); it != cache.end(); ++it) {
        entries.emplace(it.key(), it.value());
    }
    return entries;
}

// The generated parser has already refused an empty key and a missing or null value.
api::Result<cfg::SetResult> ConfigSet(const cfg::SetParams& params) {
    json* cache = GetConfigCache();
    if (!cache) return ConfigStoreUnreadable();

    (*cache)[params.key] = params.value;

    // Save to persistent storage
    std::string error;
    if (!SaveConfigCache(error)) return ConfigStoreNotSaved(error);

    cfg::SetResult result;
    result.key = params.key;
    return result;
}


api::Result<cfg::GetResult> ConfigGet(const cfg::GetParams& params) {
    const json* cache = GetConfigCache();
    if (!cache) return ConfigStoreUnreadable();

    cfg::GetResult result;
    result.key = params.key;

    auto it = cache->find(params.key);
    if (it != cache->end()) {
        result.value = *it;
        result.found = true;
        return result;
    }

    // Absent key: the default when one was given, otherwise null
    result.value = params.default_.value_or(json(nullptr));
    result.found = false;
    return result;
}


api::Result<cfg::RemoveResult> ConfigRemove(const cfg::RemoveParams& params) {
    json* cache = GetConfigCache();
    if (!cache) return ConfigStoreUnreadable();

    auto it = cache->find(params.key);
    bool existed = (it != cache->end());

    if (existed) {
        cache->erase(it);
        // Save to persistent storage
        std::string error;
        if (!SaveConfigCache(error)) return ConfigStoreNotSaved(error);
    }

    cfg::RemoveResult result;
    result.key = params.key;
    result.existed = existed;
    return result;
}


api::Result<cfg::GetAllResult> ConfigGetAll(const cfg::GetAllParams& /*params*/) {
    const json* cache = GetConfigCache();
    if (!cache) return ConfigStoreUnreadable();

    cfg::GetAllResult result;
    result.items = ConfigEntries(*cache);
    result.configs = result.items;  // the older name of the same map
    result.count = static_cast<std::int64_t>(result.items.size());
    return result;
}


api::Result<cfg::ExportResult> ConfigExport(const cfg::ExportParams& /*params*/) {
    const json* cache = GetConfigCache();
    if (!cache) return ConfigStoreUnreadable();

    // Return both object and a serialized JSON string for compatibility
    cfg::ExportResult result;
    result.data = ConfigEntries(*cache);
    result.json_ = cache->dump();
    result.count = static_cast<std::int64_t>(result.data.size());
    return result;
}

} // namespace

void RegisterConfigApi() {
    // 便携配置存储
    api::RegisterApi("config.set", ConfigSet);
    api::RegisterApi("config.get", ConfigGet);
    api::RegisterApi("config.remove", ConfigRemove);
    api::RegisterApi("config.getAll", ConfigGetAll);
    api::RegisterApi("config.export", ConfigExport);

    // 输出设备
    api::RegisterApi("config.getOutputDevices", GetOutputDevices);
    api::RegisterApi("config.getOutputConfig", GetOutputConfig);
    api::RegisterApi("config.setOutputDevice", SetOutputDevice);
    api::RegisterApi("config.setOutputBuffer", SetOutputBuffer);

    // 高级配置
    api::RegisterApi("config.getAdvancedConfig", GetAdvancedConfig);
    api::RegisterApi("config.getAdvancedConfigValue", GetAdvancedConfigValue);
    api::RegisterApi("config.setAdvancedConfigValue", SetAdvancedConfigValue);
    api::RegisterApi("config.resetAdvancedConfig", ResetAdvancedConfig);

    // 首选项与组件
    api::RegisterApi("config.getPreferencesPages", GetPreferencesPages);
    api::RegisterApi("config.getPreferencesStandardGuids", GetPreferencesStandardGuids);
    api::RegisterApi("config.getComponents", GetComponents);
    api::RegisterApi("config.getVersionInfo", GetVersionInfo);

    // 媒体库
    api::RegisterApi("config.getLibraryStatus", GetLibraryStatus);
    api::RegisterApi("config.getLibraryFilePatterns", GetLibraryFilePatterns);
    api::RegisterApi("config.showLibraryPreferences", ShowLibraryPreferences);

    // DSP
    api::RegisterApi("config.getDspPresets", GetDspPresets);
    api::RegisterApi("config.getActiveDspPreset", GetActiveDspPreset);
    api::RegisterApi("config.setActiveDspPreset", SetActiveDspPreset);

    // 播放行为
    api::RegisterApi("config.getCursorFollowPlayback", GetCursorFollowPlayback);
    api::RegisterApi("config.setCursorFollowPlayback", SetCursorFollowPlayback);
    api::RegisterApi("config.getPlaybackFollowCursor", GetPlaybackFollowCursor);
    api::RegisterApi("config.setPlaybackFollowCursor", SetPlaybackFollowCursor);

    // ReplayGain
    api::RegisterApi("config.getReplaygainMode", GetReplaygainMode);
    api::RegisterApi("config.setReplaygainMode", SetReplaygainMode);
}
