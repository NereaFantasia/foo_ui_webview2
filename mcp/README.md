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
3. **CDP remote debugging enabled** — turn on DevTools / CDP in the foobar2000 advanced settings (default port `9222`)

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

**98 bridge tools + 4 UI-testing tools**, organized into 8 bridge namespaces.

### Playback (12)

| Tool | Params | Description |
|------|--------|-------------|
| `fb2k_playback_play` | — | Start playback |
| `fb2k_playback_pause` | — | Pause playback |
| `fb2k_playback_stop` | — | Stop playback |
| `fb2k_playback_next` | — | Next track |
| `fb2k_playback_previous` | — | Previous track |
| `fb2k_playback_play_pause` | — | Toggle play / pause |
| `fb2k_playback_get_state` | — | Get playback state (`stopped` / `paused` / `playing`) |
| `fb2k_playback_get_current_track` | — | Get current track info (title, artist, album, duration, …) |
| `fb2k_playback_get_position` | — | Get playback position and total length (seconds) |
| `fb2k_playback_set_position` | `seconds` | Seek to a position |
| `fb2k_playback_get_volume` | — | Get volume (0-100) and mute state |
| `fb2k_playback_set_volume` | `volume` (0-100) | Set volume |

### Playlist (7)

| Tool | Params | Description |
|------|--------|-------------|
| `fb2k_playlist_get_all` | — | Get all playlists |
| `fb2k_playlist_get_active` | — | Get the active playlist |
| `fb2k_playlist_set_active` | `playlist` (index) | Switch the active playlist |
| `fb2k_playlist_get_tracks` | `playlist?`, `start?`, `count?` | Get tracks (paginated) |
| `fb2k_playlist_play_track` | `index`, `playlist?`, `deferred?` | Play a track |
| `fb2k_playlist_create` | `name` | Create a new playlist |
| `fb2k_playlist_remove` | `playlist` (index) | Delete a playlist |

### Library (4)

| Tool | Params | Description |
|------|--------|-------------|
| `fb2k_library_search` | `query`, `offset?`, `limit?` | Search the library (supports fb2k query syntax) |
| `fb2k_library_get_albums` | — | Get all albums |
| `fb2k_library_get_artists` | — | Get all artists |
| `fb2k_library_get_stats` | — | Get library statistics |

### Playback — extended (13)

| Tool | Params | Description |
|------|--------|-------------|
| `fb2k_playback_mute` | `muted?` | Set mute (default `true`) |
| `fb2k_playback_toggle_mute` | — | Toggle mute |
| `fb2k_playback_volume_up` | — | Volume up one step |
| `fb2k_playback_volume_down` | — | Volume down one step |
| `fb2k_playback_get_playback_order` | — | Get playback-order mode |
| `fb2k_playback_set_playback_order` | `order` | Set playback-order mode |
| `fb2k_playback_get_stop_after_current` | — | Get "stop after current" state |
| `fb2k_playback_set_stop_after_current` | `enabled` | Set "stop after current" |
| `fb2k_playback_toggle_stop_after_current` | — | Toggle "stop after current" |
| `fb2k_playback_get_current_track_index` | `includeTrackInfo?` | Get the current track index |
| `fb2k_playback_get_playing_playlist` | — | Get the currently playing playlist |
| `fb2k_playback_play_path` | `path` | Play a specific file path |
| `fb2k_playback_random` | — | Play a random track |

### Playlist — extended (40)

<details><summary>Show all 40 tools</summary>

