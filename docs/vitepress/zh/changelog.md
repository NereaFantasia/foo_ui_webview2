# 更新日志

## 未发布

### Discovery 菜单

- **行为变更** —— `discovery.searchCommands` 现在同时搜索右键菜单与主菜单，因此结果数增加，`type` 出现新取值 `'contextmenu'`。传 `{ scope: 'mainmenu' }` 可回到原有覆盖面。该端点此前只查主菜单，却把 `type: 'mainmenu'` 硬编码进每条结果，导致右键命令搜不到、且该字段不携带任何信息。
- **行为变更** —— `discovery.searchCommands` 会过滤宿主不会显示的条目，与列举端点保持一致。传 `{ includeHidden: true }` 可取回完整集合。
- **行为变更** —— `discovery.getAllServices` 将右键菜单命令计入 `services.contextMenuCommands` 并纳入 `totalServices`，因此 `totalServices` 数值变化。该汇总此前声称描述可发现面，却漏掉了两个菜单族之一。被过滤掉的数量由 `contextMenuHiddenFiltered` 给出；无选中且无播放曲目时 `stateKnown` 为 `false`。
- 搜索结果携带与列举端点一致的状态字段（`enabled`、`checked`、`radioChecked`、`hidden`、`stateKnown`、`flags`、`source`、`executable`、`unaddressableReason`），调用方无需再发一次请求即可判断能否执行。搜索包含右键菜单但无选中且无播放曲目时，响应的 `stateKnown` 为 `false`，此时这些结果的 `enabled` / `checked` 不得用于过滤。
- 搜索的大小写折叠改为仅作用于 ASCII。此前实现对每个字节调用 `::tolower`，对 `0x80` 及以上的字节属未定义行为，可能破坏 UTF-8 序列。对中文标签的可观测匹配行为不变——中文没有可折叠的大小写。
- **`discovery.getContextMenuTree` 不再静默截断。** 此前遍历把子项截到 50、深度截到 10，却仍上报宿主的真实 `childCount`，导致 `children.length` 与 `childCount` 不符且无任何说明。两个上限现统一取自共享菜单契约（深度 16、每节点 512 子项），且任何裁剪都会上报：`popup` 节点同时给出 `childCount` 与 `childrenReturned`，子树被裁剪的节点带 `truncated`，并由 `depthExceeded` / `childrenExceeded` 说明原因。标记向上传播，因此顶层 `truncated` 覆盖整棵树；`maxDepth` 与 `maxChildrenPerNode` 回显生效上限。
- `discovery.getContextMenuTree` 的节点现在上报状态——`enabled`、`checked`、`radioChecked`、`hidden`、`stateKnown`、`flags`——以及 `depth`。分隔符只带类型，这也是对它唯一有意义的信息。
- `discovery.executeContextMenuCommand` 在成功路径上也返回 `hidden` 与 `resolved`，不再只在拒绝时返回。若只在拒绝分支返回，调用方无法区分「未被拒绝」与「本版本不返回该字段」。无注册项拥有该 GUID 时 `resolved` 为 `false`，此时没有可评估的状态。
- `discovery.getMainMenuCommands` 中由动态子菜单展开的条目现在也带 `stateKnown`、`executable`、`unaddressableReason`，与静态槽位一致。

### SDK

- **破坏性类型修正** —— `DiscoveryContextMenuCommand` 原本是 `DiscoveryMainMenuCommand` 的类型别名，因而声明了右键侧根本不会返回的字段。现改为独立接口：右键条目是扁平注册、由宿主决定位置，因此没有菜单 `path`，也没有动态展开相关字段。此前读取右键命令的 `path` / `isDynamic` / `subGuid` 的 TypeScript 代码，读的是从未被填充过的字段。
- 新增导出类型 `MenuNodeState`、`MenuNodeSource`、`MenuUnaddressableReason`，由所有菜单列举结果共用。
- `fb.discovery.searchCommands()` 接受 `{ scope, includeHidden }`；`fb.discovery.getContextMenuCommands()` 接受 `{ includeHidden }`。
- 修复 SMP 主菜单派发。`buildMenuItems` 把菜单 id 优先映射到条目的 `path` 而非 `guid`；路径必须去比对生成的菜单树，而该查找在汉化宿主上直接失败，GUID 则由宿主直接解析。因此主菜单的 `ExecuteByID` 在这类宿主上完全不工作。现改为优先 `guid`；右键菜单会话仍以 `commandId` 为准。
- 修复 SMP 菜单状态解码。`buildMenuItems` 从原始 `flags` 位重新推导 `enabled` / `checked`，忽略了宿主已给出的归一化布尔值。现优先使用布尔值，仅在其缺失时回退到 `flags`。标记为 `stateKnown: false` 的条目按可用呈现而非置灰，因为 `flags == 0` 与「可用且未勾选」在位上完全相同，把未观测状态当作禁用会隐藏宿主本可执行的命令。

