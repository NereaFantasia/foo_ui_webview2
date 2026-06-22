// OutputApi.cpp - Output Format Configuration API
// Provides information about foobar2000's output settings and devices

#include "pch.h"
#include "api/OutputApi.h"
#include "api/BridgeCore.h"
#include <foobar2000/SDK/output.h>

namespace {
    using json = nlohmann::json;

    // Helper: Convert GUID to string
    std::string GuidToString(const GUID& guid) {
        char buffer[64];
        sprintf_s(buffer, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
        return buffer;
    }

    // Device enumeration callback
    class DeviceEnumCallback : public output_device_enum_callback {
    public:
        json devices = json::array();
        GUID currentEntryGuid;
        std::string currentEntryName;

        void on_device(const GUID& p_guid, const char* p_name, unsigned p_name_length) override {
            devices.push_back({
                {"guid", GuidToString(p_guid)},
                {"name", std::string(p_name, p_name_length)},
                {"entry", currentEntryName},
                {"entryGuid", GuidToString(currentEntryGuid)}
            });
        }
    };

    //==========================================================================
    // output.getDevices - Get available output devices
    //==========================================================================
    json OutputGetDevices(const json& /*params*/) {
        try {
            DeviceEnumCallback callback;
            
            service_enum_t<output_entry> e;
            output_entry::ptr entry;
            
            while (e.next(entry)) {
                pfc::string8 entryName;
                entryName = entry->get_name();
                
                callback.currentEntryGuid = entry->get_guid();
                callback.currentEntryName = entryName.get_ptr();
                
                entry->enum_devices(callback);
            }

            return {
                {"devices", callback.devices},
                {"count", callback.devices.size()}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // output.getEntries - Get available output modules (entries)
    //==========================================================================
    json OutputGetEntries(const json& /*params*/) {
        try {
            json entries = json::array();
            
            service_enum_t<output_entry> e;
            output_entry::ptr entry;
            
            while (e.next(entry)) {
                pfc::string8 name;
                name = entry->get_name();
                
                t_uint32 flags = entry->get_config_flags_compat();
                
                entries.push_back({
                    {"guid", GuidToString(entry->get_guid())},
                    {"name", name.get_ptr()},
                    {"needsBitdepthConfig", (flags & output_entry::flag_needs_bitdepth_config) != 0},
                    {"needsDitherConfig", (flags & output_entry::flag_needs_dither_config) != 0},
                    {"supportsMultipleStreams", (flags & output_entry::flag_supports_multiple_streams) != 0},
                    {"isHighLatency", (flags & output_entry::flag_high_latency) != 0},
                    {"isLowLatency", (flags & output_entry::flag_low_latency) != 0}
                });
            }

            return {
                {"entries", entries},
                {"count", entries.size()}
            };
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

    //==========================================================================
    // output.getSettings - Get current output settings (read-only)
    // Note: Actual output settings are managed through foobar2000 preferences
    //==========================================================================
    json OutputGetSettings(const json& /*params*/) {
        try {
            // These settings are stored in foobar2000's config
            // We can only report what's available to configure
            
            json settings;
            settings["note"] = "Output settings are managed through foobar2000 Preferences > Playback > Output";
            
            // Enumerate to show available options
            json entries = json::array();
            service_enum_t<output_entry> e;
            output_entry::ptr entry;
            
            while (e.next(entry)) {
                pfc::string8 name;
                name = entry->get_name();
                entries.push_back(name.get_ptr());
            }
            
            settings["availableOutputs"] = entries;
            
            return settings;
        } catch (const std::exception& e) {
            return {{"success", false}, {"error", e.what()}};
        }
    }

} // anonymous namespace

//==========================================================================
// Register Output API
//==========================================================================
void RegisterOutputApi() {
    auto& bridge = BridgeCore::GetInstance();

    // output.getDevices - Get available output devices
    bridge.RegisterApi("output.getDevices", OutputGetDevices);

    // output.getEntries - Get available output modules
    bridge.RegisterApi("output.getEntries", OutputGetEntries);

    // output.getSettings - Get output settings info
    bridge.RegisterApi("output.getSettings", OutputGetSettings);

    LOG("Output API registered (3 APIs)");
}
