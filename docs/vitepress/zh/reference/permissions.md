# 权限系统

涉及文件路径的 Bridge API 会先由 BridgeCore 的路径安全 spec 校验；非法路径直接返回 `PERMISSION_DENIED`，不会进入文件系统或 foobar2000 SDK 的路径副作用。

权威计数来自当前 `src/api/**` 中 `RegisterApi` 路径安全 spec（形态为 `{ param, SecurityLevel::... }`）：

| 级别 | spec 条数 | 含义 |
| --- | ---: | --- |
| `Read` | 9 | 普通文件系统只读校验 |
| `Write` | 1 | 严格写入目标（配置/临时目录策略） |
| `MediaRead` | 40 | 媒体上下文只读校验 |
| `MediaWrite` | 17 | 媒体上下文写校验 |
| **合计** | **67** | **64 个唯一 API** |

## 五级权限模型

| 级别 | 说明 | 校验逻辑 |
| --- | --- | --- |
| `None` | 不涉及文件路径 | 无路径校验 |
| `Read` | 只读文件系统操作 | 禁止系统保护目录、设备路径与 `..` 遍历 |
| `Write` | 严格写入目标 | 仅允许 PathSecurity 接受的配置/临时等写入目标 |
| `MediaRead` | 读取媒体元数据/内容 | Read 规则 + 媒体库/播放列表上下文信任 |
| `MediaWrite` | 修改媒体文件 | MediaRead 规则 + 写黑名单，且无非系统盘自动放行 |

::: tip 权限层级关系
`None < Read < Write` 是普通文件系统通道。
`None < Read < MediaRead < MediaWrite` 是媒体通道。
`Write` 与 `MediaWrite` 是两条独立写通道。
:::

## 错误响应

```json
{
  "success": false,
  "error": "Path rejected by security policy: C:\\Windows\\System32\\config.ini",
  "code": "PERMISSION_DENIED"
}
```

```javascript
const result = await fb2k.invoke('file.read', { path: somePath });
if (!result.success && result.code === 'PERMISSION_DENIED') {
  console.warn('路径被安全策略拒绝:', result.error);
}
```

## API 权限对照表

### Read — 只读文件系统（9 条）

| API | 参数 | 数组 | 嵌套键 | 说明 |
| --- | --- | --- | --- | --- |
| `artwork.getFolderImages` | `directory` | — | — | 权威源：`ArtworkApi.cpp` |
| `clipboard.writeFiles` | `paths` | 是 | — | 权威源：`ClipboardApi.cpp` |
| `file.copy` | `source` | — | — | 权威源：`FileApi.cpp` |
| `file.exists` | `path` | — | — | 权威源：`FileApi.cpp` |
| `file.getInfo` | `path` | — | — | 权威源：`FileApi.cpp` |
| `file.list` | `path` | — | — | 权威源：`FileApi.cpp` |
| `file.read` | `path` | — | — | 权威源：`FileApi.cpp` |
| `shell.openWith` | `path` | — | — | 权威源：`ShellApi.cpp` |
| `shell.showInExplorer` | `path` | — | — | 权威源：`ShellApi.cpp` |

### Write — 严格写入目标（1 条）

| API | 参数 | 数组 | 嵌套键 | 说明 |
| --- | --- | --- | --- | --- |
| `http.download` | `saveTo` | — | — | 权威源：`HttpApi.cpp` |

### MediaRead — 读取媒体文件（40 条）