## v1.11.0 (2026-07-27)

### 托盘与菜单

- 新增 `TrayMenuItem.playbackAction`（`'play-pause' | 'previous' | 'next' | 'stop'`）：自定义托盘项可声明由插件原生执行的播放动作。外观仍由调用方控制；声明项**不发** `tray:menuItemClicked`（同 Electron `role` / Tauri `PredefinedMenuItem`）。仅可用于 `type:'normal'` 的叶子项；取值非法，或写在分隔符 / 子菜单 / 富控件上时，整次 `setContextMenu` / `appendMenuItems` 调用会以 `INVALID_PARAMS` 失败，而不是静默忽略。不接受 `'exit'`。仅托盘菜单生效，对 `menu.show` 无效；`getMenuItems` 会原样回读该字段。主页面深挂起（最小化 / 托盘隐藏 / 锁屏）时要后台可靠的托盘播放控制，请用本字段或内置 `showPlaybackControls`；仅靠 click→`playback.*` 不保证执行。自 v1.11.0 起可用；需兼容旧宿主时先用 `config.getVersionInfo().plugin.version` 探测。
- 文档澄清：`tray:menuItemClicked` 仅覆盖普通用户项与富值控件；内置播放 / 系统注入项与声明了 `playbackAction` 的项原生执行且不发点击事件，按钮态从 `playback:*` 反映
- `menu.getMainMenu` 新增 `locale`、`i18n`、`withAvailability` 三个选项。`locale` 默认 `'auto'`，保持宿主原生标签不翻译；`i18n: false` 完全关闭 `displayLabel` 翻译；`withAvailability` 默认 `true`，附带各子菜单的命令可用性计数。SDK 侧签名相应变为 `getMainMenu(root?, opts?)`。
- 修复自定义菜单的 UTF-8 序列化与上下文模式选择。

### DSP 与输出

- **`dsp.*` 与 `output.*` 现在真正可用。** 这 11 个处理器（`dsp.getChain` / `getPresets` / `getAvailable` / `addDsp` / `removeDsp` / `moveDsp` / `applyPreset` / `setChain`，`output.getDevices` / `getEntries` / `getSettings`）此前虽有文档，但其源文件从未被加入编译，所以任何调用都会因方法未注册而失败。自本版起已编译并注册。已发布的 `fb.dsp.*` / `fb.output.*` SDK 封装本来就存在，配合本版插件后开始真正生效；插件版本低于本版时，无论 SDK 版本如何都会被拒绝。
- 修复 `output.getDevices` 崩溃。部分输出后端在回调里用「长度未知」哨兵值代替真实长度，原实现直接采用该值，导致读取远超字符串末尾并使 foobar2000 终止。
- 修复 `dsp.moveDsp` 移动到错误位置。升序移动会少一格，因此向上移动一位等于无操作，且任何项都无法移到链尾；降序移动本来是正确的。返回的 `to` 现在是真实的最终索引。
- **行为变更** —— `dsp.setChain` 遇到任何不可用条目时整体拒绝，不再静默跳过。此前用三个条目构造的链可能只应用两个却仍返回 `success: true`。每个条目级失败都返回带索引的原因：`dsps[0] must be an object`（元素不是对象，例如裸字符串或数字）、`dsps[0]: guid is required`（缺失、空串或不是字符串）、`dsps[0]: Invalid GUID format: …`、`dsps[0]: DSP not found or no default preset: …`（GUID 格式合法但该 DSP 未安装）。`dsps` 本身缺失或不是数组时仍返回 `dsps array is required`。任何调用被拒绝时链都保持不变。
- `dsp.getPresets` 在无选中预设时返回 `selectedIndex: -1`。此前返回内部哨兵值 `18446744073709551615`，该值无法用 JavaScript number 表示，到达前端时已是不可用的浮点数。
- `dsp.getChain` 始终包含 `activePreset` 与 `activePresetIndex`，无选中预设或宿主不支持预设时为 `null` / `-1`。此前这两个键在上述情况下完全缺失，调用方不得不自行探测。

