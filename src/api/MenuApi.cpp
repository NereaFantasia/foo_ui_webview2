// MenuApi.cpp - Menu command helpers
// Provides execution of main menu and context menu commands by name/path

#include "pch.h"
#include "api/MenuApi.h"
#include "api/BridgeCore.h"
#include "api/ErrorEnvelope.h"
#include "api/TypedApi.h"
#include "api/generated/MenuSchema.h"
#include "api/MenuNodeContract.h"
#include "api/MainMenuIdentity.h"
#include "api/ContextMenuCheck.h"
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
#include "window/WindowManager.h"

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

    // The per-request index retains provider identity and dynamic leaves. It does not
    // depend on generate_menu(), which can fail on localized hosts. Display state is
    // a snapshot; execution refreshes the chosen retained leaf before dispatch.
    struct MenuReadError : std::runtime_error {
        std::string reason;
        std::string path;
        std::string source;
        json candidates = json::array();
        MenuReadError(std::string why, std::string at, std::string tier)
            : std::runtime_error("Main-menu data could not be read"), reason(std::move(why)),
              path(std::move(at)), source(std::move(tier)) {}
        api::Failure Failure() const {
            json details{{"reason", reason}, {"path", path}, {"source", source}};
            if (!candidates.empty()) details["candidates"] = candidates;
            return api::Fail(what(), reason == "ambiguousMatch" ? ApiErrorCode::MENU_MATCH_AMBIGUOUS : ApiErrorCode::OPERATION_FAILED,
                             {{"details", std::move(details)}});
        }
    };

    template <class Read>
    decltype(auto) ReadMainMenu(Read&& read, const char* reason, const std::string& path,
                                const char* source = "index") {
        try { return read(); }
        catch (const MenuReadError&) { throw; }
        catch (...) { throw MenuReadError(reason, path, source); }
    }

    std::string GuidKey(const GUID& guid) {
        return guid == pfc::guid_null ? std::string() : GuidToString(guid);
    }

    bool MainMenuSegmentsEqual(const std::string& left, const std::string& right) {
        return menu_node::SegmentsEqual(left, right) || NamesMatchI18n(left, right);
    }

    struct MainMenuIndexEntry : main_menu::Entry {
        mainmenu_commands::ptr owner;
        mainmenu_node::ptr dynamicRoot;
        mainmenu_node::ptr dynamicLeaf;
    };

    std::string AppendMenuPath(const std::string& prefix, const std::string& label) {
        return prefix.empty() ? label : (label.empty() ? prefix : prefix + "/" + label);
    }

    std::vector<main_menu::Group> ReadMainMenuGroups() {
        std::vector<main_menu::Group> groups;
        service_enum_t<mainmenu_group> services;
        mainmenu_group::ptr group;
        while (services.next(group)) {
            main_menu::Group value;
            value.guid = ReadMainMenu([&] { return GuidKey(group->get_guid()); }, "incompleteHierarchy", "");
            try {
                value.parent = GuidKey(group->get_parent());
                mainmenu_group_popup::ptr popup;
                if (group->service_query_t(popup)) {
                    pfc::string8 text;
                    popup->get_display_string(text);
                    value.label = StringUtils::SafeUtf8(text.c_str());
                    mainmenu_group_popup_v2::ptr conditional;
                    value.popup = !group->service_query_t(conditional) || conditional->popup_condition();
                }
            } catch (...) { value.complete = false; }
            groups.push_back(std::move(value));
        }
        return groups;
    }

    struct DynamicNodeAdapter {
        std::string path;
        bool valid(const mainmenu_node::ptr& node) const { return node.is_valid(); }
        menu_node::Kind kind(const mainmenu_node::ptr& node) const {
            const auto type = node->get_type();
            if (type == mainmenu_node::type_separator) return menu_node::Kind::Separator;
            if (type == mainmenu_node::type_group) return menu_node::Kind::Submenu;
            if (type == mainmenu_node::type_command) return menu_node::Kind::Command;
            throw MenuReadError("incompleteHierarchy", path, "index");
        }
        std::pair<std::string, menu_node::State> display(const mainmenu_node::ptr& node) const {
            pfc::string8 text;
            t_uint32 flags = 0;
            ReadMainMenu([&] { node->get_display(text, flags); }, "stateUnavailable", path);
            return {StringUtils::SafeUtf8(text.c_str()), menu_node::NormalizeMainMenu(flags, true)};
        }
        std::string guid(const mainmenu_node::ptr& node) const { return GuidKey(node->get_guid()); }
        size_t count(const mainmenu_node::ptr& node) const { return node->get_children_count(); }
        mainmenu_node::ptr child(const mainmenu_node::ptr& node, size_t i) const { return node->get_child(i); }
    };

    void CollectIndexDynamicNodes(const mainmenu_node::ptr& node, const std::string& prefix,
                                  const MainMenuIndexEntry& owner, int depth,
                                  std::vector<MainMenuIndexEntry>& out) {
        DynamicNodeAdapter adapter{prefix};
        auto append = [&](const mainmenu_node::ptr& leaf, const std::string& path, const std::string& label,
                          const menu_node::State& state, const std::string& guid) {
            MainMenuIndexEntry entry = owner;
            entry.name = entry.displayText = label;
            entry.path = path;
            entry.node = reinterpret_cast<std::uintptr_t>(leaf.get_ptr());
            entry.isDynamicParent = false;
            entry.dynamicLeaf = leaf;
            entry.subGuid = guid;
            entry.state = state;
            out.push_back(std::move(entry));
        };
        ReadMainMenu([&] { main_menu::VisitDynamic(node, prefix, depth, adapter, append); },
                     "incompleteHierarchy", prefix);
    }

    std::vector<MainMenuIndexEntry> BuildMainMenuIndex() {
        const auto groups = ReadMainMenuGroups();
        const std::unordered_map<std::string, std::string> roots{
            {GuidKey(mainmenu_groups::file), "File"}, {GuidKey(mainmenu_groups::edit), "Edit"},
            {GuidKey(mainmenu_groups::view), "View"}, {GuidKey(mainmenu_groups::playback), "Playback"},
            {GuidKey(mainmenu_groups::library), "Library"}, {GuidKey(mainmenu_groups::help), "Help"}};
        std::vector<MainMenuIndexEntry> index;
        service_enum_t<mainmenu_commands> services;
        mainmenu_commands::ptr owner;
        while (services.next(owner)) {
            const auto count = ReadMainMenu([&] { return owner->get_command_count(); }, "incompleteHierarchy", "");
            std::string groupGuid;
            try { groupGuid = GuidKey(owner->get_parent()); } catch (...) {}
            const auto group = main_menu::ResolveGroupPath(groups, groupGuid, roots);
            mainmenu_commands_v2::ptr v2;
            const bool hasV2 = owner->service_query_t(v2);
            for (t_uint32 slot = 0; slot < count; ++slot) {
                MainMenuIndexEntry entry;
                entry.owner = owner;
                entry.provider = reinterpret_cast<std::uintptr_t>(owner.get_ptr());
                entry.slot = slot;
                entry.groupGuid = groupGuid;
                entry.groupAncestors = group.ancestors;
                entry.hierarchyComplete = group.complete;
                pfc::string8 name;
                ReadMainMenu([&] { owner->get_name(slot, name); }, "incompleteHierarchy", group.path);
                entry.name = StringUtils::SafeUtf8(name.c_str());
                entry.guid = ReadMainMenu([&] { return GuidKey(owner->get_command(slot)); }, "incompleteHierarchy", entry.name);
                pfc::string8 text;
                t_uint32 flags = 0;
                const bool shown = ReadMainMenu([&] { return owner->get_display(slot, text, flags); },
                                                "stateUnavailable", entry.name);
                entry.displayText = StringUtils::SafeUtf8(text.c_str());
                if (entry.displayText.empty()) entry.displayText = entry.name;
                entry.path = AppendMenuPath(group.path, entry.displayText);
                entry.state = menu_node::NormalizeMainMenu(flags, shown);
                entry.isDynamic = hasV2 && ReadMainMenu([&] { return v2->is_command_dynamic(slot); },
                                                       "incompleteHierarchy", entry.path);
                entry.isDynamicParent = entry.isDynamic;
                if (entry.isDynamic) {
                    entry.dynamicRoot = ReadMainMenu([&] { return v2->dynamic_instantiate(slot); },
                                                     "incompleteHierarchy", entry.path);
                }
                index.push_back(entry);
                if (entry.isDynamic && entry.dynamicRoot.is_valid()) CollectIndexDynamicNodes(entry.dynamicRoot, group.path, entry, 0, index);
            }
        }
        return index;
    }

    std::vector<const MainMenuIndexEntry*> FindIndexCandidates(
        const std::vector<MainMenuIndexEntry>& index, const std::string& query) {
        return main_menu::FindCandidates(index, query, MainMenuSegmentsEqual);
    }

    const MainMenuIndexEntry* FindUniqueIndexEntry(
        const std::vector<MainMenuIndexEntry>& index, const std::string& path) {
        const auto matches = FindIndexCandidates(index, path);
        return matches.size() == 1 ? matches.front() : nullptr;
    }

    std::vector<const MainMenuIndexEntry*> FindIndexEntryByAddress(
        const std::vector<MainMenuIndexEntry>& index, const GUID& guid, const GUID& subGuid) {
        return main_menu::FindAddress(index, GuidKey(guid), GuidKey(subGuid));
    }

    std::vector<mnu::MenuAddressCandidate> DescribeCandidates(const std::vector<const MainMenuIndexEntry*>& matches) {
        std::vector<mnu::MenuAddressCandidate> result;
        for (const auto* match : matches) {
            mnu::MenuAddressCandidate candidate;
            candidate.path = match->path;
            candidate.guid = match->guid;
            candidate.groupGuid = match->groupGuid;
            if (!match->subGuid.empty()) candidate.subGuid = match->subGuid;
            result.push_back(std::move(candidate));
        }
        return result;
    }

    void AssignMainMenuAddress(mnu::MenuTreeNode& node,
                              const std::vector<const MainMenuIndexEntry*>& matches,
                              const std::vector<MainMenuIndexEntry>& index) {
        node.guid.reset();
        node.subGuid.reset();
        std::string reason = matches.empty() ? "noStableIdentifier" : "ambiguousMatch";
        if (matches.size() == 1) {
            const auto& hit = *matches.front();
            reason = main_menu::AddressReason(hit, index);
            if (reason != "ambiguousMatch") {
                if (!hit.guid.empty()) node.guid = hit.guid;
                if (!hit.subGuid.empty()) node.subGuid = hit.subGuid;
            }
        }
        node.executable = reason.empty();
        if (reason.empty()) return;
        node.unaddressableReason = reason;
        if (reason == "ambiguousMatch") {
            auto candidates = matches;
            if (matches.size() == 1) {
                candidates = main_menu::Collect(index, [&](const MainMenuIndexEntry& entry) {
                    return entry.guid == matches.front()->guid;
                });
            }
            node.addressCandidates = DescribeCandidates(candidates);
        }
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

    void CountSerializedAvailability(const MenuNodes& nodes, int& total, int& available) {
        for (const auto& child : nodes) {
            if (child.type == "command") {
                ++total;
                if (child.available.value_or(false)) ++available;
            } else if (child.children) CountSerializedAvailability(*child.children, total, available);
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
            if (entry.isDynamicParent) continue;
            mnu::MenuTreeNode item;
            item.type = "command";
            item.label = entry.displayText;
            item.displayLabel = TranslateMenuLabel(entry.displayText, locale, enableI18n);
            item.path = entry.path;
            item.displayPath = entry.path;
            item.available = item.enabled = entry.state.enabled;
            item.checked = entry.state.checked;
            item.radioChecked = entry.state.radioChecked;
            item.hidden = entry.state.hidden;
            item.flags = entry.state.flags;
            item.source = menu_node::ToString(entry.isDynamic ? menu_node::Source::MainMenuDynamic
                                                              : menu_node::Source::MainMenuStatic);
            AssignMainMenuAddress(item, {&entry}, index);
            item.fallback = true;
            items.push_back(std::move(item));
        }
        return items;
    }

    MenuNodes WalkHMenu(HMENU hmenu, const std::string& pathPrefix, const std::string& displayPathPrefix,
                        const std::string& locale, bool enableI18n,
                        const std::vector<MainMenuIndexEntry>* index, int depth = 0) {
        MenuNodes items;
        if (menu_node::DepthExceeded(depth)) throw MenuReadError("incompleteHierarchy", pathPrefix, "v1-hmenu");
        const int count = GetMenuItemCount(hmenu);
        if (count < 0 || menu_node::ChildrenExceeded(static_cast<size_t>(count))) throw MenuReadError("incompleteHierarchy", pathPrefix, "v1-hmenu");
        for (int i = 0; i < count; ++i) {
            MENUITEMINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING | MIIM_SUBMENU | MIIM_ID;
            if (!GetMenuItemInfoW(hmenu, i, TRUE, &info)) {
                throw MenuReadError("stateUnavailable", pathPrefix, "v1-hmenu");
            }
            if (info.fType & MFT_SEPARATOR) { items.push_back(SeparatorNode()); continue; }
            // A truncated label could equal a different command's complete name.
            std::wstring text(static_cast<size_t>(info.cch) + 1, L'\0');
            info.dwTypeData = text.data();
            info.cch = static_cast<UINT>(text.size());
            if (!GetMenuItemInfoW(hmenu, i, TRUE, &info)) {
                throw MenuReadError("stateUnavailable", pathPrefix, "v1-hmenu");
            }
            text.resize(info.cch);
            std::string label = WideToUtf8(text);
            if (const auto tab = label.find('\t'); tab != std::string::npos) label.erase(tab);
            const auto display = TranslateMenuLabel(label, locale, enableI18n);
            mnu::MenuTreeNode item;
            item.type = info.hSubMenu ? "submenu" : "command";
            item.label = label;
            item.displayLabel = display;
            item.path = AppendMenuPath(pathPrefix, label);
            item.displayPath = AppendMenuPath(displayPathPrefix, display);
            if (info.hSubMenu) {
                item.children = WalkHMenu(info.hSubMenu, *item.path, *item.displayPath, locale, enableI18n, index, depth + 1);
            } else {
                const auto state = menu_node::NormalizeHmenu(info.fState);
                item.available = item.enabled = state.enabled;
                item.checked = state.checked;
                item.radioChecked = state.radioChecked;
                item.hidden = state.hidden;
                item.flags = state.flags;
                item.commandId = info.wID;
                item.source = "hmenu_fallback";
                if (index && !label.empty()) {
                    const auto* unique = FindUniqueIndexEntry(*index, *item.path);
                    const auto matches = unique ? std::vector<const MainMenuIndexEntry*>{unique}
                                                : FindIndexCandidates(*index, *item.path);
                    AssignMainMenuAddress(item, matches, *index);
                } else {
                    item.executable = false;
                    item.unaddressableReason = "noStableIdentifier";
                }
            }
            items.push_back(std::move(item));
        }
        return items;
    }

    MenuNodes BuildMainMenuV1Tree(const std::string& locale, bool enableI18n, bool withAvailability) {
        struct TopMenu { GUID guid; const char* name; };
        const TopMenu topMenus[] = {
            {mainmenu_groups::file, "File"}, {mainmenu_groups::edit, "Edit"},
            {mainmenu_groups::view, "View"}, {mainmenu_groups::playback, "Playback"},
            {mainmenu_groups::library, "Library"}, {mainmenu_groups::help, "Help"}};
        const auto index = BuildMainMenuIndex();
        MenuNodes items;
        for (const auto& top : topMenus) {
            auto manager = mainmenu_manager::get();
            manager->instantiate(top.guid);
            std::unique_ptr<std::remove_pointer_t<HMENU>, decltype(&DestroyMenu)> menu(CreatePopupMenu(), &DestroyMenu);
            if (!menu) throw std::runtime_error("Unable to allocate main menu");
            manager->generate_menu_win32(menu.get(), 1, 65535, mainmenu_manager::flag_view_full);
            const std::string display = TranslateMenuLabel(top.name, locale, enableI18n);
            auto children = ReadMainMenu([&] {
                return WalkHMenu(menu.get(), top.name, display, locale, enableI18n, &index);
            }, "incompleteHierarchy", top.name, "v1-hmenu");
            if (children.empty()) continue;
            mnu::MenuTreeNode item;
            item.type = "submenu";
            item.label = item.path = top.name;
            item.displayLabel = item.displayPath = display;
            if (withAvailability) {
                int total = 0, available = 0;
                auto count = [&](const MenuNodes& nodes, const auto& self) -> void {
                    for (const auto& child : nodes) {
                        if (child.type == "command") {
                            ++total;
                            if (child.available.value_or(false)) ++available;
                        } else if (child.children) self(*child.children, self);
                    }
                };
                count(children, count);
                item.availability = AvailabilityOf(total, available);
            }
            item.children = std::move(children);
            items.push_back(std::move(item));
        }
        return items;
    }

    struct MainTreeMatch {
        menu_tree_item::ptr node;
        std::string path;
        GUID guid = pfc::guid_null;
        GUID subGuid = pfc::guid_null;
    };

    void CollectMainTreeMatches(const menu_tree_item::ptr& root, const std::string& query,
                                bool commands, const std::string& prefix,
                                std::vector<MainTreeMatch>& matches, int depth = 0) {
        if (!root.is_valid() || menu_node::DepthExceeded(depth)) {
            throw MenuReadError("incompleteHierarchy", prefix, "v2-tree");
        }
        const auto count = ReadMainMenu([&] { return root->childCount(); }, "incompleteHierarchy", prefix, "v2-tree");
        if (menu_node::ChildrenExceeded(count)) throw MenuReadError("incompleteHierarchy", prefix, "v2-tree");
        for (size_t i = 0; i < count; ++i) {
            auto child = ReadMainMenu([&] { return root->childAt(i); }, "incompleteHierarchy", prefix, "v2-tree");
            if (!child.is_valid()) throw MenuReadError("incompleteHierarchy", prefix, "v2-tree");
            ReadMainMenu([&] {
                if (child->isSeparator()) return;
                const std::string label = StringUtils::SafeUtf8(child->name());
                const std::string path = AppendMenuPath(prefix, label);
                bool hit = false;
                if (commands && child->isCommand()) {
                    hit = main_menu::MatchesQuery(label, label, path, true, query, MainMenuSegmentsEqual);
                } else if (!commands && child->isSubmenu()) {
                    const auto actual = SplitPath(path), wanted = SplitPath(query);
                    hit = actual.size() == wanted.size() &&
                          std::equal(actual.begin(), actual.end(), wanted.begin(), MainMenuSegmentsEqual);
                }
                if (hit) {
                    MainTreeMatch match{child, path};
                    if (commands) {
                        match.guid = child->commandGuid();
                        match.subGuid = child->subCommandGuid();
                    }
                    matches.push_back(std::move(match));
                }
                if (child->isSubmenu()) CollectMainTreeMatches(child, query, commands, path, matches, depth + 1);
            }, "incompleteHierarchy", prefix, "v2-tree");
        }
    }

    menu_tree_item::ptr FindMenuNodeByPath(const menu_tree_item::ptr& root,
                                          const std::vector<std::string>& parts, size_t first,
                                          std::string* canonicalPath = nullptr) {
        if (first >= parts.size()) return nullptr;
        const std::string query = menu_node::JoinPath(std::vector<std::string>(parts.begin() + first, parts.end()));
        std::vector<MainTreeMatch> matches;
        CollectMainTreeMatches(root, query, false, "", matches);
        if (matches.size() > 1) {
            MenuReadError error("ambiguousMatch", query, "v2-tree");
            for (const auto& match : matches) error.candidates.push_back({{"path", match.path}, {"guid", ""}, {"groupGuid", ""}});
            throw error;
        }
        if (matches.empty()) return nullptr;
        if (canonicalPath) *canonicalPath = matches.front().path;
        return matches.front().node;
    }

    std::vector<MainTreeMatch> FindMainMenuCommandByPath(const menu_tree_item::ptr& root,
                                                       const std::vector<std::string>& parts, size_t first) {
        std::vector<MainTreeMatch> matches;
        if (first < parts.size()) {
            CollectMainTreeMatches(root, menu_node::JoinPath(std::vector<std::string>(parts.begin() + first, parts.end())),
                                   true, "", matches);
        }
        return matches;
    }

    std::vector<MainTreeMatch> FindMainMenuCommandByName(const menu_tree_item::ptr& root, const std::string& name) {
        std::vector<MainTreeMatch> matches;
        CollectMainTreeMatches(root, name, true, "", matches);
        return matches;
    }

    menu_tree_item::ptr BuildMainMenuTree() {
        auto manager = mainmenu_manager_v2::tryGet();
        return manager.is_valid() ? manager->generate_menu(mainmenu_manager::flag_view_full) : nullptr;
    }

    menu_tree_item::ptr TryBuildMainMenuTree() {
        const auto tree = main_menu::ReadConstructedTree(BuildMainMenuTree,
            [](const menu_tree_item::ptr& root) { return root; },
            [](const menu_tree_item::ptr& root) { return root.is_valid(); });
        return tree.value_or(nullptr);
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
    // 成功时返回实际使用的模式：auto 会落到其余某一个。`outItems` 收到菜单所对应的曲目，
    // playlist 模式读不到，留空。
    static std::variant<std::string, api::Failure> InitContextMenu(
        service_ptr_t<contextmenu_manager>& mgr,
        const std::string& mode,
        const std::optional<std::vector<json>>& handlesParam,
        unsigned flags = contextmenu_manager::flag_view_full,
        metadb_handle_list* outItems = nullptr) {
        metadb_handle_list handles;
        const bool hasHandles = handlesParam && ParseHandleList(*handlesParam, handles);
        auto initWith = [&](const metadb_handle_list& items) {
            mgr->init_context(items, flags);
            if (outItems) *outItems = items;
        };
        auto initNowPlaying = [&]() {
            if (!mgr->init_context_now_playing(flags)) return false;
            metadb_handle_ptr playing;
            if (outItems && playback_control::get()->get_now_playing(playing)) outItems->add_item(playing);
            return true;
        };

        if (mode == "handles") {
            if (!hasHandles) {
                return api::Fail("handles required for mode=handles", ApiErrorCode::INVALID_PARAMS);
            }
            initWith(handles);
            return mode;
        }
        if (mode == "playlist") {
            mgr->init_context_playlist(flags);
            return mode;
        }
        if (mode == "nowPlaying") {
            if (!initNowPlaying()) {
                return api::Fail("No now playing item", ApiErrorCode::NO_ACTIVE_ITEM);
            }
            return mode;
        }
        if (mode == "selection") {
            metadb_handle_list selection = GetSelectedContextItems();
            if (selection.get_count() == 0) {
                return api::Fail("No playlist items selected", ApiErrorCode::NO_ACTIVE_ITEM);
            }
            initWith(selection);
            return mode;
        }

        // auto mode (and any other value): handles → nowPlaying → selection → playlist
        if (hasHandles) {
            initWith(handles);
            return std::string("handles");
        }
        // 给了 handles 却一条都用不了（路径被拒、形状不对、建不出 handle）时不往下回退：
        // 回退会把命令落到页面没点名的曲目上，删除类命令就删错了东西。
        if (handlesParam && !handlesParam->empty()) {
            return api::Fail("none of the given handles is usable", ApiErrorCode::INVALID_PARAMS);
        }
        if (initNowPlaying()) {
            return std::string("nowPlaying");
        }
        metadb_handle_list selection = GetSelectedContextItems();
        if (selection.get_count() > 0) {
            initWith(selection);
            return std::string("selection");
        }
        mgr->init_context_playlist(flags);
        return std::string("playlist");
    }

    // getContextMenu 报告、核对执行比对的目标指纹；playlist 模式没有。
    std::optional<std::string> ContextTargetKey(const std::string& mode, const metadb_handle_list& items) {
        if (mode == "playlist") return std::nullopt;
        std::vector<context_menu_check::TrackIdentity> tracks;
        tracks.reserve(items.get_count());
        for (t_size i = 0; i < items.get_count(); ++i) {
            tracks.push_back({items[i]->get_path(), items[i]->get_subsong_index()});
        }
        std::string playlist;
        if (mode == "selection") {
            auto plm = playlist_manager_v5::get();
            const t_size active = plm->get_active_playlist();
            if (active != pfc::infinite_size) playlist = GuidToString(plm->playlist_get_guid(active));
        }
        return context_menu_check::TargetKey(mode, tracks, playlist);
    }

    // 主菜单还使用提供者索引验证完整地址；上下文菜单保持自身的地址与状态语义。
    std::optional<mnu::MenuTreeNode> BuildMenuTreeNode(
        const menu_tree_item::ptr& node,
        const std::string& pathPrefix,
        const std::string& displayPathPrefix,
        const std::string& locale,
        bool enableI18n,
        bool withAvailability,
        menu_node::Source source,
        const std::vector<MainMenuIndexEntry>* mainIndex = nullptr, int depth = 0
    ) {
        if (mainIndex && (!node.is_valid() || menu_node::DepthExceeded(depth))) {
            throw MenuReadError("incompleteHierarchy", pathPrefix, "v2-tree");
        }
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
            if (mainIndex && menu_node::ChildrenExceeded(count)) throw MenuReadError("incompleteHierarchy", path, "v2-tree");
            for (size_t i = 0; i < count; i++) {
                try {
                    auto child = node->childAt(i);
                    if (!child.is_valid()) {
                        if (mainIndex) throw MenuReadError("incompleteHierarchy", path, "v2-tree");
                        continue;
                    }
                    auto item = BuildMenuTreeNode(child, path, displayPath, locale, enableI18n, withAvailability, source, mainIndex, depth + 1);
                    if (item) children.push_back(std::move(*item));
                } catch (const std::exception& ex) {
                    if (mainIndex) throw;
                    // 跳过有问题的子项（中文版 SDK 可能对某些项抛异常）
                    console::printf("[MenuApi] BuildMenuTreeNode: skipping submenu child %u: %s", (unsigned)i, ex.what());
                } catch (...) {
                    if (mainIndex) throw MenuReadError("incompleteHierarchy", path, "v2-tree");
                    // Non-std exception — skip this child silently
                }
            }

            mnu::MenuTreeNode result;
            result.type = "submenu";
            result.label = label;
            result.displayLabel = displayLabel;
            result.path = path;
            result.displayPath = displayPath;
            result.flags = mainIndex
                ? ReadMainMenu([&] { return node->flags(); }, "stateUnavailable", path, "v2-tree")
                : node->flags();
            result.children = std::move(children);

            if (withAvailability) {
                int total = 0;
                int available = 0;
                try {
                    if (mainIndex) CountSerializedAvailability(*result.children, total, available);
                    else CountCommandAvailability(node, total, available);
                } catch (...) {
                    if (mainIndex) throw MenuReadError("stateUnavailable", path, "v2-tree");
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
                if (mainIndex) throw MenuReadError("stateUnavailable", path, "v2-tree");
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
                item.available = mainIndex ? state.enabled : IsMenuItemAvailable(node);
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

            if (mainIndex) {
                const std::string owner = item.guid.value_or("");
                const std::string sub = item.subGuid.value_or("");
                const auto matches = main_menu::FindAddress(*mainIndex, owner, sub);
                const bool dynamicOwner = std::any_of(mainIndex->begin(), mainIndex->end(), [&](const auto& entry) {
                    return !owner.empty() && entry.guid == owner && entry.isDynamic;
                });
                AssignMainMenuAddress(item, matches, *mainIndex);
                item.source = menu_node::ToString(dynamicOwner ? menu_node::Source::MainMenuDynamic
                                                               : menu_node::Source::MainMenuStatic);
                if (dynamicOwner && sub.empty() && item.unaddressableReason == "dynamicParent") {
                    item.unaddressableReason = "missingDynamicIdentifier";
                }
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


api::Failure AmbiguousMainMenu(const std::vector<const MainMenuIndexEntry*>& matches) {
    return api::Fail("Main-menu identity is ambiguous", ApiErrorCode::MENU_MATCH_AMBIGUOUS,
                     {{"details", {{"reason", "ambiguousMatch"}, {"candidateCount", matches.size()},
                                    {"candidates", api::results::Value(DescribeCandidates(matches))}}}});
}

std::optional<api::Failure> ExecuteMainMenuMatch(const std::vector<MainMenuIndexEntry>& index,
                                               const std::vector<const MainMenuIndexEntry*>& matches) {
    const auto reason = main_menu::ExecuteUnique(index, matches, [](const MainMenuIndexEntry& entry) {
        pfc::string8 text;
        t_uint32 flags = 0;
        ReadMainMenu([&] {
            if (entry.isDynamic) entry.dynamicLeaf->get_display(text, flags);
            else entry.owner->get_display(entry.slot, text, flags);
        }, "stateUnavailable", entry.path);
        return menu_node::NormalizeMainMenu(flags, true).enabled;
    }, [](const MainMenuIndexEntry& entry) {
        ReadMainMenu([&] {
            if (entry.isDynamic) entry.dynamicLeaf->execute(nullptr);
            else entry.owner->execute(entry.slot, nullptr);
        }, "providerFailed", entry.path);
    });
    if (reason.empty()) return std::nullopt;
    if (reason == "notFound") return api::Fail("No main-menu leaf owns this address", ApiErrorCode::NOT_FOUND);
    if (reason == "ambiguousMatch") {
        const auto candidates = matches.size() == 1
            ? main_menu::Collect(index, [&](const auto& other) { return other.guid == matches.front()->guid; }) : matches;
        return AmbiguousMainMenu(candidates);
    }
    const auto& entry = *matches.front();
    return api::Fail(reason == "disabled" ? "Main-menu command is currently disabled"
                                          : "Main-menu command has no complete execution address",
                     reason == "disabled" ? ApiErrorCode::MENU_ITEM_DISABLED : ApiErrorCode::NOT_SUPPORTED,
                     {{"details", {{"reason", reason}, {"guid", entry.guid}, {"path", entry.path}}}});
}

api::Result<mnu::RunMainMenuCommandResult> MenuRunMainMenuCommand(const mnu::RunMainMenuCommandParams& p) {
    GUID guid{}, subGuid{};
    const std::string sub = p.subGuid.value_or("");
    if (StringToGuid(p.command, guid)) {
        if (!sub.empty() && !StringToGuid(sub, subGuid)) {
            return api::Fail("Invalid subGuid format", ApiErrorCode::INVALID_PARAMS);
        }
        if (!sub.empty() && subGuid == pfc::guid_null) {
            return api::Fail("No dynamic leaf owns this address", ApiErrorCode::NOT_FOUND,
                             {{"guid", p.command}, {"subGuid", sub}, {"dynamic", true}});
        }
        if (auto failure = ExecuteMainMenuAddress(guid, subGuid)) {
            failure->extra["guid"] = p.command;
            failure->extra["dynamic"] = !sub.empty();
            if (!sub.empty()) failure->extra["subGuid"] = sub;
            return *failure;
        }
        mnu::RunMainMenuCommandResult result;
        result.guid = p.command;
        result.dynamic = !sub.empty();
        if (!sub.empty()) result.subGuid = sub;
        result.completion = "unknown";
        return result;
    }
    if (!sub.empty()) return api::Fail("subGuid requires a command GUID", ApiErrorCode::INVALID_PARAMS);
    try {
        const auto index = BuildMainMenuIndex();
        auto root = TryBuildMainMenuTree();
        std::vector<const MainMenuIndexEntry*> matches;
        std::string source = "index";
        if (root.is_valid()) {
            source = "v2-tree";
            const auto parts = SplitPath(p.command);
            const auto rawMatches = parts.size() > 1 ? FindMainMenuCommandByPath(root, parts, 0)
                                                     : FindMainMenuCommandByName(root, p.command);
            const auto identityOf = [&](const MainTreeMatch& match) -> const MainMenuIndexEntry* {
                const auto addresses = FindIndexEntryByAddress(index, match.guid, match.subGuid);
                return addresses.size() == 1 && main_menu::AddressReason(*addresses.front(), index).empty()
                    ? addresses.front() : nullptr;
            };
            const auto treeMatches = main_menu::UniqueTreeMatches(rawMatches, identityOf);
            if (treeMatches.size() > 1) {
                json candidates = json::array();
                for (const auto& candidate : treeMatches) {
                    const auto addresses = FindIndexEntryByAddress(index, candidate.guid, candidate.subGuid);
                    if (!addresses.empty()) {
                        for (const auto& address : DescribeCandidates(addresses)) candidates.push_back(ToJson(address));
                    } else {
                        json item{{"path", candidate.path}, {"guid", GuidKey(candidate.guid)}, {"groupGuid", ""}};
                        if (candidate.subGuid != pfc::guid_null) item["subGuid"] = GuidKey(candidate.subGuid);
                        candidates.push_back(std::move(item));
                    }
                }
                return api::Fail("Main-menu identity is ambiguous", ApiErrorCode::MENU_MATCH_AMBIGUOUS,
                                 {{"details", {{"reason", "ambiguousMatch"}, {"candidates", std::move(candidates)}}}});
            }
            if (!treeMatches.empty()) {
                auto node = treeMatches.front().node;
                const auto flags = ReadMainMenu([&] { node->refreshState(); return node->flags(); },
                                                "stateUnavailable", treeMatches.front().path, "v2-tree");
                if (!menu_node::NormalizeMainMenu(flags, true).enabled) {
                    return api::Fail("Main-menu command is currently disabled", ApiErrorCode::MENU_ITEM_DISABLED);
                }
                const GUID owner = treeMatches.front().guid;
                const GUID child = treeMatches.front().subGuid;
                if (owner == pfc::guid_null) return api::Fail("Main-menu node has no stable address", ApiErrorCode::NOT_SUPPORTED);
                matches = FindIndexEntryByAddress(index, owner, child);
            }
        } else {
            matches = FindIndexCandidates(index, p.command);
        }
        if (matches.empty()) {
            return api::Fail("Main-menu command not found", ApiErrorCode::MENU_COMMAND_NOT_FOUND,
                             {{"details", {{"candidateCount", 0}, {"query", p.command}}}});
        }
        if (auto failure = ExecuteMainMenuMatch(index, matches)) return *failure;
        const auto& entry = *matches.front();
        mnu::RunMainMenuCommandResult result;
        result.guid = entry.guid;
        result.dynamic = entry.isDynamic;
        if (entry.isDynamic) result.subGuid = entry.subGuid;
        result.source = source;
        result.completion = "unknown";
        return result;
    } catch (const MenuReadError& error) { return error.Failure(); }
    catch (...) { return api::Fail("Main-menu identity could not be read", ApiErrorCode::OPERATION_FAILED); }
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
    result.completion = "unknown";
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
        result.completion = "unknown";
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
    return main_menu::ReadConstructedTree(BuildMainMenuTree, [&](const menu_tree_item::ptr& root) {
      return ReadMainMenu([&]() -> mnu::GetMainMenuResult {
        const auto index = BuildMainMenuIndex();
        auto base = root;
        std::string baseLabel, canonicalPath, displayPath;
        if (!rootName.empty()) {
            auto found = FindMenuNodeByPath(root, SplitPath(rootName), 0, &canonicalPath);
            if (found.is_valid()) {
                base = found;
                baseLabel = StringUtils::SafeUtf8(base->name());
                const auto parts = SplitPath(canonicalPath);
                for (const auto& part : parts) displayPath = AppendMenuPath(displayPath, TranslateMenuLabel(part, locale, enableI18n));
            }
        }
        MenuNodes items;
        const auto childCount = base->childCount();
        if (menu_node::ChildrenExceeded(childCount)) throw MenuReadError("incompleteHierarchy", canonicalPath, "v2-tree");
        for (size_t i = 0; i < childCount; ++i) {
            auto child = base->childAt(i);
            auto item = BuildMenuTreeNode(child, canonicalPath, displayPath, locale, enableI18n,
                                          withAvailability, menu_node::Source::MainMenuStatic, &index);
            if (item) items.push_back(std::move(*item));
        }
        // The first label is resolved; the second echoes the caller's requested path.
        // NOLINTNEXTLINE(readability-suspicious-call-argument)
        return BuildMainMenuResponse(baseLabel, rootName, rootName.empty() || !baseLabel.empty(),
                                     locale, enableI18n, withAvailability, std::move(items));
      }, "incompleteHierarchy", rootName, "v2-tree");
    }, [](const menu_tree_item::ptr& root) { return root.is_valid(); });
}

static std::optional<mnu::GetMainMenuResult> TryGetMainMenuFromV1(const std::string& rootName,
                                                                  const std::string& locale,
                                                                  bool enableI18n,
                                                                  bool withAvailability) {
    auto items = BuildMainMenuV1Tree(locale, enableI18n, withAvailability);
    if (items.empty()) return std::nullopt;
    if (rootName.empty()) return BuildMainMenuResponse("", "", true, locale, enableI18n,
                                                       withAvailability, std::move(items), "v1-hmenu");
    std::vector<mnu::MenuTreeNode*> matches;
    const auto wanted = SplitPath(rootName);
    auto visit = [&](MenuNodes& nodes, const auto& self) -> void {
        for (auto& node : nodes) {
            if (node.type != "submenu") continue;
            const auto parts = SplitPath(node.path.value_or(""));
            if (parts.size() == wanted.size() && std::equal(parts.begin(), parts.end(), wanted.begin(), MainMenuSegmentsEqual)) {
                matches.push_back(&node);
            }
            if (node.children) self(*node.children, self);
        }
    };
    visit(items, visit);
    if (matches.size() > 1) {
        MenuReadError error("ambiguousMatch", rootName, "v1-hmenu");
        for (const auto* match : matches) error.candidates.push_back({{"path", match->path.value_or("")}, {"guid", ""}, {"groupGuid", ""}});
        throw error;
    }
    if (matches.size() == 1) {
        auto& match = *matches.front();
        return BuildMainMenuResponse(match.label.value_or(""), rootName, true, locale, enableI18n,
                                     withAvailability, std::move(match.children).value_or(MenuNodes{}), "v1-hmenu");
    }
    return BuildMainMenuResponse("", rootName, false, locale, enableI18n,
                                 withAvailability, std::move(items), "v1-hmenu");
}

api::Result<mnu::GetMainMenuResult> MenuGetMainMenu(const mnu::GetMainMenuParams& p) {
    try {
        if (auto result = TryGetMainMenuFromV2(p.root, p.locale, p.i18n, p.withAvailability)) return std::move(*result);
    } catch (const MenuReadError& error) { return error.Failure(); }
    catch (...) { return api::Fail("Main-menu tree is incomplete", ApiErrorCode::OPERATION_FAILED); }
    try {
        if (auto result = TryGetMainMenuFromV1(p.root, p.locale, p.i18n, p.withAvailability)) return std::move(*result);
    } catch (const MenuReadError& error) { return error.Failure(); }
    catch (...) { /* The legacy source could not construct a tree. */ }
    try {
        auto result = BuildMainMenuResponse("", p.root, false, p.locale, p.i18n, p.withAvailability,
                                            BuildMainMenuFlatFallback(p.locale, p.i18n, BuildMainMenuIndex()));
        result.fallback = "flat-mainmenu-commands";
        return result;
    } catch (const MenuReadError& error) { return error.Failure(); }
    catch (...) { return api::Fail("All main-menu sources failed", ApiErrorCode::OPERATION_FAILED); }
}


// getContextMenu and a checked runContextCommandById build the tree the same
// way, so the path a page saw is the path the check compares.
static std::variant<std::vector<mnu::MenuTreeNode>, api::Failure> BuildContextTree(
    service_ptr_t<contextmenu_manager>& mgr, const std::string& locale, bool i18n, bool withAvailability) {
    service_ptr_t<contextmenu_manager_v2> mgr2;
    if (!mgr->service_query_t(mgr2)) {
        return api::Fail("contextmenu_manager_v2 not available", ApiErrorCode::NOT_SUPPORTED);
    }

    auto root = mgr2->build_menu();
    if (!root.is_valid()) {
        return api::Fail("Failed to build context menu", ApiErrorCode::OPERATION_FAILED);
    }

    std::vector<mnu::MenuTreeNode> items;
    const size_t count = root->childCount();
    for (size_t i = 0; i < count; i++) {
        auto child = root->childAt(i);
        if (!child.is_valid()) continue;
        // 这里传族（静态）。动态判定在叶节点按 subGuid 解析，不改这个参数。
        auto item = BuildMenuTreeNode(child, "", "", locale, i18n, withAvailability,
                                      menu_node::Source::ContextMenuStatic);
        if (item) items.push_back(std::move(*item));
    }
    return items;
}

// Fails a checked run when the menu rebuilt on `mgr` no longer matches what the
// page saw; nothing has run at that point. Paths do not depend on the locale.
static std::optional<api::Failure> CheckContextCommand(service_ptr_t<contextmenu_manager>& mgr,
                                                       const std::string& mode,
                                                       const metadb_handle_list& items,
                                                       const mnu::RunContextCommandByIdParams& p) {
    auto because = [](const char* reason) {
        return nlohmann::json::object_t{{"details", {{"reason", reason}}}};
    };
    const auto key = ContextTargetKey(mode, items);
    if (!key) {
        return api::Fail("A checked run needs tracks the host can read; mode playlist has none",
                         ApiErrorCode::NOT_SUPPORTED, because("unsupportedTargetMode"));
    }
    if (*key != *p.expectTarget) {
        return api::Fail("The tracks differ from the ones the menu was built for",
                         ApiErrorCode::NOT_FOUND, because("targetChanged"));
    }

    auto tree = BuildContextTree(mgr, "", false, false);
    if (auto* failure = std::get_if<api::Failure>(&tree)) {
        return std::move(*failure);
    }
    switch (context_menu_check::CheckCommand(std::get<0>(tree), p.id, *p.expectPath)) {
        case context_menu_check::CommandCheck::Ok:
            return std::nullopt;
        case context_menu_check::CommandCheck::CommandChanged:
            return api::Fail("The menu has another item at this id", ApiErrorCode::NOT_FOUND,
                             because("commandChanged"));
        case context_menu_check::CommandCheck::AmbiguousPath:
            return api::Fail("Several menu items have this path", ApiErrorCode::MENU_MATCH_AMBIGUOUS,
                             because("ambiguousPath"));
        case context_menu_check::CommandCheck::Disabled:
            return api::Fail("The menu item or a parent is disabled or hidden",
                             ApiErrorCode::MENU_ITEM_DISABLED, because("disabled"));
    }
    return api::Fail("Unknown check result", ApiErrorCode::OPERATION_FAILED);
}


api::Result<mnu::GetContextMenuResult> MenuGetContextMenu(const mnu::GetContextMenuParams& p) {
    service_ptr_t<contextmenu_manager> mgr;
    contextmenu_manager::g_create(mgr);

    metadb_handle_list items;
    auto init = InitContextMenu(mgr, p.mode, p.handles, contextmenu_manager::flag_view_full, &items);
    if (auto* failure = std::get_if<api::Failure>(&init)) {
        return std::move(*failure);
    }

    auto tree = BuildContextTree(mgr, p.locale, p.i18n, p.withAvailability);
    if (auto* failure = std::get_if<api::Failure>(&tree)) {
        return std::move(*failure);
    }

    mnu::GetContextMenuResult result;
    result.items = std::move(std::get<0>(tree));
    result.mode = std::get<std::string>(init);
    result.locale = p.locale;
    result.i18n = p.i18n;
    result.withAvailability = p.withAvailability;
    result.targetKey = ContextTargetKey(result.mode, items);
    return result;
}


api::Result<mnu::RunContextCommandByIdResult> MenuRunContextCommandById(const mnu::RunContextCommandByIdParams& p) {
    if (p.expectPath.has_value() != p.expectTarget.has_value()) {
        return api::Fail("expectPath and expectTarget go together", ApiErrorCode::INVALID_PARAMS,
                         {{"details", {{"reason", "conflictingParams"}}}});
    }

    service_ptr_t<contextmenu_manager> mgr;
    contextmenu_manager::g_create(mgr);

    metadb_handle_list items;
    auto init = InitContextMenu(mgr, p.mode, p.handles, contextmenu_manager::flag_view_full, &items);
    if (auto* failure = std::get_if<api::Failure>(&init)) {
        return std::move(*failure);
    }

    // Checked and executed on the same manager, with no rebuild in between.
    if (p.expectPath) {
        if (auto failure = CheckContextCommand(mgr, std::get<std::string>(init), items, p)) {
            return std::move(*failure);
        }
    }

    // The declared range keeps the id within unsigned.
    if (!mgr->execute_by_id(static_cast<unsigned>(p.id))) {
        return api::Fail("No context menu item has this id", ApiErrorCode::NOT_FOUND, {{"id", p.id}});
    }
    mnu::RunContextCommandByIdResult result;
    result.completion = "unknown";
    return result;
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
                if (top) {
                    auto activation = WindowManager::GetInstance().BeginExplicitActivation(top, "menu.showNativePopup");
                    WindowManager::GetInstance().RequestForeground(top, "menu.showNativePopup");
                }
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

std::optional<api::Failure> ExecuteMainMenuAddress(const GUID& guid, const GUID& subGuid) {
    if (guid == pfc::guid_null) return api::Fail("No main-menu leaf owns this address", ApiErrorCode::NOT_FOUND);
    try {
        const auto index = BuildMainMenuIndex();
        return ExecuteMainMenuMatch(index, FindIndexEntryByAddress(index, guid, subGuid));
    } catch (const MenuReadError& error) { return error.Failure(); }
    catch (...) { return api::Fail("Main-menu identity could not be read", ApiErrorCode::OPERATION_FAILED); }
}

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
