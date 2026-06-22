# 常用场景 

本页展示常见开发场景的完整代码示例。

## 场景 1：构建播放器界面 

```javascript
// ========== 工具函数：判断静态文件 ==========
function isStaticLocalFile(path) {
    if (!path) return false;
    if (path.startsWith('http://') || path.startsWith('https://')) return false;
    if (path.startsWith('cdda://')) return false;
    return true;
}

// ========== 初始化播放器状态 ==========
async function initPlayer() {
    const state = await fb2k.invoke('playback.getState');
    updatePlayButton(state.state === 'playing');
    
    const volumeInfo = await fb2k.invoke('playback.getVolume');
    updateVolumeSlider(volumeInfo.volume);
    
    if (state.state === 'playing' || state.state === 'paused') {
        const track = await fb2k.invoke('playback.getCurrentTrack');
        updateTrackInfo(track);
        
        if (isStaticLocalFile(track.absolutePath)) {
            const artwork = await fb2k.invoke('artwork.getCurrent');
            if (artwork.available) {
                document.getElementById('cover').src = artwork.dataUrl;
            }
        }
    }
}

// ========== 播放控制按钮 ==========
document.getElementById('btn-play').onclick = () => fb2k.invoke('playback.playOrPause');
document.getElementById('btn-prev').onclick = () => fb2k.invoke('playback.previous');
document.getElementById('btn-next').onclick = () => fb2k.invoke('playback.next');

// ========== 进度条控制 ==========
progressBar.onclick = async (e) => {
    const rect = progressBar.getBoundingClientRect();
    const percent = (e.clientX - rect.left) / rect.width;
    const track = await fb2k.invoke('playback.getCurrentTrack');
    await fb2k.invoke('playback.setPosition', { position: percent * track.duration });
};

// ========== 音量控制 ==========
volumeSlider.oninput = async (e) => {
    await fb2k.invoke('playback.setVolume', { volume: parseInt(e.target.value) });
};

// ========== 事件监听 ==========
fb2k.on('playback:trackChanged', async (track) => {
    updateTrackInfo(track);
    const artwork = await fb2k.invoke('artwork.getCurrent');
    if (artwork.available) document.getElementById('cover').src = artwork.dataUrl;
});

fb2k.on('playback:stateChanged', (data) => updatePlayButton(data.state === 'playing'));

fb2k.on('playback:time', (data) => {
    const percent = (data.position / currentDuration) * 100;
    document.getElementById('progress-fill').style.width = percent + '%';
});

fb2k.on('playback:volumeChanged', (data) => {
    volumeSlider.value = data.volume;
});
```

## 场景 2：播放列表管理 

```javascript
// ========== 加载播放列表 ==========
async function loadPlaylists() {
    const playlists = await fb2k.invoke('playlist.getAll');
    const container = document.getElementById('playlists');
    container.innerHTML = '';
    
    playlists.forEach((pl, index) => {
        const item = document.createElement('div');
        item.className = 'playlist-item' + (pl.isActive ? ' active' : '');
        item.innerHTML = `
            ${pl.name}
        ${pl.trackCount} 首
        `;
        item.onclick = () => switchPlaylist(index);
        container.appendChild(item);
    });
}

// ========== 加载曲目列表（分页） ==========
async function loadTracks(playlist, page = 0) {
    const pageSize = 50;
    const result = await fb2k.invoke('playlist.getTracks', {
        playlist, start: page * pageSize, count: pageSize
    });
    
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
        item.ondblclick = () => fb2k.invoke('playlist.playTrack', { playlist, track: track.index });
        container.appendChild(item);
    });
}

// ========== 监听播放列表变化 ==========
fb2k.on('playlist:itemsAdded', () => loadTracks(currentPlaylistIndex));
fb2k.on('playlist:itemsRemoved', () => loadTracks(currentPlaylistIndex));
fb2k.on('playlist:created', () => loadPlaylists());
fb2k.on('playlist:removed', () => loadPlaylists());
```

## 场景 3：媒体库搜索与封面获取 