### Discovery

- `discovery.getMainMenuCommands` 与 `discovery.searchCommands` 现在默认展开运行时构建子菜单的组件（`mainmenu_commands_v2`，例如 ESLyric）。**这会改变既有返回结果**：除父级槽位外还会含其子命令。传 `{ expandDynamic: false }` 可回到仅静态注册表的旧行为。
- 命令条目新增 `path`、`isDynamic`、`isDynamicParent`、`subGuid`、`flags` 字段；响应新增 `expandDynamic`、`dynamicCount`，`discovery.getAllServices` 新增 `mainMenuDynamicCommands`。标记 `isDynamicParent` 的条目是容器槽位，自身不可执行。
- `discovery.executeMainMenuCommand` 新增第二个可选参数 `subGuid`：执行由动态子菜单展开出的命令时必须一并传入，否则只会派发静态命令 GUID。

### 性能与稳定性

- 页面不可见期间（最小化、被遮挡、托盘隐藏或锁屏），高频可再生流在生产侧即停止：`audio:spectrum`、`playback:time`、`playback:timeHighRes` 不再产生，恢复可见后下一拍自然到达；`window:hoverStateChanged` 与 `cursor:hiddenChanged` 在隐藏期间天然静默。其余事件一律可靠按序投递，不丢弃也不合并，包括 `http:response`、`library:getAllResult`、`audio:fullWaveformReady` 等异步应答与 `playback:itemPlayed` 等逐次事实。绘制频谱或播放进度的主题应把数据中断理解为「页面隐藏」而非「播放停止」。
- 最小化 / 被遮挡 / 锁屏 / 托盘隐藏时新增深度挂起（`TrySuspend`），冻结渲染进程计时器与动画以便系统回收内存。新增 foobar2000 高级设置项 `Deep-suspend WebView when hidden (TrySuspend; frees renderer memory)`（默认开启，关闭后退回原 Low 内存路径）。
- 新增高级设置项 `Keep WebView active in background while CDP remote debugging is on (tray/minimize/lock)`（默认开启）：开启 CDP 远程调试时保持 WebView 后台活跃，以保证截图与时序类自动化工具稳定。
- 收敛 WebView2 崩溃后的僵尸态：进程失败后按重载次数上限决定重载或重建，不再停留在无响应页面。
- 改进封面获取：修正了换曲或改标签后可能返回上一首封面的缓存问题，并把解码移出 UI 线程、改为逐个排队处理，大尺寸封面不再造成界面卡顿。`artwork.*` 的请求与响应结构未变。

### 元数据

- 修复 `metadata.read`、`metadata.readByPath`、`metadata.readBatch` 忽略容器内轨道编号的问题。路径里的 `|subsong:N` 后缀既没有被剥离也没有被采用，因此从 CUE、ISO 镜像或多轨文件里读取单条轨道时，要么直接失败，要么返回首轨的标签。这正是这类文件在 foobar2000 界面里能读、经 API 读不到的原因。`metadata.readRaw` 原本就是正确的。
- `metadata.read` 与 `metadata.readByPath` 新增 `cueIndex` 参数，可显式指定轨道，与 `metadata.readRaw` 一致；其优先级高于路径中的 `|subsong:N` 后缀。SDK 的 `fb.metadata.read()` / `readByPath()` 通过第二个 `opts` 参数传入，MCP 的 `fb2k_metadata_read` / `fb2k_metadata_read_by_path` 已声明该参数。`metadata.readBatch` 不接受此参数，批量读取请在各自路径上使用 `|subsong:N` 后缀。

