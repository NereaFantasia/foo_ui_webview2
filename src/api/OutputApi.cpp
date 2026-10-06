// OutputApi.cpp - Output Format Configuration API
// Provides information about foobar2000's output settings and devices

#include "pch.h"
#include "api/OutputApi.h"
#include "api/BridgeCore.h"
#include "api/TypedApi.h"
#include "api/generated/OutputSchema.h"
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
        std::vector<api::output::OutputDevice> devices;
        GUID currentEntryGuid;
        std::string currentEntryName;

        void on_device(const GUID& p_guid, const char* p_name, unsigned p_name_length) override {
            // p_name_length may be a sentinel meaning "unknown length, p_name is
            // NUL-terminated" (foobar2000 passes unsigned(-1) for some backends).
            // Constructing std::string(ptr, count) would memcpy that raw count, so
            // clamp to the real length first — this mirrors what the SDK's own
            // consumer does via pfc::string_base::set_string().
            pfc::string8 name;
            name.set_string(p_name, p_name_length);

            api::output::OutputDevice device;
            device.guid = GuidToString(p_guid);
            device.name = name.get_ptr();
            device.entry = currentEntryName;
            device.entryGuid = GuidToString(currentEntryGuid);
            devices.push_back(std::move(device));
        }
    };

    //==========================================================================
    // output.getDevices - Get available output devices
    //==========================================================================
    api::Result<api::output::GetDevicesResult> OutputGetDevices(const api::output::GetDevicesParams& /*params*/) {
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

            api::output::GetDevicesResult out;
            out.count = static_cast<std::int64_t>(callback.devices.size());
            out.devices = std::move(callback.devices);
            return out;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // output.getEntries - Get available output modules (entries)
    //==========================================================================
    api::Result<api::output::GetEntriesResult> OutputGetEntries(const api::output::GetEntriesParams& /*params*/) {
        try {
            api::output::GetEntriesResult out;

            service_enum_t<output_entry> e;
            output_entry::ptr entry;

            while (e.next(entry)) {
                pfc::string8 name;
                name = entry->get_name();

                t_uint32 flags = entry->get_config_flags_compat();

                api::output::OutputEntry row;
                row.guid = GuidToString(entry->get_guid());
                row.name = name.get_ptr();
                row.needsBitdepthConfig = (flags & output_entry::flag_needs_bitdepth_config) != 0;
                row.needsDitherConfig = (flags & output_entry::flag_needs_dither_config) != 0;
                row.supportsMultipleStreams = (flags & output_entry::flag_supports_multiple_streams) != 0;
                row.isHighLatency = (flags & output_entry::flag_high_latency) != 0;
                row.isLowLatency = (flags & output_entry::flag_low_latency) != 0;
                out.entries.push_back(std::move(row));
            }

            out.count = static_cast<std::int64_t>(out.entries.size());
            return out;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // output.getSettings - Get current output settings (read-only)
    // Note: Actual output settings are managed through foobar2000 preferences.
    // `availableOutputs` is a plain name list, so entries that share a display
    // name (several backends are called "Default") are indistinguishable and a
    // backend reporting an empty name shows up as "". Prefer
    // `output.getEntries`, which pairs every name with its GUID.
    //==========================================================================
    api::Result<api::output::GetSettingsResult> OutputGetSettings(const api::output::GetSettingsParams& /*params*/) {
        try {
            // These settings are stored in foobar2000's config
            // We can only report what's available to configure
            
            api::output::GetSettingsResult settings;
            settings.note = "Output settings are managed through foobar2000 Preferences > Playback > Output. "
                               "availableOutputs lists display names only and cannot disambiguate backends that "
                               "share a name; use output.getEntries for name + GUID pairs.";
            
            // Enumerate to show available options
            service_enum_t<output_entry> e;
            output_entry::ptr entry;

            while (e.next(entry)) {
                pfc::string8 name;
                name = entry->get_name();
                settings.availableOutputs.emplace_back(name.get_ptr());
            }

            return settings;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

} // anonymous namespace

//==========================================================================
// Register Output API
//==========================================================================
void RegisterOutputApi() {
    // Parameters and results come from src/api/schema/output.ts through the generated types.
    api::RegisterApi("output.getDevices", OutputGetDevices);
    api::RegisterApi("output.getEntries", OutputGetEntries);
    api::RegisterApi("output.getSettings", OutputGetSettings);

    LOG("Output API registered (3 APIs)");
}
