// Records the playback state of the instance before a live run and puts it back afterwards,
// and turns playback down for the run: stopped, at half the user's volume (6 dB lower).
//
// The saved track is replayed only when its playlist position still holds the same file;
// portable instances report file-relative:// paths, so paths are compared by their last two
// segments.
import fs from 'node:fs';

import { connect, sleep, step } from './bridge.mjs';

const normPath = (p) => String(p ?? '').replace(/\//g, '\\').toLowerCase().split('\\').slice(-2).join('\\');

function userDbOf(volume) {
  if (typeof volume?.volumeDb === 'number') return volume.volumeDb;
  const pct = Number(volume?.volume);
  return pct > 0 ? 20 * Math.log10(pct / 100) : -100;
}

async function read(tryInv) {
  return {
    at: new Date().toISOString(),
    state: await tryInv('playback.getState'),
    track: await tryInv('playback.getCurrentTrack'),
    loc: await tryInv('playback.getCurrentTrackIndex', {}),
    position: await tryInv('playback.getPosition'),
    volume: await tryInv('playback.getVolume'),
    order: await tryInv('playback.getPlaybackOrder'),
    active: await tryInv('playlist.getActive'),
    playlists: await tryInv('playlist.getAll'),
  };
}

const playlistNames = (all) => (Array.isArray(all) ? all : (all?.playlists ?? [])).map((p) => p?.name);

export async function snapshot(file) {
  const snap = await connect((_inv, tryInv) => read(tryInv));
  fs.writeFileSync(file, JSON.stringify(snap, null, 1));
  step(`saved state=${snap.state?.state} volume=${snap.volume?.volumeDb ?? snap.volume?.volume} to ${file}`);
  return snap;
}

export async function show() {
  const snap = await connect((_inv, tryInv) => read(tryInv));
  console.log(JSON.stringify({ state: snap.state, position: snap.position, volume: snap.volume, loc: snap.loc, order: snap.order, active: snap.active }, null, 1));
}

/** Stops playback and sets the volume 6 dB below the saved one. */
export async function quiet(file) {
  const snap = JSON.parse(fs.readFileSync(file, 'utf8'));
  const userDb = userDbOf(snap.volume);
  const targetDb = Math.max(-100, Math.floor(userDb - 6.0206));
  await connect(async (inv) => {
    await inv('playback.stop');
    await inv('playback.setVolume', { volume: 100 * Math.pow(10, targetDb / 20) });
  });
  step(`stopped; volume ${userDb.toFixed(1)} dB -> ${targetDb} dB`);
}

export async function restore(file, { connection = connect, wait = sleep } = {}) {
  const snap = JSON.parse(fs.readFileSync(file, 'utf8'));
  await connection(async (inv, tryInv) => {
    const s = snap.state?.state;
    const loc = snap.loc;
    const wantPath = snap.position?.path ?? snap.track?.track?.path ?? snap.track?.path;
    const wantPos = Number(snap.position?.position ?? 0);
    if (s !== 'stopped' && loc?.found && Number.isInteger(loc.playlist) && Number.isInteger(loc.index)) {
      const page = await tryInv('playlist.getTracks', { playlist: loc.playlist, start: loc.index, count: 1, fields: ['path', 'duration'] });
      const row = page?.tracks?.[0];
      if (row && normPath(row.path) === normPath(wantPath)) {
        await inv('playback.setVolume', { volume: 0 });
        await inv('playlist.playTrack', { playlist: loc.playlist, index: loc.index });
        await wait(800);
        if (s === 'paused') {
          await wait(200);
          await tryInv('playback.pause');
        }
        // Seek after pausing so the wait and output drain cannot advance a paused snapshot.
        const duration = Number(row.duration ?? snap.position?.duration ?? 0);
        if (wantPos >= 0 && (!duration || wantPos < duration)) await tryInv('playback.setPosition', { position: wantPos });
      } else {
        step('the saved playlist position no longer holds the saved track; not replaying it');
      }
    } else if (s === 'stopped') {
      await tryInv('playback.stop');
    }
    await tryInv('playback.setVolume', { volume: Number(snap.volume?.volume ?? 0) });
    if (snap.volume?.muted === true) await tryInv('playback.mute', { muted: true });
    if (Number.isInteger(snap.order?.order)) await tryInv('playback.setPlaybackOrder', { order: snap.order.order });
    const activeIdx = snap.active?.index ?? snap.active?.playlist;
    if (Number.isInteger(activeIdx) && activeIdx >= 0) await tryInv('playlist.setActive', { playlist: activeIdx });
    await wait(300);
    const after = await read(tryInv);
    const before = playlistNames(snap.playlists);
    const now = playlistNames(after.playlists);
    step(`restored state=${after.state?.state} volume=${after.volume?.volumeDb ?? after.volume?.volume} order=${after.order?.order}`);
    const added = now.filter((n) => !before.includes(n));
    const removed = before.filter((n) => !now.includes(n));
    if (added.length || removed.length) step(`playlists changed by the run: added ${JSON.stringify(added)}, removed ${JSON.stringify(removed)}`);
  });
}
