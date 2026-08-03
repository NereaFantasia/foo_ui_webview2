# Queue Tools

Eight tools manage the playback queue.

## fb2k_queue_get 

Gets every item in the playback queue.

- **Parameters**: none
- **Bridge method**: `queue.get`

**Example result:**

```json
{
  "items": [
    {
      "queueIndex": 0,
      "path": "file://D:\\Music\\track.flac",
      "absolutePath": "D:\\Music\\track.flac",
      "subsong": 0,
      "fileSize": 28456789,
      "title": "Redo",
      "artist": "164",
      "album": "Millennium Mother",
      "albumArtist": "164",
      "genre": "Vocaloid",
      "date": "2011",
      "trackNumber": 1,
      "discNumber": 1,
      "duration": 263.5,
      "bitrate": 876,
      "sampleRate": 44100,
      "channels": 2,
      "codec": "FLAC",
      "playlist": 0,
      "playlistItem": 5
    }
  ],
  "count": 1
}
```

| Field | Type | Description |
| --- | --- | --- |
| `queueIndex` | integer | Position in the queue |
| `playlist` | integer | Source playlist index |
| `playlistItem` | integer | Source item index |
| Track fields | — | Same track-data family as `playback.getCurrentTrack` |

## fb2k_queue_add 

Adds one or more playlist items to the queue.

- **Bridge method**: `queue.add`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | integer | No | Source playlist; the Bridge defaults to the active playlist |
| `tracks` | integer[] | Conditional | Track indices for batch addition |
| `track` | integer | Conditional | One track index as an alternative to `tracks` |

::: tip Conditional input
Supply either `tracks` or `track`. The MCP schema leaves both optional because the exclusivity rule is enforced by the Bridge handler.
:::

## fb2k_queue_add_paths 

Adds file paths or URLs to the queue.

- **Bridge method**: `queue.addPaths`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | string[] | Yes | File paths or URLs, optionally using `path|subsong:N` |
| `useQueuePlaylist` | boolean | No | Use the dedicated queue playlist; default `true` |
| `playlist` | integer | No | Target playlist, used when `useQueuePlaylist` is `false` |

## fb2k_queue_remove 

Removes one or more queue positions.

- **Bridge method**: `queue.remove`

| Parameter | Type | Required | Constraints |
| --- | --- | --- | --- |
| `index` | integer | Conditional | One queue index; minimum `0` |
| `indices` | integer[] | Conditional | Queue indices as an alternative to `index` |

## fb2k_queue_clear 

Clears the playback queue.

- **Parameters**: none
- **Bridge method**: `queue.clear`

## fb2k_queue_get_count 

Gets the queue item count.

- **Parameters**: none
- **Bridge method**: `queue.getCount`

**Example result:**

```json
{ "count": 3, "hasItems": true }
```

| Field | Type | Description |
| --- | --- | --- |
| `count` | integer | Queue item count |
| `hasItems` | boolean | Whether the queue is non-empty |

## fb2k_queue_move_to_top 

Moves a queue item to the top so it plays next.

- **Bridge method**: `queue.moveToTop`

| Parameter | Type | Required | Constraints |
| --- | --- | --- | --- |
| `index` | integer | Yes | Queue index; minimum `0` |

## fb2k_queue_flush 

Flushes the playback queue. The mapped Bridge method is an alias of `queue.clear`.

- **Parameters**: none
- **Bridge method**: `queue.flush`
