# 底层 API 概述 

底层 API 是 foo_ui_webview2 的原生通信接口，通过 `fb2k.invoke()` 直接与 C++ 后端交互。

## 与 SDK 的区别 

| 维度 | SDK (fb.*) | 底层 API (fb2k.invoke) |
| --- | --- | --- |
| 调用方式 | fb.player.play() | fb2k.invoke('playback.play') |
| 一致性 | SDK 含缓存层，部分属性为最终一致 | 强一致，每次都从后端获取真实值 |
| 适用场景 | 日常开发、UI 渲染 | 事务性操作、精确控制、错误处理 |
| 依赖 | 需加载 bridge.js | 插件自动注入 window.fb2k |

## 何时使用底层 API 

- 需要获取后端返回值来做分支判断（SDK 的 fire-and-forget 写入无返回值）
- 批量操作需要每步确认成功后再继续
- SDK 未封装的 API（如多窗口管理 `window.createPopup` 等）
- 需要精确错误信息的场景

## 调用方式 

```javascript
// 所有调用都返回 Promise
const result = await fb2k.invoke('namespace.method', { param1: value1 });

// 事件监听
const unsubscribe = fb2k.on('namespace:eventName', (data) => { ... });
unsubscribe(); // 取消
```

> 方法名使用 **dot 格式**（`playback.play`），事件名使用 **colon 格式**（`playback:trackChanged`）。

## API 命名空间 

| 命名空间 | 描述 | 数量 |
| --- | --- | --- |
| playback | 播放控制、状态、音量 | 27 |
| playlist | 播放列表管理、智能列表 | 45 |
| library | 媒体库浏览、搜索 | 21 |
| artwork | 封面获取、fb2k:// 协议 | 12 |
| window | 窗口控制、DWM 效果、多窗口 | 72 |
| taskbar | 任务栏缩略图按钮、进度条、覆盖图标 | 5 |
| tray | 系统托盘图标、右键菜单、行为控制 | 13 |
| config | 配置、设备、持久化存储 | 29 |
| metadata | 元数据读写 | 9 |
| audio | 音频分析、频谱 | 12 |
| queue / jitQueue | 播放队列 | 15 |
| discovery | 服务发现、上下文菜单 | 15 |
| 其他 | file、dialog、shell、http、ui、keyboard、lyrics、console、dnd、dsp、output、replaygain、playcount、titleformat、selection、menu、misc、panel、test | 111 |

详细文档见左侧各命名空间页面。
