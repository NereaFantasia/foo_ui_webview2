# Build your first theme

In this tutorial we build a small player page for foobar2000 and install it as a theme. It shows the cover art, title and artist of the playing track, a seek bar with the elapsed and total time, and previous, play/pause and next buttons. While we work on it, foobar2000 loads the page from a development server, so every change shows up as soon as we save the file.

We use [Vite](https://vite.dev/) for the development server and the build, and the [`foo-webview-sdk`](https://www.npmjs.com/package/foo-webview-sdk) package for ready-made player elements. No framework is needed. The finished project is in the repository as [`examples/starter`](https://github.com/NereaFantasia/foo_ui_webview2/tree/main/examples/starter).

## Before you start

You need:

- foobar2000 2.x with foo_ui_webview2 installed and `Webview2 UI` chosen as the user interface. [Install the component](/how-to/install) covers both.
- Node.js 20.19 or later. Run `node --version` to see which version you have.
- A few tracks in a foobar2000 playlist.

## Create the project

Make an empty folder named `my-theme`. Keep it outside the foobar2000 profile folder; we copy the finished theme into the profile at the end.

In `my-theme`, create `package.json`:

```json
{
  "name": "my-foobar2000-theme",
  "private": true,
  "version": "0.0.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "vite build",
    "preview": "vite preview"
  },
  "dependencies": {
    "foo-webview-sdk": "2.0.0"
  },
  "devDependencies": {
    "vite": "8.3.1"
  },
  "engines": {
    "node": "^20.19.0 || >=22.12.0"
  }
}
```

Open a terminal in `my-theme` and install the packages:

```bash
npm install
```

When npm finishes, `my-theme` also contains a `node_modules` folder and a `package-lock.json` file.

## Write the page

Next we add four files. Start with `vite.config.js`, which fixes the port of the development server:

```js
import { defineConfig } from 'vite';

export default defineConfig({
  server: {
    port: 5173,
    // foobar2000 loads the address typed into its preferences; if the port is
    // taken, fail instead of moving to another one.
    strictPort: true,
  },
});
```

Then `index.html`. Each `fb-*` tag is a player element from the SDK: it asks foobar2000 for its data and updates itself when the track or the position changes.

```html
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>My foobar2000 theme</title>
  </head>
  <body>
    <main class="player">
      <fb-cover-art class="cover"></fb-cover-art>

      <div class="info">
        <fb-track-text class="title" field="title" placeholder="Nothing playing"></fb-track-text>
        <fb-track-text class="artist" field="artist"></fb-track-text>
      </div>

      <div class="time">
        <fb-time-current></fb-time-current>
        <fb-seek-bar class="seek"></fb-seek-bar>
        <fb-time-total></fb-time-total>
      </div>

      <div class="controls">
        <fb-prev-button></fb-prev-button>
        <fb-play-button></fb-play-button>
        <fb-next-button></fb-next-button>
      </div>
    </main>

    <script type="module" src="/src/main.js"></script>
  </body>
</html>
```

Create a folder named `src`. In it, `src/main.js` registers the `fb-*` elements and loads the stylesheet:

```js
import { registerComponents } from 'foo-webview-sdk/components';
import './style.css';

registerComponents();
```

Last, `src/style.css`. The elements have no look of their own. The rules ending in `::part(...)` style the pieces inside them, such as the button in a play button or the fill of the seek bar.

```css
:root {
  --accent: #4cc2ff;
  color-scheme: dark;
  color: #f3f3f3;
  background: #202020;
  font-family: 'Segoe UI', system-ui, sans-serif;
}

body {
  margin: 0;
  min-height: 100vh;
  display: grid;
  place-items: center;
}

.player {
  width: 320px;
  display: grid;
  gap: 16px;
}

.cover {
  width: 320px;
  height: 320px;
  border-radius: 8px;
  overflow: hidden;
  background: #2b2b2b;
}

.info {
  display: grid;
  gap: 4px;
}

.title {
  font-size: 20px;
  font-weight: 600;
}

.artist {
  color: #bdbdbd;
}

.time {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 12px;
  font-variant-numeric: tabular-nums;
}

.seek {
  flex: 1;
}

/* The seek bar draws nothing by itself: give the track a line, the fill a colour and the thumb a dot. */
.seek::part(track) {
  background: linear-gradient(#ffffff33, #ffffff33) center / 100% 4px no-repeat;
}

.seek::part(fill) {
  height: 4px;
  border-radius: 2px;
  background: var(--accent);
}

.seek::part(thumb) {
  width: 12px;
  height: 12px;
  margin-left: -6px;
  border-radius: 50%;
  background: #ffffff;
}

.controls {
  display: flex;
  justify-content: center;
  gap: 12px;
}

.controls ::part(button) {
  width: 48px;
  height: 48px;
  border: none;
  border-radius: 50%;
  background: #2b2b2b;
  color: inherit;
  font-size: 18px;
  cursor: pointer;
}

.controls fb-play-button::part(button) {
  background: var(--accent);
  color: #000000;
}
```

The project now looks like this:

```text
my-theme/
├── node_modules/
├── src/
│   ├── main.js
│   └── style.css
├── index.html
├── package-lock.json
├── package.json
└── vite.config.js
```

## Start the development server

In the terminal, run:

```bash
npm run dev
```

Vite prints the address it serves the page on:

```text
  VITE v8.3.1  ready in 144 ms

  ➜  Local:   http://localhost:5173/
```

Leave it running for the rest of the tutorial. Open `http://localhost:5173` in a web browser: we see a dark grey square where the cover goes, "Nothing playing", `0:00` on both sides of the seek bar, and the three buttons. There is no foobar2000 behind an ordinary browser, so the buttons do nothing there.

## Load the page in foobar2000

In foobar2000, open `File → Preferences → Display → WebView2 UI → Developer`, then:

1. Tick `Use development server (HMR hot reload)`.
2. Type `http://localhost:5173` into the `URL` box.
3. Press `Apply`.

The foobar2000 window reloads and shows our page. Play a track. The cover, title and artist appear, the time counts up, and the play button turns into a pause button. Click somewhere on the seek bar to jump there, and try the previous and next buttons.

If the window shows the theme you had before, foobar2000 could not reach the development server and loaded the theme from the profile instead. Check that `npm run dev` is still running in the terminal, then restart foobar2000.

## Change the colour

Keep foobar2000 open next to the editor. At the top of `src/style.css`, change the accent colour to any colour you like, for example:

```css
:root {
  --accent: #4cc2ff; /* [!code --] */
  --accent: #ff8c42; /* [!code ++] */
  color-scheme: dark;
  color: #f3f3f3;
  background: #202020;
  font-family: 'Segoe UI', system-ui, sans-serif;
}
```

Save the file. The play button and the filled part of the seek bar change colour straight away, without the page reloading.

## Show the playback state

So far the elements have done all the work. Now we call the SDK ourselves: a line that says whether foobar2000 is playing, and a button that plays a random track.

Add this block to `index.html`, inside `<main>` after the `controls` div:

```html
      <div class="extra">
        <span id="status">Not connected to foobar2000</span>
        <button id="random" type="button">Random track</button>
      </div>
```

When we save, the whole page reloads. Vite can swap a stylesheet in place but not the page's HTML. The new line appears under the buttons, still unstyled.

Add these rules to the end of `src/style.css`:

```css
.extra {
  display: flex;
  align-items: center;
  justify-content: space-between;
  font-size: 12px;
  color: #bdbdbd;
}

#random {
  padding: 6px 12px;
  border: 1px solid #ffffff33;
  border-radius: 4px;
  background: transparent;
  color: inherit;
  font: inherit;
  cursor: pointer;
}
```

Then add the marked lines to `src/main.js`. The first one imports `fb`, the SDK object the elements have been using all along:

```js
import fb from 'foo-webview-sdk'; // [!code ++]
import { registerComponents } from 'foo-webview-sdk/components';
import './style.css';

registerComponents();

const status = document.querySelector('#status'); // [!code ++:15]

function showState(state) {
  status.textContent = state === 'playing' ? 'Playing' : state === 'paused' ? 'Paused' : 'Stopped';
}

fb.player.getState().then((res) => {
  if (res.success) showState(res.state);
});

fb.on('playback:stateChanged', ({ state }) => showState(state));

document.querySelector('#random').addEventListener('click', () => {
  fb.player.random();
});
```

`fb.player.getState()` asks foobar2000 once for the current state. `fb.on('playback:stateChanged', ...)` runs our function again each time the state changes. Every SDK call resolves with a result whose `success` field says whether it worked; outside foobar2000 it is `false`, so in an ordinary browser the line keeps saying "Not connected to foobar2000".

Save. The line now says "Playing". Pause in foobar2000 and it says "Paused"; press `Random track` and foobar2000 jumps to a random track.

## Install the theme

The development server only runs while the terminal is open. To keep the theme, we build it and copy it into the foobar2000 profile as a template.

First switch foobar2000 back: on the `Developer` page, untick `Use development server (HMR hot reload)` and press `Apply`. The window shows the theme you had before.

In the terminal, press `Ctrl+C` to stop the development server, then build:

```bash
npm run build
```

Vite writes the finished theme to the `dist` folder of the project: an `index.html` and an `assets` folder.

In foobar2000, go to the `WebView2 UI` page itself (`File → Preferences → Display → WebView2 UI`):

1. Press `Manage...` and choose `Create...`. Enter `my-theme` as the name and confirm. foobar2000 creates a template folder with a placeholder `index.html` and selects `my-theme` in the `Template` box.
2. Press `Open...`. The template folder opens in File Explorer.
3. Copy everything inside the project's `dist` folder into the template folder, replacing the `index.html` there.
4. Press `Apply`.

The window reloads and shows our theme, this time from the template folder. It stays after foobar2000 restarts, and it no longer needs the development server.

## What's next

We now have a working theme and a way to edit it live. From here:

- [Namespaces](/sdk/namespaces) lists everything the SDK can do, from playlists and the media library to windows and the tray icon.
- [Components](/components/) lists the other `fb-*` elements, such as the playlist view and the volume control.
- [Put a page in a panel](/how-to/panels) shows how to put a page inside Default UI or Columns UI as a panel.
- [Results and failures](/sdk/overview#results-and-failures) explains the result objects SDK calls resolve with.
- [Preferences](/reference/preferences) describes the other settings we passed on the way.

To start again from the finished code, copy [`examples/starter`](https://github.com/NereaFantasia/foo_ui_webview2/tree/main/examples/starter) from the repository.
