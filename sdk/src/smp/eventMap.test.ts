// sdk/src/smp/eventMap.test.ts
//
// Payload adapters that turn a bridge event into the arguments of an SMP callback.

import { describe, expect, it } from 'vitest';

import type { FbMetadbHandleList } from './classes/FbMetadbHandleList.js';
import { SMP_PARAM_ADAPTERS } from './eventMap.js';

describe('SMP_PARAM_ADAPTERS', () => {
    describe('on_metadb_changed', () => {
        const adapt = SMP_PARAM_ADAPTERS.on_metadb_changed!;

        // metadb:changed entries carry the subsong next to a path without one, so the handles
        // handed to on_metadb_changed tell the tracks of one cue sheet apart.
        it('keeps the subsong of each changed track', () => {
            const [list, fromHook] = adapt({
                tracks: [
                    { handle: 'D:\\Music\\b.cue|subsong:3', path: 'D:\\Music\\b.cue', subsong: 3 },
                    { handle: 'D:\\Music\\a.flac', path: 'D:\\Music\\a.flac', subsong: 0 },
                ],
                count: 2,
                fromHook: true,
                timestamp: 0,
            }) as [FbMetadbHandleList, boolean];

            expect(fromHook).toBe(true);
            expect(list.Count).toBe(2);
            expect([list[0].Path, list[0].SubSong, list[0].HandleId]).toEqual([
                'D:\\Music\\b.cue',
                3,
                'D:\\Music\\b.cue|subsong:3',
            ]);
            expect([list[1].Path, list[1].SubSong, list[1].HandleId]).toEqual([
                'D:\\Music\\a.flac',
                0,
                'D:\\Music\\a.flac',
            ]);
        });
    });
});
