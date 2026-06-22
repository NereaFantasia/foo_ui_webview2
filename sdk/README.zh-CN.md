[English](./README.md) | 中文

# foo_ui_webview2 SDK

简化版的 foobar2000 WebView2 插件 SDK，将冗长的 API 调用简化为简洁的命名空间调用。

仓库: https://github.com/NereaFantasia/foo_ui_webview2

## 安装

```bash
npm install foo-webview-sdk
```

### ESM / 打包工具 / TypeScript

```js
import fb from 'foo-webview-sdk'
// 或按需引入命名空间（支持 tree-shaking）
import { player, playlist, library } from 'foo-webview-sdk'
// 共享类型同样从包根引入
import type { TrackInfo, FBEventName } from 'foo-webview-sdk'

await fb.player.play()
```

> 也可从 `foo-webview-sdk/bridge` 子路径引入运行时 `fb` 与命名空间；该子路径只含运行时面，不含独立导出的共享类型（共享类型请从包根引入）。

Web Components 与 SMP 兼容层走各自子路径：

```js
import { registerComponents } from 'foo-webview-sdk/components'
import { bootstrapSmpCompat } from 'foo-webview-sdk/smp-compat'
```

### `<script>` 标签（IIFE 全局包）

```html
<script src="node_modules/foo-webview-sdk/dist/bridge.global.js"></script>
<script src="node_modules/foo-webview-sdk/dist/components.global.js"></script>
<!-- 加载后即有全局 window.fb，组件自动注册 -->
```

## 快速开始

### 旧方式（冗长）

```javascript
// 需要 150+ 字符
window.fb2k.invoke('playback.play', {})
window.fb2k.invoke('playlist.addPaths', { playlist: 0, paths: [...] })
```

### 新方式（简洁）

```javascript
// 只需 20 字符
fb.player.play()
fb.playlist.add(0, paths)
```

## API 命名空间

### `fb.player` - 播放控制

```javascript
// 播放控制
await fb.player.play()
await fb.player.pause()
await fb.player.stop()
await fb.player.next()
await fb.player.prev()
await fb.player.toggle()  // 切换播放/暂停

// 跳转
await fb.player.seek(30)  // 跳转到 30 秒

// 音量
await fb.player.setVolume(80)
const { volume, isMuted } = await fb.player.getVolume()
await fb.player.mute()

// 状态
const state = await fb.player.getState()
const track = await fb.player.getCurrentTrack()
const { position } = await fb.player.getPosition()

// 播放顺序
const { order, name } = await fb.player.getOrder()
await fb.player.setOrder(0)  // 0=默认, 1=重复列表, 2=重复曲目, 3=随机...

// 播完停止
await fb.player.setStopAfterCurrent(true)
```

### `fb.playlist` - 播放列表管理

```javascript
// 获取播放列表
const playlists = await fb.playlist.getAll()
const active = await fb.playlist.getActive()
const playing = await fb.playlist.getPlaying()

// 操作播放列表
await fb.playlist.setActive(0)
await fb.playlist.create('我的歌单')
await fb.playlist.rename(0, '新名称')
await fb.playlist.duplicate(0)
await fb.playlist.remove(0)
await fb.playlist.clear(0)

// 曲目操作
const tracks = await fb.playlist.getTracks(0, 0, 100)
const { count } = await fb.playlist.getCount(0)
await fb.playlist.add(0, ['C:/Music/song.mp3'])
await fb.playlist.removeTracks(0, [0, 1, 2])
await fb.playlist.playTrack(0, 5)

// 选择操作
await fb.playlist.selectAll(0)
await fb.playlist.deselectAll(0)
await fb.playlist.setSelection(0, [0, 1, 2], true)

// 移动曲目（delta 为偏移量，正数向下，负数向上）
await fb.playlist.moveTracks(0, [0, 1], 2)  // 向下移动2位

// 撤销/重做
await fb.playlist.undo(0)
await fb.playlist.redo(0)

// 原子操作：清空+添加+播放
await fb.playlist.replaceAllAndPlay({
    playlist: 0,
    paths: ['C:/Music/song1.mp3', 'C:/Music/song2.mp3'],
    playIndex: 0
})
```

### `fb.library` - 媒体库

