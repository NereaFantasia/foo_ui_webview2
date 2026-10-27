/**
 * Exercises editable autoplaylist rules on scratch playlists, including two-window
 * optimistic concurrency. Reads a bounded library sample without changing media
 * files or monitored roots. The caller selects and verifies the host instance.
 * Restart and native failure-injection checks require a separate host lifecycle.
 */
import CDP from 'chrome-remote-interface';
import {
    closeClient, connectBridgePage, createBridge, createEventCollector,
    createRecorder, report, requireResponsiveBridge, resolvePort,
} from './lib/e2e-harness.mjs';

const runId = Date.now().toString();
const prefix = `e2e managed ${runId}`;
const eventName = 'playlist:autoplaylistConfigChanged';
const emptyQuery = `title IS __managed_config_${runId}__`;
const recorder = createRecorder();
const owned = new Set();
let bridge, client, collector, popupBridge, popupClient, popupCollector, popupId;
let fatalError, blocked = false;
let entryActive;

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const config = (value) => ({ query: value.query, sort: value.sort, keepSorted: value.keepSorted, revision: value.revision });
const check = (name, passed, observed, expected) => recorder.assertCase(name, passed, observed, expected);

function success(value) {
    if (value?.success !== true) throw new Error(`Setup call failed: ${JSON.stringify(value)}`);
    return value;
}

async function read(guid) {
    return success(await bridge.invoke('playlist.getAutoplaylistConfig', { playlistGuid: guid }));
}

async function set(current, patch, channel = bridge) {
    return channel.invoke('playlist.setAutoplaylistConfig', {
        playlistGuid: current.playlistGuid, expectedRevision: current.revision, ...patch,
    });
}

async function tracks(guid) {
    return success(await bridge.invoke('playlist.getTracks', { playlistGuid: guid, count: 20, fields: ['path', 'subsong'] }));
}

async function waitForTracks(guid, count) {
    const deadline = Date.now() + 10000;
    let page;
    do {
        page = await tracks(guid);
        if (page.total === count) return page;
        await sleep(50);
    } while (Date.now() < deadline);
    return page;
}

async function attachPopup(windowId) {
    const deadline = Date.now() + 20000;
    do {
        const pages = await CDP.List({ port: resolvePort() });
        const page = pages.find((target) => {
            try { return target.type === 'page' && new URL(target.url).searchParams.get('windowId') === windowId; }
            catch { return false; }
        });
        if (page) {
            const connection = await CDP({ port: resolvePort(), target: page.id });
            const channel = createBridge(connection.Runtime);
            if (await channel.evaluateValue('typeof window.fb2k?.invoke === "function"', false)) {
                return { connection, channel };
            }
            await closeClient(connection);
        }
        await sleep(100);
    } while (Date.now() < deadline);
    throw new Error('The owned popup did not expose its bridge');
}

