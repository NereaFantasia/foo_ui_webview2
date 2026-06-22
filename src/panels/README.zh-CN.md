[English](./README.md) | 中文

# src/panels/ — DUI / CUI 面板集成

`panels/` 让 foo_ui_webview2 不止能整窗接管 foobar2000，还能作为一个普通面板嵌入既有布局：在 Default UI（DUI）里作为 `ui_element`，在 Columns UI（CUI）里作为 `uie::window`。本模块负责这两套宿主协议的对接、面板配置的持久化，以及共用的配置对话框。

---

## 概述

主窗口模式是「一个 WebView 占满整窗」；面板模式则是「一个布局里可以放多个 WebView 面板」。差异主要在宿主协议与生命周期，而 WebView2 + 桥接的通用能力完全复用 `core/WebViewPanel` 基类。因此每个面板实例：

- 继承 `WebViewPanel` 拿到 WebView2、API 注册、消息处理、`SelectionHolder`；
- 各自持有 **per-instance `BridgeCore`**（不走单例），经 `core/WebViewContext` 按 HWND 登记，实现多面板互不串扰；
- 用 `PanelConfig` 描述自己的配置，并按各自宿主的协议持久化。

```
WebViewPanel (core/)
     ├── WebViewDuiElementInstance : ui_element_instance + WebViewPanel   (DUI)
     └── WebViewCuiPanel           : uie::window         + WebViewPanel   (CUI)
```

---

## 关键文件 / 类

| 文件 | 职责 |
|------|------|
| `PanelConfig.h/.cpp` | 面板配置数据结构（v2，向后兼容 v1）：`templateName`、`panelName`、`edgeStyle`、`transparentBackground`、`grabFocus`、`enableDragDrop`、`enableDevTools`、`urlOverride`；提供 `Serialize/Deserialize`（二进制）与 `Write/Read`（stream，CUI 用）双套序列化，及配置比较（热重载检测用） |
| `WebViewDuiElement.h/.cpp` | DUI 集成：工厂 `WebViewDuiElement : ui_element` + 实例 `WebViewDuiElementInstance : ui_element_instance, WebViewPanel`；配置经 `ui_element_config`；处理 DUI 编辑模式占位绘制 |
| `WebViewCuiPanel.h/.cpp` | CUI 集成：`WebViewCuiPanel : uie::window, WebViewPanel`；`create_or_transfer_window` 创建窗口；配置经 `stream_reader/writer` |
| `PanelConfigDialog.h/.cpp` | DUI/CUI 共用的模态配置对话框 `ShowPanelConfigDialog(parent, config)`：模板选择（含「跟随全局设置」）+ 当前资源路径显示 |

> 两个面板类都有各自固定的 GUID（`g_webview_dui_element_guid` / `g_webview_cui_panel_guid`），对外显示名均为 "WebView2 Panel"。

---

## DUI vs CUI：两套集成差异

| 维度 | DUI（Default UI） | CUI（Columns UI） |
|------|-------------------|-------------------|
| 宿主接口 | `ui_element` / `ui_element_instance` | `uie::window`（`uie::type_panel`） |
| 创建入口 | `instantiate(parent, cfg, callback)` | `create_or_transfer_window(parent, host, position)` |
| 配置载体 | `ui_element_config::ptr`（`get/set_configuration`） | `stream_reader/writer`（`get_config`/`set_config`） |
| 配置弹窗 | `notify` 收到编辑请求 → `ShowConfigDialog()` | `have_config_popup()` + `show_config_popup()` |
| 编辑模式 | 有 DUI 编辑模式占位绘制 | 无（CUI 无等价编辑态） |
| 尺寸约束 | `get_min_max_info()` | 由宿主 `window_position_t` 决定 |
| 依赖 SDK | foobar2000 SDK | Columns UI SDK（`ui_extension.h`，git submodule） |

两者最终都落到同一个 `PanelConfig` 与同一个 `WebViewPanel` 基类，差异被收敛在「协议适配 + 序列化格式」这一薄层。

---

## 工作原理 / 数据流

### 面板创建与登记

```
宿主布局创建面板
   ├─ DUI: WebViewDuiElement::instantiate → WebViewDuiElementInstance::Initialize(parent)
   └─ CUI: WebViewCuiPanel::create_or_transfer_window(parent, host, position)
        └─ WebViewPanel::InitializeWebView(hwnd, DuiPanel | CuiPanel)
              ├─ new BridgeCore  (per-instance)
              ├─ RegisterAllApis()
              └─ WebViewContext::RegisterInstance(hwnd, host, bridge, windowId, panel)
                    （此后事件按 HWND/windowId 精确路由，多面板互不串扰）
```

### 配置持久化与热重载

```
用户改面板配置 (配置对话框)
   └─ PanelConfigDialog::ShowPanelConfigDialog(parent, config)  →  新 PanelConfig
        ├─ DUI: set_configuration(ui_element_config)  → PanelConfig::Deserialize
        └─ CUI: set_config(stream_reader)            → PanelConfig::Read
              └─ WebViewPanel::ApplyConfig(oldCfg, newCfg)
                    （对比差异，按需重载页面 / 调整边框 / 切换透明背景，避免无谓全量重载）
```

`PanelConfig` 的二进制格式带版本号（当前 v2），读取旧版本数据时缺失字段用默认值填充，保证向后兼容。

---

## 依赖关系

- **依赖**：`core/WebViewPanel`（基类）、`core/WebViewContext`（per-instance 登记与事件路由）、`api/BridgeCore`（每实例独立桥接）、`utils/I18n`（`TRU` 双语描述）、foobar2000 SDK 与 Columns UI SDK。
- **被依赖**：`main.cpp` 通过各自的 service factory 注册 `WebViewDuiElement` / `WebViewCuiPanel`，使其出现在 DUI/CUI 的「添加面板」列表中。

---

## 扩展指南

- **新增面板配置项**：在 `PanelConfig` 加字段并 **提升 `CURRENT_VERSION`**，同步更新 `Serialize/Deserialize` 与 `Write/Read`，并在 `operator==` 里纳入比较（否则热重载检测不到变化）；旧版本反序列化要给默认值。
- **新增配置 UI**：在 `PanelConfigDialog` 增控件，DUI/CUI 共用，避免两边各写一套。
- **面板专属行为**：优先用 `PanelConfig` 字段（如 `grabFocus`/`enableDevTools`）驱动，避免在面板类里堆布尔分支；通用能力应下沉到 `WebViewPanel` 基类让三种模式共享。

---

参见：[../core/README.zh-CN.md](../core/README.zh-CN.md)（`WebViewPanel` 基类与 `WebViewContext`）、[../api/README.zh-CN.md](../api/README.zh-CN.md)（per-instance BridgeCore）、仓库根 [README.md](../../README.md)。
