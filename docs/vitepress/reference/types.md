# Shared Types

Shapes that several namespaces return. A method's own return table links here instead of repeating the fields.

## Track

<!-- api-schema:begin type:Track -->
One track, as every method whose declaration returns whole rows reports it: the playing track, a media library row, a playlist row, a queue entry. Containers add their own fields (a row number, a queue position) on top of these.

| Field | Type | Description |
| --- | --- | --- |
| `handle` | `string` | The key that identifies the track across endpoints: `absolutePath`, with a `\|subsong:N` suffix when `subsong` is not `0`. The same track yields the same handle from every such method; because of the suffix it is not a plain file path, and a method that takes a path documents whether it accepts one. |
| `path` | `string` | Path as foobar2000 stores it: `file://` for a local file, `file-relative://` for a path stored relative to the foobar2000 folder (a portable install), or a remote URL. No subsong suffix. |
| `absolutePath` | `string` | Native filesystem path without the subsong suffix; the same as `path` for a remote URL. |
| `subsong` | `integer` | Subsong identifier the decoder assigns inside the file, not necessarily a sequence number; `0` for a whole file and for a remote stream. |
| `title` | `string` | First TITLE value; empty when untagged. |
| `artist` | `string` | Every ARTIST value joined with `", "`; empty when untagged. |
| `artists` | `string[]` | Every ARTIST value in tag order; empty when untagged. |
| `album` | `string` | First ALBUM value; empty when untagged. |
| `albumArtist` | `string` | Every ALBUM ARTIST value joined with `", "`; empty when untagged. |
| `albumArtists` | `string[]` | Every ALBUM ARTIST value in tag order, so `albumArtists.join(", ")` equals `albumArtist`; empty when untagged. `library.getAlbums` files a track that has an `album` under the name `album` and the album artist `albumArtists[0]`, or `artists[0]` when this array is empty (`""` when both are); those are the `name` and `albumArtist` of that album's row. |
| `genre` | `string` | Every GENRE value joined with `", "`; empty when untagged. |
| `date` | `string` | First DATE value as tagged, such as `2019` or `2019-05-01`; empty when untagged. |
| `trackNumber` | `integer` | TRACKNUMBER read as an integer; `0` when absent or not a number. |
| `discNumber` | `integer` | DISCNUMBER read as an integer; `0` when absent or not a number. |
| `duration` | `number` | Length in seconds; `0` when unknown. |
| `fileSize` | `integer` | File size in bytes; `-1` when unknown, as for a remote stream. |
| `bitrate` | `integer` | Average bitrate in kbit/s; `0` when unknown. |
| `sampleRate` | `integer` | Sample rate in Hz; `0` when unknown. |
| `channels` | `integer` | Channel count; `0` when unknown. |
| `codec` | `string` | Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown. |
| `rating` | `integer` | Rating from 0 to 5: the `%rating%` statistic (foo_playcount) when it is 1 to 5, otherwise the RATING tag clamped to that range; `0` when neither rates the track. |
<!-- api-schema:end -->

## TrackPartial

<!-- api-schema:begin type:TrackPartial -->
A track projected down to the fields a caller asked for.

Every field of [Track](./types.md#track), each optional.
<!-- api-schema:end -->

## SystemApiInfo

<!-- api-schema:begin type:SystemApiInfo -->
One registered method.

| Field | Type | Description |
| --- | --- | --- |
| `fullName` | `string` | Full method name, `namespace.method`. |
| `plugin` | `string` | Display name of the owning plugin; `foo_ui_webview2` for built-in methods. |
| `namespace` | `string` | Namespace part of the name. |
| `method` | `string` | Method part of the name. |
| `description` | `string` | Description given at registration; empty when none. |
| `version` | `string` | Version given at registration. |
| `isExternal` | `boolean` | `true` when an external plugin registered it. |
<!-- api-schema:end -->

## SystemPluginInfo

<!-- api-schema:begin type:SystemPluginInfo -->
One registered external plugin.

| Field | Type | Description |
| --- | --- | --- |
| `name` | `string` | Display name. |
| `namespace` | `string` | Namespace the plugin owns; unique among plugins. |
| `version` | `string` | Plugin version. |
| `author` | `string` | Author. |
| `description` | `string` | Description. |
| `apiCount` | `integer` | Number of entries in `apis`. |
| `apis` | `string[]` | Full names of the methods the plugin registered. |
<!-- api-schema:end -->
