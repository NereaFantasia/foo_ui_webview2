// test_api_schema_events.cpp - 由 src/api/schema/ 生成的事件描述与载荷结构体。
//
// 发射助手（src/api/EventEmit.h）只收描述里的 Payload，页面收到的键就是 ToJson 写出的键。
// 这里走 EventRegistry.h 列出的全部事件：名字是「命名空间:键」且不重复，默认构造的载荷
// 写出的每个键都在 kFields 里。单个事件的具体形状另测。
#include "pch.h"
#include "compat/fb2k_types.h"  // console:: stub: the generated headers include ErrorEnvelope.h
#include "api/generated/EventRegistry.h"

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

using nlohmann::json;

namespace {

template <class E>
void ExpectWellFormed(std::set<std::string>& names) {
    using Payload = typename E::Payload;
    const std::string_view name = E::kName;
    const auto colon = name.find(':');
    EXPECT_TRUE(colon != std::string_view::npos && colon > 0 && colon + 1 < name.size()) << name;
    EXPECT_TRUE(names.insert(std::string(name)).second) << name << " is declared twice";
    const json j = ToJson(Payload{});
    ASSERT_TRUE(j.is_object()) << name;
    for (const auto& item : j.items()) {
        const std::string& key = item.key();
        EXPECT_NE(std::find(Payload::kFields.begin(), Payload::kFields.end(), key), Payload::kFields.end())
            << name << " writes " << key << ", which its kFields does not list";
    }
}

}  // namespace

TEST(ApiSchemaEvents, EveryDeclaredEventIsNamedOnceAndWritesOnlyItsKeys) {
    std::set<std::string> names;
    std::apply([&names](auto... e) { (ExpectWellFormed<decltype(e)>(names), ...); }, api::events::All{});
    EXPECT_FALSE(names.empty());
}

TEST(ApiSchemaEvents, CursorHiddenChangedCarriesTheNewState) {
    namespace cursor = api::cursor;
    static_assert(std::is_same_v<cursor::events::HiddenChanged::Payload, cursor::HiddenChangedPayload>);
    static_assert(!cursor::events::HiddenChanged::kCustomName);
    EXPECT_EQ(std::string_view(cursor::events::HiddenChanged::kName), "cursor:hiddenChanged");
    cursor::HiddenChangedPayload payload;
    payload.hidden = true;
    EXPECT_EQ(ToJson(payload), (json{{"hidden", true}}));
}

// 下面各例按迁移前发射处写出的 JSON 核对：键集、取值类型与可选键的有无都不能变。

TEST(ApiSchemaEvents, KeyboardHotkeyCarriesTheRegistration) {
    namespace keyboard = api::keyboard;
    EXPECT_EQ(std::string_view(keyboard::events::Hotkey::kName), "keyboard:hotkey");
    keyboard::HotkeyPayload payload;
    payload.id = 3;
    payload.key = "Ctrl+Alt+K";
    payload.action = "toggle";
    EXPECT_EQ(ToJson(payload), (json{{"id", 3}, {"key", "Ctrl+Alt+K"}, {"action", "toggle"}}));
}

TEST(ApiSchemaEvents, TaskbarButtonClickedCarriesTheButtonId) {
    namespace taskbar = api::taskbar;
    EXPECT_EQ(std::string_view(taskbar::events::ButtonClicked::kName), "taskbar:buttonClicked");
    taskbar::ButtonClickedPayload payload;
    payload.id = "next";
    EXPECT_EQ(ToJson(payload), (json{{"id", "next"}}));
}

TEST(ApiSchemaEvents, TrayEventsCarryTheCursorAndTheRow) {
    namespace tray = api::tray;
    EXPECT_EQ(std::string_view(tray::events::Click::kName), "tray:click");
    EXPECT_EQ(std::string_view(tray::events::DoubleClick::kName), "tray:doubleClick");
    EXPECT_EQ(std::string_view(tray::events::MenuItemClicked::kName), "tray:menuItemClicked");
    EXPECT_EQ(std::string_view(tray::events::BeforeContextMenu::kName), "tray:beforeContextMenu");

    tray::ClickPayload click;
    click.x = -1200;
    click.y = 40;
    EXPECT_EQ(ToJson(click), (json{{"button", 0}, {"x", -1200}, {"y", 40}}));

    tray::DoubleClickPayload doubleClick;
    doubleClick.x = 5;
    doubleClick.y = 6;
    EXPECT_EQ(ToJson(doubleClick), (json{{"x", 5}, {"y", 6}}));

    tray::BeforeContextMenuPayload menu;
    menu.x = 7;
    menu.y = 8;
    EXPECT_EQ(ToJson(menu), (json{{"x", 7}, {"y", 8}}));

    // 普通行只报 id，没有 id 的行报空串；富控件才带 value。
    tray::MenuItemClickedPayload row;
    EXPECT_EQ(ToJson(row), (json{{"id", ""}}));
    row.id = "rate";
    row.value = 4;
    EXPECT_EQ(ToJson(row), (json{{"id", "rate"}, {"value", 4}}));
}

TEST(ApiSchemaEvents, UiEventsCarryTheRowTheToastOrNothing) {
    namespace ui = api::ui;
    EXPECT_EQ(std::string_view(ui::events::MenuItemClicked::kName), "ui:menuItemClicked");
    EXPECT_EQ(std::string_view(ui::events::Toast::kName), "ui:toast");
    EXPECT_EQ(std::string_view(ui::events::ColoursChanged::kName), "ui:coloursChanged");
    EXPECT_EQ(std::string_view(ui::events::FontChanged::kName), "ui:fontChanged");

    ui::MenuItemClickedPayload row;
    row.id = "copy";
    row.label = "Copy";
    EXPECT_EQ(ToJson(row), (json{{"id", "copy"}, {"label", "Copy"}}));

    ui::ToastPayload toast;
    toast.message = "Saved";
    toast.duration = 3000;
    toast.type = "info";
    toast.position = "bottom-right";
    EXPECT_EQ(ToJson(toast), (json{{"message", "Saved"}, {"duration", 3000}, {"type", "info"}, {"position", "bottom-right"}}));

    // 两个通知原先发 json::object()，页面收到的是 {} 而不是 null。
    EXPECT_EQ(ToJson(ui::ColoursChangedPayload{}), json::object());
    EXPECT_EQ(ToJson(ui::FontChangedPayload{}), json::object());
}

