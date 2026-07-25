# Queues, Discovery, and Keyboard Shortcuts

This page covers the `fb.queue`, `fb.jitQueue`, `fb.discovery`, and `fb.keyboard` namespaces.

## fb.queue Play Queue

`fb.queue` controls foobar2000's play queue. It is independent of the adaptive streaming queue exposed as `fb.jitQueue`.

### get()

`fb.queue.get(): Promise<QueueGetResponse>` returns `{ items, count }`. Each `QueueItem` extends `TrackInfo` and may include the source `playlist` and `playlistItem` indices.

```javascript
const queue = await fb.queue.get();
for (const item of queue.items) {
	console.log(item.path, item.playlist, item.playlistItem);
}
```

### getCount()

`fb.queue.getCount(): Promise<{ count: number }>` returns the current queue length.

### add(options)

`fb.queue.add(options: QueueAddParams)` adds playlist tracks to the queue. The generated contract accepts a `playlist` selector and a `tracks` array. The response can include `addedCount` and `queueCount`.

```javascript
await fb.queue.add({ playlist: 0, tracks: [3, 5] });
```

### addPaths(paths, options?)

`fb.queue.addPaths(paths, options?)` adds paths or URLs. `options` is `Omit<QueueAddPathsParams, 'paths'>` and forwards the generated `useQueuePlaylist` and `playlist` fields when supplied.

Each path or URL is limited to 2048 characters. Over-length entries are skipped and reported through `invalidCount`.

```javascript
const result = await fb.queue.addPaths(
	['E:\\Music\\song.flac'],
	{ useQueuePlaylist: true },
);
```

### remove(index)

`fb.queue.remove(index: number)` removes one queue entry and can return `removedIndex`, `removedCount`, and `queueCount`.

```javascript
await fb.queue.remove(0);
```

### moveToTop(index)

`fb.queue.moveToTop(index: number)` moves an entry to the front of the queue and can return `movedIndex` and `queueCount`.

```javascript
await fb.queue.moveToTop(3);
```

### flush()

`fb.queue.flush()` clears the host queue through `queue.flush` and can return `clearedCount`.

### clear()

`fb.queue.clear()` clears the play queue and can return `clearedCount`.

```javascript
await fb.queue.clear();
```

## fb.jitQueue Just-in-time Queue

`fb.jitQueue` is the adaptive playback queue used to preload and switch tracks at runtime. Its item contract uses `trackId`, `title`, and `url`, not playlist indices.

### getState()

`fb.jitQueue.getState(): Promise<JitQueueStateInfo>` returns `isActive`, `state`, `currentTrackId`, `nextTrackId`, `bufferSize`, and `shadowPlaylist`.

```javascript
const state = await fb.jitQueue.getState();
```

### enqueueNext(options)

`fb.jitQueue.enqueueNext(options: JitQueueEnqueueNextParams)` queues a track to play next. URLs are limited to 2048 characters; an over-length URL resolves with `{ success: false }`.

```javascript
await fb.jitQueue.enqueueNext({
	trackId: 'next-track',
	title: 'Next track',
	url: 'https://example.invalid/audio/next.flac',
});
```

### playNow(options)

`fb.jitQueue.playNow(options: JitQueuePlayNowParams)` starts a track immediately and may return `shadowPlaylist`. The same 2048-character URL limit applies.

```javascript
await fb.jitQueue.playNow({
	trackId: 'current-track',
	title: 'Current track',
	url: 'https://example.invalid/audio/current.flac',
});
```

### preloadBatch(options)

`fb.jitQueue.preloadBatch(options: JitQueuePreloadBatchParams)` preloads `urls` from an optional `startIndex`. Set `replace` to control whether the existing preload buffer is replaced. Over-length URLs are skipped and counted in `invalidCount`.

```javascript
await fb.jitQueue.preloadBatch({
	urls: ['https://example.invalid/audio/one.flac'],
	startIndex: 0,
	replace: true,
});
```

### skip(), stop(), clear(), and notifyEmpty()

- `skip()` advances past the current JIT item and may return `currentTrackId`.
- `stop()` invokes `jitQueue.stop` with the facade's no-argument default behavior.
- `clear()` empties the JIT queue.
- `notifyEmpty()` notifies the host that the producer has no more tracks.

### JIT Queue Events

Subscribe through `fb.on()` to `jitQueue:needNext`, `jitQueue:trackChanged`, `jitQueue:listExhausted`, `jitQueue:preloadComplete`, and `jitQueue:error`. The published payload types are available from the package root.

