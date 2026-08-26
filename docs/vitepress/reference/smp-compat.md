# SMP compatibility layer

Spider Monkey Panel (SMP) compatibility layer for running SMP-style scripts inside the WebView2 UI. The source layer is `sdk/src/smp/**` (bootstrap entry: `bootstrapSmpCompat`); published dist output is generated and must not be hand-edited.

## Quick start

```html
<!-- 1. bridge.js must load first -->
<script src="bridge.js"></script>
<!-- 2. smp-compat.js loads the wrappers -->
<script src="smp-compat.js"></script>

<script>
  await window.smp.ready;
  console.log('IsPlaying:', fb.IsPlaying);
  console.log('ActivePlaylist:', plman.ActivePlaylist);
</script>
```

## Architecture differences from the lower-level API

::: warning Read this before porting scripts
The compatibility layer intentionally preserves SMP synchronous-read semantics. It is not a 1:1 transparent rewrite of every `fb2k.invoke()` call.
:::

### Why a cache exists

Most SMP APIs are synchronous. WebView2 ↔ C++ communication is asynchronous (`postMessage` / `invoke`). Replacing `fb.IsPlaying` with `await fb2k.invoke('playback.getState')` would break existing scripts.

The compatibility layer therefore uses a **cache + event** model: eventual consistency in exchange for synchronous reads.

### Two modes

**Lower-level API (`fb2k.invoke`)** — each call is an independent request to C++ and returns the live result.

```javascript
const state = await fb2k.invoke('playback.getState');
```

**Compatibility layer** — reads are local/sync; writes update the cache first and notify C++ asynchronously.

```javascript
const playing = fb.IsPlaying; // from _cache.isPlaying
plman.ActivePlaylist = 2;
// immediate: _cache.activePlaylist = 2
// async fire-and-forget: invoke('playlist.setActive', { playlist: 2 })
```

### Benefits

- SMP synchronous-read semantics are preserved for UI-friendly properties
- High-frequency UI reads do not require per-frame `await`
- Fire-and-forget setters make immediate re-reads match SMP expectations
- Same BridgeCore entrypoint and security checks as the lower-level API

### Risks

1. Brief cache/backend divergence after a rejected write until the next event-driven refresh
2. Initialization ordering: always `await window.smp.ready`
3. Non-1:1 behaviors listed in the differences table below

## Architecture overview

Core entry: `sdk/src/smp` / published `smp-compat.js` — cache system, event mapping, `plman`, and `fb` extensions.

Wrapper modules under `sdk/src/smp/` include:

| File | Class / object | Role |
| --- | --- | --- |
| `utils.js` | shared helpers | handle normalization, menu item builders |
| `FbMetadbHandle.js` | `FbMetadbHandle` | track handle surface |
| `FbMetadbHandleList.js` | `FbMetadbHandleList` | handle list surface |
| `FbTitleFormat.js` | `FbTitleFormat` | titleformat evaluation helpers |
| `FbProfiler.js` | `FbProfiler` | timing helper |
| `FbFileInfo.js` | `FbFileInfo` | metadata/info surface |
| `FbUiSelectionHolder.js` | `FbUiSelectionHolder` | selection holder |
| `ContextMenuManager.js` | `ContextMenuManager` | context menu manager |
| `MainMenuManager.js` | `MainMenuManager` | main menu manager |

### Cache + event mechanism

1. Initialization fills `_cache` with a batch of backend reads
2. Runtime listens to bridge events and maps SMP names through `fb.onSMP()`
3. Property reads return from cache without another C++ round-trip

Cached fields include playback state, playlist metadata, follow-cursor flags, ReplayGain mode, and common path roots.

## fb object — properties

