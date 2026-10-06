import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { bridge } from '../../Bridge.js';
import { media, type MediaContainerTrack } from '../../namespaces/media.js';
import { canPlay } from '../canPlay.js';

const video: MediaContainerTrack = {
    id: '1', type: 'video', codec: 'avc1', codecs: 'avc1.640028', mimeType: 'video/mp4',
    width: 1920, height: 1080, frameRate: 24, bitrate: 4_000_000,
};
const supported = { supported: true, smooth: true, powerEfficient: false };
const limits = { maxFrameRate: 60, maxBitrate: 8_000_000 };
const unknown = { supported: 'unknown', smooth: 'unknown', powerEfficient: 'unknown' };
const decodingInfo = vi.fn(async (_configuration: MediaDecodingConfiguration) => supported);
const canPlayType = vi.fn((_type: string): CanPlayTypeResult => 'probably');

describe('media capability queries', () => {
    beforeEach(() => {
        decodingInfo.mockResolvedValue(supported);
        vi.stubGlobal('document', { createElement: () => ({ canPlayType }) });
        vi.stubGlobal('navigator', { mediaCapabilities: { decodingInfo } });
    });
    afterEach(() => { vi.unstubAllGlobals(); });

    it('uses the declared codec and actual decoding configuration', async () => {
        expect(await canPlay(video, limits)).toEqual({ ...supported, canPlayType: 'probably' });
        expect(canPlayType).toHaveBeenCalledWith('video/mp4; codecs="avc1.640028"');
        expect(decodingInfo).toHaveBeenCalledWith({ type: 'file', video: {
            contentType: 'video/mp4; codecs="avc1.640028"', width: 1920, height: 1080, framerate: 60, bitrate: 8_000_000,
        } });
    });

    it.each(['width', 'height', 'codecs', 'mimeType'] as const)(
        'leaves capabilities unknown when %s is missing', async (field) => {
            const track = { ...video };
            delete track[field];
            expect(await canPlay(track, limits)).toMatchObject(unknown);
            expect(decodingInfo).not.toHaveBeenCalled();
        },
    );

    it('does not replace missing or invalid frame rates and bitrates with guesses', async () => {
        expect(await canPlay(video)).toEqual({ ...unknown, canPlayType: 'probably' });
        expect(await canPlay({ ...video, frameRate: 0 })).toMatchObject(unknown);
        expect(await canPlay({ ...video, bitrate: Number.NaN })).toMatchObject(unknown);
        expect(decodingInfo).not.toHaveBeenCalled();
    });

    it.each(['frameRate', 'bitrate'] as const)('does not require average %s when actual maxima are provided', async (field) => {
        const track = { ...video };
        delete track[field];
        expect(await canPlay(track, limits)).toMatchObject(supported);
        expect(decodingInfo).toHaveBeenCalledWith({ type: 'file', video: {
            contentType: 'video/mp4; codecs="avc1.640028"', width: 1920, height: 1080, framerate: 60, bitrate: 8_000_000,
        } });
    });

    it.each([{ maxFrameRate: 60 }, { maxBitrate: 8_000_000 }, {}])('requires both video maxima: %o', async (partial) => {
        expect(await canPlay(video, partial)).toEqual({ ...unknown, canPlayType: 'probably' });
        expect(decodingInfo).not.toHaveBeenCalled();
    });

    it.each([Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, 0, -1])(
        'rejects invalid maximum video frame rate %s without querying', async (maxFrameRate) => {
            expect(await canPlay(video, { ...limits, maxFrameRate })).toMatchObject(unknown);
            expect(decodingInfo).not.toHaveBeenCalled();
        },
    );

    it.each([Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, 0, -1, 1.5, Number.MAX_SAFE_INTEGER + 1])(
        'rejects invalid maximum video bitrate %s without querying', async (maxBitrate) => {
            expect(await canPlay(video, { ...limits, maxBitrate })).toMatchObject(unknown);
            expect(decodingInfo).not.toHaveBeenCalled();
        },
    );

    it('accepts fractional maximum frame rates and safe maximum bitrates without using averages', async () => {
        await canPlay({ ...video, frameRate: Number.NaN, bitrate: 0 }, {
            maxFrameRate: 59.94, maxBitrate: Number.MAX_SAFE_INTEGER,
        });
        expect(decodingInfo).toHaveBeenCalledWith({ type: 'file', video: {
            contentType: 'video/mp4; codecs="avc1.640028"', width: 1920, height: 1080,
            framerate: 59.94, bitrate: Number.MAX_SAFE_INTEGER,
        } });
    });

    it('queries complete audio metadata without using video defaults', async () => {
        const audio: MediaContainerTrack = { id: '2', type: 'audio', codec: 'mp4a', codecs: 'mp4a.40.2', mimeType: 'audio/mp4', bitrate: 128000, channels: 2, sampleRate: 48000 };
        await canPlay(audio);
        expect(decodingInfo).toHaveBeenCalledWith({ type: 'file', audio: {
            contentType: 'audio/mp4; codecs="mp4a.40.2"', bitrate: 128000, channels: '2', samplerate: 48000,
        } });
        await canPlay(audio, { maxFrameRate: Number.NaN, maxBitrate: 8_000_000 });
        expect(decodingInfo).toHaveBeenLastCalledWith({ type: 'file', audio: {
            contentType: 'audio/mp4; codecs="mp4a.40.2"', bitrate: 128000, channels: '2', samplerate: 48000,
        } });
    });

    it('preserves unknown on absent or rejected browser APIs, with a separate type hint', async () => {
        decodingInfo.mockRejectedValueOnce(new TypeError('unsupported configuration'));
        expect(await canPlay(video, limits)).toEqual({ ...unknown, canPlayType: 'probably' });
        vi.stubGlobal('navigator', {});
        expect(await canPlay(video, limits)).toEqual({ ...unknown, canPlayType: 'probably' });
        vi.stubGlobal('document', undefined);
        expect(await canPlay(video, limits)).toEqual({ ...unknown, canPlayType: 'unknown' });
    });

    it('preserves a negative capability answer even if canPlayType is optimistic', async () => {
        decodingInfo.mockResolvedValueOnce({ supported: false, smooth: false, powerEfficient: false });
        expect(await canPlay({ ...video, mimeType: 'video/x-matroska' }, limits)).toMatchObject({ supported: false });
    });

    it('does not query image, subtitle or unknown tracks as video', async () => {
        expect(await canPlay({ ...video, type: 'image' })).toEqual({ ...unknown, canPlayType: 'unknown' });
        expect(decodingInfo).not.toHaveBeenCalled();
        expect(canPlayType).not.toHaveBeenCalled();
    });

    it('exposes typed path facades and the shared capability helper', async () => {
        const invoke = vi.spyOn(bridge, 'invoke').mockResolvedValue({ success: false, code: 'PERMISSION_DENIED', error: 'denied' });
        await media.getStreamUrl('file-relative://movie.mp4');
        await media.getContainerInfo('E:\\movie.mkv');
        expect(invoke).toHaveBeenNthCalledWith(1, 'media.getStreamUrl', { path: 'file-relative://movie.mp4' });
        expect(invoke).toHaveBeenNthCalledWith(2, 'media.getContainerInfo', { path: 'E:\\movie.mkv' });
        expect(media.canPlay).toBe(canPlay);
    });

    it('exports the same runtime symbols through named and aggregate entry points', async () => {
        const entry = await import('../../index.js');
        expect(entry.media).toBe(media);
        expect(entry.canPlay).toBe(canPlay);
        expect(entry.fb.media).toBe(media);
        expect(entry.fb.canPlay).toBe(canPlay);
        expect(entry.fb.MediaElementFollower).toBe(entry.MediaElementFollower);
    });
});
