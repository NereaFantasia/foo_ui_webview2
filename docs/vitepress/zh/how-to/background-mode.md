# 与其他界面同时运行 WebView2 UI

后台模式让 Default UI 或 Columns UI 作为主界面时，仍有一个 WebView2 UI 窗口可用，例如当作歌词或封面的辅助窗口，或者在 Default UI 里管理播放列表的同时显示可视化效果。

## 打开后台模式

1. 打开 `File → Preferences → Display → WebView2 UI` 下的 `窗口` 页，勾选 `后台模式：使用其他界面时仍运行 WebView`，按 `应用`。组件语言为英文时显示为 `Background mode: keep the WebView running while another user interface is active`。
2. 在 `File → Preferences → Display → Default User Interface` 里选择另一个界面。
3. 重启 foobar2000。后台模式在重启后生效。
4. 用 `View → WebView2 UI → Show/Hide Window` 显示窗口。

这个窗口和主窗口一样加载全局模板。在同一页勾选 `启动时恢复后台窗口可见状态`，foobar2000 启动时窗口就按上次离开时的状态出现。

## 菜单项显示为「偏好设置...」时

没有 WebView2 UI 窗口时，例如另一个界面在用且后台模式没开，`View → WebView2 UI` 下的菜单项显示为 `偏好设置...`，点它会打开 WebView2 UI 的偏好设置页。

## 让页面在隐藏时也正常

窗口最小化、隐藏到托盘或锁屏时，页面的 `document.visibilityState` 变为 `hidden`：`requestAnimationFrame` 停止，定时器被大幅节流。用事件（`fb2k.on(...)` 或 `fb.on(...)`）更新页面，不要用轮询循环，这样窗口回来时页面状态才是对的。通过 CDP 或 MCP 自动化播放器时，[MCP 配置指南](/zh/mcp/setup)里的 keep-alive 开关能让页面在这些状态下保持活跃。
