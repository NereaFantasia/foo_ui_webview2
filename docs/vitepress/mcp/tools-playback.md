# Playback Tools

Twelve tools provide playback control, state inspection, seeking, and volume management.

## Playback control

| Tool | Bridge method | Parameters | Result | Description |
| --- | --- | --- | --- | --- |
| `fb2k_playback_play` | `playback.play` | none | `success: true` | Start or resume playback |
| `fb2k_playback_pause` | `playback.pause` | none | `success: true` | Pause playback |
| `fb2k_playback_stop` | `playback.stop` | none | `success: true` | Stop playback |
| `fb2k_playback_next` | `playback.next` | none | `success: true` | Skip to the next track |
| `fb2k_playback_previous` | `playback.previous` | none | `success: true` | Go to the previous track |
| `fb2k_playback_play_pause` | `playback.playPause` | none | `success: true`; `isPlaying: boolean` | Toggle play and pause; `isPlaying` reports the playing state after the toggle |

## State

### fb2k_playback_get_state

Gets the current playback state and capability flags.

- **Parameters**: none
- **Bridge method**: `playback.getState`

**Example result:**

```json
{
  "state": "playing",
  "canSeek": true,
  "canPause": true
}
```

| Field | Type | Description |
| --- | --- | --- |
| `state` | string | `playing`, `paused`, or `stopped` |
| `canSeek` | boolean | Whether seeking is available |
| `canPause` | boolean | Whether pausing is available |

### fb2k_playback_get_current_track

Gets detailed information about the current track. When nothing is playing, the result reports `found: false`.

- **Parameters**: none
- **Bridge method**: `playback.getCurrentTrack`

**Example result while playing:**

```json
{
  "id": "D:\\Music\\Mili\\Redo.flac",
  "title": "Redo",
  "artist": "Mili",
  "album": "Millennium Mother",
  "albumArtist": "Mili",
  "genre": "Alternative",
  "date": "2018",
  "trackNumber": 1,
  "discNumber": 1,
  "duration": 263.5,
  "path": "file://D:\\Music\\Mili\\Redo.flac",
  "absolutePath": "D:\\Music\\Mili\\Redo.flac",
  "fullPath": "D:\\Music\\Mili\\Redo.flac",
  "subsong": 0,
  "fileSize": 28456789,
  "bitrate": 876,
  "sampleRate": 44100,
  "channels": 2,
  "codec": "FLAC"
}
```

| Field | Type | Description |
| --- | --- | --- |
| `id` | string | Full-path identity, including a subsong suffix when applicable |
| `title` | string | Track title |
| `artist` | string | Track artist |
| `album` | string | Album title |
| `albumArtist` | string | Album artist |
| `genre` | string | Genre |
| `date` | string | Release date |
| `trackNumber` | integer | Track number |
| `discNumber` | integer | Disc number |
| `duration` | number | Duration in seconds |
| `path` | string | Original foobar2000 path |
| `absolutePath` | string | Normalized absolute path without a subsong suffix |
| `fullPath` | string | Absolute path plus an optional `|subsong:N` suffix |
| `subsong` | integer | Subsong index, for example for a CUE track |
| `fileSize` | integer | File size in bytes |
| `bitrate` | integer | Bitrate in kbps |
| `sampleRate` | integer | Sample rate in Hz |
| `channels` | integer | Channel count |
| `codec` | string | Codec name |

**Example result while stopped:**

```json
{ "success": true, "found": false, "playing": false }
```

### fb2k_playback_get_position

Gets the playback position and total duration.

- **Parameters**: none
- **Bridge method**: `playback.getPosition`

**Example result:**

```json
{
  "position": 45.2,
  "duration": 263.5,
  "subsong": 0,
  "path": "D:\\Music\\Mili\\Redo.flac"
}
```

| Field | Type | Description |
| --- | --- | --- |
| `position` | number | Current position in seconds |
| `duration` | number | Total duration in seconds |
| `subsong` | integer | Subsong index |
| `path` | string | Current file path |

## Seeking and volume

### fb2k_playback_set_position

Seeks to a playback position.

- **Bridge method**: `playback.setPosition`

| Parameter | Type | Required | Constraints |
| --- | --- | --- | --- |
| `seconds` | number | Yes | Target position in seconds; minimum `0` |

### fb2k_playback_get_volume

Gets the current volume and mute state.

- **Parameters**: none
- **Bridge method**: `playback.getVolume`

**Example result:**

```json
{
  "volume": 75,
  "volumeDb": -6.2,
  "muted": false,
  "isMuted": false
}
```

| Field | Type | Description |
| --- | --- | --- |
| `volume` | number | Linear volume from `0` to `100` |
| `volumeDb` | number | Volume in dB |
| `muted` | boolean | Mute state |
| `isMuted` | boolean | Alias of `muted` |

### fb2k_playback_set_volume

Sets the playback volume.

- **Bridge method**: `playback.setVolume`

| Parameter | Type | Required | Constraints |
| --- | --- | --- | --- |
| `volume` | number | Yes | Linear volume from `0` to `100` inclusive |