TEST(ApiSchemaEvents, SystemThemeChangedCarriesDarkMode) {
    namespace sys = api::system;
    EXPECT_EQ(std::string_view(sys::events::ThemeChanged::kName), "system:themeChanged");
    sys::ThemeChangedPayload payload;
    payload.darkMode = true;
    EXPECT_EQ(ToJson(payload), (json{{"darkMode", true}}));
}

TEST(ApiSchemaEvents, AppBeforeQuitCarriesNoFields) {
    EXPECT_EQ(std::string_view(api::app::events::BeforeQuit::kName), "app:beforeQuit");
    EXPECT_EQ(ToJson(api::app::BeforeQuitPayload{}), json::object());
}

TEST(ApiSchemaEvents, WebviewProcessFailedCarriesKindAndRecovery) {
    namespace webview = api::webview;
    EXPECT_EQ(std::string_view(webview::events::ProcessFailed::kName), "webview:processFailed");
    webview::ProcessFailedPayload payload;
    payload.kind = "renderProcessExited";
    payload.kindRaw = 1;
    payload.recovered = true;
    payload.recoveryAction = "reload";
    EXPECT_EQ(ToJson(payload),
              (json{{"kind", "renderProcessExited"}, {"kindRaw", 1}, {"recovered", true}, {"recoveryAction", "reload"}}));
}

TEST(ApiSchemaEvents, PluginAndApiEventsCarryTheSharedDescriptors) {
    static_assert(std::is_same_v<api::plugin::events::Registered::Payload, api::common::SystemPluginInfo>);
    static_assert(std::is_same_v<api::plugin::events::Unregistered::Payload, api::common::SystemPluginInfo>);
    static_assert(std::is_same_v<api::api_::events::Registered::Payload, api::common::SystemApiInfo>);
    static_assert(std::is_same_v<api::api_::events::Unregistered::Payload, api::common::SystemApiInfo>);
    EXPECT_EQ(std::string_view(api::plugin::events::Registered::kName), "plugin:registered");
    EXPECT_EQ(std::string_view(api::plugin::events::Unregistered::kName), "plugin:unregistered");
    EXPECT_EQ(std::string_view(api::api_::events::Registered::kName), "api:registered");
    EXPECT_EQ(std::string_view(api::api_::events::Unregistered::kName), "api:unregistered");

    // 与 PluginRegistry.h 里 PluginInfo::toJson / ApiInfo::toJson 写出的键逐一相同。
    api::common::SystemPluginInfo plugin;
    plugin.name = "My Plugin";
    plugin.namespace_ = "my_plugin";
    plugin.version = "1.0.0";
    plugin.author = "Author";
    plugin.description = "Description";
    plugin.apiCount = 1;
    plugin.apis = {"my_plugin.doSomething"};
    EXPECT_EQ(ToJson(plugin), (json{{"name", "My Plugin"}, {"namespace", "my_plugin"}, {"version", "1.0.0"},
                                    {"author", "Author"}, {"description", "Description"}, {"apiCount", 1},
                                    {"apis", json::array({"my_plugin.doSomething"})}}));

    api::common::SystemApiInfo method;
    method.fullName = "my_plugin.doSomething";
    method.plugin = "My Plugin";
    method.namespace_ = "my_plugin";
    method.method = "doSomething";
    method.description = "";
    method.version = "1.0";
    method.isExternal = true;
    EXPECT_EQ(ToJson(method), (json{{"fullName", "my_plugin.doSomething"}, {"plugin", "My Plugin"}, {"namespace", "my_plugin"},
                                    {"method", "doSomething"}, {"description", ""}, {"version", "1.0"}, {"isExternal", true}}));
}

TEST(ApiSchemaEvents, MetadbChangedWritesOnlyTheTagsATrackHas) {
    // 别名不叫 metadb：foobar2000 SDK 在全局作用域有同名的类。
    namespace mdb = api::metadb;
    EXPECT_EQ(std::string_view(mdb::events::Changed::kName), "metadb:changed");
    mdb::ChangedPayload payload;
    mdb::MetadbChangedTrackItem bare;
    bare.handle = "D:\\Music\\a.flac";
    bare.path = "D:\\Music\\a.flac";
    // A cue track: the path names the sheet, handle and subsong name the track.
    mdb::MetadbChangedTrackItem tagged;
    tagged.handle = "D:\\Music\\b.cue|subsong:3";
    tagged.path = "D:\\Music\\b.cue";
    tagged.subsong = 3;
    tagged.rating = 0;
    tagged.playCount = 12;
    tagged.title = "B";
    tagged.artist = "X, Y";
    payload.tracks = {bare, tagged};
    payload.count = 60;
    payload.fromHook = true;
    payload.timestamp = 1790000000000;
    EXPECT_EQ(ToJson(payload),
              (json{{"tracks", json::array({json{{"handle", "D:\\Music\\a.flac"}, {"path", "D:\\Music\\a.flac"},
                                                 {"subsong", 0}},
                                            json{{"handle", "D:\\Music\\b.cue|subsong:3"}, {"path", "D:\\Music\\b.cue"},
                                                 {"subsong", 3}, {"rating", 0}, {"playCount", 12},
                                                 {"title", "B"}, {"artist", "X, Y"}}})},
                    {"count", 60},
                    {"fromHook", true},
                    {"timestamp", 1790000000000}}));
}

TEST(ApiSchemaEvents, StateEventsWritePreviousValueAndExpiryAsBefore) {
    namespace st = api::state;
    EXPECT_EQ(std::string_view(st::events::Changed::kName), "state:changed");
    EXPECT_EQ(std::string_view(st::events::Deleted::kName), "state:deleted");
    const json value = json::object({{"hue", 180}});

    // 新键的 previousValue 写成 null 而不是缺席；没有 ttl 时不写 expiresAt。
    st::ChangedPayload fresh;
    fresh.key = "theme:accent";
    fresh.value = value;
    fresh.sourceWindowId = "main";
    EXPECT_EQ(ToJson(fresh),
              (json{{"key", "theme:accent"}, {"value", value}, {"previousValue", nullptr}, {"sourceWindowId", "main"}}));

    st::ChangedPayload timed = fresh;
    timed.previousValue = 3;
    timed.expiresAt = 1790000000000;
    EXPECT_EQ(ToJson(timed), (json{{"key", "theme:accent"},
                                   {"value", value},
                                   {"previousValue", 3},
                                   {"sourceWindowId", "main"},
                                   {"expiresAt", 1790000000000}}));

    // 过期清扫没有来源窗口，sourceWindowId 是空串。
    st::DeletedPayload expired;
    expired.key = "theme:accent";
    expired.reason = "expired";
    EXPECT_EQ(ToJson(expired), (json{{"key", "theme:accent"}, {"sourceWindowId", ""}, {"reason", "expired"}}));
}