| Tool | Params | Description |
|------|--------|-------------|
| `fb2k_playlist_get_count` | — | Get the total number of playlists |
| `fb2k_playlist_get_playing` | — | Get info about the playing playlist |
| `fb2k_playlist_get_track_count` | `playlist?` | Get a playlist's track count |
| `fb2k_playlist_get_available_columns` | — | Get available column definitions |
| `fb2k_playlist_rename` | `playlist`, `name` | Rename a playlist |
| `fb2k_playlist_clear` | `playlist?` | Clear a playlist |
| `fb2k_playlist_duplicate` | `playlist?`, `name?` | Duplicate a playlist |
| `fb2k_playlist_reorder_playlists` | `newOrder` | Reorder playlists |
| `fb2k_playlist_insert_tracks` | `handles`, `playlist?`, `position?` | Insert tracks |
| `fb2k_playlist_remove_tracks` | `items`, `playlist?` | Remove tracks by index |
| `fb2k_playlist_remove_selected_tracks` | `playlist?` | Remove selected tracks |
| `fb2k_playlist_move_tracks` | `delta`, `playlist?`, `items?` | Move track positions |
| `fb2k_playlist_reorder` | `newOrder`, `playlist?` | Custom track ordering |
| `fb2k_playlist_reverse` | `playlist?` | Reverse track order |
| `fb2k_playlist_sort` | `playlist?`, `pattern?`, `descending?`, `selectedOnly?` | Sort by title-format pattern |
| `fb2k_playlist_shuffle` | `playlist?` | Shuffle order |
| `fb2k_playlist_add_paths` | `paths`, `playlist?` | Add tracks by file path |
| `fb2k_playlist_add_handles` | `handles`, `playlist?` | Add tracks by handle |
| `fb2k_playlist_add_paths_sequential` | `paths`, `playlist?` | Add file paths sequentially |
| `fb2k_playlist_add_paths_async` | `paths`, `playlist?` | Add file paths asynchronously |
| `fb2k_playlist_replace_all_and_play` | `paths`, `playlist?`, `playIndex?`, `stopFirst?`, `autoPlay?` | Atomically replace and play |
| `fb2k_playlist_get_selected_tracks` | `playlist?` | Get selected track info |
| `fb2k_playlist_get_selection` | `playlist?` | Get selected track indices |
| `fb2k_playlist_set_selection` | `indices`, `playlist?`, `clearOthers?` | Set the selection |
| `fb2k_playlist_select_all` | `playlist?` | Select all |
| `fb2k_playlist_deselect_all` | `playlist?` | Deselect all |
| `fb2k_playlist_get_focused_track` | `playlist?` | Get the focused track index |
| `fb2k_playlist_set_focused_track` | `index`, `playlist?` | Set the focused track |
| `fb2k_playlist_focus_track` | `playlist?`, `index?` | Deprecated |
| `fb2k_playlist_get_focus_track` | `playlist?` | Deprecated |
| `fb2k_playlist_undo` | `playlist?` | Undo |
| `fb2k_playlist_redo` | `playlist?` | Redo |
| `fb2k_playlist_get_lock_info` | `playlist?` | Get lock info |
| `fb2k_playlist_is_locked` | `playlist?` | Check whether locked |
| `fb2k_playlist_is_autoplaylist` | `playlist?` | Check whether it is an autoplaylist |
| `fb2k_playlist_create_autoplaylist` | `query`, `name?`, `sort?`, `keepSorted?` | Create an autoplaylist |
| `fb2k_playlist_convert_to_autoplaylist` | `query`, `playlist?`, `sort?`, `keepSorted?` | Convert to an autoplaylist |
| `fb2k_playlist_remove_autoplaylist` | `playlist?` | Remove autoplaylist state |
| `fb2k_playlist_get_autoplaylist_info` | `playlist?` | Get autoplaylist details |
| `fb2k_playlist_get_autoplaylist_query` | `playlist?` | Get the autoplaylist query |

</details>

### Queue (8)

| Tool | Params | Description |
|------|--------|-------------|
| `fb2k_queue_get` | — | Get queue contents |
| `fb2k_queue_add` | `playlist?`, `tracks?`, `track?` | Add tracks to the queue |
| `fb2k_queue_add_paths` | `paths`, `useQueuePlaylist?`, `playlist?` | Add to the queue by file path |
| `fb2k_queue_remove` | `index?`, `indices?` | Remove tracks at the given positions |
| `fb2k_queue_clear` | — | Clear the queue |
| `fb2k_queue_get_count` | — | Get the queue track count |
| `fb2k_queue_move_to_top` | `index` | Move a track to the top of the queue |
| `fb2k_queue_flush` | — | Flush the queue |

### Metadata & rating (12)

