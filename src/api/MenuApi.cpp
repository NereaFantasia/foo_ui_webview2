// MenuApi.cpp - Menu command helpers
// Provides execution of main menu and context menu commands by name/path

#include "pch.h"
#include "api/MenuApi.h"
#include "api/BridgeCore.h"
#include "api/PluginRegistry.h"
#include <foobar2000/SDK/menu_helpers.h>
#include <foobar2000/SDK/menu.h>
#include <foobar2000/SDK/contextmenu_manager.h>
#include <foobar2000/SDK/metadb.h>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include "utils/GuidUtils.h"
#include "utils/PathSecurity.h"
#include "window/MenuOverlayHost.h"

namespace {
    using json = nlohmann::json;

    // 延迟弹菜单所需的待执行状态（TIMERPROC 回调无法捕获，必须文件级持有）
    struct PendingContextMenu {
        service_ptr_t<contextmenu_manager> mgr;
        POINT pt{};
        HWND parent = nullptr;
        static constexpr UINT_PTR TIMER_ID = 64206;  // 0xFACE
    };
    PendingContextMenu& GetPendingContextMenu() {
        static PendingContextMenu instance;
        return instance;
    }

    using GuidUtils::StringToGuid;
    using GuidUtils::GuidToString;

    std::string Trim(const std::string& s) {
        size_t start = 0;
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
        size_t end = s.size();
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
        return s.substr(start, end - start);
    }

    std::string NormalizeLabel(const std::string& in) {
        std::string s = in;
        s.erase(std::remove(s.begin(), s.end(), '&'), s.end());
        s = Trim(s);

        // Strip trailing ellipsis
        if (s.ends_with("...")) {
            s.erase(s.size() - 3);
        }

        s = Trim(s);
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    }

