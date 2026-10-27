# Write a theme without a build step

A theme can be authored as a single static `index.html` file with inline scripts, leveraging the native `window.fb2k` Bridge object that the component injects into every loaded page. No Node.js tooling or build process is required.

## Example HTML document {#write-the-page}

Save the following markup as `index.html` within a template directory ([Install a theme](./install-theme.md) describes creating and activating templates):

```html
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>My foobar2000 UI</title>
</head>
<body>
    <h1 id="track">Waiting for playback...</h1>
    <button id="play">▶ Play</button>
    <button id="prev">⏮ Previous</button>
    <button id="next">⏭ Next</button>

    <script>
    document.getElementById('play').onclick = () => fb2k.invoke('playback.playOrPause');
    document.getElementById('prev').onclick = () => fb2k.invoke('playback.previous');
    document.getElementById('next').onclick = () => fb2k.invoke('playback.next');

    fb2k.on('playback:trackChanged', (data) => {
        document.getElementById('track').textContent = data.artist + ' - ' + data.title;
    });
    </script>
</body>
</html>
```

Press `Apply` on the preferences page, or reload the window, to see it.

## Call the host from the page

`fb2k.invoke(method, params)` calls a host method and resolves with its result; `fb2k.on(event, handler)` subscribes to an event and returns a function that unsubscribes.

```javascript
// Every call is asynchronous
await fb2k.invoke('playback.play');

// A result carries success; check it before reading the fields
const current = await fb2k.invoke('playback.getCurrentTrack');
if (current.success === false) throw new Error(current.error);
if (current.track) console.log(current.track.title, current.track.artist);

// Parameters go in an object (volume is 0-100)
await fb2k.invoke('playback.setVolume', { volume: 80 });

// Subscribe, and unsubscribe later
const unsubscribe = fb2k.on('playback:trackChanged', (data) => {
    console.log('Now playing:', data.title);
});
unsubscribe();
```

Method names use dot notation (`playback.play`) and event names use colon notation (`playback:trackChanged`). The [low-level API reference](/api/overview) indexes every registered method and event, and [Bridge protocol](/reference/bridge) describes the wire envelope format.

## Use SDK pre-built bundles in plain HTML {#use-the-sdk-without-a-bundler}

To use typed `fb.*` wrappers and `fb-*` Web Components in a plain HTML project, copy the SDK's global bundle files into your template folder and include them before custom scripts:

```html
<script src="./sdk/bridge.global.js"></script>
<script src="./sdk/components.global.js"></script>
<fb-play-button></fb-play-button>
<script>
    fb.on('playback:trackChanged', (track) => {
        document.title = `${track.title} - ${track.artist}`;
    });
</script>
```

[Load the SDK in a theme](./load-sdk.md#without-a-bundler) says where the files come from.
