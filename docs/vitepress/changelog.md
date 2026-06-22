# 更新日志

## v1.9.0 (2026-06-18)

- 托盘自绘菜单（`render: 'webview'`）普通/子菜单项支持图标：新增 `TrayMenuItem.iconSvg = { viewBox, content }`，内联单色 SVG、`currentColor` 跟随菜单文字色、左对齐固定 8px 间距；同层有图标时所有普通/子菜单项预留 16px 图标列以对齐文字（`native` 菜单忽略）
- `tray.setContextMenu` 新增 `config.autoNowPlaying`：开启后 `nowplaying` 项的空字段（cover/title/subtitle）在右键弹出时由后端用当前曲目自动兜底（前端传了就用前端值，**前端优先**）；`cover` 兜底仅 `webview`，取当前曲目封面并缩略为缩略图，title/subtitle 走 `%title%`（自动回退文件名）/`%artist%`，兼容流媒体动态标题
- `TrayMenuItem.cover` 现额外支持 `http(s)://` URL（除既有 `data:` 与裸 base64 外），便于流媒体前端直传实时解析的封面
- SDK 安装包同步到 `1.9.0`

## v1.8.0 (2026-06-10)

- 新增自绘菜单能力：`menu.show` / `menu.close` 以 WebView 渲染菜单内容，支持子菜单递归渲染；菜单窗口采用内容尺寸固定窗策略，消除展开时的闪烁
- `tray.*` 新增 `render: 'webview'` 模式，托盘右键菜单可改用自绘菜单渲染，外观与主题保持统一
- 新增 `tray.setMenuItemState`，可在不重建整个菜单的情况下更新单个菜单项状态
- 修复桌面歌词等置顶弹窗点击后，主窗口偶发被拉至前台并意外置顶的问题（撤销路径 z-order 插入误入 topmost 段 + sink 还原参照反馈环）
- 修复 SDK 发布物缺失 `HTMLElementTagNameMap` 全局声明的问题，npm 用户恢复 `fb-*` 自定义元素的类型提示
- 修复打包脚本在新版 PowerShell 下无法生成 `.fb2k-component` 的兼容性问题
- HttpApi 异步请求异常边界加固；修复 LibraryApi 一处 NUL 字符串处理缺陷
- SDK 安装包同步到 `1.8.0`；`bump-version.ps1` 版本同步覆盖面扩展到 `sdk/package-lock.json` 与 VitePress 导航栏版本号

## v1.7.0 (2026-06-06)

- 新增 Taskbar & Tray 能力：`taskbar.*` 可设置任务栏缩略图按钮、进度条、叠加图标和闪烁提示；`tray.*` 可创建系统托盘图标、气泡通知和右键菜单
- `tray.*` 新增增量菜单管理：`appendMenuItems` / `removeMenuItems` / `clearMenuItems` / `getMenuItems`，可以按 `top` / `playback` / `bottom` 分区动态维护托盘菜单，不必每次重建完整菜单
- 新增 `taskbar:buttonClicked`、`tray:click`、`tray:doubleClick`、`tray:menuItemClicked`、`tray:beforeContextMenu` 事件，主题可以响应任务栏缩略图按钮和托盘交互
- 新增 `webview:processFailed` 事件，WebView2 渲染进程异常时会广播诊断信息，并配合渲染进程自动恢复路径降低空白窗口风险
- 新增高精度播放位置事件 `playback:timeHighRes`，由独立 WinAPI 定时器驱动，适合歌词、进度条等需要亚秒级刷新的界面
- `library.getAll` 冷缓存全量序列化改为后台线程执行；SDK 会等待 `library:getAllResult` 并按 `requestId` 关联结果，避免大媒体库查询卡住 UI
- 修复托盘隐藏后窗口恢复路径，补全 `window.focus` / hidden restore 场景下的 WebView surface 恢复，降低最小化、托盘隐藏、Alt+Tab 切回后的空白风险
- 修复任务栏缩略图 pause 图标 base64 损坏和 HICON 所有权问题，避免播放按钮显示异常和 explorer.exe 崩溃
- SDK 安装包同步到 `1.7.0`，可直接使用新的 Taskbar & Tray 类型和事件声明
- VitePress 文档新增 Taskbar & Tray API 页面，并同步 Cursor、Playback 高频事件和相关示例

## v1.6.1 (2026-05-20)

- 新增 `cursor.*` 命名空间：`cursor.setHidden(hidden)` / `cursor.isHidden()` 显式控制客户区光标可见性；解决 Visual Hosting 模式下 CSS `cursor: none` 不可靠的问题
- 新增 `cursor:hiddenChanged` 事件，每窗口独立派发
- `fb.http.*` 新增 `insecureTls` 参数（双层门禁）— 全局开关 `Allow self-signed / invalid TLS certificates` ON + 每请求 `insecureTls: true` 才生效，访问自签证书的内网服务（Plex / Jellyfin / Lidarr 等）不再被强制拦截
- `fb.http.*` 新增 `responseType: 'arraybuffer' | 'binary'`，body 自动 base64 解码为 `ArrayBuffer`，封面 / 字体等二进制资源不再因 UTF-8 严格校验失败
- VitePress 文档同步上述变化（cursor.md / http.md / events.md）

## v1.6.0 (2026-05-11)

- `playlist.getAll` 不再返回 `duration` 字段，避免为了时长把整批轨道都读一遍；`playlist.getActive` / `playlist.getPlaying` 还是会返回
- `http.get` / `http.post` / `http.head` 默认改成异步；如果你就是要同步调用，需要显式传 `async: false`

## v1.1.17 (2026-02-06) 

- 可以真正开多个窗口了
- 新增 `window.createPopup` / `closePopup` / `closeAllPopups` / `getAllWindows`
- 新增 `window.sendMessage` / `window.broadcast`，窗口之间可以互相发消息
- 支持异步关闭、无标题栏和透明背景

## v1.1.16 (2026-02-06)