| Tool | Params | Description |
|------|--------|-------------|
| `fb2k_metadata_read` | `path` | Read file metadata (structured: tags + info separated) |
| `fb2k_metadata_read_by_path` | `path` | Read metadata by path (flat format) |
| `fb2k_metadata_read_raw` | `path`, `cueIndex?` | Read raw metadata directly from the file |
| `fb2k_metadata_read_batch` | `paths` | Read metadata in batch |
| `fb2k_metadata_write` | `path`, `tags` | Write metadata tags |
| `fb2k_metadata_write_batch` | `items` | Write metadata in batch |
| `fb2k_metadata_embed_artwork` | `path`, `imageData`, `type?` | Embed cover art |
| `fb2k_metadata_remove_embedded_art` | `path`, `type?`, `removeAll?` | Remove embedded art |
| `fb2k_metadata_remove_tag` | `path`, `tags` | Remove specific tags |
| `fb2k_metadata_remove_field` | `path`, `tags` | Remove specific fields |
| `fb2k_rating_set` | `rating`, `path?`, `cueIndex?` | Set a rating |
| `fb2k_rating_get` | `path`, `cueIndex?` | Get a rating |

### Artwork (2)

| Tool | Params | Description |
|------|--------|-------------|
| `fb2k_artwork_get_current` | `type?` | Get current track artwork (base64) |
| `fb2k_artwork_get_for_track` | `path`, `type?` | Get artwork for a track |

Cover types: `front`, `back`, `disc`, `icon`, `artist`.

### UI testing (4)

| Tool | Params | Description |
|------|--------|-------------|
| `fb2k_screenshot` | `fullPage?` | Capture a page screenshot (returns PNG) |
| `fb2k_dom_snapshot` | — | Get a DOM accessibility snapshot |
| `fb2k_evaluate` | `expression` | Run a JavaScript expression (requires `FB2K_ENABLE_EVAL=1`) |
| `fb2k_console_messages` | — | Get console logs |

---

## Usage examples

### Example 1 — query the current playback state

```
User: What's playing right now?
AI -> fb2k_playback_get_current_track
AI: Now playing "Redo" by Mili, from the album "Millennium Mother", 3:53.
```

### Example 2 — search and play

```
User: Find songs by Mili and play the first one.
AI -> fb2k_library_search { query: "artist IS Mili" }
AI -> fb2k_playlist_play_track { index: 0 }
AI: Started playing "Redo" by Mili.
```

### Example 3 — screenshot to verify a theme

```
User: Take a screenshot of the current UI.
AI -> fb2k_screenshot { fullPage: true }
AI: [shows screenshot] The theme loaded correctly; the playback bar is at the bottom…
```

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
│   ├── types.ts              # Shared types
│   └── tools/
│       ├── playback.ts       # 12 playback-control tools
│       ├── playback-ext.ts   # 13 playback-extended tools
│       ├── playlist.ts       # 7 playlist tools
│       ├── playlist-ext.ts   # 40 playlist-extended tools
│       ├── library.ts        # 4 library tools
│       ├── artwork.ts        # 2 artwork tools
│       ├── queue.ts          # 8 queue tools
│       └── metadata.ts       # 12 metadata + rating tools
├── tests/
│   ├── api-alignment.test.ts      # C++ source cross-check gate
│   ├── api-existence-gate.test.ts # API allowlist anti-hallucination gate
│   ├── bridge-executor.test.ts    # BridgeExecutor unit tests
│   ├── cdp-client.test.ts         # CdpClient unit tests
│   ├── error-paths.test.ts        # Error-path coverage
│   ├── tools-integration.test.ts  # Tool integration tests
│   ├── e2e-smoke.mjs              # CDP end-to-end smoke test
│   └── e2e-interact.mjs          # CDP interaction test
├── dist/                     # Compiled output
├── package.json
└── tsconfig.json
```

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

1. CDP remote debugging is enabled in the foobar2000 component (advanced settings).
2. The CDP port (default `9222`) is not occupied by another program.
3. For a non-default port, set the `FB2K_CDP_PORT` environment variable.

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
- [x] Extended tool set (playback / playlist extended, queue, metadata + rating — 98 bridge tools total)
- [ ] More extensions (window / config / audio / columns)
- [ ] Event subscriptions (MCP resource mechanism)
- [ ] Broader coverage and CI

See the project repository: [NereaFantasia/foo_ui_webview2](https://github.com/NereaFantasia/foo_ui_webview2).

---

## License

Released under the [MIT License](./LICENSE), consistent with the licensing of the `foo_ui_webview2` SDK / MCP sub-packages.
