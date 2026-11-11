# Shared Types

Common data shapes returned across multiple namespaces. Method-specific return tables reference these definitions to avoid duplication.

## Track

<!-- api-schema:begin type:Track -->
Metadata and playback properties of a single track, shared by playback, library, playlist, and queue endpoints. Containers extend this structure with domain-specific fields such as playlist row indices or queue positions.

| Field | Type | Description |
| --- | --- | --- |
| `handle` | `string` | Cross-endpoint unique track identifier: formatted as `absolutePath`, appended with `\|subsong:N` when `subsong` is non-zero. The same track yields the same handle from every method. Differs from standard filesystem paths; endpoints document whether handle identifiers are accepted in place of paths. |
| `path` | `string` | Path as foobar2000 stores it: `file://` for a local file, `file-relative://` for a path stored relative to the foobar2000 folder (a portable install), or a remote URL. No subsong suffix. |
| `absolutePath` | `string` | Native filesystem path without the subsong suffix; the same as `path` for a remote URL. |
| `subsong` | `integer` | Subsong identifier the decoder assigns inside the file, not necessarily a sequence number; `0` for a whole file and for a remote stream. |
| `title` | `string` | First TITLE value; empty when untagged. |
| `artist` | `string` | Every ARTIST value joined with `", "`; empty when untagged. |
| `artists` | `string[]` | Every ARTIST value in tag order; empty when untagged. |
| `album` | `string` | First ALBUM value; empty when untagged. |
| `albumId` | `string` | Opaque album identity when its original name or album artist contains invalid UTF-8. Use this instead of the display names to match an album row; absent for ordinary names. |
| `albumArtist` | `string` | Every ALBUM ARTIST value joined with `", "`; empty when untagged. |
| `albumArtists` | `string[]` | All ALBUM ARTIST values in tag order (`albumArtists.join(", ")` equals `albumArtist`); empty array when untagged. In `library.getAlbums`, a track that has an `album` is filed under the name `album` and the album artist `albumArtists[0]` (falling back to `artists[0]`, or `""` when both are empty); those are the `name` and `albumArtist` of that album's row. When `albumId` is present, match by `albumId` instead of display names to avoid collision from character replacement. |
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
| `rating` | `integer` | Rating value from 0 to 5. Prefers the `%rating%` statistic (foo_playcount) if within 1..5; otherwise falls back to the RATING tag clamped to 0..5; `0` if neither is present. |
<!-- api-schema:end -->

## TrackPartial

<!-- api-schema:begin type:TrackPartial -->
Track representation containing a subset of fields requested by the caller.

Every field of [Track](./types.md#track), each optional.
<!-- api-schema:end -->

## SystemApiInfo

<!-- api-schema:begin type:SystemApiInfo -->
One registered method.

| Field | Type | Description |
| --- | --- | --- |
| `fullName` | `string` | Full method name, `namespace.method`. |
| `plugin` | `string` | Display name of the owning plugin; `foo_ui_webview2` for built-in methods. |
| `namespace` | `string` | Namespace component of the method name. |
| `method` | `string` | Method name component without namespace. |
| `description` | `string` | Method description provided during registration; empty string if omitted. |
| `version` | `string` | Method API version declared during registration. |
| `isExternal` | `boolean` | Whether the method was registered by an external plugin. |
<!-- api-schema:end -->

## SystemPluginInfo

<!-- api-schema:begin type:SystemPluginInfo -->
One registered external plugin.

| Field | Type | Description |
| --- | --- | --- |
| `name` | `string` | Plugin display name. |
| `namespace` | `string` | Namespace unique to this plugin. |
| `version` | `string` | Plugin version string. |
| `author` | `string` | Plugin author. |
| `description` | `string` | Plugin description. |
| `apiCount` | `integer` | Number of methods in `apis`. |
| `apis` | `string[]` | Full method names registered by the plugin. |
<!-- api-schema:end -->