| Property | Type | Access | Notes |
| --- | --- | --- | --- |
| `fb.IsPlaying` | boolean | read-only | Playing state |
| `fb.IsPaused` | boolean | read-only | Paused state |
| `fb.Volume` | number | read/write | Volume in dB (`-100..0`) |
| `fb.PlaybackTime` | number | read/write | Position in seconds |
| `fb.PlaybackLength` | number | read-only | Current track duration |
| `fb.StopAfterCurrent` | boolean | read/write | Stop after current track |
| `fb.AlwaysOnTop` | boolean | read/write | Always-on-top window state |
| `fb.CursorFollowPlayback` | boolean | read/write | Cursor follows playback |
| `fb.PlaybackFollowCursor` | boolean | read/write | Playback follows cursor |
| `fb.ReplaygainMode` | number | read/write | ReplayGain mode |
| `fb.ComponentPath` | string | read-only | Component directory |
| `fb.FoobarPath` | string | read-only | foobar2000 install directory |
| `fb.ProfilePath` | string | read-only | Profile directory |

## fb object — methods

### Playback control

`fb.Play()` / `fb.Pause()` / `fb.Stop()` / `fb.Next()` / `fb.Prev()` / `fb.Random()` / `fb.PlayOrPause()` / `fb.VolumeUp()` / `fb.VolumeDown()` / `fb.VolumeMute()` / `fb.Exit()`

All return Promises.

### Factory methods

| Method | Returns | Notes |
| --- | --- | --- |
| `fb.TitleFormat(expr)` | `FbTitleFormat` | Titleformat object |
| `fb.CreateHandleList()` | `FbMetadbHandleList` | Empty handle list |
| `fb.CreateProfiler(name)` | `FbProfiler` | Profiler |
| `fb.AcquireUiSelectionHolder()` | `FbUiSelectionHolder` | Selection holder |
| `fb.CreateContextMenuManager()` | `ContextMenuManager` | Context menu manager |
| `fb.CreateMainMenuManager()` | `MainMenuManager` | Main menu manager |

### Query methods (Promise)

| Method | Returns | Notes |
| --- | --- | --- |
| `fb.GetNowPlaying()` | `FbMetadbHandle \| null` | Now playing |
| `fb.GetFocusItem()` | `FbMetadbHandle \| null` | Focus item |
| `fb.GetSelection()` | `FbMetadbHandleList` | Current selection |
| `fb.GetSelectionType()` | number | Selection type |
| `fb.GetLibraryItems()` | `FbMetadbHandleList` | Full library |
| `fb.GetQueryItems(handles, query)` | `FbMetadbHandleList` | Library query ⚠️ |
| `fb.IsLibraryEnabled()` | boolean | Library enabled |
| `fb.IsMetadbInMediaLibrary(handle)` | boolean | Membership check |

### Command methods (Promise)

| Method | Notes |
| --- | --- |
| `fb.RunMainMenuCommand(command)` | Run main-menu command |
| `fb.RunContextCommand(command)` | Run context-menu command |
| `fb.ShowConsole()` | Show console |
| `fb.ShowPreferences()` | Show preferences |
| `fb.ShowLibrarySearchUI(query)` | Show library search |
| `fb.ShowPopupMessage(msg, title)` | Show popup message |
| `fb.Restart()` | Restart foobar2000 |

## plman object

### Properties

| Property | Type | Access | Notes |
| --- | --- | --- | --- |
| `plman.ActivePlaylist` | number | read/write | Active playlist index |
| `plman.PlayingPlaylist` | number | read-only | Playing playlist index (`-1` when not playing) |
| `plman.PlaylistCount` | number | read-only | Playlist count |
| `plman.PlaybackOrder` | number | read/write | Playback order |

### Synchronous methods (from cache)

| Method | Returns | Notes |
| --- | --- | --- |
| `plman.GetPlaylistName(idx)` | string | Playlist name |
| `plman.PlaylistItemCount(idx)` | number | Item count |
| `plman.FindPlaylist(name)` | number | Find index (`-1` if missing) |
| `plman.IsAutoPlaylist(idx)` | boolean | Autoplaylist flag |
| `plman.IsPlaylistLocked(idx)` | boolean | Lock flag |
| `plman.UndoBackup(idx)` | true | No-op compatibility shim |