| API | 参数 | 数组 | 嵌套键 | 说明 |
| --- | --- | --- | --- | --- |
| `artwork.getAvailableArtwork` | `path` | — | — | 权威源：`ArtworkApi.cpp` |
| `artwork.getAvailableTypes` | `path` | — | — | 权威源：`ArtworkApi.cpp` |
| `artwork.getBatch` | `paths` | 是 | — | 权威源：`ArtworkApi.cpp` |
| `artwork.getByPath` | `path` | — | — | 权威源：`ArtworkApi.cpp` |
| `artwork.getFb2kUrlByPath` | `path` | — | — | 权威源：`ArtworkApi.cpp` |
| `artwork.getFb2kUrlByPathBatch` | `paths` | 是 | — | 权威源：`ArtworkApi.cpp` |
| `artwork.getFb2kUrlByPathBatch` | `items` | 是 | `path` | 权威源：`ArtworkApi.cpp` |
| `artwork.getForTrack` | `path` | — | — | 权威源：`ArtworkApi.cpp` |
| `artwork.getLyrics` | `path` | — | — | 权威源：`ArtworkApi.cpp` |
| `artwork.getMetadata` | `path` | — | — | 权威源：`ArtworkApi.cpp` |
| `audio.analyzeBPM` | `path` | — | — | 权威源：`AudioApi.cpp` |
| `audio.generateFullWaveform` | `path` | — | — | 权威源：`AudioApi.cpp` |
| `audio.generateWaveform` | `path` | — | — | 权威源：`AudioApi.cpp` |
| `discovery.executeContextMenuByPath` | `trackPath` | — | — | 权威源：`DiscoveryApi.cpp` |
| `jitQueue.enqueueNext` | `url` | — | — | 权威源：`QueueApi.cpp` |
| `jitQueue.playNow` | `url` | — | — | 权威源：`QueueApi.cpp` |
| `jitQueue.preloadBatch` | `urls` | 是 | — | 权威源：`QueueApi.cpp` |
| `library.getByPath` | `path` | — | — | 权威源：`LibraryApi.cpp` |
| `lyrics.exists` | `path` | — | — | 权威源：`LyricsApi.cpp` |
| `lyrics.get` | `path` | — | — | 权威源：`LyricsApi.cpp` |
| `metadata.read` | `path` | — | — | 权威源：`MetadataApi.cpp` |
| `metadata.readBatch` | `paths` | 是 | — | 权威源：`MetadataApi.cpp` |
| `metadata.readByPath` | `path` | — | — | 权威源：`MetadataApi.cpp` |
| `metadata.readRaw` | `path` | — | — | 权威源：`MetadataApi.cpp` |
| `playback.playPath` | `path` | — | — | 权威源：`PlaybackApi.cpp` |
| `playback.playPaths` | `paths` | 是 | — | 权威源：`PlaybackApi.cpp` |
| `playcount.get` | `paths` | 是 | — | 权威源：`PlaycountApi.cpp` |
| `playcount.getBatch` | `paths` | 是 | — | 权威源：`PlaycountApi.cpp` |
| `playlist.addPaths` | `paths` | 是 | — | 权威源：`PlaylistApi.cpp` |
| `playlist.addPathsAsync` | `paths` | 是 | — | 权威源：`PlaylistApi.cpp` |
| `playlist.addPathsSequential` | `paths` | 是 | — | 权威源：`PlaylistApi.cpp` |
| `playlist.replaceAllAndPlay` | `paths` | 是 | — | 权威源：`PlaylistApi.cpp` |
| `queue.addPaths` | `paths` | 是 | — | 权威源：`QueueApi.cpp` |
| `rating.get` | `path` | — | — | 权威源：`MetadataApi.cpp` |
| `replaygain.get` | `paths` | 是 | — | 权威源：`ReplayGainApi.cpp` |
| `replaygain.scan` | `paths` | 是 | — | 权威源：`ReplayGainApi.cpp` |
| `titleformat.eval` | `path` | — | — | 权威源：`TitleformatApi.cpp` |
| `titleformat.evalBatch` | `paths` | 是 | — | 权威源：`TitleformatApi.cpp` |
| `titleformat.evalFields` | `path` | — | — | 权威源：`TitleformatApi.cpp` |
| `titleformat.evalFieldsBatch` | `paths` | 是 | — | 权威源：`TitleformatApi.cpp` |

