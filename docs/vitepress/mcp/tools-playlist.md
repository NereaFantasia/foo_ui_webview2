# Playlist Tools

Seven tools provide the core playlist query and management operations.

## Queries

### fb2k_playlist_get_all 

Gets all playlists.

- **Parameters**: none
- **Bridge method**: `playlist.getAll`

**Example result:**

```json
[
  {
    "index": 0,
    "name": "Default",
    "trackCount": 128,
    "isActive": true,
    "isPlaying": true,
    "isLocked": false,
    "isAutoplaylist": false
  },
  {
    "index": 1,
    "name": "Favorites",
    "trackCount": 42,
    "isActive": false,
    "isPlaying": false,
    "isLocked": false,
    "isAutoplaylist": true
  }
]
```

| Field | Type | Description |
| --- | --- | --- |
| `index` | integer | Playlist index |
| `name` | string | Playlist name |
| `trackCount` | integer | Track count |
| `isActive` | boolean | Whether this is the active playlist |
| `isPlaying` | boolean | Whether this is the playing playlist |
| `isLocked` | boolean | Whether the playlist is locked |
| `isAutoplaylist` | boolean | Whether the playlist is an autoplaylist |

### fb2k_playlist_get_active 

Gets the active playlist.

- **Parameters**: none
- **Bridge method**: `playlist.getActive`

**Example result:**

```json
{
  "index": 0,
  "name": "Default",
  "trackCount": 128,
  "isActive": true,
  "isPlaying": true,
  "isLocked": false,
  "duration": 34567.8
}
```

| Field | Type | Description |
| --- | --- | --- |
| `index` | integer | Playlist index |
| `name` | string | Playlist name |
| `trackCount` | integer | Track count |
| `isActive` | boolean | Whether this is the active playlist |
| `isPlaying` | boolean | Whether this is the playing playlist |
| `isLocked` | boolean | Whether the playlist is locked |
| `duration` | number | Total duration in seconds |

## Management

### fb2k_playlist_set_active 

Sets the active playlist.

- **Bridge method**: `playlist.setActive`

| Parameter | Type | Required | Constraints |
| --- | --- | --- | --- |
| `playlist` | integer | Yes | Zero-based playlist index; minimum `0` |

### fb2k_playlist_create 

Creates an empty playlist.

- **Bridge method**: `playlist.create`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `name` | string | Yes | Playlist name |

### fb2k_playlist_remove 

Removes a playlist.

- **Bridge method**: `playlist.remove`

| Parameter | Type | Required | Constraints |
| --- | --- | --- | --- |
| `playlist` | integer | Yes | Zero-based playlist index; minimum `0` |

## Track operations

### fb2k_playlist_get_tracks 

Gets a page of tracks from a playlist.

- **Bridge method**: `playlist.getTracks`

| Parameter | Type | Required | Constraints |
| --- | --- | --- | --- |
| `playlist` | integer | No | Declared minimum `0`; defaults to the active playlist |
| `start` | integer | No | Declared minimum `0`; Bridge default `0` |
| `count` | integer | No | Declared range `1` to `500`; Bridge default `100` |

The `ToolDefinition` description currently says default `50`, but it declares no MCP default. Omission therefore reaches `playlist.getTracks`, whose handler uses `100`; that runtime behavior is documented here.

**Example result:**

```json
{
  "playlist": 0,
  "start": 0,
  "count": 2,
  "total": 128,
  "tracks": [
    {
      "index": 0,
      "title": "Redo",
      "artist": "164 feat. GUMI",
      "album": "Millennium Mother",
      "albumArtist": "164",
      "genre": "Vocaloid",
      "date": "2011",
      "trackNumber": 1,
      "discNumber": 1,
      "duration": 263.5,
      "path": "D:\\Music\\Mili\\Redo.flac",
      "absolutePath": "D:\\Music\\Mili\\Redo.flac",
      "fileSize": 28456789,
      "subsong": 0,
      "rating": 5,
      "codec": "FLAC",
      "bitrate": 876,
      "sampleRate": 44100,
      "channels": 2,
      "composer": "164",
      "comment": "",
      "playCount": "12",
      "firstPlayed": "2024-01-15",
      "lastPlayed": "2025-06-01",
      "added": "2024-01-10"
    }
  ]
}
```

::: tip Pagination
Use `start` and `count` to page through large playlists. The `total` field reports the playlist's total track count.
:::

### fb2k_playlist_play_track 

Plays a specific track in a playlist.

- **Bridge method**: `playlist.playTrack`

| Parameter | Type | Required | Constraints |
| --- | --- | --- | --- |
| `index` | integer | Yes | Zero-based track index; minimum `0` |
| `playlist` | integer | No | Minimum `0`; defaults to the active playlist |
| `deferred` | boolean | No | Whether playback should be deferred |