### SDK

- 新增仅 SDK 层的 additive 二进制适配 helper：`fb.file.readBinary()`、`fb.file.writeBinary()`、`fb.file.writeDataUrl()`、`fb.metadata.embedArtworkBytes()`、`fb.metadata.embedArtworkFromDataUrl()`，以及公开类型 `FileBinaryWriteOptions`、`MetadataArtworkBytesOptions`。
- 这些 helper 只把 `ArrayBuffer` / `Uint8Array` 与严格 Base64 Data URL 适配到既有 `file.read`、`file.write`、`metadata.embedArtwork` wire 契约；不新增 Bridge endpoint，不改变 raw `invoke` 或旧 facade。canonical Base64 与 Data URL 校验发生在 SDK 调用 Host 之前，Host 自身的校验和行为没有变化。
- 修正两处错误的 TypeScript 返回类型声明，使其与宿主实际 wire 契约一致：`ui.isMinimized()` 为 `{ minimized }`（不存在 `isMinimized` 别名）、`ui.isAlwaysOnTop()` 为 `{ enabled, isAlwaysOnTop }`（两个键同值）。**按旧声明编写的 TypeScript 代码需要相应调整。**
- `fb.playcount.set()` 不再发送宿主从未读取的 `count` 键；`window.getBackdropPolicy` / `window.setBackdropPolicy` 补齐 `windowId` 参数类型声明。
- `fb.http.request()` 现在经由文档声明的 `http.get` 端点派发；此前一个失效的内部参数可能转发错配的方法名。动词 helper（`fb.http.post()` / `put()` / `delete()` / `patch()`）仍各自派发到自己的端点，二进制响应也一样。
- 为五个 wrapper 补上可选的末位 `opts` 参数，对应的键宿主 handler 早已读取：`fb.file.delete(path, opts?)`（`moveToTrash`）、`fb.file.copy(source, destination, opts?)`（`overwrite`），以及 `fb.metadata.write(path, tags, opts?)` / `removeField(path, field, opts?)` / `removeTag(path, tags, opts?)`（`cueIndex`）。其中 `metadata.write` 支持 `cueIndex` 补上了一处真实缺口：v1.11.0 只把 `cueIndex` 接进了元数据**读取**侧，因此此前无法通过 SDK 向 CUE 或镜像文件中的单曲写标签。所有新增参数均为可选，既有调用点不受影响。
- `fb.metadata.readByPath()` 的返回类型由裸 `JsonObject` 收窄为 `MetadataReadByPathResponse`。
- 修正两处与宿主契约不符的公开类型声明：`dsp.setChain` 的 `dsps` 是**必填**的 `{ guid }` 对象数组（原声明为 `dsps?: string[]`，在可选性、元素类型、结构三处均错——宿主要求 `dsps` 必须是数组，并逐项读取 `guid`）；`playlist:created` / `playlist:renamed` 的 payload 保持 `name: string`。**按旧 `dsp.setChain` 声明编写的 TypeScript 代码需要相应调整。**

## v1.10.0 (2026-07-16)

