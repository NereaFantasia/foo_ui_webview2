# Tools

The server groups the Bridge API into a few tools. Each tool covers one namespace, or one kind of change within it, and takes the host method to call as `action`, with that method's parameters beside it:

```json
{ "action": "playlist.getTracks", "playlistGuid": "{…}", "count": 50 }
```

Grouping by what the methods do keeps each tool's [annotations](https://modelcontextprotocol.io/specification/2025-11-25/server/tools) true for every action in it: a read-only tool never changes anything, and a client can let it run without asking. Before a call reaches foobar2000, the server checks the parameters against the chosen method's own declaration and reports anything the method would refuse, together with the parameters that method takes. The result is what `fb2k.invoke()` returns for that method. `artwork.getCurrent` and `artwork.getForTrack` are the exception: the cover comes back as an image, with the other fields as JSON.

Set `FB2K_READ_ONLY` to `1` or `true` to register only the read-only tools.

<!-- mcp-tools:begin -->
**10 bridge tools** covering 90 host methods, grouped by namespace and by what they change; the page tools follow below. Each tool takes an `action`, the host method to call, and that method's parameters beside it. A parameter marked `?` is optional; types, ranges and defaults are in each tool's input schema.

### `fb2k_playback_read`

read-only · 9 actions

Read what foobar2000 is playing: transport state, current track, position, volume, playback order, stop-after-current, where the playing track sits, and the playback queue. Changes nothing.

