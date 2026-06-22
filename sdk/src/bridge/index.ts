/**
 * `foo-webview-sdk/bridge` — runtime bridge entry.
 *
 * Re-exports the singleton {@link bridge} handle, the reactive
 * {@link state} mirror, every per-namespace proxy, and the aggregate
 * {@link fb} object that combines them into a single ergonomic
 * surface (`fb.playback.play()`, `fb.on('playback:stateChanged', ...)`,
 * `fb.state.isPlaying`, ...).
 */

import { bridge } from './Bridge.js';
import { state } from './state.js';

import { artwork } from './namespaces/artwork.js';
import { audio } from './namespaces/audio.js';
import { clipboard } from './namespaces/clipboard.js';
import { config } from './namespaces/config.js';
import { consoleApi } from './namespaces/consoleApi.js';
import { cursor } from './namespaces/cursor.js';
import { dialog } from './namespaces/dialog.js';
import { discovery } from './namespaces/discovery.js';
import { dnd } from './namespaces/dnd.js';
import { dsp } from './namespaces/dsp.js';
import { event } from './namespaces/event.js';
import { file } from './namespaces/file.js';
import { http } from './namespaces/http.js';
import { jitQueue } from './namespaces/jitQueue.js';
import { keyboard } from './namespaces/keyboard.js';
import { library } from './namespaces/library.js';
import { log } from './namespaces/log.js';
import { lyrics } from './namespaces/lyrics.js';
import { menu } from './namespaces/menu.js';
import { metadata } from './namespaces/metadata.js';
import { misc } from './namespaces/misc.js';
import { notification } from './namespaces/notification.js';
import { output } from './namespaces/output.js';
import { panel } from './namespaces/panel.js';
import { player } from './namespaces/player.js';
import { playcount } from './namespaces/playcount.js';
import { playlist } from './namespaces/playlist.js';
import { port } from './namespaces/port.js';
import { queue } from './namespaces/queue.js';
import { rating } from './namespaces/rating.js';
import { replaygain } from './namespaces/replaygain.js';
import { selection } from './namespaces/selection.js';
import { sharedState } from './namespaces/sharedState.js';
import { shell } from './namespaces/shell.js';
import { system } from './namespaces/system.js';
import { taskbar } from './namespaces/taskbar.js';
import { titleformat } from './namespaces/titleformat.js';
import { tray } from './namespaces/tray.js';
import { ui } from './namespaces/ui.js';
import { utils } from './namespaces/utils.js';

export { bridge } from './Bridge.js';
export { state } from './state.js';

// Runtime constant dictionary that otherwise only lives on the type
// barrel; re-exported here so the package-root runtime target carries it
// as a real value (consumers do `import { REPLAYGAIN_SOURCE_MODE }`).
export { REPLAYGAIN_SOURCE_MODE } from '../types/responses.js';

export { artwork } from './namespaces/artwork.js';
export { audio } from './namespaces/audio.js';
export { clipboard } from './namespaces/clipboard.js';
export { config } from './namespaces/config.js';
export { consoleApi } from './namespaces/consoleApi.js';
export { cursor } from './namespaces/cursor.js';
export { dialog } from './namespaces/dialog.js';
export { discovery } from './namespaces/discovery.js';
export { dnd } from './namespaces/dnd.js';
export { dsp } from './namespaces/dsp.js';
export { event } from './namespaces/event.js';
export { file } from './namespaces/file.js';
export { http } from './namespaces/http.js';
export { jitQueue } from './namespaces/jitQueue.js';
export { keyboard } from './namespaces/keyboard.js';
export { library } from './namespaces/library.js';
export { log } from './namespaces/log.js';
export { lyrics } from './namespaces/lyrics.js';
export { menu } from './namespaces/menu.js';
export { metadata } from './namespaces/metadata.js';
export { misc } from './namespaces/misc.js';
export { notification } from './namespaces/notification.js';
export { output } from './namespaces/output.js';
export { panel } from './namespaces/panel.js';
export { player } from './namespaces/player.js';
export { playcount } from './namespaces/playcount.js';
export { playlist } from './namespaces/playlist.js';
export { port } from './namespaces/port.js';
export { queue } from './namespaces/queue.js';
export { rating } from './namespaces/rating.js';
export { replaygain } from './namespaces/replaygain.js';
export { selection } from './namespaces/selection.js';
export { sharedState } from './namespaces/sharedState.js';
export { shell } from './namespaces/shell.js';
export { system } from './namespaces/system.js';
export { taskbar } from './namespaces/taskbar.js';
export { titleformat } from './namespaces/titleformat.js';
export { tray } from './namespaces/tray.js';
export { ui } from './namespaces/ui.js';
export { utils } from './namespaces/utils.js';

/**
 * Aggregate runtime SDK surface. Provides the canonical `window.fb`
 * shape used by themes and SMP-compatible scripts.
 *
 * - `fb.playback`, `fb.playlist`, `fb.library`, ... — namespace proxies.
 * - `fb.on` / `fb.off` / `fb.once` — typed event subscription.
 * - `fb.invoke` — escape hatch for un-namespaced API calls.
 * - `fb.state` — synchronous playback state mirror.
 * - `fb.isAvailable()` / `fb.ready()` — availability probes.
 */
export const fb = {
    // Reactive state mirror
    state,

    // Event subscription surface
    on: bridge.on.bind(bridge),
    off: bridge.off.bind(bridge),
    once: bridge.once.bind(bridge),

    // Direct invoke escape hatch
    invoke: bridge.invoke.bind(bridge),

    // Availability probes
    isAvailable: () => bridge.isAvailable,
    ready: () => bridge.ready(),

    // Namespaces (alphabetical)
    artwork,
    audio,
    clipboard,
    config,
    console: consoleApi,
    cursor,
    dialog,
    discovery,
    dnd,
    dsp,
    event,
    file,
    http,
    jitQueue,
    keyboard,
    library,
    log,
    lyrics,
    menu,
    metadata,
    misc,
    notification,
    output,
    panel,
    playcount,
    player,
    playlist,
    port,
    queue,
    rating,
    replaygain,
    selection,
    sharedState,
    shell,
    system,
    taskbar,
    titleformat,
    tray,
    ui,
    utils,
} as const;

export default fb;
