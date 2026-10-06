import { call } from '../call.js';
import { canPlay } from '../media/canPlay.js';
import type { MediaGetContainerInfoSuccess } from '../../types/generated/responses.js';

/** Container track metadata; omitted fields are unknown, not zero. */
export type MediaContainerTrack = MediaGetContainerInfoSuccess['tracks'][number];

/** Attachment metadata without attachment contents or an extraction URL. */
export type MediaContainerAttachment = MediaGetContainerInfoSuccess['attachments'][number];

/**
 * Local media access and advisory browser decoding capabilities.
 * @experimental
 */
export const media = {
    /**
     * Issue a local media URL usable only by the requesting document.
     * Navigation, file changes and token eviction invalidate the URL; do not persist it.
     * Requires a trusted HTTP(S) page and a host with WebView2 media routing support.
     * Set an element's crossOrigin to 'anonymous' before assigning src. For fetch,
     * files larger than 2 MiB require Range; each response contains at most 2 MiB.
     * Resolves with a success or failure envelope; bridge transport errors may reject.
     * @experimental
     */
    getStreamUrl: (path: string) => call('media.getStreamUrl', { path }),

    /**
     * Read container metadata without decoding; missing fields remain unknown.
     * The result describes the whole container, not a subsong timeline.
     * An unrecognized file resolves successfully with recognized:false; read or
     * parse failures use a failure envelope. Bridge transport errors may reject.
     * @experimental
     */
    getContainerInfo: (path: string) => call('media.getContainerInfo', { path }),

    /**
     * Ask the browser about a declared track without filling missing metadata.
     * Video needs actual maximum frame rate and bitrate in the limits argument.
     * @experimental
     */
    canPlay,
};
