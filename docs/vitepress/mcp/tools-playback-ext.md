# Playback Extended Tools

Thirteen tools cover mute control, volume steps, playback order, stop-after-current, track location, path playback, and random playback.

## Mute and volume steps

| Tool | Bridge method | Parameters | Description |
| --- | --- | --- | --- |
| `fb2k_playback_mute` | `playback.mute` | optional `muted: boolean`, default `true` | Set mute state |
| `fb2k_playback_toggle_mute` | `playback.toggleMute` | none | Toggle mute state |
| `fb2k_playback_volume_up` | `playback.volumeUp` | none | Increase volume by one step |
| `fb2k_playback_volume_down` | `playback.volumeDown` | none | Decrease volume by one step |

## Playback order

### fb2k_playback_get_playback_order

Gets the current playback-order mode.

- **Parameters**: none
- **Bridge method**: `playback.getPlaybackOrder`

**Example result:**

```json
{
  "order": 0,
  "orderName": "default",
  "name": "default",
  "orderIndex": 0
}
```

| Field | Type | Description |
| --- | --- | --- |
| `order` | integer | Playback-order index from `0` to `6` |
| `orderName` | string | Playback-order name |
| `name` | string | Alias of `orderName` |
| `orderIndex` | integer | Alias of `order` |

### fb2k_playback_set_playback_order

Sets the playback-order mode.

- **Bridge method**: `playback.setPlaybackOrder`

| Parameter | Type | Required | Values |
| --- | --- | --- | --- |
| `order` | integer or string | Yes | Integer index `0` to `6`, or `default`, `repeat-playlist`, `repeat-track`, `random`, `shuffle-tracks`, `shuffle-albums`, or `shuffle-folders` |

## Stop after current

### fb2k_playback_get_stop_after_current

Gets the stop-after-current state.

- **Parameters**: none
- **Bridge method**: `playback.getStopAfterCurrent`

**Example result:**

```json
{ "enabled": false }
```

### fb2k_playback_set_stop_after_current

Sets stop-after-current.

- **Bridge method**: `playback.setStopAfterCurrent`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | boolean | Yes | Whether stop-after-current is enabled |

### fb2k_playback_toggle_stop_after_current

Toggles stop-after-current.

- **Parameters**: none
- **Bridge method**: `playback.toggleStopAfterCurrent`

## Track location

### fb2k_playback_get_current_track_index

Gets the current track's playlist and item indices.

- **Bridge method**: `playback.getCurrentTrackIndex`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeTrackInfo` | boolean | No | Include full track details; default `false` |

**Example result when found:**

```json
{
  "success": true,
  "found": true,
  "playlist": 0,
  "index": 5
}
```

**Example result when not found:**

```json
{
  "success": true,
  "found": false,
  "playlist": null,
  "index": null
}
```

::: tip Track details
With `includeTrackInfo: true`, the result also contains a `track` field with the same detailed track data exposed by `playback.getCurrentTrack`.
:::

### fb2k_playback_get_playing_playlist

Gets the playing playlist index and name.

- **Parameters**: none
- **Bridge method**: `playback.getPlayingPlaylist`

**Example result when found:**

```json
{
  "success": true,
  "found": true,
  "playlist": 0,
  "name": "Default"
}
```

**Example result when not found:**

```json
{ "success": true, "found": false, "playlist": null }
```

### fb2k_playback_play_path

Plays a file path, including a subsong suffix when needed.

- **Bridge method**: `playback.playPath`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | Yes | File path, optionally using `path|subsong:N` |

::: tip Subsong format
For CUE files or multi-track containers, append a subsong index such as `C:\\music\\album.flac|subsong:2`.
:::

### fb2k_playback_random

Starts a random track.

- **Parameters**: none
- **Bridge method**: `playback.random`