```javascript
// 搜索（分页字段: tracks/items/total/offset/limit/hasMore）
const result = await fb.library.search('artist HAS Radiohead', 50)
console.log(result.total, result.tracks.length)

// 枚举建议：先拿总数，再分页遍历
const { count } = await fb.library.getCount()
for (let start = 0; start < count; start += 500) {
    const page = await fb.library.getAll(start, 500) // 返回 { tracks, items, total, offset, limit, fromCache }
    // 处理 page.tracks
}

// 高层封装：异步生成器分页枚举
for await (const page of fb.library.enumerateTracks({ pageSize: 500 })) {
    console.log(page.fetched, '/', page.total)
}

// ★ 推荐：获取真实媒体库根
const { roots, total, indexedTracks, skippedTracks } = await fb.library.getRoots()
for (const root of roots) {
    console.log(root.displayName, root.absolutePath, root.trackCount)
}

// ★ 推荐：typed 目录树浏览
const tree = await fb.library.browseTree({ rootId: roots[0].id })
for (const dir of tree.directories) {
    console.log(dir.name, dir.trackCount, dir.hasChildren)
}
// 带文件列表
const treeWithFiles = await fb.library.browseTree({
    rootId: roots[0].id, pathId: 'Anime/OST', includeFiles: true
})

// Legacy: 目录视图枚举（@deprecated 不推荐作为根入口，请使用 getRoots + browseTree + enumerateTree）
const root = await fb.library.browseDirectory('', false) // { directories, files, items }
for await (const node of fb.library.enumerateDirectories({ rootPath: '', includeFiles: false })) {
    console.log(node.path, node.directories.length)
}

// ★ 推荐：Root-aware 异步树遍历器
for await (const batch of fb.library.enumerateTree({ rootId: roots[0].id, strategy: 'bfs', includeFiles: true })) {
    console.log(batch.pathId, batch.directories.length, batch.files.length, batch.visited)
}

// 分类与聚合
const albums = await fb.library.getAlbums(100)
const artists = await fb.library.getArtists(200)
const genres = await fb.library.getGenres() // { genres: [{name, trackCount}] }
const years = await fb.library.getFieldValues('date', 50)
const recent = await fb.library.getRecentlyAdded(20, 'modified')

// 统计
const stats = await fb.library.getStats()
const status = await fb.library.getStatus()

// 操作
await fb.library.refresh()
await fb.library.rescan()
const track = await fb.library.getByPath('C:/Music/song.mp3')
await fb.library.addToPlaylist(['C:/Music/song.mp3'])
// 注意: library.add/library.remove 已移除（C++ 后端未实现）
```

### `fb.ui` - 窗口控制

```javascript
// 窗口操作
await fb.ui.minimize()
await fb.ui.maximize()
await fb.ui.restore()
await fb.ui.close()
await fb.ui.toggleMaximize()
await fb.ui.toggleFullscreen()

// 窗口信息
const { isMaximized } = await fb.ui.isMaximized()
const { isFullscreen } = await fb.ui.isFullscreen()
const state = await fb.ui.getState()

// 窗口设置
await fb.ui.setPosition(100, 100)
await fb.ui.setSize(1280, 720)
await fb.ui.setTitle('我的播放器')
await fb.ui.setAlwaysOnTop(true)

// 其他
await fb.ui.startDrag()
await fb.ui.showSystemMenu(100, 100)
const { scale } = await fb.ui.getDpiScale()
await fb.ui.flashTaskbar()
```

### `fb.config` - 配置

```javascript
// 输出设备
const devices = await fb.config.getOutputDevices()
const config = await fb.config.getOutputConfig()
await fb.config.setOutputDevice('output-id', 'device-id')
await fb.config.setOutputBuffer(500)

// 高级配置
const advanced = await fb.config.getAdvancedConfig()
const { value } = await fb.config.getAdvancedConfigValue('{guid}')
await fb.config.setAdvancedConfigValue('{guid}', 'new-value')
await fb.config.resetAdvancedConfig('{guid}')

// 偏好设置
const pages = await fb.config.getPreferencesPages()
const guids = await fb.config.getPreferencesStandardGuids()
await fb.config.showLibraryPreferences()

// 组件信息
const components = await fb.config.getComponents()
const version = await fb.config.getVersionInfo()

// DSP
const presets = await fb.config.getDspPresets()
const active = await fb.config.getActiveDspPreset()
await fb.config.setActiveDspPreset(0)  // 按索引设置
```

### `fb.artwork` - 封面艺术

```javascript
// 获取封面
const art = await fb.artwork.getCurrent('front')
if (art) {
    console.log(`封面: ${art.width}x${art.height}, ${art.mimeType}`)
    // art.data 是 Base64 编码的图片数据
}

// 根据路径获取封面
const art = await fb.artwork.getByPath('C:/Music/song.mp3', 'front')
```