TEST(ApiSchemaEvents, DndEventsKeepTheirKeysWithThePathGateOpenOrClosed) {
    namespace dnd = api::dnd;
    EXPECT_EQ(std::string_view(dnd::events::Enter::kName), "dnd:enter");
    EXPECT_EQ(std::string_view(dnd::events::Leave::kName), "dnd:leave");
    EXPECT_EQ(std::string_view(dnd::events::Drop::kName), "dnd:drop");
    EXPECT_EQ(std::string_view(dnd::events::CapabilitiesChanged::kName), "dnd:capabilitiesChanged");
    EXPECT_EQ(std::string_view(dnd::events::DragEnded::kName), "dnd:dragEnded");

    // 闸打开：resolvedPaths 与 paths 等长，不是快捷方式的位置写 null。
    dnd::EnterPayload enter;
    enter.sessionId = "s1";
    enter.paths = {"C:\\in\\a.lnk", "C:\\in\\b.flac"};
    enter.resolvedPaths = {std::string("D:\\music\\a.flac"), std::nullopt};
    enter.hasFiles = true;
    enter.source = "external";
    enter.x = 10;
    enter.y = 20;
    EXPECT_EQ(ToJson(enter), (json{{"sessionId", "s1"},
                                   {"paths", json::array({"C:\\in\\a.lnk", "C:\\in\\b.flac"})},
                                   {"resolvedPaths", json::array({"D:\\music\\a.flac", nullptr})},
                                   {"hasFiles", true},
                                   {"source", "external"},
                                   {"x", 10},
                                   {"y", 20}}));

    // 闸关闭：两个数组都是空数组（不是一串 null），hasFiles 照实报。
    dnd::EnterPayload gated;
    gated.sessionId = "s2";
    gated.hasFiles = true;
    gated.source = "self";
    EXPECT_EQ(ToJson(gated), (json{{"sessionId", "s2"},
                                   {"paths", json::array()},
                                   {"resolvedPaths", json::array()},
                                   {"hasFiles", true},
                                   {"source", "self"},
                                   {"x", 0},
                                   {"y", 0}}));

    dnd::LeavePayload leave;
    leave.sessionId = "s1";
    EXPECT_EQ(ToJson(leave), (json{{"sessionId", "s1"}}));

    // drop 不带 hasFiles，带 keyState。
    dnd::DropPayload drop;
    drop.sessionId = "s1";
    drop.paths = {"C:\\in\\b.flac"};
    drop.resolvedPaths = {std::nullopt};
    drop.x = 3;
    drop.y = 4;
    drop.keyState = 8;
    drop.source = "other-window";
    EXPECT_EQ(ToJson(drop), (json{{"sessionId", "s1"},
                                  {"paths", json::array({"C:\\in\\b.flac"})},
                                  {"resolvedPaths", json::array({nullptr})},
                                  {"x", 3},
                                  {"y", 4},
                                  {"keyState", 8},
                                  {"source", "other-window"}}));

    // 事件与 dnd.getCapabilities 由同一份声明生成，字段一一相同；原因只在对应能力缺失时出现。
    static_assert(dnd::CapabilitiesChangedPayload::kFields == dnd::GetCapabilitiesResult::kFields);
    dnd::CapabilitiesChangedPayload caps;
    caps.html5 = true;
    caps.hosting = "visual";
    caps.pathsUnavailableReason = "origin-untrusted";
    caps.dragOut = true;
    EXPECT_EQ(ToJson(caps), (json{{"html5", true},
                                  {"paths", false},
                                  {"hosting", "visual"},
                                  {"pathsUnavailableReason", "origin-untrusted"},
                                  {"dragOut", true}}));

    dnd::DragEndedPayload ended;
    ended.result = "failed";
    ended.code = "PERMISSION_DENIED";
    ended.error = "Drag token was rejected.";
    EXPECT_EQ(ToJson(ended),
              (json{{"result", "failed"}, {"code", "PERMISSION_DENIED"}, {"error", "Drag token was rejected."}}));
}

