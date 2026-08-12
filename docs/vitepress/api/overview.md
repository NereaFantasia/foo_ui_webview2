# API Overview

This section documents the public Bridge APIs registered by the component.

Current dynamic inventory: **427** public methods across **40** namespaces (**6** internal `*.__*` endpoints excluded).

## Public namespaces

- `artwork` (13 methods)
- `audio` (14 methods)
- `clipboard` (4 methods)
- `config` (29 methods)
- `console` (3 methods)
- `cursor` (2 methods)
- `dialog` (4 methods)
- `discovery` (15 methods)
- `dnd` (3 methods)
- `dsp` (8 methods)
- `event` (2 methods)
- `file` (10 methods)
- `http` (8 methods)
- `jitQueue` (8 methods)
- `keyboard` (4 methods)
- `library` (25 methods)
- `log` (3 methods)
- `lyrics` (3 methods)
- `menu` (8 methods)
- `metadata` (10 methods)
- `misc` (9 methods)
- `output` (3 methods)
- `panel` (2 methods)
- `playback` (27 methods)
- `playcount` (4 methods)
- `playlist` (47 methods)
- `port` (5 methods)
- `queue` (8 methods)
- `rating` (2 methods)
- `replaygain` (8 methods)
- `selection` (6 methods)
- `shell` (5 methods)
- `state` (4 methods)
- `system` (9 methods)
- `taskbar` (5 methods)
- `test` (2 methods)
- `titleformat` (5 methods)
- `tray` (14 methods)
- `ui` (5 methods)
- `window` (81 methods)

## Conventions

- Invoke: `fb2k.invoke('namespace.method', params)`
- Events: `fb2k.on('namespace:eventName', handler)`
- The high-level SDK can expose helpers such as `fb.player.play()`; use the family pages when an application needs the precise Bridge request or response shape.
- For example, `playback:trackChanged` is an event subscription, not an invoke method.
- Internal endpoints matching `*.__*` are not part of the public API surface.
- Each family page owns one or more namespaces via the page map.