### `fb.audio` - 音频分析

```javascript
// 订阅实时频谱数据
const unsubscribe = fb.audio.subscribeSpectrum((data) => {
    renderVisualizer(data.spectrum);
}, { fftSize: 2048, fps: 60 });

// 使用自定义事件名隔离多个可视化面板
const unsubscribePanel = fb.audio.subscribeSpectrum((data) => {
    renderPanelSpectrum(data.spectrum);
}, { event: 'audio:panelSpectrum', bands: 48, fps: 30 });

// 取消订阅
unsubscribe();
unsubscribePanel();

// 生成完整文件波形（支持缓存和后台解码）
const result = await fb.audio.generateFullWaveform('E:\\Music\\song.flac', {
    resolution: 256,
    method: 'peak'  // 或 'rms'
});

if (result.status === 'ready') {
    console.log('波形数据:', result.waveform);
    console.log('时长:', result.duration);
}

// 支持 subsong 和 CUE 分轨
const result2 = await fb.audio.generateFullWaveform('E:\\Music\\disc.flac|subsong:2', {
    resolution: 512
});

const result3 = await fb.audio.generateFullWaveform('E:\\Music\\album.cue', {
    cueIndex: 3,
    resolution: 256
});

// 分析 BPM
const { bpm, source } = await fb.audio.analyzeBPM('E:\\Music\\song.flac');
console.log(`BPM: ${bpm}, 来源: ${source}`);

// 强制重新分析（忽略已有 BPM 标签）
const forced = await fb.audio.analyzeBPM('E:\\Music\\song.flac', {
    forceAnalysis: true
});
console.log(`BPM: ${forced.bpm}, 来源: ${forced.source}`);

// 无效声道模式会自动回退到 default
const channelModeResult = await fb.audio.setChannelMode('invalid');
console.log(channelModeResult.mode); // "default"
```

> SDK 层不暴露 `subscriptionId`。如果你需要显式控制订阅 ID，请直接使用 `fb2k.invoke('audio.subscribeSpectrum', ...)` / `fb2k.invoke('audio.unsubscribeSpectrum', ...)`。

### `fb.utils` - 工具函数

```javascript
// 测试连接
const { pong, timestamp } = await fb.utils.ping()

// 格式化标题
const { result } = await fb.utils.formatTitle('%artist% - %title%', 'C:/Music/song.mp3')

// 获取文件信息
const info = await fb.utils.getFileInfo('C:/Music/song.mp3')
```

### `fb.system` - 系统 API

```javascript
// API 发现
const apis = await fb.system.listApis(true, true)
const namespaceApis = await fb.system.getApisByNamespace('playback')
const searchResults = await fb.system.searchApis('volume')

// API 统计
const stats = await fb.system.getApiStats()

// 插件管理
const plugins = await fb.system.getRegisteredPlugins()
const { registered } = await fb.system.isPluginRegistered('my-plugin')
```

### `fb.state` - 响应式状态

`fb.state` 是一个自动同步的响应式状态对象，会自动更新播放状态：

```javascript
// 直接读取（自动同步）
console.log(fb.state.volume)      // 当前音量
console.log(fb.state.isPlaying)   // 是否正在播放
console.log(fb.state.currentTrack) // 当前曲目
console.log(fb.state.position)    // 当前播放位置（秒）

// 在 Vue/React 中使用
// Vue:
//   watch(() => fb.state.isPlaying, (playing) => { ... })
// React:
//   useEffect(() => { ... }, [fb.state.isPlaying])
```

> **注意**: `fb.state` 是只读的播放状态快照，与 `fb.sharedState` 不同！

### `fb.sharedState` - 跨窗口共享状态

`fb.sharedState` 用于在多个窗口之间共享键值数据（如歌词缓存、用户偏好等）：

```javascript
// 写入共享状态
await fb.sharedState.set('lyrics:123', { text: '歌词内容...' })

// 带 TTL 的临时缓存（5分钟后自动过期）
await fb.sharedState.set('cache:artwork', artworkData, false, 300000)

// 读取共享状态
const { value, exists } = await fb.sharedState.get('lyrics:123')

// 删除共享状态
await fb.sharedState.delete('lyrics:123')

// 获取所有键（支持通配符）
const { keys } = await fb.sharedState.keys('lyrics:*')

// 监听状态变更
fb.sharedState.onChange(({ key, value, previousValue, sourceWindowId }) => {
    console.log(`状态 ${key} 被窗口 ${sourceWindowId} 修改`)
})

// 监听状态删除（包括过期）
fb.sharedState.onDelete(({ key, reason }) => {
    console.log(`状态 ${key} 被删除，原因: ${reason}`)  // reason: 'deleted' | 'expired'
})
```

