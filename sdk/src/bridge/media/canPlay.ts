import type { ContainerTrack as MediaContainerTrack } from '../../types/generated/schema-types.js';

/** Browser capability result; 'unknown' means it could not be queried. */
export type MediaCapability = boolean | 'unknown';

/** Advisory decoding results; the media element remains the final authority. */
export interface MediaCanPlayResult {
    supported: MediaCapability;
    smooth: MediaCapability;
    powerEfficient: MediaCapability;
    /** Independent MIME/codec hint; an empty string is a negative browser hint. */
    canPlayType: CanPlayTypeResult | 'unknown';
}

/** Actual video maxima required by MediaCapabilities; average values do not qualify. */
export interface MediaVideoDecodingLimits {
    /** Maximum coded frame rate in frames per second, finite and greater than zero. */
    maxFrameRate?: number;
    /** Maximum coded bitrate in bits per second, a positive safe integer. */
    maxBitrate?: number;
}

/**
 * Query file decoding with the track's actual MIME, codecs and metadata.
 * Missing configuration or unavailable/rejected browser APIs leave the three
 * capabilities 'unknown'. canPlayType is a separate MIME/codec hint. Neither
 * probe guarantees playback, especially for Matroska; handle element errors.
 * Video requires both actual maxima in limits; the track's average frameRate
 * and bitrate are never substituted. Audio uses its declared average bitrate
 * and ignores limits. Omit maxima that are unknown instead of estimating them.
 * @experimental
 */
export async function canPlay(track: MediaContainerTrack, limits?: MediaVideoDecodingLimits): Promise<MediaCanPlayResult> {
    const result: MediaCanPlayResult = {
        supported: 'unknown', smooth: 'unknown', powerEfficient: 'unknown', canPlayType: 'unknown',
    };
    if (track.type !== 'video' && track.type !== 'audio') return result;
    if (!track.mimeType) return result;
    const contentType = track.codecs
        ? `${track.mimeType}; codecs="${track.codecs}"` : track.mimeType;
    try {
        if (typeof document !== 'undefined') {
            result.canPlayType = document.createElement(track.type).canPlayType(contentType);
        }
    } catch {
        // A missing MIME probe does not rule out the decoding capability API.
    }
    const configuration = decodingConfiguration(track, contentType, limits);
    if (!configuration || typeof navigator === 'undefined' || !navigator.mediaCapabilities?.decodingInfo) return result;
    try {
        const answer = await navigator.mediaCapabilities.decodingInfo(configuration);
        result.supported = answer.supported;
        result.smooth = answer.smooth;
        result.powerEfficient = answer.powerEfficient;
    } catch {
        // Rejection means the browser could not answer, not that playback failed.
    }
    return result;
}

function positive(value: number | undefined): value is number {
    return typeof value === 'number' && Number.isFinite(value) && value > 0;
}

function decodingConfiguration(
    track: MediaContainerTrack, contentType: string, limits?: MediaVideoDecodingLimits,
): MediaDecodingConfiguration | null {
    if (!track.codecs) return null;
    if (track.type === 'video' && positive(track.width) && positive(track.height)
        && positive(limits?.maxFrameRate) && positive(limits?.maxBitrate) && Number.isSafeInteger(limits.maxBitrate)) {
        return { type: 'file', video: {
            contentType, width: track.width, height: track.height, framerate: limits.maxFrameRate, bitrate: limits.maxBitrate,
        } };
    }
    if (track.type === 'audio' && positive(track.bitrate) && positive(track.channels) && positive(track.sampleRate)) {
        return { type: 'file', audio: {
            contentType, channels: String(track.channels), samplerate: track.sampleRate, bitrate: track.bitrate,
        } };
    }
    return null;
}