### Asynchronous methods (Promise)

| Method | Returns | Notes |
| --- | --- | --- |
| `plman.CreatePlaylist(pos, name)` | number | Create playlist |
| `plman.RemovePlaylist(idx)` | boolean | Remove playlist |
| `plman.RenamePlaylist(idx, name)` | boolean | Rename |
| `plman.ClearPlaylist(idx)` | boolean | Clear |
| `plman.DuplicatePlaylist(from, name)` | number | Duplicate |
| `plman.AddLocations(idx, paths, select)` | number | Add paths |
| `plman.GetPlaylistItems(idx)` | `FbMetadbHandleList` | All items |
| `plman.GetPlaylistSelectedItems(idx)` | `FbMetadbHandleList` | Selected items |
| `plman.InsertPlaylistItems(pl, base, handles, select)` | number | Insert handles |
| `plman.GetPlaylistFocusItemIndex(idx)` | number | Focus index |
| `plman.SetPlaylistFocusItem(idx, item)` | boolean | Set focus |
| `plman.SetPlaylistSelection(idx, items, state)` | boolean | Set selection |
| `plman.ClearPlaylistSelection(idx)` | boolean | Clear selection |
| `plman.RemovePlaylistSelection(idx, crop)` | boolean | Remove/crop selection |
| `plman.SortByFormat(idx, pattern, selectedOnly)` | boolean | Sort by format |
| `plman.MovePlaylistSelection(idx, delta)` | boolean | Move selection |
| `plman.AddItemToPlaybackQueue(handle)` | number | Queue append ⚠️ |
| `plman.GetPlaybackQueueContents()` | Array | Queue contents |
| `plman.CreateAutoPlaylist(idx, name, query, sort, flags)` | number | Create autoplaylist |
| `plman.FlushPlaybackQueue()` | boolean | Flush queue |

## Event system {#smp-events}

### fb.onSMP(eventName, callback)

Register a callback using SMP-style event names. Returns an unsubscribe function.

```javascript
const unsub = fb.onSMP('on_playback_new_track', (track) => {
  console.log('New track:', track?.title);
});
unsub();
```

### Event mapping

**Playback**

| SMP event | WebView2 event | Callback args |
| --- | --- | --- |
| `on_playback_starting` | `playback:starting` | `(command, is_paused)` |
| `on_playback_new_track` | `playback:trackChanged` | `(track_info)` |
| `on_playback_stop` | `playback:stopped` | `(reason: 0=user,1=eof,2=starting_another,3=shutting_down)` |
| `on_playback_pause` | `playback:paused` | `(is_paused)` |
| `on_playback_seek` | `playback:seeked` | `(time)` |
| `on_playback_time` | `playback:time` | `(time)` |
| `on_playback_order_changed` | `playback:orderChanged` | `(new_order_index)` |
| `on_playback_queue_changed` | `playback:queueChanged` | `(origin)` |
| `on_playback_edited` | `playback:edited` | `(data)` |
| `on_playback_dynamic_info` | `playback:dynamicInfo` | `(data)` |
| `on_playback_dynamic_info_track` | `playback:dynamicInfoTrack` | `(data)` |
| `on_item_played` | `playback:itemPlayed` | `(handle)` |
| `on_volume_change` | `playback:volumeChanged` | `(volume_db)` |

**Playlist**

| SMP event | WebView2 event | Callback args |
| --- | --- | --- |
| `on_playlist_switch` | `playlist:activated` | `()` |
| `on_playlist_items_added` | `playlist:itemsAdded` | `(playlist_idx)` |
| `on_playlist_items_removed` | `playlist:itemsRemoved` | `(playlist_idx, new_count)` |
| `on_playlist_items_reordered` | `playlist:itemsReordered` | `(playlist_idx)` |
| `on_playlist_items_selection_change` | `playlist:selectionChanged` | `()` |
| `on_item_focus_change` | `playlist:focusChanged` | `(playlist, from, to)` |
| `on_playlists_changed` | multi-event merge | `()` |

