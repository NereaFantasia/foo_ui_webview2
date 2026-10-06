import assert from 'node:assert/strict';
import test from 'node:test';
import { mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { restore } from '../live-e2e/state.mjs';

for (const [savedState, savedPosition] of [['paused', 5.51], ['paused', 0], ['playing', 20], ['stopped', 0]]) {
  test(`恢复 ${savedState} 的 ${savedPosition} 秒位置，不把暂停等待算入恢复位置`, async (t) => {
    const dir = mkdtempSync(join(tmpdir(), 'media-state-'));
    assert.ok(dir.startsWith(join(tmpdir(), 'media-state-')));
    t.after(() => rmSync(dir, { recursive: true, force: true }));
    const file = join(dir, 'before.json');
    const path = 'file-relative://Music/album/track.flac';
    writeFileSync(file, JSON.stringify({ state: { state: savedState }, position: { path, position: savedPosition, duration: 100 },
      loc: { found: true, playlist: 1, index: 0 }, volume: { volume: 10 }, active: { index: 0 },
      order: { order: 0 }, playlists: { playlists: [] } }));
    let state = 'stopped', position = 0;
    const wait = async (ms) => { if (state === 'playing') position += ms / 1000; };
    const inv = async (method, params) => {
      if (method === 'playlist.getTracks') return { tracks: [{ path, duration: 100 }] };
      if (method === 'playlist.playTrack') { state = 'playing'; position = 0; }
      if (method === 'playback.pause') { await wait(520); state = 'paused'; }
      if (method === 'playback.stop') state = 'stopped';
      if (method === 'playback.setPosition') position = params.position;
      if (method === 'playback.getState') return { state };
      if (method === 'playback.getPosition') return { path, position };
      if (method === 'playback.getVolume') return { volume: 10 };
      if (method === 'playback.getPlaybackOrder') return { order: 0 };
      if (method === 'playlist.getAll') return { playlists: [] };
      return {};
    };
    await restore(file, { connection: async (fn) => fn(inv, inv), wait });
    assert.equal(state, savedState);
    if (savedState === 'paused') assert.equal(position, savedPosition);
    if (savedState === 'playing') assert.ok(position >= savedPosition && position - savedPosition < 1);
  });
}
