# fb.state Playback State Mirror

`fb.state` is a plain mutable state object that the SDK keeps synchronized through playback events. It is convenient for imperative UI reads and polling without issuing an API call for every update.

## Properties

| Property | Type | Description |
| --- | --- | --- |
| volume | number | Current output volume on a linear 0-100 scale |
| isPlaying | boolean | `true` only when playback state is `'playing'`; paused and stopped states are `false` |
| currentTrack | `TrackInfo \| null` | Most recently announced now-playing track. Initially `null`; stopping playback does not clear it |
| position | number | Current playback position in seconds |

## Automatic Updates

`fb.state` listens to these events:

| Event | Updated fields |
| --- | --- |
| `playback:stateChanged` | Updates `isPlaying` from `state` and updates `position` when present |
| `playback:trackChanged` | Replaces `currentTrack` and sets `isPlaying = true` |
| `playback:stopped` | Sets `isPlaying = false` and `position = 0` |
| `playback:volumeChanged` | Updates `volume` when present |
| `playback:time` | Updates `position` when present |

## Examples

### Basic Reads

```javascript
console.log(fb.state.isPlaying);     // true
console.log(fb.state.volume);        // 80
console.log(fb.state.position);      // 42.5
console.log(fb.state.currentTrack);  // {title, artist, album, ...}
```

### Polling UI Updates

```javascript
setInterval(() => {
    document.getElementById('playing').textContent = fb.state.isPlaying ? '▶' : '⏸';
    document.getElementById('position').textContent = Math.floor(fb.state.position) + 's';
}, 500);
```

### Reading State from an Event Handler

```javascript
// The SDK updates state before consumer handlers run for this event
fb.on('playback:trackChanged', () => {
    const track = fb.state.currentTrack;
    if (track) document.title = `${track.title} - ${track.artist}`;
});
```

::: tip State semantics
`fb.state` is a mutable snapshot, not a Proxy or framework-reactive object. Subscribe through `fb.on()` for change notifications. Use APIs such as `fb.player.getState()` or `fb.player.getPosition()` when an authoritative value is required on demand.
:::

::: warning SMP compatibility layer
`fb.state` tracks only these four playback fields. For the broader cached state used by the SMP compatibility layer, including playlists and configuration, use `window.smp.cache`; see [SMP Compatibility](/reference/smp-compat).
:::