### MediaWrite — 修改媒体文件（17 条）

| API | 参数 | 数组 | 嵌套键 | 说明 |
| --- | --- | --- | --- | --- |
| `file.copy` | `destination` | — | — | 权威源：`FileApi.cpp` |
| `file.delete` | `path` | — | — | 权威源：`FileApi.cpp` |
| `file.mkdir` | `path` | — | — | 权威源：`FileApi.cpp` |
| `file.move` | `destination` | — | — | 权威源：`FileApi.cpp` |
| `file.move` | `source` | — | — | 权威源：`FileApi.cpp` |
| `file.rename` | `path` | — | — | 权威源：`FileApi.cpp` |
| `file.write` | `path` | — | — | 权威源：`FileApi.cpp` |
| `lyrics.save` | `path` | — | — | 权威源：`LyricsApi.cpp` |
| `metadata.embedArtwork` | `path` | — | — | 权威源：`MetadataApi.cpp` |
| `metadata.removeEmbeddedArt` | `path` | — | — | 权威源：`MetadataApi.cpp` |
| `metadata.removeField` | `path` | — | — | 权威源：`MetadataApi.cpp` |
| `metadata.removeTag` | `path` | — | — | 权威源：`MetadataApi.cpp` |
| `metadata.write` | `path` | — | — | 权威源：`MetadataApi.cpp` |
| `metadata.writeBatch` | `items` | 是 | `path` | 权威源：`MetadataApi.cpp` |
| `playcount.set` | `path` | — | — | 权威源：`PlaycountApi.cpp` |
| `rating.set` | `path` | — | — | 权威源：`MetadataApi.cpp` |
| `replaygain.clear` | `paths` | 是 | — | 权威源：`ReplayGainApi.cpp` |

::: info 嵌套数组校验
`metadata.writeBatch` 的 `items` 是对象数组，系统会提取每个元素的 `path` 字段进行校验。
:::

## 自定义策略 API

| API | 校验方式 |
| --- | --- |
| `shell.exec` | 无命令白名单；可选 `cwd` 仍走 PathSecurity |
| `shell.spawn` | 无可执行白名单；绝对可执行路径与 `cwd` 做路径校验 |
| `console.log` | 日志目录限制 + 保留设备名过滤 + `.log` / `.txt` 扩展名白名单 |
| `playlist.insertTracks` | 参数为 playlist handle，不是原始文件路径 |

## 路径安全规则详解

### 通用拦截

- 设备路径：`\\.\...`、`\\?\...`
- 目录遍历：包含 `..`
- 空路径 / 相对路径：必须是绝对路径

### Read

系统盘保护目录包括：

| 保护目录 | 原因 |
| --- | --- |
| `C:\\Windows\\` | 系统文件 |
| `C:\\Program Files\\` | 程序文件 |
| `C:\\Program Files (x86)\\` | 32 位程序文件 |
| `C:\\ProgramData\\` | 系统配置数据 |

非系统盘在 Read 下通常放行，以支持 NAS / 便携版场景。

### Write

仅允许严格写策略接受的目标目录；实践上为 foobar2000 配置目录与系统临时目录。

### MediaRead

在 Read 之上，目标还必须能解析到：

- foobar2000 媒体库，或
- 任意播放列表中的曲目（扫描上限由运行时实现界定）。

### MediaWrite

在 MediaRead 之上：

- 即使目标出现在媒体库/播放列表中，系统保护目录仍禁止写入
- 不会因为位于非系统盘就自动放行

## 统计

| 权限级别 | spec 条数 | 该级别唯一 API 数 |
| --- | ---: | ---: |
| Read | 9 | 9 |
| Write | 1 | 1 |
| MediaRead | 40 | 39 |
| MediaWrite | 17 | 16 |
| **合计** | **67** | **64** |

以上计数由组件源码中的 C++ `RegisterApi` 路径安全 spec 动态生成。
