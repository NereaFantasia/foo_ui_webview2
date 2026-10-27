# 与其他界面同时运行 WebView2 UI

启用后台模式后，在保持 Default UI 或 Columns UI 为活动主界面的同时，仍可唤出独立的 WebView2 UI 辅助窗口，适用于桌面歌词联动、封面画廊展示或音频频谱可视化等独立场景。

## 启用后台模式 {#打开后台模式}

1. 打开 `File → Preferences → Display → WebView2 UI` 下的 `窗口` 页，勾选 `后台模式：使用其他界面时仍运行 WebView`，单击 `应用`。组件语言为英文时该选项显示为 `Background mode: keep the WebView running while another user interface is active`。
2. 在 `File → Preferences → Display → Default User Interface` 中选择另一个活动界面。
3. 重启 foobar2000。后台模式在重启后正式生效。
4. 使用 `View → WebView2 UI → Show/Hide Window` 显隐该辅助窗口。

该辅助窗口与主窗口一致，默认加载全局活动模板。在同一页勾选 `启动时恢复后台窗口可见状态`，foobar2000 启动时将自动恢复上次退出时的窗口显示状态。

## 菜单项显示为「偏好设置...」时的行为说明 {#菜单项显示为「偏好设置-」时}

当当前不存在活动的 WebView2 UI 窗口时（例如主界面切换为其他 UI 且后台模式未开启），`View → WebView2 UI` 下的菜单项将显示为 `偏好设置...`，单击将直接导航至 WebView2 UI 偏好设置页面。

## 页面在后台隐藏状态下的事件与调度考量 {#让页面在隐藏时也正常}

当窗口最小化、隐入系统托盘或系统锁屏时，WebView 页面的 `document.visibilityState` 转为 `hidden`：浏览器内核将自动暂停 `requestAnimationFrame` 并大幅节流 `setInterval`/`setTimeout` 定时器。推荐采用事件驱动模式（`fb.on(...)` 或 `fb2k.on(...)`）监听状态变更，避免使用高频轮询；当窗口恢复可见时，视图能即时同步最新状态。若通过 CDP 或 MCP 开展自动化控制，可参考 [MCP 配置指南](/zh/mcp/setup) 启用保活机制维持后台渲染心跳。
