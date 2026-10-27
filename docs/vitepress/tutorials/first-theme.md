# Build your first theme

This tutorial walks through building a lightweight player theme for foobar2000 and installing it as a standalone theme. The interface displays the current track's album art, title, artist, a progress bar with elapsed and total duration, and transport buttons (previous, play/pause, next). During development, foobar2000 loads the page directly from a local development server with hot module replacement (HMR), reflecting changes as soon as files are saved.

The project uses [Vite](https://vite.dev/) for development and production bundling, along with unstyled Web Components from [`foo-webview-sdk`](https://www.npmjs.com/package/foo-webview-sdk) without requiring a frontend framework. Complete source code is available in the repository at [`examples/starter`](https://github.com/NereaFantasia/foo_ui_webview2/tree/main/examples/starter).

## Prerequisites {#before-you-start}

- foobar2000 v2.0 or later with foo_ui_webview2 installed and `Webview2 UI` selected as the user interface (see [Install the component](/how-to/install))
- Node.js 20.19 or later (run `node --version` to verify)
- Playable tracks in a foobar2000 playlist

## Create the project

Create an empty directory named `my-theme` outside the foobar2000 profile folder (the compiled theme is copied into the profile once complete).

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

Open a terminal in `my-theme` and install dependencies:

```bash
npm install
```

When installation completes, `my-theme` contains the `node_modules` directory and `package-lock.json`.

## Write the page

The project consists of four core source files. First, create `vite.config.js` to lock the development server port:

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

Next, create the page entry point `index.html`. All `fb-*` elements are standardized Web Components provided by the SDK that automatically subscribe to host data and update on state and progress changes:

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

Create a `src` directory containing `src/main.js` to register the components and import styles:

```js
import { registerComponents } from 'foo-webview-sdk/components';
import './style.css';

registerComponents();
```

Create `src/style.css`. SDK components follow the zero-style convention; style internal sub-elements using `::part(...)` selectors (such as button surfaces and seek fill tracks):

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

The project directory structure is now organized as follows:

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

In the terminal, run the development server:

```bash
npm run dev
```

Vite prints the local address once ready:

```text
  VITE v8.3.1  ready in 144 ms

  ➜  Local:   http://localhost:5173/
```

Keep the development server running. Open `http://localhost:5173` in an external browser: a dark gray placeholder marks the cover art, alongside "Nothing playing", `0:00` timestamps on the seek bar, and three control buttons. Because external browsers lack the native foobar2000 bridge, button clicks do not alter playback.

## Load the page in foobar2000

In foobar2000, open `File → Preferences → Display → WebView2 UI → Developer`:

1. Tick `Use development server (HMR hot reload)`
2. Type `http://localhost:5173` into the `URL` box
3. Click `Apply`

The foobar2000 window reloads and displays the page. Play any track: the album art, track title, and artist populate immediately, playback progress advances, and the play button toggles to pause. Clicking along the seek bar navigates the track, and the transport controls take effect.

If the window shows the theme you had before, foobar2000 could not reach the development server and loaded the theme from the profile instead. Check that `npm run dev` is still running in the terminal, then restart foobar2000.

## Experience hot reload (change accent colour) {#change-the-colour}

Open `src/style.css` in your editor, and change the accent colour variable at the top (for example, to orange):

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

Save the file. The play button and the filled part of the seek bar change colour immediately without reloading the page.

## Show the playback state

Up to this point, Web Components have managed state and communication autonomously. Next, call the SDK directly to implement custom logic: displaying live playback state and triggering a random track.

Add this block to `index.html`, inside `<main>` after the `.controls` div:

```html
      <div class="extra">
        <span id="status">Not connected to foobar2000</span>
        <button id="random" type="button">Random track</button>
      </div>
```

Saving triggers a page reload as HTML structure changes require a full refresh. The unstyled text and button appear below the controls.

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

Add the marked lines to `src/main.js` to import `fb` and register the event listeners:

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

`fb.player.getState()` retrieves the initial transport state. `fb.on('playback:stateChanged', ...)` listens for continuous state transitions. SDK calls resolve with a standard result object where `success` indicates host availability; outside foobar2000 it is `false`, keeping the fallback text visible in standard browsers.

Save the file. The status line updates to "Playing". Pausing in foobar2000 toggles it to "Paused"; clicking `Random track` commands the host to jump to a random item.

## Install the theme into the profile {#install-the-theme}

The development server is intended for live debugging. To use the theme permanently, build the production bundle and copy it into the foobar2000 profile directory as a template.

In foobar2000, open the `Developer` page, untick `Use development server (HMR hot reload)`, and click `Apply` to return to local template mode.

Stop the development server in your terminal (`Ctrl+C`), then run the build:

```bash
npm run build
```

Vite outputs production assets into the project's `dist` folder (containing `index.html` and the `assets` directory).

In foobar2000, navigate to `File → Preferences → Display → WebView2 UI`:

1. Click `Manage...` to open the template menu, select `New...`, enter `my-theme`, and confirm. foobar2000 creates the template directory with a placeholder `index.html` and selects `my-theme`
2. Click `Open...` to locate the template directory in File Explorer
3. Copy all files and folders from the project's `dist` directory into the template directory, replacing the placeholder `index.html`
4. Click `Apply`

The window reloads and renders the theme directly from the profile template directory. The theme persists across foobar2000 restarts without requiring an active development server.

## Next steps {#what-s-next}

Now that you have a functioning custom theme, explore these topics:

- [Namespaces](/sdk/namespaces): Browse complete SDK capabilities across playlists, media library, window management, and tray integration
- [Components](/components/): Discover ready-to-use Web Components such as playlist views and volume sliders
- [Put a page in a panel](/how-to/panels): Embed web themes into Default UI or Columns UI layout panels
- [Results and failures](/sdk/overview#results-and-failures): Understand error codes and result envelope structures
- [Preferences](/reference/preferences): Explore theme appearance, backdrop effects, and developer settings

The complete starter project is available in the repository at [`examples/starter`](https://github.com/NereaFantasia/foo_ui_webview2/tree/main/examples/starter).