> **`fb.state` vs `fb.sharedState` 区别**:
> - `fb.state`: 只读的播放状态快照（音量、播放状态、当前曲目等），自动同步
> - `fb.sharedState`: 可读写的跨窗口共享键值存储，用于自定义数据共享

### `fb.port` - 命名通道

`fb.port` 提供命名通道机制，用于窗口间的双向消息通信：

```javascript
// 创建通道
const { portId } = await fb.port.connect('lyrics-sync')

// 向同名通道广播消息
await fb.port.postMessage(portId, { type: 'sync', data: '...' })

// 定向发送到指定 Port
await fb.port.postMessageTo(portId, targetPortId, { type: 'request' })

// 获取同名通道的所有 Port
const { ports } = await fb.port.getPorts('lyrics-sync')

// 监听消息
fb.port.onMessage(({ portId, message, sourceWindowId }) => {
    console.log(`收到来自 ${sourceWindowId} 的消息:`, message)
})

// 监听连接/断开
fb.port.onConnect(({ portId, name, windowId }) => {
    console.log(`Port ${portId} 已连接到通道 ${name}`)
})

// 断开通道
await fb.port.disconnect(portId)
```

### `fb.event` - 自定义事件

`fb.event` 提供语义化的自定义事件发布机制：

```javascript
// 广播自定义事件到所有窗口（示例：主题作者定义的事件名）
await fb.event.emit('myapp:lyricsUpdated', { trackId: '123' })

// 广播但排除自己
await fb.event.emit('myapp:lyricsUpdated', { trackId: '123' }, true)

// 定向发送到指定窗口
await fb.event.emitTo('myapp:lyricsUpdated', { trackId: '123' }, 'popup_1')

// 监听自定义事件
fb.on('myapp:lyricsUpdated', ({ payload, sourceWindowId }) => {
    console.log(`来自 ${sourceWindowId} 的歌词更新:`, payload.trackId)
})
```

> **`fb.event` vs `window.sendMessage/broadcast` 区别**:
> - `window.sendMessage/broadcast`: 原始消息通道，固定事件名 `window:message`
> - `fb.event`: 语义化事件通道，事件名由调用方提供

## 事件订阅

```javascript
// 订阅播放状态变化
const unsubscribe = fb.on('playback.state', (data) => {
    console.log('状态:', data.state)
    console.log('曲目:', data.track)
    console.log('位置:', data.position)
})

// 订阅播放列表变化
fb.on('playlist.changed', (data) => {
    console.log('播放列表:', data.playlistIndex)
})

// 取消订阅
unsubscribe()

// 一次性订阅
fb.once('playback.state', (data) => {
    console.log('一次性事件:', data)
})
```

## TypeScript 支持

类型定义随包发布，IDE 自动提供智能提示：

```typescript
import fb from 'foo-webview-sdk'

async function playNext() {
    await fb.player.next()
    const track = await fb.player.getCurrentTrack()
    if (track) {
        console.log(`正在播放: ${track.artist} - ${track.title}`)
    }
}
```

> 包根 `foo-webview-sdk` 同时暴露运行时（`fb`、各命名空间、`bridge`、`state`）与共享类型（`TrackInfo`、`FBEventName` 等）；`foo-webview-sdk/bridge` 子路径只含运行时面，不含独立导出的共享类型。

## 可用性检查

```javascript
if (fb.isAvailable()) {
    // WebView2 环境可用
    await fb.player.play()
} else {
    console.warn('WebView2 不可用')
}
```

## 开发模式 Mock

在非 WebView2 环境中，SDK 会自动使用 Mock 模式，所有 API 调用会返回模拟数据：

```javascript
// 非 WebView2 环境下的行为
const result = await fb.player.play()
// 返回: { mock: true, method: 'playback.play' }
```

---

## Web Components 组件库

除了 JavaScript API，SDK 还提供了开箱即用的 Web Components。

### 快速开始

通过 `<script>` 引入 IIFE 全局包（`components.global.js` 会自动注册所有组件）：

