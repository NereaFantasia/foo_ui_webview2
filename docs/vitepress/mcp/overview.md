# MCP Server Overview

Control foobar2000 from an AI agent through the [Model Context Protocol (MCP)](https://modelcontextprotocol.io/). The server registers **11 tools by default**: 10 Bridge tools and `fb2k_page_inspect`. Setting `FB2K_ENABLE_EVAL` to `1` or `true` adds `fb2k_page_evaluate`; setting `FB2K_READ_ONLY` leaves only the 5 read-only tools.

## What is MCP?

MCP is an open protocol that lets AI clients such as VS Code, Claude Desktop, and Cursor call external tools through a standard interface. `foo-ui-webview2-mcp` exposes a controlled part of the foobar2000 Bridge API as MCP tools for playback control, playlist management, library queries, track tags and artwork, and page inspection.

## Architecture

```text
AI agent (VS Code / Claude Desktop / Cursor)
    │  MCP (stdio, JSON-RPC)
    ▼
foo-ui-webview2-mcp (Node.js)
    │  CDP (localhost:9222)
    ▼
WebView2 (running inside foobar2000)
    │  window.fb2k.invoke('namespace.method', params)
    ▼
C++ BridgeCore -> foobar2000 SDK
```

### Data flow

1. The AI agent calls a tool with an action, such as `fb2k_playback_control { "action": "playback.play" }`.
2. The server checks the other arguments against the declaration of `playback.play` and fills in its declared defaults.
3. The CDP client evaluates `window.fb2k.invoke('playback.play', {})` in WebView2.
4. The C++ Bridge handles the request and returns a JSON-compatible value.
5. The MCP server returns the result to the client as compact JSON; the cover of `artwork.getCurrent` or `artwork.getForTrack` comes with it as an image.

### Implementation characteristics

| Characteristic | Behavior |
| --- | --- |
| Pre-connect | After the stdio server starts, it attempts a CDP connection; a failed attempt is retried on the first tool call. |
| Screenshot warm-up | Each successful CDP connection performs a best-effort 1×1 PNG capture before real screenshots. No fixed timing guarantee is implied. |
| Automatic reconnect | A disconnected client retries with 1 s, 2 s, and 4 s delays after the initial attempt. |
| Console recording | From the first connection on, the server collects the page's console messages and uncaught exceptions, together with the earlier ones the page still holds, and keeps the last 200. |
| Rate limit | 40 tool calls in a burst, then 20 a second. |
| Structured logging | JSON Lines are written to stderr so stdout remains available for MCP stdio traffic. |

## Tools

| Tool | Kind | Scope |
| --- | --- | --- |
| `fb2k_playback_read` | read-only | Playback state, current track, position, volume, order, and the queue |
| `fb2k_playback_control` | changes state | Transport, seeking, volume, mute, order, and playing a file or playlist row |
| `fb2k_playlist_read` | read-only | Playlists, their tracks, selection, focus, lock and autoplaylist state |
| `fb2k_playlist_manage` | destructive | Creating, removing, renaming, duplicating and reordering playlists; autoplaylists |
| `fb2k_playlist_edit` | destructive | Adding, removing, moving, sorting and replacing a playlist's tracks; undo and redo |
| `fb2k_playlist_select` | changes state | The active playlist, selection and focus |
| `fb2k_library_read` | read-only | Library search, albums, artists and statistics |
| `fb2k_queue_edit` | destructive | Every change to the playback queue |
| `fb2k_track_read` | read-only | Tags, technical info, ratings and artwork of track files |
| `fb2k_track_write` | destructive | Writing tags, ratings and artwork |
| `fb2k_page_inspect` | read-only | Screenshot, DOM snapshot and console messages of the page |
| `fb2k_page_evaluate` | destructive, conditional | JavaScript in the page, with `FB2K_ENABLE_EVAL` only |

Every action of every tool, with its parameters, is listed on the [Tools](./tools.md) page.

## Relationship to the SDK and Bridge API

| Dimension | MCP tools | SDK (`fb.*`) | Low-level API (`fb2k.invoke`) |
| --- | --- | --- | --- |
| Caller | AI agent | Web frontend | Web frontend |
| Transport | stdio + CDP | WebView2 `postMessage` | WebView2 `postMessage` |
| Typical use | AI automation and testing | Theme and application development | Direct Bridge access |
| Parameters | `action` plus the method's parameters | SDK function arguments | JSON object |

::: tip Parameters come from the declarations
The Bridge tools are generated from the same host method declarations as the C++ parameter parsers and the SDK types, so an action takes exactly what its method takes. The server checks every argument against the chosen method's declaration, with its ranges, enums, array element types and nested required fields, before the call; an argument the method would refuse comes back as a tool error that lists what the method takes. A few actions are narrower than their methods, for example page sizes capped at 500 rows so one call cannot flood the client's context.
:::

## Usage examples

### Query current playback

```text
User: What's playing right now?
AI → fb2k_playback_read { action: "playback.getCurrentTrack" }
AI: "Redo" by Mili is playing.
```

### Search and play

```text
User: Find songs by Mili and play the first result.
AI → fb2k_library_read { action: "library.search", query: "artist IS Mili", limit: 20 }
AI → fb2k_playback_control { action: "playback.playPath", path: "D:\\Music\\Mili\\Redo.flac" }
AI: Started playing "Redo" by Mili.
```

### Verify the UI with a screenshot

```text
User: Take a screenshot of the current UI.
AI → fb2k_page_inspect { action: "screenshot", fullPage: true }
AI: [shows screenshot] The theme loaded and the playback bar is visible.
```
