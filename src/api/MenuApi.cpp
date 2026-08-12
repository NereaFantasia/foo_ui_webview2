// MenuApi.cpp - Menu command helpers
// Provides execution of main menu and context menu commands by name/path

#include "pch.h"
#include "api/MenuApi.h"
#include "api/BridgeCore.h"
#include "api/ErrorEnvelope.h"
#include "api/MenuNodeContract.h"
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
#include "utils/StringUtils.h"
#include "window/MenuOverlayHost.h"
#include "window/MenuResourceLimits.h"

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

    // ================================================================
    // Main menu resolution index
    //
    // A flat "display name -> GUID + live state" table built straight from the
    // mainmenu_commands service enumeration, deliberately WITHOUT going through
    // mainmenu_manager_v2::generate_menu(). That call throws on localized hosts
    // (see MenuGetMainMenu below), which is exactly why the v2 menu tree cannot
    // be the only way to resolve or execute a command.
    //
    // The index is what lets three separate defects share one fix:
    //   - execution can resolve a name without the v2 tree
    //   - the v1 HMENU tier can backfill the `guid` it structurally lacks
    //   - the flat tier can report real availability instead of hardcoding it
    //
    // Text comes from get_display(), which is the same call generate_menu_win32()
    // uses to render the HMENU. That shared origin is why HMENU labels line up
    // with index entries; a JS-side join can never reach this alignment point.
    //
    // State is a SNAPSHOT. get_display() is only meaningful at the moment a menu
    // is about to be shown (Pause/Resume, Always-on-top swap as the user acts),
    // so the index must be rebuilt per request and never cached across calls.
    // ================================================================

    struct MainMenuIndexEntry {
        std::string name;         // get_name(), the stable-ish internal label
        std::string displayText;  // get_display() text, what the menu really shows
        std::string path;         // dynamic children only; empty for static slots
        GUID guid = pfc::guid_null;
        GUID subGuid = pfc::guid_null;  // non-null only for dynamic children
        menu_node::State state;
        bool isDynamicParent = false;  // container slot: never executable
    };

    // Recursively appends the leaf commands of a v2 dynamic subtree.
    void CollectIndexDynamicNodes(const mainmenu_node::ptr& node,
                                  const std::string& pathPrefix,
                                  const GUID& ownerGuid,
                                  int depth,
                                  std::vector<MainMenuIndexEntry>& out) {
        if (!node.is_valid() || menu_node::DepthExceeded(depth)) return;

        t_uint32 type = mainmenu_node::type_separator;
        try {
            type = node->get_type();
        } catch (...) {
            return;
        }
        if (type == mainmenu_node::type_separator) return;

        pfc::string8 text;
        t_uint32 flags = 0;
        try {
            node->get_display(text, flags);
        } catch (...) {
            // Keep walking with an empty label rather than dropping the subtree.
        }

        const std::string label = StringUtils::SafeUtf8(text.get_ptr());
        std::string path = pathPrefix;
        // Components often label the root of their dynamic subtree with the same
        // text as the owning static slot; appending it twice yields paths like
        // "Desktop Lyrics/Desktop Lyrics/Show".
        const bool duplicatesOwnerLabel = (depth == 0 && label == pathPrefix);
        if (!label.empty() && !duplicatesOwnerLabel) {
            if (!path.empty()) path += '/';
            path += label;
        }

        if (type == mainmenu_node::type_group) {
            t_size childCount = 0;
            try {
                childCount = node->get_children_count();
            } catch (...) {
                return;
            }
            for (t_size i = 0; i < childCount; i++) {
                mainmenu_node::ptr child;
                try {
                    child = node->get_child(i);
                } catch (...) {
                    continue;
                }
                CollectIndexDynamicNodes(child, path, ownerGuid, depth + 1, out);
            }
            return;
        }

        MainMenuIndexEntry entry;
        entry.name = label;
        entry.displayText = label;
        entry.path = path;
        entry.guid = ownerGuid;
        try {
            entry.subGuid = node->get_guid();
        } catch (...) {
            // Leave null; the entry then resolves to the owning slot only.
        }
        // mainmenu_node::get_display() returns void, so a dynamic node has no
        // "return false to hide" signal; flag_defaulthidden is the only cue.
        entry.state = menu_node::NormalizeMainMenu(flags, /*displayReturnedTrue=*/true);
        out.push_back(std::move(entry));
    }

    std::vector<MainMenuIndexEntry> BuildMainMenuIndex() {
        std::vector<MainMenuIndexEntry> index;

        service_enum_t<mainmenu_commands> e;
        service_ptr_t<mainmenu_commands> ptr;
        while (e.next(ptr)) {
            t_uint32 count = 0;
            try {
                count = ptr->get_command_count();
            } catch (...) {
                continue;
            }

            service_ptr_t<mainmenu_commands_v2> v2;
            const bool hasV2 = ptr->service_query_t(v2);

            for (t_uint32 i = 0; i < count; i++) {
                MainMenuIndexEntry entry;

                pfc::string8 name;
                try {
                    ptr->get_name(i, name);
                } catch (...) {
                    // A throwing component still gets an entry via get_display.
                }
                entry.name = StringUtils::SafeUtf8(name.get_ptr());

                try {
                    entry.guid = ptr->get_command(i);
                } catch (...) {
                    continue;  // No GUID means no stable address; skip.
                }
                if (entry.guid == pfc::guid_null) continue;

                t_uint32 flags = 0;
                bool displayed = true;
                pfc::string8 displayText;
                try {
                    displayed = ptr->get_display(i, displayText, flags);
                } catch (...) {
                    // Treat a throwing component as "shown with no flags".
                }
                entry.displayText = StringUtils::SafeUtf8(displayText.get_ptr());
                if (entry.displayText.empty()) entry.displayText = entry.name;
                entry.state = menu_node::NormalizeMainMenu(flags, displayed);

                bool isDynamic = false;
                if (hasV2) {
                    try {
                        isDynamic = v2->is_command_dynamic(i);
                    } catch (...) {
                        isDynamic = false;
                    }
                }
                entry.isDynamicParent = isDynamic;
                index.push_back(entry);

                if (!isDynamic) continue;

                mainmenu_node::ptr root;
                try {
                    root = v2->dynamic_instantiate(i);
                } catch (...) {
                    continue;
                }
                CollectIndexDynamicNodes(root, entry.name, entry.guid, 0, index);
            }
        }

        return index;
    }

    // Exact-match lookup by leaf label, against both the internal name and the
    // displayed text. Matching is EXACT per menu_node::SegmentsEqual: a substring
    // matcher would let "Rating/1" resolve to "Rating/10" and run the wrong
    // command while reporting success.
    //
    // Only the LAST path segment is matched. Uniqueness is then enforced across
    // the whole menu, so a preceding path prefix could not have narrowed a unique
    // hit any further; an ambiguous leaf name is reported rather than guessed.
    std::vector<const MainMenuIndexEntry*> FindIndexCandidates(
        const std::vector<MainMenuIndexEntry>& index, const std::string& query) {
        std::vector<const MainMenuIndexEntry*> matches;

        const auto parts = menu_node::SplitPath(query);
        if (parts.empty()) return matches;
        const std::string& leaf = parts.back();

        for (const auto& entry : index) {
            if (entry.isDynamicParent) continue;  // container, not a command
            const bool hit = menu_node::SegmentsEqual(entry.name, leaf) ||
                             menu_node::SegmentsEqual(entry.displayText, leaf);
            if (hit) matches.push_back(&entry);
        }
        return matches;
    }

    // Looks up a leaf label for the guid backfill in the v1 HMENU tier. Returns
    // nullptr unless exactly one entry matches, so an ambiguous label leaves the
    // `guid` field absent instead of attaching a guessed address.
    const MainMenuIndexEntry* FindUniqueIndexEntry(
        const std::vector<MainMenuIndexEntry>& index, const std::string& label) {
        const auto matches = FindIndexCandidates(index, label);
        return matches.size() == 1 ? matches.front() : nullptr;
    }

    // Looks up an entry by address so the GUID request form can be state-checked
    // like the name and path forms. Without this the same disabled command would
    // be refused by name yet report success by GUID.
    //
    // Returns nullptr when the address is not in the index at all; the caller
    // must then still attempt execution, because a GUID the caller obtained
    // elsewhere may be valid even though this enumeration did not surface it.
    const MainMenuIndexEntry* FindIndexEntryByAddress(
        const std::vector<MainMenuIndexEntry>& index,
        const GUID& guid,
        const GUID& subGuid) {
        for (const auto& entry : index) {
            if (entry.guid != guid) continue;
            if (entry.subGuid != subGuid) continue;
            return &entry;
        }
        return nullptr;
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

    // Last-resort tier: a flat command list with no hierarchy at all.
    //
    // Built from the shared index so availability is READ from get_display()
    // rather than hardcoded. The previous version claimed `available: true` for
    // every entry, which meant the one tier that always produces a usable GUID
    // also reported state that was outright fabricated — disabled commands were
    // indistinguishable from enabled ones.
    json BuildMainMenuFlatFallback(const std::string& locale, bool enableI18n,
                                   const std::vector<MainMenuIndexEntry>& index) {
        json items = json::array();

        for (const auto& entry : index) {
            // A dynamic container slot is not invokable; executing one is
            // undefined behaviour in the SDK.
            if (entry.isDynamicParent) continue;

            const std::string& label = entry.name.empty() ? entry.displayText : entry.name;
            std::string displayLabel = TranslateMenuLabel(label, locale, enableI18n);
            if (!entry.displayText.empty() && entry.displayText != entry.name) {
                // Prefer what the menu actually shows; on a localized host this
                // is the only label the user can recognize.
                displayLabel = entry.displayText;
            }

            const std::string path = entry.path.empty() ? label : entry.path;

            json item = {
                { "type", "command" },
                { "label", label },
                { "displayLabel", displayLabel },
                { "path", path },
                { "displayPath", path },
                { "guid", GuidToString(entry.guid) },
                { "available", entry.state.enabled },
                { "enabled", entry.state.enabled },
                { "checked", entry.state.checked },
                { "radioChecked", entry.state.radioChecked },
                { "hidden", entry.state.hidden },
                { "flags", entry.state.flags },
                { "source", menu_node::ToString(entry.subGuid != pfc::guid_null
                                                    ? menu_node::Source::MainMenuDynamic
                                                    : menu_node::Source::MainMenuStatic) },
                { "executable", true },
                { "fallback", true }
            };
            if (entry.subGuid != pfc::guid_null) {
                item["subGuid"] = GuidToString(entry.subGuid);
            }
            items.push_back(item);
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
                   const std::string& locale, bool enableI18n,
                   const std::vector<MainMenuIndexEntry>* index) {
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
                    json children = WalkHMenu(mii.hSubMenu, path, displayPath, locale, enableI18n, index);
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
                    const menu_node::State state =
                        menu_node::NormalizeHmenu(static_cast<std::uint32_t>(mii.fState));

                    json item = {
                        { "type", "command" },
                        { "label", cleanLabel },
                        { "displayLabel", displayLabel },
                        { "path", path },
                        { "displayPath", displayPath },
                        { "available", state.enabled },
                        { "enabled", state.enabled },
                        { "checked", state.checked },
                        { "radioChecked", state.radioChecked },
                        { "hidden", state.hidden },
                        { "flags", state.flags },
                        { "source", menu_node::ToString(menu_node::Source::HmenuFallback) },
                        { "commandId", (int)mii.wID }
                    };

                    // Backfill the GUID this tier cannot produce on its own: a
                    // Win32 HMENU carries only wID, and that id dies with the
                    // local mainmenu_manager that generated it, so it addresses
                    // nothing. Resolving the label against the index makes the
                    // v1 tier executable for the first time.
                    //
                    // The label is matched against the same get_display() text
                    // generate_menu_win32() rendered this HMENU from, so the two
                    // agree by construction. An ambiguous label yields no guid
                    // rather than a guessed one.
                    const MainMenuIndexEntry* hit =
                        index ? FindUniqueIndexEntry(*index, cleanLabel) : nullptr;
                    if (hit) {
                        item["guid"] = GuidToString(hit->guid);
                        if (hit->subGuid != pfc::guid_null) {
                            item["subGuid"] = GuidToString(hit->subGuid);
                        }
                        item["executable"] = true;
                    } else {
                        item["executable"] = false;
                        item["unaddressableReason"] =
                            menu_node::ToString(menu_node::Unaddressable::NoStableIdentifier);
                    }
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

        // Built once per request and shared by every top-level menu: the index
        // enumerates all mainmenu_commands services, so rebuilding it per menu
        // would repeat the same full enumeration six times. It must NOT outlive
        // the request — get_display() state is a snapshot (Pause/Resume flips).
        const std::vector<MainMenuIndexEntry> index = BuildMainMenuIndex();

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
                json children = WalkHMenu(hmenu, label, displayLabel, locale, enableI18n, &index);

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

    // `outCaller` reports which caller GUID the returned tracks belong to.
    // Components may vary an item's visibility and enabled state per caller, so
    // executing under caller_undefined while the enumeration side reads state
    // under the real caller can run a command the caller was told was
    // unavailable — the two must agree on the same context.
    metadb_handle_list GetDefaultContextItems(GUID* outCaller = nullptr) {
        metadb_handle_list items;

        // Prefer now playing item
        auto pc = playback_control::get();
        metadb_handle_ptr nowPlaying;
        if (pc->get_now_playing(nowPlaying)) {
            items.add_item(nowPlaying);
            if (outCaller) *outCaller = contextmenu_item::caller_now_playing;
            return items;
        }

        // Fallback to active playlist selection
        playlist_manager::get()->activeplaylist_get_selected_items(items);
        if (outCaller) *outCaller = contextmenu_item::caller_active_playlist_selection;
        return items;
    }

    metadb_handle_list GetSelectedContextItems() {
        metadb_handle_list items;
        playlist_manager::get()->activeplaylist_get_selected_items(items);
        return items;
    }

    bool ParseHandleList(const json& handlesJson, metadb_handle_list& out) {
        if (!handlesJson.is_array()) return false;

        auto mdb = metadb::get();

        // subsong 在校验前已从 path 剥离，故一张 CUE 的 N 个 subsong 会对同一裸路径
        // 各校验一次。缓存结论（含拒绝）以消除该冗余；handle_create 仍需逐个执行。
        std::unordered_map<std::string, bool> pathVerdicts;

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
            auto verdict = pathVerdicts.find(path);
            if (verdict == pathVerdicts.end()) {
                std::wstring wpath = pfc::stringcvt::string_wide_from_utf8(path.c_str()).get_ptr();
                std::wstring pathError;
                // Argument order verified correct: wpath -> path (in), pathError -> errorMsg (out).
                // The name-similarity heuristic cross-matches the "path" prefix of pathError.
                // NOLINTNEXTLINE(readability-suspicious-call-argument)
                const bool allowed = PathSecurity::Instance().ValidateMediaAccess(wpath, pathError);
                verdict = pathVerdicts.emplace(path, allowed).first;
            }
            if (!verdict->second) {
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
        const json& params,
        unsigned flags = contextmenu_manager::flag_view_full) {
        ContextMenuInitResult result;
        result.effectiveMode = mode;

        metadb_handle_list handles;
        bool hasHandles = ParseHandleList(params.value("handles", json::array()), handles);

        if (mode == "handles") {
            if (!hasHandles) {
                result.error = "handles required for mode=handles";
                return result;
            }
            mgr->init_context(handles, flags);
            result.inited = true;
        } else if (mode == "playlist") {
            mgr->init_context_playlist(flags);
            result.inited = true;
        } else if (mode == "nowPlaying") {
            result.inited = mgr->init_context_now_playing(flags);
            if (!result.inited) {
                result.error = "No now playing item";
            }
        } else if (mode == "selection") {
            metadb_handle_list selection = GetSelectedContextItems();
            if (selection.get_count() == 0) {
                result.error = "No playlist items selected";
                return result;
            }
            mgr->init_context(selection, flags);
            result.inited = true;
        } else {
            // auto mode: handles → nowPlaying → selection → playlist
            if (hasHandles) {
                mgr->init_context(handles, flags);
                result.inited = true;
                result.effectiveMode = "handles";
            } else if (mgr->init_context_now_playing(flags)) {
                result.inited = true;
                result.effectiveMode = "nowPlaying";
            } else {
                metadb_handle_list selection = GetSelectedContextItems();
                if (selection.get_count() > 0) {
                    mgr->init_context(selection, flags);
                    result.inited = true;
                    result.effectiveMode = "selection";
                } else {
                    mgr->init_context_playlist(flags);
                    result.inited = true;
                    result.effectiveMode = "playlist";
                }
            }
        }
        return result;
    }

    // Shared by the main-menu v2 tier and the context-menu tier, hence the
    // explicit `source`: the two produce structurally identical nodes but must
    // not claim the same origin in the response.
    json BuildMenuTreeJson(
        const menu_tree_item::ptr& node,
        const std::string& pathPrefix,
        const std::string& displayPathPrefix,
        const std::string& locale,
        bool enableI18n,
        bool withAvailability,
        menu_node::Source source
    ) {
        if (!node.is_valid()) return json();

        if (node->isSeparator()) {
            return { { "type", "separator" } };
        }

        // menu_tree_item::name() may return truncated/invalid UTF-8 from plugins
        // or localized SDK builds; nlohmann::json dump throws type_error.316 otherwise.
        const char* namePtr = node->name();
        std::string label = StringUtils::SafeUtf8(namePtr);
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
                    auto item = BuildMenuTreeJson(child, path, displayPath, locale, enableI18n, withAvailability, source);
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
            // menu_flags (menu_common.h) uses the same bit assignments as
            // mainmenu_commands' flags, so one normalizer covers both. The v2
            // tier previously reported `available` + `flags` while the v1 tier
            // reported `available` + `checked`, so a caller had to know which
            // tier answered before it could read state at all.
            std::uint32_t rawFlags = 0;
            try {
                rawFlags = static_cast<std::uint32_t>(node->flags());
            } catch (...) {
                // Treat a throwing node as unflagged.
            }
            // menu_tree_item exposes no get_display() bool, so there is no
            // "return false to hide" signal here; defaulthidden is the only cue.
            const menu_node::State state =
                menu_node::NormalizeMainMenu(rawFlags, /*displayReturnedTrue=*/true);

            json item = {
                { "type", "command" },
                { "label", label },
                { "displayLabel", displayLabel },
                { "path", path },
                { "displayPath", displayPath },
                { "flags", rawFlags },
                { "enabled", state.enabled },
                { "checked", state.checked },
                { "radioChecked", state.radioChecked },
                { "hidden", state.hidden },
                { "source", menu_node::ToString(source) }
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
            // A throwing commandGuid() is why a v2 leaf can come back with no
            // address at all. Swallowing it silently is what made the whole
            // failure invisible: the node still looked like a normal command.
            bool haveGuid = false;
            try {
                GUID guid = node->commandGuid();
                if (guid != pfc::guid_null) {
                    item["guid"] = GuidToString(guid);
                    haveGuid = true;
                }
            } catch (const std::exception& ex) {
                console::printf("[MenuApi] BuildMenuTreeJson: commandGuid() failed for '%s': %s",
                                label.c_str(), ex.what());
            } catch (...) {
                console::printf("[MenuApi] BuildMenuTreeJson: commandGuid() failed for '%s' (unknown)",
                                label.c_str());
            }
            try {
                GUID subGuid = node->subCommandGuid();
                if (subGuid != pfc::guid_null) item["subGuid"] = GuidToString(subGuid);
            } catch (const std::exception& ex) {
                console::printf("[MenuApi] BuildMenuTreeJson: subCommandGuid() failed for '%s': %s",
                                label.c_str(), ex.what());
            } catch (...) {
                console::printf("[MenuApi] BuildMenuTreeJson: subCommandGuid() failed for '%s' (unknown)",
                                label.c_str());
            }

            // State a missing address explicitly rather than emitting a listed
            // entry the caller cannot act on and cannot explain.
            item["executable"] = haveGuid;
            if (!haveGuid) {
                item["unaddressableReason"] =
                    menu_node::ToString(menu_node::Unaddressable::NoStableIdentifier);
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
        // A dynamic child is addressed by owning GUID + subGuid; g_execute alone
        // cannot reach it.
        GUID subGuid = pfc::guid_null;
        std::string subGuidStr = params.value("subGuid", "");
        if (!subGuidStr.empty() && !StringToGuid(subGuidStr, subGuid)) {
            return { {"success", false}, {"error", "Invalid subGuid format"} };
        }
        const bool dynamic = !subGuidStr.empty();

        // The GUID form is state-checked exactly like the name and path forms.
        // Skipping it here is what let a disabled command ("Undo" with nothing to
        // undo) report success when addressed by GUID while the very same command
        // was correctly refused when addressed by name.
        //
        // A GUID absent from the index is NOT refused: the caller may hold a
        // valid address this enumeration did not surface, and refusing it would
        // turn a working call into a failure.
        const auto index = BuildMainMenuIndex();
        const MainMenuIndexEntry* known = FindIndexEntryByAddress(index, guid, subGuid);
        if (known && !known->state.enabled) {
            return {
                {"success", false},
                {"error", "Command is currently disabled: " + command},
                {"code", "MENU_ITEM_DISABLED"},
                {"guid", command}
            };
        }

        const bool ok = dynamic
            ? mainmenu_commands::g_execute_dynamic(guid, subGuid)
            : mainmenu_commands::g_execute(guid);

        // Both arms are spelled as braced literals on purpose: the response
        // schema extractor enumerates keys statically, so building one object and
        // conditionally inserting a key downgrades this endpoint's inferred
        // response to a partial guess (SPEC §10.2 / §12).
        if (dynamic) {
            return {
                {"success", ok},
                {"guid", command},
                {"dynamic", true},
                {"subGuid", subGuidStr}
            };
        }
        return { {"success", ok}, {"guid", command}, {"dynamic", false} };
    }

    // Path/name form.
    //
    // The v2 menu tree is tried first because it is the only tier that carries
    // full submenu paths. It is wrapped because generate_menu() throws on
    // localized hosts ("找不到命令"); before this guard the exception escaped the
    // handler, surfaced to JS as a raw host-language Error, and — worse — skipped
    // every fallback below, making name and path forms fail outright.
    try {
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
                    // Refuse a disabled command instead of reporting a success
                    // the user would never observe.
                    if (!IsMenuItemAvailable(item)) {
                        return {
                            {"success", false},
                            {"error", "Command is currently disabled: " + command},
                            {"code", "MENU_ITEM_DISABLED"}
                        };
                    }
                    item->execute(service_ptr_t<service_base>());
                    return { {"success", true}, {"source", "v2-tree"} };
                }
            }
        }
    } catch (const std::exception& ex) {
        console::printf("[MenuApi] runMainMenuCommand: v2 tree failed (%s), trying index...",
                        ex.what());
    } catch (...) {
        console::printf("[MenuApi] runMainMenuCommand: v2 tree failed (unknown), trying index...");
    }

    // Fallback: resolve through the service-enumeration index, which needs no v2
    // tree and therefore still works on hosts where the above throws. This
    // replaces a mainmenu_commands::g_find_by_name() call that could only ever
    // match an exact leaf name via stricmp_utf8 — never a path, and never the
    // displayed text.
    const auto index = BuildMainMenuIndex();
    const auto matches = FindIndexCandidates(index, command);
    const menu_node::MatchKind matchKind = menu_node::ClassifyMatch(matches.size());

    if (matchKind == menu_node::MatchKind::Ambiguous) {
        json candidates = json::array();
        for (const auto* entry : matches) {
            candidates.push_back({
                {"name", entry->name},
                {"displayLabel", entry->displayText},
                {"guid", GuidToString(entry->guid)}
            });
        }
        return {
            {"success", false},
            {"error", "Command is ambiguous; address it by GUID instead: " + command},
            {"code", "MENU_MATCH_AMBIGUOUS"},
            {"match", menu_node::ToString(matchKind)},
            {"candidateCount", matches.size()},
            {"candidates", candidates}
        };
    }

    if (matchKind == menu_node::MatchKind::Unique) {
        const MainMenuIndexEntry* entry = matches.front();
        if (!entry->state.enabled) {
            return {
                {"success", false},
                {"error", "Command is currently disabled: " + command},
                {"code", "MENU_ITEM_DISABLED"}
            };
        }

        const bool dynamic = entry->subGuid != pfc::guid_null;
        const bool ok = dynamic
            ? mainmenu_commands::g_execute_dynamic(entry->guid, entry->subGuid)
            : mainmenu_commands::g_execute(entry->guid);

        if (dynamic) {
            return {
                {"success", ok},
                {"guid", GuidToString(entry->guid)},
                {"dynamic", true},
                {"subGuid", GuidToString(entry->subGuid)},
                {"source", "index"}
            };
        }
        return {
            {"success", ok},
            {"guid", GuidToString(entry->guid)},
            {"dynamic", false},
            {"source", "index"}
        };
    }

    return {
        {"success", false},
        {"error", "Command not found: " + command},
        {"code", "MENU_COMMAND_NOT_FOUND"},
        {"match", menu_node::ToString(matchKind)},
        {"candidateCount", 0}
    };
}


json MenuRunContextCommand(const json& params) {
    std::string command = params.value("command", "");
    if (command.empty()) {
        return { {"success", false}, {"error", "command is required"} };
    }

    // A dynamic context child is addressed by the owning command GUID plus its
    // own node GUID. Passing guid_null unconditionally, as this handler used
    // to, reaches the owner instead — for a container that is a silent no-op
    // reported as success.
    GUID subGuid = pfc::guid_null;
    const std::string subGuidStr = params.value("subGuid", "");
    if (!subGuidStr.empty() && !StringToGuid(subGuidStr, subGuid)) {
        return { {"success", false}, {"error", "Invalid subGuid format"} };
    }

    GUID caller = contextmenu_item::caller_active_playlist_selection;
    metadb_handle_list items = GetDefaultContextItems(&caller);
    if (items.get_count() == 0) {
        return { {"success", false}, {"error", "No track selected or playing"} };
    }

    GUID guid;
    if (StringToGuid(command, guid)) {
        bool ok = menu_helpers::run_command_context_ex(guid, subGuid, items, caller);
        return {
            {"success", ok},
            {"guid", command},
            {"itemCount", items.get_count()},
            {"executionConfirmed", true}
        };
    }

    service_ptr_t<contextmenu_item> item;
    unsigned index = 0;
    if (menu_helpers::find_command_by_name(command.c_str(), item, index)) {
        // Resolve to a GUID and dispatch through the bool-returning entry
        // point. item_execute_simple() returns void, so a caller could not tell
        // a completed command from a no-op — which is why this branch reported
        // a hardcoded success regardless of what actually happened.
        GUID itemGuid = pfc::guid_null;
        try {
            itemGuid = item->get_item_guid(index);
        } catch (...) {
            // Leave null; the unconfirmed path below still dispatches.
        }

        if (itemGuid != pfc::guid_null) {
            bool ok = menu_helpers::run_command_context_ex(itemGuid, subGuid, items, caller);
            return {
                {"success", ok},
                {"guid", GuidToString(itemGuid)},
                {"itemCount", items.get_count()},
                {"executionConfirmed", true}
            };
        }

        // Degenerate registration: no stable GUID, so the void entry point is
        // the only way in and the SDK reports nothing back. `success` stays
        // true because the command was dispatched, but `executionConfirmed`
        // marks the difference between "ran" and "was handed to the host".
        item->item_execute_simple(index, subGuid, items, caller);
        return {
            {"success", true},
            {"itemCount", items.get_count()},
            {"executionConfirmed", false}
        };
    }

    if (menu_helpers::guid_from_name(command.c_str(), (unsigned)command.size(), guid)) {
        bool ok = menu_helpers::run_command_context_ex(guid, subGuid, items, caller);
        return {
            {"success", ok},
            {"guid", GuidToString(guid)},
            {"itemCount", items.get_count()},
            {"executionConfirmed", true}
        };
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
            baseLabel = StringUtils::SafeUtf8(label ? label : rootName.c_str());
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
                                          withAvailability,
                                          menu_node::Source::MainMenuStatic);
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
                                            BuildMainMenuFlatFallback(locale, enableI18n,
                                                                      BuildMainMenuIndex()));
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
        auto item = BuildMenuTreeJson(child, "", "", locale, enableI18n, withAvailability,
                                      menu_node::Source::ContextMenuStatic);
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
    std::string mode = params.value("mode", "auto");
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
    
    auto initResult = InitContextMenu(mgr, mode, params, flags);
    if (!initResult.inited) {
        return {{"success", false},
                {"error", initResult.error.empty()
                    ? "Failed to init context for mode: " + mode
                    : initResult.error}};
    }

    console::printf("[MenuApi] showNativePopup: requestedMode=%s effectiveMode=%s",
                    mode.c_str(), initResult.effectiveMode.c_str());
    
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
    // Resource preflight before opening the overlay (DESIGN 8.5): strip single
    // SVGs over 32 KiB, then fail the whole call on item/depth/segment/svgTotal
    // breaches. Do not open the overlay on failure.
    menu_limits::StripOversizedSvgInJsonItems(items);
    auto breach = menu_limits::ValidateShowMenuResources(items);
    if (!breach.ok) {
        return ApiEnvelope::MakeError("menu resource limit exceeded",
                                      ApiErrorCode::INVALID_PARAMS,
                                      menu_limits::DetailsJson(breach));
    }
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

// ---- Internal overlay IPC (menu.__*) --------------------------------------
// Every internal handler validates that the caller is the current overlay
// window; select/dismiss/ready/valueChanged additionally validate menuId. The
// validation LOGIC lives in MenuOverlayHost's narrow interface (not copied per
// handler). An invalid caller / menuId returns an INVALID_PARAMS envelope and
// must NOT change any menu state (no Hide, no action, no event, no measure
// consumption). DESIGN 8.1 / 8.2.
static HWND MenuCallerHwnd(const json& params) {
    if (params.contains("_callerHwnd") && params["_callerHwnd"].is_number_integer()) {
        return reinterpret_cast<HWND>(params["_callerHwnd"].get<intptr_t>());
    }
    return nullptr;
}

// menu.__getMenuState: 前端 pull 当前菜单状态(内部)。仅当前 overlay 可调。
json MenuGetMenuState(const json& params) {
    auto& host = MenuOverlayHost::GetInstance();
    const HWND caller = MenuCallerHwnd(params);
    if (!host.IsOverlayCaller(caller)) {
        return ApiEnvelope::MakeError("menu overlay caller required", ApiErrorCode::INVALID_PARAMS);
    }
    return host.GetMenuStateJson(caller);
}

// menu.__select {menuId,token}: 前端点击菜单项回报(内部) -> 校验 caller+menuId
// -> 解析 opaque token -> menu:select + 关闭。
json MenuSelect(const json& params) {
    auto& host = MenuOverlayHost::GetInstance();
    if (!host.IsOverlayCaller(MenuCallerHwnd(params))) {
        return ApiEnvelope::MakeError("menu overlay caller required", ApiErrorCode::INVALID_PARAMS);
    }
    if (!host.ValidateMenuId(params.value("menuId", std::string()))) {
        return ApiEnvelope::MakeError("menu id mismatch", ApiErrorCode::INVALID_PARAMS);
    }
    host.OnSelect(params.value("token", std::string()));
    return {{"success", true}};
}

// menu.__dismiss {menuId,reason?}: 前端外点击/Esc 回报(内部) -> 校验 caller+menuId -> 关闭。
json MenuDismiss(const json& params) {
    auto& host = MenuOverlayHost::GetInstance();
    const HWND caller = MenuCallerHwnd(params);
    if (!host.IsOverlayCaller(caller)) {
        return ApiEnvelope::MakeError("menu overlay caller required", ApiErrorCode::INVALID_PARAMS);
    }
    if (!host.ValidateMenuId(params.value("menuId", std::string()))) {
        return ApiEnvelope::MakeError("menu id mismatch", ApiErrorCode::INVALID_PARAMS);
    }
    host.OnDismissRequested(caller, params.value("reason", std::string("api")));
    return {{"success", true}};
}

// menu.__ready {menuId,root:{w,h},submenu:{maxW,maxH}}: ContentSized renderer
// reports physical-pixel root and first-level submenu maxima. The host derives
// whether a submenu exists from the current normalized model; renderer booleans
// are ignored. Invalid reports do not consume the measure gate or timer.
json MenuReady(const json& params) {
    auto& host = MenuOverlayHost::GetInstance();
    if (!host.IsOverlayCaller(MenuCallerHwnd(params))) {
        return ApiEnvelope::MakeError("menu overlay caller required", ApiErrorCode::INVALID_PARAMS);
    }
    if (!host.ValidateMenuId(params.value("menuId", std::string()))) {
        return ApiEnvelope::MakeError("menu id mismatch", ApiErrorCode::INVALID_PARAMS);
    }
    if (!params.contains("root") || !params["root"].is_object() ||
        !params.contains("submenu") || !params["submenu"].is_object()) {
        return ApiEnvelope::MakeError("invalid measure report", ApiErrorCode::INVALID_PARAMS);
    }
    const auto& root = params["root"];
    const auto& submenu = params["submenu"];
    if (!root.contains("w") || !root["w"].is_number_integer() ||
        !root.contains("h") || !root["h"].is_number_integer() ||
        !submenu.contains("maxW") || !submenu["maxW"].is_number_integer() ||
        !submenu.contains("maxH") || !submenu["maxH"].is_number_integer()) {
        return ApiEnvelope::MakeError("invalid measure report", ApiErrorCode::INVALID_PARAMS);
    }
    menu_overlay_geometry::MeasureReport report;
    report.root.w = root["w"].get<long long>();
    report.root.h = root["h"].get<long long>();
    report.submenu.w = submenu["maxW"].get<long long>();
    report.submenu.h = submenu["maxH"].get<long long>();
    if (!menu_overlay_geometry::IsValidMeasureReport(report, host.HasFirstLevelSubmenu()) ||
        !host.OnContentMeasured(report)) {
        return ApiEnvelope::MakeError("invalid measure report", ApiErrorCode::INVALID_PARAMS);
    }
    return {{"success", true}};
}

// Overlay-private internal IPC; the .__ namespace is recorded by Graph but
// excluded from public SDK wrappers/codegen. Opens or closes the independent,
// tight submenu HWND; no SetWindowRgn-based backdrop cropping is relied upon.
json MenuSubmenuPanel(const json& params) {
    auto& host = MenuOverlayHost::GetInstance();
    const HWND caller = MenuCallerHwnd(params);
    if (!host.IsOverlayCaller(caller)) {
        return ApiEnvelope::MakeError("menu overlay caller required", ApiErrorCode::INVALID_PARAMS);
    }
    if (!host.ValidateMenuId(params.value("menuId", std::string()))) {
        return ApiEnvelope::MakeError("menu id mismatch", ApiErrorCode::INVALID_PARAMS);
    }
    // The independently hosted submenu reuses this private endpoint to
    // acknowledge that its hidden DOM has pulled and rendered the child items.
    // No public API registration is added for this overlay-only handshake.
    if (params.value("ready", false)) {
        if (!params.contains("parentToken") || !params["parentToken"].is_string() ||
            !host.OnSubmenuSurfaceReady(caller, params["parentToken"].get<std::string>())) {
            return ApiEnvelope::MakeError("submenu surface not ready", ApiErrorCode::INVALID_PARAMS);
        }
        return {{"success", true}};
    }
    if (!host.IsRootOverlayCaller(caller)) {
        return ApiEnvelope::MakeError("root menu overlay caller required", ApiErrorCode::INVALID_PARAMS);
    }
    if (!params.contains("sequence") || !params["sequence"].is_number_unsigned()) {
        return ApiEnvelope::MakeError("invalid submenu panel sequence", ApiErrorCode::INVALID_PARAMS);
    }
    const auto sequence = params["sequence"].get<std::uint64_t>();
    const bool visible = params.value("visible", false);
    if (!visible) {
        if (!host.OnSubmenuPanelChanged({false, 0, 0, 0, 0, sequence}, {})) {
            return ApiEnvelope::MakeError("submenu panel update rejected", ApiErrorCode::INVALID_PARAMS);
        }
        return {{"success", true}};
    }
    if (!params.contains("x") || !params["x"].is_number_integer() ||
        !params.contains("y") || !params["y"].is_number_integer() ||
        !params.contains("w") || !params["w"].is_number_integer() ||
        !params.contains("h") || !params["h"].is_number_integer() ||
        !params.contains("parentToken") || !params["parentToken"].is_string()) {
        return ApiEnvelope::MakeError("invalid submenu panel", ApiErrorCode::INVALID_PARAMS);
    }
    const auto x64 = params["x"].get<long long>();
    const auto y64 = params["y"].get<long long>();
    const auto w64 = params["w"].get<long long>();
    const auto h64 = params["h"].get<long long>();
    const menu_overlay_geometry::SubmenuPanelRequest request{
        true, x64, y64, w64, h64, sequence
    };
    if (!menu_overlay_geometry::IsValidSubmenuPanelCoordinates(request)) {
        return ApiEnvelope::MakeError("invalid submenu panel", ApiErrorCode::INVALID_PARAMS);
    }
    if (!host.OnSubmenuPanelChanged(request, params["parentToken"].get<std::string>())) {
        return ApiEnvelope::MakeError("submenu panel update rejected", ApiErrorCode::INVALID_PARAMS);
    }
    return {{"success", true}};
}

// menu.__valueChanged {menuId,token,value}: 富控件(rating/slider/segmented)值变更
// 回报(内部) -> 校验 caller+menuId -> 解析 opaque token + 按控件类型校验值 -> 经
// owner-mode value sink 回报，【不关闭菜单】。
json MenuValueChanged(const json& params) {
    auto& host = MenuOverlayHost::GetInstance();
    if (!host.IsOverlayCaller(MenuCallerHwnd(params))) {
        return ApiEnvelope::MakeError("menu overlay caller required", ApiErrorCode::INVALID_PARAMS);
    }
    if (!host.ValidateMenuId(params.value("menuId", std::string()))) {
        return ApiEnvelope::MakeError("menu id mismatch", ApiErrorCode::INVALID_PARAMS);
    }
    int value = params.value("value", 0);
    host.OnValueChanged(params.value("token", std::string()), value);
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
    bridge.RegisterApi("menu.__submenuPanel", MenuSubmenuPanel);
    bridge.RegisterApi("menu.__valueChanged", MenuValueChanged);
}
