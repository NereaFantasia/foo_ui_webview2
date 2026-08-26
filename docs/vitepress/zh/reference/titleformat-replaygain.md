# Titleformat & ReplayGain

通用标题格式化与 ReplayGain 交叉参考。详见：

- [Titleformat API](/zh/api/titleformat)
- [Audio / ReplayGain API](/zh/api/audio)

## titleformat.eval

对单个文件求值一个 titleformat 表达式。


## titleformat.evalBatch

对多个文件批量求值同一表达式。


## titleformat.evalFields

对单个文件求值多个字段（推荐）。


## titleformat.evalFieldsBatch

对多个文件批量求值多个字段（适合自定义列表列）。


## titleformat.getBuiltinFields

获取内置字段参考。

**用途场景**:

- 自定义列表列（播放次数、评分、添加日期等）
- 条件显示（如 `$if(%album%,%album%,Unknown)`）
- 获取 foo_playcount 数据（`%play_count%`, `%rating%`, `%last_played%`）


## replaygain.get

获取文件的 ReplayGain 信息（`trackGain`, `trackPeak`, `albumGain`, `albumPeak` 及 handler 返回的状态字段）。


## replaygain.scan

触发 ReplayGain 扫描（通过 handler 暴露的上下文菜单工作流）。


## replaygain.clear

清除文件的 ReplayGain 信息。


## 相关配置 API

模式/前置增益相关接口见 Audio 页：

- `replaygain.getMode`
- `replaygain.setMode`
- `replaygain.getPreamp`
- `replaygain.setPreamp`
- `replaygain.getSettings`