try {
    const connected = await connectBridgePage();
    client = connected.client;
    bridge = createBridge(client.Runtime, { invokeTimeoutMs: 15000 });
    await requireResponsiveBridge(bridge);
    entryActive = success(await bridge.invoke('playlist.getActive', {}));
    collector = await createEventCollector(bridge, [eventName], { collectorId: `${runId}main` });

    const before = success(await bridge.invoke('playlist.getAll', {}));
    for (const query of ['', ' \t\r\n', 'NOT ALL', 'x\u0000y', 'x'.repeat(65537)]) {
        const refused = await bridge.invoke('playlist.createManagedAutoplaylist', { name: prefix, query });
        check('invalid creation reports the query field', refused.success === false && refused.code === 'INVALID_PARAMS' && refused.details?.param === 'query', refused, 'INVALID_PARAMS/query');
    }
    const after = success(await bridge.invoke('playlist.getAll', {}));
    check('invalid creation leaves the playlist set unchanged', same(before.playlists, after.playlists), after, before);

    let current = success(await bridge.invoke('playlist.createManagedAutoplaylist', {
        name: prefix, query: emptyQuery, sort: '%path%', keepSorted: true,
    }));
    const guid = current.playlistGuid;
    owned.add(guid);
    check('creation returns the declared snapshot keys', same(Object.keys(current).sort(), ['keepSorted', 'playlistGuid', 'query', 'revision', 'sort', 'success']), Object.keys(current), 'six declared keys');
    const info = success(await bridge.invoke('playlist.getAutoplaylistInfo', { playlistGuid: guid }));
    check('managed configuration and native editor capabilities are separate', info.configAvailability === 'available' && info.configAvailabilityReason === 'supported' && info.editorAvailability === 'unavailable', info, 'config available, native editor unavailable');
    const oldQuery = success(await bridge.invoke('playlist.getAutoplaylistQuery', { playlistGuid: guid }));
    check('the existing query getter keeps its null contract', oldQuery.query === null, oldQuery, { query: null });
    check('the getter reads original text and current flags', same(config(await read(guid)), config(current)), await read(guid), current);

    await collector.drain();
    const eventCount = collector.received.length;
    const unchanged = success(await set(current, { query: current.query, sort: current.sort, keepSorted: true }));
    await sleep(100);
    await collector.drain();
    check('same-value update changes neither revision nor config event count', unchanged.revision === current.revision && collector.received.length === eventCount, { unchanged, count: collector.received.length }, { revision: current.revision, count: eventCount });

    for (const patch of [{}, { query: 'NOT ALL' }, { query: `${emptyQuery} SORT BY %title%`, sort: '%path%' }]) {
        const refused = await set(current, patch);
        check('invalid edits leave the complete snapshot unchanged', refused.success === false && refused.code === 'INVALID_PARAMS' && same(config(await read(guid)), config(current)), refused, 'INVALID_PARAMS and unchanged snapshot');
    }
    const noTarget = await bridge.invoke('playlist.getAutoplaylistConfig', {});
    const bothTargets = await bridge.invoke('playlist.getAutoplaylistConfig', { playlist: 0, playlistGuid: guid });
    check('configuration requires exactly one explicit target', noTarget.code === 'INVALID_PARAMS' && bothTargets.code === 'INVALID_PARAMS', { noTarget, bothTargets }, 'INVALID_PARAMS');

    const textChanged = success(await set(current, { query: `${emptyQuery} AND title PRESENT` }));
    check('text-only changes advance revision even with identical results', textChanged.revision !== current.revision && textChanged.keepSorted === true, textChanged, 'new revision');
    const conflict = await set(current, { sort: '' });
    check('stale revisions return the current snapshot without overwriting', conflict.code === 'OPERATION_FAILED' && conflict.details?.reason === 'revisionConflict' && conflict.details.current.revision === textChanged.revision, conflict, 'revisionConflict/current');
    current = success(await set(textChanged, { sort: '', keepSorted: false }));
    check('empty sorting and false flags are preserved', current.sort === '' && current.keepSorted === false && current.query === textChanged.query, current, 'only requested fields changed');

    const popup = success(await bridge.invoke('window.createPopup', { title: prefix, width: 240, height: 180, maxWidth: 240, maxHeight: 180, resizable: false }));
    popupId = popup.windowId;
    const attached = await attachPopup(popupId);
    popupClient = attached.connection;
    popupBridge = attached.channel;
    popupCollector = await createEventCollector(popupBridge, [eventName], { collectorId: `${runId}popup` });
    const racers = await Promise.all([set(current, { sort: '%title%' }), set(current, { sort: '%path%' }, popupBridge)]);
    const winner = racers.find((value) => value.success);
    const loser = racers.find((value) => value.success === false);
    check('two windows cannot overwrite the same expected revision', Boolean(winner) && loser?.details?.reason === 'revisionConflict', racers, 'one success and one conflict');
    current = await read(guid);
    for (const events of [collector, popupCollector]) {
        await events.waitFor((received) => received.some((e) => e.payload?.playlistGuid === guid && e.payload?.revision === current.revision), { timeoutMs: 4000 });
        check('configuration change is broadcast to each component window', events.received.some((e) => e.payload?.playlistGuid === guid && e.payload?.revision === current.revision), events.received, current.revision);
    }

    const second = success(await bridge.invoke('playlist.createManagedAutoplaylist', { name: `${prefix} second`, query: emptyQuery }));
    owned.add(second.playlistGuid);
    const secondChanged = success(await set(second, { keepSorted: true }));
    await collector.waitFor((events) => events.some((e) => e.payload?.playlistGuid === second.playlistGuid && e.payload?.revision === secondChanged.revision), { timeoutMs: 4000 });
    check('events identify the changed playlist independently', collector.received.some((e) => e.payload?.playlistGuid === second.playlistGuid && e.payload?.revision === secondChanged.revision) && (await read(guid)).revision === current.revision, collector.received, secondChanged);

    const all = success(await bridge.invoke('playlist.getAll', {}));
    const originalOrder = all.playlists.map((p) => p.guid);
    success(await bridge.invoke('playlist.reorderPlaylists', { newOrderGuids: [guid, ...originalOrder.filter((id) => id !== guid)] }));
    check('GUID and revision survive playlist reordering', (await read(guid)).revision === current.revision, await read(guid), current);
    const byIndex = success(await bridge.invoke('playlist.getAutoplaylistConfig', { playlist: 0 }));
    check('index lookup resolves the current playlist identity', byIndex.playlistGuid === guid, byIndex, guid);
    success(await bridge.invoke('playlist.reorderPlaylists', { newOrderGuids: originalOrder }));

    const sample = success(await bridge.invoke('library.query', { query: 'ALL', limit: 3, fields: ['absolutePath'] }));
    const formats = success(await bridge.invoke('titleformat.evalBatch', { pattern: '%path%', paths: sample.tracks.map((t) => t.absolutePath) }));
    const paths = [...new Set(formats.results.filter((r) => r.success && r.result && !r.result.includes('"')).map((r) => r.result))];
    if (paths.length < 2) throw new Error('A library sample with distinct paths is required');
    const sampleQuery = paths.map((path) => `%path% IS "${path}"`).join(' OR ');
    const matches = success(await bridge.invoke('library.query', { query: sampleQuery, limit: 11, fields: ['path', 'subsong'] }));
    if (matches.total < 2 || matches.total > 10) throw new Error('The selected sample is not bounded to 2–10 tracks');
    current = success(await set(current, { query: sampleQuery, sort: '%path%', keepSorted: true }));
    const ascending = await waitForTracks(guid, matches.total);
    current = success(await set(current, { query: `${sampleQuery} SORT DESCENDING BY %path%`, sort: '' }));
    let descending;
    for (let attempt = 0; attempt < 100; ++attempt) {
        descending = await tracks(guid);
        if (same(descending.tracks.map((t) => t.path), ascending.tracks.map((t) => t.path).reverse())) break;
        await sleep(50);
    }
    check('actual nonempty query sorting reverses independent sorting', ascending.total === matches.total && same(descending.tracks.map((t) => t.path), ascending.tracks.map((t) => t.path).reverse()), { ascending, descending }, 'same bounded tracks in reverse order');
    const beforeRelease = await tracks(guid);
    success(await bridge.invoke('playlist.removeAutoplaylist', { playlistGuid: guid }));
    const released = await bridge.invoke('playlist.getAutoplaylistConfig', { playlistGuid: guid });
    check('release preserves nonempty tracks and removes configuration support', same((await tracks(guid)).tracks, beforeRelease.tracks) && released.code === 'NOT_SUPPORTED' && released.details?.reason === 'notAutoplaylist', released, 'preserved tracks, notAutoplaylist');

    const external = success(await bridge.invoke('playlist.createAutoplaylist', { name: `${prefix} external`, query: emptyQuery }));
    owned.add(external.guid);
    const unsupported = await bridge.invoke('playlist.getAutoplaylistConfig', { playlistGuid: external.guid });
    const externalInfo = success(await bridge.invoke('playlist.getAutoplaylistInfo', { playlistGuid: external.guid }));
    check('core providers remain external to editable configuration', unsupported.code === 'NOT_SUPPORTED' && unsupported.details?.reason === 'externalProvider' && externalInfo.configAvailabilityReason === 'externalProvider', { unsupported, externalInfo }, 'externalProvider');
    success(await bridge.invoke('playlist.remove', { playlistGuid: second.playlistGuid }));
    owned.delete(second.playlistGuid);
    const gone = await bridge.invoke('playlist.setAutoplaylistConfig', { playlistGuid: second.playlistGuid, expectedRevision: secondChanged.revision, sort: '' });
    check('a deleted target does not fall back to another playlist', gone.code === 'NOT_FOUND', gone, 'NOT_FOUND');
} catch (error) {
    fatalError = error;
    blocked = Boolean(error?.blocked);
    console.error(error);
} finally {
    if (popupCollector) await popupCollector.stop().catch(() => undefined);
    await closeClient(popupClient);
    if (bridge && popupId) await bridge.invokeRaw('window.closePopup', { windowId: popupId });
    if (bridge) {
        for (const guid of owned) {
            const removed = await bridge.invokeRaw('playlist.remove', { playlistGuid: guid });
            check('scratch playlist cleanup succeeds by GUID', removed?.kind === 'result' && removed.value?.success === true, removed, guid);
        }
        if (entryActive?.guid) await bridge.invokeRaw('playlist.setActive', { playlistGuid: entryActive.guid });
    }
    if (collector) await collector.stop().catch(() => undefined);
    await closeClient(client);
}

process.exit(report({ recorder, fatalError, blocked, extra: { runId, targetPort: resolvePort() } }));
