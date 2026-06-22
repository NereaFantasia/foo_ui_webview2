# 概述 

> **foobar2000 最低版本**: 2.0+

**foo_ui_webview2** 是一个 foobar2000 插件，使用 WebView2 技术替换默认用户界面，允许开发者使用现代 Web 技术（HTML/CSS/JavaScript）构建自定义 UI。

::: tip 从 Spider Monkey Panel (SMP) 迁移？
v1.2.0 提供完整的 SMP 兼容层，包含：
- **35 个 SMP 事件映射** — `on_playback_new_track` 等 SMP 事件名直接可用
- **8 个包装器类** — FbMetadbHandle、FbTitleFormat、ContextMenuManager 等
- **完整的 `fb` / `plman` 对象** — 同步属性、播放控制、播放列表管理

核心工作原理：通过「缓存 + 事件」将异步 API 包装为同步属性，以兼容 SMP 的同步语义。这是**最终一致性**模型（而非强一致），适合 UI 渲染场景；对于需要精确控制的事务性操作，应直接使用底层 `fb2k.invoke()` API。⚠️ SMP 的 GDI/GDI+ 绘图（`on_paint(gr)` 等）无法迁移，UI 渲染层需要用 HTML/CSS/Canvas 重写。→ [查看 SMP 兼容层完整文档](/reference/smp-compat)
:::

## 主要特性 

## API 分布 

| 模块 | 数量 | 描述 |
| --- | --- | --- |
| playback | 27 | 播放控制、状态、音量、路径播放 |
| playlist | 45 | 播放列表管理、智能列表 |
| library | 21 | 媒体库浏览、搜索、路径查找 |
| artwork | 12 | 封面、fb2k:// URL、批量获取、服务端缩放 |
| config | 29 | 配置、设备、持久化存储、输出设置 |
| window | 72 | 窗口控制、DWM 效果、多窗口系统 |
| system | 9 | 主题/DPI/语言、API 发现 |
| file | 10 | 文件读写、复制、移动、目录操作 |
| dialog | 4 | 文件/文件夹对话框 |
| clipboard | 4 | 剪贴板操作 |
| shell | 4 | 系统集成 |
| http | 4 | HTTP 请求 |
| ui | 5 | 自定义菜单、通知 |
| keyboard | 4 | 全局快捷键 |
| lyrics | 3 | 歌词管理 |
| metadata | 9 | 元数据读写、批量读取 |
| audio | 12 | 音频分析、频谱、波形 |
| console | 6 | 日志输出 |
| dnd | 4 | 拖放支持 |
| queue | 8 | 播放队列管理 |
| jitQueue | 7 | JIT 即时队列 |
| discovery | 15 | 服务发现、上下文菜单 |
| dsp | 8 | DSP 效果器管理 |
| output | 3 | 音频输出设备 |
| replaygain | 8 | ReplayGain 设置 |
| playcount | 6 | 播放统计、评分 |
| titleformat | 5 | Titleformat 表达式评估 |
| selection | 6 | 选择同步、Viewer 模式 |
| menu | 5 | 主菜单/上下文菜单命令 |
| misc | 9 | 路径查询、首选项、控制台 |
| panel | 2 | 面板模式检测 |
| test | 2 | 测试工具 |

## 设计原则 

> foo_ui_webview2 遵循"插件做能力，应用做策略"的设计原则。

### 插件层职责（foo_ui_webview2 提供） 

| 类别 | 描述 | 示例 |
| --- | --- | --- |
| foobar2000 核心能力 | 播放控制、播放列表、媒体库、封面 | playback.*, playlist.* |
| 窗口管理 | DWM 效果、标题栏、缩放 | window.* |
| 系统集成 | 文件操作、对话框、Shell | file.*, dialog.* |
| 音频分析 | 频谱、波形 | audio.* |
| 配置存储 | 持久化 key-value 存储 | config.* |
| 流媒体能力 | 通用 URL 播放、即时队列 | jitQueue.* |

### 应用层职责（开发者实现） 

| 类别 | 描述 | 为什么不在插件层 |
| --- | --- | --- |
| 在线服务集成 | Spotify、网易云、QQ 音乐等 | 业务逻辑、授权流程各异 |
| 歌词多源聚合 | 在线搜索、翻译合并 | 业务逻辑 |
| 封面缓存策略 | 预加载、LRU 缓存 | 策略选择 |
| UI 主题与布局 | 颜色、字体、动画 | 应用特性 |

### 示例：能力 vs 策略 

```javascript
// ✅ 插件提供能力：获取封面
await fb2k.invoke('artwork.getFb2kUrl', { path: trackPath, maxSize: 200 });

// ✅ 应用层做策略：预加载/缓存
function prefetchCovers(albums) {
    albums.forEach(album => {
        const img = new Image();
        img.src = `fb2k://artwork/${encodeURIComponent(album.path)}?maxSize=200`;
    });
}
```
