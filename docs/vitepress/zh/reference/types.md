# 共享类型

多个命名空间共同返回的数据结构形状。各方法自身的返回值文档直接链接至此处，避免重复定义字段语义。

## Track

<!-- api-schema:begin type:Track -->
曲目基础元数据与回放属性，由播放状态、媒体库、播放列表与播放队列等端点通用返回。具体容器可在本结构基础上扩展行索引或队列位次等专用字段

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `handle` | `string` | 跨端点曲目唯一标识：格式为 `absolutePath`，`subsong` 非 0 时追加 `\|subsong:N` 后缀。同一首曲目在所有方法中的 handle 相同。因包含后缀故不同于普通文件路径，各路径参数接口均单独说明是否接受此标识 |
| `path` | `string` | foobar2000 存储的路径：本地文件是 `file://`，相对 foobar2000 目录存储的路径（便携安装）是 `file-relative://`，网络流是 URL。不带子曲目后缀。 |
| `absolutePath` | `string` | 不带子曲目后缀的原生文件系统路径；网络流与 `path` 相同。 |
| `subsong` | `integer` | 解码器在文件内分配的子曲目标识，不一定是连续序号；整个文件和网络流都是 `0`。 |
| `title` | `string` | 第一个 TITLE 值；没有标签时为空。 |
| `artist` | `string` | 全部 ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `artists` | `string[]` | 全部 ARTIST 值，按标签顺序；没有标签时为空数组。 |
| `album` | `string` | 第一个 ALBUM 值；没有标签时为空。 |
| `albumId` | `string` | 原始专辑名或专辑艺术家含非法 UTF-8 时提供的不透明身份标识。出现此标识时优先据此匹配专辑行，避免依赖显示名称；标准名称不包含此字段 |
| `albumArtist` | `string` | 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `albumArtists` | `string[]` | 按标签顺序排列的全部 ALBUM ARTIST 值，`albumArtists.join(", ")` 等于 `albumArtist`；无标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑（本数组为空时回退至 `artists[0]`，两者都空时为 `""`）；这两个值即该专辑行的 `name` 与 `albumArtist`。存在 `albumId` 时优先按其匹配，避免替换字符导致显示名冲突 |
| `genre` | `string` | 全部 GENRE 值用 `", "` 连接；没有标签时为空。 |
| `date` | `string` | 第一个 DATE 值，按标签原样，如 `2019` 或 `2019-05-01`；没有标签时为空。 |
| `trackNumber` | `integer` | TRACKNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `discNumber` | `integer` | DISCNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `duration` | `number` | 时长，单位秒；未知时为 `0`。 |
| `fileSize` | `integer` | 文件大小，单位字节；未知（如网络流）时为 `-1`。 |
| `bitrate` | `integer` | 平均码率，单位 kbit/s；未知时为 `0`。 |
| `sampleRate` | `integer` | 采样率，单位 Hz；未知时为 `0`。 |
| `channels` | `integer` | 声道数；未知时为 `0`。 |
| `codec` | `string` | 解码器报告的编码名，如 `FLAC`、`MP3`；未知时为空。 |
| `rating` | `integer` | 评分（0 到 5）。优先读取 1..5 范围内的 `%rating%` 统计字段（foo_playcount）；否则回退至 RATING 标签并钳位到 0..5；两处均未提供时为 0 |
<!-- api-schema:end -->

## TrackPartial

<!-- api-schema:begin type:TrackPartial -->
按请求字段投影的曲目子集对象

包含 [Track](./types.md#track) 的全部字段，每个都可选。
<!-- api-schema:end -->

## SystemApiInfo

<!-- api-schema:begin type:SystemApiInfo -->
一个已注册的方法。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `fullName` | `string` | 完整方法名，形如 `namespace.method`。 |
| `plugin` | `string` | 所属插件的显示名；内置方法为 `foo_ui_webview2`。 |
| `namespace` | `string` | 方法所属命名空间 |
| `method` | `string` | 不含命名空间的方法名 |
| `description` | `string` | 注册时声明的方法描述；未提供时为空字符串 |
| `version` | `string` | 注册时声明的 API 版本 |
| `isExternal` | `boolean` | 是否由外部插件注册 |
<!-- api-schema:end -->

## SystemPluginInfo

<!-- api-schema:begin type:SystemPluginInfo -->
一个已注册的外部插件。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `name` | `string` | 插件显示名称 |
| `namespace` | `string` | 插件独占的命名空间标识 |
| `version` | `string` | 插件版本字符串 |
| `author` | `string` | 插件作者 |
| `description` | `string` | 插件功能描述 |
| `apiCount` | `integer` | 插件注册的方法总数 |
| `apis` | `string[]` | 插件注册的方法完整全名列表 |
<!-- api-schema:end -->