TEST(ApiSchemaEvents, WindowEventsKeepTheirKeys) {
    namespace wn = api::window;
    EXPECT_EQ(std::string_view(wn::events::Activated::kName), "window:activated");
    EXPECT_EQ(std::string_view(wn::events::StateChanged::kName), "window:stateChanged");
    EXPECT_EQ(std::string_view(wn::events::Message::kName), "window:message");
    EXPECT_EQ(std::string_view(wn::events::AlwaysOnTopChanged::kName), "window:alwaysOnTopChanged");

    wn::ActivatedPayload activated;
    activated.active = true;
    EXPECT_EQ(ToJson(activated), (json{{"active", true}}));

    // dpiScale 是浮点，其余尺寸是整数。
    wn::DpiChangedPayload dpi;
    dpi.dpi = 144;
    dpi.dpiScale = 1.5;
    dpi.titlebarHeight = 48;
    dpi.captionButtonWidth = 69;
    dpi.captionButtonsWidth = 207;
    EXPECT_EQ(ToJson(dpi), (json{{"dpi", 144},
                                 {"dpiScale", 1.5},
                                 {"titlebarHeight", 48},
                                 {"captionButtonWidth", 69},
                                 {"captionButtonsWidth", 207}}));

    // 带上状态变了的窗口的 windowId；四个状态各带一个同值的别名键。
    wn::StateChangedPayload state;
    state.windowId = "popup-1";
    state.isMaximized = state.maximized = true;
    state.isActive = state.active = true;
    EXPECT_EQ(ToJson(state), (json{{"windowId", "popup-1"},
                                   {"isMaximized", true},
                                   {"isMinimized", false},
                                   {"maximized", true},
                                   {"minimized", false},
                                   {"isActive", true},
                                   {"active", true},
                                   {"isFullscreen", false},
                                   {"fullscreen", false}}));

    wn::BackdropStateChangedPayload backdrop;
    backdrop.windowId = "main";
    backdrop.active = false;
    backdrop.mode = "inactive";
    backdrop.effect = "mica";
    EXPECT_EQ(ToJson(backdrop),
              (json{{"windowId", "main"}, {"active", false}, {"mode", "inactive"}, {"effect", "mica"}}));

    wn::HoverStateChangedPayload hover;
    hover.windowId = "popup-1";
    hover.hovering = true;
    EXPECT_EQ(ToJson(hover), (json{{"windowId", "popup-1"}, {"hovering", true}}));

    // 消息原样送达，不一定是对象。
    wn::MessagePayload message;
    message.sourceWindowId = "popup-1";
    message.message = "ping";
    EXPECT_EQ(ToJson(message), (json{{"sourceWindowId", "popup-1"}, {"message", "ping"}}));

    wn::MinimizeSuppressedPayload suppressed;
    suppressed.windowId = "popup-1";
    suppressed.reason = "policy.keepVisibleOnShowDesktop";
    EXPECT_EQ(ToJson(suppressed), (json{{"windowId", "popup-1"}, {"reason", "policy.keepVisibleOnShowDesktop"}}));

    wn::PopupOpenedPayload opened;
    opened.windowId = "popup-1";
    opened.title = "Lyrics";
    opened.url = "http://127.0.0.1:5180/lyrics";
    EXPECT_EQ(ToJson(opened),
              (json{{"windowId", "popup-1"}, {"title", "Lyrics"}, {"url", "http://127.0.0.1:5180/lyrics"}}));

    wn::PopupClosedPayload closed;
    closed.windowId = "popup-1";
    EXPECT_EQ(ToJson(closed), (json{{"windowId", "popup-1"}}));
    wn::BeforeClosePayload beforeClose;
    beforeClose.windowId = "popup-1";
    EXPECT_EQ(ToJson(beforeClose), (json{{"windowId", "popup-1"}}));

    wn::AlwaysOnTopChangedPayload onTop;
    onTop.enabled = true;
    EXPECT_EQ(ToJson(onTop), (json{{"enabled", true}}));

    // 事件与 window.getPopupBehavior 由同一份声明生成，字段一一相同。
    static_assert(wn::BehaviorChangedPayload::kFields == wn::GetPopupBehaviorResult::kFields);
    wn::BehaviorChangedPayload behavior;
    behavior.windowId = "popup-1";
    behavior.profile = "miniPlayer";
    behavior.behavior["noActivate"] = true;
    behavior.resolvedBehavior.showInTaskbar = true;
    behavior.resolvedBehavior.owner = "main";
    behavior.resolvedBehavior.noActivate = true;
    EXPECT_EQ(ToJson(behavior), (json{{"windowId", "popup-1"},
                                      {"profile", "miniPlayer"},
                                      {"behavior", {{"noActivate", true}}},
                                      {"resolvedBehavior",
                                       {{"showInTaskbar", true},
                                        {"showInAltTab", false},
                                        {"keepVisibleOnShowDesktop", false},
                                        {"allowMinimize", false},
                                        {"owner", "main"},
                                        {"noActivate", true}}}}));
}

TEST(ApiSchemaEvents, PanelEventsKeepTheirKeys) {
    namespace pn = api::panel;
    EXPECT_EQ(std::string_view(pn::events::Focus::kName), "panel:focus");
    EXPECT_EQ(std::string_view(pn::events::ConfigChanged::kName), "panel:configChanged");

    // focus 与 blur 不带字段。
    EXPECT_EQ(ToJson(pn::FocusPayload{}), json::object());
    EXPECT_EQ(ToJson(pn::BlurPayload{}), json::object());

    pn::InitializedPayload initialized;
    initialized.mode = "dui";
    initialized.panelMode = true;
    initialized.windowId = "panel-1";
    EXPECT_EQ(ToJson(initialized), (json{{"mode", "dui"}, {"panelMode", true}, {"windowId", "panel-1"}}));

    pn::VisibilityChangedPayload visibility;
    visibility.visible = false;
    EXPECT_EQ(ToJson(visibility), (json{{"visible", false}}));

    // 事件与 panel.getConfig 的 config 由同一份声明生成；edgeStyle 是数字。
    static_assert(pn::ConfigChangedPayload::kFields == pn::PanelConfig::kFields);
    pn::ConfigChangedPayload config;
    config.panelName = "Lyrics";
    config.templateName = "default";
    config.edgeStyle = 2;
    config.transparentBackground = true;
    config.grabFocus = true;
    EXPECT_EQ(ToJson(config), (json{{"panelName", "Lyrics"},
                                    {"templateName", "default"},
                                    {"edgeStyle", 2},
                                    {"urlOverride", ""},
                                    {"transparentBackground", true},
                                    {"grabFocus", true},
                                    {"enableDragDrop", false},
                                    {"enableDevTools", false}}));
}

TEST(ApiSchemaEvents, MenuAndPlaybackOptionEventsKeepTheirKeys) {
    namespace mn = api::menu;
    EXPECT_EQ(std::string_view(mn::events::Select::kName), "menu:select");
    EXPECT_EQ(std::string_view(mn::events::Dismiss::kName), "menu:dismiss");
    EXPECT_EQ(std::string_view(mn::events::ValueChanged::kName), "menu:valueChanged");

    mn::SelectPayload select;
    select.menuId = "menu-3";
    select.itemId = "play";
    EXPECT_EQ(ToJson(select), (json{{"menuId", "menu-3"}, {"itemId", "play"}}));
    mn::DismissPayload dismiss;
    dismiss.menuId = "menu-3";
    dismiss.reason = "select";
    EXPECT_EQ(ToJson(dismiss), (json{{"menuId", "menu-3"}, {"reason", "select"}}));
    mn::ValueChangedPayload value;
    value.menuId = "menu-3";
    value.itemId = "rating";
    value.value = 4;
    EXPECT_EQ(ToJson(value), (json{{"menuId", "menu-3"}, {"itemId", "rating"}, {"value", 4}}));

    namespace pb = api::playback;
    EXPECT_EQ(std::string_view(pb::events::StopAfterCurrentChanged::kName), "playback:stopAfterCurrentChanged");
    EXPECT_EQ(std::string_view(pb::events::FollowCursorChanged::kName), "playback:followCursorChanged");
    EXPECT_EQ(std::string_view(pb::events::CursorFollowChanged::kName), "playback:cursorFollowChanged");
    pb::StopAfterCurrentChangedPayload stop;
    stop.enabled = true;
    EXPECT_EQ(ToJson(stop), (json{{"enabled", true}}));
    EXPECT_EQ(ToJson(pb::FollowCursorChangedPayload{}), (json{{"enabled", false}}));
    EXPECT_EQ(ToJson(pb::CursorFollowChangedPayload{}), (json{{"enabled", false}}));
}

