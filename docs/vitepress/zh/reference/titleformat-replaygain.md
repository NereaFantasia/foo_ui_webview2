# Titleformat & ReplayGain

通用标题格式化与 ReplayGain 交叉功能参考，主端点文档详见：

- [Titleformat API](/zh/api/titleformat)
- [ReplayGain API](/zh/api/replaygain)

## titleformat.eval

对单个音频文件执行单一 TitleFormat 表达式求值。

## titleformat.evalBatch

对多个音频文件批量执行同一 TitleFormat 表达式求值。

## titleformat.evalFields

对单个音频文件批量评估多个具名字段表达式（结构化 UI 推荐）。

## titleformat.evalFieldsBatch

对多个音频文件批量评估多个具名字段表达式（适用于自定义播放列表列）。

## titleformat.getBuiltinFields

获取内置常用 TitleFormat 字段表达式字典映射。

**典型场景**:

- 自定义播放列表列（如 `%play_count%`、`%rating%`、`%added%`）
- 条件分支展示（如 `$if(%album%,%album%,Unknown)`）
- 读取已安装 foo_playcount 插件提供的扩展统计字段

## replaygain.get

读取指定路径文件的 ReplayGain 音频增益信息（包括 `trackGain`、`trackPeak`、`albumGain`、`albumPeak` 及各条目状态字段）。

## replaygain.scan

触发针对目标路径的 ReplayGain 音频增益扫描任务（通过底层上下文菜单工作流实现）。

## replaygain.clear

清除指定路径文件的 ReplayGain 增益元数据。

## 相关配置 API

模式与前置增益（Preamp）相关配置端点位于 Audio 文档页：

- `replaygain.getMode`
- `replaygain.setMode`
- `replaygain.getPreamp`
- `replaygain.setPreamp`
- `replaygain.getSettings`
