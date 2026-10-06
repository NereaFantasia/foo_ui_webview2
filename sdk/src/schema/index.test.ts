import { describe, expect, it } from 'vitest';

import { API_PARAM_SHAPES, findParamKeyProblem, PARAM_SHAPE_TYPES } from './index.js';

describe('schema entry', () => {
    it('has a shape for every declared method and none for an undeclared one', () => {
        expect(API_PARAM_SHAPES['playlist.getTracks']).toBeDefined();
        expect(findParamKeyProblem('test.echo', { anything: 1 })).toBeNull();
        expect(findParamKeyProblem('no.suchMethod', { anything: 1 })).toBeNull();
    });

    it('passes declared keys and keys the bridge adds', () => {
        expect(findParamKeyProblem('playlist.getTracks', { playlist: 0, start: 0, count: 10 })).toBeNull();
        expect(findParamKeyProblem('playlist.getTracks', { playlist: 0, _hwnd: 1 })).toBeNull();
        expect(findParamKeyProblem('playlist.getTracks', undefined)).toBeNull();
    });

    it('refuses an undeclared key', () => {
        expect(findParamKeyProblem('playlist.getTracks', { playlist: 0, limit: 10 }))
            .toEqual({ path: 'limit', reason: 'unknown' });
    });

    it('refuses a missing required key, and null counts as missing', () => {
        expect(findParamKeyProblem('playback.setVolume', {})).toEqual({ path: 'volume', reason: 'missing' });
        expect(findParamKeyProblem('playback.setVolume', { volume: null })).toEqual({ path: 'volume', reason: 'missing' });
        expect(findParamKeyProblem('playback.setVolume', { volume: 50 })).toBeNull();
    });

    it('treats a key whose value is undefined as absent, since JSON leaves it out', () => {
        expect(findParamKeyProblem('playback.setVolume', { volume: 50, bogus: undefined })).toBeNull();
        expect(findParamKeyProblem('playback.setVolume', { volume: undefined })).toEqual({ path: 'volume', reason: 'missing' });
        expect(findParamKeyProblem('metadata.writeBatch', { items: [{ path: 'C:\\a.flac', tags: {}, tag: undefined }] }))
            .toBeNull();
    });

    it('checks every object of an array, with the index in the path', () => {
        const items = [
            { path: 'C:\\a.flac', tags: { TITLE: 'A' } },
            { path: 'C:\\b.flac', tag: {} },
        ];
        expect(findParamKeyProblem('metadata.writeBatch', { items })).toEqual({ path: 'items[1].tag', reason: 'unknown' });
        expect(findParamKeyProblem('metadata.writeBatch', { items: [{ tags: {} }] }))
            .toEqual({ path: 'items[0].path', reason: 'missing' });
    });

    it('takes any key of a map', () => {
        expect(findParamKeyProblem('metadata.write', { path: 'C:\\a.flac', tags: { ANY_TAG: 'x', other: 1 } })).toBeNull();
    });

    it('follows a recursive type into its nested items', () => {
        expect(Object.keys(PARAM_SHAPE_TYPES)).toContain('TrayMenuItem');
        const items = [{ id: 'a', label: 'A', submenu: [{ id: 'b', label: 'B', colour: 'red' }] }];
        expect(findParamKeyProblem('tray.setContextMenu', { items }))
            .toEqual({ path: 'items[0].submenu[0].colour', reason: 'unknown' });
    });

    it('refuses parameters that are not an object', () => {
        expect(findParamKeyProblem('playlist.getTracks', [0])).toEqual({ path: '', reason: 'notObject' });
        expect(findParamKeyProblem('metadata.writeBatch', { items: ['C:\\a.flac'] }))
            .toEqual({ path: 'items[0]', reason: 'notObject' });
    });
});