TEST(ApiSchemaEvents, LibraryJitQueueAndPortEventsKeepTheirKeys) {
    namespace lb = api::library;
    EXPECT_EQ(std::string_view(lb::events::Initialized::kName), "library:initialized");
    EXPECT_EQ(std::string_view(lb::events::ItemsAdded::kName), "library:itemsAdded");
    EXPECT_EQ(std::string_view(lb::events::ItemsRemoved::kName), "library:itemsRemoved");
    EXPECT_EQ(std::string_view(lb::events::ItemsModified::kName), "library:itemsModified");
    lb::InitializedPayload initialized;
    initialized.timestamp = 1790000000000;
    EXPECT_EQ(ToJson(initialized), (json{{"timestamp", 1790000000000}}));
    lb::ItemsAddedPayload added;
    added.count = 12;
    added.timestamp = 1790000000000;
    EXPECT_EQ(ToJson(added), (json{{"count", 12}, {"timestamp", 1790000000000}}));
    EXPECT_EQ(ToJson(lb::ItemsRemovedPayload{}), (json{{"count", 0}, {"timestamp", 0}}));
    EXPECT_EQ(ToJson(lb::ItemsModifiedPayload{}), (json{{"count", 0}, {"timestamp", 0}}));

    namespace jq = api::jitQueue;
    EXPECT_EQ(std::string_view(jq::events::TrackChanged::kName), "jitQueue:trackChanged");
    EXPECT_EQ(std::string_view(jq::events::NeedNext::kName), "jitQueue:needNext");
    EXPECT_EQ(std::string_view(jq::events::ListExhausted::kName), "jitQueue:listExhausted");
    EXPECT_EQ(std::string_view(jq::events::PreloadComplete::kName), "jitQueue:preloadComplete");
    EXPECT_EQ(std::string_view(jq::events::Error::kName), "jitQueue:error");
    // preloadBatch 起播后的曲目没有标识，trackId / title 照旧写成空串而不是缺席。
    EXPECT_EQ(ToJson(jq::TrackChangedPayload{}), (json{{"trackId", ""}, {"title", ""}}));
    jq::NeedNextPayload need;
    need.currentTrackId = "t1";
    need.reason = "trackChange";
    EXPECT_EQ(ToJson(need), (json{{"currentTrackId", "t1"}, {"reason", "trackChange"}}));
    EXPECT_EQ(ToJson(jq::ListExhaustedPayload{}), (json{{"lastTrackId", ""}}));
    jq::PreloadCompletePayload done;
    done.count = 3;
    done.startIndex = 1;
    done.replace = true;
    EXPECT_EQ(ToJson(done), (json{{"count", 3}, {"startIndex", 1}, {"replace", true}}));
    // url 与 path 只写有值的那个。
    jq::ErrorPayload streamError;
    streamError.trackId = "t2";
    streamError.error = "Failed to resolve URL";
    streamError.url = "https://example.invalid/a.mp3";
    EXPECT_EQ(ToJson(streamError), (json{{"trackId", "t2"},
                                         {"error", "Failed to resolve URL"},
                                         {"url", "https://example.invalid/a.mp3"}}));
    jq::ErrorPayload fileError;
    fileError.trackId = "t3";
    fileError.error = "Failed to resolve local file path";
    fileError.path = "C:\\Music\\a.flac";
    EXPECT_EQ(ToJson(fileError), (json{{"trackId", "t3"},
                                       {"error", "Failed to resolve local file path"},
                                       {"path", "C:\\Music\\a.flac"}}));

    namespace pt = api::port;
    EXPECT_EQ(std::string_view(pt::events::Connected::kName), "port:connected");
    EXPECT_EQ(std::string_view(pt::events::Disconnected::kName), "port:disconnected");
    EXPECT_EQ(std::string_view(pt::events::Message::kName), "port:message");
    pt::ConnectedPayload connected;
    connected.portId = "port_00000001";
    connected.name = "sync";
    connected.windowId = "main";
    EXPECT_EQ(ToJson(connected), (json{{"portId", "port_00000001"}, {"name", "sync"}, {"windowId", "main"}}));
    EXPECT_EQ(ToJson(pt::DisconnectedPayload{}), (json{{"portId", ""}, {"name", ""}, {"windowId", ""}}));
    // message 是发送方传入的任意 JSON，数组与标量原样送达。
    pt::MessagePayload message;
    message.portId = "port_00000002";
    message.sourcePortId = "port_00000001";
    message.sourceWindowId = "main";
    message.message = json::array({1, "two"});
    EXPECT_EQ(ToJson(message), (json{{"portId", "port_00000002"},
                                     {"sourcePortId", "port_00000001"},
                                     {"sourceWindowId", "main"},
                                     {"message", json::array({1, "two"})}}));
}

