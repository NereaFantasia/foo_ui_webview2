# 共享类型

多个命名空间共同返回的形状。各方法自己的返回值表只链接到这里，不重复列字段。

## Track

<!-- api-schema:begin type:Track -->
一首曲目；声明中返回整行曲目的方法（正在播放、媒体库行、播放列表行、队列项）都用这一形状，容器再在它之上加自己的字段（行号、队列位置等）。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `handle` | `string` | 跨端点识别同一首曲目的键：`absolutePath`，`subsong` 不为 `0` 时带 `\|subsong:N` 后缀。同一首曲目在这些方法里得到同一个 handle；因为带后缀，它不是普通文件路径，接收路径的方法会各自说明是否接受它。 |
| `path` | `string` | foobar2000 存储的路径：本地文件是 `file://`，相对 foobar2000 目录存储的路径（便携安装）是 `file-relative://`，网络流是 URL。不带子曲目后缀。 |
| `absolutePath` | `string` | 不带子曲目后缀的原生文件系统路径；网络流与 `path` 相同。 |
| `subsong` | `integer` | 解码器在文件内分配的子曲目标识，不一定是连续序号；整个文件和网络流都是 `0`。 |
| `title` | `string` | 第一个 TITLE 值；没有标签时为空。 |
| `artist` | `string` | 全部 ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `artists` | `string[]` | 全部 ARTIST 值，按标签顺序；没有标签时为空数组。 |
| `album` | `string` | 第一个 ALBUM 值；没有标签时为空。 |
| `albumArtist` | `string` | 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `albumArtists` | `string[]` | 全部 ALBUM ARTIST 值，按标签顺序，`albumArtists.join(", ")` 等于 `albumArtist`；没有标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑，本数组为空时取 `artists[0]`（两者都空时为 `""`）；这两个值就是该专辑行的 `name` 与 `albumArtist`。 |
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
| `rating` | `integer` | 评分 0 到 5：`%rating%` 统计值（foo_playcount）在 1 到 5 之间时用它，否则用 RATING 标签并夹到该范围；两处都没有评分时为 `0`。 |
<!-- api-schema:end -->

## TrackPartial

<!-- api-schema:begin type:TrackPartial -->
按调用方要求的字段投影后的曲目。

包含 [Track](./types.md#track) 的全部字段，每个都可选。
<!-- api-schema:end -->

## SystemApiInfo

<!-- api-schema:begin type:SystemApiInfo -->
一个已注册的方法。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `fullName` | `string` | 完整方法名，形如 `namespace.method`。 |
| `plugin` | `string` | 所属插件的显示名；内置方法为 `foo_ui_webview2`。 |
| `namespace` | `string` | 名字里的命名空间部分。 |
| `method` | `string` | 名字里的方法部分。 |
| `description` | `string` | 注册时给的描述；没有时为空。 |
| `version` | `string` | 注册时给的版本。 |
| `isExternal` | `boolean` | 由外部插件注册时为 `true`。 |
<!-- api-schema:end -->

## SystemPluginInfo

<!-- api-schema:begin type:SystemPluginInfo -->
一个已注册的外部插件。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `name` | `string` | 显示名。 |
| `namespace` | `string` | 插件拥有的命名空间；插件之间唯一。 |
| `version` | `string` | 插件版本。 |
| `author` | `string` | 作者。 |
| `description` | `string` | 描述。 |
| `apiCount` | `integer` | `apis` 的条数。 |
| `apis` | `string[]` | 插件注册的方法全名。 |
<!-- api-schema:end -->
