// DspApi.cpp - DSP Chain Management API
// Provides control over foobar2000's DSP processing chain

#include "pch.h"
#include "api/DspApi.h"
#include "api/TypedApi.h"
#include "api/generated/DspSchema.h"

#include <foobar2000/SDK/dsp.h>
#include <foobar2000/SDK/dsp_manager.h>
#include "utils/GuidUtils.h"

namespace {
    namespace dsp = api::dsp;
    using GuidUtils::GuidToString;
    using GuidUtils::StringToGuid;


    //==========================================================================
    // dsp.getChain - Get current DSP chain configuration
    //==========================================================================
    api::Result<dsp::GetChainResult> DspGetChain(const dsp::GetChainParams& /*params*/) {
        try {
            auto dsp_mgr = dsp_config_manager::get();
            dsp_chain_config_impl chain;
            dsp_mgr->get_core_settings(chain);

            dsp::GetChainResult result;
            for (size_t i = 0; i < chain.get_count(); i++) {
                const dsp_preset& preset = chain.get_item(i);
                GUID owner = preset.get_owner();

                pfc::string8 name;
                dsp_entry::g_name_from_guid(name, owner);

                dsp::DspChainEntry entry;
                entry.index = static_cast<std::int64_t>(i);
                entry.guid = GuidToString(owner);
                entry.name = name.get_ptr();
                result.dsps.push_back(std::move(entry));
            }

            // Both keys are always present so callers never have to probe for
            // them: null / -1 means "no preset selected" or "presets not
            // supported by this host".
            result.activePreset = std::nullopt;
            result.activePresetIndex = -1;

            try {
                auto dsp_mgr_v2 = dsp_config_manager_v2::get();
                size_t selected = dsp_mgr_v2->get_selected_preset();
                if (selected != pfc::infinite_size && selected < dsp_mgr_v2->get_preset_count()) {
                    pfc::string8 presetName;
                    dsp_mgr_v2->get_preset_name(selected, presetName);
                    result.activePreset = std::string(presetName.get_ptr());
                    result.activePresetIndex = static_cast<std::int64_t>(selected);
                }
            } catch (...) {
                // Host without dsp_config_manager_v2: keep the null / -1 defaults.
            }

            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // dsp.getPresets - Get available DSP presets
    //==========================================================================
    api::Result<dsp::GetPresetsResult> DspGetPresets(const dsp::GetPresetsParams& /*params*/) {
        try {
            auto dsp_mgr_v2 = dsp_config_manager_v2::get();

            size_t count = dsp_mgr_v2->get_preset_count();
            size_t selected = dsp_mgr_v2->get_selected_preset();

            dsp::GetPresetsResult result;
            for (size_t i = 0; i < count; i++) {
                pfc::string8 name;
                dsp_mgr_v2->get_preset_name(i, name);
                dsp::DspPresetEntry entry;
                entry.index = static_cast<std::int64_t>(i);
                entry.name = name.get_ptr();
                entry.active = (i == selected);
                result.presets.push_back(std::move(entry));
            }

            // `get_selected_preset` reports pfc::infinite_size when nothing is
            // selected. That value is not representable as a JS number, so it is
            // normalized to -1 instead of overflowing into an unusable float.
            result.count = static_cast<std::int64_t>(count);
            result.selectedIndex = (selected == pfc::infinite_size || selected >= count)
                ? -1
                : static_cast<std::int64_t>(selected);
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // dsp.applyPreset - Apply a DSP preset by name or index
    //==========================================================================
    api::Result<dsp::ApplyPresetResult> DspApplyPreset(const dsp::ApplyPresetParams& params) {
        try {
            auto dsp_mgr_v2 = dsp_config_manager_v2::get();

            size_t targetIndex = pfc::infinite_size;

            // Find by index
            if (params.index) {
                targetIndex = static_cast<size_t>(*params.index);
            }
            // Find by name
            else if (params.name) {
                const std::string& targetName = *params.name;
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
                    return api::Fail("Preset not found: " + targetName, ApiErrorCode::NOT_FOUND);
                }
            } else {
                return api::Fail("name or index parameter required", ApiErrorCode::INVALID_PARAMS);
            }

            if (targetIndex >= dsp_mgr_v2->get_preset_count()) {
                return api::Fail("Invalid preset index", ApiErrorCode::INVALID_INDEX);
            }

            // Apply preset
            dsp_mgr_v2->select_preset(targetIndex);

            pfc::string8 appliedName;
            dsp_mgr_v2->get_preset_name(targetIndex, appliedName);

            dsp::ApplyPresetResult result;
            result.appliedPreset = appliedName.get_ptr();
            result.appliedIndex = static_cast<std::int64_t>(targetIndex);
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // dsp.getAvailable - Get list of available DSP processors
    //==========================================================================
    api::Result<dsp::GetAvailableResult> DspGetAvailable(const dsp::GetAvailableParams& /*params*/) {
        try {
            dsp::GetAvailableResult result;

            service_enum_t<dsp_entry> e;
            dsp_entry::ptr ptr;

            while (e.next(ptr)) {
                pfc::string8 name;
                ptr->get_name(name);

                dsp::DspAvailableEntry entry;
                entry.guid = GuidToString(ptr->get_guid());
                entry.name = name.get_ptr();
                entry.hasConfig = ptr->have_config_popup();
                result.dsps.push_back(std::move(entry));
            }

            result.count = static_cast<std::int64_t>(result.dsps.size());
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // dsp.addDsp - Add a DSP to the chain
    //==========================================================================
    api::Result<dsp::AddDspResult> DspAddDsp(const dsp::AddDspParams& params) {
        const std::int64_t position = params.position;

        GUID guid;
        if (!StringToGuid(params.guid, guid)) {
            return api::Fail("Invalid GUID format", ApiErrorCode::INVALID_PARAMS);
        }

        try {
            // Get default preset for this DSP
            dsp_preset_impl preset;
            if (!dsp_entry::g_get_default_preset(preset, guid)) {
                return api::Fail("DSP not found or no default preset", ApiErrorCode::NOT_FOUND);
            }

            // Get current chain
            auto dsp_mgr = dsp_config_manager::get();
            dsp_chain_config_impl chain;
            dsp_mgr->get_core_settings(chain);

            // Insert at position; -1 or past the end appends.
            std::int64_t landed;
            if (position < 0 || position >= static_cast<std::int64_t>(chain.get_count())) {
                landed = static_cast<std::int64_t>(chain.get_count());
                chain.insert_item(preset, chain.get_count());
            } else {
                landed = position;
                chain.insert_item(preset, static_cast<size_t>(position));
            }

            // Apply new chain
            dsp_mgr->set_core_settings(chain);

            pfc::string8 name;
            dsp_entry::g_name_from_guid(name, guid);

            dsp::AddDspResult result;
            result.addedDsp = name.get_ptr();
            result.position = landed;
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // dsp.removeDsp - Remove a DSP from the chain
    //==========================================================================
    api::Result<dsp::RemoveDspResult> DspRemoveDsp(const dsp::RemoveDspParams& params) {
        const size_t index = static_cast<size_t>(params.index);

        try {
            auto dsp_mgr = dsp_config_manager::get();
            dsp_chain_config_impl chain;
            dsp_mgr->get_core_settings(chain);

            if (index >= chain.get_count()) {
                return api::Fail("Index out of range", ApiErrorCode::INVALID_INDEX);
            }

            // Get name before removing
            const dsp_preset& preset = chain.get_item(index);
            pfc::string8 name;
            dsp_entry::g_name_from_guid(name, preset.get_owner());

            // Remove
            chain.remove_item(index);

            // Apply
            dsp_mgr->set_core_settings(chain);

            dsp::RemoveDspResult result;
            result.removedDsp = name.get_ptr();
            result.removedIndex = static_cast<std::int64_t>(index);
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // dsp.moveDsp - Move a DSP within the chain
    //==========================================================================
    api::Result<dsp::MoveDspResult> DspMoveDsp(const dsp::MoveDspParams& params) {
        const size_t from = static_cast<size_t>(params.from);
        const size_t to = static_cast<size_t>(params.to);

        try {
            auto dsp_mgr = dsp_config_manager::get();
            dsp_chain_config_impl chain;
            dsp_mgr->get_core_settings(chain);

            if (from >= chain.get_count() || to >= chain.get_count()) {
                return api::Fail("Index out of range", ApiErrorCode::INVALID_INDEX);
            }

            dsp::MoveDspResult result;
            result.from = params.from;
            result.to = params.to;

            if (from == to) {
                result.message = "No change needed";
                return result;
            }

            // Get the preset to move
            dsp_preset_impl preset;
            preset.copy(chain.get_item(from));
            pfc::string8 name;
            dsp_entry::g_name_from_guid(name, preset.get_owner());

            // Remove from original position. The chain is now one shorter, so
            // inserting at `to` already yields final index `to` — subtracting 1
            // for upward moves would over-correct (a 1-slot upward move would
            // become a no-op, and nothing could reach the tail).
            chain.remove_item(from);
            chain.insert_item(preset, to);

            // Apply
            dsp_mgr->set_core_settings(chain);

            result.movedDsp = std::string(name.get_ptr());
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // dsp.setChain - Set complete DSP chain (advanced)
    //==========================================================================
    api::Result<dsp::SetChainResult> DspSetChain(const dsp::SetChainParams& params) {
        try {
            dsp_chain_config_impl newChain;

            // Every entry must resolve to an installed DSP. Skipping bad entries
            // silently would apply a shorter chain than the caller asked for while
            // still reporting success, so each failure mode rejects the whole call
            // with an index-tagged reason instead. Shape and empty-guid errors are
            // the generated parser's, under the same dsps[i] prefix.
            for (size_t i = 0; i < params.dsps.size(); i++) {
                const std::string& guidStr = params.dsps[i].guid;
                const std::string at = "dsps[" + std::to_string(i) + "]";

                GUID guid;
                if (!StringToGuid(guidStr, guid)) {
                    return api::Fail(at + ": Invalid GUID format: " + guidStr, ApiErrorCode::INVALID_PARAMS);
                }

                // A well-formed GUID for a DSP that is not installed (component
                // removed, typo in a hand-built GUID) lands here.
                dsp_preset_impl preset;
                if (!dsp_entry::g_get_default_preset(preset, guid)) {
                    return api::Fail(at + ": DSP not found or no default preset: " + guidStr, ApiErrorCode::NOT_FOUND);
                }

                newChain.insert_item(preset, newChain.get_count());
            }

            auto dsp_mgr = dsp_config_manager::get();
            dsp_mgr->set_core_settings(newChain);

            dsp::SetChainResult result;
            result.count = static_cast<std::int64_t>(newChain.get_count());
            return result;
        } catch (const std::exception& e) {
            return api::Fail(e.what(), ApiErrorCode::OPERATION_FAILED);
        }
    }

} // anonymous namespace

//==========================================================================
// Register DSP API
//==========================================================================
void RegisterDspApi() {
    api::RegisterApi("dsp.getChain", DspGetChain);
    api::RegisterApi("dsp.getPresets", DspGetPresets);
    api::RegisterApi("dsp.applyPreset", DspApplyPreset);
    api::RegisterApi("dsp.getAvailable", DspGetAvailable);
    api::RegisterApi("dsp.addDsp", DspAddDsp);
    api::RegisterApi("dsp.removeDsp", DspRemoveDsp);
    api::RegisterApi("dsp.moveDsp", DspMoveDsp);
    api::RegisterApi("dsp.setChain", DspSetChain);

    LOG("DSP API registered (8 APIs)");
}
