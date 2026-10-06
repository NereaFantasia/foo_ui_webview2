// Compile-time checks for the overloads of `audio.subscribeSpectrum`:
// `npm run type-check` fails when the frame type a callback receives drifts.
// Nothing here runs or ships; no entry point imports this file.

import { audio } from '../audio.js';
import type { AudioSpectrumPayload } from '../../../types/events.js';
import type { SpectrumBinsFrame } from '../../../types/responses.js';

type Equals<A, B> = (<T>() => T extends A ? 1 : 2) extends <T>() => T extends B ? 1 : 2 ? true : false;

function expectTrue<T extends true>(): T | undefined {
    return undefined;
}

export function checkSubscribeSpectrumFrameTypes(): void {
    audio.subscribeSpectrum(
        (frame) => {
            expectTrue<Equals<typeof frame, SpectrumBinsFrame>>();
            if (frame.channels === 'stereo') {
                expectTrue<Equals<typeof frame.left, number[]>>();
            } else {
                expectTrue<Equals<typeof frame.spectrum, number[]>>();
            }
        },
        { output: 'bins', channels: 'stereo' },
    );
    audio.subscribeSpectrum((frame) => {
        expectTrue<Equals<typeof frame, AudioSpectrumPayload>>();
    });
    audio.subscribeSpectrum(
        (frame) => {
            expectTrue<Equals<typeof frame, AudioSpectrumPayload>>();
        },
        { bands: 64, scale: 'db' },
    );
}
