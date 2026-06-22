English | [中文](./README.zh-CN.md)

# src/callbacks/ — foobar2000 Events → Frontend Event Bridge

`callbacks/` is the source of "event return": it listens for the various foobar2000 SDK callbacks (playback, playlist, library, metadata, queue, statistics, DSP, config, audio devices), translates them into colon-format bridge events, and broadcasts them to all WebView instances via `WebViewContext::BroadcastEvent`, letting the frontend perceive foobar2000's state changes in real time.

---

## Overview

This is "③ Event return" in the three-stage bridge. In contrast to `api/` (where the frontend actively invokes), this module is foobar2000 proactively notifying the frontend. The unified pattern is:

```
foobar2000 SDK callback (on_playback_new_track / on_items_added / ...)
      └─ build JSON data
            └─ WebViewContext::GetInstance().BroadcastEvent("namespace:event", data)
                  └─ each instance postMessage → fb2k.on("namespace:event", cb)
```

> Naming convention: event names always use the **colon format** (`playback:trackChanged`), strictly distinguished from the dot format of invoke (`playback.play`). When the frontend isn't receiving events, the first thing to check is whether dot/colon got confused.

---

## Registration: Manual vs Automatic

Callbacks have two registration paths:

- **Manual initialization**: explicitly call `InitXxxCallbacks()` in `core/WebViewPanel::InitializeCallbacks()`. This includes `InitPlaybackCallbacks` / `InitPlaylistCallbacks` / `InitLibraryCallbacks` / `InitMetadbCallbacks`.
- **SDK auto-registration**: self-registers at component load time via `service_factory_single_t` / `playback_statistics_collector_factory_t`, with no manual initialization needed. This includes the queue, statistics, DSP, and config callbacks.
- **Dynamic registration**: `AudioConfigCallback` is registered at startup by `initquit`, with `AudioConfigCallbackManager` managing its lifecycle.

---

## Key Files / Classes & Event Inventory

| File | Registration | SDK Changes Listened | Events Broadcast |
|------|---------|----------------|-----------|
| `PlaybackCallback.*` | Manual `InitPlaybackCallbacks` | Playback state, track change, volume, progress, dynamic info | `playback:trackChanged` `playback:starting` `playback:stopped` `playback:paused` `playback:seeked` `playback:volumeChanged` `playback:time` `playback:timeHighRes` `playback:stateChanged` `playback:dynamicInfo` `playback:dynamicInfoTrack` `playback:edited` |
| `PlaylistCallback.*` | Manual `InitPlaylistCallbacks` | Playlist content/selection/focus/add-remove-modify, playback order | `playlist:itemsAdded` `playlist:itemsRemoved` `playlist:itemsReordered` `playlist:itemsReplaced` `playlist:selectionChanged` `playlist:focusChanged` `playlist:created` `playlist:removed` `playlist:reordered` `playlist:activated` `playlist:renamed` `playlist:lockChanged` `playlist:defaultFormatChanged` `playback:orderChanged` |
| `LibraryCallback.*` | Manual `InitLibraryCallbacks` | Library add/remove/modify, initialization (and triggers cache invalidation) | `library:itemsAdded` `library:itemsRemoved` `library:itemsModified` `library:initialized` |
| `MetadbCallback.*` | Manual `InitMetadbCallbacks` | Metadata changes (rating, tags, playback statistics) | `metadb:changed` |
| `QueueCallback.*` | Automatic `service_factory_single_t` | Playback queue changes | `playback:queueChanged` |
| `PlaybackStatsCallback.*` | Automatic `playback_statistics_collector_factory_t` | A track is deemed "played" (>=60s, or it ended after playing >1/3) | `playback:itemPlayed` |
| `DspCallback.*` | Automatic `service_factory_single_t` | DSP preset changes | `audio:dspPresetChanged` |
| `ConfigCallback.*` | Automatic `service_factory_single_t` (`config_object_notify`, listening to 4 standard config objects) | Always on top, stop after current, playback follows cursor, cursor follows playback | `window:alwaysOnTopChanged` `playback:stopAfterCurrentChanged` `playback:followCursorChanged` `playback:cursorFollowChanged` |
| `AudioConfigCallback.*` | Dynamic `initquit` | Output device, ReplayGain mode changes | `audio:outputDeviceChanged` `audio:replaygainModeChanged` |

> `QueueCallback` (the SDK playback queue `playback:queueChanged`) and `core/QueueManager` (the JIT streaming shadow queue `jitQueue:*`) are two different mechanisms; do not confuse them.

---

## How It Works / Data Flow

Using a track change as an example:

```
user clicks next / auto-continue
   └─ playback_control switches track
        └─ on_playback_new_track(metadb_handle_ptr)        (PlaybackCallback)
              ├─ build trackInfo JSON
              ├─ WebViewContext::BroadcastEvent("playback:trackChanged", trackInfo)
              └─ QueueManager::OnPlaybackNewTrack(track)     (advance the JIT state machine)
```

While broadcasting `library:*` events, `LibraryCallback` also calls `LibraryCache::Invalidate()` / `LibraryTreeIndex::Invalidate()` to ensure the next library query reads the latest data — it is both an event source and the guardian of cache consistency.

Some high-frequency events (such as selection changes and progress) are throttled/deduplicated at the source (see the 50ms throttle of `selection/SelectionWatcher` and the progress-emission control of `PlaybackCallback`), to avoid overwhelming the frontend.

---

## Dependencies

- **Depends on**: the foobar2000 SDK callback interfaces (`play_callback` / `playlist_callback` / `library_callback` / `config_object_notify`, etc.), `core/WebViewContext` (the broadcast channel), `core/QueueManager`, and `window/TaskbarIntegration` (playback state driving taskbar buttons).
- **Depended on by**: `core/WebViewPanel::InitializeCallbacks()` brings up the manually registered callbacks; the frontend consumes these events through `fb2k.on()` / the SDK's `fb.on()`.

---

## Extension Guide

To add a new event (e.g., `playback:fooChanged`):

1. **Three-point verification**: confirm the SDK has the corresponding callback, that JSON field naming is stable, and that the event name uses the colon format (distinguished from any invoke's dot name).
2. **Choose a registration method**: self-register via `service_factory_single_t` whenever possible; for those that need to initialize with the panel lifecycle, put them in `InitXxxCallbacks()` and call it from `WebViewPanel::InitializeCallbacks()`.
3. **Broadcast**: in the callback implementation, call `WebViewContext::GetInstance().BroadcastEvent("playback:fooChanged", data)`; to send to a single instance, route via `api/CallerContext` instead.
4. **Sync**: register the new event in `sdk/` (event types) and `docs/` (event inventory), keeping the documentation consistent with the code.
5. **Throttle**: high-frequency events must be deduplicated/throttled at the source to avoid flooding the frontend.

---

See also: the repository root [README.md](../../README.md) ("Events (colon format)"), [../api/README.md](../api/README.md), and [../core/README.md](../core/README.md).
