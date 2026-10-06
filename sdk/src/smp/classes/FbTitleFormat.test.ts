// sdk/src/smp/classes/FbTitleFormat.test.ts
//
// Every path FbTitleFormat sends to titleformat.eval / titleformat.evalBatch is
// the track's handle id: a subsong of a multi-track file (a CUE sheet) keeps
// its `|subsong:N` suffix, so the host evaluates that subsong and not the
// file's first one. Plain files are sent as bare paths.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { FbMetadbHandle } from './FbMetadbHandle.js';
import { FbMetadbHandleList } from './FbMetadbHandleList.js';
import { FbTitleFormat } from './FbTitleFormat.js';

const globalWithSmp = globalThis as { smp?: { invoke?: unknown; cache?: unknown } };
let savedSmp: unknown;
let invoke: ReturnType<typeof vi.fn>;

/** A host track row: `absolutePath` without suffix, `subsong` beside it. */
function trackRow(absolutePath: string, subsong: number): Record<string, unknown> {
    return { absolutePath, path: `file://${absolutePath}`, subsong, title: 't' };
}

beforeEach(() => {
    savedSmp = globalWithSmp.smp;
    invoke = vi.fn(async (method: string, params: { paths?: string[] }) =>
        method === 'titleformat.evalBatch'
            ? {
                  success: true,
                  results: (params.paths ?? []).map((p) => ({ success: true, result: p })),
              }
            : { success: true, result: 'r' },
    );
    globalWithSmp.smp = { invoke };
});

afterEach(() => {
    globalWithSmp.smp = savedSmp as typeof globalWithSmp.smp;
});

describe('FbTitleFormat keeps the subsong suffix', () => {
    it('EvalWithMetadb sends the handle id of a subsong handle', async () => {
        await new FbTitleFormat('%title%').EvalWithMetadb(
            new FbMetadbHandle('C:\\album.cue|subsong:2'),
        );
        expect(invoke).toHaveBeenCalledWith('titleformat.eval', {
            path: 'C:\\album.cue|subsong:2',
            pattern: '%title%',
        });
    });

    it('EvalWithMetadb keeps the suffix of a string and adds it for a track row', async () => {
        const tf = new FbTitleFormat('%title%');
        await tf.EvalWithMetadb('C:\\album.cue|subsong:4');
        await tf.EvalWithMetadb(trackRow('C:\\album.cue', 3));
        await tf.EvalWithMetadb('C:\\a.flac');
        expect(invoke.mock.calls.map((c) => (c[1] as { path: string }).path)).toEqual([
            'C:\\album.cue|subsong:4',
            'C:\\album.cue|subsong:3',
            'C:\\a.flac',
        ]);
    });

    it('Eval sends the now-playing track row with its subsong', async () => {
        globalWithSmp.smp = { invoke, cache: { currentTrack: trackRow('D:\\live.cue', 5) } };
        const r = await new FbTitleFormat('%artist%').Eval();
        expect(r).toBe('r');
        expect(invoke).toHaveBeenCalledWith('titleformat.eval', {
            path: 'D:\\live.cue|subsong:5',
            pattern: '%artist%',
        });
    });

    it('EvalWithMetadbs sends one handle id per entry of a list or an array', async () => {
        const tf = new FbTitleFormat('%title%');
        const list = new FbMetadbHandleList([
            new FbMetadbHandle('C:\\album.cue|subsong:1'),
            new FbMetadbHandle('C:\\album.cue|subsong:2'),
            new FbMetadbHandle('C:\\a.flac'),
        ]);
        const fromList = await tf.EvalWithMetadbs(list);
        const fromArray = await tf.EvalWithMetadbs([trackRow('C:\\album.cue', 7), 'C:\\b.flac']);

        expect(fromList).toEqual([
            'C:\\album.cue|subsong:1',
            'C:\\album.cue|subsong:2',
            'C:\\a.flac',
        ]);
        expect(fromArray).toEqual(['C:\\album.cue|subsong:7', 'C:\\b.flac']);
        expect(invoke.mock.calls.every((c) => c[0] === 'titleformat.evalBatch')).toBe(true);
    });
});
