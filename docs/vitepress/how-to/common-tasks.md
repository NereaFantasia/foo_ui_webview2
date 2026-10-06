# Common tasks

Each section below solves one frequent task with the native bridge `fb2k`, as a complete example to adapt. The same calls exist as `fb.*` methods in the [SDK](/sdk/namespaces).

## Scenario 1: build a player UI

```javascript
// ========== Helper: static local file? ==========
function isStaticLocalFile(path) {
    if (!path) return false;
    if (path.startsWith('http://') || path.startsWith('https://')) return false;
    if (path.startsWith('cdda://')) return false;
    return true;
}

// ========== Initialize player state ==========
async function initPlayer() {
    const state = await fb2k.invoke('playback.getState');
    if (state.success === false) throw new Error(state.error);
    updatePlayButton(state.state === 'playing');
    
    const volumeInfo = await fb2k.invoke('playback.getVolume');
    if (volumeInfo.success === false) throw new Error(volumeInfo.error);
    updateVolumeSlider(volumeInfo.volume);
    
    if (state.state === 'playing' || state.state === 'paused') {
        const current = await fb2k.invoke('playback.getCurrentTrack');
        if (current.success === false) throw new Error(current.error);
        const track = current.track;
        if (!track) return;
        updateTrackInfo(track);
        
        if (isStaticLocalFile(track.absolutePath)) {
            const artwork = await fb2k.invoke('artwork.getCurrent');
            if (artwork.success === false) throw new Error(artwork.error);
            if (artwork.available) {
                document.getElementById('cover').src = artwork.dataUrl;
            }
        }
    }
}

// ========== Transport buttons ==========
document.getElementById('btn-play').onclick = () => fb2k.invoke('playback.playOrPause');
document.getElementById('btn-prev').onclick = () => fb2k.invoke('playback.previous');
document.getElementById('btn-next').onclick = () => fb2k.invoke('playback.next');

// ========== Seek bar ==========
progressBar.onclick = async (e) => {
    const rect = progressBar.getBoundingClientRect();
    const percent = (e.clientX - rect.left) / rect.width;
    const current = await fb2k.invoke('playback.getCurrentTrack');
    if (current.success === false) throw new Error(current.error);
    if (current.track) await fb2k.invoke('playback.setPosition', { position: percent * current.track.duration });
};

// ========== Volume ==========
volumeSlider.oninput = async (e) => {
    await fb2k.invoke('playback.setVolume', { volume: parseInt(e.target.value) });
};

// ========== Events ==========
let currentDuration = 0; // playback:time carries only the position

fb2k.on('playback:trackChanged', async (track) => {
    currentDuration = track.duration;
    updateTrackInfo(track);
    const artwork = await fb2k.invoke('artwork.getCurrent');
    if (artwork.success === false) throw new Error(artwork.error);
    if (artwork.available) document.getElementById('cover').src = artwork.dataUrl;
});

fb2k.on('playback:stateChanged', (data) => updatePlayButton(data.state === 'playing'));

fb2k.on('playback:time', (data) => {
    if (currentDuration <= 0) return; // duration is 0 when the length is unknown
    const percent = (data.position / currentDuration) * 100;
    document.getElementById('progress-fill').style.width = percent + '%';
});

fb2k.on('playback:volumeChanged', (data) => {
    volumeSlider.value = data.volume;
});
```

## Scenario 2: playlist management

```javascript
// ========== Load playlists ==========
async function loadPlaylists() {
    const res = await fb2k.invoke('playlist.getAll');
    if (res.success === false) throw new Error(res.error);
    const container = document.getElementById('playlists');
    container.innerHTML = '';
    
    res.playlists.forEach((pl, index) => {
        const item = document.createElement('div');
        item.className = 'playlist-item' + (pl.isActive ? ' active' : '');
        item.innerHTML = `
            ${pl.name}
        ${pl.trackCount} tracks
        `;
        item.onclick = () => switchPlaylist(index);
        container.appendChild(item);
    });
}

// ========== Load tracks (paged) ==========
async function loadTracks(playlist, page = 0) {
    const pageSize = 50;
    const result = await fb2k.invoke('playlist.getTracks', {
        playlist, start: page * pageSize, count: pageSize
    });
    if (result.success === false) throw new Error(result.error);
    
    const container = document.getElementById('track-list');
    if (page === 0) container.innerHTML = '';
    
    result.tracks.forEach(track => {
        const item = document.createElement('div');
        item.className = 'track-item';
        item.innerHTML = `
            ${track.index + 1}
            ${track.title}
            ${track.artist}
        `;
        item.ondblclick = () => fb2k.invoke('playlist.playTrack', { playlist, index: track.index });
        container.appendChild(item);
    });
}

// ========== Playlist change events ==========
fb2k.on('playlist:itemsAdded', () => loadTracks(currentPlaylistIndex));
fb2k.on('playlist:itemsRemoved', () => loadTracks(currentPlaylistIndex));
fb2k.on('playlist:created', () => loadPlaylists());
fb2k.on('playlist:removed', () => loadPlaylists());
```

## Scenario 3: library search and artwork