```html
<script src="node_modules/foo-webview-sdk/dist/bridge.global.js"></script>
<script src="node_modules/foo-webview-sdk/dist/components.global.js"></script>

<!-- 直接使用组件 -->
<fb-play-button></fb-play-button>
<fb-seek-bar show-time></fb-seek-bar>
<fb-cover-art size="200"></fb-cover-art>
```

打包工具用户改用 ESM 入口并显式注册（ESM 入口无自动副作用）：

```js
import { registerComponents } from 'foo-webview-sdk/components'
registerComponents()
```

### 控制类组件

#### `<fb-play-button>` - 播放/暂停按钮
自动切换播放/暂停状态，自动更新图标。

```html
<fb-play-button size="32"></fb-play-button>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `size` | string | "24" | 图标大小 (px) |
| `disabled` | boolean | false | 禁用状态 |

#### `<fb-prev-button>` - 上一首
```html
<fb-prev-button size="24"></fb-prev-button>
```

#### `<fb-next-button>` - 下一首
```html
<fb-next-button size="24"></fb-next-button>
```

#### `<fb-stop-button>` - 停止
```html
<fb-stop-button size="24"></fb-stop-button>
```

#### `<fb-seek-bar>` - 进度条
支持点击跳转、拖拽定位。

```html
<fb-seek-bar show-time color="#1db954"></fb-seek-bar>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `show-time` | boolean | false | 显示时间 |
| `color` | string | "#1db954" | 进度条颜色 |

#### `<fb-volume-bar>` - 音量条
支持点击、拖拽、滚轮调节。

```html
<fb-volume-bar show-icon vertical color="#1db954"></fb-volume-bar>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `show-icon` | boolean | false | 显示音量图标 |
| `vertical` | boolean | false | 垂直显示 |
| `color` | string | "#1db954" | 滑块颜色 |

### 展示类组件

#### `<fb-cover-art>` - 封面图
自动加载当前播放曲目的封面，支持懒加载和备用图片。

```html
<fb-cover-art size="200" radius="8" fallback-src="default.jpg"></fb-cover-art>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `size` | string | "200" | 尺寸 (px) |
| `radius` | string | "8" | 圆角大小 (px) |
| `fallback-src` | string | "" | 加载失败时的备用图片 |

#### `<fb-title>` - 曲目标题
```html
<fb-title placeholder="未在播放" marquee></fb-title>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `placeholder` | string | "未在播放" | 无标题时显示 |
| `marquee` | boolean | false | 启用跑马灯效果 |

#### `<fb-artist>` - 艺术家
```html
<fb-artist placeholder="未知艺术家"></fb-artist>
```

#### `<fb-album>` - 专辑名
```html
<fb-album placeholder="未知专辑"></fb-album>
```

#### `<fb-time-display>` - 时间显示
```html
<fb-time-display format="current / total"></fb-time-display>
<fb-time-display format="-remaining"></fb-time-display>
```

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `format` | string | "current / total" | 格式模板 |

格式占位符：
- `current` - 当前播放时间
- `total` - 总时长
- `remaining` - 剩余时间

### 样式自定义

所有组件都使用 Shadow DOM，你可以通过 CSS 变量和 `::part()` 选择器自定义样式：

```css
/* 使用 ::part() 选择器 */
fb-play-button::part(button) {
    background: #1db954;
    border-radius: 50%;
}

fb-seek-bar::part(progress) {
    background: linear-gradient(90deg, #ff6b6b, #feca57);
}

fb-cover-art::part(container) {
    border: 2px solid rgba(255, 255, 255, 0.1);
}
```

### 完整示例

完整示例见项目仓库 [examples 目录](https://github.com/NereaFantasia/foo_ui_webview2/tree/main/examples)：

```html
<div class="player">
    <fb-cover-art size="280" radius="16"></fb-cover-art>
    
    <div class="track-info">
        <fb-title></fb-title>
        <fb-artist></fb-artist>
    </div>
    
    <fb-seek-bar show-time color="#1db954"></fb-seek-bar>
    
    <div class="controls">
        <fb-prev-button></fb-prev-button>
        <fb-play-button size="32"></fb-play-button>
        <fb-next-button></fb-next-button>
    </div>
    
    <fb-volume-bar show-icon></fb-volume-bar>
</div>
```

---

## 许可证

本 SDK 是 foo_ui_webview2 插件的一部分，以 [MIT 许可证](./LICENSE) 发布。
Copyright © 2024-2026 NereaFantasia.
