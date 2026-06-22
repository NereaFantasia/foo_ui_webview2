// MiscApi.cpp - Miscellaneous app-level APIs
// Provides system path queries and basic UI commands

#include "pch.h"
#include "api/MiscApi.h"
#include "api/BridgeCore.h"
#include <foobar2000/SDK/menu_helpers.h>
#include <foobar2000/SDK/popup_message.h>
#include <foobar2000/SDK/library_manager.h>
#include <filesystem>

namespace {
    using json = nlohmann::json;
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

    std::string GetFoobarPathUtf8() {
        std::wstring comp = GetComponentDirectoryW();
        if (comp.empty()) return "";
        fs::path p(comp);
        if (p.has_parent_path()) {
            return WideToUtf8Safe(p.parent_path().wstring());
        }
        return "";
    }

    std::string GetProfilePathUtf8() {
        pfc::string8 profilePath;
        filesystem::g_get_display_path(core_api::get_profile_path(), profilePath);
        return profilePath.get_ptr();
    }
}


// ==========================================================================
// Misc API handler functions
// ==========================================================================
namespace {


// Path queries
json MiscGetFoobarPath(const json& params) {
    std::string path = GetFoobarPathUtf8();
    return { {"path", path}, {"value", path} };
}


json MiscGetProfilePath(const json& params) {
    std::string path = GetProfilePathUtf8();
    return { {"path", path}, {"value", path} };
}


json MiscGetComponentPath(const json& params) {
    std::string path = GetComponentPathUtf8();
    return { {"path", path}, {"value", path} };
}


// UI helpers
json MiscShowConsole(const json& params) {
    bool ok = standard_commands::run_main(standard_commands::guid_main_show_console);
    return { {"success", ok} };
}


json MiscShowPreferences(const json& params) {
    bool ok = standard_commands::run_main(standard_commands::guid_main_preferences);
    return { {"success", ok} };
}


json MiscShowLibrarySearch(const json& params) {
    std::string query = params.value("query", "");
    library_search_ui::get()->show(query.c_str());
    return { {"success", true}, {"query", query} };
}


json MiscShowPopupMessage(const json& params) {
    std::string msg = params.value("message", params.value("msg", ""));
    std::string title = params.value("title", "Message");
    popup_message::g_show(msg.c_str(), title.c_str());
    return { {"success", true} };
}


json MiscRestart(const json& params) {
    bool ok = standard_commands::run_main(standard_commands::guid_main_restart);
    return { {"success", ok} };
}


json MiscExit(const json& params) {
    bool ok = standard_commands::run_main(standard_commands::guid_main_exit);
    return { {"success", ok} };
}

} // namespace

void RegisterMiscApi() {
    auto& bridge = BridgeCore::GetInstance();

    bridge.RegisterApi("misc.getFoobarPath", MiscGetFoobarPath);
    bridge.RegisterApi("misc.getProfilePath", MiscGetProfilePath);
    bridge.RegisterApi("misc.getComponentPath", MiscGetComponentPath);
    bridge.RegisterApi("misc.showConsole", MiscShowConsole);
    bridge.RegisterApi("misc.showPreferences", MiscShowPreferences);
    bridge.RegisterApi("misc.showLibrarySearch", MiscShowLibrarySearch);
    bridge.RegisterApi("misc.showPopupMessage", MiscShowPopupMessage);
    bridge.RegisterApi("misc.restart", MiscRestart);
    bridge.RegisterApi("misc.exit", MiscExit);
}