- 新增：`TrayMenuItem.orientation`（仅 `type:'slider'`，`'horizontal' | 'vertical'`，默认水平）。仅精确 `vertical` 为纵向（min 底 / max 顶；Up/Right 增、Down/Left 减、Home/End 边界）；`native` 忽略并保持分级子菜单；旧 runtime 忽略未知字段保持水平。范围规范化：`max<min` 交换、`max==min` 常量不发 value、初始 value clamp、IPC 越界拒绝。`getMenuItems` round-trip。需要纵向时先用 `config.getVersionInfo().plugin.version` 探测运行时是否为 1.10.0 及以上
- 变更：自绘菜单两态焦点（导航 roving tabindex + 真实 focus / 富控件编辑态）与 ARIA（`menuitem` / `menuitemcheckbox` / 内部 `role=slider` / segmented `radiogroup`）；`checked:false` 仍为 checkable；默认入退场在 `prefers-reduced-motion: reduce` 下禁用 transform/transition（不改 hide protocol / `closeAnimationMs`）
- 新增：`TrayMenuConfig.layoutMode`（`'flat' | 'zones'`）。默认 `'flat'` 保留 `#menu > .fb-item` 直接子 DOM；显式 `'zones'` 时为非空 top / playback / bottom 生成 `.fb-zone[data-zone]`。`native` 忽略；旧 runtime 忽略未知字段但不生成 wrapper；`menu.show` 不受影响。需要 zones 的主题应先用 `config.getVersionInfo().plugin.version` 探测运行时是否为 1.10.0 及以上
- 变更：自绘菜单受保护 CSS 不再强制可见态 `#menu { display:block !important }`；主题可直接把根菜单 / zone 设为 flex 或 grid，但无法用 `display:* !important` 重新显示已隐藏菜单
- 安全：自绘菜单 SVG 图标改为 DOMParser + 元素/属性白名单克隆，移除 raw `innerHTML` 注入；非法或超限单图标被丢弃，菜单继续显示；`transform` 严格解析（拒绝前缀/函数间杂质与空参），节点必须位于 SVG namespace
- 安全：`tray.setContextMenu` / `tray.appendMenuItems` / `menu.show` 在写入持久配置或打开 overlay 前做资源上限事务性校验（item ≤ 512、`menu.show` 深度 ≤ 8、segmented options ≤ 64、CSS ≤ 256 KiB、SVG 总量 ≤ 256 KiB）；单图 SVG > 32 KiB 仅丢弃该图标。超限返回 `INVALID_PARAMS` 且 `details` 含 `field` / `limit` / `actual`；非法 / 超限输入属有意不兼容
- 安全加固：托盘 / 自绘菜单的内置动作改按可信内部来源路由，不再凭 public id 前缀。唯一兼容例外是 tray API 中精确、大小写敏感的 `_sys_exit`（保持 1.9.0 的真实退出行为）；调用方提供的 `_pb_playPause` / `_pb_prev` / `_pb_next` / `_pb_stop` 仍是普通用户项，且不能通过同名去重抑制 runtime 自动注入的 trusted 播放项；重复 public ID 由 opaque token 区分，公共 `menu.show` 不提权。每次选择 / 值变更由不可预测的单次 token 承载并按本次菜单索引校验，未知 / 过期 token、禁用项、越界富值（rating / slider / segmented）一律拒绝。内部 `menu.__*` IPC 另校验调用方是否为 overlay 自身窗口，且 select / dismiss / ready / submenuPanel / valueChanged 须匹配当前菜单 id；外部调用方或过期 / 伪造的 menuId 一律拒绝且不改变菜单状态
- 自绘 tray `ContentSized` 现在在字体就绪后离屏测量 root 与全部一级 submenu，并等待连续两帧尺寸稳定；C++ 使用 64 位安全槽位分配。固定 HWND 的 region 只覆盖当前可见 root/submenu panel，因此未展开预留区不再形成空白 acrylic/mica 面板，且调用方配置的 backdrop 不会被静默关闭
- 托盘自绘菜单 `segmented` 分段控件的值变更明确纳入“保持菜单打开”契约：一次分段切换经 `tray:menuItemClicked` 回传 `{ id, value }`（`value` = 被选中分段的从 0 起索引）且**不关闭**菜单，与 `rating` / `slider` 一致。`webview` 运行时一直保持菜单打开，本次仅修正此前只列 `rating` / `slider` 的共享契约与事件文档
- 托盘菜单全隐藏（或空）的分区不再产生多余 separator：此前分区内所有项 `visible:false` 时，隐藏项被过滤后会残留一条首/尾分隔线；现在可见性过滤先于 separator 判定，native 与 webview 菜单均不再出现该多余分隔线（默认行为修正）
- 文档修正：`TrayMenuItem.icon`（base64 ICO）为保留字段，当前两种后端均不渲染（`native` 纯文本、webview 绘制 `iconSvg`）；菜单项图标请改用 `iconSvg`

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