```javascript
// ========== 搜索媒体库 ==========
async function searchLibrary(query) {
    const result = await fb2k.invoke('library.search', {
        query, offset: 0, limit: 50
    });
    
    for (const track of result.tracks) {
        let coverSrc = 'default-cover.png';
        if (isStaticLocalFile(track.absolutePath)) {
            const artwork = await fb2k.invoke('artwork.getForTrack', {
                path: track.absolutePath, type: 'front'
            });
            if (artwork.available) coverSrc = artwork.dataUrl;
        }
        // 渲染搜索结果...
    }
}

// ========== 搜索语法示例 ==========
searchLibrary('love');                                    // 简单搜索
searchLibrary('artist HAS Beatles');                      // 字段搜索
searchLibrary('artist HAS Beatles AND year GREATER 1968');// 组合搜索

// ========== 获取专辑列表（带封面） ==========
async function loadAlbums() {
    const result = await fb2k.invoke('library.getAlbums', {
        sort: 'year', limit: 30, includeTracks: false
    });
    
    for (const album of result.albums) {
        const artwork = await fb2k.invoke('artwork.getForTrack', {
            path: album.firstTrackAbsolutePath, type: 'front'
        });
        // 渲染专辑卡片...
    }
}
```

## 场景 4：窗口与外观控制 

```javascript
// ========== 窗口控制 ==========
document.getElementById('btn-minimize').onclick = () => fb2k.invoke('window.minimize');
document.getElementById('btn-maximize').onclick = () => fb2k.invoke('window.maximize');
document.getElementById('btn-close').onclick = () => fb2k.invoke('window.close');

// ========== Mica/毛玻璃效果 ==========
async function applyMicaEffect() {
    await fb2k.invoke('window.setMica', { enabled: true });
    // Windows 10 可用 Acrylic:
    // await fb2k.invoke('window.setAcrylic', { enabled: true });
}

// ========== 圆角窗口 (Windows 11) ==========
await fb2k.invoke('window.setCornerPreference', { preference: 'round' });

// ========== 监听窗口状态 ==========
fb2k.on('window:stateChanged', (state) => {
    updateMaximizeButton(state.isMaximized);
});

fb2k.on('panel:focus', () => {
    document.body.classList.remove('window-inactive');
});
fb2k.on('panel:blur', () => {
    document.body.classList.add('window-inactive');
});

// ========== 自定义标题栏拖拽 ==========
document.getElementById('titlebar').setAttribute('data-webview-drag', 'true');
document.querySelectorAll('.titlebar-button').forEach(btn => {
    btn.setAttribute('data-webview-drag', 'false');
});
```

## 场景 5：全局快捷键 

```javascript
async function registerHotkeys() {
    await fb2k.invoke('keyboard.registerHotkey', {
        id: 'play-pause', key: 'Space', modifiers: ['ctrl', 'shift']
    });
    await fb2k.invoke('keyboard.registerHotkey', {
        id: 'next-track', key: 'Right', modifiers: ['ctrl', 'shift']
    });
}

fb2k.on('keyboard:hotkey', async (data) => {
    switch (data.id) {
        case 'play-pause': await fb2k.invoke('playback.playOrPause'); break;
        case 'next-track': await fb2k.invoke('playback.next'); break;
    }
});

registerHotkeys();
```

## 场景 6：配置与文件存储 

```javascript
const CONFIG_PREFIX = 'myapp.';

async function saveConfig(key, value) {
    await fb2k.invoke('config.set', {
        key: CONFIG_PREFIX + key, value: JSON.stringify(value)
    });
}

async function loadConfig(key, defaultValue = null) {
    const result = await fb2k.invoke('config.get', { key: CONFIG_PREFIX + key });
    return result.value ? JSON.parse(result.value) : defaultValue;
}

// ========== 文件读写 ==========
async function exportPlaylistToFile() {
    const result = await fb2k.invoke('dialog.saveFile', {
        title: '导出播放列表',
        filters: [{ name: 'JSON 文件', spec: '*.json' }],
        defaultName: 'playlist-backup.json'
    });
    if (!result.cancelled) {
        const { tracks } = await fb2k.invoke('playlist.getTracks', { count: 10000 });
        await fb2k.invoke('file.write', {
            path: result.path,
            content: JSON.stringify(tracks, null, 2)
        });
    }
}
```

## 场景 7：路径处理与封面获取最佳实践 

```javascript
// ❌ 错误：使用 path 可能失败
const tracks = await fb2k.invoke('playlist.getTracks', { count: 1 });
const artwork = await fb2k.invoke('artwork.getForTrack', {
    path: tracks.tracks[0].path  // 可能是 file-relative://...
});

// ✅ 正确：使用 absolutePath
const t = tracks.tracks[0];
if (getFileType(t.absolutePath) !== 'stream') {
    const artwork = await fb2k.invoke('artwork.getForTrack', {
        path: t.absolutePath
    });
}

// ✅ 更好：使用 getByPlaylistItem（不需要路径，自动处理）
const artwork = await fb2k.invoke('artwork.getByPlaylistItem', {
    playlist: 0, index: 0, type: 'front'
});

// ========== 批量获取封面 ==========
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