TEST(ApiSchemaEvents, FileAndMetadataEventsKeepTheirKeys) {
    namespace fl = api::file;
    EXPECT_EQ(std::string_view(fl::events::OpProgress::kName), "file:opProgress");
    EXPECT_EQ(std::string_view(fl::events::OpComplete::kName), "file:opComplete");
    // delete 的结果没有 destination；直接完成的条目没有 reason。
    fl::FileOpResultItem deleted;
    deleted.source = "%TEMP%\\a.txt";
    deleted.status = "ok";
    fl::FileOpResultItem skipped;
    skipped.source = "C:\\a.txt";
    skipped.destination = "D:\\a.txt";
    skipped.status = "skipped";
    skipped.reason = "already-exists";
    fl::OpProgressPayload progress;
    progress.operationId = "fileop_1";
    progress.op = "copy";
    progress.done = 2;
    progress.total = 3;
    progress.results = {deleted, skipped};
    EXPECT_EQ(ToJson(progress),
              (json{{"operationId", "fileop_1"},
                    {"op", "copy"},
                    {"done", 2},
                    {"total", 3},
                    {"results", json::array({json{{"source", "%TEMP%\\a.txt"}, {"status", "ok"}},
                                             json{{"source", "C:\\a.txt"},
                                                  {"destination", "D:\\a.txt"},
                                                  {"status", "skipped"},
                                                  {"reason", "already-exists"}}})}}));
    fl::OpCompletePayload complete;
    complete.operationId = "fileop_1";
    complete.op = "move";
    complete.total = 3;
    complete.successCount = 1;
    complete.skippedCount = 1;
    complete.failureCount = 1;
    complete.cancelled = true;
    EXPECT_EQ(ToJson(complete), (json{{"operationId", "fileop_1"},
                                      {"op", "move"},
                                      {"total", 3},
                                      {"successCount", 1},
                                      {"skippedCount", 1},
                                      {"failureCount", 1},
                                      {"cancelled", true}}));

    namespace md = api::metadata;
    EXPECT_EQ(std::string_view(md::events::ProbeProgress::kName), "metadata:probeProgress");
    EXPECT_EQ(std::string_view(md::events::ProbeComplete::kName), "metadata:probeComplete");
    EXPECT_EQ(std::string_view(md::events::WriteComplete::kName), "metadata:writeComplete");
    // 失败行只有 path / success / infoSource / failure；成功行带 info，tags 只在 includeTags 时出现。
    md::MetadataProbeResultItem missing;
    missing.path = "D:\\gone.flac";
    missing.infoSource = "none";
    missing.failure = "not-found";
    md::MetadataProbeResultItem read;
    read.path = "D:\\a.cue|subsong:2";
    read.success = true;
    read.infoSource = "direct";
    read.info = md::TrackTechnicalInfo{215.4, 1000, 44100, 2, "FLAC"};
    read.tags = std::map<std::string, json>{{"ARTIST", json::array({"A", "B"})}, {"DURATION", "215.400"}};
    md::ProbeProgressPayload probe;
    probe.operationId = "probe_1";
    probe.done = 2;
    probe.total = 2;
    probe.results = {missing, read};
    EXPECT_EQ(ToJson(probe),
              (json{{"operationId", "probe_1"},
                    {"done", 2},
                    {"total", 2},
                    {"results", json::array({json{{"path", "D:\\gone.flac"},
                                                  {"success", false},
                                                  {"infoSource", "none"},
                                                  {"failure", "not-found"}},
                                             json{{"path", "D:\\a.cue|subsong:2"},
                                                  {"success", true},
                                                  {"infoSource", "direct"},
                                                  {"info", {{"duration", 215.4},
                                                            {"bitrate", 1000},
                                                            {"sampleRate", 44100},
                                                            {"channels", 2},
                                                            {"codec", "FLAC"}}},
                                                  {"tags", {{"ARTIST", json::array({"A", "B"})},
                                                            {"DURATION", "215.400"}}}}})}}));
    md::ProbeCompletePayload probeDone;
    probeDone.operationId = "probe_1";
    probeDone.total = 5;
    probeDone.successCount = 1;
    probeDone.failureCount = 1;
    probeDone.cancelled = true;
    EXPECT_EQ(ToJson(probeDone), (json{{"operationId", "probe_1"},
                                       {"total", 5},
                                       {"successCount", 1},
                                       {"failureCount", 1},
                                       {"cancelled", true}}));
    md::WriteCompletePayload written;
    written.operation = "removeTag";
    written.path = "D:\\a.flac";
    written.subsong = 0;
    written.code = 2;
    written.status = "error";
    EXPECT_EQ(ToJson(written), (json{{"operation", "removeTag"},
                                     {"path", "D:\\a.flac"},
                                     {"subsong", 0},
                                     {"code", 2},
                                     {"success", false},
                                     {"status", "error"}}));
}

TEST(ApiSchemaEvents, HttpAndLibraryGetAllEventsKeepTheirKeys) {
    namespace ht = api::http;
    EXPECT_EQ(std::string_view(ht::events::Response::kName), "http:response");
    EXPECT_EQ(std::string_view(ht::events::DownloadComplete::kName), "http:downloadComplete");
    // 成功只写响应字段；contentLength 只有 HEAD 才有。
    ht::ResponsePayload ok;
    ok.requestId = "http_1";
    ok.success = true;
    ok.status = 404;
    ok.headers = std::map<std::string, std::string>{{"Content-Length", "0"}};
    ok.body = "";
    ok.responseType = "text";
    ok.contentLength = 0;
    EXPECT_EQ(ToJson(ok), (json{{"requestId", "http_1"},
                                {"success", true},
                                {"status", 404},
                                {"headers", {{"Content-Length", "0"}}},
                                {"body", ""},
                                {"responseType", "text"},
                                {"contentLength", 0}}));
    // 失败与失败的调用同形：error 与 code，取消另带 cancelled。
    ht::ResponsePayload cancelled;
    cancelled.requestId = "http_2";
    cancelled.error = "Request cancelled";
    cancelled.code = "CANCELLED";
    cancelled.cancelled = true;
    EXPECT_EQ(ToJson(cancelled), (json{{"requestId", "http_2"},
                                       {"success", false},
                                       {"error", "Request cancelled"},
                                       {"code", "CANCELLED"},
                                       {"cancelled", true}}));
    ht::DownloadCompletePayload saved;
    saved.requestId = "http_3";
    saved.success = true;
    saved.status = 200;
    saved.bytesWritten = 1024;
    saved.path = "C:\\Temp\\a.bin";
    EXPECT_EQ(ToJson(saved), (json{{"requestId", "http_3"},
                                   {"success", true},
                                   {"status", 200},
                                   {"bytesWritten", 1024},
                                   {"path", "C:\\Temp\\a.bin"}}));
    ht::DownloadCompletePayload refused;
    refused.requestId = "http_4";
    refused.status = 302;
    refused.error = "Redirect not allowed";
    refused.code = "OPERATION_FAILED";
    EXPECT_EQ(ToJson(refused), (json{{"requestId", "http_4"},
                                     {"success", false},
                                     {"status", 302},
                                     {"error", "Redirect not allowed"},
                                     {"code", "OPERATION_FAILED"}}));

    namespace lb = api::library;
    EXPECT_EQ(std::string_view(lb::events::GetAllResult::kName), "library:getAllResult");
    // 失败时两个列表照旧写成空数组，total 为 0。
    lb::GetAllResultPayload failed;
    failed.requestId = "libraryGetAll_1";
    failed.limit = 100;
    failed.error = "boom";
    EXPECT_EQ(ToJson(failed), (json{{"requestId", "libraryGetAll_1"},
                                    {"tracks", json::array()},
                                    {"items", json::array()},
                                    {"total", 0},
                                    {"offset", 0},
                                    {"limit", 100},
                                    {"fromCache", false},
                                    {"error", "boom"}}));
}

