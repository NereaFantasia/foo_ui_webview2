English | [中文](./README.zh-CN.md)

# foo-ui-webview2-mcp

> MCP server for [foo_ui_webview2](https://github.com/NereaFantasia/foo_ui_webview2) — let AI agents drive a foobar2000 WebView2 UI directly.

[![MCP](https://img.shields.io/badge/MCP-Compatible-blue)](https://modelcontextprotocol.io/)
[![Node.js](https://img.shields.io/badge/Node.js-18%2B-green)](https://nodejs.org/)

---

## Overview

`foo-ui-webview2-mcp` connects to the WebView2 instance running inside a foobar2000 host over the [Chrome DevTools Protocol (CDP)](https://chromedevtools.github.io/devtools-protocol/) and exposes the bridge API as standard [Model Context Protocol (MCP)](https://modelcontextprotocol.io/) tools.

```
AI agent (VS Code / Claude Desktop / Cursor)
    |  MCP (stdio)
    v
foo-ui-webview2-mcp (Node.js)
    |  CDP (localhost:9222)
    v
WebView2 (running inside fb2k)
    |  window.fb2k.invoke()
    v
C++ BridgeCore -> foobar2000 SDK
```

---

## Prerequisites

1. **Node.js 18+**
2. **foobar2000** with the `foo_ui_webview2` component installed and running
3. **CDP remote debugging enabled** — turn it on in foobar2000 under `Preferences → Display → WebView2 UI → Developer` (restart afterwards). The port is set on the same page (default `9222`); if you change it, set `FB2K_CDP_PORT` to the same value for the MCP server

---

## Installation

### Option 1 — npx (recommended)

No install required; just point your MCP client at the package:

```json
{
  "foo-ui-webview2": {
    "command": "npx",
    "args": ["-y", "foo-ui-webview2-mcp"],
    "env": {
      "FB2K_CDP_PORT": "9222",
      "FB2K_CDP_HOST": "localhost"
    },
    "type": "stdio"
  }
}
```

### Option 2 — global install

```bash
npm install -g foo-ui-webview2-mcp
foo-ui-webview2-mcp
```

### Option 3 — local development

```bash
cd mcp/
npm install
npm run build
npm start
```

---

## Configuration

### Environment variables

| Variable | Default | Description |
|----------|---------|-------------|
| `FB2K_CDP_PORT` | `9222` | WebView2 CDP debugging port |
| `FB2K_CDP_HOST` | `localhost` | WebView2 CDP host address |
| `FB2K_CDP_TARGET_URL` | — | Part of the page URL to attach to, when several WebView2 pages share the port |
| `FB2K_READ_ONLY` | off | `1` or `true` registers only the read-only tools |
| `FB2K_ENABLE_EVAL` | off | `1` or `true` registers `fb2k_page_evaluate`, which runs arbitrary JavaScript in the page |
| `FB2K_MAX_RESPONSE_CHARS` | `100000` | Longest text result, in characters; a longer one is cut and marked |
| `FB2K_MAX_IMAGE_BYTES` | `3932160` | Largest cover sent as an image, in bytes (3.75 MiB); a larger one is left out and the result says so |

### VS Code (GitHub Copilot)

Add to `.vscode/mcp.json`:

```json
{
  "servers": {
    "foo-ui-webview2": {
      "command": "npx",
      "args": ["-y", "foo-ui-webview2-mcp"],
      "env": { "FB2K_CDP_PORT": "9222" },
      "type": "stdio"
    }
  }
}
```

### Claude Desktop

Add to `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "foo-ui-webview2": {
      "command": "npx",
      "args": ["-y", "foo-ui-webview2-mcp"],
      "env": { "FB2K_CDP_PORT": "9222" }
    }
  }
}
```

Config file location:

- **Windows**: `%APPDATA%\Claude\claude_desktop_config.json`
- **macOS**: `~/Library/Application Support/Claude/claude_desktop_config.json`

### Cursor

Add the same configuration under **MCP Servers** in Cursor settings.

---

## Tools

Each bridge tool covers one namespace, or one kind of change within it. Pass the host method
to call as `action`, with that method's parameters beside it:

```json
{ "action": "playlist.getTracks", "playlistGuid": "{…}", "count": 50 }
```

A tool's annotations hold for every action in it, so a read-only tool never changes anything.
Before a call reaches foobar2000 the server checks the arguments against the chosen method's
own declaration; anything the method would refuse comes back as a tool error that names the
problem and lists what the method takes. The result is what `fb2k.invoke()` returns for that
method, as compact JSON. `artwork.getCurrent` and `artwork.getForTrack` are the exception: the
cover comes back as an image, with the other fields as JSON.

<!-- mcp-tools:begin -->
**10 bridge tools** covering 90 host methods, grouped by namespace and by what they change; the page tools follow below. Each tool takes an `action`, the host method to call, and that method's parameters beside it. A parameter marked `?` is optional; types, ranges and defaults are in each tool's input schema.

### `fb2k_playback_read`

read-only · 9 actions

Read what foobar2000 is playing: transport state, current track, position, volume, playback order, stop-after-current, where the playing track sits, and the playback queue. Changes nothing.

| Action | Params | Description |
|--------|--------|-------------|
| `playback.getState` | — | Report the transport state and what the current track allows. |
| `playback.getCurrentTrack` | — | Report the track now loaded, playing or paused. |
| `playback.getPosition` | — | Report the playback position together with the length and identity of the track it is in. |
| `playback.getVolume` | — | Report the output volume on both scales the host uses, and whether it is muted. |
| `playback.getPlaybackOrder` | — | Report the active playback order as its index and its name. |
| `playback.getStopAfterCurrent` | — | Report whether playback stops after the current track. |
| `playback.getCurrentTrackIndex` | `includeTrackInfo?` | Locate the playing track in its playlist. |
| `playback.getPlayingPlaylist` | — | Report the playlist playback was last started from. |
| `queue.get` | — | Read the whole playback queue, in play order. |

### `fb2k_playback_control`

changes state · 18 actions

Control playback: start, pause, stop, skip, seek, set or step the volume, mute, pick the playback order, stop after the current track, and play a given file or playlist row.

| Action | Params | Description |
|--------|--------|-------------|
| `playback.play` | — | Start playback, or resume when paused. |
| `playback.pause` | — | Pause playback. |
| `playback.stop` | — | Stop playback. |
| `playback.next` | — | Skip to the next track under the current playback order. |
| `playback.previous` | — | Skip to the previous track under the current playback order. |
| `playback.playPause` | — | Toggle between playing and paused; starts playback when stopped. |
| `playback.random` | — | Start a random track of the active playlist. |
| `playback.playPath` | `path` | Append one file to the active playlist and play it; a playlist is created when there is none. |
| `playlist.playTrack` | `playlist?`, `playlistGuid?`, `index`, `deferred?`, `muted?` | Play a row of a playlist, as double-clicking it does. |
| `playback.setPosition` | `position` | Seek within the current track, playing or paused. |
| `playback.setVolume` | `volume` | Set the output volume as a percentage; values outside `0` to `100` are clamped. |
| `playback.volumeUp` | — | Raise the volume by one of the host's steps: one decibel, landing on a whole decibel. |
| `playback.volumeDown` | — | Lower the volume by one of the host's steps: one decibel, landing on a whole decibel. |
| `playback.mute` | `muted?` | Mute or unmute; a call that asks for the state already in effect does nothing. |
| `playback.toggleMute` | — | Flip the mute state and report the new one. |
| `playback.setPlaybackOrder` | `order?`, `name?` | Select a playback order by index or by name; exactly one of the two must be given. |
| `playback.setStopAfterCurrent` | `enabled` | Arm or disarm stopping after the current track. |
| `playback.toggleStopAfterCurrent` | — | Flip the stop-after-current flag and report the new value. |

### `fb2k_playlist_read`

read-only · 12 actions

Read playlists: the list of playlists (index, guid, name, track count, whether active, playing, locked or an autoplaylist), a page of one playlist's tracks or the rows picked by number, the rows whose tracks match a query, its selection and focus, its lock and autoplaylist details, and the columns the playlist view offers. Changes nothing.

| Action | Params | Description |
|--------|--------|-------------|
| `playlist.getAll` | — | List every playlist in playlist order. |
| `playlist.getActive` | — | Describe the active playlist, the one the user is looking at. |
| `playlist.getPlaying` | — | Describe the playing playlist, the one playback takes its next track from. |
| `playlist.getTracks` | `playlist?`, `playlistGuid?`, `start?`, `count?`, `formats?`, `fields?` | A page of a playlist's rows. |
| `playlist.getTracksAt` | `rows`, `playlist?`, `playlistGuid?`, `formats?`, `fields?` | Rows of a playlist picked by row number, in the order given, such as the rows `playlist.getMatchingRows` answers, which need not be adjacent. |
| `playlist.getMatchingRows` | `query`, `playlist?`, `playlistGuid?` | The rows of a playlist whose tracks match a foobar2000 query, in playlist order, in one call however long the playlist is; read the rows themselves with `playlist.getTracksAt`. |
| `playlist.getSelectedTracks` | `playlist?`, `playlistGuid?` | The selected rows of a playlist in playlist order, without the play statistics. |
| `playlist.getSelection` | `playlist?`, `playlistGuid?` | List the selected rows of a playlist. |
| `playlist.getFocusedTrack` | `playlist?`, `playlistGuid?` | Report the focused row of a playlist. |
| `playlist.getLockInfo` | `playlist?`, `playlistGuid?` | Report whether a playlist carries a lock, such as an autoplaylist's. |
| `playlist.getAutoplaylistInfo` | `playlist?`, `playlistGuid?` | Describe a playlist's autoplaylist status. |
| `playlist.getAvailableColumns` | — | List the playlist columns that foobar2000 and the installed components define for the Default UI playlist view. |

### `fb2k_playlist_manage`

destructive · 8 actions

Manage the playlists themselves: create, remove, rename, duplicate and reorder them, and turn a playlist into an autoplaylist that a library query fills, or back into a normal one.

| Action | Params | Description |
|--------|--------|-------------|
| `playlist.create` | `name`, `position?` | Create an empty playlist. |
| `playlist.duplicate` | `playlist?`, `playlistGuid?`, `name?` | Copy a playlist, tracks included, into a new playlist placed right after it. |
| `playlist.rename` | `playlist?`, `playlistGuid?`, `name` | Rename a playlist. |
| `playlist.remove` | `playlist` | Delete a playlist. |
| `playlist.reorderPlaylists` | `newOrder?`, `newOrderGuids?` | Reorder the playlist list, giving for each new position either the current index of the playlist that goes there (`newOrder`) or its GUID (`newOrderGuids`); give exactly one of the two, or the call fails with `INVALID_PARAMS`. |
| `playlist.createAutoplaylist` | `name?`, `query`, `sort?`, `keepSorted?` | Create a playlist that foobar2000 fills from a library query and keeps up to date. |
| `playlist.convertToAutoplaylist` | `playlist?`, `playlistGuid?`, `query`, `sort?`, `keepSorted?` | Turn an existing playlist into an autoplaylist; its tracks are replaced by the query results. |
| `playlist.removeAutoplaylist` | `playlist?`, `playlistGuid?` | Turn an autoplaylist back into an ordinary playlist that keeps its current tracks. |

### `fb2k_playlist_edit`

destructive · 15 actions

Change the tracks of a playlist: add files, folders or tracks, insert, remove, move, reorder, sort, shuffle or reverse rows, clear it, replace everything and play, and undo or redo.

| Action | Params | Description |
|--------|--------|-------------|
| `playlist.addPaths` | `playlist?`, `playlistGuid?`, `paths` | Append files, folders or URLs to a playlist, saving an undo point first. |
| `playlist.addPathsSequential` | `playlist?`, `playlistGuid?`, `paths` | Append paths to a playlist in the given order, saving an undo point first; a path that expands to several tracks (a folder, a cue sheet) keeps its place. |
| `playlist.addHandles` | `playlist?`, `playlistGuid?`, `handles` | Append tracks to a playlist, saving an undo point first. |
| `playlist.insertTracks` | `playlist?`, `playlistGuid?`, `position?`, `handles` | Insert tracks at a position of a playlist, saving an undo point first. |
| `playlist.replaceAllAndPlay` | `playlist?`, `playlistGuid?`, `paths`, `playIndex?`, `stopFirst?`, `autoPlay?` | Replace the whole content of a playlist and play it: stop playback, clear the playlist (saving an undo point), add the paths as `playlist.addPaths` does, make the playlist active and play or focus `playIndex`. |
| `playlist.removeTracks` | `playlist?`, `playlistGuid?`, `items` | Remove rows from a playlist, saving an undo point first. |
| `playlist.removeSelectedTracks` | `playlist?`, `playlistGuid?` | Remove the selected rows from a playlist, saving an undo point first. |
| `playlist.clear` | `playlist?`, `playlistGuid?` | Remove every track from a playlist, saving an undo point first. |
| `playlist.moveTracks` | `playlist?`, `playlistGuid?`, `items?`, `delta` | Move the selected rows of a playlist by `delta` positions, saving an undo point first. |
| `playlist.reorder` | `playlist?`, `playlistGuid?`, `newOrder` | Reorder a playlist's tracks, saving an undo point first: `newOrder[i]` is the current row of the track that moves to row `i`. |
| `playlist.sort` | `playlist?`, `playlistGuid?`, `pattern?`, `descending?`, `selectedOnly?` | Sort a playlist by a Title Formatting pattern, saving an undo point first. |
| `playlist.shuffle` | `playlist?`, `playlistGuid?` | Put a playlist's tracks in random order, saving an undo point first. |
| `playlist.reverse` | `playlist?`, `playlistGuid?` | Reverse the order of a playlist's tracks, saving an undo point first. |
| `playlist.undo` | `playlist?`, `playlistGuid?` | Revert a playlist to its last undo point. |
| `playlist.redo` | `playlist?`, `playlistGuid?` | Reapply the change the last `playlist.undo` reverted. |

### `fb2k_playlist_select`

changes state, idempotent · 5 actions

Change which playlist is active and which of its rows are selected or focused. The tracks themselves are left alone.

| Action | Params | Description |
|--------|--------|-------------|
| `playlist.setActive` | `playlist?`, `playlistGuid?` | Make a playlist the active one. |
| `playlist.setSelection` | `playlist?`, `playlistGuid?`, `indices`, `clearOthers?` | Select rows of a playlist. |
| `playlist.selectAll` | `playlist?`, `playlistGuid?` | Select every row of a playlist. |
| `playlist.deselectAll` | `playlist?`, `playlistGuid?` | Clear the selection of a playlist. |
| `playlist.setFocusedTrack` | `playlist?`, `playlistGuid?`, `index` | Move the focus of a playlist to a row, or remove it. |

### `fb2k_library_read`

read-only · 4 actions

Read the media library: search it with foobar2000 query syntax (e.g. `artist IS Beatles`), list its albums or artists, and get its statistics. Results come in pages; ask for fewer `fields` to keep them short. Changes nothing.

| Action | Params | Description |
|--------|--------|-------------|
| `library.search` | `query`, `offset?`, `limit?`, `fields?` | One page of the tracks matching a foobar2000 query, in library order: a `SORT BY` clause is accepted but not applied. |
| `library.getAlbums` | `sort?`, `query?`, `offset?`, `limit?`, `includeTracks?`, `useCache?` | Albums of the whole library, grouped by album name plus album artist; tracks without an `album` tag are skipped. |
| `library.getArtists` | `sort?`, `limit?`, `includeAlbums?` | Every credited artist with participation counts: each value of a multi-value `artist` gets its own row. |
| `library.getStats` | — | Aggregate counts over the whole library. |

### `fb2k_queue_edit`

destructive · 8 actions

Change the playback queue: add playlist rows or files, insert tracks to play next, remove entries, move one to the front, replace or clear the whole queue, and play an entry now. Read the queue with `queue.get` on fb2k_playback_read.

| Action | Params | Description |
|--------|--------|-------------|
| `queue.add` | `playlist?`, `playlistGuid?`, `tracks?`, `track?` | Queue one or more tracks by their position in a playlist. |
| `queue.addPaths` | `paths`, `useQueuePlaylist?`, `playlist?`, `playlistGuid?` | Queue tracks by path. |
| `queue.insertNext` | `paths?`, `items?`, `position?` | Insert tracks so they play next, ahead of everything already queued. |
| `queue.setContents` | `items` | Replace the whole queue with an ordered list of references. |
| `queue.moveToTop` | `index` | Move a queued entry to the front so it plays next. |
| `queue.playNow` | `index?` | Play the queue entry at `index` now, moving it to the front first when it is not already there. |
| `queue.remove` | `index?`, `indices?` | Remove one entry by `index`, or several by `indices`. |
| `queue.clear` | — | Empty the queue. |

### `fb2k_track_read`

read-only · 6 actions

Read track files: tags and technical info from the library cache or straight from the file, the tags of several files at once, a track's rating, and its cover art. Changes nothing.

| Action | Params | Description |
|--------|--------|-------------|
| `metadata.read` | `path`, `cueIndex?` | Read the tags and technical info of one track. |
| `metadata.readRaw` | `path`, `cueIndex?` | Read the tags and technical info of one track straight from the file, bypassing the host's cache; the result is that of `metadata.read` plus `source`. |
| `metadata.readBatch` | `paths` | Read the flat fields of several tracks, one row per path. |
| `rating.get` | `path`, `cueIndex?` | Read a track's rating from 0 to 5: a foo_playcount `%rating%` between 1 and 5 wins, otherwise the file's `RATING` tag clamped to 0..5. |
| `artwork.getForTrack` | `path`, `type?` | Read a picture through the album art manager, which also finds folder covers, and measure it. |
| `artwork.getCurrent` | `type?` | Read the picture of the playing track. |

### `fb2k_track_write`

destructive · 5 actions

Change track files: write tags to one file or many (a `null` value removes a tag), set a rating, and embed or remove cover art.

| Action | Params | Description |
|--------|--------|-------------|
| `metadata.write` | `path`, `tags`, `cueIndex?` | Queue a tag write to one track and return at once. |
| `metadata.writeBatch` | `items` | Queue one `metadata.write` per entry and report how many went through. |
| `rating.set` | `path?`, `rating`, `cueIndex?` | Set a track's rating through foo_playcount's context menu, or write the file's `RATING` tag when that menu is not available. |
| `metadata.embedArtwork` | `path`, `imageData`, `type?`, `target?`, `filename?` | Write a picture into a track's tags, into an image file next to it, or both. |
| `metadata.removeEmbeddedArt` | `path`, `type?`, `removeAll?` | Remove embedded pictures from a file: one type, or every picture when `type` is omitted or `removeAll` is set. |
<!-- mcp-tools:end -->

### Page tools

These work on the WebView2 page through CDP rather than through a bridge method.

| Tool | Params | Description |
|------|--------|-------------|
| `fb2k_page_inspect` | `action`, `fullPage?`, `limit?` | Read-only. `screenshot` returns a PNG (`fullPage` for the whole page); `domSnapshot` a simplified DOM tree; `consoleMessages` the page's recent console messages and uncaught exceptions, the last 200 kept (`limit`, default 100) |
| `fb2k_page_evaluate` | `expression` | Run a JavaScript expression in the page. Registered only with `FB2K_ENABLE_EVAL=1`, and not in read-only mode |

### Limits

- The server takes 40 tool calls in a burst and 20 a second after that; a call over the limit
  fails with a message saying how long to wait.
- A text result longer than `FB2K_MAX_RESPONSE_CHARS` characters is cut, with a note suggesting
  paging or fewer `fields`.
- A cover is sent as an image only when it is PNG, JPEG, GIF or WebP and at most
  `FB2K_MAX_IMAGE_BYTES` bytes (3.75 MiB by default, which base64-encodes to the 5 MiB the
  Claude API accepts for one image). Otherwise the result carries the other fields and a note
  saying why the picture was left out.

---

## Usage examples

### Example 1 — query the current playback state

```
User: What's playing right now?
AI -> fb2k_playback_read { action: "playback.getCurrentTrack" }
AI: Now playing "Redo" by Mili, from the album "Millennium Mother", 3:53.
```

### Example 2 — search and play

```
User: Find songs by Mili and play the first one.
AI -> fb2k_library_read { action: "library.search", query: "artist IS Mili", limit: 20 }
AI -> fb2k_playback_control { action: "playback.playPath", path: "D:\\Music\\Mili\\Redo.flac" }
AI: Started playing "Redo" by Mili.
```

### Example 3 — screenshot to verify a theme

```
User: Take a screenshot of the current UI.
AI -> fb2k_page_inspect { action: "screenshot", fullPage: true }
AI: [shows screenshot] The theme loaded correctly; the playback bar is at the bottom…
```

### Error responses

Bridge handlers can return a structured failure such as
`{ success: false, error, code, details }`. The MCP server reports that result
as a tool error and includes the host error message, stable code, and details in
the text content. CDP connection failures and invocation timeouts are also
reported as tool errors.

### Parameter validation

Each call is checked twice before the bridge handler runs. The tool's own schema accepts
`action` and any parameter one of its actions takes; an argument no action takes is refused.
Then the arguments go through the chosen method's exact schema: its required parameters,
numeric bounds, integer and array element types, string enums and nested required fields, with
its declared defaults filled in. A parameter that belongs to another action of the same tool is
refused there, with the list of what the chosen method takes. Metadata tag keys stay open.
Prototype-sensitive keys (`__proto__`, `prototype`, and `constructor`) are rejected at any
argument depth before bridge invocation rather than being normalized or forwarded ambiguously.

---

## Development

```bash
cd mcp/

# Install dependencies
npm install

# Compile TypeScript
npm run build

# Watch mode
npm run dev

# Run tests (offline; foobar2000 not required)
npm test

# End-to-end smoke test (requires foobar2000 running with CDP enabled)
node tests/e2e-smoke.mjs
```

### Project structure

```
mcp/
├── src/
│   ├── index.ts              # MCP server entry point
│   ├── cdp-client.ts         # CDP connection management (auto-discovery, reconnect, timeout)
│   ├── bridge-executor.ts    # fb2k.invoke() wrapper
│   ├── bridge-tools.ts       # Bridge tool registration and per-action validation
│   ├── page-tools.ts         # fb2k_page_inspect and fb2k_page_evaluate
│   ├── rate-limit.ts         # Call budget shared by every tool
│   ├── tool-results.ts       # Text, JSON and error results, with the size cap
│   ├── guarded-stdio-transport.ts # Raw JSON safety before SDK normalization
│   ├── tool-schema.ts        # Declarative JSON Schema to Zod validation
│   ├── types.ts              # Shared types
│   └── generated/
│       └── bridge-tools.ts   # Bridge tools, generated; do not edit
├── tool-table.json           # Which host methods each tool bundles, with the tool descriptions
├── tests/
│   ├── bridge-executor.test.ts    # BridgeExecutor unit tests
│   ├── cdp-client.test.ts         # CdpClient unit tests
│   ├── error-paths.test.ts        # Error-path coverage
│   ├── guarded-stdio-transport.test.ts # Raw stdio JSON safety tests
│   ├── page-tools.test.ts         # Page tool tests
│   ├── tool-schema.test.ts        # Schema builder and production registration tests
│   ├── tools-integration.test.ts  # Checks on the generated tools
│   ├── e2e-smoke.mjs              # CDP end-to-end smoke test
│   └── e2e-interact.mjs          # CDP interaction test
├── dist/                     # Compiled output
├── package.json
└── tsconfig.json
```

`src/generated/bridge-tools.ts` and the bridge tool tables in this README are generated from
the host method declarations under `src/api/schema/` of the repository and from
`tool-table.json`: parameter types, ranges, defaults and descriptions come from the
declarations, and each tool's annotations from its methods' `@effect` tags. To add or change a
tool, edit `tool-table.json` and run `node scripts/api-schema/generate.mjs --write` from the
repository root.

### Testing

| Test type | Command | foobar2000 required |
|-----------|---------|---------------------|
| All offline tests | `npm test` | No |
| E2E smoke | `node tests/e2e-smoke.mjs` | Yes |
| E2E interaction | `node tests/e2e-interact.mjs` | Yes |

---

## Connection notes

### CDP port configuration

The MCP server connects to the WebView2 inside foobar2000 over CDP. Make sure:

1. CDP remote debugging is enabled in the foobar2000 component (`Preferences → Display → WebView2 UI → Developer`, takes effect after a restart).
2. The CDP port (default `9222`) is not occupied by another program.
3. If the port on the Developer page is not `9222`, set the `FB2K_CDP_PORT` environment variable to the same value.

### Handling connection failures

- **foobar2000 not running** — tool calls return `"WebView2 not available at port 9222"`.
- **CDP not enabled** — same as above.
- **Connection dropped** — automatic reconnect (up to 3 attempts, backoff 1s -> 2s -> 4s).
- **Call timeout** — returns an error after a 30-second timeout.

---

## Pairing with Chrome DevTools MCP

This project pairs well with [chrome-devtools-mcp](https://github.com/ChromeDevTools/chrome-devtools-mcp); the two are complementary:

| MCP server | Role | Use cases |
|------------|------|-----------|
| **foo-ui-webview2-mcp** | Semantic bridge API | Playback control, playlist management, library search |
| **chrome-devtools-mcp** | Generic DOM interaction | Clicking buttons, filling inputs, screenshot diffing |

Both connect to the same WebView2 instance at `localhost:9222`.

---

## Roadmap

- [x] Infrastructure (CDP connection, MCP framework, core tool set)
- [x] Tools generated from the host declarations, grouped by namespace and by what they change, with annotations
- [ ] More namespaces, chosen by task (title formatting, main-menu commands, output and DSP, play counts, lyrics)
- [ ] Waiting for events (for example `playlist:addComplete`) through a subscription handle
- [ ] Structured output (`outputSchema`), once per-action result schemas can be expressed

See the project repository: [NereaFantasia/foo_ui_webview2](https://github.com/NereaFantasia/foo_ui_webview2).

---

## License

Released under the [MIT License](./LICENSE), consistent with the licensing of the `foo_ui_webview2` SDK / MCP sub-packages.