```javascript
// ========== Search the media library ==========
async function searchLibrary(query) {
    const result = await fb2k.invoke('library.search', {
        query, offset: 0, limit: 50
    });
    if (result.success === false) throw new Error(result.error);
    
    for (const track of result.tracks) {
        let coverSrc = 'default-cover.png';
        if (isStaticLocalFile(track.absolutePath)) {
            const artwork = await fb2k.invoke('artwork.getForTrack', {
                path: track.absolutePath, type: 'front'
            });
            if (artwork.success === false) throw new Error(artwork.error);
            if (artwork.available) coverSrc = artwork.dataUrl;
        }
        // Render search results...
    }
}

// ========== Query syntax examples ==========
searchLibrary('love');                                    // simple search
searchLibrary('artist HAS Beatles');                      // field search
searchLibrary('artist HAS Beatles AND year GREATER 1968');// combined search

// ========== Album list with covers ==========
async function loadAlbums() {
    const result = await fb2k.invoke('library.getAlbums', {
        sort: 'year', limit: 30, includeTracks: false
    });
    if (result.success === false) throw new Error(result.error);
    
    for (const album of result.albums) {
        const artwork = await fb2k.invoke('artwork.getForTrack', {
            path: album.firstTrackAbsolutePath, type: 'front'
        });
        // Render album cards...
    }
}
```

## Scenario 4: window and appearance

```javascript
// ========== Window chrome ==========
document.getElementById('btn-minimize').onclick = () => fb2k.invoke('window.minimize');
document.getElementById('btn-maximize').onclick = () => fb2k.invoke('window.maximize');
document.getElementById('btn-close').onclick = () => fb2k.invoke('window.close');

// ========== Mica / acrylic ==========
async function applyMicaEffect() {
    await fb2k.invoke('window.setMica', { enabled: true });
    // Acrylic is available on Windows 10:
    // await fb2k.invoke('window.setAcrylic', { enabled: true });
}

// ========== Rounded corners (Windows 11) ==========
await fb2k.invoke('window.setCornerPreference', { mode: 'round' });

// ========== Window state events ==========
fb2k.on('window:stateChanged', (state) => {
    updateMaximizeButton(state.isMaximized);
});

fb2k.on('panel:focus', () => {
    document.body.classList.remove('window-inactive');
});
fb2k.on('panel:blur', () => {
    document.body.classList.add('window-inactive');
});

// ========== Custom title-bar drag ==========
document.getElementById('titlebar').setAttribute('data-webview-drag', 'true');
document.querySelectorAll('.titlebar-button').forEach(btn => {
    btn.setAttribute('data-webview-drag', 'false');
});
```

## Scenario 5: global hotkeys

```javascript
async function registerHotkeys() {
    // C++ contract: key + action (+ optional global). Registration returns a numeric id.
    await fb2k.invoke('keyboard.registerHotkey', {
        key: 'Ctrl+Shift+Space',
        action: 'play-pause',
        global: true
    });
    await fb2k.invoke('keyboard.registerHotkey', {
        key: 'Ctrl+Shift+Right',
        action: 'next-track',
        global: true
    });
}

fb2k.on('keyboard:hotkey', async (data) => {
    // Event payload: { id: number, key, action }
    switch (data.action) {
        case 'play-pause': await fb2k.invoke('playback.playOrPause'); break;
        case 'next-track': await fb2k.invoke('playback.next'); break;
    }
});

registerHotkeys();
```

## Scenario 6: config and file storage

```javascript
const CONFIG_PREFIX = 'myapp.';

async function saveConfig(key, value) {
    await fb2k.invoke('config.set', {
        key: CONFIG_PREFIX + key, value: JSON.stringify(value)
    });
}

async function loadConfig(key, defaultValue = null) {
    const result = await fb2k.invoke('config.get', { key: CONFIG_PREFIX + key });
    if (result.success === false) throw new Error(result.error);
    return typeof result.value === 'string' ? JSON.parse(result.value) : defaultValue;
}

// ========== File read/write ==========
async function exportPlaylistToFile() {
    const result = await fb2k.invoke('dialog.saveFile', {
        title: 'Export playlist',
        filters: [{ name: 'JSON files', extensions: ['json'] }],
        defaultName: 'playlist-backup.json'
    });
    if (result.success === false) throw new Error(result.error);
    // dialog.saveFile returns { canceled, filePath }
    if (!result.canceled) {
        const res = await fb2k.invoke('playlist.getTracks', { count: 10000 });
        if (res.success === false) throw new Error(res.error);
        const { tracks } = res;
        await fb2k.invoke('file.write', {
            path: result.filePath,
            content: JSON.stringify(tracks, null, 2)
        });
    }
}
```

## Scenario 7: path handling and artwork best practices

```javascript
// ❌ Wrong: path may be a non-filesystem URI
const tracks = await fb2k.invoke('playlist.getTracks', { count: 1 });
if (tracks.success === false) throw new Error(tracks.error);
const artwork = await fb2k.invoke('artwork.getForTrack', {
    path: tracks.tracks[0].path  // may be file-relative://...
});

// ✅ Correct: use absolutePath
const t = tracks.tracks[0];
if (getFileType(t.absolutePath) !== 'stream') {
    const artwork = await fb2k.invoke('artwork.getForTrack', {
        path: t.absolutePath
    });
}

// ✅ Better: getByPlaylistItem needs no path and handles resolution
const artwork = await fb2k.invoke('artwork.getByPlaylistItem', {
    playlist: 0, index: 0, type: 'front'
});

// ========== Batch cover loading ==========
async function loadCoversForTracks(tracks) {
    const batchSize = 5;
    for (let i = 0; i < tracks.length; i += batchSize) {
        const batch = tracks.slice(i, i + batchSize);
        await Promise.all(batch.map(track =>
            fb2k.invoke('artwork.getForTrack', { path: track.absolutePath })
        ));
    }
}
```