TEST(ApiSchemaEvents, AudioEventsKeepTheirKeys) {
    namespace au = api::audio;
    EXPECT_EQ(std::string_view(au::events::Spectrum::kName), "audio:spectrum");
    EXPECT_TRUE(au::events::Spectrum::kCustomName);
    EXPECT_FALSE(au::events::PcmReady::kCustomName);

    // 频带帧不写频点帧才有的 left / right / firstBin / channels / channelCount。
    au::SpectrumPayload bands;
    bands.subscriptionId = "spectrum_1";
    bands.output = "bands";
    bands.spectrum = std::vector<double>{0.5, 0.25};
    bands.bands = 2;
    bands.fftSize = 1024;
    bands.scale = "weighted";
    bands.sampleRate = 44100;
    bands.minFrequency = 20;
    bands.maxFrequency = 22050;
    bands.state = "playing";
    bands.streamTime = 1.5;
    bands.hostTime = 1000.25;
    EXPECT_EQ(ToJson(bands), (json{{"subscriptionId", "spectrum_1"},
                                   {"output", "bands"},
                                   {"spectrum", {0.5, 0.25}},
                                   {"bands", 2},
                                   {"fftSize", 1024},
                                   {"scale", "weighted"},
                                   {"sampleRate", 44100},
                                   {"minFrequency", 20.0},
                                   {"maxFrequency", 22050.0},
                                   {"state", "playing"},
                                   {"streamTime", 1.5},
                                   {"hostTime", 1000.25}}));

    au::FullWaveformReadyPayload ready;
    ready.taskId = "wf_1";
    ready.path = "D:/music/a.flac";
    ready.waveform = {0.5, 1.0};
    ready.maxAmplitude = 0.75;
    ready.duration = 2.0;
    ready.sampleRate = 48000;
    ready.channels = 2;
    ready.resolution = 2;
    ready.method = "peak";
    ready.scale = "linear";
    ready.signed_ = false;
    EXPECT_EQ(ToJson(ready), (json{{"taskId", "wf_1"},
                                   {"path", "D:/music/a.flac"},
                                   {"waveform", {0.5, 1.0}},
                                   {"maxAmplitude", 0.75},
                                   {"duration", 2.0},
                                   {"sampleRate", 48000},
                                   {"channels", 2},
                                   {"resolution", 2},
                                   {"method", "peak"},
                                   {"scale", "linear"},
                                   {"signed", false},
                                   {"cached", false}}));

    // 两个失败事件与原先 MakeFailureEvent 的四个键相同。
    au::FullWaveformFailedPayload waveFailed;
    waveFailed.taskId = "wf_2";
    waveFailed.path = "D:/music/b.flac";
    waveFailed.error = "Cancelled";
    waveFailed.code = "CANCELLED";
    const json failedShape{{"taskId", "wf_2"}, {"path", "D:/music/b.flac"}, {"error", "Cancelled"}, {"code", "CANCELLED"}};
    EXPECT_EQ(ToJson(waveFailed), failedShape);
    au::PcmFailedPayload pcmFailed;
    pcmFailed.taskId = "wf_2";
    pcmFailed.path = "D:/music/b.flac";
    pcmFailed.error = "Cancelled";
    pcmFailed.code = "CANCELLED";
    EXPECT_EQ(ToJson(pcmFailed), failedShape);

    au::PcmReadyPayload pcm;
    pcm.taskId = "pcm_1";
    pcm.path = "D:/music/c.flac";
    pcm.sampleRate = 44100;
    pcm.channels = 1;
    pcm.frames = 44100;
    pcm.start = 1.0;
    pcm.end = 2.0;
    pcm.duration = 1.0;
    pcm.truncated = false;
    pcm.resampled = true;
    EXPECT_EQ(ToJson(pcm), (json{{"taskId", "pcm_1"},
                                 {"path", "D:/music/c.flac"},
                                 {"sampleRate", 44100},
                                 {"channels", 1},
                                 {"frames", 44100},
                                 {"start", 1.0},
                                 {"end", 2.0},
                                 {"duration", 1.0},
                                 {"truncated", false},
                                 {"resampled", true}}));

    au::StreamPayload stream;
    stream.subscriptionId = "stream_1";
    stream.type = "ended";
    stream.epoch = 3;
    stream.reason = "format-change";
    EXPECT_EQ(ToJson(stream), (json{{"subscriptionId", "stream_1"},
                                    {"type", "ended"},
                                    {"epoch", 3},
                                    {"reason", "format-change"}}));

    // 输出设备事件原先显式发空对象，声明为无载荷后仍是空对象。
    EXPECT_EQ(ToJson(au::OutputDeviceChangedPayload{}), json::object());
    au::ReplaygainModeChangedPayload rg;
    rg.mode = 2;
    EXPECT_EQ(ToJson(rg), (json{{"mode", 2}}));
}

TEST(ApiSchemaEvents, PlaylistEventsKeepTheirKeys) {
    namespace pl = api::playlist;
    EXPECT_EQ(std::string_view(pl::events::ItemsAdded::kName), "playlist:itemsAdded");
    const std::string guid = "{12345678-1111-2222-3031-323334353637}";
    pl::ItemsAddedPayload added;
    added.playlist = 1;
    added.playlistGuid = guid;
    added.start = 5;
    added.count = 2;
    EXPECT_EQ(ToJson(added), (json{{"playlist", 1}, {"playlistGuid", guid}, {"start", 5}, {"count", 2}}));
    // 没有焦点行或活动列表时写 -1。
    pl::FocusChangedPayload focus;
    focus.playlist = 0;
    focus.playlistGuid = guid;
    focus.from = -1;
    focus.to = 3;
    EXPECT_EQ(ToJson(focus), (json{{"playlist", 0}, {"playlistGuid", guid}, {"from", -1}, {"to", 3}}));
    // 没有活动列表时 newGuid 写 null，键必在。
    pl::ActivatedPayload activated;
    activated.oldIndex = 0;
    activated.newIndex = -1;
    EXPECT_EQ(ToJson(activated), (json{{"oldIndex", 0}, {"newIndex", -1}, {"newGuid", nullptr}}));
    activated.newIndex = 2;
    activated.newGuid = guid;
    EXPECT_EQ(ToJson(activated), (json{{"oldIndex", 0}, {"newIndex", 2}, {"newGuid", guid}}));
    pl::CreatedPayload created;
    created.index = 2;
    created.guid = guid;
    created.name = "New";
    EXPECT_EQ(ToJson(created), (json{{"index", 2}, {"guid", guid}, {"name", "New"}}));
    pl::RemovedPayload removed;
    removed.oldCount = 3;
    removed.newCount = 1;
    removed.indices = {0, 2};
    removed.guids = {guid, guid};
    EXPECT_EQ(ToJson(removed),
              (json{{"oldCount", 3}, {"newCount", 1}, {"indices", {0, 2}}, {"guids", {guid, guid}}}));
    pl::ReorderedPayload reordered;
    reordered.count = 1;
    reordered.guids = {guid};
    EXPECT_EQ(ToJson(reordered), (json{{"count", 1}, {"guids", {guid}}}));
    pl::LockChangedPayload lock;
    lock.playlist = 4;
    lock.playlistGuid = guid;
    lock.locked = true;
    EXPECT_EQ(ToJson(lock), (json{{"playlist", 4}, {"playlistGuid", guid}, {"locked", true}}));
    pl::AddCompletePayload done;
    done.operationId = "op_1";
    done.playlistGuid = guid;
    done.success = true;
    done.addedCount = 3;
    done.totalCount = 4;
    EXPECT_EQ(ToJson(done), (json{{"operationId", "op_1"}, {"playlistGuid", guid}, {"success", true},
                                  {"addedCount", 3}, {"totalCount", 4}}));

    namespace pb = api::playback;
    pb::OrderChangedPayload order;
    order.orderIndex = 3;
    order.order = 3;
    EXPECT_EQ(ToJson(order), (json{{"orderIndex", 3}, {"order", 3}}));
}