**Selection / metadata / library**

| SMP event | WebView2 event | Callback args |
| --- | --- | --- |
| `on_selection_changed` | `selection:changed` | `()` |
| `on_metadb_changed` | `metadb:changed` | `(handle_list, fromHook)` |
| `on_library_items_added` | `library:itemsAdded` | `()` |
| `on_library_items_removed` | `library:itemsRemoved` | `()` |
| `on_library_items_changed` | `library:itemsModified` | `()` |

**Audio / UI / window**

| SMP event | WebView2 event | Callback args |
| --- | --- | --- |
| `on_dsp_preset_changed` | `audio:dspPresetChanged` | `()` |
| `on_output_device_changed` | `audio:outputDeviceChanged` | `()` |
| `on_replaygain_mode_changed` | `audio:replaygainModeChanged` | `(new_mode)` |
| `on_colours_changed` | `ui:coloursChanged` | `(data)` |
| `on_font_changed` | `ui:fontChanged` | `(data)` |
| `on_always_on_top_changed` | `window:alwaysOnTopChanged` | `(state)` |
| `on_cursor_follow_playback_changed` | `playback:cursorFollowChanged` | `(state)` |
| `on_playback_follow_cursor_changed` | `playback:followCursorChanged` | `(state)` |
| `on_playlist_stop_after_current_changed` | `playback:stopAfterCurrentChanged` | `(state)` |
| `on_focus` | `panel:focus` + `panel:blur` | `(is_focused)` |

Utility formatting notes: `utils.FormatDuration(seconds)` returns `H:MM:SS` or `M:SS`.

## smp object

### smp.ready

```javascript
await window.smp.ready;
```

### smp.refreshCache()

Force a full cache refresh after missed events.

```javascript
await smp.refreshCache();
```

### smp.dispose()

Remove cache-related listeners. Call on unload/navigation.

```javascript
smp.dispose();
```

## Known non-1:1 differences

| API | Limitation | Notes |
| --- | --- | --- |
| `fb.GetQueryItems(handles, query)` | `handles` ignored | Always queries the full library |
| `fb.GetFocusItem(force)` | `force=true` fallback not implemented | Returns `null` when no focus item exists |
| `plman.FindOrCreatePlaylist(name, unlocked)` | `unlocked` ignored | Does not auto-unlock after create |
| `plman.CreateAutoPlaylist(idx, ...)` | position arg ignored | Always appends |
| `plman.AddItemToPlaybackQueue(handle)` | path-based enqueue | Subsong fidelity may be lost |
| `fb.RunContextCommandWithMetadb(cmd, handle)` | `handles` ignored | Backend uses default context |
| `window.NotifyOthers(name, info)` | no automatic `on_notify_data` | Receivers should use `fb2k.on('window:message', ...)` |
| `utils.ColourPicker()` | no native picker | Returns the provided default |
| All methods | Promise-based where C++ is async | Unlike original SMP sync APIs |
| GDI / canvas APIs | unsupported | Use HTML/CSS/Canvas instead |

## window helpers

| Method | Notes | Lower-level API |
| --- | --- | --- |
| `window.GetProperty(name, default?)` | Persistent property read | `config.get` with `smp.prop.` prefix |
| `window.SetProperty(name, value)` | Persistent property write (`null` removes) | `config.set` / `config.remove` |
| `window.NotifyOthers(name, info)` | Broadcast to other windows | `window.broadcast` |

## Migration example

```javascript
await window.smp.ready;

fb.onSMP('on_playback_new_track', async (track) => {
  const tf = fb.TitleFormat('%title% - %artist%');
  const title = await tf.EvalWithMetadb(track);
  console.log('Now playing:', title);
});
```

## Related pages

- [SDK overview](/sdk/overview)
- [Event reference](/reference/events)
- [API events](/api/events)
