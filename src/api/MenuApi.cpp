// MenuApi.cpp - Menu command helpers
// Provides execution of main menu and context menu commands by name/path

#include "pch.h"
#include "api/MenuApi.h"
#include "api/BridgeCore.h"
#include "api/ErrorEnvelope.h"
#include "api/TypedApi.h"
#include "api/generated/MenuSchema.h"
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
#include "domain/PathSecurity.h"
#include "utils/StringUtils.h"
#include "utils/SubsongUtils.h"
#include "window/MenuOverlayHost.h"
#include "window/MenuResourceLimits.h"

namespace {
    using json = nlohmann::json;
    namespace mnu = api::menu;
    using MenuNodes = std::vector<mnu::MenuTreeNode>;

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

    mnu::MenuTreeNode SeparatorNode() {
        mnu::MenuTreeNode node;
        node.type = "separator";
        return node;
    }

    // Shared by the v1 top-level menus and every v2 submenu.
    mnu::MenuAvailability AvailabilityOf(int total, int available) {
        mnu::MenuAvailability counts;
        counts.totalCommands = total;
        counts.availableCommands = available;
        counts.disabledCommands = total - available;
        counts.allAvailable = total > 0 ? (available == total) : true;
        return counts;
    }

    // Last-resort tier: a flat command list with no hierarchy at all.
    //
    // Built from the shared index so availability is READ from get_display()
    // rather than hardcoded. The previous version claimed `available: true` for
    // every entry, which meant the one tier that always produces a usable GUID
    // also reported state that was outright fabricated — disabled commands were
    // indistinguishable from enabled ones.
    MenuNodes BuildMainMenuFlatFallback(const std::string& locale, bool enableI18n,
                                        const std::vector<MainMenuIndexEntry>& index) {
        MenuNodes items;

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

            mnu::MenuTreeNode item;
            item.type = "command";
            item.label = label;
            item.displayLabel = displayLabel;
            item.path = path;
            item.displayPath = path;
            item.guid = GuidToString(entry.guid);
            item.available = entry.state.enabled;
            item.enabled = entry.state.enabled;
            item.checked = entry.state.checked;
            item.radioChecked = entry.state.radioChecked;
            item.hidden = entry.state.hidden;
            item.flags = entry.state.flags;
            item.source = menu_node::ToString(entry.subGuid != pfc::guid_null
                                                  ? menu_node::Source::MainMenuDynamic
                                                  : menu_node::Source::MainMenuStatic);
            item.executable = true;
            item.fallback = true;
            if (entry.subGuid != pfc::guid_null) {
                item.subGuid = GuidToString(entry.subGuid);
            }
            items.push_back(std::move(item));
        }

