# MCP Server Overview

Control foobar2000 from an AI agent through the [Model Context Protocol (MCP)](https://modelcontextprotocol.io/). The server registers **104 tools by default**: 101 Bridge tools and 3 always-on UI-testing tools. Setting `FB2K_ENABLE_EVAL` to `1` or `true` adds `fb2k_evaluate`, bringing the total to **105 tools**.

## What is MCP?

MCP is an open protocol that lets AI clients such as VS Code, Claude Desktop, and Cursor call external tools through a standard interface. `foo-ui-webview2-mcp` exposes a controlled mapping of the foobar2000 Bridge API as MCP tools for playback control, playlist management, library queries, metadata operations, and UI inspection.

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

1. The AI agent requests a tool call such as `fb2k_playback_play`.
2. The server resolves the tool through its explicit method map to `playback.play`.
3. The CDP client evaluates `window.fb2k.invoke('playback.play', {})` in WebView2.
4. The C++ Bridge handles the request and returns a JSON-compatible value.
5. The MCP server serializes the result for the client.

### Implementation characteristics

| Characteristic | Behavior |
| --- | --- |
| Pre-connect | After the stdio server starts, it attempts a CDP connection; a failed attempt is retried on the first tool call. |
| Screenshot warm-up | Each successful CDP connection performs a best-effort 1×1 PNG capture before real screenshots. No fixed timing guarantee is implied. |
| Automatic reconnect | A disconnected client retries with 1 s, 2 s, and 4 s delays after the initial attempt. |
| Method validation | Bridge method IDs must match the `namespace.method` pattern before they are interpolated into the evaluated expression. |
| Structured logging | JSON Lines are written to stderr so stdout remains available for MCP stdio traffic. |

## Tool groups

| Group | Tools | Scope |
| --- | --- | --- |
| Playback | 12 | Playback control, state, position, and volume |
| Playback Extended | 13 | Mute, playback order, stop-after-current, and path playback |
| Playlist | 7 | Core playlist operations |
| Playlist Extended | 40 | Ordering, selection, focus, locking, and autoplaylists |
| Library | 4 | Media-library search and statistics |
| Artwork | 2 | Artwork retrieval |
| Queue | 8 | Playback-queue management |
| Metadata | 12 | Metadata, artwork writing, and ratings |
| UI Testing | 3 default + 1 conditional | Screenshot, DOM snapshot, console messages, and optional evaluation |

## Relationship to the SDK and Bridge API

| Dimension | MCP tools | SDK (`fb.*`) | Low-level API (`fb2k.invoke`) |
| --- | --- | --- | --- |
| Caller | AI agent | Web frontend | Web frontend |
| Transport | stdio + CDP | WebView2 `postMessage` | WebView2 `postMessage` |
| Typical use | AI automation and testing | Theme and application development | Direct Bridge access |
| Parameters | Registered MCP input schema | SDK function arguments | JSON object |

::: tip Controlled parameter mapping
Each Bridge tool has an explicit tool-name-to-method mapping. The generic registration path enforces required fields, string enums, declared defaults, primitive `string` / `number` / `integer` / `boolean` types, and the outer array shape before passing fields to the mapped `fb2k.invoke()` method. Object-valued fields use the generic fallback. Numeric `minimum` / `maximum` values and array item types shown on these pages come from `ToolDefinition` metadata and describe the intended contract, but the current generic Zod conversion does not apply those two classes of constraint; the mapped Bridge handler may perform additional validation. This is a controlled subset and mapping of the Bridge contract, not a guarantee that every low-level parameter or coercion is exposed unchanged.
:::

## Usage examples

### Query current playback

```text
User: What's playing right now?
AI → fb2k_playback_get_current_track
AI: "Redo" by Mili is playing.
```

### Search and play

```text
User: Find songs by Mili and play the first result.
AI → fb2k_library_search { query: "artist IS Mili" }
AI → fb2k_playlist_play_track { index: 0 }
AI: Started playing "Redo" by Mili.
```

### Verify the UI with a screenshot

```text
User: Take a screenshot of the current UI.
AI → fb2k_screenshot { fullPage: true }
AI: [shows screenshot] The theme loaded and the playback bar is visible.
```