| Action | Params | Description |
|--------|--------|-------------|
| [`playback.getState`](../api/playback.md#playback-getstate) | — | Report the transport state and what the current track allows. |
| [`playback.getCurrentTrack`](../api/playback.md#playback-getcurrenttrack) | — | Report the track now loaded, playing or paused. |
| [`playback.getPosition`](../api/playback.md#playback-getposition) | — | Report the playback position together with the length and identity of the track it is in. |
| [`playback.getVolume`](../api/playback.md#playback-getvolume) | — | Report the output volume on both scales the host uses, and whether it is muted. |
| [`playback.getPlaybackOrder`](../api/playback.md#playback-getplaybackorder) | — | Report the active playback order as its index and its name. |
| [`playback.getStopAfterCurrent`](../api/playback.md#playback-getstopaftercurrent) | — | Report whether playback stops after the current track. |
| [`playback.getCurrentTrackIndex`](../api/playback.md#playback-getcurrenttrackindex) | `includeTrackInfo?` | Locate the playing track in its playlist. |
| [`playback.getPlayingPlaylist`](../api/playback.md#playback-getplayingplaylist) | — | Report the playlist playback was last started from. |
| [`queue.get`](../api/queue.md#queue-get) | — | Read the whole playback queue, in play order. |

### `fb2k_playback_control`

changes state · 18 actions

Control playback: start, pause, stop, skip, seek, set or step the volume, mute, pick the playback order, stop after the current track, and play a given file or playlist row.

| Action | Params | Description |
|--------|--------|-------------|
| [`playback.play`](../api/playback.md#playback-play) | — | Start playback, or resume when paused. |
| [`playback.pause`](../api/playback.md#playback-pause) | — | Pause playback. |
| [`playback.stop`](../api/playback.md#playback-stop) | — | Stop playback. |
| [`playback.next`](../api/playback.md#playback-next) | — | Skip to the next track under the current playback order. |
| [`playback.previous`](../api/playback.md#playback-previous) | — | Skip to the previous track under the current playback order. |
| [`playback.playPause`](../api/playback.md#playback-playpause) | — | Toggle between playing and paused; starts playback when stopped. |
| [`playback.random`](../api/playback.md#playback-random) | — | Start a random track of the active playlist. |
| [`playback.playPath`](../api/playback.md#playback-playpath) | `path` | Append one file to the active playlist and play it; a playlist is created when there is none. |
| [`playlist.playTrack`](../api/playlist.md#playlist-playtrack) | `playlist?`, `playlistGuid?`, `index`, `deferred?`, `muted?` | Play a row of a playlist, as double-clicking it does. |
| [`playback.setPosition`](../api/playback.md#playback-setposition) | `position` | Seek within the current track, playing or paused. |
| [`playback.setVolume`](../api/playback.md#playback-setvolume) | `volume` | Set the output volume as a percentage; values outside `0` to `100` are clamped. |
| [`playback.volumeUp`](../api/playback.md#playback-volumeup) | — | Raise the volume by one of the host's steps: one decibel, landing on a whole decibel. |
| [`playback.volumeDown`](../api/playback.md#playback-volumedown) | — | Lower the volume by one of the host's steps: one decibel, landing on a whole decibel. |
| [`playback.mute`](../api/playback.md#playback-mute) | `muted?` | Mute or unmute; a call that asks for the state already in effect does nothing. |
| [`playback.toggleMute`](../api/playback.md#playback-togglemute) | — | Flip the mute state and report the new one. |
| [`playback.setPlaybackOrder`](../api/playback.md#playback-setplaybackorder) | `order?`, `name?` | Select a playback order by index or by name; exactly one of the two must be given. |
| [`playback.setStopAfterCurrent`](../api/playback.md#playback-setstopaftercurrent) | `enabled` | Arm or disarm stopping after the current track. |
| [`playback.toggleStopAfterCurrent`](../api/playback.md#playback-togglestopaftercurrent) | — | Flip the stop-after-current flag and report the new value. |

### `fb2k_playlist_read`

read-only · 12 actions

Read playlists: the list of playlists (index, guid, name, track count, whether active, playing, locked or an autoplaylist), a page of one playlist's tracks or the rows picked by number, the rows whose tracks match a query, its selection and focus, its lock and autoplaylist details, and the columns the playlist view offers. Changes nothing.

| Action | Params | Description |
|--------|--------|-------------|
| [`playlist.getAll`](../api/playlist.md#playlist-getall) | — | List every playlist in playlist order. |
| [`playlist.getActive`](../api/playlist.md#playlist-getactive) | — | Describe the active playlist, the one the user is looking at. |
| [`playlist.getPlaying`](../api/playlist.md#playlist-getplaying) | — | Describe the playing playlist, the one playback takes its next track from. |
| [`playlist.getTracks`](../api/playlist.md#playlist-gettracks) | `playlist?`, `playlistGuid?`, `start?`, `count?`, `formats?`, `fields?` | A page of a playlist's rows. |
| [`playlist.getTracksAt`](../api/playlist.md#playlist-gettracksat) | `rows`, `playlist?`, `playlistGuid?`, `formats?`, `fields?` | Rows of a playlist picked by row number, in the order given, such as the rows `playlist.getMatchingRows` answers, which need not be adjacent. |
| [`playlist.getMatchingRows`](../api/playlist.md#playlist-getmatchingrows) | `query`, `playlist?`, `playlistGuid?` | The rows of a playlist whose tracks match a foobar2000 query, in playlist order, in one call however long the playlist is; read the rows themselves with `playlist.getTracksAt`. |
| [`playlist.getSelectedTracks`](../api/playlist.md#playlist-getselectedtracks) | `playlist?`, `playlistGuid?` | The selected rows of a playlist in playlist order, without the play statistics. |
| [`playlist.getSelection`](../api/playlist.md#playlist-getselection) | `playlist?`, `playlistGuid?` | List the selected rows of a playlist. |
| [`playlist.getFocusedTrack`](../api/playlist.md#playlist-getfocusedtrack) | `playlist?`, `playlistGuid?` | Report the focused row of a playlist. |
| [`playlist.getLockInfo`](../api/playlist.md#playlist-getlockinfo) | `playlist?`, `playlistGuid?` | Report whether a playlist carries a lock, such as an autoplaylist's. |
| [`playlist.getAutoplaylistInfo`](../api/playlist.md#playlist-getautoplaylistinfo) | `playlist?`, `playlistGuid?` | Describe a playlist's autoplaylist status. |
| [`playlist.getAvailableColumns`](../api/playlist.md#playlist-getavailablecolumns) | — | List the playlist columns that foobar2000 and the installed components define for the Default UI playlist view. |

### `fb2k_playlist_manage`

destructive · 8 actions

Manage the playlists themselves: create, remove, rename, duplicate and reorder them, and turn a playlist into an autoplaylist that a library query fills, or back into a normal one.

| Action | Params | Description |
|--------|--------|-------------|
| [`playlist.create`](../api/playlist.md#playlist-create) | `name`, `position?` | Create an empty playlist. |
| [`playlist.duplicate`](../api/playlist.md#playlist-duplicate) | `playlist?`, `playlistGuid?`, `name?` | Copy a playlist, tracks included, into a new playlist placed right after it. |
| [`playlist.rename`](../api/playlist.md#playlist-rename) | `playlist?`, `playlistGuid?`, `name` | Rename a playlist. |
| [`playlist.remove`](../api/playlist.md#playlist-remove) | `playlist` | Delete a playlist. |
| [`playlist.reorderPlaylists`](../api/playlist.md#playlist-reorderplaylists) | `newOrder?`, `newOrderGuids?` | Reorder the playlist list, giving for each new position either the current index of the playlist that goes there (`newOrder`) or its GUID (`newOrderGuids`); give exactly one of the two, or the call fails with `INVALID_PARAMS`. |
| [`playlist.createAutoplaylist`](../api/playlist.md#playlist-createautoplaylist) | `name?`, `query`, `sort?`, `keepSorted?` | Create a playlist that foobar2000 fills from a library query and keeps up to date. |
| [`playlist.convertToAutoplaylist`](../api/playlist.md#playlist-converttoautoplaylist) | `playlist?`, `playlistGuid?`, `query`, `sort?`, `keepSorted?` | Turn an existing playlist into an autoplaylist; its tracks are replaced by the query results. |
| [`playlist.removeAutoplaylist`](../api/playlist.md#playlist-removeautoplaylist) | `playlist?`, `playlistGuid?` | Turn an autoplaylist back into an ordinary playlist that keeps its current tracks. |

### `fb2k_playlist_edit`

destructive · 15 actions

Change the tracks of a playlist: add files, folders or tracks, insert, remove, move, reorder, sort, shuffle or reverse rows, clear it, replace everything and play, and undo or redo.

| Action | Params | Description |
|--------|--------|-------------|
| [`playlist.addPaths`](../api/playlist.md#playlist-addpaths) | `playlist?`, `playlistGuid?`, `paths` | Append files, folders or URLs to a playlist, saving an undo point first. |
| [`playlist.addPathsSequential`](../api/playlist.md#playlist-addpathssequential) | `playlist?`, `playlistGuid?`, `paths` | Append paths to a playlist in the given order, saving an undo point first; a path that expands to several tracks (a folder, a cue sheet) keeps its place. |
| [`playlist.addHandles`](../api/playlist.md#playlist-addhandles) | `playlist?`, `playlistGuid?`, `handles` | Append tracks to a playlist, saving an undo point first. |
| [`playlist.insertTracks`](../api/playlist.md#playlist-inserttracks) | `playlist?`, `playlistGuid?`, `position?`, `handles` | Insert tracks at a position of a playlist, saving an undo point first. |
| [`playlist.replaceAllAndPlay`](../api/playlist.md#playlist-replaceallandplay) | `playlist?`, `playlistGuid?`, `paths`, `playIndex?`, `stopFirst?`, `autoPlay?` | Replace the whole content of a playlist and play it: stop playback, clear the playlist (saving an undo point), add the paths as `playlist.addPaths` does, make the playlist active and play or focus `playIndex`. |
| [`playlist.removeTracks`](../api/playlist.md#playlist-removetracks) | `playlist?`, `playlistGuid?`, `items` | Remove rows from a playlist, saving an undo point first. |
| [`playlist.removeSelectedTracks`](../api/playlist.md#playlist-removeselectedtracks) | `playlist?`, `playlistGuid?` | Remove the selected rows from a playlist, saving an undo point first. |
| [`playlist.clear`](../api/playlist.md#playlist-clear) | `playlist?`, `playlistGuid?` | Remove every track from a playlist, saving an undo point first. |
| [`playlist.moveTracks`](../api/playlist.md#playlist-movetracks) | `playlist?`, `playlistGuid?`, `items?`, `delta` | Move the selected rows of a playlist by `delta` positions, saving an undo point first. |
| [`playlist.reorder`](../api/playlist.md#playlist-reorder) | `playlist?`, `playlistGuid?`, `newOrder` | Reorder a playlist's tracks, saving an undo point first: `newOrder[i]` is the current row of the track that moves to row `i`. |
| [`playlist.sort`](../api/playlist.md#playlist-sort) | `playlist?`, `playlistGuid?`, `pattern?`, `descending?`, `selectedOnly?` | Sort a playlist by a Title Formatting pattern, saving an undo point first. |
| [`playlist.shuffle`](../api/playlist.md#playlist-shuffle) | `playlist?`, `playlistGuid?` | Put a playlist's tracks in random order, saving an undo point first. |
| [`playlist.reverse`](../api/playlist.md#playlist-reverse) | `playlist?`, `playlistGuid?` | Reverse the order of a playlist's tracks, saving an undo point first. |
| [`playlist.undo`](../api/playlist.md#playlist-undo) | `playlist?`, `playlistGuid?` | Revert a playlist to its last undo point. |
| [`playlist.redo`](../api/playlist.md#playlist-redo) | `playlist?`, `playlistGuid?` | Reapply the change the last `playlist.undo` reverted. |

### `fb2k_playlist_select`

changes state, idempotent · 5 actions

Change which playlist is active and which of its rows are selected or focused. The tracks themselves are left alone.

| Action | Params | Description |
|--------|--------|-------------|
| [`playlist.setActive`](../api/playlist.md#playlist-setactive) | `playlist?`, `playlistGuid?` | Make a playlist the active one. |
| [`playlist.setSelection`](../api/playlist.md#playlist-setselection) | `playlist?`, `playlistGuid?`, `indices`, `clearOthers?` | Select rows of a playlist. |
| [`playlist.selectAll`](../api/playlist.md#playlist-selectall) | `playlist?`, `playlistGuid?` | Select every row of a playlist. |
| [`playlist.deselectAll`](../api/playlist.md#playlist-deselectall) | `playlist?`, `playlistGuid?` | Clear the selection of a playlist. |
| [`playlist.setFocusedTrack`](../api/playlist.md#playlist-setfocusedtrack) | `playlist?`, `playlistGuid?`, `index` | Move the focus of a playlist to a row, or remove it. |

### `fb2k_library_read`

read-only · 4 actions

Read the media library: search it with foobar2000 query syntax (e.g. `artist IS Beatles`), list its albums or artists, and get its statistics. Results come in pages; ask for fewer `fields` to keep them short. Changes nothing.

| Action | Params | Description |
|--------|--------|-------------|
| [`library.search`](../api/library.md#library-search) | `query`, `offset?`, `limit?`, `fields?` | One page of the tracks matching a foobar2000 query, in library order: a `SORT BY` clause is accepted but not applied. |
| [`library.getAlbums`](../api/library.md#library-getalbums) | `sort?`, `query?`, `offset?`, `limit?`, `includeTracks?`, `useCache?` | Albums of the whole library, grouped by album name plus album artist; tracks without an `album` tag are skipped. |
| [`library.getArtists`](../api/library.md#library-getartists) | `sort?`, `limit?`, `includeAlbums?` | Every credited artist with participation counts: each value of a multi-value `artist` gets its own row. |
| [`library.getStats`](../api/library.md#library-getstats) | — | Aggregate counts over the whole library. |

### `fb2k_queue_edit`

destructive · 8 actions

Change the playback queue: add playlist rows or files, insert tracks to play next, remove entries, move one to the front, replace or clear the whole queue, and play an entry now. Read the queue with `queue.get` on fb2k_playback_read.

| Action | Params | Description |
|--------|--------|-------------|
| [`queue.add`](../api/queue.md#queue-add) | `playlist?`, `playlistGuid?`, `tracks?`, `track?` | Queue one or more tracks by their position in a playlist. |
| [`queue.addPaths`](../api/queue.md#queue-addpaths) | `paths`, `useQueuePlaylist?`, `playlist?`, `playlistGuid?` | Queue tracks by path. |
| [`queue.insertNext`](../api/queue.md#queue-insertnext) | `paths?`, `items?`, `position?` | Insert tracks so they play next, ahead of everything already queued. |
| [`queue.setContents`](../api/queue.md#queue-setcontents) | `items` | Replace the whole queue with an ordered list of references. |
| [`queue.moveToTop`](../api/queue.md#queue-movetotop) | `index` | Move a queued entry to the front so it plays next. |
| [`queue.playNow`](../api/queue.md#queue-playnow) | `index?` | Play the queue entry at `index` now, moving it to the front first when it is not already there. |
| [`queue.remove`](../api/queue.md#queue-remove) | `index?`, `indices?` | Remove one entry by `index`, or several by `indices`. |
| [`queue.clear`](../api/queue.md#queue-clear) | — | Empty the queue. |

### `fb2k_track_read`

read-only · 6 actions

Read track files: tags and technical info from the library cache or straight from the file, the tags of several files at once, a track's rating, and its cover art. Changes nothing.

| Action | Params | Description |
|--------|--------|-------------|
| [`metadata.read`](../api/metadata.md#metadata-read) | `path`, `cueIndex?` | Read the tags and technical info of one track. |
| [`metadata.readRaw`](../api/metadata.md#metadata-readraw) | `path`, `cueIndex?` | Read the tags and technical info of one track straight from the file, bypassing the host's cache; the result is that of `metadata.read` plus `source`. |
| [`metadata.readBatch`](../api/metadata.md#metadata-readbatch) | `paths` | Read the flat fields of several tracks, one row per path. |
| [`rating.get`](../api/rating.md#rating-get) | `path`, `cueIndex?` | Read a track's rating from 0 to 5: a foo_playcount `%rating%` between 1 and 5 wins, otherwise the file's `RATING` tag clamped to 0..5. |
| [`artwork.getForTrack`](../api/artwork.md#artwork-getfortrack) | `path`, `type?` | Read a picture through the album art manager, which also finds folder covers, and measure it. |
| [`artwork.getCurrent`](../api/artwork.md#artwork-getcurrent) | `type?` | Read the picture of the playing track. |

### `fb2k_track_write`

destructive · 5 actions

Change track files: write tags to one file or many (a `null` value removes a tag), set a rating, and embed or remove cover art.

| Action | Params | Description |
|--------|--------|-------------|
| [`metadata.write`](../api/metadata.md#metadata-write) | `path`, `tags`, `cueIndex?` | Queue a tag write to one track and return at once. |
| [`metadata.writeBatch`](../api/metadata.md#metadata-writebatch) | `items` | Queue one `metadata.write` per entry and report how many went through. |
| [`rating.set`](../api/rating.md#rating-set) | `path?`, `rating`, `cueIndex?` | Set a track's rating through foo_playcount's context menu, or write the file's `RATING` tag when that menu is not available. |
| [`metadata.embedArtwork`](../api/metadata.md#metadata-embedartwork) | `path`, `imageData`, `type?`, `target?`, `filename?` | Write a picture into a track's tags, into an image file next to it, or both. |
| [`metadata.removeEmbeddedArt`](../api/metadata.md#metadata-removeembeddedart) | `path`, `type?`, `removeAll?` | Remove embedded pictures from a file: one type, or every picture when `type` is omitted or `removeAll` is set. |
<!-- mcp-tools:end -->

## Page tools

These two tools work on the WebView2 page through the Chrome DevTools Protocol rather than through a Bridge method.

### `fb2k_page_inspect`

read-only

| Action | Params | Result |
| --- | --- | --- |
| `screenshot` | `fullPage?` | A PNG image of the page. With `fullPage` `true` the viewport is sized to the whole content first; omitted, the visible viewport. |
| `domSnapshot` | — | One line per element, `tag#id.class "text"`, indented by depth. Text appears only for an element whose one child is a text node, cut to 80 characters. |
| `consoleMessages` | `limit?` | The page's most recent console messages and uncaught exceptions, `[level] text` per line, oldest first. The server collects them from its first connection to the page, together with the earlier ones the page still holds, and keeps the last 200; `limit` (default 100) caps how many come back. |

### `fb2k_page_evaluate`

destructive · registered only when `FB2K_ENABLE_EVAL` is `1` or `true`, and not in read-only mode

| Param | Description |
| --- | --- |
| `expression` | JavaScript to evaluate in the page. The result comes back as JSON. |

::: danger Security boundary
`fb2k_page_evaluate` runs arbitrary JavaScript in the page, with every Bridge method the page can call. Enable it only for a trusted development or debugging session.
:::

## Limits

- **Rate**: the server takes at most 40 tool calls in a burst and 20 a second after that; a call over the limit fails with a message saying how long to wait.
- **Response size**: a text result longer than `FB2K_MAX_RESPONSE_CHARS` characters (default 100000) is cut, and the cut is marked with a note suggesting paging or fewer `fields`.
- **Pictures**: a cover is sent as an image only when it is PNG, JPEG, GIF or WebP and at most `FB2K_MAX_IMAGE_BYTES` bytes (default 3932160, 3.75 MiB, which base64-encodes to the 5 MiB the Claude API accepts for one image). Otherwise the result carries the other fields and a note saying why the picture was left out.