        return items;
    }

    // ================================================================
    // V1 HMENU-based tree builder
    // 使用 mainmenu_manager v1 API (instantiate + generate_menu_win32)
    // 遍历 Win32 HMENU 构建层级菜单树
    // 兼容所有 foobar2000 版本（含中文汉化版）
    // ================================================================

    MenuNodes WalkHMenu(HMENU hmenu, const std::string& pathPrefix, const std::string& displayPathPrefix,
                        const std::string& locale, bool enableI18n,
                        const std::vector<MainMenuIndexEntry>* index) {
        MenuNodes items;
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
                    items.push_back(SeparatorNode());
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
                    mnu::MenuTreeNode submenu;
                    submenu.type = "submenu";
                    submenu.label = cleanLabel;
                    submenu.displayLabel = displayLabel;
                    submenu.path = path;
                    submenu.displayPath = displayPath;
                    submenu.children = WalkHMenu(mii.hSubMenu, path, displayPath, locale, enableI18n, index);
                    items.push_back(std::move(submenu));
                } else {
                    const menu_node::State state =
                        menu_node::NormalizeHmenu(static_cast<std::uint32_t>(mii.fState));

                    mnu::MenuTreeNode item;
                    item.type = "command";
                    item.label = cleanLabel;
                    item.displayLabel = displayLabel;
                    item.path = path;
                    item.displayPath = displayPath;
                    item.available = state.enabled;
                    item.enabled = state.enabled;
                    item.checked = state.checked;
                    item.radioChecked = state.radioChecked;
                    item.hidden = state.hidden;
                    item.flags = state.flags;
                    item.source = menu_node::ToString(menu_node::Source::HmenuFallback);
                    item.commandId = static_cast<int>(mii.wID);

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
                        item.guid = GuidToString(hit->guid);
                        if (hit->subGuid != pfc::guid_null) {
                            item.subGuid = GuidToString(hit->subGuid);
                        }
                        item.executable = true;
                    } else {
                        item.executable = false;
                        item.unaddressableReason =
                            menu_node::ToString(menu_node::Unaddressable::NoStableIdentifier);
                    }
                    items.push_back(std::move(item));
                }
            } catch (...) {
                // 跳过有问题的菜单项
            }
        }

        return items;
    }

    MenuNodes BuildMainMenuV1Tree(const std::string& locale, bool enableI18n, bool withAvailability) {
        struct TopMenu {
            GUID guid;
            const char* enName;
        };

        auto countAvailableCommands = [](const MenuNodes& items,
                                         int& total,
                                         int& available,
                                         const auto& self) -> void {
            for (const auto& item : items) {
                if (item.type == "command") {
                    total++;
                    if (item.available.value_or(true)) {
                        available++;
                    }
                    continue;
                }

                if (item.type == "submenu" && item.children) {
                    self(*item.children, total, available, self);
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

        MenuNodes items;

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
                MenuNodes children = WalkHMenu(hmenu, label, displayLabel, locale, enableI18n, &index);

                DestroyMenu(hmenu);

                if (children.empty()) continue;

                mnu::MenuTreeNode submenu;
                submenu.type = "submenu";
                submenu.label = label;
                submenu.displayLabel = displayLabel;
                submenu.path = label;
                submenu.displayPath = displayLabel;

                if (withAvailability) {
                    int total = 0, available = 0;
                    countAvailableCommands(children, total, available, countAvailableCommands);
                    submenu.availability = AvailabilityOf(total, available);
                }
                submenu.children = std::move(children);

                items.push_back(std::move(submenu));
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

    bool ParseHandleList(const std::vector<json>& handles, metadb_handle_list& out) {

        // subsong 在校验前已从 path 剥离，故一张 CUE 的 N 个 subsong 会对同一裸路径
        // 各校验一次。缓存结论（含拒绝）以消除该冗余；handle 仍需逐个创建。
        std::unordered_map<std::string, bool> pathVerdicts;

        for (const auto& h : handles) {
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

            // 规范化后建 handle：菜单命令拿到的 handle 必须与播放列表 / 媒体库里的
            // 同一曲目是同一 metadb 身份，否则"已在列表中"类判定失效。
            metadb_handle_ptr handle = SubsongUtils::CreateCanonicalHandle(path, subsong);
            if (handle.is_valid()) {
                out.add_item(handle);
            }
        }

        return out.get_count() > 0;
    }

    // 上下文菜单初始化 — getContextMenu / runContextCommandById / showNativePopup 共享逻辑。
    // 成功时返回实际使用的模式：auto 会落到其余某一个。
    static std::variant<std::string, api::Failure> InitContextMenu(
        service_ptr_t<contextmenu_manager>& mgr,
        const std::string& mode,
        const std::optional<std::vector<json>>& handlesParam,
        unsigned flags = contextmenu_manager::flag_view_full) {
        metadb_handle_list handles;
        const bool hasHandles = handlesParam && ParseHandleList(*handlesParam, handles);

        if (mode == "handles") {
            if (!hasHandles) {
                return api::Fail("handles required for mode=handles", ApiErrorCode::INVALID_PARAMS);
            }
            mgr->init_context(handles, flags);
            return mode;
        }
        if (mode == "playlist") {
            mgr->init_context_playlist(flags);
            return mode;
        }
        if (mode == "nowPlaying") {
            if (!mgr->init_context_now_playing(flags)) {
                return api::Fail("No now playing item", ApiErrorCode::NO_ACTIVE_ITEM);
            }
            return mode;
        }
        if (mode == "selection") {
            metadb_handle_list selection = GetSelectedContextItems();
            if (selection.get_count() == 0) {
                return api::Fail("No playlist items selected", ApiErrorCode::NO_ACTIVE_ITEM);
            }
            mgr->init_context(selection, flags);
            return mode;
        }

        // auto mode (and any other value): handles → nowPlaying → selection → playlist
        if (hasHandles) {
            mgr->init_context(handles, flags);
            return std::string("handles");
        }
        // 给了 handles 却一条都用不了（路径被拒、形状不对、建不出 handle）时不往下回退：
        // 回退会把命令落到页面没点名的曲目上，删除类命令就删错了东西。
        if (handlesParam && !handlesParam->empty()) {
            return api::Fail("none of the given handles is usable", ApiErrorCode::INVALID_PARAMS);
        }
        if (mgr->init_context_now_playing(flags)) {
            return std::string("nowPlaying");
        }
        metadb_handle_list selection = GetSelectedContextItems();
        if (selection.get_count() > 0) {
            mgr->init_context(selection, flags);
            return std::string("selection");
        }
        mgr->init_context_playlist(flags);
        return std::string("playlist");
    }

    // 主菜单 v2 与上下文菜单共用。`source` 是本次遍历所属族（调用方都传静态值）；
    // 命令叶节点的 `source` 要等拿到子命令 GUID 后再解析，避免把动态子项标成静态槽。
    std::optional<mnu::MenuTreeNode> BuildMenuTreeNode(
        const menu_tree_item::ptr& node,
        const std::string& pathPrefix,
        const std::string& displayPathPrefix,
        const std::string& locale,
        bool enableI18n,
        bool withAvailability,
        menu_node::Source source
    ) {
        if (!node.is_valid()) return std::nullopt;

        if (node->isSeparator()) {
            return SeparatorNode();
        }

        // menu_tree_item::name() may return truncated/invalid UTF-8 from plugins
        // or localized SDK builds; nlohmann::json dump throws type_error.316 otherwise.
        const char* namePtr = node->name();
        std::string label = StringUtils::SafeUtf8(namePtr);
        std::string displayLabel = TranslateMenuLabel(label, locale, enableI18n);
        std::string path = pathPrefix.empty() ? label : (pathPrefix + "/" + label);
        std::string displayPath = displayPathPrefix.empty() ? displayLabel : (displayPathPrefix + "/" + displayLabel);

        if (node->isSubmenu()) {
            MenuNodes children;
            const size_t count = node->childCount();
            for (size_t i = 0; i < count; i++) {
                try {
                    auto child = node->childAt(i);
                    if (!child.is_valid()) continue;
                    auto item = BuildMenuTreeNode(child, path, displayPath, locale, enableI18n, withAvailability, source);
                    if (item) children.push_back(std::move(*item));
                } catch (const std::exception& ex) {
                    // 跳过有问题的子项（中文版 SDK 可能对某些项抛异常）
                    console::printf("[MenuApi] BuildMenuTreeNode: skipping submenu child %u: %s", (unsigned)i, ex.what());
                } catch (...) {
                    // Non-std exception — skip this child silently
                }
            }

            mnu::MenuTreeNode result;
            result.type = "submenu";
            result.label = label;
            result.displayLabel = displayLabel;
            result.path = path;
            result.displayPath = displayPath;
            result.flags = node->flags();
            result.children = std::move(children);

            if (withAvailability) {
                int total = 0;
                int available = 0;
                try {
                    CountCommandAvailability(node, total, available);
                } catch (...) {
                    // Silently ignore — totals stay 0
                }
                result.availability = AvailabilityOf(total, available);
            }

            return result;
        }

        if (node->isCommand()) {
            // menu_flags (menu_common.h) uses the same bit assignments as
            // mainmenu_commands' flags, so one normalizer covers both and the v1 and
            // v2 tiers report state in the same shape; a caller does not need to know
            // which tier answered.
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

            mnu::MenuTreeNode item;
            item.type = "command";
            item.label = label;
            item.displayLabel = displayLabel;
            item.path = path;
            item.displayPath = displayPath;
            item.flags = rawFlags;
            item.enabled = state.enabled;
            item.checked = state.checked;
            item.radioChecked = state.radioChecked;
            item.hidden = state.hidden;

            // commandID / commandGuid / subCommandGuid 在中文版 SDK 可能抛异常
            try {
                item.commandId = node->commandID();
            } catch (...) {
                item.commandId = 0;
            }
            try {
                item.available = IsMenuItemAvailable(node);
            } catch (...) {
                item.available = true;
            }
            // A throwing commandGuid() is why a v2 leaf can come back with no
            // address at all. Swallowing it silently is what made the whole
            // failure invisible: the node still looked like a normal command.
            bool haveGuid = false;
            try {
                GUID guid = node->commandGuid();
                if (guid != pfc::guid_null) {
                    item.guid = GuidToString(guid);
                    haveGuid = true;
                }
            } catch (const std::exception& ex) {
                console::printf("[MenuApi] BuildMenuTreeNode: commandGuid() failed for '%s': %s",
                                label.c_str(), ex.what());
            } catch (...) {
                console::printf("[MenuApi] BuildMenuTreeNode: commandGuid() failed for '%s' (unknown)",
                                label.c_str());
            }
            bool haveSubGuid = false;
            try {
                GUID subGuid = node->subCommandGuid();
                if (subGuid != pfc::guid_null) {
                    item.subGuid = GuidToString(subGuid);
                    haveSubGuid = true;
                }
            } catch (const std::exception& ex) {
                console::printf("[MenuApi] BuildMenuTreeNode: subCommandGuid() failed for '%s': %s",
                                label.c_str(), ex.what());
            } catch (...) {
                console::printf("[MenuApi] BuildMenuTreeNode: subCommandGuid() failed for '%s' (unknown)",
                                label.c_str());
            }

            // 调用方传入的是族，不是叶节点来源；有子命令 GUID 才标成动态。
            item.source = menu_node::ToString(
                menu_node::ResolveLeafSource(source, haveSubGuid));

            // State a missing address explicitly rather than emitting a listed
            // entry the caller cannot act on and cannot explain.
            item.executable = haveGuid;
            if (!haveGuid) {
                item.unaddressableReason =
                    menu_node::ToString(menu_node::Unaddressable::NoStableIdentifier);
            }

            return item;
        }

        return std::nullopt;
    }
}


// ==========================================================================
// Menu API handler functions
// ==========================================================================
namespace {


api::Result<mnu::RunMainMenuCommandResult> MenuRunMainMenuCommand(const mnu::RunMainMenuCommandParams& p) {
    const std::string& command = p.command;

    // GUID form
    GUID guid;
    if (StringToGuid(command, guid)) {
        // A dynamic child is addressed by owning GUID + subGuid; g_execute alone
        // cannot reach it. An empty subGuid is the same as none.
        GUID subGuid = pfc::guid_null;
        const std::string subGuidStr = p.subGuid.value_or("");
        if (!subGuidStr.empty() && !StringToGuid(subGuidStr, subGuid)) {
            return api::Fail("Invalid subGuid format", ApiErrorCode::INVALID_PARAMS);
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
            return api::Fail("Command is currently disabled: " + command, ApiErrorCode::MENU_ITEM_DISABLED,
                             {{"guid", command}});
        }

        const bool ok = dynamic
            ? mainmenu_commands::g_execute_dynamic(guid, subGuid)
            : mainmenu_commands::g_execute(guid);
        if (!ok) {
            json::object_t extra{{"guid", command}, {"dynamic", dynamic}};
            if (dynamic) extra["subGuid"] = subGuidStr;
            return api::Fail(dynamic ? "No dynamic main-menu command owns this guid and subGuid"
                                     : "No main-menu command owns this guid",
                             ApiErrorCode::NOT_FOUND, std::move(extra));
        }

        mnu::RunMainMenuCommandResult result;
        result.guid = command;
        result.dynamic = dynamic;
        if (dynamic) result.subGuid = subGuidStr;
        return result;
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
                        return api::Fail("Command is currently disabled: " + command, ApiErrorCode::MENU_ITEM_DISABLED);
                    }
                    item->execute(service_ptr_t<service_base>());
                    mnu::RunMainMenuCommandResult result;
                    result.source = "v2-tree";
                    return result;
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
        return api::Fail("Command is ambiguous; address it by GUID instead: " + command, ApiErrorCode::MENU_MATCH_AMBIGUOUS,
                         {{"match", menu_node::ToString(matchKind)},
                          {"candidateCount", matches.size()},
                          {"candidates", std::move(candidates)}});
    }

    if (matchKind == menu_node::MatchKind::Unique) {
        const MainMenuIndexEntry* entry = matches.front();
        if (!entry->state.enabled) {
            return api::Fail("Command is currently disabled: " + command, ApiErrorCode::MENU_ITEM_DISABLED);
        }

        const bool dynamic = entry->subGuid != pfc::guid_null;
        const bool ok = dynamic
            ? mainmenu_commands::g_execute_dynamic(entry->guid, entry->subGuid)
            : mainmenu_commands::g_execute(entry->guid);

        mnu::RunMainMenuCommandResult result;
        result.guid = GuidToString(entry->guid);
        result.dynamic = dynamic;
        if (dynamic) result.subGuid = GuidToString(entry->subGuid);
        result.source = "index";
        if (!ok) {
            json::object_t extra{{"guid", *result.guid}, {"dynamic", dynamic}, {"source", "index"}};
            if (dynamic) extra["subGuid"] = *result.subGuid;
            return api::Fail("No main-menu command owns this guid", ApiErrorCode::NOT_FOUND, std::move(extra));
        }
        return result;
    }

    return api::Fail("Command not found: " + command, ApiErrorCode::MENU_COMMAND_NOT_FOUND,
                     {{"match", menu_node::ToString(matchKind)}, {"candidateCount", 0}});
}


// run_command_context_ex fails only when no context-menu command owns the GUID;
// the caller has already made sure there are tracks. `guidText` is echoed back
// as the caller spelled it for the GUID form.
api::Result<mnu::RunContextCommandResult> RunContextCommandByGuid(const GUID& commandGuid,
                                                                  const std::string& guidText,
                                                                  const GUID& subGuid,
                                                                  const metadb_handle_list& items,
                                                                  const GUID& caller) {
    const auto itemCount = static_cast<std::int64_t>(items.get_count());
    if (!menu_helpers::run_command_context_ex(commandGuid, subGuid, items, caller)) {
        return api::Fail("No context-menu command owns this guid", ApiErrorCode::NOT_FOUND,
                         {{"guid", guidText}, {"itemCount", itemCount}, {"executionConfirmed", true}});
    }
    mnu::RunContextCommandResult result;
    result.guid = guidText;
    result.itemCount = itemCount;
    result.executionConfirmed = true;
    return result;
}

api::Result<mnu::RunContextCommandResult> MenuRunContextCommand(const mnu::RunContextCommandParams& p) {
    const std::string& command = p.command;

    // A dynamic context child is addressed by the owning command GUID plus its
    // own node GUID. Passing guid_null unconditionally, as this handler used
    // to, reaches the owner instead — for a container that is a silent no-op
    // reported as success.
    GUID subGuid = pfc::guid_null;
    const std::string subGuidStr = p.subGuid.value_or("");
    if (!subGuidStr.empty() && !StringToGuid(subGuidStr, subGuid)) {
        return api::Fail("Invalid subGuid format", ApiErrorCode::INVALID_PARAMS);
    }

    GUID caller = contextmenu_item::caller_active_playlist_selection;
    metadb_handle_list items = GetDefaultContextItems(&caller);
    if (items.get_count() == 0) {
        return api::Fail("No track selected or playing", ApiErrorCode::NO_ACTIVE_ITEM);
    }

    GUID guid;
    if (StringToGuid(command, guid)) {
        return RunContextCommandByGuid(guid, command, subGuid, items, caller);
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
            return RunContextCommandByGuid(itemGuid, GuidToString(itemGuid), subGuid, items, caller);
        }

        // Degenerate registration: no stable GUID, so the void entry point is
        // the only way in and the SDK reports nothing back. `success` stays
        // true because the command was dispatched, but `executionConfirmed`
        // marks the difference between "ran" and "was handed to the host".
        item->item_execute_simple(index, subGuid, items, caller);
        mnu::RunContextCommandResult result;
        result.itemCount = static_cast<std::int64_t>(items.get_count());
        result.executionConfirmed = false;
        return result;
    }

    if (menu_helpers::guid_from_name(command.c_str(), (unsigned)command.size(), guid)) {
        return RunContextCommandByGuid(guid, GuidToString(guid), subGuid, items, caller);
    }

    return api::Fail("Command not found", ApiErrorCode::MENU_COMMAND_NOT_FOUND);
}

static mnu::GetMainMenuResult BuildMainMenuResponse(const std::string& root,
                                                    const std::string& requestedRoot,
                                                    bool rootMatched,
                                                    const std::string& locale,
                                                    bool enableI18n,
                                                    bool withAvailability,
                                                    MenuNodes items,
                                                    const char* source = nullptr) {
    mnu::GetMainMenuResult result;
    result.root = root;
    result.requestedRoot = requestedRoot;
    result.rootMatched = rootMatched;
    result.locale = locale;
    result.i18n = enableI18n;
    result.withAvailability = withAvailability;
    result.items = std::move(items);

    if (source && *source) {
        result.source = source;
    }

    return result;
}

static std::optional<mnu::GetMainMenuResult> TryGetMainMenuFromV2(const std::string& rootName,
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

    MenuNodes items;
    for (size_t index = 0; index < base->childCount(); index++) {
        try {
            auto child = base->childAt(index);
            if (!child.is_valid()) {
                continue;
            }

            // 这里传族（静态）。动态判定在叶节点按 subGuid 解析，不改这个参数。
            auto item = BuildMenuTreeNode(child, baseLabel, baseLabel,
                                          locale, enableI18n,
                                          withAvailability,
                                          menu_node::Source::MainMenuStatic);
            if (item) {
                items.push_back(std::move(*item));
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
                                 locale, enableI18n, withAvailability, std::move(items));
}

static std::optional<mnu::GetMainMenuResult> TryGetMainMenuFromV1(const std::string& rootName,
                                                                  const std::string& locale,
                                                                  bool enableI18n,
                                                                  bool withAvailability) {
    MenuNodes v1Items = BuildMainMenuV1Tree(locale, enableI18n, withAvailability);
    if (v1Items.empty()) {
        return std::nullopt;
    }

    console::printf("[MenuApi] getMainMenu: v1 HMENU tree OK, %u top-level menus",
                    static_cast<unsigned>(v1Items.size()));

    if (rootName.empty()) {
        return BuildMainMenuResponse("", rootName, true, locale, enableI18n,
                                     withAvailability, std::move(v1Items), "v1-hmenu");
    }

    for (auto& topMenu : v1Items) {
        if (NamesMatchI18n(topMenu.label.value_or(""), rootName) ||
            NamesMatchI18n(topMenu.displayLabel.value_or(""), rootName)) {
            return BuildMainMenuResponse(topMenu.label.value_or(""), rootName,
                                         true, locale, enableI18n,
                                         withAvailability,
                                         std::move(topMenu.children).value_or(MenuNodes{}),
                                         "v1-hmenu");
        }
    }

    return BuildMainMenuResponse("", rootName, false, locale, enableI18n,
                                 withAvailability, std::move(v1Items), "v1-hmenu");
}


api::Result<mnu::GetMainMenuResult> MenuGetMainMenu(const mnu::GetMainMenuParams& p) {
    const std::string& rootName = p.root;
    const std::string& locale = p.locale;
    const bool enableI18n = p.i18n;
    const bool withAvailability = p.withAvailability;

    // ================================================================
    // 策略: v2 menu_tree → v1 HMENU → flat fallback
    // 中文汉化版 foobar2000 的 v2 generate_menu() 会抛 "找不到命令"，
    // 因此需要 v1 HMENU 作为可靠回退。
    // ================================================================

    try {
        auto v2Result = TryGetMainMenuFromV2(rootName, locale, enableI18n,
                                             withAvailability);
        if (v2Result) return std::move(*v2Result);
    } catch (const std::exception& ex) {
        console::printf("[MenuApi] getMainMenu: v2 failed (%s), trying v1 HMENU...", ex.what());
    } catch (...) {
        console::printf("[MenuApi] getMainMenu: v2 failed (unknown), trying v1 HMENU...");
    }

    // — v1 HMENU 方案（兼容中文版 + 1.x） —
    try {
        auto v1Result = TryGetMainMenuFromV1(rootName, locale, enableI18n,
                                             withAvailability);
        if (v1Result) return std::move(*v1Result);
    } catch (const std::exception& ex) {
        console::printf("[MenuApi] getMainMenu: v1 HMENU also failed: %s", ex.what());
    } catch (...) {
        console::printf("[MenuApi] getMainMenu: v1 HMENU failed (unknown)");
    }

    // — 最终回退: flat 命令列表 —
    console::printf("[MenuApi] getMainMenu: all tree methods failed, using flat fallback");
    try {
        mnu::GetMainMenuResult result = BuildMainMenuResponse("", rootName, false, locale,
                                                              enableI18n, withAvailability,
                                                              BuildMainMenuFlatFallback(locale, enableI18n,
                                                                                        BuildMainMenuIndex()));
        result.fallback = "flat-mainmenu-commands";
        return result;
    } catch (...) {
        return api::Fail("All menu tree methods failed", ApiErrorCode::OPERATION_FAILED,
                         {{"items", json::array()}});
    }
}


api::Result<mnu::GetContextMenuResult> MenuGetContextMenu(const mnu::GetContextMenuParams& p) {
    service_ptr_t<contextmenu_manager> mgr;
    contextmenu_manager::g_create(mgr);

    auto init = InitContextMenu(mgr, p.mode, p.handles);
    if (auto* failure = std::get_if<api::Failure>(&init)) {
        return std::move(*failure);
    }

    service_ptr_t<contextmenu_manager_v2> mgr2;
    if (!mgr->service_query_t(mgr2)) {
        return api::Fail("contextmenu_manager_v2 not available", ApiErrorCode::NOT_SUPPORTED);
    }

    auto root = mgr2->build_menu();
    if (!root.is_valid()) {
        return api::Fail("Failed to build context menu", ApiErrorCode::OPERATION_FAILED);
    }

    mnu::GetContextMenuResult result;
    const size_t count = root->childCount();
    for (size_t i = 0; i < count; i++) {
        auto child = root->childAt(i);
        if (!child.is_valid()) continue;
        // 这里传族（静态）。动态判定在叶节点按 subGuid 解析，不改这个参数。
        auto item = BuildMenuTreeNode(child, "", "", p.locale, p.i18n, p.withAvailability,
                                      menu_node::Source::ContextMenuStatic);
        if (item) result.items.push_back(std::move(*item));
    }

    result.mode = std::get<std::string>(init);
    result.locale = p.locale;
    result.i18n = p.i18n;
    result.withAvailability = p.withAvailability;
    return result;
}


api::Result<void> MenuRunContextCommandById(const mnu::RunContextCommandByIdParams& p) {
    service_ptr_t<contextmenu_manager> mgr;
    contextmenu_manager::g_create(mgr);

    auto init = InitContextMenu(mgr, p.mode, p.handles);
    if (auto* failure = std::get_if<api::Failure>(&init)) {
        return std::move(*failure);
    }

    // The declared range keeps the id within unsigned.
    if (!mgr->execute_by_id(static_cast<unsigned>(p.id))) {
        return api::Fail("No context menu item has this id", ApiErrorCode::NOT_FOUND, {{"id", p.id}});
    }
    return api::Ok();
}


api::Result<void> MenuShowNativePopup(const mnu::ShowNativePopupParams& p, const CallerContext& caller) {
    unsigned flags = contextmenu_manager::flag_show_shortcuts | contextmenu_manager::flag_view_full;

    // 获取面板 HWND 用于坐标转换
    HWND panelHwnd = (caller.callerHwnd && IsWindow(caller.callerHwnd)) ? caller.callerHwnd : nullptr;
    HWND parentHwnd = panelHwnd ? panelHwnd : core_api::get_main_window();
    if (!parentHwnd) {
        return api::Fail("No parent window", ApiErrorCode::OPERATION_FAILED);
    }

    // 直接使用系统光标位置（最可靠，不受 DPI/CSS 像素差异影响）；x / y 只为兼容而接受。
    POINT pt;
    GetCursorPos(&pt);

    // 创建并初始化 contextmenu_manager
    service_ptr_t<contextmenu_manager> mgr;
    contextmenu_manager::g_create(mgr);

    auto init = InitContextMenu(mgr, p.mode, p.handles, flags);
    if (auto* failure = std::get_if<api::Failure>(&init)) {
        return std::move(*failure);
    }

    console::printf("[MenuApi] showNativePopup: requestedMode=%s effectiveMode=%s",
                    p.mode.c_str(), std::get<std::string>(init).c_str());

    // 保存状态，通过 SetTimer 延迟执行 TrackPopupMenu
    // 让桥接回调先返回，WebView2 待处理消息先完成，然后再弹菜单
    auto& pending = GetPendingContextMenu();
    pending.mgr = mgr;
    pending.pt = pt;
    pending.parent = parentHwnd;

    SetTimer(parentHwnd, PendingContextMenu::TIMER_ID, 1,
        [](HWND hwnd, UINT, UINT_PTR id, DWORD) {
            KillTimer(hwnd, id);
            auto& queued = GetPendingContextMenu();
            if (queued.mgr.is_valid()) {
                HWND top = ::GetAncestor(hwnd, GA_ROOT);
                if (top) SetForegroundWindow(top);
                queued.mgr->win32_run_menu_popup(hwnd, &queued.pt);
                queued.mgr.release();
            }
        });

    return api::Ok();
}

// Screen coordinates arrive as 64-bit declared integers; the overlay takes int.
int ScreenCoordinate(const std::optional<std::int64_t>& value) {
    if (!value) return -1;
    return static_cast<int>(std::clamp<std::int64_t>(*value, std::numeric_limits<int>::min(),
                                                      std::numeric_limits<int>::max()));
}

// ---- Self-Drawn Menu APIs (自绘菜单引擎) ----
// menu.show {items, x?, y?, windowModel?, css?, cssReplace?, backdrop?,
//            backdropDarkMode?, closeAnimationMs?}:
// 在屏幕坐标(缺省取光标)显示自绘菜单，返回 menuId。全部可选参数缺省即现状行为。
api::Result<mnu::ShowResult> MenuShow(const mnu::ShowParams& p, const CallerContext& caller) {
    json items = p.items ? json(*p.items) : json::array();
    // Resource preflight before opening the overlay (DESIGN 8.5): strip single
    // SVGs over 32 KiB, then fail the whole call on item/depth/segment/svgTotal
    // breaches. Do not open the overlay on failure.
    menu_limits::StripOversizedSvgInJsonItems(items);
    auto breach = menu_limits::ValidateShowMenuResources(items);
    if (!breach.ok) {
        return api::Fail("menu resource limit exceeded", ApiErrorCode::INVALID_PARAMS,
                         {{"details", menu_limits::DetailsJson(breach)}});
    }

    // 引擎选项：没传的键保留默认值（默认值 = 现状行为）。
    MenuShowOptions opts{};
    if (p.windowModel) {
        // 只有精确 "contentSized" 才切换到内容尺寸窗；未知串回落全屏覆盖面。
        opts.windowModel = (*p.windowModel == "contentSized")
            ? MenuWindowModel::ContentSized : MenuWindowModel::FullscreenOverlay;
    }
    if (p.css) {
        opts.css = *p.css;
    }
    if (p.cssReplace) {
        opts.cssReplace = *p.cssReplace;
    }
    if (p.backdrop) {
        const std::string& b = *p.backdrop;
        if (b == "acrylic" || b == "mica" || b == "mica-alt" || b == "none") opts.backdrop = b;
    }
    if (p.backdropDarkMode) {
        opts.backdropDarkMode = *p.backdropDarkMode;
    }
    if (p.closeAnimationMs) {
        opts.closeAnimationMs = static_cast<int>(std::clamp<std::int64_t>(*p.closeAnimationMs, 0, 1000));
    }
    // css 与 items 同属渲染器资源，同样必须在打开 overlay 之前预检（与 tray 共用帮助函数）。
    auto cssBreach = menu_limits::ValidateCssBytes(opts.css);
    if (!cssBreach.ok) {
        return api::Fail("menu resource limit exceeded", ApiErrorCode::INVALID_PARAMS,
                         {{"details", menu_limits::DetailsJson(cssBreach)}});
    }
    // 锚定策略由窗口模型推导，不作为公共参数：内容尺寸窗是标准右键菜单语义（贴光标向下
    // 展开），全屏覆盖面保持 bottomUp 默认（其定位由渲染器在客户区内完成，此值不参与）。
    if (opts.windowModel == MenuWindowModel::ContentSized) {
        opts.anchorPolicy = "cursor";
    }

    int x = ScreenCoordinate(p.x);
    int y = ScreenCoordinate(p.y);
    if (x < 0 || y < 0) {
        POINT pt{};
        GetCursorPos(&pt);
        if (x < 0) x = pt.x;
        if (y < 0) y = pt.y;
    }
    // 不传 sink = 非 owner-mode：select / dismiss / valueChanged 走公共 menu:* 事件，
    // 发回调用 menu.show 的页面（popup 与面板页面也能等到结果）。
    std::string menuId = MenuOverlayHost::GetInstance().Show(
        items, x, y, nullptr, nullptr, opts, nullptr, MenuCaller{caller.windowId, caller.callerHwnd});
    if (menuId.empty()) {
        return api::Fail("failed to show menu overlay", ApiErrorCode::OPERATION_FAILED);
    }
    mnu::ShowResult result;
    result.menuId = std::move(menuId);
    return result;
}

// menu.close {reason?}: 关闭当前自绘菜单。
api::Result<void> MenuClose(const mnu::CloseParams& p) {
    MenuOverlayHost::GetInstance().Hide(p.reason);
    return api::Ok();
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
    // The public methods take their parameters and results from
    // src/api/schema/menu.ts; the overlay's private menu.__* calls stay undeclared.
    api::RegisterApi("menu.runMainMenuCommand", MenuRunMainMenuCommand);
    api::RegisterApi("menu.runContextCommand", MenuRunContextCommand);
    api::RegisterApi("menu.getMainMenu", MenuGetMainMenu);
    api::RegisterApi("menu.getContextMenu", MenuGetContextMenu);
    api::RegisterApi("menu.runContextCommandById", MenuRunContextCommandById);
    api::RegisterApi("menu.showNativePopup", MenuShowNativePopup);
    api::RegisterApi("menu.show", MenuShow);
    api::RegisterApi("menu.close", MenuClose);

    auto& bridge = BridgeCore::GetInstance();
    bridge.RegisterUndeclaredApi("menu.__getMenuState", MenuGetMenuState);
    bridge.RegisterUndeclaredApi("menu.__select", MenuSelect);
    bridge.RegisterUndeclaredApi("menu.__dismiss", MenuDismiss);
    bridge.RegisterUndeclaredApi("menu.__ready", MenuReady);
    bridge.RegisterUndeclaredApi("menu.__submenuPanel", MenuSubmenuPanel);
    bridge.RegisterUndeclaredApi("menu.__valueChanged", MenuValueChanged);
}
