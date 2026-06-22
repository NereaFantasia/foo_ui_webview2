# 权限系统

foo_ui_webview2 v1.4.1 起，所有涉及文件路径的 API 端点由 BridgeCore decorator 统一校验路径安全。非法路径请求将被拦截并返回 `PERMISSION_DENIED` 错误码，不会触达底层文件系统或 foobar2000 SDK。

## 五级权限模型

| 级别 | 说明 | 校验逻辑 |
|------|------|----------|
| **None** | 不涉及文件路径的 API | 无路径校验 |
| **Read** | 只读文件系统操作 | 禁止系统盘保护目录（`Windows`、`Program Files`、`ProgramData`），禁止设备路径（`\\.\`、`\\?\`），禁止目录遍历（`..`） |
| **Write** | 写入文件系统 | 仅允许 foobar2000 配置目录和系统临时目录 |
| **MediaRead** | 读取媒体文件元数据/内容 | Read 规则 + 文件必须在 foobar2000 媒体库或播放列表中 |
| **MediaWrite** | 修改媒体文件（标签/歌词等） | MediaRead 规则 + 黑名单目录校验 + 无非系统盘自动放行 |

::: tip 权限层级关系
`None < Read < Write`（独立写入通道）  
`None < Read < MediaRead < MediaWrite`（媒体通道）  

Write 和 MediaWrite 是两条独立通道：Write 严格限制在配置/临时目录，MediaWrite 限制在媒体库范围。
:::

## 错误响应

当路径校验失败时，API 返回统一的错误信封：

```json
{
  "error": "PERMISSION_DENIED",
  "details": "Path rejected by security policy: C:\\Windows\\System32\\config.ini"
}
```

主题脚本可以捕获此错误并提示用户：

```javascript
try {
  const data = await fb2k.invoke('file.read', { path: somePath });
} catch (e) {
  if (e.error === 'PERMISSION_DENIED') {
    console.warn('路径被安全策略拒绝:', e.details);
  }
}
```

## API 权限对照表

下表列出所有受路径校验保护的 API 端点（共 64 条 spec，覆盖 62 个唯一端点）。

### Read — 只读文件系统（9 条）

| API | 参数 | 数组 | 说明 |
|-----|------|------|------|
| `clipboard.writeFiles` | `paths` | ? | 复制文件路径到剪贴板 |
| `file.copy` | `source` | — | 复制文件（源路径） |
| `file.exists` | `path` | — | 检查文件是否存在 |
| `file.getInfo` | `path` | — | 获取文件信息 |
| `file.list` | `path` | — | 列出目录内容 |
| `file.read` | `path` | — | 读取文件内容 |
| `shell.openWith` | `path` | — | 用外部程序打开文件 |
| `shell.showInExplorer` | `path` | — | 在资源管理器中显示 |
| `artwork.getFolderImages` | `directory` | — | 获取文件夹中的图片 |

### Write — 写入文件系统（8 条 / 6 个 API）

| API | 参数 | 数组 | 说明 |
|-----|------|------|------|
| `file.copy` | `destination` | — | 复制文件（目标路径） |
| `file.delete` | `path` | — | 删除文件 |
| `file.mkdir` | `path` | — | 创建目录 |
| `file.move` | `source` | — | 移动文件（源路径） |
| `file.move` | `destination` | — | 移动文件（目标路径） |
| `file.rename` | `path` | — | 重命名文件 |
| `file.write` | `path` | — | 写入文件内容 |
| `http.download` | `saveTo` | — | 下载文件到指定路径 |

::: warning 注意
`file.copy` 同时有 Read（source）和 Write（destination）两条校验。  
`file.move` 对 source 和 destination 均执行 Write 校验。
:::

### MediaRead — 读取媒体文件（37 条）

| API | 参数 | 数组 | 说明 |
|-----|------|------|------|
| `artwork.getAvailableArtwork` | `path` | — | 获取可用封面 |
| `artwork.getAvailableTypes` | `path` | — | 获取封面类型 |
| `artwork.getBatch` | `paths` | ? | 批量获取封面 |
| `artwork.getByPath` | `path` | — | 按路径获取封面 |
| `artwork.getFb2kUrlByPath` | `path` | — | 获取封面 fb2k URL |
| `artwork.getFb2kUrlByPathBatch` | `paths` | ? | 批量获取封面 URL |
| `artwork.getForTrack` | `path` | — | 获取曲目封面 |
| `artwork.getLyrics` | `path` | — | 获取歌词 |
| `artwork.getMetadata` | `path` | — | 获取元数据 |
| `audio.analyzeBPM` | `path` | — | 分析 BPM |
| `audio.generateFullWaveform` | `path` | — | 生成完整波形 |
| `audio.generateWaveform` | `path` | — | 生成波形 |
| `discovery.executeContextMenuByPath` | `trackPath` | — | 按路径执行上下文菜单 |
| `jitQueue.enqueueNext` | `url` | — | JIT 队列下一首 |
| `jitQueue.playNow` | `url` | — | JIT 队列立即播放 |
| `library.getByPath` | `path` | — | 按路径查询媒体库 |
| `lyrics.exists` | `path` | — | 检查歌词文件是否存在 |
| `lyrics.get` | `path` | — | 获取歌词 |
| `metadata.read` | `path` | — | 读取元数据 |
| `metadata.readBatch` | `paths` | ? | 批量读取元数据 |
| `metadata.readByPath` | `path` | — | 按路径读取元数据 |
| `playback.playPath` | `path` | — | 播放指定路径 |
| `playback.playPaths` | `paths` | ? | 播放多个路径 |
| `playcount.get` | `paths` | ? | 获取播放次数 |
| `playcount.getBatch` | `paths` | ? | 批量获取播放次数 |
| `playlist.addPaths` | `paths` | ? | 向播放列表添加路径 |
| `playlist.addPathsAsync` | `paths` | ? | 异步添加路径 |
| `playlist.addPathsSequential` | `paths` | ? | 顺序添加路径 |
| `playlist.replaceAllAndPlay` | `paths` | ? | 替换并播放 |
| `queue.addPaths` | `paths` | ? | 向队列添加路径 |
| `rating.get` | `path` | — | 获取评分 |
| `replaygain.get` | `paths` | ? | 获取 ReplayGain 值 |
| `replaygain.scan` | `paths` | ? | 扫描 ReplayGain |
| `titleformat.eval` | `path` | — | 评估 Titleformat |
| `titleformat.evalBatch` | `paths` | ? | 批量评估 Titleformat |
| `titleformat.evalFields` | `path` | — | 评估 Titleformat 字段 |
| `titleformat.evalFieldsBatch` | `paths` | ? | 批量评估字段 |

### MediaWrite — 修改媒体文件（10 条）

| API | 参数 | 数组 | 嵌套键 | 说明 |
|-----|------|------|--------|------|
| `lyrics.save` | `path` | — | — | 保存歌词文件 |
| `metadata.embedArtwork` | `path` | — | — | 嵌入封面到文件 |
| `metadata.removeEmbeddedArt` | `path` | — | — | 移除嵌入封面 |
| `metadata.removeField` | `path` | — | — | 移除元数据字段 |
| `metadata.removeTag` | `path` | — | — | 移除完整标签 |
| `metadata.write` | `path` | — | — | 写入元数据 |
| `metadata.writeBatch` | `items` | ? | `path` | 批量写入元数据 |
| `playcount.set` | `path` | — | — | 设置播放次数 |
| `rating.set` | `path` | — | — | 设置评分 |
| `replaygain.clear` | `paths` | ? | — | 清除 ReplayGain |

::: info 嵌套数组校验
`metadata.writeBatch` 的参数 `items` 是对象数组，系统自动提取每个元素的 `path` 字段进行校验。
:::

## 自定义策略 API

以下 API 的安全策略由内部逻辑自行管理，不通过 decorator 校验：

| API | 校验方式 |
|-----|----------|
| `shell.exec` | 无命令白名单（信任主题作者）+ cwd 路径校验 |
| `shell.spawn` | 无可执行白名单（信任主题作者）+ 绝对路径/cwd 路径校验 |
| `console.log` | 日志目录限制 + 保留设备名过滤 + 文件扩展名白名单（.log/.txt） |
| `playlist.insertTracks` | 参数为 playlist handle 而非文件路径，不需要路径校验 |

## 路径安全规则详解

### 通用拦截（所有级别）

- **设备路径**: `\\.\` 和 `\\?\` 前缀一律拒绝
- **目录遍历**: 包含 `..` 的路径一律拒绝
- **空路径 / 相对路径**: 必须是绝对路径

### Read 级别

系统盘（通常为 `C:`）上的以下目录被禁止访问：

| 保护目录 | 原因 |
|----------|------|
| `C:\Windows\` | 系统文件 |
| `C:\Program Files\` | 程序文件 |
| `C:\Program Files (x86)\` | 32 位程序文件 |
| `C:\ProgramData\` | 系统配置数据 |

非系统盘（`D:`、`E:` 等）默认放行，以支持 NAS 和便携版场景。

### Write 级别

仅允许写入以下两个目录（及其子目录）：

| 允许目录 | 用途 |
|----------|------|
| foobar2000 配置目录 | `%APPDATA%\foobar2000\` 或便携版 profile |
| 系统临时目录 | `%TEMP%` |

### MediaRead 级别

在 Read 规则之上，额外要求文件必须存在于：
- foobar2000 **媒体库** 中，或
- 任意 **播放列表** 中（遍历全部播放列表，上限 50,000 项）

::: tip 路径规范化
输入路径会通过 `filesystem::g_get_canonical_path` 规范化后再比较，支持大小写不敏感、斜杠统一、CUE 子轨道匹配等场景。
:::

### MediaWrite 级别

在 MediaRead 规则之上，额外增加：
- **黑名单目录校验** — 即使文件在媒体库中，处于系统保护目录的文件仍然拒绝写入
- **无非系统盘自动放行** — 与 Read 不同，MediaWrite 不会因为路径在非系统盘就自动放行

## 统计

| 权限级别 | spec 条数 | 唯一 API 数 | 含数组参数 | 含嵌套键 |
|----------|-----------|-------------|-----------|----------|
| Read | 9 | 9 | 1 | 0 |
| Write | 8 | 6 | 0 | 0 |
| MediaRead | 37 | 37 | 17 | 0 |
| MediaWrite | 10 | 10 | 2 | 1 |
| **合计** | **64** | **62** | **20** | **1** |

::: tip 为什么 spec 数 > API 数？
`file.copy` 同时有 Read（source）+ Write（destination）两条 spec；`file.move` 有两条 Write spec。因此 64 条 spec 对应 62 个唯一 API。
:::