TEST(ApiSchemaEvents, PlaybackEventsKeepTheirKeys) {
    namespace pb = api::playback;
    EXPECT_EQ(std::string_view(pb::events::TimeHighRes::kName), "playback:timeHighRes");
    pb::StoppedPayload stopped;
    stopped.reason = "eof";
    EXPECT_EQ(ToJson(stopped), (json{{"reason", "eof"}}));
    pb::VolumeChangedPayload volume;
    volume.volume = 50;
    volume.volumeDb = -6;
    volume.muted = false;
    volume.isMuted = false;
    EXPECT_EQ(ToJson(volume), (json{{"volume", 50.0}, {"volumeDb", -6.0}, {"muted", false}, {"isMuted", false}}));
    // 没有标题时不写 streamTitle；dynamicInfoTrack 两个字段都可缺。
    pb::DynamicInfoPayload dynamic;
    dynamic.bitrate = 320;
    EXPECT_EQ(ToJson(dynamic), (json{{"bitrate", 320}}));
    pb::DynamicInfoTrackPayload track;
    track.title = "Song";
    EXPECT_EQ(ToJson(track), (json{{"title", "Song"}}));
    pb::StartingPayload starting;
    starting.command = "previous";
    starting.paused = true;
    EXPECT_EQ(ToJson(starting), (json{{"command", "previous"}, {"paused", true}}));
    pb::StateChangedPayload state;
    state.state = "paused";
    state.position = 1.5;
    state.duration = 200;
    state.canSeek = true;
    state.hostTime = 1758800000123.5;
    EXPECT_EQ(ToJson(state), (json{{"state", "paused"},
                                   {"position", 1.5},
                                   {"duration", 200.0},
                                   {"canSeek", true},
                                   {"hostTime", 1758800000123.5}}));
    // 带读取时刻的其余两个事件：seeked 的 position 是跳转目标，timeHighRes 是读到的位置。
    pb::SeekedPayload seeked;
    seeked.position = 30;
    seeked.hostTime = 1758800000200.25;
    EXPECT_EQ(ToJson(seeked), (json{{"position", 30.0}, {"hostTime", 1758800000200.25}}));
    pb::TimeHighResPayload tick;
    tick.position = 30.1;
    tick.hostTime = 1758800000300.75;
    EXPECT_EQ(ToJson(tick), (json{{"position", 30.1}, {"hostTime", 1758800000300.75}}));
    pb::QueueChangedPayload queue;
    queue.origin = "user_added";
    queue.count = 2;
    EXPECT_EQ(ToJson(queue), (json{{"origin", "user_added"}, {"count", 2}}));
}

TEST(ApiSchemaEvents, TrackEventsSendTheSharedTrackAndEmptyEventsAnObject) {
    namespace pb = api::playback;
    static_assert(std::is_same_v<pb::events::TrackChanged::Payload, api::common::Track>);
    static_assert(std::is_same_v<pb::events::Edited::Payload, api::common::Track>);
    static_assert(std::is_same_v<pb::events::ItemPlayed::Payload, api::common::Track>);
    EXPECT_EQ(std::string_view(pb::events::ItemPlayed::kName), "playback:itemPlayed");

    // 旧的曲目载荷带 id 与 fullPath；Track 用 handle 代替两者。
    api::common::Track track;
    track.handle = "D:/m/a.cue|subsong:2";
    track.subsong = 2;
    const json row = ToJson(track);
    EXPECT_EQ(row.at("handle"), "D:/m/a.cue|subsong:2");
    EXPECT_FALSE(row.contains("id"));
    EXPECT_FALSE(row.contains("fullPath"));

    // 这两个事件原先发 null，现在与其他无字段的事件一样发空对象。
    EXPECT_EQ(ToJson(api::audio::DspPresetChangedPayload{}), json::object());
    EXPECT_EQ(ToJson(api::playlist::DefaultFormatChangedPayload{}), json::object());
}

TEST(ApiSchemaEvents, SelectionChangedWritesTrackAndNowPlayingOnlyWhenPresent) {
    EXPECT_EQ(std::string_view(api::selection::events::Changed::kName), "selection:changed");
    api::selection::ChangedPayload changed;
    changed.count = 2;
    changed.type = "active_playlist_selection";
    changed.handles = {"D:/m/a.flac", "D:/m/b.flac"};
    changed.truncated = false;
    EXPECT_EQ(ToJson(changed), (json{{"count", 2},
                                     {"type", "active_playlist_selection"},
                                     {"handles", {"D:/m/a.flac", "D:/m/b.flac"}},
                                     {"truncated", false}}));

    changed.count = 1;
    changed.handles = {"D:/m/a.flac"};
    changed.track = api::common::Track{};
    changed.track->handle = "D:/m/a.flac";
    const json one = ToJson(changed);
    EXPECT_EQ(one.at("track").at("handle"), "D:/m/a.flac");
    EXPECT_FALSE(one.contains("nowPlaying"));
}
