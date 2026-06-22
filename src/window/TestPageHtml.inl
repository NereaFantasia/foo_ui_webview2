// ============================================
// TestPageHtml.inl - Embedded test page HTML
// This file is included by MainWindow.cpp
// Contains the fallback test page when no frontend resources are available
// ============================================

// 重写基类虚函数，返回 MainWindow 特有的测试页面
std::wstring MainWindow::GetTestPageHtml() const {
    // Simple test page with embedded bridge script and custom titlebar
    return L"<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
        L"<script>"
        L"window.fb2k={_cb:new Map(),_id:0,_evts:{},"
        L"invoke:function(m,p){var s=this;return new Promise(function(ok,no){"
        L"var id=++s._id;s._cb.set(id,{ok:ok,no:no});"
        L"console.log('[fb2k]Send:',m,id);"
        L"try{window.chrome.webview.postMessage({id:id,method:m,params:p||{}});}"
        L"catch(e){no(e);}"
        L"setTimeout(function(){if(s._cb.has(id)){s._cb.delete(id);no(new Error('timeout'));}},30000);"
        L"});},_resp:function(id,e,r){console.log('[fb2k]Resp:',id,e,r);"
        L"var c=this._cb.get(id);if(c){this._cb.delete(id);if(e)c.no(new Error(e));else c.ok(r);}},"
        L"on:function(evt,fn){if(!this._evts[evt])this._evts[evt]=[];this._evts[evt].push(fn);return function(){};}};"
        L"window.chrome.webview.addEventListener('message',function(e){"
        L"var m=e.data;console.log('[fb2k]Recv:',m);"
        L"if(m.type==='response')window.fb2k._resp(m.id,m.error,m.result);"
        L"else if(m.type==='event'&&fb2k._evts[m.event])fb2k._evts[m.event].forEach(function(fn){fn(m.data);});});"
        L"console.log('[fb2k]Ready');"
        L"</script>"
        L"<style>"
        L"*{margin:0;padding:0;box-sizing:border-box}"
        L"html,body{width:100%;height:100%;background:rgba(30,30,30,0.85);overflow:hidden}"
        L"body{display:flex;flex-direction:column;font-family:'Segoe UI Variable','Segoe UI',system-ui,sans-serif}"
        
        // Custom Titlebar styles
        L".titlebar{height:32px;display:flex;align-items:center;background:rgba(0,0,0,0.3);user-select:none;cursor:default}"
        L".titlebar-icon{width:32px;height:32px;display:flex;align-items:center;justify-content:center;padding:8px}"
        L".titlebar-icon img{width:16px;height:16px}"
        L".titlebar-title{flex:1;font-size:12px;color:rgba(255,255,255,0.9);padding-left:4px}"
        L".titlebar-buttons{display:flex}"
        L".titlebar-btn{width:46px;height:32px;border:none;background:transparent;color:rgba(255,255,255,0.9);"
        L"font-family:'Segoe Fluent Icons','Segoe MDL2 Assets';font-size:10px;cursor:pointer;display:flex;align-items:center;justify-content:center}"
        L".titlebar-btn:hover{background:rgba(255,255,255,0.1)}"
        L".titlebar-btn.close:hover{background:#c42b1c;color:white}"
        L".titlebar-btn svg{width:10px;height:10px;fill:currentColor}"
        
        // Content area
        L".content{flex:1;overflow-y:auto;padding:15px;display:flex;flex-direction:column;align-items:center;gap:8px}"
        L"h1{font-size:24px;font-weight:600;color:rgba(255,255,255,0.95)}"
        L"h2{font-size:13px;font-weight:500;color:rgba(255,255,255,0.8);margin-top:6px}"
        L"p{font-size:12px;color:rgba(255,255,255,0.6)}"
        L".st{padding:4px 10px;background:rgba(0,120,212,0.3);border-radius:4px;color:#6cb6ff;font-size:11px}"
        L".ct{display:flex;gap:6px;flex-wrap:wrap;justify-content:center;max-width:800px}"
        L"button{padding:6px 10px;font-size:11px;border:none;border-radius:4px;"
        L"background:rgba(0,120,212,0.8);color:white;cursor:pointer;transition:background 0.15s}"
        L"button:hover{background:rgba(0,140,255,1)}"
        L"button.cfg{background:rgba(100,100,100,0.6)}"
        L"button.cfg:hover{background:rgba(120,120,120,0.8)}"
        L"button.pl{background:rgba(16,124,16,0.8)}"
        L"button.pl:hover{background:rgba(20,160,20,1)}"
        L"button.lib{background:rgba(156,39,176,0.8)}"
        L"button.lib:hover{background:rgba(180,50,200,1)}"
        L"button.win{background:rgba(255,87,34,0.8)}"
        L"button.win:hover{background:rgba(255,110,60,1)}"
        L"button.art{background:rgba(233,30,99,0.8)}"
        L"button.art:hover{background:rgba(255,50,120,1)}"
        L"button.file{background:rgba(0,150,136,0.8)}"
        L"button.file:hover{background:rgba(0,180,160,1)}"
        L"button.dlg{background:rgba(63,81,181,0.8)}"
        L"button.dlg:hover{background:rgba(80,100,220,1)}"
        L"button.clip{background:rgba(121,85,72,0.8)}"
        L"button.clip:hover{background:rgba(150,105,90,1)}"
        L"button.shell{background:rgba(255,152,0,0.8)}"
        L"button.shell:hover{background:rgba(255,170,30,1)}"
        L"button.http{background:rgba(3,169,244,0.8)}"
        L"button.http:hover{background:rgba(30,190,255,1)}"
        L"button.kbd{background:rgba(139,195,74,0.8)}"
        L"button.kbd:hover{background:rgba(160,220,90,1)}"
        L"button.ui{background:rgba(244,67,54,0.8)}"
        L"button.ui:hover{background:rgba(255,90,80,1)}"
        L"button.lyr{background:rgba(156,39,176,0.8)}"
        L"button.lyr:hover{background:rgba(180,60,200,1)}"
        L"button.meta{background:rgba(103,58,183,0.8)}"
        L"button.meta:hover{background:rgba(130,80,210,1)}"
        L"button.aud{background:rgba(0,188,212,0.8)}"
        L"button.aud:hover{background:rgba(30,210,235,1)}"
        L"button.con{background:rgba(96,125,139,0.8)}"
        L"button.con:hover{background:rgba(120,150,165,1)}"
        L"button.dnd{background:rgba(255,193,7,0.8);color:#333}"
        L"button.dnd:hover{background:rgba(255,210,50,1)}"
        L".ip{margin-top:8px;padding:10px;background:rgba(255,255,255,0.08);border-radius:6px;"
        L"width:95%;max-width:800px;border:1px solid rgba(255,255,255,0.1)}"
        L".ip h3{margin-bottom:6px;color:rgba(255,255,255,0.85);font-size:12px}"
        L"#out{font-family:'Cascadia Code',Consolas,monospace;font-size:10px;color:rgba(255,255,255,0.75);"
        L"white-space:pre-wrap;word-break:break-all;max-height:180px;overflow-y:auto}"
        L"#evts{font-family:'Cascadia Code',Consolas,monospace;font-size:10px;color:rgba(100,255,100,0.85);margin-top:8px;"
        L"max-height:100px;overflow-y:auto}"
        L"</style></head><body>"
        
        // Custom Titlebar
        L"<div class=\"titlebar\" id=\"titlebar\">"
        L"<div class=\"titlebar-icon\"><svg viewBox=\"0 0 16 16\" fill=\"rgba(255,255,255,0.9)\"><circle cx=\"8\" cy=\"8\" r=\"6\"/></svg></div>"
        L"<div class=\"titlebar-title\">foobar2000</div>"
        L"<div class=\"titlebar-buttons\">"
        L"<button class=\"titlebar-btn\" id=\"btn-min\" title=\"Minimize\">"
        L"<svg viewBox=\"0 0 10 10\"><path d=\"M0,5 L10,5\" stroke=\"currentColor\" stroke-width=\"1\" fill=\"none\"/></svg></button>"
        L"<button class=\"titlebar-btn\" id=\"btn-max\" title=\"Maximize\">"
        L"<svg viewBox=\"0 0 10 10\"><rect x=\"0.5\" y=\"0.5\" width=\"9\" height=\"9\" stroke=\"currentColor\" stroke-width=\"1\" fill=\"none\"/></svg></button>"
        L"<button class=\"titlebar-btn close\" id=\"btn-close\" title=\"Close\">"
        L"<svg viewBox=\"0 0 10 10\"><path d=\"M0,0 L10,10 M10,0 L0,10\" stroke=\"currentColor\" stroke-width=\"1\" fill=\"none\"/></svg></button>"
        L"</div></div>"
        
        // Main content area
        L"<div class=\"content\">"
        L"<h1>foobar2000 WebView2 UI</h1>"
        L"<p>WebView2 + Mica Effect + Bridge API | Author: NereaFantasia</p>"
        L"<div class=\"st\">Plugin Loaded - 178 APIs Available | Custom Titlebar Demo</div>"
        
        // Playback Controls
        L"<h2>Playback</h2>"
        L"<div class=\"ct\">"
        L"<button onclick=\"doPlay()\">Play</button>"
        L"<button onclick=\"doPause()\">Pause</button>"
        L"<button onclick=\"doStop()\">Stop</button>"
        L"<button onclick=\"doPrev()\">Prev</button>"
        L"<button onclick=\"doNext()\">Next</button>"
        L"<button onclick=\"getState()\">State</button>"
        L"<button onclick=\"getTrack()\">Track Info</button>"
        L"<button onclick=\"getVolume()\">Volume</button>"
        L"<button onclick=\"getOrder()\">Order</button></div>"
        
        // Playlist APIs
        L"<h2>Playlist</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"pl\" onclick=\"plGetAll()\">All Lists</button>"
        L"<button class=\"pl\" onclick=\"plGetActive()\">Active</button>"
        L"<button class=\"pl\" onclick=\"plGetPlaying()\">Playing</button>"
        L"<button class=\"pl\" onclick=\"plGetTracks()\">Tracks(20)</button>"
        L"<button class=\"pl\" onclick=\"plGetCount()\">Count</button>"
        L"<button class=\"pl\" onclick=\"plGetTrackCount()\">Track Count</button>"
        L"<button class=\"pl\" onclick=\"plGetSelected()\">Selected</button>"
        L"<button class=\"pl\" onclick=\"plGetFocus()\">Focus</button>"
        L"<button class=\"pl\" onclick=\"plIsAuto()\">Is Auto?</button>"
        L"<button class=\"pl\" onclick=\"plAutoInfo()\">Auto Info</button>"
        L"<button class=\"pl\" onclick=\"plDuplicate()\">Duplicate</button>"
        L"<button class=\"pl\" onclick=\"plLockInfo()\">Lock Info</button></div>"
        
        // Library APIs
        L"<h2>Library</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"lib\" onclick=\"libEnabled()\">Enabled</button>"
        L"<button class=\"lib\" onclick=\"libStats()\">Stats</button>"
        L"<button class=\"lib\" onclick=\"libAlbums()\">Albums</button>"
        L"<button class=\"lib\" onclick=\"libArtists()\">Artists</button>"
        L"<button class=\"lib\" onclick=\"libGenres()\">Genres</button>"
        L"<button class=\"lib\" onclick=\"libRandom()\">Random 10</button>"
        L"<button class=\"lib\" onclick=\"libSearch()\">Search</button></div>"
        
        // Window APIs
        L"<h2>Window</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"win\" onclick=\"winState()\">State</button>"
        L"<button class=\"win\" onclick=\"winBounds()\">Bounds</button>"
        L"<button class=\"win\" onclick=\"winToggleMax()\">Max Toggle</button>"
        L"<button class=\"win\" onclick=\"winMinimize()\">Minimize</button>"
        L"<button class=\"win\" onclick=\"winTopmost()\">Topmost</button>"
        L"<button class=\"win\" onclick=\"winCenter()\">Center</button>"
        L"<button class=\"win\" onclick=\"winFlash()\">Flash</button>"
        L"<button class=\"win\" onclick=\"winTitlebarInfo()\">Titlebar</button>"
        L"<button class=\"win\" onclick=\"winGetMinSize()\">GetMin</button>"
        L"<button class=\"win\" onclick=\"winSetMinSize()\">SetMin</button>"
        L"<button class=\"win\" onclick=\"winGetMaxSize()\">GetMax</button>"
        L"<button class=\"win\" onclick=\"winSetMaxSize()\">SetMax</button>"
        L"<button class=\"win\" onclick=\"winIsResizable()\">Resizable?</button>"
        L"<button class=\"win\" onclick=\"winToggleResizable()\">Toggle Resize</button></div>"
        
        // Config APIs
        L"<h2>Config</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"cfg\" onclick=\"getVersion()\">Version</button>"
        L"<button class=\"cfg\" onclick=\"getComponents()\">Components</button>"
        L"<button class=\"cfg\" onclick=\"getOutputDevices()\">Output Dev</button>"
        L"<button class=\"cfg\" onclick=\"getOutputConfig()\">Output Cfg</button>"
        L"<button class=\"cfg\" onclick=\"getLibStatus()\">Lib Status</button>"
        L"<button class=\"cfg\" onclick=\"getPrefsPages()\">Prefs</button>"
        L"<button class=\"cfg\" onclick=\"getDspPresets()\">DSP</button></div>"
        
        // Artwork APIs
        L"<h2>Artwork</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"art\" onclick=\"artCurrent()\">Cover</button>"
        L"<button class=\"art\" onclick=\"artTypes()\">Types</button>"
        L"<button class=\"art\" onclick=\"artLyrics()\">Lyrics</button>"
        L"<button class=\"art\" onclick=\"artMeta()\">Metadata</button></div>"
        
        // File System APIs
        L"<h2>File System</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"file\" onclick=\"fileExists()\">Exists</button>"
        L"<button class=\"file\" onclick=\"fileList()\">List Dir</button>"
        L"<button class=\"file\" onclick=\"fileRead()\">Read</button>"
        L"<button class=\"file\" onclick=\"fileWrite()\">Write</button>"
        L"<button class=\"file\" onclick=\"fileMkdir()\">Mkdir</button>"
        L"<button class=\"file\" onclick=\"fileDelete()\">Delete</button></div>"
        
        // Dialog APIs
        L"<h2>Dialogs</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"dlg\" onclick=\"dlgOpen()\">Open File</button>"
        L"<button class=\"dlg\" onclick=\"dlgSave()\">Save File</button>"
        L"<button class=\"dlg\" onclick=\"dlgFolder()\">Open Folder</button>"
        L"<button class=\"dlg\" onclick=\"dlgConfirm()\">Confirm</button></div>"
        
        // Clipboard APIs
        L"<h2>Clipboard</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"clip\" onclick=\"clipRead()\">Read</button>"
        L"<button class=\"clip\" onclick=\"clipWrite()\">Write Text</button>"
        L"<button class=\"clip\" onclick=\"clipWriteHtml()\">Write HTML</button></div>"
        
        // Shell APIs
        L"<h2>Shell</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"shell\" onclick=\"shellExplorer()\">Show in Explorer</button>"
        L"<button class=\"shell\" onclick=\"shellOpen()\">Open External</button>"
        L"<button class=\"shell\" onclick=\"shellExec()\">Execute</button></div>"
        
        // HTTP APIs
        L"<h2>HTTP</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"http\" onclick=\"httpGet()\">GET</button>"
        L"<button class=\"http\" onclick=\"httpPost()\">POST</button>"
        L"<button class=\"http\" onclick=\"httpHead()\">HEAD</button></div>"
        
        // UI APIs
        L"<h2>UI</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"ui\" onclick=\"uiMenu()\">Custom Menu</button>"
        L"<button class=\"ui\" onclick=\"uiToast()\">Toast</button>"
        L"<button class=\"ui\" onclick=\"uiNotify()\">Notification</button>"
        L"<button class=\"ui\" onclick=\"uiContextMenu()\">Context Menu</button></div>"
        
        // Lyrics APIs
        L"<h2>Lyrics</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"lyr\" onclick=\"lyrGet()\">Get Lyrics</button>"
        L"<button class=\"lyr\" onclick=\"lyrExists()\">Exists</button></div>"
        
        // Metadata APIs
        L"<h2>Metadata</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"meta\" onclick=\"metaRead()\">Read</button>"
        L"<button class=\"meta\" onclick=\"metaWrite()\">Write</button></div>"
        
        // Audio APIs
        L"<h2>Audio</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"aud\" onclick=\"audSpectrum()\">Subscribe Spectrum</button>"
        L"<button class=\"aud\" onclick=\"audUnsubSpectrum()\">Unsubscribe</button>"
        L"<button class=\"aud\" onclick=\"audBpm()\">Analyze BPM</button>"
        L"<button class=\"aud\" onclick=\"audVis()\">Visualization Data</button></div>"
        
        // Console APIs
        L"<h2>Console/Log</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"con\" onclick=\"conLog()\">console.log</button>"
        L"<button class=\"con\" onclick=\"conWarn()\">console.warn</button>"
        L"<button class=\"con\" onclick=\"conError()\">console.error</button>"
        L"<button class=\"con\" onclick=\"logWrite()\">log.write</button>"
        L"<button class=\"con\" onclick=\"logRead()\">log.read</button>"
        L"<button class=\"con\" onclick=\"logClear()\">log.clear</button></div>"
        
        // Drag & Drop APIs
        L"<h2>Drag &amp; Drop</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"dnd\" onclick=\"dndRegister()\">Register Zone</button>"
        L"<button class=\"dnd\" onclick=\"dndUnregister()\">Unregister</button>"
        L"<button class=\"dnd\" onclick=\"dndGetZones()\">Get Zones</button></div>"
        
        // System APIs
        L"<h2>System</h2>"
        L"<div class=\"ct\">"
        L"<button class=\"win\" onclick=\"sysTheme()\">Get Theme</button>"
        L"<button class=\"win\" onclick=\"sysDpi()\">Get DPI</button>"
        L"<button class=\"win\" onclick=\"sysLocale()\">Get Locale</button></div>"
        
        // Output area
        L"<div class=\"ip\"><h3>API Response:</h3><div id=\"out\">Click a button to test API...</div>"
        L"<h3 style=\"margin-top:8px\">Events:</h3><div id=\"evts\">Waiting for events...</div></div>"
        L"</div>"
        
        // JavaScript functions
        L"<script>"
        L"function log(m){console.log('[UI]',m);document.getElementById('out').textContent="
        L"typeof m==='object'?JSON.stringify(m,null,2):String(m);}"
        L"function err(e){log('Error: '+e.message);}"
        L"function logEvt(e,d){var el=document.getElementById('evts');"
        L"el.textContent=e+': '+JSON.stringify(d)+'\\n'+el.textContent.slice(0,500);}"
        
        L"function openSettings(){fb2k.invoke('config.openPreferences').then(log).catch(err);}"
        
        // Playback
        L"function doPlay(){fb2k.invoke('playback.play').then(log).catch(err);}"
        L"function doPause(){fb2k.invoke('playback.pause').then(log).catch(err);}"
        L"function doStop(){fb2k.invoke('playback.stop').then(log).catch(err);}"
        L"function doPrev(){fb2k.invoke('playback.previous').then(log).catch(err);}"
        L"function doNext(){fb2k.invoke('playback.next').then(log).catch(err);}"
        L"function getState(){fb2k.invoke('playback.getState').then(log).catch(err);}"
        L"function getTrack(){fb2k.invoke('playback.getCurrentTrack').then(log).catch(err);}"
        L"function getVolume(){fb2k.invoke('playback.getVolume').then(log).catch(err);}"
        L"function getOrder(){fb2k.invoke('playback.getPlaybackOrder').then(log).catch(err);}"
        
        // Playlist
        L"function plGetAll(){fb2k.invoke('playlist.getAll').then(log).catch(err);}"
        L"function plGetActive(){fb2k.invoke('playlist.getActive').then(log).catch(err);}"
        L"function plGetPlaying(){fb2k.invoke('playlist.getPlaying').then(log).catch(err);}"
        L"function plGetTracks(){fb2k.invoke('playlist.getTracks',{start:0,count:20}).then(log).catch(err);}"
        L"function plGetCount(){fb2k.invoke('playlist.getCount').then(log).catch(err);}"
        L"function plGetTrackCount(){fb2k.invoke('playlist.getTrackCount').then(log).catch(err);}"
        L"function plGetSelected(){fb2k.invoke('playlist.getSelectedTracks').then(log).catch(err);}"
        L"function plGetFocus(){fb2k.invoke('playlist.getFocusTrack').then(log).catch(err);}"
        L"function plIsAuto(){fb2k.invoke('playlist.isAutoplaylist').then(log).catch(err);}"
        L"function plAutoInfo(){fb2k.invoke('playlist.getAutoplaylistInfo').then(log).catch(err);}"
        L"function plDuplicate(){fb2k.invoke('playlist.duplicate').then(log).catch(err);}"
        L"function plLockInfo(){fb2k.invoke('playlist.getLockInfo').then(log).catch(err);}"
        
        // Library
        L"function libEnabled(){fb2k.invoke('library.isEnabled').then(log).catch(err);}"
        L"function libStats(){fb2k.invoke('library.getStats').then(log).catch(err);}"
        L"function libAlbums(){fb2k.invoke('library.getAlbums',{limit:20}).then(log).catch(err);}"
        L"function libArtists(){fb2k.invoke('library.getArtists',{limit:20}).then(log).catch(err);}"
        L"function libGenres(){fb2k.invoke('library.getGenres').then(log).catch(err);}"
        L"function libRandom(){fb2k.invoke('library.getRandomTracks',{count:10}).then(log).catch(err);}"
        L"function libSearch(){fb2k.invoke('library.search',{query:'love',limit:10}).then(log).catch(err);}"
        
        // Window
        L"function winState(){fb2k.invoke('window.getState').then(log).catch(err);}"
        L"function winBounds(){fb2k.invoke('window.getBounds').then(log).catch(err);}"
        L"function winToggleMax(){fb2k.invoke('window.toggleMaximize').then(log).catch(err);}"
        L"function winMinimize(){fb2k.invoke('window.minimize').then(log).catch(err);}"
        L"function winTopmost(){fb2k.invoke('window.toggleAlwaysOnTop').then(log).catch(err);}"
        L"function winCenter(){fb2k.invoke('window.center').then(log).catch(err);}"
        L"function winFlash(){fb2k.invoke('window.flash',{count:3}).then(log).catch(err);}"
        L"function winFullscreen(){fb2k.invoke('window.toggleFullscreen').then(log).catch(err);}"
        L"function winGetMinSize(){fb2k.invoke('window.getMinSize').then(log).catch(err);}"
        L"function winSetMinSize(){fb2k.invoke('window.setMinSize',{width:800,height:600}).then(log).catch(err);}"
        L"function winGetMaxSize(){fb2k.invoke('window.getMaxSize').then(log).catch(err);}"
        L"function winSetMaxSize(){fb2k.invoke('window.setMaxSize',{width:1920,height:1080}).then(log).catch(err);}"
        L"function winIsResizable(){fb2k.invoke('window.isResizable').then(log).catch(err);}"
        L"function winToggleResizable(){fb2k.invoke('window.isResizable').then(function(r){fb2k.invoke('window.setResizable',{resizable:!r.resizable}).then(log).catch(err);}).catch(err);}"
        
        // Config
        L"function getVersion(){fb2k.invoke('config.getVersionInfo').then(log).catch(err);}"
        L"function getComponents(){fb2k.invoke('config.getComponents').then(log).catch(err);}"
        L"function getOutputDevices(){fb2k.invoke('config.getOutputDevices').then(log).catch(err);}"
        L"function getOutputConfig(){fb2k.invoke('config.getOutputConfig').then(log).catch(err);}"
        L"function getLibStatus(){fb2k.invoke('config.getLibraryStatus').then(log).catch(err);}"
        L"function getPrefsPages(){fb2k.invoke('config.getPreferencesPages').then(log).catch(err);}"
        L"function getDspPresets(){fb2k.invoke('config.getDspPresets').then(log).catch(err);}"
        
        // Artwork
        L"function artCurrent(){fb2k.invoke('artwork.getCurrent',{type:'front'}).then(function(r){"
        L"if(r.data){log('Cover loaded: '+r.mimeType+' ('+Math.round(r.data.length/1024)+'KB base64)');}"
        L"else{log(r);}}).catch(err);}"
        L"function artTypes(){fb2k.invoke('artwork.getAvailableTypes').then(log).catch(err);}"
        L"function artLyrics(){fb2k.invoke('artwork.getLyrics').then(log).catch(err);}"
        L"function artMeta(){fb2k.invoke('artwork.getMetadata').then(log).catch(err);}"
        
        // File System
        L"function fileExists(){fb2k.invoke('file.exists',{path:'C:/Windows/notepad.exe'}).then(log).catch(err);}"
        L"function fileList(){fb2k.invoke('file.list',{path:'C:/Windows',pattern:'*.exe',limit:10}).then(log).catch(err);}"
        L"function fileRead(){fb2k.invoke('file.read',{path:'C:/Windows/System32/drivers/etc/hosts'}).then(log).catch(err);}"
        L"function fileWrite(){fb2k.invoke('file.write',{path:'C:/temp/fb2k_test.txt',content:'Hello from fb2k!'}).then(log).catch(err);}"
        L"function fileMkdir(){fb2k.invoke('file.mkdir',{path:'C:/temp/fb2k_test_dir'}).then(log).catch(err);}"
        L"function fileDelete(){fb2k.invoke('file.delete',{path:'C:/temp/fb2k_test.txt'}).then(log).catch(err);}"
        
        // Dialog
        L"function dlgOpen(){fb2k.invoke('dialog.openFile',{title:'Select Audio File',filters:[{name:'Audio',extensions:['mp3','flac','wav']}]}).then(log).catch(err);}"
        L"function dlgSave(){fb2k.invoke('dialog.saveFile',{title:'Save Playlist',defaultName:'playlist.m3u8'}).then(log).catch(err);}"
        L"function dlgFolder(){fb2k.invoke('dialog.openFolder',{title:'Select Music Folder'}).then(log).catch(err);}"
        L"function dlgConfirm(){fb2k.invoke('dialog.confirm',{title:'Confirm',message:'Are you sure?',buttons:'yesno'}).then(log).catch(err);}"
        
        // Clipboard
        L"function clipRead(){fb2k.invoke('clipboard.read').then(log).catch(err);}"
        L"function clipWrite(){fb2k.invoke('clipboard.write',{text:'Hello from foobar2000!'}).then(log).catch(err);}"
        L"function clipWriteHtml(){fb2k.invoke('clipboard.writeHTML',{html:'<b>Bold Text</b>'}).then(log).catch(err);}"
        
        // Shell
        L"function shellExplorer(){fb2k.invoke('shell.showInExplorer',{path:'C:/Windows'}).then(log).catch(err);}"
        L"function shellOpen(){fb2k.invoke('shell.openExternal',{url:'https://www.foobar2000.org'}).then(log).catch(err);}"
        L"function shellExec(){fb2k.invoke('shell.exec',{command:'notepad.exe'}).then(log).catch(err);}"
        
        // HTTP
        L"function httpGet(){fb2k.invoke('http.get',{url:'https://httpbin.org/get'}).then(log).catch(err);}"
        L"function httpPost(){fb2k.invoke('http.post',{url:'https://httpbin.org/post',body:JSON.stringify({test:1}),contentType:'application/json'}).then(log).catch(err);}"
        L"function httpHead(){fb2k.invoke('http.head',{url:'https://www.foobar2000.org'}).then(log).catch(err);}"
        
        // UI
        L"function uiMenu(){fb2k.invoke('ui.showCustomMenu',{items:[{id:'play',label:'Play'},{id:'stop',label:'Stop'},{type:'separator'},{id:'quit',label:'Quit'}]}).then(log).catch(err);}"
        L"function uiToast(){fb2k.invoke('ui.showToast',{message:'Hello from WebView!',duration:3000}).then(log).catch(err);}"
        L"function uiNotify(){fb2k.invoke('ui.showNotification',{title:'foobar2000',message:'Track changed!',timeout:5000}).then(log).catch(err);}"
        L"function uiContextMenu(){fb2k.invoke('ui.showContextMenu',{x:100,y:100}).then(log).catch(err);}"
        
        // Lyrics
        L"function lyrGet(){fb2k.invoke('lyrics.get').then(log).catch(err);}"
        L"function lyrExists(){fb2k.invoke('lyrics.exists').then(log).catch(err);}"
        
        // Metadata
        L"function metaRead(){fb2k.invoke('metadata.read').then(log).catch(err);}"
        L"function metaWrite(){fb2k.invoke('metadata.write',{tags:{COMMENT:'Added by WebView'}}).then(log).catch(err);}"
        
        // Audio
        L"function audSpectrum(){fb2k.invoke('audio.subscribeSpectrum',{bands:32,fps:30}).then(log).catch(err);}"
        L"function audUnsubSpectrum(){fb2k.invoke('audio.unsubscribeSpectrum').then(log).catch(err);}"
        L"function audBpm(){fb2k.invoke('audio.analyzeBPM').then(log).catch(err);}"
        L"function audVis(){fb2k.invoke('audio.getVisualizationData').then(log).catch(err);}"
        
        // Console
        L"function conLog(){fb2k.invoke('console.log',{message:'Test log message'}).then(log).catch(err);}"
        L"function conWarn(){fb2k.invoke('console.warn',{message:'Test warning'}).then(log).catch(err);}"
        L"function conError(){fb2k.invoke('console.error',{message:'Test error'}).then(log).catch(err);}"
        L"function logWrite(){fb2k.invoke('log.write',{message:'Custom log entry'}).then(log).catch(err);}"
        L"function logRead(){fb2k.invoke('log.read',{lines:10}).then(log).catch(err);}"
        L"function logClear(){fb2k.invoke('log.clear').then(log).catch(err);}"
        
        // Drag & Drop
        L"function dndRegister(){fb2k.invoke('dnd.registerDropZone',{elementId:'content',accepts:['audio/*']}).then(log).catch(err);}"
        L"function dndUnregister(){fb2k.invoke('dnd.unregisterDropZone',{elementId:'content'}).then(log).catch(err);}"
        L"function dndGetZones(){fb2k.invoke('dnd.getDropZones').then(log).catch(err);}"
        
        // System
        L"function sysTheme(){fb2k.invoke('system.getTheme').then(log).catch(err);}"
        L"function sysDpi(){fb2k.invoke('system.getDPI').then(log).catch(err);}"
        L"function sysLocale(){fb2k.invoke('system.getLocale').then(log).catch(err);}"
        
        // Titlebar Info
        L"function winTitlebarInfo(){fb2k.invoke('window.getTitlebarInfo').then(log).catch(err);}"
        
        // Event listeners
        L"fb2k.on('playback:trackChanged',function(d){logEvt('trackChanged',d);});"
        L"fb2k.on('playback:paused',function(d){logEvt('paused',d);});"
        L"fb2k.on('playback:stopped',function(d){logEvt('stopped',d);});"
        L"fb2k.on('playback:volumeChanged',function(d){logEvt('volumeChanged',d);});"
        L"fb2k.on('playlist:activated',function(d){logEvt('playlist:activated',d);});"
        L"fb2k.on('playlist:itemsAdded',function(d){logEvt('playlist:itemsAdded',d);});"
        
        // Titlebar button handlers
        L"document.getElementById('btn-min').onclick=function(){fb2k.invoke('window.minimize');};"
        L"document.getElementById('btn-max').onclick=function(){fb2k.invoke('window.toggleMaximize');};"
        L"document.getElementById('btn-close').onclick=function(){fb2k.invoke('window.close');};"
        
        // Double-click on titlebar to maximize/restore
        L"document.getElementById('titlebar').ondblclick=function(e){"
        L"if(e.target.closest('.titlebar-buttons'))return;"
        L"fb2k.invoke('window.toggleMaximize');};"
        
        // Drag window by titlebar
        L"document.getElementById('titlebar').onmousedown=function(e){"
        L"if(e.button!==0||e.detail>1)return;"
        L"if(e.target.closest('.titlebar-buttons'))return;"
        L"fb2k.invoke('window.startDrag');};"
        
        // Right-click on titlebar
        L"document.getElementById('titlebar').oncontextmenu=function(e){"
        L"e.preventDefault();"
        L"fb2k.invoke('ui.showContextMenu',{x:e.screenX,y:e.screenY});};"
        
        // Update maximize button icon
        L"fb2k.on('window:stateChanged',function(d){"
        L"var btn=document.getElementById('btn-max');"
        L"if(d.isMaximized){"
        L"btn.innerHTML='<svg viewBox=\"0 0 10 10\"><rect x=\"2\" y=\"0\" width=\"7\" height=\"7\" stroke=\"currentColor\" stroke-width=\"1\" fill=\"none\"/><rect x=\"0\" y=\"2\" width=\"7\" height=\"7\" stroke=\"currentColor\" stroke-width=\"1\" fill=\"rgba(30,30,30,1)\"/></svg>';"
        L"btn.title='Restore';"
        L"}else{"
        L"btn.innerHTML='<svg viewBox=\"0 0 10 10\"><rect x=\"0.5\" y=\"0.5\" width=\"9\" height=\"9\" stroke=\"currentColor\" stroke-width=\"1\" fill=\"none\"/></svg>';"
        L"btn.title='Maximize';"
        L"}});"
        
        // Edge detection for window resize
        L"(function(){"
        L"var EDGE=5,curEdge=null,cursors={'top':'ns-resize','bottom':'ns-resize','right':'ew-resize','left':'ew-resize',"
        L"'topleft':'nwse-resize','topright':'nesw-resize','bottomleft':'nesw-resize','bottomright':'nwse-resize'};"
        L"function getEdge(e){"
        L"var w=window.innerWidth,h=window.innerHeight,x=e.clientX,y=e.clientY;"
        L"var t=y<EDGE,b=y>=h-EDGE,l=x<EDGE,r=x>=w-EDGE;"
        L"if(t&&l)return'topleft';if(t&&r)return'topright';if(b&&l)return'bottomleft';if(b&&r)return'bottomright';"
        L"if(t)return'top';if(b)return'bottom';if(l)return'left';if(r)return'right';return null;}"
        L"document.addEventListener('mousemove',function(e){"
        L"curEdge=getEdge(e);"
        L"if(curEdge){document.body.style.cursor=cursors[curEdge];}else{document.body.style.cursor='';}});"
        L"document.addEventListener('mousedown',function(e){"
        L"if(e.button===0&&curEdge){e.preventDefault();e.stopPropagation();"
        L"fb2k.invoke('window.startResize',{edge:curEdge});}},true);"
        L"document.addEventListener('mouseleave',function(){curEdge=null;document.body.style.cursor='';});"
        L"})();"
        
        L"console.log('[UI]Ready,fb2k:',typeof fb2k);"
        L"</script></body></html>";
}
