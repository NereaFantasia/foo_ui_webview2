# Queue Tools

Eleven tools manage the playback queue.

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

> Multi-value tags in `artist` / `albumArtist` / `genre` / `composer` (only the fields this API actually returns) are joined with `, ` in their original order, without de-duplication.

| Field | Type | Description |
| --- | --- | --- |
| `queueIndex` | integer | Position in the queue |
| `playlist` | integer \| null | Source playlist index; `null` when the entry has no playlist position |
| `playlistItem` | integer \| null | Source item index; `null` together with `playlist` |
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

## fb2k_queue_set_contents 

Replaces the entire playback queue with an ordered list of references.

- **Bridge method**: `queue.setContents`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | array of `{ queueIndex }` or `{ playlist, item }` | Yes | Ordered references; an empty array clears the queue |

::: tip Fails atomically
An entry with an unrecognized shape fails the whole call before anything is written, leaving the queue unchanged — unlike `fb2k_queue_add`, which skips bad entries one at a time.
:::

## fb2k_queue_insert_next 

Inserts tracks so they play next, ahead of everything already queued. Entries are given as file paths, as playlist positions, or both.

- **Bridge method**: `queue.insertNext`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | string[] | No¹ | File paths or URLs, optionally using `path\|subsong:N`; the resulting entries carry no playlist position |
| `items` | `[{ playlist, item }]` | No¹ | Playlist positions (non-negative integers); the resulting entries carry that position, so the playback cursor follows them |
| `position` | integer | No | Insertion index after any moved entries are removed; defaults to `0` |

¹ At least one of `paths` and `items` must be non-empty.

A track already in the queue is moved to `position` instead of being queued a second time. Within one call the `items` block lands first, then the `paths` block. One bad `items` entry fails the whole call before anything is written; `items` deduplicates by track, so two positions of one track become one queue entry. The result carries `insertedCount`, `movedCount`, and `invalidCount` (paths only) alongside `queueCount`.

## fb2k_queue_play_now 

Plays the queue entry at the given index immediately, moving it to the front of the queue first if it is not already there.

- **Bridge method**: `queue.playNow`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | integer | No | Queue index to play; defaults to `0` (the current queue head) |
