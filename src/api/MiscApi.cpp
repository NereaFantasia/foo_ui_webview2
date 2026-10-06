// MiscApi.cpp - Miscellaneous app-level APIs
// Provides system path queries and basic UI commands

#include "pch.h"
#include "api/MiscApi.h"
#include "api/TypedApi.h"
#include "api/generated/MiscSchema.h"
#include <foobar2000/SDK/menu_helpers.h>
#include <foobar2000/SDK/popup_message.h>
#include <foobar2000/SDK/library_manager.h>
#include <filesystem>

namespace {
    namespace misc = api::misc;
    namespace fs = std::filesystem;

    std::wstring GetComponentDirectoryW() {
        wchar_t path[MAX_PATH];
        HMODULE hModule = core_api::get_my_instance();
        if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0) {
            fs::path p(path);
            if (p.has_parent_path()) {
                return p.parent_path().wstring();
            }
        }
        return L"";
    }

    std::string WideToUtf8Safe(const std::wstring& w) {
        if (w.empty()) return "";
        return pfc::stringcvt::string_utf8_from_wide(w.c_str()).get_ptr();
    }

    std::string GetComponentPathUtf8() {
        return WideToUtf8Safe(GetComponentDirectoryW());
    }

    // The directory of foobar2000.exe. The component's own directory is no guide to it: a
    // profile install keeps components under the profile, not under the installation.
    std::string GetFoobarPathUtf8() {
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) return "";
        fs::path p(path);
        return p.has_parent_path() ? WideToUtf8Safe(p.parent_path().wstring()) : "";
    }

    std::string GetProfilePathUtf8() {
        pfc::string8 profilePath;
        filesystem::g_get_display_path(core_api::get_profile_path(), profilePath);
        return profilePath.get_ptr();
    }

    // The three path endpoints report the directory under both keys; `value` is the older name.
    // Each method has its own generated result struct, so the helper is a template.
    template <class R>
    R PathResultOf(std::string path) {
        R result;
        result.value = path;
        result.path = std::move(path);
        return result;
    }

    // standard_commands::run_main reports whether the command ran.
    api::Result<void> RunMainCommand(const GUID& command, const char* what) {
        if (standard_commands::run_main(command)) return api::Ok();
        return api::Fail(std::string("foobar2000 did not run the ") + what + " command", ApiErrorCode::OPERATION_FAILED);
    }
}


// ==========================================================================
// Misc API handler functions
// ==========================================================================
namespace {


// Path queries
api::Result<misc::GetFoobarPathResult> MiscGetFoobarPath(const misc::GetFoobarPathParams& /*params*/) {
    return PathResultOf<misc::GetFoobarPathResult>(GetFoobarPathUtf8());
}


api::Result<misc::GetProfilePathResult> MiscGetProfilePath(const misc::GetProfilePathParams& /*params*/) {
    return PathResultOf<misc::GetProfilePathResult>(GetProfilePathUtf8());
}


api::Result<misc::GetComponentPathResult> MiscGetComponentPath(const misc::GetComponentPathParams& /*params*/) {
    return PathResultOf<misc::GetComponentPathResult>(GetComponentPathUtf8());
}


// UI helpers
api::Result<void> MiscShowConsole(const misc::ShowConsoleParams& /*params*/) {
    return RunMainCommand(standard_commands::guid_main_show_console, "show console");
}


api::Result<void> MiscShowPreferences(const misc::ShowPreferencesParams& /*params*/) {
    return RunMainCommand(standard_commands::guid_main_preferences, "preferences");
}


api::Result<misc::ShowLibrarySearchResult> MiscShowLibrarySearch(const misc::ShowLibrarySearchParams& params) {
    library_search_ui::get()->show(params.query.c_str());
    misc::ShowLibrarySearchResult result;
    result.query = params.query;
    return result;
}


api::Result<void> MiscShowPopupMessage(const misc::ShowPopupMessageParams& params) {
    popup_message::g_show(params.message.c_str(), params.title.c_str());
    return api::Ok();
}


api::Result<void> MiscRestart(const misc::RestartParams& /*params*/) {
    return RunMainCommand(standard_commands::guid_main_restart, "restart");
}


api::Result<void> MiscExit(const misc::ExitParams& /*params*/) {
    return RunMainCommand(standard_commands::guid_main_exit, "exit");
}

} // namespace

void RegisterMiscApi() {
    api::RegisterApi("misc.getFoobarPath", MiscGetFoobarPath);
    api::RegisterApi("misc.getProfilePath", MiscGetProfilePath);
    api::RegisterApi("misc.getComponentPath", MiscGetComponentPath);
    api::RegisterApi("misc.showConsole", MiscShowConsole);
    api::RegisterApi("misc.showPreferences", MiscShowPreferences);
    api::RegisterApi("misc.showLibrarySearch", MiscShowLibrarySearch);
    api::RegisterApi("misc.showPopupMessage", MiscShowPopupMessage);
    api::RegisterApi("misc.restart", MiscRestart);
    api::RegisterApi("misc.exit", MiscExit);
}