```javascript
const off = fb.on('jitQueue:needNext', ({ currentTrackId, reason }) => {
	console.log(currentTrackId, reason);
});
```

## fb.discovery Service Discovery

`fb.discovery` enumerates foobar2000 services, menu commands, components, input formats, DSP entries, output devices, UI elements, and preference pages.

### Menu Discovery and Execution

| Method | Contract |
| --- | --- |
| `getMainMenuCommands(options?)` | Returns `{ commands, count, dynamicCount }`. Each command includes `name`, `description`, `guid`, `parentGuid`, and `index`. Runtime submenus (`mainmenu_commands_v2`) are expanded by default; expanded children add `subGuid`, `isDynamic`, and `path`. Pass `{ expandDynamic: false }` for the static registry only. |
| `getMainMenuGroups()` | Returns `{ groups, count }`. Only statically registered `mainmenu_group` services are listed; runtime submenus are command-node trees, so use `getMainMenuCommands()` for those. |
| `executeMainMenuCommand(guid, subGuid?)` | Invokes a main-menu command by GUID; pass `subGuid` for an entry expanded from a dynamic submenu. |
| `getContextMenuCommands()` | Returns `{ commands, count }`. |
| `executeContextMenuCommand(options)` | Accepts `DiscoveryExecuteContextMenuCommandParams`; the facade forwards `guid`. |
| `executeContextMenuByPath(options)` | Accepts `path` and optional `trackPath`, then may return `foundName` and `itemCount`. |
| `getContextMenuTree()` | Returns the current context-menu tree and optional `itemCount`. |
| `searchCommands(query, options?)` | Searches main-menu command names, descriptions, and paths; returns optional `results` and `count`. Dynamic submenus are expanded by default. |

```javascript
const commands = await fb.discovery.getMainMenuCommands();
await fb.discovery.executeMainMenuCommand(commands.commands[0].guid);

await fb.discovery.executeContextMenuByPath({
	path: 'Properties',
	trackPath: 'E:\\Music\\song.flac',
});
```

### Service Inventories

| Method | Response data |
| --- | --- |
| `getAllServices()` | `{ services, totalServices }`, where `services` contains aggregate counts. |
| `getInputFormats()` | `{ fileTypes, count }`. |
| `getComponents()` | `{ components, count }`. |
| `getUIElements()` | `{ elements, count }`. |
| `getDspEntries()` | `{ entries, count }`. |
| `getOutputDevices()` | `{ devices, count }`. |
| `getPreferencePages()` | `{ pages, count }`. |

```javascript
const formats = await fb.discovery.getInputFormats();
const components = await fb.discovery.getComponents();
```

## fb.keyboard Hotkeys and Shortcuts

### registerHotkey(key, action, options?)

`fb.keyboard.registerHotkey(key, action, options?)` invokes `keyboard.registerHotkey`. `options` is `Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>` and can set `global`. The facade currently types the optional response `id` as `string`; `getRegisteredHotkeys()` exposes each registered `HotkeyInfo.id` as `number`.

```javascript
const result = await fb.keyboard.registerHotkey(
	'Ctrl+Shift+P',
	'playPause',
	{ global: true },
);
```

### registerShortcut(key, action)

`fb.keyboard.registerShortcut(key, action)` registers a shortcut without the extended options object.

```javascript
await fb.keyboard.registerShortcut('Space', 'toggle');
```

### unregisterHotkey(options)

`fb.keyboard.unregisterHotkey(options: KeyboardUnregisterHotkeyParams)` unregisters by `key`, `id`, or the supported combination.

```javascript
await fb.keyboard.unregisterHotkey({ key: 'Ctrl+Shift+P' });
```

### getRegisteredHotkeys()

`fb.keyboard.getRegisteredHotkeys(): Promise<KeyboardGetRegisteredHotkeysResponse>` returns `{ success, hotkeys }`. Each `HotkeyInfo` contains `id`, `key`, `action`, and `global`.

```javascript
const { hotkeys } = await fb.keyboard.getRegisteredHotkeys();
```

### keyboard:hotkey

The `keyboard:hotkey` event carries `KeyboardHotkeyPayload` with `id`, `key`, and `action`.

```javascript
fb.on('keyboard:hotkey', ({ id, key, action }) => {
	console.log(id, key, action);
});
```
