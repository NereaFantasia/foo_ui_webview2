English | [中文](./README.zh-CN.md)

# src/panels/ — DUI / CUI Panel Integration

`panels/` lets foo_ui_webview2 not only take over the entire foobar2000 window but also embed into an existing layout as an ordinary panel: as a `ui_element` in the Default UI (DUI), and as a `uie::window` in Columns UI (CUI). This module is responsible for interfacing with these two host protocols, persisting panel configuration, and the shared configuration dialog.

---

## Overview

The main-window mode is "one WebView fills the entire window"; the panel mode is "multiple WebView panels can be placed in one layout". The differences lie mainly in the host protocol and lifecycle, while the common WebView2 + bridge capabilities fully reuse the `core/WebViewPanel` base class. Therefore each panel instance:

- inherits `WebViewPanel` to gain WebView2, API registration, message handling, and `SelectionHolder`;
- each holds its own **per-instance `BridgeCore`** (not the singleton), registered by HWND through `core/WebViewContext`, so multiple panels never cross-talk;
- describes its own configuration with `PanelConfig` and persists it according to its host's protocol.

```
WebViewPanel (core/)
     ├── WebViewDuiElementInstance : ui_element_instance + WebViewPanel   (DUI)
     └── WebViewCuiPanel           : uie::window         + WebViewPanel   (CUI)
```

---

## Key Files / Classes

| File | Responsibility |
|------|------|
| `PanelConfig.h/.cpp` | Panel config data structure (v2, backward compatible with v1): `templateName`, `panelName`, `edgeStyle`, `transparentBackground`, `grabFocus`, `enableDragDrop`, `enableDevTools`, `urlOverride`; provides two serialization sets, `Serialize/Deserialize` (binary) and `Write/Read` (stream, used by CUI), plus config comparison (for hot-reload detection) |
| `WebViewDuiElement.h/.cpp` | DUI integration: the factory `WebViewDuiElement : ui_element` + the instance `WebViewDuiElementInstance : ui_element_instance, WebViewPanel`; config goes through `ui_element_config`; handles DUI edit-mode placeholder drawing |
| `WebViewCuiPanel.h/.cpp` | CUI integration: `WebViewCuiPanel : uie::window, WebViewPanel`; `create_or_transfer_window` creates the window; config goes through `stream_reader/writer` |
| `PanelConfigDialog.h/.cpp` | The modal config dialog `ShowPanelConfigDialog(parent, config)` shared by DUI/CUI: template selection (including "follow global settings") + display of the current resource path |

> Both panel classes have their own fixed GUIDs (`g_webview_dui_element_guid` / `g_webview_cui_panel_guid`), and both display externally as "WebView2 Panel".

---

## DUI vs CUI: Two Integration Differences

| Aspect | DUI (Default UI) | CUI (Columns UI) |
|------|-------------------|-------------------|
| Host interface | `ui_element` / `ui_element_instance` | `uie::window` (`uie::type_panel`) |
| Creation entry | `instantiate(parent, cfg, callback)` | `create_or_transfer_window(parent, host, position)` |
| Config carrier | `ui_element_config::ptr` (`get/set_configuration`) | `stream_reader/writer` (`get_config`/`set_config`) |
| Config popup | `notify` receives an edit request → `ShowConfigDialog()` | `have_config_popup()` + `show_config_popup()` |
| Edit mode | Has DUI edit-mode placeholder drawing | None (CUI has no equivalent edit state) |
| Size constraints | `get_min_max_info()` | Determined by the host's `window_position_t` |
| SDK dependency | foobar2000 SDK | Columns UI SDK (`ui_extension.h`, a git submodule) |

Both ultimately land on the same `PanelConfig` and the same `WebViewPanel` base class; the differences are confined to the thin layer of "protocol adaptation + serialization format".

---

## How It Works / Data Flow

### Panel Creation & Registration

```
host layout creates panel
   ├─ DUI: WebViewDuiElement::instantiate → WebViewDuiElementInstance::Initialize(parent)
   └─ CUI: WebViewCuiPanel::create_or_transfer_window(parent, host, position)
        └─ WebViewPanel::InitializeWebView(hwnd, DuiPanel | CuiPanel)
              ├─ new BridgeCore  (per-instance)
              ├─ RegisterAllApis()
              └─ WebViewContext::RegisterInstance(hwnd, host, bridge, windowId, panel)
                    (from now on, events are routed precisely by HWND/windowId, and panels never cross-talk)
```

### Config Persistence & Hot-Reload

```
user changes panel config (config dialog)
   └─ PanelConfigDialog::ShowPanelConfigDialog(parent, config)  →  new PanelConfig
        ├─ DUI: set_configuration(ui_element_config)  → PanelConfig::Deserialize
        └─ CUI: set_config(stream_reader)            → PanelConfig::Read
              └─ WebViewPanel::ApplyConfig(oldCfg, newCfg)
                    (diff the changes, reload the page / adjust the border / toggle the transparent background as needed, avoiding pointless full reloads)
```

`PanelConfig`'s binary format carries a version number (currently v2); when reading older-version data, missing fields are filled with default values, ensuring backward compatibility.

---

## Dependencies

- **Depends on**: `core/WebViewPanel` (base class), `core/WebViewContext` (per-instance registration and event routing), `api/BridgeCore` (a separate bridge per instance), `utils/I18n` (the `TRU` bilingual descriptions), and the foobar2000 SDK and Columns UI SDK.
- **Depended on by**: `main.cpp` registers `WebViewDuiElement` / `WebViewCuiPanel` via their respective service factories, making them appear in the DUI/CUI "add panel" lists.

---

## Extension Guide

- **Add a panel config field**: add the field in `PanelConfig` and **bump `CURRENT_VERSION`**, update `Serialize/Deserialize` and `Write/Read` in sync, and include it in the `operator==` comparison (otherwise hot-reload won't detect the change); deserialization of older versions must provide a default value.
- **Add config UI**: add controls in `PanelConfigDialog`, shared by DUI/CUI, to avoid writing a separate set on each side.
- **Panel-specific behavior**: prefer driving it with `PanelConfig` fields (such as `grabFocus`/`enableDevTools`) rather than piling boolean branches into the panel classes; common capabilities should sink down to the `WebViewPanel` base class so the three modes can share them.

---

See also: [../core/README.md](../core/README.md) (the `WebViewPanel` base class and `WebViewContext`), [../api/README.md](../api/README.md) (per-instance BridgeCore), and the repository root [README.md](../../README.md).
