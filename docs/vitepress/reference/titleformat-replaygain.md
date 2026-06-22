# Titleformat & ReplayGain

通用标题格式化评估。v1.1.13+

## titleformat.eval 

评估单个文件的单个 titleformat 表达式。

## titleformat.evalBatch 

批量评估多个文件的同一表达式。

## titleformat.evalFields 

评估单个文件的多个字段（推荐）。

## titleformat.evalFieldsBatch 

批量评估多个文件的多个字段（适合自定义列表列）。

## titleformat.getBuiltinFields 

获取内置字段参考。

**用途场景**:

- 自定义列表列（播放次数、评分、添加日期等）
- 条件显示（如 `$if(%album%,%album%,Unknown)`）
- 获取 foo_playcount 数据（`%play_count%`, `%rating%`, `%last_played%`）

## replaygain.get 

获取文件的 ReplayGain 信息（trackGain, trackPeak, albumGain, albumPeak）。

## replaygain.scan 

触发 ReplayGain 扫描（通过上下文菜单）。

## replaygain.clear 

清除文件的 ReplayGain 信息。