    std::vector<std::string> SplitPath(const std::string& path) {
        std::vector<std::string> parts;
        std::string current;
        for (char c : path) {
            if (c == '/' || c == '\\') {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) parts.push_back(current);
        return parts;
    }

    bool NamesMatch(const std::string& a, const std::string& b) {
        return NormalizeLabel(a) == NormalizeLabel(b);
    }

    std::string ToLowerAscii(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    }

    bool IsChineseLocaleTag(const std::string& locale) {
        auto l = ToLowerAscii(locale);
        return l == "zh" || l == "zh-cn" || l == "zh-hans" || l.starts_with("zh-");
    }

    bool IsEnglishLocaleTag(const std::string& locale) {
        auto l = ToLowerAscii(locale);
        return l == "en" || l == "en-us" || l == "en-gb" || l.starts_with("en-");
    }

    std::string TranslateMenuLabel(const std::string& label, const std::string& locale, bool enableI18n) {
        if (!enableI18n || locale.empty() || ToLowerAscii(locale) == "auto") return label;

        static const std::unordered_map<std::string, std::string> enToZh = {
            {"file", "文件"},
            {"edit", "编辑"},
            {"view", "视图"},
            {"playback", "播放"},
            {"library", "媒体库"},
            {"help", "帮助"},
            {"utilities", "工具"},
            {"tagging", "标签"},
            {"convert", "转换"},
            {"replaygain", "播放增益"},
            {"properties", "属性"},
            {"copy", "复制"},
            {"paste", "粘贴"},
            {"cut", "剪切"},
            {"remove", "移除"},
            {"open containing folder", "打开所在文件夹"},
            {"send to playlist", "发送到播放列表"},
            {"add to playback queue", "添加到播放队列"},
            {"remove from playback queue", "从播放队列中移除"},
            {"playback order", "播放顺序"},
            {"playback statistics", "播放统计信息"},
            {"open", "打开"},
            {"open audio cd", "打开音频 CD"},
            {"preferences", "首选项"},
            {"check for updates", "检查更新"},
            {"play", "播放"},
            {"pause", "暂停"},
            {"stop", "停止"},
            {"play or pause", "播放或暂停"},
            {"next", "下一首"},
            {"previous", "上一首"},
            {"next track", "下一首"},
            {"previous track", "上一首"},
            {"random", "随机"},
            {"shuffle", "乱序"},
            {"default", "默认"},
            {"repeat (playlist)", "重复(播放列表)"},
            {"repeat (track)", "重复(音轨)"},
            {"restart", "重启"},
            {"exit", "退出"},
            {"volume", "音量"},
            {"mute", "静音"},
            {"console", "控制台"}
        };

        static const std::unordered_map<std::string, std::string> zhToEn = {
            {"文件", "File"},
            {"编辑", "Edit"},
            {"视图", "View"},
            {"播放", "Playback"},
            {"媒体库", "Library"},
            {"帮助", "Help"},
            {"工具", "Utilities"},
            {"标签", "Tagging"},
            {"转换", "Convert"},
            {"播放增益", "ReplayGain"},
            {"属性", "Properties"},
            {"播放顺序", "Playback order"},
            {"播放统计信息", "Playback Statistics"},
            {"暂停", "Pause"},
            {"停止", "Stop"},
            {"上一首", "Previous"},
            {"下一首", "Next"},
            {"随机", "Random"},
            {"默认", "Default"},
            {"重启", "Restart"},
            {"退出", "Exit"}
        };

        if (IsChineseLocaleTag(locale)) {
            auto it = enToZh.find(NormalizeLabel(label));
            if (it != enToZh.end()) return it->second;
        } else if (IsEnglishLocaleTag(locale)) {
            auto it = zhToEn.find(Trim(label));
            if (it != zhToEn.end()) return it->second;
        }

        return label;
    }

    bool NamesMatchI18n(const std::string& actualName, const std::string& expectedName) {
        if (NamesMatch(actualName, expectedName)) return true;

        static const std::vector<std::pair<std::string, std::string>> aliases = {
            {"file", "文件"},
            {"edit", "编辑"},
            {"view", "视图"},
            {"playback", "播放"},
            {"library", "媒体库"},
            {"help", "帮助"},
            {"playback statistics", "播放统计信息"},
            {"playback order", "播放顺序"}
        };

        auto a = NormalizeLabel(actualName);
        auto b = NormalizeLabel(expectedName);
        for (const auto& [en, zh] : aliases) {
            if ((a == en && b == NormalizeLabel(zh)) || (a == NormalizeLabel(zh) && b == en)) {
                return true;
            }
        }
        return false;
    }

    bool IsMenuItemAvailable(const menu_tree_item::ptr& node) {
        if (!node.is_valid()) return false;
        auto f = node->flags();
        return (f & menu_flags::disabled) == 0;
    }

    void CountCommandAvailability(const menu_tree_item::ptr& node, int& total, int& available) {
        if (!node.is_valid()) return;
        if (node->isCommand()) {
            total++;
            if (IsMenuItemAvailable(node)) available++;
            return;
        }
        if (node->isSubmenu()) {
            const size_t count = node->childCount();
            for (size_t i = 0; i < count; i++) {
                auto child = node->childAt(i);
                if (!child.is_valid()) continue;
                CountCommandAvailability(child, total, available);
            }
        }
    }

    json BuildMainMenuFlatFallback(const std::string& locale, bool enableI18n) {
        json items = json::array();

        service_enum_t<mainmenu_commands> e;
        service_ptr_t<mainmenu_commands> ptr;
        while (e.next(ptr)) {
            t_uint32 count = ptr->get_command_count();
            for (t_uint32 i = 0; i < count; i++) {
                pfc::string8 name;
                ptr->get_name(i, name);

                GUID cmdGuid = ptr->get_command(i);
                std::string label = name.get_ptr() ? name.get_ptr() : "";
                std::string displayLabel = TranslateMenuLabel(label, locale, enableI18n);

                json item = {
                    { "type", "command" },
                    { "label", label },
                    { "displayLabel", displayLabel },
                    { "path", label },
                    { "displayPath", displayLabel },
                    { "guid", GuidToString(cmdGuid) },
                    { "available", true },
                    { "fallback", true }
                };
                items.push_back(item);
            }
        }

        return items;
    }

    // ================================================================
    // V1 HMENU-based tree builder
    // 使用 mainmenu_manager v1 API (instantiate + generate_menu_win32)
    // 遍历 Win32 HMENU 构建层级菜单树
    // 兼容所有 foobar2000 版本（含中文汉化版）
    // ================================================================

    json WalkHMenu(HMENU hmenu, const std::string& pathPrefix, const std::string& displayPathPrefix,
                   const std::string& locale, bool enableI18n) {
        json items = json::array();
        int count = GetMenuItemCount(hmenu);
        if (count <= 0) return items;

        for (int i = 0; i < count; i++) {
            try {
                MENUITEMINFOW mii = {};
                mii.cbSize = sizeof(mii);
                mii.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_SUBMENU | MIIM_ID;
                wchar_t buf[512] = {};
                mii.dwTypeData = buf;
                mii.cch = 511;

                if (!GetMenuItemInfoW(hmenu, i, TRUE, &mii)) continue;

                if (mii.fType & MFT_SEPARATOR) {
                    items.push_back({ {"type", "separator"} });
                    continue;
                }

                std::string label = WideToUtf8(std::wstring(buf));
                // 去掉快捷键后缀 (&X) 和加速键标记 (&)
                // 但保留用于 TranslateMenuLabel 的纯净名称
                std::string cleanLabel = label;
                // 移除 tab 后面的快捷键文本（如 "打开...\tCtrl+O" -> "打开..."）
                auto tabPos = cleanLabel.find('\t');
                if (tabPos != std::string::npos) {
                    cleanLabel = cleanLabel.substr(0, tabPos);
                }
                std::string displayLabel = TranslateMenuLabel(cleanLabel, locale, enableI18n);
                std::string path = pathPrefix.empty() ? cleanLabel : pathPrefix;
                if (!pathPrefix.empty()) {
                    path += '/';
                    path += cleanLabel;
                }

                std::string displayPath = displayPathPrefix.empty() ? displayLabel : displayPathPrefix;
                if (!displayPathPrefix.empty()) {
                    displayPath += '/';
                    displayPath += displayLabel;
                }

                if (mii.hSubMenu) {
                    json children = WalkHMenu(mii.hSubMenu, path, displayPath, locale, enableI18n);
                    json submenu = {
                        { "type", "submenu" },
                        { "label", cleanLabel },
                        { "displayLabel", displayLabel },
                        { "path", path },
                        { "displayPath", displayPath },
                        { "children", children }
                    };
                    items.push_back(submenu);
                } else {
                    bool disabled = (mii.fState & MFS_DISABLED) || (mii.fState & MFS_GRAYED);
                    bool checked = (mii.fState & MFS_CHECKED) != 0;

                    json item = {
                        { "type", "command" },
                        { "label", cleanLabel },
                        { "displayLabel", displayLabel },
                        { "path", path },
                        { "displayPath", displayPath },
                        { "available", !disabled },
                        { "checked", checked },
                        { "commandId", (int)mii.wID }
                    };
                    items.push_back(item);
                }
            } catch (...) {
                // 跳过有问题的菜单项
            }
        }

        return items;
    }

    json BuildMainMenuV1Tree(const std::string& locale, bool enableI18n, bool withAvailability) {
        struct TopMenu {
            GUID guid;
            const char* enName;
        };

        auto countAvailableCommands = [](const json& items,
                                         int& total,
                                         int& available,
                                         const auto& self) -> void {
            for (const auto& item : items) {
                std::string type = item.value("type", "");
                if (type == "command") {
                    total++;
                    if (item.value("available", true)) {
                        available++;
                    }
                    continue;
                }

                if (type == "submenu" && item.contains("children")) {
                    self(item["children"], total, available, self);
                }
            }
        };

        static const TopMenu topMenus[] = {
            { mainmenu_groups::file,     "File" },
            { mainmenu_groups::edit,     "Edit" },
            { mainmenu_groups::view,     "View" },
            { mainmenu_groups::playback, "Playback" },
            { mainmenu_groups::library,  "Library" },
            { mainmenu_groups::help,     "Help" },
        };

        json items = json::array();

        for (const auto& top : topMenus) {
            try {
                auto mgr = mainmenu_manager::get();
                mgr->instantiate(top.guid);

                HMENU hmenu = CreatePopupMenu();
                if (!hmenu) continue;

                mgr->generate_menu_win32(hmenu, 1, 65535,
                                         mainmenu_manager::flag_view_full);

                std::string label = top.enName;
                std::string displayLabel = TranslateMenuLabel(label, locale, enableI18n);
                json children = WalkHMenu(hmenu, label, displayLabel, locale, enableI18n);

                DestroyMenu(hmenu);

                if (children.empty()) continue;

                json submenu = {
                    { "type", "submenu" },
                    { "label", label },
                    { "displayLabel", displayLabel },
                    { "path", label },
                    { "displayPath", displayLabel },
                    { "children", children }
                };

                if (withAvailability) {
                    int total = 0, available = 0;
                    countAvailableCommands(children, total, available, countAvailableCommands);
                    submenu["availability"] = {
                        { "totalCommands", total },
                        { "availableCommands", available },
                        { "disabledCommands", total - available },
                        { "allAvailable", total > 0 ? (available == total) : true }
                    };
                }

                items.push_back(submenu);
            } catch (const std::exception& ex) {
                console::printf("[MenuApi] BuildMainMenuV1Tree: error for %s: %s", top.enName, ex.what());
            } catch (...) {
                console::printf("[MenuApi] BuildMainMenuV1Tree: unknown error for %s", top.enName);
            }
        }

        return items;
    }

    menu_tree_item::ptr FindMenuNodeByPath(const menu_tree_item::ptr& node, const std::vector<std::string>& parts, size_t index) {
        if (!node.is_valid() || index >= parts.size()) return nullptr;

        const size_t count = node->childCount();
        for (size_t i = 0; i < count; i++) {
            auto child = node->childAt(i);
            if (!child.is_valid()) continue;

            const char* name = child->name();
            if (!name) continue;

            if (!NamesMatchI18n(name, parts[index])) continue;
            if (index == parts.size() - 1)
                return child;
            if (child->isSubmenu()) {
                auto found = FindMenuNodeByPath(child, parts, index + 1);
                if (found.is_valid()) return found;
            }
        }

        return nullptr;
    }

    menu_tree_item::ptr FindMainMenuCommandByPath(const menu_tree_item::ptr& node, const std::vector<std::string>& parts, size_t index) {
        if (!node.is_valid() || index >= parts.size()) return nullptr;

        const size_t count = node->childCount();
        for (size_t i = 0; i < count; i++) {
            auto child = node->childAt(i);
            if (!child.is_valid()) continue;

            const char* name = child->name();
            if (!name) continue;

            if (!NamesMatchI18n(name, parts[index])) continue;
            if (index == parts.size() - 1 && child->isCommand()) return child;
            if (child->isSubmenu()) {
                auto found = FindMainMenuCommandByPath(child, parts, index + 1);
                if (found.is_valid()) return found;
            }
        }

        return nullptr;
    }

    menu_tree_item::ptr FindMainMenuCommandByName(const menu_tree_item::ptr& node, const std::string& name) {
        if (!node.is_valid()) return nullptr;

        const size_t count = node->childCount();
        for (size_t i = 0; i < count; i++) {
            auto child = node->childAt(i);
            if (!child.is_valid()) continue;

            const char* childName = child->name();
            if (childName && child->isCommand() && NamesMatchI18n(childName, name)) {
                return child;
            }
            if (child->isSubmenu()) {
                auto found = FindMainMenuCommandByName(child, name);
                if (found.is_valid()) return found;
            }
        }

        return nullptr;
    }

    metadb_handle_list GetDefaultContextItems() {
        metadb_handle_list items;

        // Prefer now playing item
        auto pc = playback_control::get();
        metadb_handle_ptr nowPlaying;
        if (pc->get_now_playing(nowPlaying)) {
            items.add_item(nowPlaying);
            return items;
        }

        // Fallback to active playlist selection
        playlist_manager::get()->activeplaylist_get_selected_items(items);
        return items;
    }

    bool ParseHandleList(const json& handlesJson, metadb_handle_list& out) {
        if (!handlesJson.is_array()) return false;

        auto mdb = metadb::get();

        for (const auto& h : handlesJson) {
            std::string path;
            t_uint32 subsong = 0;

            if (h.is_object()) {
                path = h.value("path", "");
                subsong = h.value("subsong", 0);
            } else if (h.is_string()) {
                path = h.get<std::string>();
                auto pos = path.find("|subsong:");
                if (pos != std::string::npos) {
                    try {
                        subsong = static_cast<t_uint32>(std::stoul(path.substr(pos + 9)));
                    } catch (...) {
                        subsong = 0;
                    }
                    path = path.substr(0, pos);
                }
            } else {
                continue;
            }

            if (path.empty()) continue;

            // Validate path against PathSecurity before creating handle
            std::wstring wpath = pfc::stringcvt::string_wide_from_utf8(path.c_str()).get_ptr();
            std::wstring pathError;
            // Argument order verified correct: wpath -> path (in), pathError -> errorMsg (out).
            // The name-similarity heuristic cross-matches the "path" prefix of pathError.
            // NOLINTNEXTLINE(readability-suspicious-call-argument)
            if (!PathSecurity::Instance().ValidateMediaAccess(wpath, pathError)) {
                continue;
            }

            metadb_handle_ptr handle = mdb->handle_create(path.c_str(), subsong);
            if (handle.is_valid()) {
                out.add_item(handle);
            }
        }

        return out.get_count() > 0;
    }

    // 上下文菜单初始化 — getContextMenu / runContextCommandById 共享逻辑
    struct ContextMenuInitResult {
        bool inited = false;
        std::string effectiveMode;
        std::string error;
    };

    static ContextMenuInitResult InitContextMenu(
        service_ptr_t<contextmenu_manager>& mgr,
        const std::string& mode,
        const json& params) {
        ContextMenuInitResult result;
        result.effectiveMode = mode;

        metadb_handle_list handles;
        bool hasHandles = ParseHandleList(params.value("handles", json::array()), handles);

        if (mode == "handles") {
            if (!hasHandles) {
                result.error = "handles required for mode=handles";
                return result;
            }
            mgr->init_context(handles, contextmenu_manager::flag_view_full);
            result.inited = true;
        } else if (mode == "playlist") {
            mgr->init_context_playlist(contextmenu_manager::flag_view_full);
            result.inited = true;
        } else if (mode == "nowPlaying") {
            result.inited = mgr->init_context_now_playing(contextmenu_manager::flag_view_full);
            if (!result.inited) {
                result.error = "No now playing item";
            }
        } else {
            // auto mode: handles → nowPlaying → selection → playlist
            if (hasHandles) {
                mgr->init_context(handles, contextmenu_manager::flag_view_full);
                result.inited = true;
                result.effectiveMode = "handles";
            } else if (mgr->init_context_now_playing(contextmenu_manager::flag_view_full)) {
                result.inited = true;
                result.effectiveMode = "nowPlaying";
            } else {
                metadb_handle_list fallback = GetDefaultContextItems();
                if (fallback.get_count() > 0) {
                    mgr->init_context(fallback, contextmenu_manager::flag_view_full);
                    result.inited = true;
                    result.effectiveMode = "selection";
                } else {
                    mgr->init_context_playlist(contextmenu_manager::flag_view_full);
                    result.inited = true;
                    result.effectiveMode = "playlist";
                }
            }
        }
        return result;
    }

    json BuildMenuTreeJson(
        const menu_tree_item::ptr& node,
        const std::string& pathPrefix,
        const std::string& displayPathPrefix,
        const std::string& locale,
        bool enableI18n,
        bool withAvailability
    ) {
        if (!node.is_valid()) return json();

        if (node->isSeparator()) {
            return { { "type", "separator" } };
        }

        const char* namePtr = node->name();
        std::string label = namePtr ? namePtr : "";
        std::string displayLabel = TranslateMenuLabel(label, locale, enableI18n);
        std::string path = pathPrefix.empty() ? label : (pathPrefix + "/" + label);
        std::string displayPath = displayPathPrefix.empty() ? displayLabel : (displayPathPrefix + "/" + displayLabel);

        if (node->isSubmenu()) {
            json children = json::array();
            const size_t count = node->childCount();
            for (size_t i = 0; i < count; i++) {
                try {
                    auto child = node->childAt(i);
                    if (!child.is_valid()) continue;
                    auto item = BuildMenuTreeJson(child, path, displayPath, locale, enableI18n, withAvailability);
                    if (!item.is_null()) children.push_back(item);
                } catch (const std::exception& ex) {
                    // 跳过有问题的子项（中文版 SDK 可能对某些项抛异常）
                    console::printf("[MenuApi] BuildMenuTreeJson: skipping submenu child %u: %s", (unsigned)i, ex.what());
                } catch (...) {
                    // Non-std exception — skip this child silently
                }
            }

            json result = {
                { "type", "submenu" },
                { "label", label },
                { "displayLabel", displayLabel },
                { "path", path },
                { "displayPath", displayPath },
                { "flags", node->flags() },
                { "children", children }
            };

            if (withAvailability) {
                int total = 0;
                int available = 0;
                try {
                    CountCommandAvailability(node, total, available);
                } catch (...) {
                    // Silently ignore — totals stay 0
                }
                result["availability"] = {
                    { "totalCommands", total },
                    { "availableCommands", available },
                    { "disabledCommands", total - available },
                    { "allAvailable", total > 0 ? (available == total) : true }
                };
            }

            return result;
        }

        if (node->isCommand()) {
            json item = {
                { "type", "command" },
                { "label", label },
                { "displayLabel", displayLabel },
                { "path", path },
                { "displayPath", displayPath },
                { "flags", node->flags() }
            };

            // commandID / commandGuid / subCommandGuid 在中文版 SDK 可能抛异常
            try {
                item["commandId"] = node->commandID();
            } catch (...) {
                item["commandId"] = 0;
            }
            try {
                item["available"] = IsMenuItemAvailable(node);
            } catch (...) {
                item["available"] = true;
            }
            try {
                GUID guid = node->commandGuid();
                if (guid != pfc::guid_null) item["guid"] = GuidToString(guid);
            } catch (...) {
                // Silently ignore — guid field omitted
            }
            try {
                GUID subGuid = node->subCommandGuid();
                if (subGuid != pfc::guid_null) item["subGuid"] = GuidToString(subGuid);
            } catch (...) {
                // Silently ignore — subGuid field omitted
            }

            return item;
        }

        return json();
    }
}


// ==========================================================================
// Menu API handler functions
// ==========================================================================
namespace {


json MenuRunMainMenuCommand(const json& params) {
    std::string command = params.value("command", "");
    if (command.empty()) {
        return { {"success", false}, {"error", "command is required"} };
    }

    // GUID form
    GUID guid;
    if (StringToGuid(command, guid)) {
        bool ok = mainmenu_commands::g_execute(guid);
        return { {"success", ok}, {"guid", command} };
    }

    // Path/name form
    auto mgr = mainmenu_manager_v2::tryGet();
    if (mgr.is_valid()) {
        auto root = mgr->generate_menu(mainmenu_manager::flag_view_full);
        if (root.is_valid()) {
            auto parts = SplitPath(command);
            menu_tree_item::ptr item;
            if (parts.size() > 1) {
                item = FindMainMenuCommandByPath(root, parts, 0);
            } else {
                item = FindMainMenuCommandByName(root, command);
            }
            if (item.is_valid()) {
                item->execute(service_ptr_t<service_base>());
                return { {"success", true} };
            }
        }
    }

    // Fallback: exact command name
    GUID foundGuid;
    if (mainmenu_commands::g_find_by_name(command.c_str(), foundGuid)) {
        bool ok = mainmenu_commands::g_execute(foundGuid);
        return { {"success", ok}, {"guid", GuidToString(foundGuid)} };
    }

    return { {"success", false}, {"error", "Command not found"} };
}


json MenuRunContextCommand(const json& params) {
    std::string command = params.value("command", "");
    if (command.empty()) {
        return { {"success", false}, {"error", "command is required"} };
    }

    metadb_handle_list items = GetDefaultContextItems();
    if (items.get_count() == 0) {
        return { {"success", false}, {"error", "No track selected or playing"} };
    }

    GUID guid;
    if (StringToGuid(command, guid)) {
        bool ok = menu_helpers::run_command_context(guid, pfc::guid_null, items);
        return { {"success", ok}, {"guid", command}, {"itemCount", items.get_count()} };
    }

    service_ptr_t<contextmenu_item> item;
    unsigned index = 0;
    if (menu_helpers::find_command_by_name(command.c_str(), item, index)) {
        item->item_execute_simple(index, pfc::guid_null, items, contextmenu_item::caller_undefined);
        return { {"success", true}, {"itemCount", items.get_count()} };
    }

    if (menu_helpers::guid_from_name(command.c_str(), (unsigned)command.size(), guid)) {
        bool ok = menu_helpers::run_command_context(guid, pfc::guid_null, items);
        return { {"success", ok}, {"guid", GuidToString(guid)}, {"itemCount", items.get_count()} };
    }

    return { {"success", false}, {"error", "Command not found"} };
}

static json BuildMainMenuResponse(const std::string& root,
                                  const std::string& requestedRoot,
                                  bool rootMatched,
                                  const std::string& locale,
                                  bool enableI18n,
                                  bool withAvailability,
                                  const json& items,
                                  const char* source = nullptr) {
    json result = {
        {"success", true},
        {"root", root},
        {"requestedRoot", requestedRoot},
        {"rootMatched", rootMatched},
        {"locale", locale},
        {"i18n", enableI18n},
        {"withAvailability", withAvailability},
        {"items", items}
    };

    if (source && *source) {
        result["source"] = source;
    }

    return result;
}

static std::optional<json> TryGetMainMenuFromV2(const std::string& rootName,
                                                const std::string& locale,
                                                bool enableI18n,
                                                bool withAvailability) {
    auto mgr = mainmenu_manager_v2::tryGet();
    if (!mgr.is_valid()) {
        return std::nullopt;
    }

    auto root = mgr->generate_menu(mainmenu_manager::flag_view_full);
    if (!root.is_valid()) {
        return std::nullopt;
    }

    console::printf("[MenuApi] getMainMenu: v2 tree OK, childCount=%u",
                    static_cast<unsigned>(root->childCount()));

    menu_tree_item::ptr base = root;
    std::string baseLabel;

    if (!rootName.empty()) {
        auto parts = SplitPath(rootName);
        auto found = FindMenuNodeByPath(root, parts, 0);
        if (found.is_valid() && found->isSubmenu()) {
            base = found;
            const char* label = base->name();
            baseLabel = label ? label : rootName;
        }
    }

    json items = json::array();
    for (size_t index = 0; index < base->childCount(); index++) {
        try {
            auto child = base->childAt(index);
            if (!child.is_valid()) {
                continue;
            }

            auto item = BuildMenuTreeJson(child, baseLabel, baseLabel,
                                          locale, enableI18n,
                                          withAvailability);
            if (!item.is_null()) {
                items.push_back(item);
            }
        } catch (const std::exception& itemEx) {
            console::printf("[MenuApi] v2 tree: skip item %u: %s",
                            static_cast<unsigned>(index), itemEx.what());
        } catch (...) {
        }
    }

    // Argument order verified correct against all 5 call sites: the first
    // parameter is always the RESOLVED root label (baseLabel, may be empty),
    // the second always echoes the REQUESTED root name. Swapping them would
    // return the request as the effective root — the actual bug the heuristic
    // fears. It cross-matches only because rootName lexically resembles 'root'.
    // NOLINTNEXTLINE(readability-suspicious-call-argument)
    return BuildMainMenuResponse(baseLabel, rootName,
                                 rootName.empty() ? true : !baseLabel.empty(),
                                 locale, enableI18n, withAvailability, items);
}

static std::optional<json> TryGetMainMenuFromV1(const std::string& rootName,
                                                const std::string& locale,
                                                bool enableI18n,
                                                bool withAvailability) {
    json v1Items = BuildMainMenuV1Tree(locale, enableI18n, withAvailability);
    if (v1Items.empty()) {
        return std::nullopt;
    }

    console::printf("[MenuApi] getMainMenu: v1 HMENU tree OK, %u top-level menus",
                    static_cast<unsigned>(v1Items.size()));

    if (rootName.empty()) {
        return BuildMainMenuResponse("", rootName, true, locale, enableI18n,
                                     withAvailability, v1Items, "v1-hmenu");
    }

    for (const auto& topMenu : v1Items) {
        if (NamesMatchI18n(topMenu.value("label", ""), rootName) ||
            NamesMatchI18n(topMenu.value("displayLabel", ""), rootName)) {
            return BuildMainMenuResponse(topMenu.value("label", ""), rootName,
                                         true, locale, enableI18n,
                                         withAvailability,
                                         topMenu.value("children", json::array()),
                                         "v1-hmenu");
        }
    }

    return BuildMainMenuResponse("", rootName, false, locale, enableI18n,
                                 withAvailability, v1Items, "v1-hmenu");
}


json MenuGetMainMenu(const json& params) {
    std::string rootName = params.value("root", "");
    std::string locale = params.value("locale", "auto");
    bool enableI18n = params.value("i18n", true);
    bool withAvailability = params.value("withAvailability", true);

    // ================================================================
    // 策略: v2 menu_tree → v1 HMENU → flat fallback
    // 中文汉化版 foobar2000 的 v2 generate_menu() 会抛 "找不到命令"，
    // 因此需要 v1 HMENU 作为可靠回退。
    // ================================================================

    try {
        auto v2Result = TryGetMainMenuFromV2(rootName, locale, enableI18n,
                                             withAvailability);
        if (v2Result) return *v2Result;
    } catch (const std::exception& ex) {
        console::printf("[MenuApi] getMainMenu: v2 failed (%s), trying v1 HMENU...", ex.what());
    } catch (...) {
        console::printf("[MenuApi] getMainMenu: v2 failed (unknown), trying v1 HMENU...");
    }

    // — v1 HMENU 方案（兼容中文版 + 1.x） —
    try {
        auto v1Result = TryGetMainMenuFromV1(rootName, locale, enableI18n,
                                             withAvailability);
        if (v1Result) return *v1Result;
    } catch (const std::exception& ex) {
        console::printf("[MenuApi] getMainMenu: v1 HMENU also failed: %s", ex.what());
    } catch (...) {
        console::printf("[MenuApi] getMainMenu: v1 HMENU failed (unknown)");
    }

    // — 最终回退: flat 命令列表 —
    console::printf("[MenuApi] getMainMenu: all tree methods failed, using flat fallback");
    try {
        json result = BuildMainMenuResponse("", rootName, false, locale,
                                            enableI18n, withAvailability,
                                            BuildMainMenuFlatFallback(locale, enableI18n));
        result["fallback"] = "flat-mainmenu-commands";
        return result;
    } catch (...) {
        return {
            {"success", false},
            {"error", "All menu tree methods failed"},
            {"items", json::array()}
        };
    }
}


json MenuGetContextMenu(const json& params) {
    std::string mode = params.value("mode", "auto");
    std::string locale = params.value("locale", "auto");
    bool enableI18n = params.value("i18n", true);
    bool withAvailability = params.value("withAvailability", true);

    service_ptr_t<contextmenu_manager> mgr;
    contextmenu_manager::g_create(mgr);

    auto initResult = InitContextMenu(mgr, mode, params);
    if (!initResult.inited) {
        return { {"success", false}, {"error", initResult.error.empty() ? "Failed to initialize context menu" : initResult.error} };
    }

    service_ptr_t<contextmenu_manager_v2> mgr2;
    if (!mgr->service_query_t(mgr2)) {
        return { {"success", false}, {"error", "contextmenu_manager_v2 not available"} };
    }

    auto root = mgr2->build_menu();
    if (!root.is_valid()) {
        return { {"success", false}, {"error", "Failed to build context menu"} };
    }

    json items = json::array();
    const size_t count = root->childCount();
    for (size_t i = 0; i < count; i++) {
        auto child = root->childAt(i);
        if (!child.is_valid()) continue;
        auto item = BuildMenuTreeJson(child, "", "", locale, enableI18n, withAvailability);
        if (!item.is_null()) items.push_back(item);
    }

    return {
        {"success", true},
        {"mode", initResult.effectiveMode},
        {"locale", locale},
        {"i18n", enableI18n},
        {"withAvailability", withAvailability},
        {"items", items}
    };
}


json MenuRunContextCommandById(const json& params) {
    int id = params.value("id", -1);
    if (id < 0) {
        return { {"success", false}, {"error", "id is required"} };
    }

    std::string mode = params.value("mode", "auto");

    service_ptr_t<contextmenu_manager> mgr;
    contextmenu_manager::g_create(mgr);

    auto initResult = InitContextMenu(mgr, mode, params);
    if (!initResult.inited) {
        return { {"success", false}, {"error", initResult.error.empty() ? "Failed to initialize context menu" : initResult.error} };
    }

    bool ok = mgr->execute_by_id(static_cast<unsigned>(id));
    return { {"success", ok} };
}


json MenuShowNativePopup(const json& params) {
    std::string mode = params.value("mode", "playlist");
    unsigned flags = contextmenu_manager::flag_show_shortcuts | contextmenu_manager::flag_view_full;
    
    // 获取面板 HWND 用于坐标转换
    HWND panelHwnd = nullptr;
    if (params.contains("_callerHwnd")) {
        auto h = reinterpret_cast<HWND>(params["_callerHwnd"].get<intptr_t>());
        if (h && IsWindow(h)) panelHwnd = h;
    }
    HWND parentHwnd = panelHwnd ? panelHwnd : core_api::get_main_window();
    if (!parentHwnd) {
        return {{"success", false}, {"error", "No parent window"}};
    }
    
    // 直接使用系统光标位置（最可靠，不受 DPI/CSS 像素差异影响）
    POINT pt;
    GetCursorPos(&pt);
    
    // 创建并初始化 contextmenu_manager
    service_ptr_t<contextmenu_manager> mgr;
    contextmenu_manager::g_create(mgr);
    
    bool inited = false;
    if (mode == "playlist") {
        mgr->init_context_playlist(flags);
        inited = true;
    } else if (mode == "nowPlaying") {
        inited = mgr->init_context_now_playing(flags);
    } else if (mode == "handles") {
        metadb_handle_list handles;
        if (ParseHandleList(params.value("handles", json::array()), handles)) {
            mgr->init_context(handles, flags);
            inited = true;
        }
    }
    
    if (!inited) {
        return {{"success", false}, {"error", "Failed to init context for mode: " + mode}};
    }
    
    // 保存状态，通过 SetTimer 延迟执行 TrackPopupMenu
    // 让桥接回调先返回，WebView2 待处理消息先完成，然后再弹菜单
    auto& pending = GetPendingContextMenu();
    pending.mgr = mgr;
    pending.pt = pt;
    pending.parent = parentHwnd;
    
    SetTimer(parentHwnd, PendingContextMenu::TIMER_ID, 1,
        [](HWND hwnd, UINT, UINT_PTR id, DWORD) {
            KillTimer(hwnd, id);
            auto& p = GetPendingContextMenu();
            if (p.mgr.is_valid()) {
                HWND top = ::GetAncestor(hwnd, GA_ROOT);
                if (top) SetForegroundWindow(top);
                p.mgr->win32_run_menu_popup(hwnd, &p.pt);
                p.mgr.release();
            }
        });
    
    return {{"success", true}};
}

// ---- Self-Drawn Menu APIs (自绘菜单引擎) ----
// menu.show {items, x?, y?}: 在屏幕坐标(缺省取光标)显示自绘菜单，返回 menuId。
json MenuShow(const json& params) {
    json items = (params.contains("items") && params["items"].is_array()) ? params["items"] : json::array();
    int x = params.value("x", -1);
    int y = params.value("y", -1);
    if (x < 0 || y < 0) {
        POINT pt{};
        GetCursorPos(&pt);
        if (x < 0) x = pt.x;
        if (y < 0) y = pt.y;
    }
    // menu.show 走 FullscreenOverlay（默认）；tray 自绘菜单的 ContentSized 由 tray owner-mode 驱动。
    std::string menuId = MenuOverlayHost::GetInstance().Show(items, x, y);
    if (menuId.empty()) {
        return {{"success", false}, {"error", "failed to show menu overlay"}};
    }
    return {{"success", true}, {"menuId", menuId}};
}

// menu.close {reason?}: 关闭当前自绘菜单。
json MenuClose(const json& params) {
    const std::string reason = params.value("reason", std::string("api"));
    MenuOverlayHost::GetInstance().Hide(reason);
    return {{"success", true}};
}

// menu.__getMenuState: 前端 pull 当前菜单状态(内部)。
json MenuGetMenuState(const json& /*params*/) {
    return MenuOverlayHost::GetInstance().GetMenuStateJson();
}

// menu.__select {itemId}: 前端点击菜单项回报(内部) -> menu:select + 关闭。
json MenuSelect(const json& params) {
    const std::string itemId = params.value("itemId", std::string());
    MenuOverlayHost::GetInstance().OnSelect(itemId);
    return {{"success", true}};
}

// menu.__dismiss {reason?}: 前端外点击/Esc 回报(内部) -> 关闭。
json MenuDismiss(const json& params) {
    const std::string reason = params.value("reason", std::string("api"));
    MenuOverlayHost::GetInstance().Hide(reason);
    return {{"success", true}};
}

// menu.__ready {w,h}: ContentSized 渲染器回报内容物理像素(内部) -> 定位窗口。
json MenuReady(const json& params) {
    int w = params.value("w", 0);
    int h = params.value("h", 0);
    int ox = params.value("ox", 0);  // 固定窗设计未使用（OnContentMeasured 内 (void)originXPhysical）；保留参数兼容渲染器回报
    bool hasSub = params.value("hasSub", false);  // 根菜单是否有子菜单：决定窗口是否需 ×2 预留（无则=精确根宽，避免亚克力露出空白面板）
    MenuOverlayHost::GetInstance().OnContentMeasured(w, h, ox, hasSub);
    return {{"success", true}};
}

// menu.__valueChanged {itemId,value}: 富控件(rating/slider)值变更回报(内部)
// -> 经 owner-mode value sink 回报，【不关闭菜单】。
json MenuValueChanged(const json& params) {
    const std::string itemId = params.value("itemId", std::string());
    int value = params.value("value", 0);
    MenuOverlayHost::GetInstance().OnValueChanged(itemId, value);
    return {{"success", true}};
}
} // namespace

void RegisterMenuApi() {
    auto& bridge = BridgeCore::GetInstance();

    bridge.RegisterApi("menu.runMainMenuCommand", MenuRunMainMenuCommand);
    bridge.RegisterApi("menu.runContextCommand", MenuRunContextCommand);
    bridge.RegisterApi("menu.getMainMenu", MenuGetMainMenu);
    bridge.RegisterApi("menu.getContextMenu", MenuGetContextMenu);
    bridge.RegisterApi("menu.runContextCommandById", MenuRunContextCommandById);
    bridge.RegisterApi("menu.showNativePopup", MenuShowNativePopup);
    bridge.RegisterApi("menu.show", MenuShow);
    bridge.RegisterApi("menu.close", MenuClose);
    bridge.RegisterApi("menu.__getMenuState", MenuGetMenuState);
    bridge.RegisterApi("menu.__select", MenuSelect);
    bridge.RegisterApi("menu.__dismiss", MenuDismiss);
    bridge.RegisterApi("menu.__ready", MenuReady);
    bridge.RegisterApi("menu.__valueChanged", MenuValueChanged);
}
