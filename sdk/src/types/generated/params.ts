// ─────────────────────────────────────────────────────────────
// GENERATED FILE — DO NOT EDIT
// File: sdk/src/types/generated/params.ts
// Source: src/api/schema/*.ts
// Emitter: scripts/gen_sdk_types.mjs
// Regenerate: npm run gen:types (in sdk/) or `node scripts/gen_sdk_types.mjs --all`
// ─────────────────────────────────────────────────────────────

/* eslint-disable */

import type { JsonValue } from '../json.js';

import type { ArtworkBatchItem, DspChainSpec, FileFilter, FileOpEntry, MetadataWriteBatchItem, QueueContentRef, QueueListRef, ThumbnailButton, TrayMenuConfig, TrayMenuItem, UiMenuItem, WindowRegion } from './schema-types.js';

/**
 * Parameters for `artwork.getAvailableArtwork`.
 * Report which picture types a file embeds and which cover files sit next to it.
 */
export interface ArtworkGetAvailableArtworkParams {
    /** Track path; a `|subsong:N` suffix is dropped. */
    path: string;
}

/**
 * Parameters for `artwork.getAvailableTypes`.
 * List the picture types embedded in a file, in the fixed probe order front, back, disc, icon, artist. Without `path` the playing track is used; with nothing playing the call fails with `NO_ACTIVE_ITEM`. A file that is missing, cannot be read or has a format without embedded pictures is not an error: it lists no types.
 */
export interface ArtworkGetAvailableTypesParams {
    /** Track path; omitted for the playing track. */
    path?: string;
}

/**
 * Parameters for `artwork.getBatch`.
 * Read one picture type from several files with the album art extractor, one row per path in the given order; a file without the picture gets a row with `available: false`.
 */
export interface ArtworkGetBatchParams {
    /** Track paths; a `|subsong:N` suffix is dropped. */
    paths: string[];
    /**
     * Picture type for every path.
     * @default "front"
     */
    type?: "front" | "cover_front" | "back" | "cover_back" | "disc" | "icon" | "artist";
}

/**
 * Parameters for `artwork.getByPath`.
 * Read a picture straight from the file at `path` with the album art extractor. The `|subsong:N` suffix is dropped, since pictures belong to the file; a `file-relative://` path is refused with `INVALID_PATH`, because only a playlist row can resolve it.
 */
export interface ArtworkGetByPathParams {
    /** Track path: native, `file://`, or with a `|subsong:N` suffix. */
    path: string;
    /**
     * Picture type.
     * @default "front"
     */
    type?: "front" | "cover_front" | "back" | "cover_back" | "disc" | "icon" | "artist";
}

/**
 * Parameters for `artwork.getByPlaylistItem`.
 * Read the picture of a playlist row through the album art manager. A negative `playlist` means the active playlist and a negative `index` means the first row. A row past the end, an empty playlist included, fails with `NOT_FOUND`. A playlist index past the last playlist fails with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`, and no active playlist when the active one is meant with `NO_ACTIVE_ITEM`. A row without the picture is a success with `available` `false`.
 */
export interface ArtworkGetByPlaylistItemParams {
    /**
     * Playlist index; negative for the active playlist. An index past the last playlist fails with `INVALID_INDEX`.
     * @default -1
     */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /**
     * Row index; negative for the first row.
     * @default -1
     */
    index?: number;
    /**
     * Picture type.
     * @default "front"
     */
    type?: "front" | "cover_front" | "back" | "cover_back" | "disc" | "icon" | "artist";
}

/**
 * Parameters for `artwork.getCurrent`.
 * Read the picture of the playing track. Three lookups are tried in order and `source` names the one that answered; `available: false` with `reason: "no_track"` when nothing is playing and `reason: "not_found"` when none of them had a picture.
 */
export interface ArtworkGetCurrentParams {
    /**
     * Picture type.
     * @default "front"
     */
    type?: "front" | "cover_front" | "back" | "cover_back" | "disc" | "icon" | "artist";
}

/**
 * Parameters for `artwork.getFb2kUrl`.
 * Build the `fb2k://artwork/` URL of the playing track's picture; the resource handler reads and scales the picture when the page loads the URL. `available: false` with `reason: "no_track"` when nothing is playing.
 */
export interface ArtworkGetFb2kUrlParams {
    /**
     * Picture type.
     * @default "front"
     */
    type?: "front" | "cover_front" | "back" | "cover_back" | "disc" | "icon" | "artist";
    /** Longest side in pixels the handler scales the picture down to; omitted or `0` keeps the original size. */
    maxSize?: number;
}

/**
 * Parameters for `artwork.getFb2kUrlByPath`.
 * Build the `fb2k://artwork/` URL for a path. Only a string is built: the file is not opened, so a path with no file behind it is still reported available.
 */
export interface ArtworkGetFb2kUrlByPathParams {
    /** Track path. */
    path: string;
    /**
     * Picture type.
     * @default "front"
     */
    type?: "front" | "cover_front" | "back" | "cover_back" | "disc" | "icon" | "artist";
    /** Longest side in pixels the handler scales the picture down to; omitted or `0` keeps the original size. */
    maxSize?: number;
}

/**
 * Parameters for `artwork.getFb2kUrlByPathBatch`.
 * Build `fb2k://artwork/` URLs for up to 100 entries given as exactly one of `paths` or `items`; giving both or neither fails with `INVALID_PARAMS`. Each row reports its own outcome, so a bad entry does not fail the call.
 */
export interface ArtworkGetFb2kUrlByPathBatchParams {
    /** Track paths; exactly one of `paths` and `items`. */
    paths?: string[];
    /** Entries with their own type or scale limit; exactly one of `paths` and `items`. */
    items?: ArtworkBatchItem[];
    /**
     * Picture type for entries that do not name their own.
     * @default "front"
     */
    type?: "front" | "cover_front" | "back" | "cover_back" | "disc" | "icon" | "artist";
    /** Longest side in pixels for entries that do not name their own; omitted or `0` keeps the original size. */
    maxSize?: number;
}

/**
 * Parameters for `artwork.getFolderImages`.
 * List the image files (`.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, `.webp`) directly inside a directory. A directory that is missing, is a file or cannot be listed is not an error: it lists no images.
 */
export interface ArtworkGetFolderImagesParams {
    /** Directory to list. */
    directory: string;
}

/**
 * Parameters for `artwork.getForTrack`.
 * Read a picture through the album art manager, which also finds folder covers, and measure it. `width` and `height` are read from the PNG header and are `0` for other formats. A `file-relative://` path is refused with `INVALID_PATH`.
 */
export interface ArtworkGetForTrackParams {
    /** Track path: native, `file://`, or with a `|subsong:N` suffix. */
    path: string;
    /**
     * Picture type.
     * @default "front"
     */
    type?: "front" | "cover_front" | "back" | "cover_back" | "disc" | "icon" | "artist";
}

/**
 * Parameters for `artwork.getLyrics`.
 * Read the lyrics tag of a track. The host's cached track info is used when complete; a local file it has not read is read from disk, and a remote or unrecognised path keeps whatever the cache has. The tags `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`, `SYNCEDLYRICS` and `SYNCED LYRICS` are probed in that order and the first non-empty one wins. Without `path` the playing track is used; with nothing playing the call fails with `NO_ACTIVE_ITEM`. A path no track can be made for fails with `NOT_FOUND`, a local file that cannot be read with `OPERATION_FAILED`.
 */
export interface ArtworkGetLyricsParams {
    /** Track path; omitted for the playing track. */
    path?: string;
}

/**
 * Parameters for `artwork.getMetadata`.
 * Read the album-level tags of a track, plus whether the file embeds any picture and whether it carries a lyrics tag. The tags come from the host's cached track info when complete; a local file it has not read is read from disk, and a remote or unrecognised path keeps whatever the cache has, which can be blank. Without `path` the playing track is used; with nothing playing the call fails with `NO_ACTIVE_ITEM`. A path no track can be made for fails with `NOT_FOUND`, a local file that cannot be read with `OPERATION_FAILED`.
 */
export interface ArtworkGetMetadataParams {
    /** Track path; omitted for the playing track. */
    path?: string;
}

/**
 * Parameters for `audio.analyzeBPM`.
 * Read the track's `BPM` tag. The host does not detect tempo. foobar2000's cached info is used when complete; otherwise the file itself is read, so tracks that are in no playlist or library work too.
 */
export interface AudioAnalyzeBPMParams {
    /** Track path; a `|subsong:N` suffix selects a subsong. */
    path: string;
}

/**
 * Parameters for `audio.cancelDecodePcm`.
 * Cancel a pending decodePcm task started by the same page.
 * @experimental
 */
export interface AudioCancelDecodePcmParams {
    /** Task id from the decodePcm answer. */
    taskId: string;
}

/**
 * Parameters for `audio.cancelFullWaveform`.
 * Cancel a pending generateFullWaveform request made by the same page.
 */
export interface AudioCancelFullWaveformParams {
    /** Task id from a `pending` answer of generateFullWaveform. */
    taskId: string;
}

/**
 * Parameters for `audio.decodePcm`.
 * Decode a track, or a range of it, into float32 planar PCM delivered through a shared buffer. Answers with a task id right away; the samples arrive as a `sharedbufferreceived` event on `chrome.webview` together with `audio:pcmReady`, or the task ends with `audio:pcmFailed`.
 * @experimental
 */
export interface AudioDecodePcmParams {
    /** Path of the track to decode; a `|subsong:N` suffix selects a subsong. */
    path: string;
    /** Cue subsong index, 0-based; takes precedence over a subsong in `path`. */
    cueIndex?: number;
    /** Start of the range, in seconds. */
    start?: number;
    /** End of the range, in seconds; defaults to the end of the track and is cut to the track length. Must be greater than `start`. */
    end?: number;
    /** Sample rate to convert to with foobar2000's resampler; defaults to the source rate. */
    sampleRate?: number;
    /** Average all channels into one. */
    mono?: boolean;
}

/**
 * Parameters for `audio.generateFullWaveform`.
 * Compute a waveform of a whole track. A cache hit answers `ready` with the waveform; otherwise the answer is `pending` with a task id, and the result arrives as `audio:fullWaveformReady` or `audio:fullWaveformFailed`.
 */
export interface AudioGenerateFullWaveformParams {
    /** Track path; a `|subsong:N` suffix selects a subsong. */
    path: string;
    /** Subsong index, 0-based; takes precedence over a subsong in `path`. */
    cueIndex?: number;
    /**
     * Number of points, clamped to 64 to 4096. A track is decoded once per resolution; the other options are computed from the cache.
     * @default 256
     */
    resolution?: number;
    /**
     * How each point is taken.
     * @default "rms"
     */
    method?: "rms" | "peak";
    /**
     * Scale of the points: `linear`, or `db`, where 60 dB below the track's own maximum maps to 0. Ignored when `signed` is set.
     * @default "linear"
     */
    scale?: "linear" | "db";
    /**
     * Keep PCM polarity; points fall in `[-1, 1]`.
     * @default false
     */
    signed?: boolean;
    /**
     * Answer from the cache when possible.
     * @default true
     */
    preferCache?: boolean;
}

/**
 * Parameters for `audio.getOutputInfo`.
 * Report foobar2000's volume.
 */
export type AudioGetOutputInfoParams = Record<string, never>;

/**
 * Parameters for `audio.getPcmDebugState`.
 * Report the shared-buffer support of the calling page, the decode task queue and the stream subscriptions, for tests and diagnostics.
 * @experimental
 */
export type AudioGetPcmDebugStateParams = Record<string, never>;

/**
 * Parameters for `audio.getSpectrum`.
 * Compute one spectrum frame on demand. At least one spectrum subscription must exist.
 */
export interface AudioGetSpectrumParams {
    /** Compute the frame with this subscription's parameters, bin output included; the other parameters are then ignored. An unknown id fails with `NOT_FOUND`. */
    subscriptionId?: string;
    /**
     * Used without `subscriptionId`. `0` takes the largest band count among all subscriptions; the value is capped at `fftSize / 2`.
     * @default 0
     */
    bands?: number;
    /**
     * Used without `subscriptionId`; as in subscribeSpectrum.
     * @default "weighted"
     */
    scale?: "weighted" | "db";
    /**
     * Used without `subscriptionId`; as in subscribeSpectrum.
     * @default 20
     */
    minFrequency?: number;
    /** Used without `subscriptionId`; as in subscribeSpectrum. */
    maxFrequency?: number;
    /**
     * Without `subscriptionId` only `bands` is accepted: the FFT size then follows the other subscriptions, so bin output needs a subscription of its own.
     * @default "bands"
     */
    output?: "bands" | "bins";
    /**
     * Without `subscriptionId` only `mix` is accepted.
     * @default "mix"
     */
    channels?: "mix" | "stereo";
}

/**
 * Parameters for `audio.getSpectrumDebugState`.
 * Report the spectrum runtime as the host sees it, for tests and diagnostics. The shape is not a stable contract.
 */
export type AudioGetSpectrumDebugStateParams = Record<string, never>;

/**
 * Parameters for `audio.getStreamInfo`.
 * Report the format of the playing track.
 */
export type AudioGetStreamInfoParams = Record<string, never>;

/**
 * Parameters for `audio.getWaveform`.
 * Return the most recent `duration` seconds of the visualization stream, ending at its current time (`streamTime` in spectrum frames). Needs a spectrum subscription.
 */
export interface AudioGetWaveformParams {
    /**
     * Window length in seconds, greater than 0.
     * @default 0.05
     */
    duration?: number;
    /**
     * `true` keeps PCM polarity, clamped to `[-1, 1]`; `false` maps the magnitude from -70 to 0 dB onto `[0, 1]`.
     * @default false
     */
    signed?: boolean;
    /**
     * `mix` averages the channels into `waveform`; `stereo` returns the first two channels as `left` and `right`.
     * @default "mix"
     */
    channels?: "mix" | "stereo";
    /** Thin the window to this many evenly spaced samples, without averaging; every sample is returned when omitted. */
    points?: number;
}

/**
 * Parameters for `audio.isVisualizationAvailable`.
 * Report whether foobar2000 offers a visualization stream. Spectrum frames and getWaveform need one.
 */
export type AudioIsVisualizationAvailableParams = Record<string, never>;

/**
 * Parameters for `audio.setChannelMode`.
 * Choose which channels the visualization stream carries. Applies to spectrum frames and getWaveform, not to what is heard.
 */
export interface AudioSetChannelModeParams {
    /**
     * Channels the visualization stream carries.
     * @default "default"
     */
    mode?: "default" | "mono" | "front" | "back";
}

/**
 * Parameters for `audio.subscribeSpectrum`.
 * Subscribe to spectrum frames of the playing audio. Each subscription keeps its own parameters; frames arrive under `event` in the calling window only, at up to `fps` frames per second while playback runs, plus one silence frame when it pauses or stops.
 */
export interface AudioSubscribeSpectrumParams {
    /** Key of the subscription; subscribing again with the same id replaces that subscription. When omitted, the key is derived from the calling window and `event`. */
    subscriptionId?: string;
    /**
     * FFT size, a power of two; other values fail with `INVALID_PARAMS`. Band output raises it for 32 or more bands; bin output uses it as given. 65536 is this component's ceiling, not a foobar2000 limit, and costs about 1.5 s of PCM per frame.
     * @default 1024
     */
    fftSize?: number;
    /**
     * Band output only: number of bands, clamped to 8 to `fftSize / 2`.
     * @default 48
     */
    bands?: number;
    /**
     * Frames per second, clamped to 1 to 60.
     * @default 30
     */
    fps?: number;
    /** Band scale, `weighted` when omitted. With `output: 'bins'` omit it or pass `db`; anything else fails with `INVALID_PARAMS`. */
    scale?: "weighted" | "db";
    /**
     * What the frames carry; `bands` is ignored for bin output.
     * @default "bands"
     */
    output?: "bands" | "bins";
    /**
     * Bin output only; `stereo` with band output fails with `INVALID_PARAMS`.
     * @default "mix"
     */
    channels?: "mix" | "stereo";
    /**
     * `true` limits this subscription to at most 12 frames per second, usually 10 to 12, while another application is in the foreground; `false` keeps the full rate.
     * @default true
     */
    backgroundThrottle?: boolean;
    /**
     * Lower edge of the frequency range in Hz; bands divide the range logarithmically.
     * @default 20
     */
    minFrequency?: number;
    /** Upper edge in Hz, greater than `minFrequency`. Capped at half the stream's sample rate when frames are computed, and follows it when omitted. A range entirely above that frequency yields silence frames. */
    maxFrequency?: number;
    /**
     * Event name the frames are delivered under.
     * @default "audio:spectrum"
     */
    event?: string;
}

/**
 * Parameters for `audio.subscribeStream`.
 * Subscribe to the audio foobar2000 is playing. Every chunk the core plays is copied into a ring buffer shared with the page, one buffer per subscription. The buffer arrives as a `sharedbufferreceived` event on `chrome.webview` with the first chunk of audio, and again with a new `epoch` whenever the sample rate or channel count changes; `audio:stream` then reports the old epoch as ended. Nothing arrives while playback is stopped or paused.
 * @experimental
 */
export interface AudioSubscribeStreamParams {
    /** Subscription id chosen by the caller; the host generates one (`pcmstream_N`) when absent. Subscribing again with the same id from the same page replaces the earlier subscription. */
    subscriptionId?: string;
    /** Callback interval to request from foobar2000, in seconds. The core rounds it to its own tick of about 16 ms and never goes above 200 ms; one subscription asking for a short interval shortens it for every stream subscription in the component. Defaults to the core's 200 ms. */
    interval?: number;
    /**
     * Ring buffer length, in seconds. Reduced when the buffer would exceed 64 MiB.
     * @default 1
     */
    bufferSeconds?: number;
}

/**
 * Parameters for `audio.unsubscribeSpectrum`.
 * Remove spectrum subscriptions of the calling page.
 */
export interface AudioUnsubscribeSpectrumParams {
    /** Subscription to remove; omit to remove every spectrum subscription of the calling page. */
    subscriptionId?: string;
}

/**
 * Parameters for `audio.unsubscribeStream`.
 * Remove stream subscriptions of the calling page. Their buffers get the `ended` flag and are closed on the host side; no event is sent.
 * @experimental
 */
export interface AudioUnsubscribeStreamParams {
    /** Subscription to remove; omit to remove every stream subscription of the calling page. */
    subscriptionId?: string;
}

/**
 * Parameters for `clipboard.read`.
 * Report what the Windows clipboard holds: text, a file list, an image.
 */
export type ClipboardReadParams = Record<string, never>;

/**
 * Parameters for `clipboard.write`.
 * Replace the clipboard contents with Unicode text.
 */
export interface ClipboardWriteParams {
    /** Text to place on the clipboard. */
    text: string;
}

/**
 * Parameters for `clipboard.writeFiles`.
 * Place a file list on the clipboard (`CF_HDROP`) for Explorer and other shell targets to paste.
 */
export interface ClipboardWriteFilesParams {
    /** Absolute file paths, in the order they are placed on the clipboard. */
    paths: string[];
}

/**
 * Parameters for `clipboard.writeHTML`.
 * Replace the clipboard contents with rich text in `HTML Format`, plus a plain-text fallback.
 */
export interface ClipboardWriteHTMLParams {
    /** HTML fragment to write as `HTML Format`. */
    html: string;
    /** Plain-text fallback written as `CF_UNICODETEXT`; when omitted or empty, the HTML text itself. */
    plainText?: string;
}

/**
 * Parameters for `config.export`.
 * Read the whole store both as a map and as one JSON text.
 */
export type ConfigExportParams = Record<string, never>;

/**
 * Parameters for `config.get`.
 * Read the value stored under a key. A key that is not there is not an error: `found` is `false` and `value` is `default`, or `null` when no default was given.
 */
export interface ConfigGetParams {
    /** Key to read. */
    key: string;
    /** Value to answer with when the key is absent. */
    default?: JsonValue;
}

/**
 * Parameters for `config.getActiveDspPreset`.
 * Report which DSP preset is selected. `index` and `name` are `null` and `isActive` is `false` when none is selected or presets are unavailable.
 */
export type ConfigGetActiveDspPresetParams = Record<string, never>;

/**
 * Parameters for `config.getAdvancedConfig`.
 * List foobar2000's Advanced preferences as a tree, from the root or from below `parentGuid`.
 */
export interface ConfigGetAdvancedConfigParams {
    /** GUID of the branch to list, rendered as `{...}`; the root of the tree when omitted or empty. A well-formed GUID that names no branch lists nothing. */
    parentGuid?: string;
}

/**
 * Parameters for `config.getAdvancedConfigValue`.
 * Read one Advanced preferences entry by GUID. An integer entry's value comes back as a number here, while `config.getAdvancedConfig` lists it as its decimal text.
 */
export interface ConfigGetAdvancedConfigValueParams {
    /** Entry GUID rendered as `{...}`. */
    guid: string;
}

/**
 * Parameters for `config.getAll`.
 * Read the whole store: every key with its value.
 */
export type ConfigGetAllParams = Record<string, never>;

/**
 * Parameters for `config.getComponents`.
 * List the installed components with their versions.
 */
export type ConfigGetComponentsParams = Record<string, never>;

/**
 * Parameters for `config.getCursorFollowPlayback`.
 * Report foobar2000's "cursor follows playback" setting.
 */
export type ConfigGetCursorFollowPlaybackParams = Record<string, never>;

/**
 * Parameters for `config.getDspPresets`.
 * List the DSP presets stored in the profile. A host without DSP preset support reports an empty list.
 */
export type ConfigGetDspPresetsParams = Record<string, never>;

/**
 * Parameters for `config.getLibraryFilePatterns`.
 * Report where foobar2000 puts newly encoded, copied or moved tracks and album images. A pattern that is not configured is absent, so with neither configured the response is `{ success: true }` alone.
 */
export type ConfigGetLibraryFilePatternsParams = Record<string, never>;

/**
 * Parameters for `config.getLibraryStatus`.
 * Report whether the media library is enabled and loaded, and how many items it holds. The count walks the whole library on every call.
 */
export type ConfigGetLibraryStatusParams = Record<string, never>;

/**
 * Parameters for `config.getOutputConfig`.
 * Report the output settings in effect: module, device, buffer length, bit depth, dithering and fades.
 */
export type ConfigGetOutputConfigParams = Record<string, never>;

/**
 * Parameters for `config.getOutputDevices`.
 * List the devices of every output module and flag the one in effect. These are the devices `output.getDevices` lists, in a flat shape that pairs each with the GUID of its module.
 */
export type ConfigGetOutputDevicesParams = Record<string, never>;

/**
 * Parameters for `config.getPlaybackFollowCursor`.
 * Report foobar2000's "playback follows cursor" setting.
 */
export type ConfigGetPlaybackFollowCursorParams = Record<string, never>;

/**
 * Parameters for `config.getPreferencesPages`.
 * List every registered preferences page and preferences branch: the pages first, then the branches.
 */
export type ConfigGetPreferencesPagesParams = Record<string, never>;

/**
 * Parameters for `config.getPreferencesStandardGuids`.
 * Report the GUIDs of foobar2000's standard preferences parents, which registered pages and branches name as their parent.
 */
export type ConfigGetPreferencesStandardGuidsParams = Record<string, never>;

/**
 * Parameters for `config.getReplaygainMode`.
 * Report the ReplayGain source mode as a number. `replaygain.getMode` reports the same setting by name.
 */
export type ConfigGetReplaygainModeParams = Record<string, never>;

/**
 * Parameters for `config.getVersionInfo`.
 * Report the foobar2000 version, this component's version and the profile folder.
 */
export type ConfigGetVersionInfoParams = Record<string, never>;

/**
 * Parameters for `config.remove`.
 * Delete a key from the store. Deleting a key that is not there succeeds with `existed: false`.
 */
export interface ConfigRemoveParams {
    /** Key to delete. */
    key: string;
}

/**
 * Parameters for `config.resetAdvancedConfig`.
 * Return one Advanced preferences entry to its default value.
 */
export interface ConfigResetAdvancedConfigParams {
    /** Entry GUID rendered as `{...}`. */
    guid: string;
}

/**
 * Parameters for `config.set`.
 * Store a value under a key in this component's own key-value store, replacing any value the key held. The store is kept in foobar2000's configuration store, so it survives a restart. Fails with `OPERATION_FAILED` when the store cannot be read or the value cannot be saved.
 */
export interface ConfigSetParams {
    /** Key to store under. Every page of this component shares one store, so give your keys a prefix of your own. */
    key: string;
    /** Value to store, any JSON value. A top-level `null` counts as missing and is refused, so clear a key with `config.remove`; a `null` inside an object or an array is kept. */
    value: JsonValue;
}

/**
 * Parameters for `config.setActiveDspPreset`.
 * Select a DSP preset by index, which replaces the whole active DSP chain with the preset's; the same operation as `dsp.applyPreset` by index. Nothing selects "no preset" again.
 */
export interface ConfigSetActiveDspPresetParams {
    /** Position of the preset in `config.getDspPresets`. */
    index: number;
}

/**
 * Parameters for `config.setAdvancedConfigValue`.
 * Write one Advanced preferences entry. The value has to suit the entry: a boolean for a `checkbox` or `radio` entry; a string or a number for an `integer` or `string` entry, where a number is stored as its decimal text (an `integer` entry drops the fraction, a `string` entry keeps six decimals). A `branch` or an entry of another kind cannot be written.
 */
export interface ConfigSetAdvancedConfigValueParams {
    /** Entry GUID rendered as `{...}`. */
    guid: string;
    /** Value to write; what each kind of entry accepts is in the method description. */
    value: JsonValue;
}

/**
 * Parameters for `config.setCursorFollowPlayback`.
 * Turn foobar2000's "cursor follows playback" setting on or off.
 */
export interface ConfigSetCursorFollowPlaybackParams {
    /** `true` turns the setting on, `false` turns it off. */
    enabled: boolean;
}

/**
 * Parameters for `config.setOutputBuffer`.
 * Set the output buffer length, in seconds (`bufferLength`) or in milliseconds (`milliseconds`). One of the two is required; `milliseconds` wins when both are given, and `bufferLength` is range-checked even then.
 */
export interface ConfigSetOutputBufferParams {
    /** Buffer length in seconds. Ignored when `milliseconds` is given, but range-checked all the same. */
    bufferLength?: number;
    /** Buffer length in milliseconds; takes precedence over `bufferLength`. */
    milliseconds?: number;
}

/**
 * Parameters for `config.setOutputDevice`.
 * Switch output to a device of an output module; take both GUIDs from `config.getOutputDevices`.
 */
export interface ConfigSetOutputDeviceParams {
    /** GUID of the output module, as `config.getOutputDevices` reports it in `outputId`. */
    outputId: string;
    /** GUID of the device, as `config.getOutputDevices` reports it in `deviceId`. */
    deviceId: string;
}

/**
 * Parameters for `config.setPlaybackFollowCursor`.
 * Turn foobar2000's "playback follows cursor" setting on or off.
 */
export interface ConfigSetPlaybackFollowCursorParams {
    /** `true` turns the setting on, `false` turns it off. */
    enabled: boolean;
}

/**
 * Parameters for `config.setReplaygainMode`.
 * Set the ReplayGain source mode by number (`mode`) or by name (`sourceMode`). One of the two is required; `mode` wins when both are given. `replaygain.setMode` changes the same setting by name.
 */
export interface ConfigSetReplaygainModeParams {
    /** Source mode by number, as `config.getReplaygainMode` reports it; takes precedence over `sourceMode`. */
    mode?: number;
    /** Source mode by name: `none` is `0`, `track` `1`, `album` `2`, and `byPlaybackOrder` or its other spelling `auto` is `3`. Read only when `mode` is absent. */
    sourceMode?: "none" | "track" | "album" | "byPlaybackOrder" | "auto";
}

/**
 * Parameters for `config.showLibraryPreferences`.
 * Open foobar2000's Media Library preferences page.
 */
export type ConfigShowLibraryPreferencesParams = Record<string, never>;

/**
 * Parameters for `console.error`.
 * Write an error line to the foobar2000 console, prefixed with `[WebView][ERROR]`.
 */
export interface ConsoleErrorParams {
    /** What to write. A string is written as it is; any other JSON value as its JSON text. Takes precedence over `args`. */
    message?: JsonValue;
    /** Values to write, separated by spaces: strings as they are, other values as their JSON text. Used only when `message` is absent. */
    args?: JsonValue[];
}

/**
 * Parameters for `console.log`.
 * Write a line to the foobar2000 console, prefixed with `[WebView]`.
 */
export interface ConsoleLogParams {
    /** What to write. A string is written as it is; any other JSON value as its JSON text. Takes precedence over `args`. */
    message?: JsonValue;
    /** Values to write, separated by spaces: strings as they are, other values as their JSON text. Used only when `message` is absent. */
    args?: JsonValue[];
}

/**
 * Parameters for `console.warn`.
 * Write a warning line to the foobar2000 console, prefixed with `[WebView][WARN]`.
 */
export interface ConsoleWarnParams {
    /** What to write. A string is written as it is; any other JSON value as its JSON text. Takes precedence over `args`. */
    message?: JsonValue;
    /** Values to write, separated by spaces: strings as they are, other values as their JSON text. Used only when `message` is absent. */
    args?: JsonValue[];
}

/**
 * Parameters for `cursor.isHidden`.
 * Report whether the calling window's cursor is hidden; `false` when the calling window cannot be resolved.
 */
export type CursorIsHiddenParams = Record<string, never>;

/**
 * Parameters for `cursor.setHidden`.
 * Hide or restore the client-area cursor of the calling window. Only a call that changes the state announces `cursor:hiddenChanged` to that window; repeating the same value reports `changed: false`. Each window keeps its own state.
 */
export interface CursorSetHiddenParams {
    /** `true` hides the cursor, `false` restores it. */
    hidden: boolean;
}

/**
 * Parameters for `dialog.confirm`.
 * Show a modal task dialog with custom buttons. Escape and the close button do not dismiss it.
 */
export interface DialogConfirmParams {
    /** Window title. Defaults to "Confirm", localized to the UI language. */
    title?: string;
    /**
     * Body text.
     * @default ""
     */
    message?: string;
    /**
     * Icon. The task dialog shows `question` with the information icon.
     * @default "question"
     */
    type?: "info" | "warning" | "error" | "question";
    /** Button labels, in order. Defaults to OK and Cancel, localized to the UI language. */
    buttons?: string[];
    /**
     * Index of the initially focused button.
     * @default 0
     */
    defaultButton?: number;
}

/**
 * Parameters for `dialog.openFile`.
 * Show the system dialog for opening files.
 */
export interface DialogOpenFileParams {
    /** Window title. Defaults to "Open File", localized to the UI language. */
    title?: string;
    /**
     * Allow selecting several files.
     * @default false
     */
    multiple?: boolean;
    /**
     * Folder the dialog opens in, every time it is shown. `%music%` expands to the Music folder. Ignored when the path does not resolve to a folder.
     * @default ""
     */
    defaultPath?: string;
    /** File type list. When omitted or empty, the dialog shows a single "All Files" entry. */
    filters?: FileFilter[];
}

/**
 * Parameters for `dialog.openFolder`.
 * Show the system dialog for choosing a folder.
 */
export interface DialogOpenFolderParams {
    /** Window title. Defaults to "Select Folder", localized to the UI language. */
    title?: string;
    /**
     * Folder the dialog opens in, every time it is shown. `%music%` expands to the Music folder. Ignored when the path does not resolve to a folder.
     * @default ""
     */
    defaultPath?: string;
}

/**
 * Parameters for `dialog.saveFile`.
 * Show the system dialog for saving a file.
 */
export interface DialogSaveFileParams {
    /** Window title. Defaults to "Save File", localized to the UI language. */
    title?: string;
    /**
     * File name filled in when the dialog opens.
     * @default ""
     */
    defaultName?: string;
    /** File type list. When omitted or empty, the dialog shows a single "All Files" entry. */
    filters?: FileFilter[];
}

/**
 * Parameters for `discovery.executeContextMenuByPath`.
 * Run a context-menu command addressed by its slash-separated label path, such as `Playback Statistics/Rating/5`. Each segment must match one label exactly after normalization (mnemonic `&`, trailing ellipsis, accelerator text and ASCII case are ignored). A path matching several commands is refused with `INVALID_PARAMS` and the candidates listed; one matching none is `NOT_FOUND`.
 */
export interface DiscoveryExecuteContextMenuByPathParams {
    /** Slash-separated label path of the command, such as `Playback Statistics/Rating/5`. */
    path: string;
    /** Run against this track instead of the current target. A `|subsong:N` suffix selects a subsong; without one the first subsong of the file is used. */
    trackPath?: string;
}

/**
 * Parameters for `discovery.executeContextMenuCommand`.
 * Run a context-menu command by GUID against the playing track, or the active playlist selection when nothing is playing. A command the host would never draw (`FORCE_OFF`) is refused with `NOT_SUPPORTED` unless `force` is set; no target at all is `NO_ACTIVE_ITEM`.
 */
export interface DiscoveryExecuteContextMenuCommandParams {
    /** Command GUID in `{...}` form, as `discovery.getContextMenuCommands` reports it. */
    guid: string;
    /**
     * Dispatch even a command the host would never draw.
     * @default false
     */
    force?: boolean;
}

/**
 * Parameters for `discovery.executeMainMenuCommand`.
 * Run a main-menu command by GUID. A child of a dynamic submenu is addressed by its parent's `guid` plus its own `subGuid`. Fails with `NOT_FOUND` when no command owns the GUID.
 */
export interface DiscoveryExecuteMainMenuCommandParams {
    /** Command GUID in `{...}` form. For a dynamic child, its parent command's GUID. */
    guid: string;
    /** `subGuid` of a dynamic child, as `discovery.getMainMenuCommands` reports it. Empty means absent. */
    subGuid?: string;
}

/**
 * Parameters for `discovery.getAllServices`.
 * Count the discoverable service families in the running process: how many main-menu commands, context-menu commands, groups, input formats, UI elements, DSPs, output backends, preference pages and components are registered. Both menu families are counted through the same walks the listing endpoints use, filtered to what the host would show.
 */
export type DiscoveryGetAllServicesParams = Record<string, never>;

/**
 * Parameters for `discovery.getComponents`.
 * List the installed components.
 */
export type DiscoveryGetComponentsParams = Record<string, never>;

/**
 * Parameters for `discovery.getContextMenuCommands`.
 * List the registered context-menu commands, flattened, with their state for the current target. `enabled` and `checked` are only observable with a track selected or playing; check `stateKnown` before trusting them.
 */
export interface DiscoveryGetContextMenuCommandsParams {
    /**
     * Also list entries the host would not show (`FORCE_OFF`, shortcut-list-only).
     * @default false
     */
    includeHidden?: boolean;
}

/**
 * Parameters for `discovery.getContextMenuTree`.
 * Dump the context menu for the current target as a nested tree, keeping submenus and separators. Depth and children per node are bounded; any clipping is reported through the `truncated` flags rather than left silent.
 */
export type DiscoveryGetContextMenuTreeParams = Record<string, never>;

/**
 * Parameters for `discovery.getDspEntries`.
 * List the registered DSP entries.
 */
export type DiscoveryGetDspEntriesParams = Record<string, never>;

/**
 * Parameters for `discovery.getInputFormats`.
 * List the playable input file types, each with a display name and a filename mask.
 */
export type DiscoveryGetInputFormatsParams = Record<string, never>;

/**
 * Parameters for `discovery.getMainMenuCommands`.
 * List the main-menu commands, flattened, with their label path, GUID and display state. Submenus that components build at runtime (`mainmenu_commands_v2`) are expanded by default, so their child commands appear next to the parent slot.
 */
export interface DiscoveryGetMainMenuCommandsParams {
    /**
     * Expand submenus that components build at runtime. `false` lists the static registry only.
     * @default true
     */
    expandDynamic?: boolean;
    /**
     * Also list entries the host would not show.
     * @default false
     */
    includeHidden?: boolean;
}

/**
 * Parameters for `discovery.getMainMenuGroups`.
 * List the groups main-menu commands are filed under.
 */
export type DiscoveryGetMainMenuGroupsParams = Record<string, never>;

/**
 * Parameters for `discovery.getOutputDevices`.
 * List the registered output backends by GUID.
 */
export type DiscoveryGetOutputDevicesParams = Record<string, never>;

/**
 * Parameters for `discovery.getPreferencePages`.
 * List the pages of the Preferences dialog.
 */
export type DiscoveryGetPreferencePagesParams = Record<string, never>;

/**
 * Parameters for `discovery.getUIElements`.
 * List the registered UI elements.
 */
export type DiscoveryGetUIElementsParams = Record<string, never>;

/**
 * Parameters for `discovery.searchCommands`.
 * Search command names, descriptions and menu paths across both menu families, case-insensitively (ASCII folding only). Each hit carries the same state fields the listing endpoints return, so a caller can tell whether it is invocable without a second call.
 */
export interface DiscoverySearchCommandsParams {
    /** Text to look for in names, descriptions and paths; ASCII case is ignored. */
    query: string;
    /**
     * Also search commands expanded from dynamic submenus.
     * @default true
     */
    expandDynamic?: boolean;
    /**
     * Which menu family to search.
     * @default "all"
     */
    scope?: "all" | "mainmenu" | "contextmenu";
    /**
     * Also search entries the host would not show.
     * @default false
     */
    includeHidden?: boolean;
}

/**
 * Parameters for `dnd.getCapabilities`.
 * Report what this window's drag-drop integration can currently deliver. Fails with `NOT_FOUND` when the calling window has no drag-drop registration.
 */
export type DndGetCapabilitiesParams = Record<string, never>;

/**
 * Parameters for `dnd.getPathsAsync`.
 * Read the real filesystem paths of a drag session from host memory, so the answer does not depend on the delivery order of `dnd:*` messages and stays valid after an `await`. Fails with `NOT_FOUND` when the calling window has no drag-drop registration.
 */
export interface DndGetPathsAsyncParams {
    /** Session to query, from a `dnd:*` payload. Omit it, or pass an empty string, for the session that is active or most recently ended for this window. */
    sessionId?: string;
}

/**
 * Parameters for `dnd.prepareDrag`.
 * Exchange paths for a one-shot token that lets the next drag out of this window carry those files. Parameter shape errors are reported first; then the document origin is checked (`ORIGIN_DENIED`) before any path is resolved or validated, then the window's `dragOut` capability (`NOT_SUPPORTED`), then each path must resolve to a local file (`INVALID_PATH`) and pass path security (`PERMISSION_DENIED`).
 */
export interface DndPrepareDragParams {
    /** Locations to drag out: native paths, `file://` and `file-relative://` URLs, `archive://` / `unpack://` entries, and `path|subsong:N`. File existence is not checked. */
    paths: string[];
}

/**
 * Parameters for `dnd.startDrag`.
 * Not implemented: always fails with `NOT_SUPPORTED`. Use `dnd.prepareDrag` instead.
 */
export type DndStartDragParams = Record<string, never>;

/**
 * Parameters for `dsp.addDsp`.
 * Insert a processor into the active chain with its default settings.
 */
export interface DspAddDspParams {
    /** GUID of an installed processor, as `dsp.getAvailable` reports it. */
    guid: string;
    /**
     * Position to insert at; any negative position, or one past the end, appends.
     * @default -1
     */
    position?: number;
}

/**
 * Parameters for `dsp.applyPreset`.
 * Make a preset the active chain, by index or by name; `index` wins when both are given. Replaces the whole active chain and never writes the preset file. A name matches exactly, case included, and the first preset of that name is taken. Giving neither fails with `INVALID_PARAMS`, an index past the last preset with `INVALID_INDEX`, a name no preset has with `NOT_FOUND`.
 */
export interface DspApplyPresetParams {
    /** Preset index from `dsp.getPresets`. */
    index?: number;
    /** Preset name from `dsp.getPresets`. */
    name?: string;
}

/**
 * Parameters for `dsp.getAvailable`.
 * List every installed DSP processor.
 */
export type DspGetAvailableParams = Record<string, never>;

/**
 * Parameters for `dsp.getChain`.
 * Read the active DSP chain and which preset, if any, it currently corresponds to.
 */
export type DspGetChainParams = Record<string, never>;

/**
 * Parameters for `dsp.getPresets`.
 * List the DSP presets stored in the profile and which one is selected.
 */
export type DspGetPresetsParams = Record<string, never>;

/**
 * Parameters for `dsp.moveDsp`.
 * Move one entry of the active chain to another position, keeping its settings. Prefer this over `setChain` for reordering: `setChain` rebuilds every entry from its default settings.
 */
export interface DspMoveDspParams {
    /** Current position of the entry. */
    from: number;
    /** Final position in the reordered chain. */
    to: number;
}

/**
 * Parameters for `dsp.removeDsp`.
 * Remove one entry from the active chain.
 */
export interface DspRemoveDspParams {
    /** Position of the entry to remove. */
    index: number;
}

/**
 * Parameters for `dsp.setChain`.
 * Replace the whole active chain, each entry built from its processor's default settings. An empty list clears the chain. One entry that does not resolve to an installed processor fails the whole call and leaves the chain untouched.
 */
export interface DspSetChainParams {
    /** The new chain in processing order; empty clears the chain. */
    dsps: DspChainSpec[];
}

/**
 * Parameters for `event.emit`.
 * Broadcast an event to every window. Receivers get an event named `event` whose data is `{ payload, sourceWindowId }`.
 */
export interface EventEmitParams {
    /** Event name the receivers subscribe to. */
    event: string;
    /** Data delivered as `payload`; absent or `null` sends an empty object. */
    payload?: JsonValue;
    /**
     * Leave out the calling window.
     * @default false
     */
    excludeSelf?: boolean;
}

/**
 * Parameters for `event.emitTo`.
 * Send an event to one window. It receives an event named `event` whose data is `{ payload, sourceWindowId }`. A window id no open window has fails with `NOT_FOUND`.
 */
export interface EventEmitToParams {
    /** Event name the receiver subscribes to. */
    event: string;
    /** Id of the window to send to, such as `main` or the `windowId` a port reported. */
    targetWindowId: string;
    /** Data delivered as `payload`; absent or `null` sends an empty object. */
    payload?: JsonValue;
}

/**
 * Parameters for `file.cancelOp`.
 * Stop an operation started by `file.copyAsync`, `file.moveAsync` or `file.deleteAsync`. The entries it has not reached are reported as `skipped` / `cancelled`, and the run still ends with `file:opComplete`.
 */
export interface FileCancelOpParams {
    /** Id from the receipt of `file.copyAsync`, `file.moveAsync` or `file.deleteAsync`. */
    operationId: string;
}

/**
 * Parameters for `file.copy`.
 * Copy a file or a whole directory tree, blocking until the copy is done. A source that does not exist fails with `NOT_FOUND`. Prefer `file.copyAsync` for anything large.
 */
export interface FileCopyParams {
    /** File or directory to copy; a directory is copied with everything below it. */
    source: string;
    /** Target path. Its parent directory has to exist already. */
    destination: string;
    /**
     * Replace files that already exist at the target. Otherwise they are left as they are and the call still succeeds.
     * @default false
     */
    overwrite?: boolean;
}

/**
 * Parameters for `file.copyAsync`.
 * Start a cancellable batch copy on a worker thread and return a receipt at once. The results arrive in batches on `file:opProgress`, followed by one `file:opComplete`. At most 8 batch operations run at a time across the process; beyond that the call fails with `OPERATION_FAILED`.
 */
export interface FileCopyAsyncParams {
    /** Entries to copy, reported one result each, in this order. An entry that is not an object holding exactly the string members `source` and `destination` fails the call with `INVALID_PARAMS`. Every path is checked before anything starts, `source` for reading and `destination` for writing; one refused path, an empty string included, fails the whole call with `PERMISSION_DENIED` and no `operationId`. */
    items: FileOpEntry[];
    /**
     * Replace an existing file destination instead of reporting the entry as `skipped` / `already-exists`. A directory copied onto an existing directory is merged into it either way; the files already inside are skipped, without being reported, unless this is set.
     * @default false
     */
    overwrite?: boolean;
}

/**
 * Parameters for `file.delete`.
 * Delete a file or directory, to the Recycle Bin unless `moveToTrash` is `false`. A path that does not exist fails with `NOT_FOUND`.
 */
export interface FileDeleteParams {
    /** File or directory to delete. */
    path: string;
    /**
     * Send it to the Recycle Bin. `false` deletes it permanently, which fails on a directory that is not empty.
     * @default true
     */
    moveToTrash?: boolean;
}

/**
 * Parameters for `file.deleteAsync`.
 * Start a cancellable batch delete, with the same receipt and events as `file.copyAsync`; its results carry no `destination`. Recycle Bin deletes run on the main thread in batches of 16, permanent deletes on a worker thread.
 */
export interface FileDeleteAsyncParams {
    /** Paths to delete, reported one result each, in this order. An entry that is not a string fails the call with `INVALID_PARAMS`. Every path is checked for writing before anything starts; one refused path, an empty string included, fails the whole call with `PERMISSION_DENIED` and no `operationId`. */
    paths: string[];
    /**
     * Send each entry to the Recycle Bin. `false` deletes permanently and removes directories that are not empty, which the synchronous `file.delete` refuses to do.
     * @default true
     */
    moveToTrash?: boolean;
}

/**
 * Parameters for `file.exists`.
 * Check whether a path exists and whether it is a file or a directory.
 */
export interface FileExistsParams {
    /** Path to check. */
    path: string;
}

/**
 * Parameters for `file.getInfo`.
 * Describe a file or directory. A path that does not exist is a success with `exists: false` and no other field.
 */
export interface FileGetInfoParams {
    /** File or directory to describe. */
    path: string;
}

/**
 * Parameters for `file.list`.
 * List the files and subdirectories of a directory. A path that does not exist fails with `NOT_FOUND`, and one that is not a directory with `INVALID_PATH`.
 */
export interface FileListParams {
    /** Directory to list. */
    path: string;
    /**
     * Filter for files; directories are always listed. Only `*`, `*.*` and a single extension such as `*.flac` are understood, and nothing is refused: a pattern that does not start with `*.` (`song*`, `?.txt`) matches every file, while a `*.ext` pattern is compared, ignoring case, with the text from the file name's last dot on, so `*.{flac,mp3}` matches nothing and no `*.ext` matches a file without an extension.
     * @default "*"
     */
    pattern?: string;
    /**
     * Walk the subdirectories as well; the entries are then full paths instead of names. A subdirectory that cannot be read makes the call fail with `OPERATION_FAILED`.
     * @default false
     */
    recursive?: boolean;
}

/**
 * Parameters for `file.mkdir`.
 * Create a directory together with any missing parents. A directory that already exists is a success with `created: false`; a file at the path fails with `INVALID_PATH`.
 */
export interface FileMkdirParams {
    /** Directory to create. */
    path: string;
}

/**
 * Parameters for `file.move`.
 * Move a file or directory, blocking until the move is done. A file can move to another volume; a directory cannot, and that fails with `NOT_SUPPORTED` and `details.reason: "cross-volume"`. A source that does not exist fails with `NOT_FOUND`.
 */
export interface FileMoveParams {
    /** File or directory to move. */
    source: string;
    /** Target path. An existing file there is replaced; moving a file onto an existing directory, or under a parent directory that does not exist, fails. */
    destination: string;
}

/**
 * Parameters for `file.moveAsync`.
 * Start a cancellable batch move on a worker thread, with the same receipt and events as `file.copyAsync`. Within one volume a move is a rename; across volumes an entry is copied and its source then deleted.
 */
export interface FileMoveAsyncParams {
    /** Entries to move, reported one result each, in this order. An entry that is not an object holding exactly the string members `source` and `destination` fails the call with `INVALID_PARAMS`. Every path is checked for writing before anything starts, `source` included, since a move deletes it; one refused path, an empty string included, fails the whole call with `PERMISSION_DENIED` and no `operationId`. */
    items: FileOpEntry[];
    /**
     * Replace an existing file destination instead of reporting the entry as `skipped` / `already-exists`; the synchronous `file.move` always replaces one. An existing directory destination is never replaced: Windows cannot swap a directory in place, so on the same volume such an entry ends as `skipped` or `failed`.
     * @default false
     */
    overwrite?: boolean;
}

/**
 * Parameters for `file.read`.
 * Read a file as text, or as Base64 with `encoding: "binary"`. A path that does not exist fails with `NOT_FOUND`, and one that is not a regular file with `INVALID_PATH`.
 */
export interface FileReadParams {
    /** File to read. */
    path: string;
    /**
     * Only exactly `binary` reads the raw bytes and returns them as Base64; any other value reads the file as text.
     * @default "utf-8"
     */
    encoding?: string;
}

/**
 * Parameters for `file.rename`.
 * Rename a file or directory within its parent directory. A path that does not exist fails with `NOT_FOUND`, and a name that is already taken with `OPERATION_FAILED`.
 */
export interface FileRenameParams {
    /** File or directory to rename. */
    path: string;
    /** New name, kept in the same directory. A name containing `/` or `\` fails with `INVALID_PARAMS`. */
    newName: string;
}

/**
 * Parameters for `file.write`.
 * Write text, or bytes decoded from Base64, to a file. Missing parent directories are created first.
 */
export interface FileWriteParams {
    /** File to write; missing parent directories are created. */
    path: string;
    /**
     * What to write. With `encoding: "binary"` a value starting with `base64:` is decoded and the bytes are written: characters outside the Base64 alphabet are skipped and decoding stops at the first `=`. Any other value, plain Base64 or a Data URL included, is written as it is. An empty string empties the file unless `append` is set.
     * @default ""
     */
    content?: string;
    /**
     * Only exactly `binary` changes anything: the file is written in binary mode and a `content` starting with `base64:` is decoded. Any other value writes text.
     * @default "utf-8"
     */
    encoding?: string;
    /**
     * Add to the end of the file instead of replacing its content.
     * @default false
     */
    append?: boolean;
    /**
     * Write a temporary file next to the target, flush it to disk, then replace the target with one rename: readers see the old content or the new content, never a partly written file. If the replacement fails, for example because another program has the target open, the call fails and the target keeps its old content. The temporary file is named `.~<process id>-<sequence>.tmp`, and its path has to fit in 259 characters as well. Cannot be combined with `append`; passing both fails with `INVALID_PARAMS`.
     * @default false
     */
    atomic?: boolean;
}

/**
 * Parameters for `http.abort`.
 * Cancel an async request or download. The request then ends with its event reporting `CANCELLED`.
 */
export interface HttpAbortParams {
    /** Id from the receipt of the request or download. */
    requestId: string;
}

/**
 * Parameters for `http.delete`.
 * Send a DELETE request; otherwise the same as `http.get`.
 */
export interface HttpDeleteParams {
    /** Request URL; only `http` and `https` are allowed. */
    url: string;
    /** Request body: a string is sent as it is, an object or array as its JSON text, and any other value as an empty body. */
    body?: JsonValue;
    /** Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. */
    headers?: Record<string, string>;
    /**
     * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults.
     * @default 30000
     */
    timeout?: number;
    /**
     * Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in.
     * @default true
     */
    async?: boolean;
    /**
     * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is.
     * @default "follow"
     */
    redirect?: "follow" | "error" | "manual";
    /**
     * `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`, return it Base64-encoded.
     * @default "text"
     */
    responseType?: "text" | "base64" | "arraybuffer" | "binary";
    /**
     * Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well.
     * @default false
     */
    insecureTls?: boolean;
}

/**
 * Parameters for `http.download`.
 * Download a URL into a file, creating the missing folders. By default the call blocks until the file is written; with `async: true` it returns a receipt at once and the outcome arrives as `http:downloadComplete`. Downloads over 500 MB fail with `OPERATION_FAILED`.
 */
export interface HttpDownloadParams {
    /** URL to download; only `http` and `https` are allowed. */
    url: string;
    /** File to write; path variables are expanded and missing folders are created. An existing file is replaced. */
    saveTo: string;
    /** Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. */
    headers?: Record<string, string>;
    /**
     * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults.
     * @default 60000
     */
    timeout?: number;
    /**
     * Return a receipt at once and deliver the outcome as `http:downloadComplete`; `false` blocks until the file is written.
     * @default false
     */
    async?: boolean;
    /**
     * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` saves the redirect response as it is.
     * @default "follow"
     */
    redirect?: "follow" | "error" | "manual";
    /**
     * Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well.
     * @default false
     */
    insecureTls?: boolean;
}

/**
 * Parameters for `http.get`.
 * Send a GET request. By default the call returns a receipt at once and the response arrives as `http:response` carrying the same `requestId`; with `async: false` the host blocks until the response is in and returns it. Requests to a local or private network address fail with `PERMISSION_DENIED` unless the host setting allows them, checked again at every redirect.
 */
export interface HttpGetParams {
    /** Request URL; only `http` and `https` are allowed. */
    url: string;
    /** Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. */
    headers?: Record<string, string>;
    /**
     * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults.
     * @default 30000
     */
    timeout?: number;
    /**
     * Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in.
     * @default true
     */
    async?: boolean;
    /**
     * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is.
     * @default "follow"
     */
    redirect?: "follow" | "error" | "manual";
    /**
     * `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`, return it Base64-encoded.
     * @default "text"
     */
    responseType?: "text" | "base64" | "arraybuffer" | "binary";
    /**
     * Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well.
     * @default false
     */
    insecureTls?: boolean;
}

/**
 * Parameters for `http.head`.
 * Send a HEAD request; otherwise the same as `http.get`. The response body is always empty text, and `contentLength` is added when the server sends a numeric `Content-Length`.
 */
export interface HttpHeadParams {
    /** Request URL; only `http` and `https` are allowed. */
    url: string;
    /** Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. */
    headers?: Record<string, string>;
    /**
     * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults.
     * @default 30000
     */
    timeout?: number;
    /**
     * Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in.
     * @default true
     */
    async?: boolean;
    /**
     * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is.
     * @default "follow"
     */
    redirect?: "follow" | "error" | "manual";
    /**
     * Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well.
     * @default false
     */
    insecureTls?: boolean;
}

/**
 * Parameters for `http.patch`.
 * Send a PATCH request; otherwise the same as `http.get`.
 */
export interface HttpPatchParams {
    /** Request URL; only `http` and `https` are allowed. */
    url: string;
    /** Request body: a string is sent as it is, an object or array as its JSON text, and any other value as an empty body. */
    body?: JsonValue;
    /** Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. */
    headers?: Record<string, string>;
    /**
     * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults.
     * @default 30000
     */
    timeout?: number;
    /**
     * Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in.
     * @default true
     */
    async?: boolean;
    /**
     * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is.
     * @default "follow"
     */
    redirect?: "follow" | "error" | "manual";
    /**
     * `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`, return it Base64-encoded.
     * @default "text"
     */
    responseType?: "text" | "base64" | "arraybuffer" | "binary";
    /**
     * Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well.
     * @default false
     */
    insecureTls?: boolean;
}

/**
 * Parameters for `http.post`.
 * Send a POST request; otherwise the same as `http.get`.
 */
export interface HttpPostParams {
    /** Request URL; only `http` and `https` are allowed. */
    url: string;
    /** Request body: a string is sent as it is, an object or array as its JSON text, and any other value as an empty body. */
    body?: JsonValue;
    /** Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. */
    headers?: Record<string, string>;
    /**
     * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults.
     * @default 30000
     */
    timeout?: number;
    /**
     * Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in.
     * @default true
     */
    async?: boolean;
    /**
     * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is.
     * @default "follow"
     */
    redirect?: "follow" | "error" | "manual";
    /**
     * `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`, return it Base64-encoded.
     * @default "text"
     */
    responseType?: "text" | "base64" | "arraybuffer" | "binary";
    /**
     * Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well.
     * @default false
     */
    insecureTls?: boolean;
}

/**
 * Parameters for `http.put`.
 * Send a PUT request; otherwise the same as `http.get`.
 */
export interface HttpPutParams {
    /** Request URL; only `http` and `https` are allowed. */
    url: string;
    /** Request body: a string is sent as it is, an object or array as its JSON text, and any other value as an empty body. */
    body?: JsonValue;
    /** Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. */
    headers?: Record<string, string>;
    /**
     * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults.
     * @default 30000
     */
    timeout?: number;
    /**
     * Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in.
     * @default true
     */
    async?: boolean;
    /**
     * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is.
     * @default "follow"
     */
    redirect?: "follow" | "error" | "manual";
    /**
     * `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`, return it Base64-encoded.
     * @default "text"
     */
    responseType?: "text" | "base64" | "arraybuffer" | "binary";
    /**
     * Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well.
     * @default false
     */
    insecureTls?: boolean;
}

/**
 * Parameters for `jitQueue.clear`.
 * Empty the buffer and return the session to idle.
 */
export type JitQueueClearParams = Record<string, never>;

/**
 * Parameters for `jitQueue.enqueueNext`.
 * Preload the next track into the buffer, in answer to `jitQueue:needNext`. Refused with `NO_ACTIVE_ITEM` unless a JIT session is playing or waiting for its next track.
 */
export interface JitQueueEnqueueNextParams {
    /** Caller-assigned identifier of the track. */
    trackId: string;
    /**
     * Display title.
     * @default ""
     */
    title?: string;
    /** Stream URL or file path. At most 2048 characters. */
    url: string;
}

/**
 * Parameters for `jitQueue.getState`.
 * Report the JIT session state.
 */
export type JitQueueGetStateParams = Record<string, never>;

/**
 * Parameters for `jitQueue.notifyEmpty`.
 * Tell the host the page has no more tracks to offer; the session moves to `Exhausted` and `jitQueue:listExhausted` is emitted.
 */
export type JitQueueNotifyEmptyParams = Record<string, never>;

/**
 * Parameters for `jitQueue.playNow`.
 * Start a new JIT session with this track: the buffer is cleared and playback begins. The host detects the URL kind itself (`http(s)://` streams, Windows or UNC paths are local).
 */
export interface JitQueuePlayNowParams {
    /** Caller-assigned identifier of the track, echoed in the session state and events. */
    trackId: string;
    /**
     * Display title.
     * @default ""
     */
    title?: string;
    /** Stream URL or file path. At most 2048 characters. */
    url: string;
}

/**
 * Parameters for `jitQueue.preloadBatch`.
 * Insert a batch of tracks into the shadow playlist, starting playback from `startIndex` when the session is not already playing.
 */
export interface JitQueuePreloadBatchParams {
    /** Track URLs or `path|subsong:N` values. Entries longer than 2048 characters are dropped and counted in `invalidCount`; at most 10000 entries may remain. */
    urls?: string[];
    /**
     * Which entry to start playing from.
     * @default 0
     */
    startIndex?: number;
    /**
     * Empty the shadow playlist before inserting. `false` appends.
     * @default true
     */
    replace?: boolean;
}

/**
 * Parameters for `jitQueue.skip`.
 * Skip to the next buffered track. Refused with `NO_ACTIVE_ITEM` while no JIT session is active.
 */
export type JitQueueSkipParams = Record<string, never>;

/**
 * Parameters for `jitQueue.stop`.
 * Stop playback, optionally keeping the buffer.
 */
export interface JitQueueStopParams {
    /**
     * Also empty the buffer.
     * @default true
     */
    clearBuffer?: boolean;
}

/**
 * Parameters for `keyboard.getRegisteredHotkeys`.
 * List the registered hotkeys followed by the shortcuts.
 */
export type KeyboardGetRegisteredHotkeysParams = Record<string, never>;

/**
 * Parameters for `keyboard.registerHotkey`.
 * Register a system-wide hotkey on the main window. When it is pressed the registering window receives `keyboard:hotkey` with `{ id, key, action }`. Windows refuses a combination another program already holds.
 */
export interface KeyboardRegisterHotkeyParams {
    /** The combination to register. */
    key: string;
    /** Action name reported back in the `keyboard:hotkey` payload. */
    action: string;
    /**
     * Recorded on the entry and reported by `getRegisteredHotkeys`; the hotkey is registered system-wide either way.
     * @default true
     */
    global?: boolean;
}

/**
 * Parameters for `keyboard.registerShortcut`.
 * Store a key combination as a WebView-local shortcut under the given action name. It is listed by `getRegisteredHotkeys` with `id` `0` and removed by `unregisterHotkey` with the key.
 */
export interface KeyboardRegisterShortcutParams {
    /** The combination to store. */
    key: string;
    /** Action name stored with the shortcut. */
    action: string;
}

/**
 * Parameters for `keyboard.unregisterHotkey`.
 * Remove a hotkey by its id, or a hotkey or shortcut by its key string; exactly one of the two must be given.
 */
export interface KeyboardUnregisterHotkeyParams {
    /** Id from `registerHotkey`. */
    id?: number;
    /** The key string the hotkey or shortcut was registered with. */
    key?: string;
}

/**
 * Parameters for `library.addToPlaylist`.
 * Append tracks to a playlist by path. A path does not have to be in the library, and a file that does not exist is added all the same. A locked playlist fails with `LOCKED` and nothing is added.
 */
export interface LibraryAddToPlaylistParams {
    /** Paths to add, in this order; a `|subsong:N` suffix selects a subsong. */
    paths: string[];
    /** Index of the target playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `library.browseDirectory`.
 * List the library by path prefix: the folders one level below `path` and, with `includeFiles`, every track under it at any depth. The prefix is matched against the paths as foobar2000 stores them, so a local folder has to be written as `file://` followed by its path; a plain absolute path matches nothing and succeeds with empty lists. `library.browseTree` walks the real library roots instead. Fails with `LIBRARY_DISABLED` when the library is disabled.
 */
export interface LibraryBrowseDirectoryParams {
    /**
     * Path prefix, compared with ASCII letters case-insensitive; empty matches every track.
     * @default ""
     */
    path?: string;
    /**
     * Add every track under `path` as `files`. On by default, so an empty `path` returns the whole library.
     * @default true
     */
    includeFiles?: boolean;
}

/**
 * Parameters for `library.browseTree`.
 * One folder under a library root: its subfolders and, with `includeFiles`, its tracks. Reads the directory tree index `library.getRoots` uses, building it first when needed. An unknown `rootId` or `pathId` fails with `NOT_FOUND`; a failed index build fails with `OPERATION_FAILED`.
 */
export interface LibraryBrowseTreeParams {
    /** `id` of a root from `library.getRoots`, compared case-insensitively. */
    rootId: string;
    /**
     * `pathId` of the folder to open, as a `directories` entry reports it; empty opens the root. Folder names are separated by `/`, so a path written with `\` is not found.
     * @default ""
     */
    pathId?: string;
    /**
     * Add the folder's tracks as `files`.
     * @default false
     */
    includeFiles?: boolean;
    /**
     * With `includeFiles`, add the tracks of every folder below as well.
     * @default false
     */
    recursiveFiles?: boolean;
}

/**
 * Parameters for `library.getAlbums`.
 * Albums of the whole library, grouped by album name plus album artist; tracks without an `album` tag are skipped. A complete list (`offset` 0, no `includeTracks`, every album within `limit`) is kept until the library changes, and a later call with the same `query`, `sort` and `includeCover` from `offset` 0 without `includeTracks` is answered from it. The grouping behind the list is kept the same way and shared with `library.getAlbumTracks`, so other calls do not read the library again either until it changes. With the library disabled it succeeds with no albums.
 */
export interface LibraryGetAlbumsParams {
    /**
     * `name`, `artist` (the album artist), `year` or `trackCount`; any other value sorts by name.
     * @default "name"
     */
    sort?: string;
    /**
     * Keep only albums whose name or artist contains this text, compared case-insensitively for ASCII letters; empty keeps every album.
     * @default ""
     */
    query?: string;
    /**
     * Albums to skip.
     * @default 0
     */
    offset?: number;
    /**
     * Most albums to return.
     * @default 100
     */
    limit?: number;
    /**
     * Add each album's tracks as `tracks`.
     * @default false
     */
    includeTracks?: boolean;
    /**
     * Add each album's front cover as `coverDataUrl`, read from its first track.
     * @default false
     */
    includeCover?: boolean;
    /**
     * Largest cover to inline, in KiB; a larger cover is left out. `0` or less inlines any size.
     * @default 500
     */
    coverMaxSize?: number;
    /**
     * Answer from the kept list when there is one; `false` always scans the library, and the result is still kept.
     * @default true
     */
    useCache?: boolean;
}

/**
 * Parameters for `library.getAlbumTracks`.
 * The tracks of one `library.getAlbums` row, named by the row's `name` and `albumArtist`. Tracks are grouped exactly as `library.getAlbums` groups them, so `total` equals the row's `trackCount`. They are sorted by disc number, then track number, then library order. The grouping is kept until the library changes and `library.getAlbums` builds and reads the same one, so a call after it, or after an earlier call, does not read the library again. A name and album artist that no row has, and a disabled library, succeed with no tracks and no `row`.
 */
export interface LibraryGetAlbumTracksParams {
    /** The row's `name`: the first `album` value of its tracks, compared byte for byte. */
    album: string;
    /** The row's `albumArtist`: the first `album artist` value of its tracks, or their first `artist` value when they have no `album artist`, or `""` when they have neither. Compared byte for byte, so the row's `artist` is not a substitute. */
    albumArtist: string;
}

/**
 * Parameters for `library.getAll`.
 * Tracks of the whole library in library order, one page at a time. A request from `offset` 0 that covers every track is kept until the library changes, and a later call from `offset` 0 with `useCache` is answered from it. With `asyncResult`, such a request, when it is not answered from the kept list, is built off the main thread: the call answers `{ pending: true, requestId }` at once and the page arrives as the `library:getAllResult` event on the calling window.
 */
export interface LibraryGetAllParams {
    /**
     * Tracks to skip.
     * @default 0
     */
    offset?: number;
    /**
     * Most tracks to return.
     * @default 100
     */
    limit?: number;
    /**
     * From `offset` 0, answer from the kept list when there is one. A request from `offset` 0 that covers every track is kept either way.
     * @default true
     */
    useCache?: boolean;
    /**
     * Build a request from `offset` 0 that covers every track off the main thread and deliver it as the `library:getAllResult` event; takes effect only with `useCache`, and not when the kept list answers.
     * @default false
     */
    asyncResult?: boolean;
}

/**
 * Parameters for `library.getArtistAlbums`.
 * The albums an artist appears on. `trackCount`, `duration` and `discCount` count only the tracks of this artist, so an album the artist appears on only partly reports smaller figures than in `library.getAlbums`. Rows are grouped by album name alone; tracks without an `album` tag fall under `(Unknown Album)`. Fails with `LIBRARY_DISABLED` when the library is disabled.
 */
export interface LibraryGetArtistAlbumsParams {
    /** Artist name, compared byte for byte against each value of the `artist` tag, so a name from `library.getArtists` matches even when it is not the first of several credited artists. */
    artist: string;
    /**
     * Most albums to return, applied after grouping; there is no offset.
     * @default 100
     */
    limit?: number;
    /**
     * `name`, `artist`, `year` or `trackCount`, as in `library.getAlbums`; any other value sorts by name.
     * @default "name"
     */
    sort?: string;
    /**
     * `exact` needs a whole tag value to equal `artist`; `substring` matches a tag value that contains it, and then a lowercase letter in `artist` matches either case while an uppercase one matches only uppercase.
     * @default "exact"
     */
    match?: "exact" | "substring";
}

/**
 * Parameters for `library.getArtists`.
 * Every credited artist with participation counts: each value of a multi-value `artist` gets its own row. The scan is kept until the library changes. Fails with `LIBRARY_DISABLED` when the library is disabled.
 */
export interface LibraryGetArtistsParams {
    /**
     * `name`, `trackCount` or `albumCount` (both largest first); any other value keeps name order.
     * @default "name"
     */
    sort?: string;
    /**
     * Most artists to return.
     * @default 1000
     */
    limit?: number;
    /**
     * Add each artist's albums as `albums`.
     * @default false
     */
    includeAlbums?: boolean;
}

/**
 * Parameters for `library.getArtistTracks`.
 * Tracks an artist is credited on, in library order. `artist` is matched as `library.getArtistAlbums` matches it under `match: 'exact'`. An empty `artist` and a disabled library both succeed with no tracks.
 */
export interface LibraryGetArtistTracksParams {
    /**
     * Artist name, compared byte for byte against each value of the `artist` tag; empty, the call succeeds with no tracks.
     * @default ""
     */
    artist?: string;
    /**
     * Most tracks to return; the first ones in library order are kept.
     * @default 500
     */
    limit?: number;
}

/**
 * Parameters for `library.getByPath`.
 * Look up one file in the library and return its main fields as a flat object. A file that is not in the library succeeds with `found: false`.
 */
export interface LibraryGetByPathParams {
    /** File path. It is looked up as the file's first subsong; a `|subsong:N` suffix is not recognized. */
    path: string;
}

/**
 * Parameters for `library.getCacheStats`.
 * Counters of the host's library caches and of the directory tree index, for diagnostics.
 */
export type LibraryGetCacheStatsParams = Record<string, never>;

/**
 * Parameters for `library.getCount`.
 * Number of tracks in the library.
 */
export type LibraryGetCountParams = Record<string, never>;

/**
 * Parameters for `library.getFieldValues`.
 * Every distinct value of one tag across the library with its track count, most used first. Fails with `LIBRARY_DISABLED` when the library is disabled.
 */
export interface LibraryGetFieldValuesParams {
    /** Tag name, such as `genre`. */
    field: string;
    /**
     * Splits each tag value into several values at this string, trimming spaces and tabs around each; empty, values are taken whole.
     * @default ""
     */
    separator?: string;
    /**
     * Most values to return.
     * @default 5000
     */
    limit?: number;
}

/**
 * Parameters for `library.getGenres`.
 * Every genre in the library with its track count, ordered by name. Every value of a multi-value `genre` gets its own entry, and a track tagged with several genres is counted under each. Fails with `LIBRARY_DISABLED` when the library is disabled.
 */
export type LibraryGetGenresParams = Record<string, never>;

/**
 * Parameters for `library.getRandomTracks`.
 * Tracks drawn at random from the whole library without repeats, a new draw on every call. A disabled or empty library succeeds with no tracks.
 */
export interface LibraryGetRandomTracksParams {
    /**
     * Tracks to draw; at most the library size is returned.
     * @default 10
     */
    count?: number;
}

/**
 * Parameters for `library.getRecentlyAdded`.
 * The newest tracks of the library. `added` orders by the `%added%` field of foo_playcount; when no track has it, the call orders by file modification time instead and says so in `fallback`. The library is walked on every call.
 */
export interface LibraryGetRecentlyAddedParams {
    /**
     * Most tracks to return.
     * @default 50
     */
    limit?: number;
    /**
     * `added` orders by `%added%`, newest first, with tracks that lack it last; `modified` orders by file modification time, newest first.
     * @default "added"
     */
    sortBy?: "added" | "modified";
}

/**
 * Parameters for `library.getRoots`.
 * The library's root folders. Only tracks that resolve to a stable local path count toward the roots; `http://`, `file-relative://`, `unpack://`, `archive://` and similar ones are counted in `skippedTracks`. The first call builds the index synchronously and later calls reuse it until the library changes or `library.invalidateCache` is called. With the library disabled it succeeds with `enabled: false` and no roots; a failed build fails with `OPERATION_FAILED`.
 */
export type LibraryGetRootsParams = Record<string, never>;

/**
 * Parameters for `library.getStats`.
 * Aggregate counts over the whole library. With the library disabled every count is `0`. The library is walked on every call.
 */
export type LibraryGetStatsParams = Record<string, never>;

/**
 * Parameters for `library.getStatus`.
 * Whether the library is enabled and how many tracks it holds. The answer is kept until the library changes; with the library disabled nothing is kept.
 */
export type LibraryGetStatusParams = Record<string, never>;

/**
 * Parameters for `library.invalidateCache`.
 * Drop the host's cached library results and the directory tree index; the next call to an endpoint that uses them rebuilds them. They are also dropped whenever the library changes.
 */
export type LibraryInvalidateCacheParams = Record<string, never>;

/**
 * Parameters for `library.isEnabled`.
 * Whether the foobar2000 media library is enabled, that is, whether any library folder is configured.
 */
export type LibraryIsEnabledParams = Record<string, never>;

/**
 * Parameters for `library.query`.
 * Tracks matching a foobar2000 query, optionally sorted by `sort`, up to `limit`. A query the parser rejects fails with `INVALID_PARAMS` and `details.param` `query`, and so does one carrying `SORT BY`; many malformed queries are not rejected but simply match nothing. The rows are written off the main thread. Fails with `LIBRARY_DISABLED` when the library is disabled.
 */
export interface LibraryQueryParams {
    /** foobar2000 query expression. */
    query: string;
    /**
     * Title Formatting expression to sort the matches by before `limit` applies; empty keeps library order, and an expression that fails to compile is ignored.
     * @default ""
     */
    sort?: string;
    /**
     * Most rows to return; `total` still counts every match.
     * @default 100
     */
    limit?: number;
    /** Keys each row carries; omitted, every key of a library track row. Names are matched case-sensitively against those keys: `index`, `handle`, `title`, `artist`, `artists`, `album`, `albumArtist`, `albumArtists`, `genre`, `date`, `trackNumber`, `discNumber`, `duration`, `path`, `absolutePath`, `fileSize`, `bitrate`, `sampleRate`, `channels`, `codec`, `subsong` and `rating`. An unknown name fails with `INVALID_PARAMS` and is listed in `details.unknownFields`. */
    fields?: string[];
}

/**
 * Parameters for `library.refresh`.
 * Same as `library.rescan`.
 */
export type LibraryRefreshParams = Record<string, never>;

/**
 * Parameters for `library.rescan`.
 * Ask foobar2000 to rescan the library folders by calling `library_manager::rescan()`, which the foobar2000 SDK marks as obsolete and not to be called. The library follows file changes on its own.
 */
export type LibraryRescanParams = Record<string, never>;

/**
 * Parameters for `library.search`.
 * One page of the tracks matching a foobar2000 query, in library order: a `SORT BY` clause is accepted but not applied. A query the parser rejects fails with `INVALID_PARAMS` and `details.param` `query`. An empty `query` and a disabled library both succeed with no tracks. The rows are written off the main thread.
 */
export interface LibrarySearchParams {
    /**
     * foobar2000 query expression; empty, the call succeeds with no tracks.
     * @default ""
     */
    query?: string;
    /**
     * Matches to skip.
     * @default 0
     */
    offset?: number;
    /**
     * Most rows to return.
     * @default 100
     */
    limit?: number;
    /** Keys each row carries, as for `library.query`. */
    fields?: string[];
}

/**
 * Parameters for `log.clear`.
 * Empty `webview_ui.log`.
 */
export type LogClearParams = Record<string, never>;

/**
 * Parameters for `log.read`.
 * Read the last lines of `webview_ui.log`. A file that does not exist yet reads as empty.
 */
export interface LogReadParams {
    /**
     * How many lines to return from the end of the file.
     * @default 100
     */
    lines?: number;
}

/**
 * Parameters for `log.write`.
 * Write a line to a log file in the foobar2000 profile directory: `webview_ui.log`, or the file named by `file`.
 */
export interface LogWriteParams {
    /** What to write. A string is written as it is; any other JSON value as its JSON text. Takes precedence over `args`. */
    message?: JsonValue;
    /** Values to write, separated by spaces: strings as they are, other values as their JSON text. Used only when `message` is absent. */
    args?: JsonValue[];
    /**
     * Level written in brackets before the message, upper-cased.
     * @default "info"
     */
    level?: string;
    /**
     * Append to the file; `false` replaces its content with this line.
     * @default true
     */
    append?: boolean;
    /**
     * Prefix the line with the local time, as `[YYYY-MM-DD HH:MM:SS.mmm]`.
     * @default true
     */
    timestamp?: boolean;
    /** Name of another file in the profile directory. It must be a plain file name ending in `.log` or `.txt` and not a Windows device name such as `CON`; any other value is ignored and the line goes to `webview_ui.log`. */
    file?: string;
}

/**
 * Parameters for `lyrics.exists`.
 * List the known lyrics tags and sidecar files that exist for a track. File candidates and access checks match `lyrics.get`, but contents are not read: empty tags and files still count as existing.
 */
export interface LyricsExistsParams {
    /** Track path; a `|subsong:N` suffix selects a subsong. */
    path: string;
    /** Exact file name, including any extension, next to the audio. Replaces the automatic file candidates; embedded tags are still checked. Empty means automatic lookup. The same Windows filename rules and `INVALID_PARAMS` failure as `lyrics.get` apply. */
    filename?: string;
}

/**
 * Parameters for `lyrics.get`.
 * Read non-empty lyrics from the five known lyrics tags, then files next to the audio. Automatic lookup tries the audio file's stem only for a verified single-track file, followed by `<first artist> - <title>`; subsongs use only the latter. Unknown input layouts skip the audio-stem candidate. No numbered or shared album fallback is used. Results are cached for 1200 ms per track, filename and filters. Successful file saves and metadata changes invalidate the cache.
 */
export interface LyricsGetParams {
    /** Track path; a `|subsong:N` suffix selects a subsong. Omit it for the playing track; with nothing playing the call fails with `NO_ACTIVE_ITEM`. The playing track keeps its subsong. */
    path?: string;
    /** Exact file name, including any extension, next to the audio. A non-empty value replaces all automatic file candidates and ignores `format`; `source` and `type` still apply. Empty means automatic lookup. Must be a valid single Windows filename, without path syntax, device names or trailing dots/spaces; invalid names fail with `INVALID_PARAMS`. */
    filename?: string;
    /**
     * Where to look: the embedded tags, the sidecar files, or both in that order.
     * @default "any"
     */
    source?: "embedded" | "file" | "any";
    /**
     * Keep only synced lyrics (with LRC timestamps) or only unsynced ones.
     * @default "any"
     */
    type?: "synced" | "unsynced" | "any";
    /**
     * Sidecar extension to try; `.lrc` candidates are tried before `.txt` with `any`. Applies to automatic sidecar lookup only; a non-empty `filename` takes precedence.
     * @default "any"
     */
    format?: "lrc" | "txt" | "any";
}

/**
 * Parameters for `lyrics.save`.
 * Save lyrics to a sidecar file, an embedded tag, or both. A file save invalidates the read cache; an embedded save dispatches an asynchronous write and metadata changes invalidate the cache. Without `filename`, verified single-track files use the audio stem; other tracks use `<first artist> - <title>`. Missing tags in the latter case fail that target with `INVALID_PARAMS`. Existing files are overwritten, so use distinct filenames when tracks have the same artist and title. With one target the result describes that write; with several, each outcome is under `results` and `OPERATION_FAILED` means all failed.
 */
export interface LyricsSaveParams {
    /** Track path the lyrics belong to; a `|subsong:N` suffix selects a subsong. */
    path: string;
    /** Lyrics text to save. */
    lyrics: string;
    /** Where to write: `file` (a sidecar next to the audio file), `embedded` (a tag in the file), or `all` for both; several entries write to each. Any other value fails with `INVALID_PARAMS`. Omitted means `file`. */
    target?: string[];
    /** Exact file name, including any extension, for the `file` target. A non-empty value replaces the derived name and ignores `format`; empty means the default name. The same Windows filename rules and `INVALID_PARAMS` failure as `lyrics.get` apply. */
    filename?: string;
    /**
     * Tag the `embedded` target writes.
     * @default "LYRICS"
     */
    tagName?: string;
    /**
     * Extension for the default sidecar name; ignored when `filename` is non-empty.
     * @default "lrc"
     */
    format?: "lrc" | "txt";
}

/**
 * Parameters for `media.getContainerInfo`.
 * Inspect MP4/QuickTime or Matroska/WebM container metadata without decoding. Unknown fields are omitted; an unrecognized format returns recognized:false. Malformed recognized containers and parser resource limits fail with OPERATION_FAILED, not an empty successful track list. Paths follow getStreamUrl. The result describes the whole container, whatever subsong suffix the path has. foobar2000 plays each chapter of an MP4 or Matroska file as its own subsong, timed from the chapter start; a chapter's subsong field names that subsong. Malformed chapter structures only omit chapters. Requires a trusted HTTP(S) document, but not the media routing runtime interface. QuickTime compressed movie headers (cmov) and reference movies (rmra) are recognized but unsupported and fail with OPERATION_FAILED.
 * @experimental
 */
export interface MediaGetContainerInfoParams {
    /** Local container path; a subsong suffix is stripped before file access. */
    path: string;
}

/**
 * Parameters for `media.getStreamUrl`.
 * Issue a URL for a local media file, usable by the calling document until navigation, file modification or eviction. Use crossorigin="anonymous" on media elements. Requires a trusted HTTP(S) document and WebView2 ICoreWebView2_22; older runtimes return NOT_SUPPORTED. GET supports one byte range, returning at most 2 MiB. GET without Range rejects files larger than 2 MiB with HTTP 416. HEAD returns metadata without a body. Only native local paths, UNC, foobar2000 file:// paths and file-relative:// are accepted. A subsong suffix selects the container, not a chapter timeline. Network and archive protocols fail with INVALID_PARAMS; denied files with PERMISSION_DENIED; missing files with NOT_FOUND. File IO or resource limits fail with OPERATION_FAILED.
 * @experimental
 */
export interface MediaGetStreamUrlParams {
    /** Local container path; a subsong suffix is stripped before file access. */
    path: string;
}

/**
 * Parameters for `menu.close`.
 * Close the self-drawn menu if one is open; its `menu:dismiss` carries `reason`.
 */
export interface MenuCloseParams {
    /**
     * Reason reported in `menu:dismiss`.
     * @default "api"
     */
    reason?: string;
}

/**
 * Parameters for `menu.getContextMenu`.
 * Get the context menu for the tracks `mode` selects, as a tree.
 */
export interface MenuGetContextMenuParams {
    /**
     * Tracks to build the menu for: `handles` uses `handles`, `playlist` the active playlist, `nowPlaying` the playing track and `selection` the active playlist selection. Any other value, `auto` included, takes the first of `handles`, the playing track, the selection and the active playlist that is available. Given `handles` of which none is usable fail with `INVALID_PARAMS` in every mode, `auto` included, rather than falling back to other tracks; `nowPlaying` with nothing playing and `selection` with nothing selected fail with `NO_ACTIVE_ITEM`.
     * @default "auto"
     */
    mode?: string;
    /**
     * Locale for `displayLabel`: `auto` keeps the host's labels; a `zh` or `en` tag translates common labels.
     * @default "auto"
     */
    locale?: string;
    /**
     * `false` turns the label translation off.
     * @default true
     */
    i18n?: boolean;
    /**
     * Add command counts to submenus.
     * @default true
     */
    withAvailability?: boolean;
    /** Tracks for `mode: "handles"` and `auto`: each a path, optionally ending in `|subsong:N`, or an object `{ path, subsong }`. Paths the security policy refuses and entries of other types are skipped. Each path is checked on disk, so prefer `selection`, `playlist` or `nowPlaying` for tracks the host already lists. */
    handles?: JsonValue[];
}

/**
 * Parameters for `menu.getMainMenu`.
 * Get the main menu as a tree. The host builds it from its menu tree, falls back to the Win32 menus (`source: "v1-hmenu"`), and as a last resort returns a flat command list (`fallback: "flat-mainmenu-commands"`); fails with `OPERATION_FAILED` when none can be built.
 */
export interface MenuGetMainMenuParams {
    /**
     * Label or slash-separated path of a submenu to return instead of the whole menu. When nothing matches, the whole menu is returned with `rootMatched: false`.
     * @default ""
     */
    root?: string;
    /**
     * Locale for `displayLabel`: `auto` keeps the host's labels; a `zh` or `en` tag translates common labels.
     * @default "auto"
     */
    locale?: string;
    /**
     * `false` turns the label translation off.
     * @default true
     */
    i18n?: boolean;
    /**
     * Add command counts to submenus.
     * @default true
     */
    withAvailability?: boolean;
}

/**
 * Parameters for `menu.runContextCommand`.
 * Run a context-menu command on the playing track, or on the active playlist selection when nothing is playing; fails with `NO_ACTIVE_ITEM` when there is neither. A name that matches no command fails with `MENU_COMMAND_NOT_FOUND`, a GUID that no command owns with `NOT_FOUND`.
 */
export interface MenuRunContextCommandParams {
    /** The command: a GUID or a command name. */
    command: string;
    /** GUID of a dynamic child such as a rating value or a converter preset; without it the owning container is targeted, which does nothing. Empty is the same as absent. */
    subGuid?: string;
}

/**
 * Parameters for `menu.runContextCommandById`.
 * Run the context-menu item whose `commandId` a `menu.getContextMenu` call reported. The menu is rebuilt from `mode` and `handles`, so pass the `handles` that call used and the `mode` it reported, not `auto` again. An id is a position in the rebuilt menu, not a name: an id past its end fails with `NOT_FOUND`, but when the tracks, the selection, the playing track or the installed components changed since, the same id can run another item without failing.
 */
export interface MenuRunContextCommandByIdParams {
    /** The `commandId` of a context-menu node. */
    id: number;
    /**
     * The `mode` the menu was built with; see `menu.getContextMenu`.
     * @default "auto"
     */
    mode?: string;
    /** The `handles` the menu was built with; see `menu.getContextMenu`. */
    handles?: JsonValue[];
}

/**
 * Parameters for `menu.runMainMenuCommand`.
 * Run a main-menu command addressed by GUID, leaf name or slash-separated path. A disabled command fails with `MENU_ITEM_DISABLED`; a name that matches several commands fails with `MENU_MATCH_AMBIGUOUS` and lists them in `candidates`; a name that matches none fails with `MENU_COMMAND_NOT_FOUND`, and a GUID that no command owns with `NOT_FOUND`.
 */
export interface MenuRunMainMenuCommandParams {
    /** The command: a GUID such as `{11213A01-...}`, a leaf name, or a slash-separated path. Prefer the GUID, the one form that does not depend on the host's language. */
    command: string;
    /** GUID of a dynamic child of the command; only with the GUID form. Empty is the same as absent. */
    subGuid?: string;
}

/**
 * Parameters for `menu.show`.
 * Show a self-drawn menu. The choice arrives as `menu:select` and a dismissal as `menu:dismiss`; changing a rating, slider or segmented row reports `menu:valueChanged` and leaves the menu open. Too many rows, too deep nesting, too many segments or too much style text fail with `INVALID_PARAMS` and `details`.
 */
export interface MenuShowParams {
    /** Menu rows; the SDK's `MenuPopupItem` describes their fields. A row icon over 32 KiB is dropped. */
    items?: JsonValue[];
    /** Screen x of the anchor in physical pixels; absent or negative uses the mouse pointer. */
    x?: number;
    /** Screen y of the anchor in physical pixels; absent or negative uses the mouse pointer. */
    y?: number;
    /** `contentSized` draws each panel in a window sized to its content; any other value uses one full-screen overlay window. */
    windowModel?: string;
    /** Style sheet applied to the menu on top of the built-in styles, at most 256 KiB. */
    css?: string;
    /** Replace the built-in styles with `css` instead of adding to them. */
    cssReplace?: boolean;
    /** Window backdrop: `acrylic` (the default), `mica`, `mica-alt` or `none`. Other values keep the default. */
    backdrop?: string;
    /** Dark tint for the backdrop; the default is `true`. */
    backdropDarkMode?: boolean;
    /** Length of the closing animation in milliseconds, clamped to 0–1000; the default `0` closes at once. */
    closeAnimationMs?: number;
}

/**
 * Parameters for `menu.showNativePopup`.
 * Show the host's native context menu at the mouse pointer for the tracks `mode` selects. The menu opens just after the call returns.
 */
export interface MenuShowNativePopupParams {
    /**
     * Tracks to build the menu for; see `menu.getContextMenu`.
     * @default "auto"
     */
    mode?: string;
    /** Tracks for `mode: "handles"` and `auto`; see `menu.getContextMenu`. */
    handles?: JsonValue[];
    /** Accepted for compatibility and ignored: the menu always opens at the mouse pointer. */
    x?: number;
    /** Accepted for compatibility and ignored: the menu always opens at the mouse pointer. */
    y?: number;
}

/**
 * Parameters for `metadata.cancelProbe`.
 * Stop a probe started by `metadata.probeBatchAsync`. The read in progress is interrupted, the paths not yet reached are never reported, and the run still ends with `metadata:probeComplete` carrying `cancelled: true`.
 */
export interface MetadataCancelProbeParams {
    /** Id from the `metadata.probeBatchAsync` receipt. */
    operationId: string;
}

/**
 * Parameters for `metadata.embedArtwork`.
 * Write a picture into a track's tags, into an image file next to it, or both. With one target the result describes that write; with both, each target's outcome is reported under `results` and the call fails with `OPERATION_FAILED` only when both failed.
 */
export interface MetadataEmbedArtworkParams {
    /** Track path. The `embedded` target writes into this file, which needs a format the album art editor supports; the `file` target writes into its directory with a `|subsong:N` suffix dropped, so every track of a CUE sheet shares one image. */
    path: string;
    /** The image as plain Base64, without a Data URL header or a `base64:` marker. Characters outside the Base64 alphabet are skipped and decoding stops at the first `=`; input that decodes to no bytes fails with `INVALID_PARAMS`. */
    imageData: string;
    /**
     * Picture type.
     * @default "front"
     */
    type?: "front" | "cover_front" | "back" | "cover_back" | "disc" | "icon" | "artist";
    /** Where to write: `embedded` (into the file's tags), `file` (an image next to the audio file, named after `type`, `cover` for the front picture, with the extension read from the image bytes), or `all` for both; several entries write to each. Omitted, it writes `embedded`. Any other value fails with `INVALID_PARAMS`. */
    target?: string[];
    /** File name for the `file` target, used as it is instead of the derived one (no extension is added); must be a plain name without separators or traversal. */
    filename?: string;
}

/**
 * Parameters for `metadata.probeBatchAsync`.
 * Start a cancellable batch probe on a worker thread and return a receipt at once. The results arrive in batches on `metadata:probeProgress`, every 64 results or 100 ms, followed by one `metadata:probeComplete`. Each result says whether its info came from the cache or the disk, and a failure is classified as `not-found`, `unsupported-format` or `read-error`.
 */
export interface MetadataProbeBatchAsyncParams {
    /** Paths to probe, reported one result each, in this order. A `|subsong:N` suffix selects a subsong; the `#N` spelling is not recognized, since it would split a file name that ends in `#<digits>`. Every path is checked before anything starts; one refused path fails the whole call with `PERMISSION_DENIED` and no `operationId`. */
    paths: string[];
    /**
     * Attach the flat fields, as in `metadata.readByPath` without `path` and without the `TRACKNUMBER` taken from the file name, to each successful result; `false` reports technical info only.
     * @default true
     */
    includeTags?: boolean;
}

/**
 * Parameters for `metadata.read`.
 * Read the tags and technical info of one track. The host reads its cached info and falls back to reading the file when that info cannot be had or has no title tag; a track that cannot be opened or read fails with `OPERATION_FAILED`.
 */
export interface MetadataReadParams {
    /** Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong. */
    path: string;
    /**
     * Subsong index that wins over a suffix in `path`; negative values are ignored.
     * @default -1
     */
    cueIndex?: number;
}

/**
 * Parameters for `metadata.readBatch`.
 * Read the flat fields of several tracks, one row per path. A path that cannot be read gets a row with its own error and does not fail the call. The reads run on the main thread; prefer `metadata.probeBatchAsync` for many files.
 */
export interface MetadataReadBatchParams {
    /** Track paths, each resolved on its own; a `|subsong:N` suffix or a trailing `#N` selects a subsong, and there is no batch-wide `cueIndex`. An empty list is an empty success. */
    paths: string[];
}

/**
 * Parameters for `metadata.readByPath`.
 * Read one track as a flat object: every tag and technical-info field becomes a top-level key under its upper-case name, beside `path`. The track is read the same way as by `metadata.read`, with the same failures.
 */
export interface MetadataReadByPathParams {
    /** Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong. */
    path: string;
    /**
     * Subsong index that wins over a suffix in `path`; negative values are ignored.
     * @default -1
     */
    cueIndex?: number;
}

/**
 * Parameters for `metadata.readRaw`.
 * Read the tags and technical info of one track straight from the file, bypassing the host's cache; the result is that of `metadata.read` plus `source`. A file that cannot be read fails with `OPERATION_FAILED`.
 */
export interface MetadataReadRawParams {
    /** Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong. */
    path: string;
    /**
     * Subsong index that wins over a suffix in `path`; negative values are ignored.
     * @default -1
     */
    cueIndex?: number;
}

/**
 * Parameters for `metadata.removeEmbeddedArt`.
 * Remove embedded pictures from a file: one type, or every picture when `type` is omitted or `removeAll` is set. A format the album art editor does not support fails with `NOT_SUPPORTED`.
 */
export interface MetadataRemoveEmbeddedArtParams {
    /** Track path; the file needs a format the album art editor supports. */
    path: string;
    /** Picture type to remove; omitted, every picture is removed. */
    type?: "front" | "cover_front" | "back" | "cover_back" | "disc" | "icon" | "artist";
    /**
     * Remove every picture whatever `type` says.
     * @default false
     */
    removeAll?: boolean;
}

/**
 * Parameters for `metadata.removeField`.
 * Same as `metadata.removeTag`, under the name older callers use; its completion event says `removeTag` as well.
 */
export interface MetadataRemoveFieldParams {
    /** Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong. */
    path: string;
    /** Tag names to remove; they are upper-cased. An empty list removes nothing and succeeds without dispatching. */
    tags: string[];
    /**
     * Subsong index that wins over a suffix in `path`; negative values are ignored.
     * @default -1
     */
    cueIndex?: number;
}

/**
 * Parameters for `metadata.removeTag`.
 * Queue the removal of tags from one track and return at once. The outcome arrives as `metadata:writeComplete` with `operation: "removeTag"`, broadcast to every window.
 */
export interface MetadataRemoveTagParams {
    /** Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong. */
    path: string;
    /** Tag names to remove; they are upper-cased. An empty list removes nothing and succeeds without dispatching. */
    tags: string[];
    /**
     * Subsong index that wins over a suffix in `path`; negative values are ignored.
     * @default -1
     */
    cueIndex?: number;
}

/**
 * Parameters for `metadata.write`.
 * Queue a tag write to one track and return at once. foobar2000 writes the file in the background and reports the outcome as `metadata:writeComplete`, broadcast to every window. A request that leaves nothing to write succeeds without dispatching.
 */
export interface MetadataWriteParams {
    /** Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong. */
    path: string;
    /** Tags to change, keyed by name. Names are upper-cased, so keys that differ only in case name the same tag. `null`, an empty string or an empty array removes the tag. A non-empty string array replaces all values of the tag, preserving order, duplicates and whitespace without splitting commas or semicolons. Every array element must be a non-empty string without NUL; an invalid element fails this track with `INVALID_PARAMS` before any of its tags are queued, with the tag name and zero-based element index in the error. A scalar string is written as it is; an integer or a fraction is written as its decimal text (`2.5` becomes `2.500000`). Boolean and object values are ignored. File formats and tag writers may limit multivalue support; the completion event reports the final write result. */
    tags: Record<string, JsonValue>;
    /**
     * Subsong index that wins over a suffix in `path`; negative values are ignored.
     * @default -1
     */
    cueIndex?: number;
}

/**
 * Parameters for `metadata.writeBatch`.
 * Queue one `metadata.write` per entry and report how many went through. A dispatched write reports its own outcome later as `metadata:writeComplete`, so a file that does not exist still counts here. When any entry fails, the call fails with `OPERATION_FAILED` and the failure carries the same `successCount`, `failCount` and `errors`; the other entries were queued all the same.
 */
export interface MetadataWriteBatchParams {
    /** Writes to queue, one per entry, in this order. An entry that is not an object, holds a member other than `path`, `tags` and `cueIndex`, or has no string `path` fails the call with `INVALID_PARAMS`. Every path is checked before anything starts; one refused path, an empty string included, fails the whole call with `PERMISSION_DENIED`. An empty list is an empty success. */
    items: MetadataWriteBatchItem[];
}

/**
 * Parameters for `misc.exit`.
 * Ask foobar2000 to exit.
 */
export type MiscExitParams = Record<string, never>;

/**
 * Parameters for `misc.getComponentPath`.
 * Report the directory the component DLL was loaded from.
 */
export type MiscGetComponentPathParams = Record<string, never>;

/**
 * Parameters for `misc.getFoobarPath`.
 * Report the foobar2000 installation directory.
 */
export type MiscGetFoobarPathParams = Record<string, never>;

/**
 * Parameters for `misc.getProfilePath`.
 * Report the foobar2000 profile directory, where configuration and components live.
 */
export type MiscGetProfilePathParams = Record<string, never>;

/**
 * Parameters for `misc.restart`.
 * Ask foobar2000 to restart.
 */
export type MiscRestartParams = Record<string, never>;

/**
 * Parameters for `misc.showConsole`.
 * Open the foobar2000 console window.
 */
export type MiscShowConsoleParams = Record<string, never>;

/**
 * Parameters for `misc.showLibrarySearch`.
 * Open the media library search window, optionally with a query filled in.
 */
export interface MiscShowLibrarySearchParams {
    /**
     * Query filled into the search box; empty opens a blank search.
     * @default ""
     */
    query?: string;
}

/**
 * Parameters for `misc.showPopupMessage`.
 * Show a foobar2000 popup message dialog. The call returns when the dialog is shown, not when it is closed.
 */
export interface MiscShowPopupMessageParams {
    /** Body text of the dialog. */
    message: string;
    /**
     * Title bar text.
     * @default "Message"
     */
    title?: string;
}

/**
 * Parameters for `misc.showPreferences`.
 * Open the foobar2000 Preferences dialog.
 */
export type MiscShowPreferencesParams = Record<string, never>;

/**
 * Parameters for `output.getDevices`.
 * List the devices of every output module.
 */
export type OutputGetDevicesParams = Record<string, never>;

/**
 * Parameters for `output.getEntries`.
 * List the output modules (foobar2000's output entries) with their capability flags.
 */
export type OutputGetEntriesParams = Record<string, never>;

/**
 * Parameters for `output.getSettings`.
 * Report the display names of the output modules. Informational only: the output settings are edited in foobar2000 Preferences, and `config.setOutputDevice` switches the device.
 */
export type OutputGetSettingsParams = Record<string, never>;

/**
 * Parameters for `panel.getConfig`.
 * Report the configuration of the calling panel. A panel configuration exists only for a DUI element or a CUI panel; on a standalone window the call fails.
 */
export type PanelGetConfigParams = Record<string, never>;

/**
 * Parameters for `panel.setConfig`.
 * Change the calling panel's configuration. Only the keys below can be set from a page; omitted keys keep their value. The other fields change only through the panel's dialog.
 */
export interface PanelSetConfigParams {
    /** Display name of the panel. */
    panelName?: string;
    /** Whether the panel renders with a transparent background. */
    transparentBackground?: boolean;
    /** Whether a click gives the panel keyboard focus. */
    grabFocus?: boolean;
    /** Whether files can be dropped onto the panel. */
    enableDragDrop?: boolean;
}

/**
 * Parameters for `playback.getCurrentTrack`.
 * Report the track now loaded, playing or paused. Always succeeds; `found` says whether there is one.
 */
export type PlaybackGetCurrentTrackParams = Record<string, never>;

/**
 * Parameters for `playback.getCurrentTrackIndex`.
 * Locate the playing track in its playlist. Known only when playback started from a playlist item; a track played from elsewhere has no location even while a playing playlist exists. The location is also lost once the playing row is removed from the playlist it started from, the playlist included, while the track keeps playing; and there is none while stopped.
 */
export interface PlaybackGetCurrentTrackIndexParams {
    /**
     * `true` adds the track row at that location.
     * @default false
     */
    includeTrackInfo?: boolean;
}

/**
 * Parameters for `playback.getPlaybackOrder`.
 * Report the active playback order as its index and its name.
 */
export type PlaybackGetPlaybackOrderParams = Record<string, never>;

/**
 * Parameters for `playback.getPlayingPlaylist`.
 * Report the playlist playback was last started from. foobar2000 keeps the pointer after playback stops, so a stopped instance that has played before still reports one.
 */
export type PlaybackGetPlayingPlaylistParams = Record<string, never>;

/**
 * Parameters for `playback.getPosition`.
 * Report the playback position together with the length and identity of the track it is in. Everything is zero or empty when nothing is playing.
 */
export type PlaybackGetPositionParams = Record<string, never>;

/**
 * Parameters for `playback.getState`.
 * Report the transport state and what the current track allows.
 */
export type PlaybackGetStateParams = Record<string, never>;

/**
 * Parameters for `playback.getStopAfterCurrent`.
 * Report whether playback stops after the current track.
 */
export type PlaybackGetStopAfterCurrentParams = Record<string, never>;

/**
 * Parameters for `playback.getVolume`.
 * Report the output volume on both scales the host uses, and whether it is muted.
 */
export type PlaybackGetVolumeParams = Record<string, never>;

/**
 * Parameters for `playback.mute`.
 * Mute or unmute; a call that asks for the state already in effect does nothing. Unmuting restores the level from before the mute.
 */
export interface PlaybackMuteParams {
    /**
     * `true` to mute, `false` to unmute.
     * @default true
     */
    muted?: boolean;
}

/**
 * Parameters for `playback.next`.
 * Skip to the next track under the current playback order.
 */
export type PlaybackNextParams = Record<string, never>;

/**
 * Parameters for `playback.pause`.
 * Pause playback. Does nothing when stopped.
 */
export type PlaybackPauseParams = Record<string, never>;

/**
 * Parameters for `playback.play`.
 * Start playback, or resume when paused.
 */
export type PlaybackPlayParams = Record<string, never>;

/**
 * Parameters for `playback.playOrPause`.
 * The same as `playPause`.
 */
export type PlaybackPlayOrPauseParams = Record<string, never>;

/**
 * Parameters for `playback.playPath`.
 * Append one file to the active playlist and play it; a playlist is created when there is none. A `|subsong:N` suffix selects a subsong of the file. When the active playlist is locked the call fails with `LOCKED`, and nothing is added or played. A path the media-root check refuses fails with `PERMISSION_DENIED`; one foobar2000 resolves to nothing fails with `NOT_FOUND`, the failure carrying `path` and `subsong`.
 */
export interface PlaybackPlayPathParams {
    /** File path, optionally with a `|subsong:N` suffix. */
    path: string;
}

/**
 * Parameters for `playback.playPaths`.
 * Add several files to the active playlist and start playing one of them. Entries that are not strings or fail the media-root check are dropped and counted in `skippedPaths`, which only a successful call reports; when every entry is dropped the call fails with `INVALID_PARAMS`. Each remaining path becomes a track as written: folders and playlist files are not expanded and the file is not opened, so a missing file is added too. When no track can be made the call fails with `NOT_FOUND`. When the active playlist is locked the call fails with `LOCKED`, and nothing is cleared, added or played.
 */
export interface PlaybackPlayPathsParams {
    /** File paths, each optionally with a `|subsong:N` suffix. */
    paths: string[];
    /**
     * Which of the added tracks to start with, counted among the tracks that resolved; a value past the end starts the first.
     * @default 0
     */
    startIndex?: number;
    /**
     * `true` clears the active playlist before adding; `false` appends.
     * @default false
     */
    replace?: boolean;
}

/**
 * Parameters for `playback.playPause`.
 * Toggle between playing and paused; starts playback when stopped. Reports the state that results.
 */
export type PlaybackPlayPauseParams = Record<string, never>;

/**
 * Parameters for `playback.previous`.
 * Skip to the previous track under the current playback order.
 */
export type PlaybackPreviousParams = Record<string, never>;

/**
 * Parameters for `playback.random`.
 * Start a random track of the active playlist.
 */
export type PlaybackRandomParams = Record<string, never>;

/**
 * Parameters for `playback.setPlaybackOrder`.
 * Select a playback order by index or by name; exactly one of the two must be given. Reports the order in effect afterwards.
 */
export interface PlaybackSetPlaybackOrderParams {
    /** Order by index, `0` to `6` in the order of `PlaybackOrderName`. */
    order?: number;
    /** Order by name. */
    name?: "default" | "repeat-playlist" | "repeat-track" | "random" | "shuffle-tracks" | "shuffle-albums" | "shuffle-folders";
}

/**
 * Parameters for `playback.setPosition`.
 * Seek within the current track, playing or paused. The position is clamped to the track: negative values seek to the start, values past the end stop just short of it so playback does not advance to the next track; a track of unknown length has no upper bound. With nothing playing the call fails with `NO_ACTIVE_ITEM`, and a track that cannot seek, such as most streams, fails with `NOT_SUPPORTED`.
 */
export interface PlaybackSetPositionParams {
    /** Target position in seconds. */
    position: number;
}

/**
 * Parameters for `playback.setStopAfterCurrent`.
 * Arm or disarm stopping after the current track.
 */
export interface PlaybackSetStopAfterCurrentParams {
    /** `true` to stop after the current track. */
    enabled: boolean;
}

/**
 * Parameters for `playback.setVolume`.
 * Set the output volume as a percentage; values outside `0` to `100` are clamped. `0` mutes.
 */
export interface PlaybackSetVolumeParams {
    /** Volume as a percentage; clamped to `0` to `100`. */
    volume: number;
}

/**
 * Parameters for `playback.stop`.
 * Stop playback.
 */
export type PlaybackStopParams = Record<string, never>;

/**
 * Parameters for `playback.toggleMute`.
 * Flip the mute state and report the new one.
 */
export type PlaybackToggleMuteParams = Record<string, never>;

/**
 * Parameters for `playback.toggleStopAfterCurrent`.
 * Flip the stop-after-current flag and report the new value.
 */
export type PlaybackToggleStopAfterCurrentParams = Record<string, never>;

/**
 * Parameters for `playback.volumeDown`.
 * Lower the volume by one of the host's steps: one decibel, landing on a whole decibel.
 */
export type PlaybackVolumeDownParams = Record<string, never>;

/**
 * Parameters for `playback.volumeUp`.
 * Raise the volume by one of the host's steps: one decibel, landing on a whole decibel.
 */
export type PlaybackVolumeUpParams = Record<string, never>;

/**
 * Parameters for `playcount.get`.
 * Read the foo_playcount statistics of tracks. Without foo_playcount installed the counts read as zero and the dates are absent.
 */
export interface PlaycountGetParams {
    /** Track paths; a `|subsong:N` suffix selects a CUE subsong. A suffix whose index cannot be read, such as `|subsong:abc`, is dropped and the path means the first track. */
    paths: string[];
}

/**
 * Parameters for `playcount.getBatch`.
 * Same as `playcount.get`, under the name batch callers expect.
 */
export interface PlaycountGetBatchParams {
    /** Track paths; a `|subsong:N` suffix selects a CUE subsong. A suffix whose index cannot be read, such as `|subsong:abc`, is dropped and the path means the first track. */
    paths: string[];
}

/**
 * Parameters for `playcount.getStats`.
 * Summarise play counts and ratings over the whole media library.
 */
export type PlaycountGetStatsParams = Record<string, never>;

/**
 * Parameters for `playcount.set`.
 * Placeholder: foo_playcount offers no way to change statistics, so this always fails with `NOT_SUPPORTED`. Change ratings with `rating.set`.
 */
export interface PlaycountSetParams {
    /** Path of the track. */
    path: string;
}

/**
 * Parameters for `playlist.addHandles`.
 * Append tracks to a playlist, saving an undo point first. `handles` takes the same entries as `playlist.insertTracks`; when none is usable the call fails with `NOT_FOUND`. A locked playlist fails with `LOCKED`.
 */
export interface PlaylistAddHandlesParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Tracks to append: each a path with an optional `|subsong:N` suffix, or `{ path, subsong }`. */
    handles: JsonValue[];
}

/**
 * Parameters for `playlist.addPaths`.
 * Append files, folders or URLs to a playlist, saving an undo point first. foobar2000 resolves the batch as its own Add Files does: folders and playlist files are expanded, duplicates are dropped and the rows are sorted by the user's incoming-item order, so they do not keep the given order (`playlist.addPathsSequential` does). Entries longer than 2048 characters, and entries that resolve to nothing, are counted in `invalidCount`. When nothing is added the call fails with `NOT_FOUND`. A locked playlist fails with `LOCKED`. foobar2000 can show a progress dialog while it resolves the paths, and the playlist is looked up again afterwards: moved meanwhile, the tracks still go into it and `playlist` in the result is its new index; removed meanwhile, the call fails with `OPERATION_FAILED`.
 */
export interface PlaylistAddPathsParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Files, folders or URLs; a `|subsong:N` suffix selects a subsong. */
    paths: string[];
}

/**
 * Parameters for `playlist.addPathsAsync`.
 * Start appending paths to a playlist without waiting: the call returns a receipt, and `playlist:addComplete` with the same `operationId` reports the outcome. Plain files and URLs are added before the call returns; playlist files (`.pls`, `.m3u`, `.cue` and the like) are expanded in the background. Entries that are empty or longer than 2048 characters are counted in `invalidCount`; when no entry is left the call fails with `INVALID_PARAMS`. A locked playlist fails with `LOCKED`. The expanded tracks go into the same playlist even if it was moved meanwhile; if it was removed or locked meanwhile they are dropped.
 */
export interface PlaylistAddPathsAsyncParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Files, folders, URLs or playlist files; a `|subsong:N` suffix selects a subsong. */
    paths: string[];
}

/**
 * Parameters for `playlist.addPathsSequential`.
 * Append paths to a playlist in the given order, saving an undo point first; a path that expands to several tracks (a folder, a cue sheet) keeps its place. Entries longer than 2048 characters, and entries that resolve to nothing, are skipped. A locked playlist fails with `LOCKED`. A playlist moved or removed while the paths are resolved is handled as in `playlist.addPaths`.
 */
export interface PlaylistAddPathsSequentialParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Files, folders or URLs, in the order to add them; a `|subsong:N` suffix selects a subsong. */
    paths: string[];
}

/**
 * Parameters for `playlist.clear`.
 * Remove every track from a playlist, saving an undo point first. A locked playlist fails with `LOCKED` before anything changes; tracks left behind afterwards fail with `OPERATION_FAILED`, the failure carrying the same fields as a success.
 */
export interface PlaylistClearParams {
    /** Index of the playlist to empty; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.convertToAutoplaylist`.
 * Turn an existing playlist into an autoplaylist; its tracks are replaced by the query results. Fails with `OPERATION_FAILED` when foobar2000 refuses, for instance because the playlist is already an autoplaylist.
 */
export interface PlaylistConvertToAutoplaylistParams {
    /** Index of the playlist to convert; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Library query that selects the tracks, in foobar2000's query syntax. */
    query: string;
    /**
     * Title Formatting pattern to sort the results by; empty, library order.
     * @default ""
     */
    sort?: string;
    /**
     * Keep the playlist sorted by `sort`: when set, the user cannot reorder its tracks.
     * @default false
     */
    keepSorted?: boolean;
}

/**
 * Parameters for `playlist.create`.
 * Create an empty playlist. Fails with `OPERATION_FAILED` when foobar2000 refuses to create one.
 */
export interface PlaylistCreateParams {
    /**
     * Name of the new playlist.
     * @default "New Playlist"
     */
    name?: string;
    /** Where to insert it in the playlist list; omitted, at the end. */
    position?: number;
}

/**
 * Parameters for `playlist.createAutoplaylist`.
 * Create a playlist that foobar2000 fills from a library query and keeps up to date. An autoplaylist foobar2000 refuses to set up fails with `OPERATION_FAILED`, and the new playlist is removed again.
 */
export interface PlaylistCreateAutoplaylistParams {
    /**
     * Name of the new playlist.
     * @default "New Autoplaylist"
     */
    name?: string;
    /** Library query that selects the tracks, in foobar2000's query syntax. */
    query: string;
    /**
     * Title Formatting pattern to sort the results by; empty, library order.
     * @default ""
     */
    sort?: string;
    /**
     * Keep the playlist sorted by `sort`: when set, the user cannot reorder its tracks.
     * @default false
     */
    keepSorted?: boolean;
}

/**
 * Parameters for `playlist.deselectAll`.
 * Clear the selection of a playlist.
 */
export interface PlaylistDeselectAllParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.duplicate`.
 * Copy a playlist, tracks included, into a new playlist placed right after it.
 */
export interface PlaylistDuplicateParams {
    /** Index of the playlist to copy; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Name of the copy; omitted or empty, the original name followed by ` (Copy)`. */
    name?: string;
}

/**
 * Parameters for `playlist.focusTrack`.
 * Move the focus of a playlist; the same operation as `playlist.setFocusedTrack` under its older name.
 */
export interface PlaylistFocusTrackParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Row to focus; omitted or negative, the playlist loses its focus. */
    index?: number;
}

/**
 * Parameters for `playlist.getActive`.
 * Describe the active playlist, the one the user is looking at. `found` is `false` when there is none, and the other fields are then absent.
 */
export type PlaylistGetActiveParams = Record<string, never>;

/**
 * Parameters for `playlist.getAll`.
 * List every playlist in playlist order.
 */
export type PlaylistGetAllParams = Record<string, never>;

/**
 * Parameters for `playlist.getAutoplaylistInfo`.
 * Describe a playlist's autoplaylist status.
 */
export interface PlaylistGetAutoplaylistInfoParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.getAutoplaylistQuery`.
 * Describe a playlist's autoplaylist status, as `playlist.getAutoplaylistInfo` does. foobar2000 does not expose the query of an autoplaylist, so `query` is always `null`.
 */
export interface PlaylistGetAutoplaylistQueryParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.getAvailableColumns`.
 * List the playlist columns that foobar2000 and the installed components define for the Default UI playlist view.
 */
export type PlaylistGetAvailableColumnsParams = Record<string, never>;

/**
 * Parameters for `playlist.getCount`.
 * Report how many playlists there are.
 */
export type PlaylistGetCountParams = Record<string, never>;

/**
 * Parameters for `playlist.getFocusedTrack`.
 * Report the focused row of a playlist. An index past the last playlist, a `playlistGuid` no playlist has, or no active playlist when both are omitted, reports `index` `-1` without `playlist` and `playlistGuid` rather than failing.
 */
export interface PlaylistGetFocusedTrackParams {
    /** Index of the playlist; omitted, the active playlist. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written as the results and events that name a playlist report it, for a query that answers an empty result for a missing playlist: a GUID no playlist has answers the same way. Either hex case is accepted. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.getFocusTrack`.
 * Report the focused row of a playlist; the same as `playlist.getFocusedTrack` under its older name, except that a missing playlist fails instead of answering `index` `-1`: an index past the last playlist with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`, and no active playlist when both are omitted with `NO_ACTIVE_ITEM`. On success `playlist` and `playlistGuid` are always present.
 */
export interface PlaylistGetFocusTrackParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.getGroupRuns`.
 * Split a whole playlist into runs of adjacent rows whose group key is equal, with ASCII letters compared case-insensitively; a second pattern sub-groups each run. Rows are never reordered and only the run boundaries come back, so the answer grows with the number of runs, not of rows. Keys are evaluated off the main thread. More than two patterns, an empty pattern or one that fails to compile fails with `INVALID_PARAMS`, the last two with `details.pattern` giving its position. An index past the last playlist fails with `INVALID_INDEX` and `details.playlist` giving the index asked for, a `playlistGuid` no playlist has with `NOT_FOUND` and `details.playlistGuid`; with both omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. An error while the keys are evaluated fails with `OPERATION_FAILED`, the failure carrying an empty `runs` and `total` `0`.
 */
export interface PlaylistGetGroupRunsParams {
    /** One or two Title Formatting patterns; the second sub-groups each run. */
    patterns: string[];
    /** Playlist index; omitted, the active playlist. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.getLockInfo`.
 * Report whether a playlist carries a lock, such as an autoplaylist's.
 */
export interface PlaylistGetLockInfoParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.getMatchingRows`.
 * The rows of a playlist whose tracks match a foobar2000 query, in playlist order, in one call however long the playlist is; read the rows themselves with `playlist.getTracksAt`. The query takes the Media Library search syntax without `SORT BY`. A query the parser rejects fails with `INVALID_PARAMS` and `details.param` `query`, and so does one carrying `SORT BY`; many malformed queries are not rejected but simply match nothing. An index past the last playlist fails with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`, and with both omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. The answer is a snapshot taken on the main thread: editing the playlist or a track's tags can change it, and so can the passing of time for a query such as `%added% DURING LAST 2 WEEKS`, which no event announces.
 */
export interface PlaylistGetMatchingRowsParams {
    /** foobar2000 query, as in the Media Library search; `SORT BY` is refused. */
    query: string;
    /** Playlist index; omitted, the active playlist. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.getPlaying`.
 * Describe the playing playlist, the one playback takes its next track from. `found` is `false` when there is none, and the other fields are then absent.
 */
export type PlaylistGetPlayingParams = Record<string, never>;

/**
 * Parameters for `playlist.getSelectedTracks`.
 * The selected rows of a playlist in playlist order, without the play statistics. An index past the last playlist fails with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`; with both omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. Each of these failures, and giving both keys or a malformed GUID, carries an empty `tracks`.
 */
export interface PlaylistGetSelectedTracksParams {
    /** Playlist index; omitted, the active playlist. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.getSelection`.
 * List the selected rows of a playlist.
 */
export interface PlaylistGetSelectionParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.getTrackCount`.
 * Report how many tracks a playlist holds. An index past the last playlist, a `playlistGuid` no playlist has, or no active playlist when both are omitted, reports `0` rather than failing.
 */
export interface PlaylistGetTrackCountParams {
    /** Index of the playlist; omitted, the active playlist. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written as the results and events that name a playlist report it, for a query that answers an empty result for a missing playlist: a GUID no playlist has answers the same way. Either hex case is accepted. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.getTracks`.
 * A page of a playlist's rows. Without `fields` each row is a whole playlist row with the play statistics foo_playcount provides; `fields` narrows every row to the named keys, from the list `library.query` takes, and always keeps `index`. A playlist index past the last one, a `playlistGuid` no playlist has, and no active playlist when both are omitted, answer an empty page.
 */
export interface PlaylistGetTracksParams {
    /** Playlist index; omitted, the active playlist. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written as the results and events that name a playlist report it, for a query that answers an empty result for a missing playlist: a GUID no playlist has answers the same way. Either hex case is accepted. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. */
    playlistGuid?: string;
    /**
     * First row to return.
     * @default 0
     */
    start?: number;
    /**
     * Most rows to return.
     * @default 100
     */
    count?: number;
    /** Extra columns, each a Title Formatting pattern under a name of your choosing; every row carries their values under `formats`, in projected rows too. */
    formats?: Record<string, string>;
    /** Keys each row carries besides `index`; omitted, the whole row. The names are those `library.query` takes; an unknown name fails with `INVALID_PARAMS` and is listed in `details.unknownFields`. */
    fields?: string[];
}

/**
 * Parameters for `playlist.getTracksAt`.
 * Rows of a playlist picked by row number, in the order given, such as the rows `playlist.getMatchingRows` answers, which need not be adjacent. Each row is shaped as in `playlist.getTracks` and carries its `index`; a row past the last one is skipped, and a row given twice comes back twice. A playlist index past the last one, a `playlistGuid` no playlist has, and no active playlist when both are omitted, answer an empty result as `playlist.getTracks` does.
 */
export interface PlaylistGetTracksAtParams {
    /** Rows to read, in the order to return them. */
    rows: number[];
    /** Playlist index; omitted, the active playlist. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written as the results and events that name a playlist report it, for a query that answers an empty result for a missing playlist: a GUID no playlist has answers the same way. Either hex case is accepted. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. */
    playlistGuid?: string;
    /** Extra columns, as in `playlist.getTracks`. */
    formats?: Record<string, string>;
    /** Keys each row carries besides `index`, as in `playlist.getTracks`; omitted, the whole row. */
    fields?: string[];
}

/**
 * Parameters for `playlist.insertTracks`.
 * Insert tracks at a position of a playlist, saving an undo point first. Each entry of `handles` is a path with an optional `|subsong:N` suffix, or `{ path, subsong }`; entries that are neither, or that name nothing, are counted in `invalidCount`. When none is usable the call fails with `NOT_FOUND`. A locked playlist fails with `LOCKED`.
 */
export interface PlaylistInsertTracksParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /**
     * Row before which the tracks go; past the last row, they are appended.
     * @default 0
     */
    position?: number;
    /** Tracks to insert: each a path with an optional `|subsong:N` suffix, or `{ path, subsong }`. */
    handles: JsonValue[];
}

/**
 * Parameters for `playlist.isAutoplaylist`.
 * Report whether a playlist is an autoplaylist: one foobar2000 fills from a library query, or one carrying a lock whose name contains `Auto` (autoplaylists that other components manage).
 */
export interface PlaylistIsAutoplaylistParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.isLocked`.
 * Report whether a playlist carries a lock. An index past the last playlist fails with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`. Every failure to find the playlist, and giving both keys or a malformed GUID, carries `isLocked` `false`.
 */
export interface PlaylistIsLockedParams {
    /** Index of the playlist; omitted, the active playlist. With this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.moveTracks`.
 * Move the selected rows of a playlist by `delta` positions, saving an undo point first. With `items`, the selection is first replaced by those rows; the caller's own selection is lost. A locked playlist fails with `LOCKED`.
 */
export interface PlaylistMoveTracksParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Rows to move; they replace the current selection first. Omitted or empty, the current selection moves. */
    items?: number[];
    /** How many positions to move: negative towards the top, positive towards the bottom. */
    delta: number;
}

/**
 * Parameters for `playlist.playTrack`.
 * Play a row of a playlist, as double-clicking it does. A row past the last one fails with `INVALID_INDEX`.
 */
export interface PlaylistPlayTrackParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /**
     * Row to play.
     * @default 0
     */
    index?: number;
    /**
     * Start the playback after the current message has been handled instead of before the call returns. The playlist is looked up again by its GUID when the playback starts: moved meanwhile, the same playlist plays; removed meanwhile, or shorter than `index` by then, nothing plays.
     * @default false
     */
    deferred?: boolean;
    /**
     * Mute before starting, so a page that sets the volume right afterwards leaves no audible gap. The mute is not undone by this call.
     * @default false
     */
    muted?: boolean;
}

/**
 * Parameters for `playlist.redo`.
 * Reapply the change the last `playlist.undo` reverted. Fails with `NOT_FOUND` when there is nothing to redo, and with `LOCKED` when a lock on the playlist refuses the change.
 */
export interface PlaylistRedoParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.remove`.
 * Delete a playlist. Fails with `LOCKED` when a lock on the playlist refuses the removal, and with `OPERATION_FAILED` when foobar2000 refuses it otherwise.
 */
export interface PlaylistRemoveParams {
    /** Index of the playlist to delete; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.removeAutoplaylist`.
 * Turn an autoplaylist back into an ordinary playlist that keeps its current tracks. An autoplaylist that another component manages cannot be released this way: the call succeeds with `source` `dui` and changes nothing, and `playlist.remove` deletes it. A playlist that is not an autoplaylist fails with `NOT_FOUND`.
 */
export interface PlaylistRemoveAutoplaylistParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.removeSelectedTracks`.
 * Remove the selected rows from a playlist, saving an undo point first. A locked playlist fails with `LOCKED`.
 */
export interface PlaylistRemoveSelectedTracksParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.removeTracks`.
 * Remove rows from a playlist, saving an undo point first. Rows past the last one are ignored; a negative row fails with `INVALID_PARAMS`. A locked playlist fails with `LOCKED`.
 */
export interface PlaylistRemoveTracksParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Rows to remove. */
    items: number[];
}

/**
 * Parameters for `playlist.rename`.
 * Rename a playlist. Fails with `LOCKED` when a lock on the playlist refuses the new name, and with `OPERATION_FAILED` when foobar2000 refuses it otherwise.
 */
export interface PlaylistRenameParams {
    /** Index of the playlist to rename. An index past the last playlist fails with `INVALID_INDEX`. Give this or `playlistGuid`; with neither the call fails with `INVALID_PARAMS`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** New name. */
    name: string;
}

/**
 * Parameters for `playlist.reorder`.
 * Reorder a playlist's tracks, saving an undo point first: `newOrder[i]` is the current row of the track that moves to row `i`. A locked playlist fails with `LOCKED`, a list whose length is not the track count or that names a row twice with `INVALID_PARAMS`, an entry past the last row with `INVALID_INDEX`.
 */
export interface PlaylistReorderParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** For each new row, the current row of the track that goes there; every row exactly once. */
    newOrder: number[];
}

/**
 * Parameters for `playlist.reorderPlaylists`.
 * Reorder the playlist list, giving for each new position either the current index of the playlist that goes there (`newOrder`) or its GUID (`newOrderGuids`); give exactly one of the two, or the call fails with `INVALID_PARAMS`. Indices are read as the call arrives, so indices taken from `playlist.getAll` before another change reorder the wrong playlists without an error as long as the count still matches; GUIDs keep naming the playlists they were read from. A list whose length is not the playlist count fails with `INVALID_PARAMS`, and so does one naming a playlist twice or a malformed GUID; an index past the last playlist fails with `INVALID_INDEX`, a GUID no playlist has with `NOT_FOUND`.
 */
export interface PlaylistReorderPlaylistsParams {
    /** For each new position, the current index of the playlist that goes there; every playlist exactly once. */
    newOrder?: number[];
    /** For each new position, the `guid` of the playlist that goes there, as `playlist.getAll` reports it; every playlist exactly once. */
    newOrderGuids?: string[];
}

/**
 * Parameters for `playlist.replaceAllAndPlay`.
 * Replace the whole content of a playlist and play it: stop playback, clear the playlist (saving an undo point), add the paths as `playlist.addPaths` does, make the playlist active and play or focus `playIndex`. When nothing could be added the call fails with `NOT_FOUND`, and the playlist stays empty. A locked playlist fails with `LOCKED` before anything changes. The paths are resolved after the clear, as in `playlist.addPaths`: a playlist moved meanwhile is still the one filled and played; one removed meanwhile fails with `OPERATION_FAILED`, and one locked meanwhile fails with `LOCKED` and stays empty.
 */
export interface PlaylistReplaceAllAndPlayParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** The new content: files, folders or URLs. */
    paths: string[];
    /**
     * Row to play, or to focus with `autoPlay` `false`; past the last row, the first one. The rows do not keep the order of `paths`, so this names a position, not one of the given paths.
     * @default 0
     */
    playIndex?: number;
    /**
     * Stop playback first, when something is playing.
     * @default true
     */
    stopFirst?: boolean;
    /**
     * Play `playIndex`; `false` only focuses it.
     * @default true
     */
    autoPlay?: boolean;
}

/**
 * Parameters for `playlist.reverse`.
 * Reverse the order of a playlist's tracks, saving an undo point first. A locked playlist fails with `LOCKED`.
 */
export interface PlaylistReverseParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.selectAll`.
 * Select every row of a playlist.
 */
export interface PlaylistSelectAllParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.setActive`.
 * Make a playlist the active one. An index past the last playlist fails with `INVALID_INDEX`.
 */
export interface PlaylistSetActiveParams {
    /** Index of the playlist to activate. Give this or `playlistGuid`; with neither the call fails with `INVALID_PARAMS`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.setFocusedTrack`.
 * Move the focus of a playlist to a row, or remove it. A row past the last one fails with `INVALID_INDEX`.
 */
export interface PlaylistSetFocusedTrackParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Row to focus; omitted or negative, the playlist loses its focus. */
    index?: number;
}

/**
 * Parameters for `playlist.setSelection`.
 * Select rows of a playlist. Rows past the last one are ignored; a negative row fails with `INVALID_PARAMS`.
 */
export interface PlaylistSetSelectionParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Rows to select; an empty list selects nothing. */
    indices: number[];
    /**
     * Deselect every other row; `false` adds `indices` to the current selection.
     * @default true
     */
    clearOthers?: boolean;
}

/**
 * Parameters for `playlist.shuffle`.
 * Put a playlist's tracks in random order, saving an undo point first. A locked playlist fails with `LOCKED`.
 */
export interface PlaylistShuffleParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `playlist.sort`.
 * Sort a playlist by a Title Formatting pattern, saving an undo point first. A locked playlist fails with `LOCKED`.
 */
export interface PlaylistSortParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /**
     * Title Formatting pattern to sort by.
     * @default "%title%"
     */
    pattern?: string;
    /**
     * Reverse the whole playlist after sorting. The reversal covers every track, the unselected ones too, even with `selectedOnly`.
     * @default false
     */
    descending?: boolean;
    /**
     * Sort only the selected tracks among themselves, keeping the others in place.
     * @default false
     */
    selectedOnly?: boolean;
}

/**
 * Parameters for `playlist.undo`.
 * Revert a playlist to its last undo point. Fails with `NOT_FOUND` when there is none, and with `LOCKED` when a lock on the playlist refuses the change.
 */
export interface PlaylistUndoParams {
    /** Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
}

/**
 * Parameters for `port.connect`.
 * Open a port on a named channel for the calling window and announce it with `port:connected`. Any number of ports, from one window or several, can share a channel. A port belongs to the page that opened it and stays open until `port.disconnect`, until that page starts a top-level navigation (a reload or another address; same-document navigation such as a hash change does not count, and a navigation that ends in a download still does), or until its WebView goes away: the popup closes, the panel is removed, or the WebView is rebuilt or crashes beyond recovery. Each of these closes the port with `port:disconnected` before the next page can open ports.
 */
export interface PortConnectParams {
    /** Channel name; ports with the same name exchange messages. */
    name: string;
}

/**
 * Parameters for `port.disconnect`.
 * Close a port and announce it with `port:disconnected`. Only the window that opened the port may close it; another window fails with `PERMISSION_DENIED`, and an id no open port has with `PORT_NOT_FOUND`.
 */
export interface PortDisconnectParams {
    /** Id of the port to close. */
    portId: string;
}

/**
 * Parameters for `port.getPorts`.
 * List the open ports, of every channel or of one.
 */
export interface PortGetPortsParams {
    /** List only the ports of this channel. */
    name?: string;
}

/**
 * Parameters for `port.postMessage`.
 * Send a message to every other port on the sender's channel, as a `port:message` event to each port's window. The sending port must belong to the calling window, otherwise the call fails with `PERMISSION_DENIED`; an unknown sending port fails with `PORT_NOT_FOUND`.
 */
export interface PortPostMessageParams {
    /** Id of the sending port. */
    portId: string;
    /** The message, delivered as `message` of the event. `null` counts as missing. */
    message: JsonValue;
}

/**
 * Parameters for `port.postMessageTo`.
 * Send a message to one port, as a `port:message` event to that port's window. An unknown sending port fails with `PORT_NOT_FOUND`, a sending port of another window with `PERMISSION_DENIED`, an unknown target port with `TARGET_NOT_FOUND`, and a target window that did not take the event with `OPERATION_FAILED`.
 */
export interface PortPostMessageToParams {
    /** Id of the sending port. */
    portId: string;
    /** Id of the port to deliver to. */
    targetPortId: string;
    /** The message, delivered as `message` of the event. `null` counts as missing. */
    message: JsonValue;
}

/**
 * Parameters for `queue.add`.
 * Queue one or more tracks by their position in a playlist. Positions past the last row are skipped; when none is in range the call fails with `INVALID_INDEX`. A negative position fails with `INVALID_PARAMS`.
 */
export interface QueueAddParams {
    /** Playlist the positions refer to; the active playlist when omitted. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. */
    playlistGuid?: string;
    /** Rows to queue, in order. Takes precedence over `track`. */
    tracks?: number[];
    /** A single row to queue; read only when `tracks` is absent. */
    track?: number;
}

/**
 * Parameters for `queue.addPaths`.
 * Queue tracks by path. Queueing needs playlist membership, so the paths are appended to a playlist first: by default a dedicated `[WebView Queue]` playlist, created when missing. A locked target playlist fails with `LOCKED`, and nothing is added or queued. The target is looked up again after the paths are resolved, as in `playlist.addPaths`: moved meanwhile, it still gets the tracks and `playlist` in the result is its new index; removed meanwhile, the call fails with `OPERATION_FAILED` and nothing is queued. A path the media-root check refuses fails the whole call with `PERMISSION_DENIED`. Paths that resolve to nothing are dropped; when none resolves the call fails with `NOT_FOUND`, the failure carrying `invalidCount`.
 */
export interface QueueAddPathsParams {
    /** File paths or URLs, each optionally with a `|subsong:N` suffix. Entries longer than 2048 characters are dropped and counted in `invalidCount`. */
    paths: string[];
    /**
     * Append to the dedicated `[WebView Queue]` playlist, creating it when missing. `false` uses `playlist`, or the active playlist when that is omitted too.
     * @default true
     */
    useQueuePlaylist?: boolean;
    /** Target playlist; read only when `useQueuePlaylist` is `false`. An index past the last playlist fails with `INVALID_INDEX`; with this and `playlistGuid` omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. */
    playlist?: number;
    /** The target playlist's `guid`, instead of `playlist`; like `playlist` it is read only when `useQueuePlaylist` is `false`, and not checked at all otherwise. */
    playlistGuid?: string;
}

/**
 * Parameters for `queue.clear`.
 * Empty the queue.
 */
export type QueueClearParams = Record<string, never>;

/**
 * Parameters for `queue.flush`.
 * Empty the queue; the same operation as `queue.clear` under its older name.
 */
export type QueueFlushParams = Record<string, never>;

/**
 * Parameters for `queue.get`.
 * Read the whole playback queue, in play order. No paging: every entry comes back.
 */
export type QueueGetParams = Record<string, never>;

/**
 * Parameters for `queue.getCount`.
 * Report how many entries the queue holds.
 */
export type QueueGetCountParams = Record<string, never>;

/**
 * Parameters for `queue.insertNext`.
 * Insert tracks so they play next, ahead of everything already queued. A track that is already queued is moved instead of queued twice. `items` land before `paths`; each block keeps its own order. A call with neither `paths` nor `items` fails with `INVALID_PARAMS`. Every entry of `items` is checked before any path is resolved, and one bad entry fails the whole call with nothing written: an entry naming its playlist by neither or both of `playlist` and `playlistGuid`, or by a malformed GUID, fails with `INVALID_PARAMS`, a GUID no playlist has with `NOT_FOUND`, a playlist index or row out of range with `INVALID_INDEX`. A path the media-root check refuses fails the whole call with `PERMISSION_DENIED`; a path that resolves to nothing is dropped and counted in `invalidCount`, and when nothing at all is left to insert the call fails with `NOT_FOUND`. A playlist row that changed while the paths were resolved fails the call with `OPERATION_FAILED`, and so does a queue that cannot be rebuilt.
 */
export interface QueueInsertNextParams {
    /** File paths or URLs, each optionally with a `|subsong:N` suffix. The entries they produce carry no playlist position. */
    paths?: string[];
    /** Playlist rows; the entries they produce carry that position, so the playback cursor follows them. At most `max(256, current queue length)` entries; more fail with `INVALID_PARAMS`. */
    items?: QueueListRef[];
    /**
     * Where to insert, counted after any moved entries are taken out; `0` is the front.
     * @default 0
     */
    position?: number;
}

/**
 * Parameters for `queue.moveToTop`.
 * Move a queued entry to the front so it plays next. Only queue order changes; the relative order of the other entries is kept. An `index` of `0`, one past the end, and any index while the queue is empty fail with `INVALID_INDEX`. When the queue cannot be rebuilt the call fails with `OPERATION_FAILED`, the failure carrying `queueCount`.
 */
export interface QueueMoveToTopParams {
    /** Position of the entry to promote; `0` is already the front and is refused. */
    index: number;
}

/**
 * Parameters for `queue.playNow`.
 * Play the queue entry at `index` now, moving it to the front first when it is not already there. An empty queue fails with `NOT_FOUND`, an `index` past the end with `INVALID_INDEX`. When the entry has to be moved and the queue cannot be rebuilt, the call fails with `OPERATION_FAILED` and playback is not started.
 */
export interface QueuePlayNowParams {
    /**
     * Queue position to play.
     * @default 0
     */
    index?: number;
}

/**
 * Parameters for `queue.remove`.
 * Remove one entry by `index`, or several by `indices`. An empty queue fails with `NOT_FOUND` whatever is given. An `index` past the end fails with `INVALID_INDEX`. In a batch, duplicate indices and those past the end of the queue are skipped; when none is in range the call fails with `INVALID_INDEX`, the failure carrying `removedCount` `0` and `queueCount`. Giving neither key, or a negative index, fails with `INVALID_PARAMS`.
 */
export interface QueueRemoveParams {
    /** One queue position to remove. Takes precedence over `indices`. */
    index?: number;
    /** Queue positions to remove; read only when `index` is absent. */
    indices?: number[];
}

/**
 * Parameters for `queue.setContents`.
 * Replace the whole queue with an ordered list of references. One unusable reference fails the whole call before anything is written. An empty list clears the queue. A playlist index is read as the call arrives, so one taken before the playlist list changed queues a row of another playlist; `playlistGuid` keeps naming the playlist it was read from. An entry giving both `playlist` and `playlistGuid`, or a malformed GUID, fails with `INVALID_PARAMS`, a GUID no playlist has with `NOT_FOUND`.
 */
export interface QueueSetContentsParams {
    /** The new queue, in order. At most `max(256, current queue length)` entries; more fail with `INVALID_PARAMS`. */
    items: QueueContentRef[];
}

/**
 * Parameters for `rating.get`.
 * Read a track's rating from 0 to 5: a foo_playcount `%rating%` between 1 and 5 wins, otherwise the file's `RATING` tag clamped to 0..5. `0` means unrated. A file whose tags cannot be read, a missing file included, is not an error: it reads as having no `RATING` tag.
 */
export interface RatingGetParams {
    /** Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong. */
    path: string;
    /**
     * Subsong index that wins over a suffix in `path`; negative values are ignored.
     * @default -1
     */
    cueIndex?: number;
}

/**
 * Parameters for `rating.set`.
 * Set a track's rating through foo_playcount's context menu, or write the file's `RATING` tag when that menu is not available. Without `path` the playing track is rated, or else the active playlist's selection; with neither the call fails with `NO_ACTIVE_ITEM`.
 */
export interface RatingSetParams {
    /** Track path; a `|subsong:N` suffix or a trailing `#N` selects a subsong. Omit it for the playing track, or else the active playlist's selection. */
    path?: string;
    /** Rating from 0 to 5; `0` clears it. */
    rating: number;
    /**
     * Subsong index that wins over a suffix in `path`; negative values are ignored.
     * @default -1
     */
    cueIndex?: number;
}

/**
 * Parameters for `replaygain.clear`.
 * Remove the ReplayGain values from files. The tags are rewritten in the background; read the files again to see the values gone.
 */
export interface ReplaygainClearParams {
    /** Tracks whose ReplayGain values are removed. A `|subsong:N` suffix selects a subsong, such as one track of a CUE sheet; a path without it means the first track. */
    paths: string[];
}

/**
 * Parameters for `replaygain.get`.
 * Read the ReplayGain values stored in files, one row per path. A file the host cannot read is a failed row, not a failed call.
 */
export interface ReplaygainGetParams {
    /** Tracks to read. A `|subsong:N` suffix selects a subsong, such as one track of a CUE sheet; a path without it reads the first track. */
    paths: string[];
}

/**
 * Parameters for `replaygain.getMode`.
 * Read the source mode and the processing mode.
 */
export type ReplaygainGetModeParams = Record<string, never>;

/**
 * Parameters for `replaygain.getPreamp`.
 * Read the two preamps, in dB.
 */
export type ReplaygainGetPreampParams = Record<string, never>;

/**
 * Parameters for `replaygain.getSettings`.
 * Read the whole ReplayGain configuration: source mode, processing mode, both preamps and whether ReplayGain is in effect.
 */
export type ReplaygainGetSettingsParams = Record<string, never>;

/**
 * Parameters for `replaygain.scan`.
 * Start foobar2000's own ReplayGain scanner on the files through its context menu command. The call returns when the scan has been started; results are written to the files by the scanner.
 */
export interface ReplaygainScanParams {
    /** Tracks to scan. A `|subsong:N` suffix selects a subsong, such as one track of a CUE sheet; a path without it means the first track. */
    paths: string[];
    /**
     * `track` scans each file's own gain; `album` scans the files as one album.
     * @default "track"
     */
    mode?: "track" | "album";
}

/**
 * Parameters for `replaygain.setMode`.
 * Change the source mode, the processing mode, or both; a key left out keeps its value. The response reports the modes in effect afterwards.
 */
export interface ReplaygainSetModeParams {
    /** A source mode to set; `byPlaybackOrder` is the older spelling of `auto`. */
    sourceMode?: "none" | "track" | "album" | "auto" | "byPlaybackOrder";
    /** What is applied: `gain` the gain only, `peak` peak limiting only, `gain_and_peak` both, `none` nothing. */
    processingMode?: "none" | "gain" | "gain_and_peak" | "peak";
}

/**
 * Parameters for `replaygain.setPreamp`.
 * Change one or both preamps; a key left out keeps its value. The response reports the values in effect afterwards.
 */
export interface ReplaygainSetPreampParams {
    /** Preamp for tracks that carry ReplayGain, in dB. */
    withRg?: number;
    /** Preamp for tracks without ReplayGain, in dB. */
    withoutRg?: number;
}

/**
 * Parameters for `selection.get`.
 * Read the current global selection as a page of handles. Observes state only; nothing is modified.
 */
export interface SelectionGetParams {
    /**
     * Index of the first handle to return.
     * @default 0
     */
    offset?: number;
    /** Maximum number of handles to return; `0` means every handle from `offset` on. When omitted the page is capped at 100 and `truncated` reports whether the cap applied. */
    limit?: number;
}

/**
 * Parameters for `selection.getType`.
 * Report the type of the current global selection, as a numeric index and as a name.
 */
export type SelectionGetTypeParams = Record<string, never>;

/**
 * Parameters for `selection.getViewerMode`.
 * Report which source the user's Selection Viewer preference favours, derived from the live selection type rather than read from a stored setting.
 */
export type SelectionGetViewerModeParams = Record<string, never>;

/**
 * Parameters for `selection.getViewingTrack`.
 * Resolve the track a viewer should show: the preferred source first, the other one when it has no track. Always succeeds; `found` says whether there was anything to show.
 */
export interface SelectionGetViewingTrackParams {
    /**
     * Also return the full track row as `track`.
     * @default false
     */
    includeTrackInfo?: boolean;
}

/**
 * Parameters for `selection.set`.
 * Replace the global selection with the given tracks. Handles are minted from the strings, not looked up, so a path that does not exist is selected just the same. The host holds its selection holder only for the duration of the call; foobar2000 clears the selection once no holder is left, so it outlives the call only while another one is held, such as the calling panel's own while that panel has focus with `grabFocus` on.
 */
export interface SelectionSetParams {
    /** Tracks to select, as native paths with an optional `|subsong:N` suffix. A suffix that is not a number is read as subsong `0`. */
    handles: string[];
}

/**
 * Parameters for `selection.setPlaylistTracking`.
 * Set the selection to the whole active playlist, or to that playlist's own selected rows. foobar2000 ends tracking when the holder that started it is released, and the host releases it when the call returns, so later playlist changes are not followed; how long the selection itself lasts is as for `set`.
 */
export interface SelectionSetPlaylistTrackingParams {
    /**
     * `playlist` takes the whole active playlist; `selection` takes its selected rows.
     * @default "selection"
     */
    mode?: "selection" | "playlist";
}

/**
 * Parameters for `shell.exec`.
 * Start a process from a command line and return at once; nothing is waited for. Commands are not allow-listed: a theme is trusted like an installed component. `cwd` goes through path security.
 */
export interface ShellExecParams {
    /** Command line, run as given. */
    command: string;
    /** Arguments appended to the command line, each quoted. */
    args?: string[];
    /** Working directory; the process inherits foobar2000's when omitted or empty. */
    cwd?: string;
    /**
     * Start the process without a visible window.
     * @default true
     */
    hidden?: boolean;
}

/**
 * Parameters for `shell.openExternal`.
 * Open an `http://`, `https://` or `mailto:` URL with the default handler.
 */
export interface ShellOpenExternalParams {
    /** URL to open; any other scheme fails with `INVALID_PARAMS`. */
    url: string;
}

/**
 * Parameters for `shell.openWith`.
 * Open a file with the application Windows associates with its type. Executables, scripts, installers, shortcuts and libraries are refused with `PERMISSION_DENIED`.
 */
export interface ShellOpenWithParams {
    /** File to hand to its associated application. */
    path: string;
}

/**
 * Parameters for `shell.showInExplorer`.
 * Reveal a file or folder in Windows Explorer, selecting it.
 */
export interface ShellShowInExplorerParams {
    /** File or folder to reveal. */
    path: string;
}

/**
 * Parameters for `shell.spawn`.
 * Start a process from an executable and an argument list. With `waitForExitMs` the call waits that long for an early exit; a non-zero exit within the wait fails with `OPERATION_FAILED` and the failure carries `processId`, `exited` and `exitCode`. An absolute executable path and `cwd` go through path security.
 */
export interface ShellSpawnParams {
    /** Executable name (resolved through PATH) or path. Surrounding whitespace and quotes are removed. */
    executable: string;
    /** Arguments passed to the process, each quoted. */
    args?: string[];
    /** Working directory, which has to exist; the process inherits foobar2000's when omitted or empty. */
    cwd?: string;
    /**
     * Start the process without a visible window.
     * @default true
     */
    hidden?: boolean;
    /**
     * Milliseconds to wait for an early exit; `0` returns as soon as the process starts.
     * @default 0
     */
    waitForExitMs?: number;
}

/**
 * Parameters for `state.delete`.
 * Remove a value from the shared state; when it existed, announce it with `state:deleted` and reason `deleted`.
 */
export interface StateDeleteParams {
    /** The key. */
    key: string;
}

/**
 * Parameters for `state.get`.
 * Read a value from the state shared by all windows. Expired values are removed first, each announced with `state:deleted` and reason `expired`.
 */
export interface StateGetParams {
    /** The key. */
    key: string;
}

/**
 * Parameters for `state.keys`.
 * List the keys of the shared state. Expired values are removed first, as in `state.get`.
 */
export interface StateKeysParams {
    /**
     * `*` for every key, a prefix followed by `*` such as `lyrics:*`, or an exact key.
     * @default "*"
     */
    pattern?: string;
}

/**
 * Parameters for `state.set`.
 * Store a value in the shared state and announce it with `state:changed`, unless `silent`.
 */
export interface StateSetParams {
    /** The key. */
    key: string;
    /** The value to store. `null` counts as missing; remove a key with `state.delete`. */
    value: JsonValue;
    /**
     * Store without announcing `state:changed`.
     * @default false
     */
    silent?: boolean;
    /** Lifetime in milliseconds, after which the value is removed; `0` or less stores it without expiry. */
    ttlMs?: number;
}

/**
 * Parameters for `system.getApisByNamespace`.
 * List the registered methods of one namespace; an unknown namespace lists nothing.
 */
export interface SystemGetApisByNamespaceParams {
    /** Namespace to list, such as `playback`. */
    namespace: string;
}

/**
 * Parameters for `system.getApiStats`.
 * Count the registered methods, overall and per namespace, and the registered plugins.
 */
export type SystemGetApiStatsParams = Record<string, never>;

/**
 * Parameters for `system.getDPI`.
 * Report the DPI of the display showing the calling window; the main window's when the caller cannot be resolved.
 */
export type SystemGetDPIParams = Record<string, never>;

/**
 * Parameters for `system.getLocale`.
 * Report the Windows user locale with its localized language and country names.
 */
export type SystemGetLocaleParams = Record<string, never>;

/**
 * Parameters for `system.getRegisteredPlugins`.
 * List the external plugins that registered methods with the bridge.
 */
export type SystemGetRegisteredPluginsParams = Record<string, never>;

/**
 * Parameters for `system.getTheme`.
 * Report the Windows personalization settings: app theme, accent color and transparency.
 */
export type SystemGetThemeParams = Record<string, never>;

/**
 * Parameters for `system.isPluginRegistered`.
 * Report whether an external plugin owns the namespace.
 */
export interface SystemIsPluginRegisteredParams {
    /** Namespace to look up. */
    namespace: string;
}

/**
 * Parameters for `system.listAvailableApis`.
 * List the methods the bridge currently accepts, built-in ones and those registered by external plugins.
 */
export interface SystemListAvailableApisParams {
    /**
     * Include built-in methods.
     * @default true
     */
    includeInternal?: boolean;
    /**
     * Include methods registered by external plugins.
     * @default true
     */
    includeExternal?: boolean;
}

/**
 * Parameters for `system.searchApis`.
 * Find methods whose full name or description contains `query`, ignoring case.
 */
export interface SystemSearchApisParams {
    /** Text to look for in full names and descriptions. */
    query: string;
}

/**
 * Parameters for `taskbar.flash`.
 * Flash the taskbar button to draw attention.
 */
export interface TaskbarFlashParams {
    /**
     * Number of flashes.
     * @default 3
     */
    count?: number;
    /**
     * Milliseconds between flashes; `0` uses the system cursor blink rate.
     * @default 0
     */
    interval?: number;
}

/**
 * Parameters for `taskbar.setOverlayIcon`.
 * Draw a small overlay badge on the taskbar button, or clear it.
 */
export interface TaskbarSetOverlayIconParams {
    /** Overlay icon as raw Base64 of an `.ico` file, without a prefix. Empty, `null` or omitted clears the overlay. */
    icon?: string | null;
    /**
     * Accessibility text for the overlay.
     * @default ""
     */
    description?: string;
}

/**
 * Parameters for `taskbar.setProgress`.
 * Set the progress bar drawn on the taskbar button. Once a theme sets it, the plugin stops mirroring playback progress there until the next playback state change.
 */
export interface TaskbarSetProgressParams {
    /**
     * Progress bar state. `none` removes the bar.
     * @default "none"
     */
    state?: "none" | "indeterminate" | "normal" | "error" | "paused";
    /** Fill fraction, meaningful for the `normal`, `error` and `paused` states. */
    value?: number;
}

/**
 * Parameters for `taskbar.setThumbnailButtons`.
 * Install the thumbnail toolbar shown on the taskbar preview. Windows lets a window install it once; change the buttons afterwards with `taskbar.updateButton`.
 */
export interface TaskbarSetThumbnailButtonsParams {
    /** Buttons in display order, at most seven. A longer list fails the whole call; it is never truncated. */
    buttons: ThumbnailButton[];
}

/**
 * Parameters for `taskbar.updateButton`.
 * Update one installed thumbnail button in place. Buttons cannot be added or removed.
 */
export interface TaskbarUpdateButtonParams {
    /** Id of the button, as passed to `taskbar.setThumbnailButtons`. */
    id: string;
    /** New icon as raw Base64 of an `.ico` file, without a prefix. Empty, `null` or omitted keeps the current icon. */
    icon?: string | null;
    /**
     * New hover text. Empty or omitted keeps the current text.
     * @default ""
     */
    tooltip?: string;
    /** Whether the button accepts clicks. Omitted keeps the current state. */
    enabled?: boolean;
    /** Whether the button is shown. Omitted keeps the current state. */
    visible?: boolean;
}

/**
 * Parameters for `titleformat.eval`.
 * Evaluate one title formatting pattern against one track, or against the playing track when no path is given.
 */
export interface TitleformatEvalParams {
    /** Path of the track to evaluate against; a `|subsong:N` suffix selects a subsong, such as one track of a CUE sheet. Omit it to evaluate against the playing track, which also fills dynamic fields such as `%playback_time%` and stream titles; the call then fails with `NO_ACTIVE_ITEM` when nothing is playing. */
    path?: string;
    /** Title formatting pattern, such as `%artist% - %title%`. */
    pattern: string;
}

/**
 * Parameters for `titleformat.evalBatch`.
 * Evaluate one pattern against many tracks. The pattern is compiled once.
 */
export interface TitleformatEvalBatchParams {
    /** Paths of the tracks to evaluate against; a `|subsong:N` suffix selects a subsong. */
    paths: string[];
    /** Title formatting pattern applied to every path. */
    pattern: string;
}

/**
 * Parameters for `titleformat.evalFields`.
 * Evaluate several named patterns against one track in a single pass. The patterns are compiled as one script, so a pattern that fails to compile does not fail the call: the result then has no field keys and no `infoAvailable`, unlike `titleformat.evalFieldsBatch`, which fails with `INVALID_PARAMS`.
 */
export interface TitleformatEvalFieldsParams {
    /** Path of the track to evaluate against; a `|subsong:N` suffix selects a subsong. */
    path: string;
    /** Map from output key to title formatting pattern, such as `{ "year": "$year(%date%)" }`. */
    fields: Record<string, string>;
}

/**
 * Parameters for `titleformat.evalFieldsBatch`.
 * Evaluate several named patterns against many tracks. The merged pattern is compiled once.
 */
export interface TitleformatEvalFieldsBatchParams {
    /** Paths of the tracks to evaluate against; a `|subsong:N` suffix selects a subsong. */
    paths: string[];
    /** Map from output key to title formatting pattern. */
    fields: Record<string, string>;
}

/**
 * Parameters for `titleformat.getBuiltinFields`.
 * List commonly used title formatting fields, keyed by a readable name.
 */
export type TitleformatGetBuiltinFieldsParams = Record<string, never>;

/**
 * Parameters for `tray.appendMenuItems`.
 * Append rows to one menu zone; the same validation as `setContextMenu`.
 */
export interface TrayAppendMenuItemsParams {
    /** Rows to append. */
    items: TrayMenuItem[];
    /**
     * Zone to append to.
     * @default "top"
     */
    position?: "top" | "playback" | "bottom";
}

/**
 * Parameters for `tray.clearMenuItems`.
 * Clear one zone, or every zone when `position` is omitted.
 */
export interface TrayClearMenuItemsParams {
    /** Zone to clear; every zone when omitted. */
    position?: "top" | "playback" | "bottom";
}

/**
 * Parameters for `tray.create`.
 * Create the tray icon. Call it once before any other `tray.*` method: without it the other calls succeed but no icon shows, no tray event fires and `isVisible` reports `false`. Fails in panel mode, when the main window has no handle, and when the shell refuses the icon.
 */
export interface TrayCreateParams {
    /** Icon to show. */
    icon?: string;
    /**
     * Hover text.
     * @default "foobar2000"
     */
    tooltip?: string;
}

/**
 * Parameters for `tray.destroy`.
 * Remove the tray icon.
 */
export type TrayDestroyParams = Record<string, never>;

/**
 * Parameters for `tray.getMenuItems`.
 * List the user rows of every zone, flattened in `top`, `playback`, `bottom` order, as they were stored. The rows the runtime injects for `showPlaybackControls` and `showSystemItems` are not included.
 */
export type TrayGetMenuItemsParams = Record<string, never>;

/**
 * Parameters for `tray.isVisible`.
 * Report whether the tray icon exists.
 */
export type TrayIsVisibleParams = Record<string, never>;

/**
 * Parameters for `tray.removeMenuItems`.
 * Remove rows by id from every zone, submenus included.
 */
export interface TrayRemoveMenuItemsParams {
    /** Ids of the rows to remove. */
    ids: string[];
}

/**
 * Parameters for `tray.setCloseToTray`.
 * Hide the window to the tray instead of quitting when it is closed.
 */
export interface TraySetCloseToTrayParams {
    /** `true` hides to the tray. */
    enabled: boolean;
}

/**
 * Parameters for `tray.setContextMenu`.
 * Replace the user rows of one menu zone (`config.customPosition`, default `top`); the other zones are left as they are. Nothing is stored when any row is rejected or the resource limits are exceeded. An ordinary row reports its click through `tray:menuItemClicked`; the built-in playback and system rows, and rows declaring `playbackAction`, run natively and do not. To replace every zone at once, use `setMenuZones`.
 */
export interface TraySetContextMenuParams {
    /** The rows of the zone. */
    items: TrayMenuItem[];
    /** Menu-wide options to change. */
    config?: TrayMenuConfig;
}

/**
 * Parameters for `tray.setIcon`.
 * Replace the tray icon image; fails while no icon is created.
 */
export interface TraySetIconParams {
    /** Icon to show. */
    icon?: string;
}

/**
 * Parameters for `tray.setMenuItemState`.
 * Change one row's `checked` or `enabled` state in place, searching every zone and submenu for the id. Giving `checked` (even `false`) makes the row checkable. The native menu is rebuilt each time it opens, so the change shows on the next opening.
 */
export interface TraySetMenuItemStateParams {
    /** Id of the row. */
    id: string;
    /** New checkmark state. */
    checked?: boolean;
    /** New enabled state. */
    enabled?: boolean;
}

/**
 * Parameters for `tray.setMenuZones`.
 * Replace the user rows of all three zones in one call; a zone that is not given is cleared. Rows and `config` follow the rules of `setContextMenu`, and nothing is stored, `config` included, when any row is rejected or the resource limits are exceeded. Use it when the whole menu changes, instead of `clearMenuItems` followed by `appendMenuItems`: the menu can open between separate calls and show only part of the update, and two such updates that interleave add their rows twice.
 */
export interface TraySetMenuZonesParams {
    /** Rows of the `top` zone; omitted clears the zone. */
    top?: TrayMenuItem[];
    /** Rows of the `playback` zone; omitted clears the zone. The rows `config.showPlaybackControls` injects are not stored in the zone, so clearing it leaves them in place. */
    playback?: TrayMenuItem[];
    /** Rows of the `bottom` zone; omitted clears the zone. The rows `config.showSystemItems` injects are not stored in the zone, so clearing it leaves them in place. */
    bottom?: TrayMenuItem[];
    /** Menu-wide options to change. `customPosition` is stored for later `setContextMenu` calls and does not affect this one. */
    config?: TrayMenuConfig;
}

/**
 * Parameters for `tray.setMinimizeToTray`.
 * Hide the window to the tray instead of the taskbar when it is minimized.
 */
export interface TraySetMinimizeToTrayParams {
    /** `true` hides to the tray. */
    enabled: boolean;
}

/**
 * Parameters for `tray.setTooltip`.
 * Update the icon's hover text; fails while no icon is created.
 */
export interface TraySetTooltipParams {
    /**
     * Hover text; empty clears it.
     * @default ""
     */
    tooltip?: string;
}

/**
 * Parameters for `tray.showBalloon`.
 * Show a balloon notification from the tray icon; fails while no icon is created.
 */
export interface TrayShowBalloonParams {
    /**
     * Notification title.
     * @default ""
     */
    title?: string;
    /**
     * Notification body.
     * @default ""
     */
    message?: string;
    /**
     * Icon shown in the balloon.
     * @default "info"
     */
    icon?: "info" | "warning" | "error";
}

/**
 * Parameters for `ui.hideNotification`.
 * Hide the balloon shown by `showNotification`; nothing to do when none was shown.
 */
export type UiHideNotificationParams = Record<string, never>;

/**
 * Parameters for `ui.showContextMenu`.
 * Open the main window's own context menu. Coordinates that are omitted, not positive, or more than 50 px away from the real cursor are replaced by the cursor position.
 */
export interface UiShowContextMenuParams {
    /**
     * Screen x in pixels.
     * @default -1
     */
    x?: number;
    /**
     * Screen y in pixels.
     * @default -1
     */
    y?: number;
}

/**
 * Parameters for `ui.showCustomMenu`.
 * Show a native popup menu at the system cursor position and wait for the choice. The chosen row is reported as `selectedId` and also announced to the calling window as `ui:menuItemClicked` with `{ id, label }`; dismissing the menu reports `selectedId: null`.
 */
export interface UiShowCustomMenuParams {
    /** The rows. */
    items: UiMenuItem[];
    /**
     * Accepted for compatibility; the menu opens at the system cursor.
     * @default 0
     */
    x?: number;
    /**
     * Accepted for compatibility; the menu opens at the system cursor.
     * @default 0
     */
    y?: number;
    /**
     * With `h`, the size of a rectangle below the cursor the menu must not cover; the menu then opens below it.
     * @default 0
     */
    w?: number;
    /**
     * See `w`.
     * @default 0
     */
    h?: number;
    /**
     * Accepted for compatibility; has no effect.
     * @default false
     */
    suppressDefault?: boolean;
}

/**
 * Parameters for `ui.showNotification`.
 * Show a Windows balloon notification from the plugin's notification icon, creating the icon on first use. At least one of `title` and `body` must be given.
 */
export interface UiShowNotificationParams {
    /**
     * Notification title.
     * @default ""
     */
    title?: string;
    /**
     * Notification body.
     * @default ""
     */
    body?: string;
    /**
     * `true` shows it without the notification sound.
     * @default false
     */
    silent?: boolean;
    /**
     * Display time in milliseconds, as a hint to the shell.
     * @default 5000
     */
    timeout?: number;
}

/**
 * Parameters for `ui.showToast`.
 * Ask the calling window's page to show a toast: nothing is painted natively, the payload is delivered to that window as `ui:toast` with `{ message, duration, type, position }`.
 */
export interface UiShowToastParams {
    /** Toast text. */
    message: string;
    /**
     * Display time in milliseconds.
     * @default 3000
     */
    duration?: number;
    /**
     * Toast kind, passed through to the page.
     * @default "info"
     */
    type?: "info" | "success" | "warning" | "error";
    /**
     * Screen corner, passed through to the page.
     * @default "bottom-right"
     */
    position?: string;
}

/**
 * Parameters for `webview.getSource`.
 * Report where the host loaded the calling WebView's page from: the development server, a URL, a template folder, or the built-in "Frontend not found" page. The host records this when it submits the navigation; a page that later navigates elsewhere by itself is still reported by that record. Fails with `NOT_FOUND` when the caller is not a WebView of this plugin or the host has not loaded a page into it.
 */
export type WebviewGetSourceParams = Record<string, never>;

/**
 * Parameters for `window.blur`.
 * Hand the foreground to the window below the calling window in the z-order. Fails when there is no window.
 */
export type WindowBlurParams = Record<string, never>;

/**
 * Parameters for `window.broadcast`.
 * Send a message to every window except the caller, delivered as `window:message` with `{ sourceWindowId, message }`.
 */
export interface WindowBroadcastParams {
    /** The message, delivered as `message` of the event. `null` counts as missing. */
    message: JsonValue;
}

/**
 * Parameters for `window.cancelClose`.
 * Keep the calling popup open after it received `window:beforeClose`; nothing happens when no close is pending. Fails with `NOT_FOUND` when the caller is not a popup.
 */
export type WindowCancelCloseParams = Record<string, never>;

/**
 * Parameters for `window.center`.
 * Center the calling window on the work area of its monitor, keeping its size. Fails in panel mode and when there is no window.
 */
export type WindowCenterParams = Record<string, never>;

/**
 * Parameters for `window.clearClickThroughExcludeRegions`.
 * Remove a popup's click-through exclude rectangles. Targets as `window.setClickThrough` does.
 */
export interface WindowClearClickThroughExcludeRegionsParams {
    /** Id of the popup; omitted or empty, the calling window. */
    windowId?: string;
}

/**
 * Parameters for `window.clearDragRegions`.
 * Remove the calling window's drag rectangles. Fails in panel mode, and with `NOT_FOUND` when the caller is neither the main window nor a popup.
 */
export type WindowClearDragRegionsParams = Record<string, never>;

/**
 * Parameters for `window.clearNoDragRegions`.
 * Remove the calling window's no-drag rectangles. Fails in panel mode, and with `NOT_FOUND` when the caller is neither the main window nor a popup.
 */
export type WindowClearNoDragRegionsParams = Record<string, never>;

/**
 * Parameters for `window.close`.
 * Close the calling window as the system close command does. Succeeds without doing anything when there is no window. Fails in panel mode.
 */
export type WindowCloseParams = Record<string, never>;

/**
 * Parameters for `window.closeAllPopups`.
 * Close every popup as their close buttons do.
 */
export type WindowCloseAllPopupsParams = Record<string, never>;

/**
 * Parameters for `window.closePopup`.
 * Close a popup as its close button does, so a popup created with `beforeClose` first gets `window:beforeClose`. `main` fails with `INVALID_PARAMS`, an id no popup has with `NOT_FOUND`.
 */
export interface WindowClosePopupParams {
    /** Id of the popup to close. */
    windowId: string;
}

/**
 * Parameters for `window.confirmClose`.
 * Let the calling popup close after it received `window:beforeClose`; nothing happens when no close is pending. Fails with `NOT_FOUND` when the caller is not a popup.
 */
export type WindowConfirmCloseParams = Record<string, never>;

/**
 * Parameters for `window.createPopup`.
 * Open a popup window with its own WebView and bridge; at most 8 popups can be open. Works from a DUI/CUI panel too. Fails with `OPERATION_FAILED` when 8 popups are already open or the window cannot be created.
 */
export interface WindowCreatePopupParams {
    /**
     * Page to load. An `http://`, `https://`, `file:///` or `data:` URL is loaded as it is; a path containing `.html` is loaded from the theme (or the development server); anything else, including empty, loads the theme's `index.html` with this value as the `route` query parameter. Every URL except `data:` gets a `windowId` query parameter. A page loaded from an `http://` or `https://` URL can use the bridge only when the calling page already trusts that origin; any other absolute URL loads a page that cannot. Such a page still shows, but its calls fail with `ORIGIN_DENIED` and no event reaches it.
     * @default ""
     */
    url?: string;
    /**
     * Window title.
     * @default ""
     */
    title?: string;
    /** Left edge in screen coordinates; omitted, Windows places the window. Fractions are dropped. */
    x?: number;
    /** Top edge in screen coordinates; omitted, Windows places the window. Fractions are dropped. */
    y?: number;
    /**
     * Width in physical pixels; fractions are dropped.
     * @default 400
     */
    width?: number;
    /**
     * Height in physical pixels; fractions are dropped.
     * @default 300
     */
    height?: number;
    /**
     * Minimum width for resizing, in physical pixels.
     * @default 200
     */
    minWidth?: number;
    /**
     * Minimum height for resizing, in physical pixels.
     * @default 150
     */
    minHeight?: number;
    /**
     * Maximum width for resizing, in physical pixels; `0` means no bound.
     * @default 0
     */
    maxWidth?: number;
    /**
     * Maximum height for resizing, in physical pixels; `0` means no bound.
     * @default 0
     */
    maxHeight?: number;
    /**
     * Whether the user can resize the popup.
     * @default true
     */
    resizable?: boolean;
    /**
     * Whether to draw the native frame and caption; `false` creates a borderless popup.
     * @default true
     */
    frame?: boolean;
    /**
     * Whether the background is transparent.
     * @default false
     */
    transparent?: boolean;
    /**
     * Keep the popup above every other window. To keep it above the main window only, use the `standard` preset or `behavior.owner: "main"` instead.
     * @default false
     */
    alwaysOnTop?: boolean;
    /** Whether the popup shows on the taskbar and in Alt+Tab; omitted, the preset decides. */
    showInTaskbar?: boolean;
    /**
     * Let mouse input pass through the popup.
     * @default false
     */
    clickThrough?: boolean;
    /**
     * Send `window:beforeClose` and wait for `window.confirmClose` or `window.cancelClose` before closing.
     * @default false
     */
    beforeClose?: boolean;
    /** Behavior preset: `standard`, `miniPlayer` or `desktopLyrics`, case-insensitive, with `-` or `_` allowed (`mini-player`); an unknown value is `standard`. Omitted, no preset. */
    profile?: string;
    /** Behavior overrides on top of the preset, keyed like `resolvedBehavior` of `window.getPopupBehavior`. */
    behavior?: Record<string, JsonValue>;
    /** Backdrop policy overrides on top of the preset, keyed like `resolvedBackdropPolicy` of `window.getBackdropPolicy`. */
    backdropPolicy?: Record<string, JsonValue>;
}

/**
 * Parameters for `window.enterFullscreen`.
 * Enter fullscreen. A window that is already fullscreen fails with `OPERATION_FAILED`. Fails in panel mode.
 */
export interface WindowEnterFullscreenParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
}

/**
 * Parameters for `window.exitFullscreen`.
 * Leave fullscreen and restore the state the window had before. A window that is not fullscreen fails with `OPERATION_FAILED`. Fails in panel mode.
 */
export interface WindowExitFullscreenParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
}

/**
 * Parameters for `window.flash`.
 * Flash the calling window's caption and taskbar button to draw attention, or stop flashing. Fails in panel mode and when there is no window.
 */
export interface WindowFlashParams {
    /**
     * `true` to flash, `false` to stop.
     * @default true
     */
    enabled?: boolean;
    /**
     * How many times to flash.
     * @default 3
     */
    count?: number;
}

/**
 * Parameters for `window.flashTaskbar`.
 * Flash the calling window's taskbar button and caption a number of times. Fails in panel mode and when there is no window.
 */
export interface WindowFlashTaskbarParams {
    /**
     * How many times to flash.
     * @default 3
     */
    count?: number;
}

/**
 * Parameters for `window.focus`.
 * Bring a window to the foreground, restoring it first when minimized and showing it when hidden. Fails with `NOT_FOUND` when there is no such window.
 */
export interface WindowFocusParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
}

/**
 * Parameters for `window.getAllWindows`.
 * List the main window and every popup with their policies, capabilities and rectangles.
 */
export type WindowGetAllWindowsParams = Record<string, never>;

/**
 * Parameters for `window.getBackdropPolicy`.
 * Report a window's backdrop policy: the overrides set on it and the policy in effect.
 */
export interface WindowGetBackdropPolicyParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
}

/**
 * Parameters for `window.getBounds`.
 * Report the calling window's rectangle in screen coordinates, in physical pixels, frame included. While the window is minimized this is where Windows parks it, not the geometry it restores to. The rectangle is zero when there is no window.
 */
export type WindowGetBoundsParams = Record<string, never>;

/**
 * Parameters for `window.getCaptionButtonsWidth`.
 * Report the width of the main window's caption buttons (minimize, maximize, close) in physical pixels at its DPI. Main window only; without one the call reports `138` and `46` rather than failing.
 */
export type WindowGetCaptionButtonsWidthParams = Record<string, never>;

/**
 * Parameters for `window.getCornerPreference`.
 * Report the main window's corner rounding as last set; `default` when there is no main window.
 */
export type WindowGetCornerPreferenceParams = Record<string, never>;

/**
 * Parameters for `window.getCurrentWindowId`.
 * Report the id of the calling window: `main`, a popup id or a panel's id. A caller that cannot be matched gets `main`.
 */
export type WindowGetCurrentWindowIdParams = Record<string, never>;

/**
 * Parameters for `window.getDevServerConfig`.
 * Report whether pages load from a development server instead of the installed files, and that server's address.
 */
export type WindowGetDevServerConfigParams = Record<string, never>;

/**
 * Parameters for `window.getDpiScale`.
 * Report the DPI of the calling window's device context and its ratio to 96; `96` when there is no window.
 */
export type WindowGetDpiScaleParams = Record<string, never>;

/**
 * Parameters for `window.getMaxSize`.
 * Report a window's requested maximum size; `0` means no bound.
 */
export interface WindowGetMaxSizeParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
}

/**
 * Parameters for `window.getMinSize`.
 * Report a window's requested minimum size, which is the value last set rather than the effective window size.
 */
export interface WindowGetMinSizeParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
}

/**
 * Parameters for `window.getMode`.
 * Report the calling page's hosting mode and window id. The main window and popups report `standalone` with `panelMode: false`, including popups opened from a panel. A registered window with an id reports the same id as `window.getCurrentWindowId`. Pages that adapt to panel mode read it on startup; DUI/CUI panels also announce the same fields with `panel:initialized`.
 */
export type WindowGetModeParams = Record<string, never>;

/**
 * Parameters for `window.getPopupBehavior`.
 * Report a popup's behavior preset, the overrides set on it and the behavior in effect. Popups only: `main` fails with `NOT_SUPPORTED`; without `windowId` the calling popup is used, and a caller that is not a popup fails with `NOT_FOUND`.
 */
export interface WindowGetPopupBehaviorParams {
    /** Id of the popup; omitted or empty, the calling popup. */
    windowId?: string;
}

/**
 * Parameters for `window.getState`.
 * Report the calling window's state flags and rectangle. Every flag is sent twice, bare and `is`-prefixed. When there is no window every flag is `false` and the rectangle is zero.
 */
export type WindowGetStateParams = Record<string, never>;

/**
 * Parameters for `window.getTitle`.
 * Report the calling window's title, up to 255 characters; empty when there is no window.
 */
export type WindowGetTitleParams = Record<string, never>;

/**
 * Parameters for `window.getTitlebarHeight`.
 * Report the title bar height of the calling window, the main window or a popup, in physical pixels. A caller that is neither gets the default, `32`.
 */
export type WindowGetTitlebarHeightParams = Record<string, never>;

/**
 * Parameters for `window.getTitlebarInfo`.
 * Report the main window's title bar height, caption button widths and maximized state together, in physical pixels. Main window only; without one the call reports `32`, `138`, `46` and `false`.
 */
export type WindowGetTitlebarInfoParams = Record<string, never>;

/**
 * Parameters for `window.getZoom`.
 * Report the zoom factor of the calling window's WebView and the window's DPI. Without a WebView of its own the call reports zoom `1` and leaves out `dpi` and `dpiScale`.
 */
export type WindowGetZoomParams = Record<string, never>;

/**
 * Parameters for `window.hasSavedBounds`.
 * Report whether an earlier session saved the main window's position, which a page can use to decide whether to apply a default size on first launch.
 */
export type WindowHasSavedBoundsParams = Record<string, never>;

/**
 * Parameters for `window.isAlwaysOnTop`.
 * Report whether the calling window is kept above other windows; `false` when there is no window.
 */
export type WindowIsAlwaysOnTopParams = Record<string, never>;

/**
 * Parameters for `window.isClickThrough`.
 * Report whether mouse input passes through a popup. Targets as `window.setClickThrough` does.
 */
export interface WindowIsClickThroughParams {
    /** Id of the popup; omitted or empty, the calling window. */
    windowId?: string;
}

/**
 * Parameters for `window.isFullscreen`.
 * Report whether a window is fullscreen.
 */
export interface WindowIsFullscreenParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
}

/**
 * Parameters for `window.isMaximized`.
 * Report whether the calling window is maximized; `false` when there is no window.
 */
export type WindowIsMaximizedParams = Record<string, never>;

/**
 * Parameters for `window.isMinimized`.
 * Report whether the calling window is minimized; `false` when there is no window.
 */
export type WindowIsMinimizedParams = Record<string, never>;

/**
 * Parameters for `window.isResizable`.
 * Report whether the user can resize a window by its frame.
 */
export interface WindowIsResizableParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
}

/**
 * Parameters for `window.maximize`.
 * Maximize the calling window with the system animation, leaving fullscreen first when it is fullscreen. The request is posted. Fails in panel mode and when there is no window.
 */
export type WindowMaximizeParams = Record<string, never>;

/**
 * Parameters for `window.minimize`.
 * Minimize the calling window with the system animation. The request is posted, so the window changes state after the call returns. Fails in panel mode and when there is no window.
 */
export type WindowMinimizeParams = Record<string, never>;

/**
 * Parameters for `window.refreshWebView`.
 * Redraw the calling window's WebView, which clears rendering left behind by a backdrop change. Fails with `NOT_FOUND` when the calling window has no WebView of its own, as with a DUI/CUI panel.
 */
export type WindowRefreshWebViewParams = Record<string, never>;

/**
 * Parameters for `window.reload`.
 * Reload the calling window's page. Fails with `NOT_FOUND` when the calling window has no WebView of its own, as with a DUI/CUI panel.
 */
export type WindowReloadParams = Record<string, never>;

/**
 * Parameters for `window.resetZoom`.
 * Set the zoom factor of the calling window's WebView back to `1`; as with `window.setZoom`, the default zoom in the preferences no longer applies to it afterwards. Fails with `NOT_FOUND` when the calling window has no WebView of its own, and with `OPERATION_FAILED` when WebView2 refuses the factor.
 */
export type WindowResetZoomParams = Record<string, never>;

/**
 * Parameters for `window.restore`.
 * Restore the calling window from minimized or maximized with the system animation. A fullscreen window leaves fullscreen instead and returns to the state it had before. Fails in panel mode and when there is no window.
 */
export type WindowRestoreParams = Record<string, never>;

/**
 * Parameters for `window.sendMessage`.
 * Send a message to one window, delivered as `window:message` with `{ sourceWindowId, message }`. Fails with `NOT_FOUND` when no window has that id.
 */
export interface WindowSendMessageParams {
    /** Id of the receiving window. */
    targetWindowId: string;
    /** The message, delivered as `message` of the event. `null` counts as missing. */
    message: JsonValue;
}

/**
 * Parameters for `window.setAcrylic`.
 * Turn the acrylic backdrop of a window on or off. The setting is stored even when the window does not draw it right away; the call then fails with `OPERATION_FAILED` and the failure carries the result fields. Fails in panel mode.
 */
export interface WindowSetAcrylicParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /**
     * `true` shows acrylic; `false` removes the backdrop set through `setMica` or `setAcrylic`.
     * @default true
     */
    enabled?: boolean;
    /** Dark (`true`) or light (`false`) backdrop; omitted, unchanged. */
    darkMode?: boolean;
}

/**
 * Parameters for `window.setAlwaysOnTop`.
 * Keep the calling window above other windows, or stop doing so. Fails in panel mode and when there is no window.
 */
export interface WindowSetAlwaysOnTopParams {
    /**
     * Whether to keep the window above other windows.
     * @default true
     */
    enabled?: boolean;
}

/**
 * Parameters for `window.setBackdropPolicy`.
 * Merge per-field overrides into a window's backdrop policy; a `null` value removes that override. The overrides are stored even when the window does not draw the result right away, for example while it is still hidden at startup; the call then fails with `OPERATION_FAILED`.
 */
export interface WindowSetBackdropPolicyParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /** Overrides keyed like `resolvedBackdropPolicy`, merged into the current ones; a `null` value removes that override. Other keys are stored and echoed but have no effect, and an effect name outside the allowed values leaves the effect in use unchanged. */
    backdropPolicy: Record<string, JsonValue>;
}

/**
 * Parameters for `window.setBackgroundTransparency`.
 * Make a window's WebView background transparent, so the backdrop effect shows through, or opaque. Fails in panel mode, and with `OPERATION_FAILED` when neither the window nor its WebView took the change; that failure carries the result fields unless the window has no WebView.
 */
export interface WindowSetBackgroundTransparencyParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /**
     * `true` for a transparent background, `false` for an opaque one.
     * @default true
     */
    transparent?: boolean;
}

/**
 * Parameters for `window.setBlur`.
 * Turn the blur-behind effect of a window on or off. The setting is stored even when the window does not draw it right away; the call then fails with `OPERATION_FAILED` and the failure carries `enabled`. Fails in panel mode.
 */
export interface WindowSetBlurParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /**
     * Whether to blur what is behind the window.
     * @default true
     */
    enabled?: boolean;
}

/**
 * Parameters for `window.setBounds`.
 * Move or resize the calling window in one call; omitted keys keep their current value and fractions are dropped. Fails in panel mode and when there is no window.
 */
export interface WindowSetBoundsParams {
    /** New left edge in screen coordinates. */
    x?: number;
    /** New top edge in screen coordinates. */
    y?: number;
    /** New width in physical pixels, frame included. */
    width?: number;
    /** New height in physical pixels, frame included. */
    height?: number;
}

/**
 * Parameters for `window.setClickThrough`.
 * Let mouse input pass through a popup to the windows below, except over the rectangles set with `window.setClickThroughExcludeRegions`. Popups only: without `windowId` the calling window is used, and a window that is not a popup fails with `NOT_FOUND`.
 */
export interface WindowSetClickThroughParams {
    /** Id of the popup; omitted or empty, the calling window. */
    windowId?: string;
    /**
     * Whether mouse input passes through.
     * @default true
     */
    enabled?: boolean;
}

/**
 * Parameters for `window.setClickThroughExcludeRegions`.
 * Set the rectangles of a popup that keep taking mouse input while it lets input pass through, replacing the earlier ones. Rectangles are CSS pixels of the page, scaled by the popup's DPI; rectangles without a positive width and height are skipped, and at most 32 are kept. Targets as `window.setClickThrough` does.
 */
export interface WindowSetClickThroughExcludeRegionsParams {
    /** Id of the popup; omitted or empty, the calling window. */
    windowId?: string;
    /** The rectangles; omitted, none. */
    regions?: WindowRegion[];
}

/**
 * Parameters for `window.setCornerPreference`.
 * Set the main window's Windows 11 corner rounding. Main window only, whichever window calls; popups manage their corners themselves. Fails in panel mode and when there is no main window.
 */
export interface WindowSetCornerPreferenceParams {
    /**
     * `default` or `round` rounds the corners, `small` rounds them slightly and `none` keeps them square. Any other value rounds them and is reported back as given.
     * @default "default"
     */
    mode?: string;
}

/**
 * Parameters for `window.setDarkMode`.
 * Switch a window's backdrop between its dark and light variant. The setting is stored even when the window does not draw it right away; the call then fails with `OPERATION_FAILED` and the failure carries `enabled`. Fails in panel mode.
 */
export interface WindowSetDarkModeParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /**
     * `true` for the dark variant, `false` for the light one.
     * @default true
     */
    enabled?: boolean;
}

/**
 * Parameters for `window.setDevServerConfig`.
 * Change the development server settings; omitted keys keep their value. They are used the next time a page loads. The response reports both settings as stored.
 */
export interface WindowSetDevServerConfigParams {
    /** Whether pages load from the development server. */
    useDevServer?: boolean;
    /** Address of the development server, such as `http://localhost:5173`; stored as given. */
    devServerUrl?: string;
}

/**
 * Parameters for `window.setDragRegions`.
 * Set the rectangles that drag the calling window, the main window or a popup, replacing the earlier ones. Rectangles are CSS pixels of the page, scaled by the window's DPI; rectangles without a positive width and height are skipped. Fails in panel mode, and with `NOT_FOUND` when the caller is neither the main window nor a popup.
 */
export interface WindowSetDragRegionsParams {
    /** The drag rectangles; omitted, none. */
    regions?: WindowRegion[];
}

/**
 * Parameters for `window.setFrameless`.
 * Remove or restore the native frame and caption of a window. Fails in panel mode.
 */
export interface WindowSetFramelessParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /**
     * `true` removes the frame, `false` restores it.
     * @default true
     */
    frameless?: boolean;
}

/**
 * Parameters for `window.setFullscreen`.
 * Enter or leave fullscreen. Setting the state the window already has succeeds. Leaving fullscreen restores the rectangle, the maximized state and the always-on-top state the window had before. Fails in panel mode.
 */
export interface WindowSetFullscreenParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /**
     * `true` to enter fullscreen, `false` to leave it.
     * @default true
     */
    enabled?: boolean;
}

/**
 * Parameters for `window.setMaximizeButtonRegion`.
 * Tell the host where the page draws the main window's maximize button, so that on Windows 11 hovering it offers Snap layouts as a standard window's button does. The rectangle is CSS pixels of the page, scaled by the window's DPI and the page zoom, the same factor as `devicePixelRatio`, and replaces the earlier one; `region` omitted, or without a positive width and height, removes it. While the window is frameless, resizable and not full screen, the host answers the rectangle as the window's maximize button and passes the mouse input it receives there on to the page, so the button keeps its hover and pressed styles and its own click handler; on Windows 10 and in every other state the rectangle stays ordinary page content. Set it again whenever layout moves the button; a navigation or reload of the page removes it. Main window only: fails with `NOT_SUPPORTED` from a popup, in panel mode, and with `NOT_FOUND` when the caller is no window.
 */
export interface WindowSetMaximizeButtonRegionParams {
    /** Where the page draws the maximize button; omitted, the host forgets the button. */
    region?: WindowRegion;
}

/**
 * Parameters for `window.setMaxSize`.
 * Set a window's maximum size; `0` removes the bound and fractions are dropped. The host keeps it in DIPs of the target window, so it reads back within 1 px (`0` exactly). A window larger than the new maximum shrinks at once, unless it is maximized, fullscreen or minimized. Fails in panel mode.
 */
export interface WindowSetMaxSizeParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /**
     * Maximum width in physical pixels; `0` removes the bound.
     * @default 0
     */
    width?: number;
    /**
     * Maximum height in physical pixels; `0` removes the bound.
     * @default 0
     */
    height?: number;
}

/**
 * Parameters for `window.setMica`.
 * Turn the Mica backdrop of a window on or off. The setting is stored even when the window does not draw it right away, for example while it is still hidden at startup; the call then fails with `OPERATION_FAILED` and the failure carries the result fields. Fails in panel mode.
 */
export interface WindowSetMicaParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /**
     * `true` shows Mica; `false` removes the backdrop set through `setMica` or `setAcrylic`.
     * @default true
     */
    enabled?: boolean;
    /**
     * `mica-alt` for the tabbed variant; any other value is `mica`.
     * @default "mica"
     */
    variant?: string;
    /** Dark (`true`) or light (`false`) backdrop; omitted, unchanged. */
    darkMode?: boolean;
}

/**
 * Parameters for `window.setMicaEffect`.
 * Same as `window.setMica`.
 */
export interface WindowSetMicaEffectParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /**
     * `true` shows Mica; `false` removes the backdrop set through `setMica` or `setAcrylic`.
     * @default true
     */
    enabled?: boolean;
    /**
     * `mica-alt` for the tabbed variant; any other value is `mica`.
     * @default "mica"
     */
    variant?: string;
    /** Dark (`true`) or light (`false`) backdrop; omitted, unchanged. */
    darkMode?: boolean;
}

/**
 * Parameters for `window.setMinSize`.
 * Set a window's minimum size; fractions are dropped. The host keeps it in DIPs of the target window, so it reads back within 1 px. A window smaller than the new minimum grows at once, unless it is maximized, fullscreen or minimized (a minimized window grows when restored). Fails in panel mode.
 */
export interface WindowSetMinSizeParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /**
     * Minimum width in physical pixels; `0` or less lowers it to 1 DIP.
     * @default 0
     */
    width?: number;
    /**
     * Minimum height in physical pixels; `0` or less lowers it to 1 DIP.
     * @default 0
     */
    height?: number;
}

/**
 * Parameters for `window.setNoDragRegions`.
 * Set the rectangles that never drag the calling window, such as buttons inside a drag area, replacing the earlier ones. Rectangles are CSS pixels of the page, scaled by the window's DPI; rectangles without a positive width and height are skipped. Fails in panel mode, and with `NOT_FOUND` when the caller is neither the main window nor a popup.
 */
export interface WindowSetNoDragRegionsParams {
    /** The no-drag rectangles; omitted, none. */
    regions?: WindowRegion[];
}

/**
 * Parameters for `window.setPopupBehavior`.
 * Change a popup's behavior preset or its per-field overrides at runtime, then announce the result with `window:behaviorChanged`. `profile` and `behavior` are independent: passing one leaves the other alone. Targets as `window.getPopupBehavior` does; an unknown preset fails with `INVALID_PARAMS`.
 */
export interface WindowSetPopupBehaviorParams {
    /** Id of the popup; omitted or empty, the calling popup. */
    windowId?: string;
    /** New preset: `standard`, `miniPlayer` or `desktopLyrics`, case-insensitive, with `-` or `_` allowed (`mini-player`). */
    profile?: string;
    /** Overrides merged into the current ones; a `null` value removes that override. */
    behavior?: Record<string, JsonValue>;
}

/**
 * Parameters for `window.setPosition`.
 * Move the calling window's top-left corner, keeping its size; fractions are dropped. Fails in panel mode and when there is no window.
 */
export interface WindowSetPositionParams {
    /**
     * New left edge in screen coordinates.
     * @default 0
     */
    x?: number;
    /**
     * New top edge in screen coordinates.
     * @default 0
     */
    y?: number;
}

/**
 * Parameters for `window.setResizable`.
 * Set whether the user can resize a window by its frame. Setting the value the window already has succeeds. Fails in panel mode, and with `OPERATION_FAILED` when Windows refuses the new style; that failure carries `windowId`.
 */
export interface WindowSetResizableParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
    /**
     * Whether the user can resize the window by its frame.
     * @default true
     */
    resizable?: boolean;
}

/**
 * Parameters for `window.setSize`.
 * Resize the calling window, keeping its position; fractions are dropped. The size stays within the window's minimum and maximum, and the response does not say whether it was held back. Fails in panel mode and when there is no window.
 */
export interface WindowSetSizeParams {
    /**
     * New width in physical pixels, frame included.
     * @default 800
     */
    width?: number;
    /**
     * New height in physical pixels, frame included.
     * @default 600
     */
    height?: number;
}

/**
 * Parameters for `window.setTitle`.
 * Set the calling window's title, shown in the title bar and on the taskbar. Fails in panel mode and when there is no window.
 */
export interface WindowSetTitleParams {
    /**
     * The new title.
     * @default "foobar2000"
     */
    title?: string;
}

/**
 * Parameters for `window.setTitlebarHeight`.
 * Set the title bar height of the calling window, the main window or a popup; a popup's height leaves the main window alone. The height must be 24 to 100, otherwise the call fails with `INVALID_PARAMS`. Fails in panel mode, and with `NOT_FOUND` when the caller is neither the main window nor a popup.
 */
export interface WindowSetTitlebarHeightParams {
    /**
     * Title bar height in physical pixels, 24 to 100; fractions are dropped.
     * @default 32
     */
    height?: number;
}

/**
 * Parameters for `window.setZoom`.
 * Set the zoom factor of the calling window's WebView. From then on, for the rest of the session, the default zoom in the preferences no longer applies to this WebView. Fails with `NOT_FOUND` when the calling window has no WebView of its own, and with `OPERATION_FAILED` when WebView2 refuses the factor; that failure carries the `zoom` in effect.
 */
export interface WindowSetZoomParams {
    /**
     * Zoom factor; `1` is 100 %.
     * @default 1
     */
    zoom?: number;
}

/**
 * Parameters for `window.setZoomForDpi`.
 * Set the zoom factor of the calling window's WebView to `dpi / 96`; as with `window.setZoom`, the default zoom in the preferences no longer applies to it afterwards. Fails with `NOT_FOUND` when the calling window has no WebView of its own, and with `OPERATION_FAILED` when WebView2 refuses the factor; that failure carries the result fields.
 */
export interface WindowSetZoomForDpiParams {
    /**
     * DPI to match; `0` or less uses the window's DPI, or `96` when it is unknown.
     * @default 0
     */
    dpi?: number;
}

/**
 * Parameters for `window.showSystemMenu`.
 * Open the calling window's system menu (restore, move, size, minimize, maximize, close) and run the command picked from it. Coordinates are screen pixels; fractions are dropped. With a positive `w` and `h` the menu opens below the rectangle `x`, `y`, `w`, `h` and keeps clear of it; otherwise it opens at `x`, `y`. Fails in panel mode, when there is no window, and with `OPERATION_FAILED` when the window has no system menu.
 */
export interface WindowShowSystemMenuParams {
    /**
     * Left edge of the rectangle to keep clear, or the menu position when `w` or `h` is not positive.
     * @default 0
     */
    x?: number;
    /**
     * Top edge of the rectangle to keep clear, or the menu position when `w` or `h` is not positive.
     * @default 0
     */
    y?: number;
    /**
     * Width of the rectangle to keep clear.
     * @default 0
     */
    w?: number;
    /**
     * Height of the rectangle to keep clear.
     * @default 0
     */
    h?: number;
}

/**
 * Parameters for `window.startDrag`.
 * Start moving the calling window with the mouse, as pressing its title bar does. Call it from a `mousedown` handler while the button is still down. Fails in panel mode and when there is no window.
 */
export type WindowStartDragParams = Record<string, never>;

/**
 * Parameters for `window.startResize`.
 * Start resizing the calling window from one edge or corner with the mouse. Call it from a `mousedown` handler while the button is still down. Fails in panel mode and when there is no window.
 */
export interface WindowStartResizeParams {
    /**
     * Edge or corner to drag: `left`, `right`, `top`, `bottom`, `topleft`, `topright`, `bottomleft` or `bottomright`. Any other value drags the bottom-right corner.
     * @default "bottomright"
     */
    edge?: string;
}

/**
 * Parameters for `window.toggleAlwaysOnTop`.
 * Flip whether the calling window is kept above other windows. Fails in panel mode, and with `enabled: false` when there is no window.
 */
export type WindowToggleAlwaysOnTopParams = Record<string, never>;

/**
 * Parameters for `window.toggleFullscreen`.
 * Enter fullscreen, or leave it when the window is fullscreen. Fails in panel mode.
 */
export interface WindowToggleFullscreenParams {
    /** Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. */
    windowId?: string;
}

/**
 * Parameters for `window.toggleMaximize`.
 * Maximize the calling window, or restore it when it is maximized. A fullscreen window leaves fullscreen instead. Fails in panel mode, and with `maximized: false` when there is no window.
 */
export type WindowToggleMaximizeParams = Record<string, never>;
// ── API-name → Params map ────────────────────────────────────────────────
/**
 * Map from each declared method name to its `*Params` type.
 */
export interface ApiParamsMap {
    "artwork.getAvailableArtwork": ArtworkGetAvailableArtworkParams;
    "artwork.getAvailableTypes": ArtworkGetAvailableTypesParams;
    "artwork.getBatch": ArtworkGetBatchParams;
    "artwork.getByPath": ArtworkGetByPathParams;
    "artwork.getByPlaylistItem": ArtworkGetByPlaylistItemParams;
    "artwork.getCurrent": ArtworkGetCurrentParams;
    "artwork.getFb2kUrl": ArtworkGetFb2kUrlParams;
    "artwork.getFb2kUrlByPath": ArtworkGetFb2kUrlByPathParams;
    "artwork.getFb2kUrlByPathBatch": ArtworkGetFb2kUrlByPathBatchParams;
    "artwork.getFolderImages": ArtworkGetFolderImagesParams;
    "artwork.getForTrack": ArtworkGetForTrackParams;
    "artwork.getLyrics": ArtworkGetLyricsParams;
    "artwork.getMetadata": ArtworkGetMetadataParams;
    "audio.analyzeBPM": AudioAnalyzeBPMParams;
    /** @experimental */
    "audio.cancelDecodePcm": AudioCancelDecodePcmParams;
    "audio.cancelFullWaveform": AudioCancelFullWaveformParams;
    /** @experimental */
    "audio.decodePcm": AudioDecodePcmParams;
    "audio.generateFullWaveform": AudioGenerateFullWaveformParams;
    "audio.getOutputInfo": AudioGetOutputInfoParams;
    /** @experimental */
    "audio.getPcmDebugState": AudioGetPcmDebugStateParams;
    "audio.getSpectrum": AudioGetSpectrumParams;
    "audio.getSpectrumDebugState": AudioGetSpectrumDebugStateParams;
    "audio.getStreamInfo": AudioGetStreamInfoParams;
    "audio.getWaveform": AudioGetWaveformParams;
    "audio.isVisualizationAvailable": AudioIsVisualizationAvailableParams;
    "audio.setChannelMode": AudioSetChannelModeParams;
    "audio.subscribeSpectrum": AudioSubscribeSpectrumParams;
    /** @experimental */
    "audio.subscribeStream": AudioSubscribeStreamParams;
    "audio.unsubscribeSpectrum": AudioUnsubscribeSpectrumParams;
    /** @experimental */
    "audio.unsubscribeStream": AudioUnsubscribeStreamParams;
    "clipboard.read": ClipboardReadParams;
    "clipboard.write": ClipboardWriteParams;
    "clipboard.writeFiles": ClipboardWriteFilesParams;
    "clipboard.writeHTML": ClipboardWriteHTMLParams;
    "config.export": ConfigExportParams;
    "config.get": ConfigGetParams;
    "config.getActiveDspPreset": ConfigGetActiveDspPresetParams;
    "config.getAdvancedConfig": ConfigGetAdvancedConfigParams;
    "config.getAdvancedConfigValue": ConfigGetAdvancedConfigValueParams;
    "config.getAll": ConfigGetAllParams;
    "config.getComponents": ConfigGetComponentsParams;
    "config.getCursorFollowPlayback": ConfigGetCursorFollowPlaybackParams;
    "config.getDspPresets": ConfigGetDspPresetsParams;
    "config.getLibraryFilePatterns": ConfigGetLibraryFilePatternsParams;
    "config.getLibraryStatus": ConfigGetLibraryStatusParams;
    "config.getOutputConfig": ConfigGetOutputConfigParams;
    "config.getOutputDevices": ConfigGetOutputDevicesParams;
    "config.getPlaybackFollowCursor": ConfigGetPlaybackFollowCursorParams;
    "config.getPreferencesPages": ConfigGetPreferencesPagesParams;
    "config.getPreferencesStandardGuids": ConfigGetPreferencesStandardGuidsParams;
    "config.getReplaygainMode": ConfigGetReplaygainModeParams;
    "config.getVersionInfo": ConfigGetVersionInfoParams;
    "config.remove": ConfigRemoveParams;
    "config.resetAdvancedConfig": ConfigResetAdvancedConfigParams;
    "config.set": ConfigSetParams;
    "config.setActiveDspPreset": ConfigSetActiveDspPresetParams;
    "config.setAdvancedConfigValue": ConfigSetAdvancedConfigValueParams;
    "config.setCursorFollowPlayback": ConfigSetCursorFollowPlaybackParams;
    "config.setOutputBuffer": ConfigSetOutputBufferParams;
    "config.setOutputDevice": ConfigSetOutputDeviceParams;
    "config.setPlaybackFollowCursor": ConfigSetPlaybackFollowCursorParams;
    "config.setReplaygainMode": ConfigSetReplaygainModeParams;
    "config.showLibraryPreferences": ConfigShowLibraryPreferencesParams;
    "console.error": ConsoleErrorParams;
    "console.log": ConsoleLogParams;
    "console.warn": ConsoleWarnParams;
    "cursor.isHidden": CursorIsHiddenParams;
    "cursor.setHidden": CursorSetHiddenParams;
    "dialog.confirm": DialogConfirmParams;
    "dialog.openFile": DialogOpenFileParams;
    "dialog.openFolder": DialogOpenFolderParams;
    "dialog.saveFile": DialogSaveFileParams;
    "discovery.executeContextMenuByPath": DiscoveryExecuteContextMenuByPathParams;
    "discovery.executeContextMenuCommand": DiscoveryExecuteContextMenuCommandParams;
    "discovery.executeMainMenuCommand": DiscoveryExecuteMainMenuCommandParams;
    "discovery.getAllServices": DiscoveryGetAllServicesParams;
    "discovery.getComponents": DiscoveryGetComponentsParams;
    "discovery.getContextMenuCommands": DiscoveryGetContextMenuCommandsParams;
    "discovery.getContextMenuTree": DiscoveryGetContextMenuTreeParams;
    "discovery.getDspEntries": DiscoveryGetDspEntriesParams;
    "discovery.getInputFormats": DiscoveryGetInputFormatsParams;
    "discovery.getMainMenuCommands": DiscoveryGetMainMenuCommandsParams;
    "discovery.getMainMenuGroups": DiscoveryGetMainMenuGroupsParams;
    "discovery.getOutputDevices": DiscoveryGetOutputDevicesParams;
    "discovery.getPreferencePages": DiscoveryGetPreferencePagesParams;
    "discovery.getUIElements": DiscoveryGetUIElementsParams;
    "discovery.searchCommands": DiscoverySearchCommandsParams;
    "dnd.getCapabilities": DndGetCapabilitiesParams;
    "dnd.getPathsAsync": DndGetPathsAsyncParams;
    "dnd.prepareDrag": DndPrepareDragParams;
    "dnd.startDrag": DndStartDragParams;
    "dsp.addDsp": DspAddDspParams;
    "dsp.applyPreset": DspApplyPresetParams;
    "dsp.getAvailable": DspGetAvailableParams;
    "dsp.getChain": DspGetChainParams;
    "dsp.getPresets": DspGetPresetsParams;
    "dsp.moveDsp": DspMoveDspParams;
    "dsp.removeDsp": DspRemoveDspParams;
    "dsp.setChain": DspSetChainParams;
    "event.emit": EventEmitParams;
    "event.emitTo": EventEmitToParams;
    "file.cancelOp": FileCancelOpParams;
    "file.copy": FileCopyParams;
    "file.copyAsync": FileCopyAsyncParams;
    "file.delete": FileDeleteParams;
    "file.deleteAsync": FileDeleteAsyncParams;
    "file.exists": FileExistsParams;
    "file.getInfo": FileGetInfoParams;
    "file.list": FileListParams;
    "file.mkdir": FileMkdirParams;
    "file.move": FileMoveParams;
    "file.moveAsync": FileMoveAsyncParams;
    "file.read": FileReadParams;
    "file.rename": FileRenameParams;
    "file.write": FileWriteParams;
    "http.abort": HttpAbortParams;
    "http.delete": HttpDeleteParams;
    "http.download": HttpDownloadParams;
    "http.get": HttpGetParams;
    "http.head": HttpHeadParams;
    "http.patch": HttpPatchParams;
    "http.post": HttpPostParams;
    "http.put": HttpPutParams;
    "jitQueue.clear": JitQueueClearParams;
    "jitQueue.enqueueNext": JitQueueEnqueueNextParams;
    "jitQueue.getState": JitQueueGetStateParams;
    "jitQueue.notifyEmpty": JitQueueNotifyEmptyParams;
    "jitQueue.playNow": JitQueuePlayNowParams;
    "jitQueue.preloadBatch": JitQueuePreloadBatchParams;
    "jitQueue.skip": JitQueueSkipParams;
    "jitQueue.stop": JitQueueStopParams;
    "keyboard.getRegisteredHotkeys": KeyboardGetRegisteredHotkeysParams;
    "keyboard.registerHotkey": KeyboardRegisterHotkeyParams;
    "keyboard.registerShortcut": KeyboardRegisterShortcutParams;
    "keyboard.unregisterHotkey": KeyboardUnregisterHotkeyParams;
    "library.addToPlaylist": LibraryAddToPlaylistParams;
    "library.browseDirectory": LibraryBrowseDirectoryParams;
    "library.browseTree": LibraryBrowseTreeParams;
    "library.getAlbums": LibraryGetAlbumsParams;
    "library.getAlbumTracks": LibraryGetAlbumTracksParams;
    "library.getAll": LibraryGetAllParams;
    "library.getArtistAlbums": LibraryGetArtistAlbumsParams;
    "library.getArtists": LibraryGetArtistsParams;
    "library.getArtistTracks": LibraryGetArtistTracksParams;
    "library.getByPath": LibraryGetByPathParams;
    "library.getCacheStats": LibraryGetCacheStatsParams;
    "library.getCount": LibraryGetCountParams;
    "library.getFieldValues": LibraryGetFieldValuesParams;
    "library.getGenres": LibraryGetGenresParams;
    "library.getRandomTracks": LibraryGetRandomTracksParams;
    "library.getRecentlyAdded": LibraryGetRecentlyAddedParams;
    "library.getRoots": LibraryGetRootsParams;
    "library.getStats": LibraryGetStatsParams;
    "library.getStatus": LibraryGetStatusParams;
    "library.invalidateCache": LibraryInvalidateCacheParams;
    "library.isEnabled": LibraryIsEnabledParams;
    "library.query": LibraryQueryParams;
    "library.refresh": LibraryRefreshParams;
    "library.rescan": LibraryRescanParams;
    "library.search": LibrarySearchParams;
    "log.clear": LogClearParams;
    "log.read": LogReadParams;
    "log.write": LogWriteParams;
    "lyrics.exists": LyricsExistsParams;
    "lyrics.get": LyricsGetParams;
    "lyrics.save": LyricsSaveParams;
    /** @experimental */
    "media.getContainerInfo": MediaGetContainerInfoParams;
    /** @experimental */
    "media.getStreamUrl": MediaGetStreamUrlParams;
    "menu.close": MenuCloseParams;
    "menu.getContextMenu": MenuGetContextMenuParams;
    "menu.getMainMenu": MenuGetMainMenuParams;
    "menu.runContextCommand": MenuRunContextCommandParams;
    "menu.runContextCommandById": MenuRunContextCommandByIdParams;
    "menu.runMainMenuCommand": MenuRunMainMenuCommandParams;
    "menu.show": MenuShowParams;
    "menu.showNativePopup": MenuShowNativePopupParams;
    "metadata.cancelProbe": MetadataCancelProbeParams;
    "metadata.embedArtwork": MetadataEmbedArtworkParams;
    "metadata.probeBatchAsync": MetadataProbeBatchAsyncParams;
    "metadata.read": MetadataReadParams;
    "metadata.readBatch": MetadataReadBatchParams;
    "metadata.readByPath": MetadataReadByPathParams;
    "metadata.readRaw": MetadataReadRawParams;
    "metadata.removeEmbeddedArt": MetadataRemoveEmbeddedArtParams;
    "metadata.removeField": MetadataRemoveFieldParams;
    "metadata.removeTag": MetadataRemoveTagParams;
    "metadata.write": MetadataWriteParams;
    "metadata.writeBatch": MetadataWriteBatchParams;
    "misc.exit": MiscExitParams;
    "misc.getComponentPath": MiscGetComponentPathParams;
    "misc.getFoobarPath": MiscGetFoobarPathParams;
    "misc.getProfilePath": MiscGetProfilePathParams;
    "misc.restart": MiscRestartParams;
    "misc.showConsole": MiscShowConsoleParams;
    "misc.showLibrarySearch": MiscShowLibrarySearchParams;
    "misc.showPopupMessage": MiscShowPopupMessageParams;
    "misc.showPreferences": MiscShowPreferencesParams;
    "output.getDevices": OutputGetDevicesParams;
    "output.getEntries": OutputGetEntriesParams;
    "output.getSettings": OutputGetSettingsParams;
    "panel.getConfig": PanelGetConfigParams;
    "panel.setConfig": PanelSetConfigParams;
    "playback.getCurrentTrack": PlaybackGetCurrentTrackParams;
    "playback.getCurrentTrackIndex": PlaybackGetCurrentTrackIndexParams;
    "playback.getPlaybackOrder": PlaybackGetPlaybackOrderParams;
    "playback.getPlayingPlaylist": PlaybackGetPlayingPlaylistParams;
    "playback.getPosition": PlaybackGetPositionParams;
    "playback.getState": PlaybackGetStateParams;
    "playback.getStopAfterCurrent": PlaybackGetStopAfterCurrentParams;
    "playback.getVolume": PlaybackGetVolumeParams;
    "playback.mute": PlaybackMuteParams;
    "playback.next": PlaybackNextParams;
    "playback.pause": PlaybackPauseParams;
    "playback.play": PlaybackPlayParams;
    "playback.playOrPause": PlaybackPlayOrPauseParams;
    "playback.playPath": PlaybackPlayPathParams;
    "playback.playPaths": PlaybackPlayPathsParams;
    "playback.playPause": PlaybackPlayPauseParams;
    "playback.previous": PlaybackPreviousParams;
    "playback.random": PlaybackRandomParams;
    "playback.setPlaybackOrder": PlaybackSetPlaybackOrderParams;
    "playback.setPosition": PlaybackSetPositionParams;
    "playback.setStopAfterCurrent": PlaybackSetStopAfterCurrentParams;
    "playback.setVolume": PlaybackSetVolumeParams;
    "playback.stop": PlaybackStopParams;
    "playback.toggleMute": PlaybackToggleMuteParams;
    "playback.toggleStopAfterCurrent": PlaybackToggleStopAfterCurrentParams;
    "playback.volumeDown": PlaybackVolumeDownParams;
    "playback.volumeUp": PlaybackVolumeUpParams;
    "playcount.get": PlaycountGetParams;
    "playcount.getBatch": PlaycountGetBatchParams;
    "playcount.getStats": PlaycountGetStatsParams;
    "playcount.set": PlaycountSetParams;
    "playlist.addHandles": PlaylistAddHandlesParams;
    "playlist.addPaths": PlaylistAddPathsParams;
    "playlist.addPathsAsync": PlaylistAddPathsAsyncParams;
    "playlist.addPathsSequential": PlaylistAddPathsSequentialParams;
    "playlist.clear": PlaylistClearParams;
    "playlist.convertToAutoplaylist": PlaylistConvertToAutoplaylistParams;
    "playlist.create": PlaylistCreateParams;
    "playlist.createAutoplaylist": PlaylistCreateAutoplaylistParams;
    "playlist.deselectAll": PlaylistDeselectAllParams;
    "playlist.duplicate": PlaylistDuplicateParams;
    "playlist.focusTrack": PlaylistFocusTrackParams;
    "playlist.getActive": PlaylistGetActiveParams;
    "playlist.getAll": PlaylistGetAllParams;
    "playlist.getAutoplaylistInfo": PlaylistGetAutoplaylistInfoParams;
    "playlist.getAutoplaylistQuery": PlaylistGetAutoplaylistQueryParams;
    "playlist.getAvailableColumns": PlaylistGetAvailableColumnsParams;
    "playlist.getCount": PlaylistGetCountParams;
    "playlist.getFocusedTrack": PlaylistGetFocusedTrackParams;
    "playlist.getFocusTrack": PlaylistGetFocusTrackParams;
    "playlist.getGroupRuns": PlaylistGetGroupRunsParams;
    "playlist.getLockInfo": PlaylistGetLockInfoParams;
    "playlist.getMatchingRows": PlaylistGetMatchingRowsParams;
    "playlist.getPlaying": PlaylistGetPlayingParams;
    "playlist.getSelectedTracks": PlaylistGetSelectedTracksParams;
    "playlist.getSelection": PlaylistGetSelectionParams;
    "playlist.getTrackCount": PlaylistGetTrackCountParams;
    "playlist.getTracks": PlaylistGetTracksParams;
    "playlist.getTracksAt": PlaylistGetTracksAtParams;
    "playlist.insertTracks": PlaylistInsertTracksParams;
    "playlist.isAutoplaylist": PlaylistIsAutoplaylistParams;
    "playlist.isLocked": PlaylistIsLockedParams;
    "playlist.moveTracks": PlaylistMoveTracksParams;
    "playlist.playTrack": PlaylistPlayTrackParams;
    "playlist.redo": PlaylistRedoParams;
    "playlist.remove": PlaylistRemoveParams;
    "playlist.removeAutoplaylist": PlaylistRemoveAutoplaylistParams;
    "playlist.removeSelectedTracks": PlaylistRemoveSelectedTracksParams;
    "playlist.removeTracks": PlaylistRemoveTracksParams;
    "playlist.rename": PlaylistRenameParams;
    "playlist.reorder": PlaylistReorderParams;
    "playlist.reorderPlaylists": PlaylistReorderPlaylistsParams;
    "playlist.replaceAllAndPlay": PlaylistReplaceAllAndPlayParams;
    "playlist.reverse": PlaylistReverseParams;
    "playlist.selectAll": PlaylistSelectAllParams;
    "playlist.setActive": PlaylistSetActiveParams;
    "playlist.setFocusedTrack": PlaylistSetFocusedTrackParams;
    "playlist.setSelection": PlaylistSetSelectionParams;
    "playlist.shuffle": PlaylistShuffleParams;
    "playlist.sort": PlaylistSortParams;
    "playlist.undo": PlaylistUndoParams;
    "port.connect": PortConnectParams;
    "port.disconnect": PortDisconnectParams;
    "port.getPorts": PortGetPortsParams;
    "port.postMessage": PortPostMessageParams;
    "port.postMessageTo": PortPostMessageToParams;
    "queue.add": QueueAddParams;
    "queue.addPaths": QueueAddPathsParams;
    "queue.clear": QueueClearParams;
    "queue.flush": QueueFlushParams;
    "queue.get": QueueGetParams;
    "queue.getCount": QueueGetCountParams;
    "queue.insertNext": QueueInsertNextParams;
    "queue.moveToTop": QueueMoveToTopParams;
    "queue.playNow": QueuePlayNowParams;
    "queue.remove": QueueRemoveParams;
    "queue.setContents": QueueSetContentsParams;
    "rating.get": RatingGetParams;
    "rating.set": RatingSetParams;
    "replaygain.clear": ReplaygainClearParams;
    "replaygain.get": ReplaygainGetParams;
    "replaygain.getMode": ReplaygainGetModeParams;
    "replaygain.getPreamp": ReplaygainGetPreampParams;
    "replaygain.getSettings": ReplaygainGetSettingsParams;
    "replaygain.scan": ReplaygainScanParams;
    "replaygain.setMode": ReplaygainSetModeParams;
    "replaygain.setPreamp": ReplaygainSetPreampParams;
    "selection.get": SelectionGetParams;
    "selection.getType": SelectionGetTypeParams;
    "selection.getViewerMode": SelectionGetViewerModeParams;
    "selection.getViewingTrack": SelectionGetViewingTrackParams;
    "selection.set": SelectionSetParams;
    "selection.setPlaylistTracking": SelectionSetPlaylistTrackingParams;
    "shell.exec": ShellExecParams;
    "shell.openExternal": ShellOpenExternalParams;
    "shell.openWith": ShellOpenWithParams;
    "shell.showInExplorer": ShellShowInExplorerParams;
    "shell.spawn": ShellSpawnParams;
    "state.delete": StateDeleteParams;
    "state.get": StateGetParams;
    "state.keys": StateKeysParams;
    "state.set": StateSetParams;
    "system.getApisByNamespace": SystemGetApisByNamespaceParams;
    "system.getApiStats": SystemGetApiStatsParams;
    "system.getDPI": SystemGetDPIParams;
    "system.getLocale": SystemGetLocaleParams;
    "system.getRegisteredPlugins": SystemGetRegisteredPluginsParams;
    "system.getTheme": SystemGetThemeParams;
    "system.isPluginRegistered": SystemIsPluginRegisteredParams;
    "system.listAvailableApis": SystemListAvailableApisParams;
    "system.searchApis": SystemSearchApisParams;
    "taskbar.flash": TaskbarFlashParams;
    "taskbar.setOverlayIcon": TaskbarSetOverlayIconParams;
    "taskbar.setProgress": TaskbarSetProgressParams;
    "taskbar.setThumbnailButtons": TaskbarSetThumbnailButtonsParams;
    "taskbar.updateButton": TaskbarUpdateButtonParams;
    "titleformat.eval": TitleformatEvalParams;
    "titleformat.evalBatch": TitleformatEvalBatchParams;
    "titleformat.evalFields": TitleformatEvalFieldsParams;
    "titleformat.evalFieldsBatch": TitleformatEvalFieldsBatchParams;
    "titleformat.getBuiltinFields": TitleformatGetBuiltinFieldsParams;
    "tray.appendMenuItems": TrayAppendMenuItemsParams;
    "tray.clearMenuItems": TrayClearMenuItemsParams;
    "tray.create": TrayCreateParams;
    "tray.destroy": TrayDestroyParams;
    "tray.getMenuItems": TrayGetMenuItemsParams;
    "tray.isVisible": TrayIsVisibleParams;
    "tray.removeMenuItems": TrayRemoveMenuItemsParams;
    "tray.setCloseToTray": TraySetCloseToTrayParams;
    "tray.setContextMenu": TraySetContextMenuParams;
    "tray.setIcon": TraySetIconParams;
    "tray.setMenuItemState": TraySetMenuItemStateParams;
    "tray.setMenuZones": TraySetMenuZonesParams;
    "tray.setMinimizeToTray": TraySetMinimizeToTrayParams;
    "tray.setTooltip": TraySetTooltipParams;
    "tray.showBalloon": TrayShowBalloonParams;
    "ui.hideNotification": UiHideNotificationParams;
    "ui.showContextMenu": UiShowContextMenuParams;
    "ui.showCustomMenu": UiShowCustomMenuParams;
    "ui.showNotification": UiShowNotificationParams;
    "ui.showToast": UiShowToastParams;
    "webview.getSource": WebviewGetSourceParams;
    "window.blur": WindowBlurParams;
    "window.broadcast": WindowBroadcastParams;
    "window.cancelClose": WindowCancelCloseParams;
    "window.center": WindowCenterParams;
    "window.clearClickThroughExcludeRegions": WindowClearClickThroughExcludeRegionsParams;
    "window.clearDragRegions": WindowClearDragRegionsParams;
    "window.clearNoDragRegions": WindowClearNoDragRegionsParams;
    "window.close": WindowCloseParams;
    "window.closeAllPopups": WindowCloseAllPopupsParams;
    "window.closePopup": WindowClosePopupParams;
    "window.confirmClose": WindowConfirmCloseParams;
    "window.createPopup": WindowCreatePopupParams;
    "window.enterFullscreen": WindowEnterFullscreenParams;
    "window.exitFullscreen": WindowExitFullscreenParams;
    "window.flash": WindowFlashParams;
    "window.flashTaskbar": WindowFlashTaskbarParams;
    "window.focus": WindowFocusParams;
    "window.getAllWindows": WindowGetAllWindowsParams;
    "window.getBackdropPolicy": WindowGetBackdropPolicyParams;
    "window.getBounds": WindowGetBoundsParams;
    "window.getCaptionButtonsWidth": WindowGetCaptionButtonsWidthParams;
    "window.getCornerPreference": WindowGetCornerPreferenceParams;
    "window.getCurrentWindowId": WindowGetCurrentWindowIdParams;
    "window.getDevServerConfig": WindowGetDevServerConfigParams;
    "window.getDpiScale": WindowGetDpiScaleParams;
    "window.getMaxSize": WindowGetMaxSizeParams;
    "window.getMinSize": WindowGetMinSizeParams;
    "window.getMode": WindowGetModeParams;
    "window.getPopupBehavior": WindowGetPopupBehaviorParams;
    "window.getState": WindowGetStateParams;
    "window.getTitle": WindowGetTitleParams;
    "window.getTitlebarHeight": WindowGetTitlebarHeightParams;
    "window.getTitlebarInfo": WindowGetTitlebarInfoParams;
    "window.getZoom": WindowGetZoomParams;
    "window.hasSavedBounds": WindowHasSavedBoundsParams;
    "window.isAlwaysOnTop": WindowIsAlwaysOnTopParams;
    "window.isClickThrough": WindowIsClickThroughParams;
    "window.isFullscreen": WindowIsFullscreenParams;
    "window.isMaximized": WindowIsMaximizedParams;
    "window.isMinimized": WindowIsMinimizedParams;
    "window.isResizable": WindowIsResizableParams;
    "window.maximize": WindowMaximizeParams;
    "window.minimize": WindowMinimizeParams;
    "window.refreshWebView": WindowRefreshWebViewParams;
    "window.reload": WindowReloadParams;
    "window.resetZoom": WindowResetZoomParams;
    "window.restore": WindowRestoreParams;
    "window.sendMessage": WindowSendMessageParams;
    "window.setAcrylic": WindowSetAcrylicParams;
    "window.setAlwaysOnTop": WindowSetAlwaysOnTopParams;
    "window.setBackdropPolicy": WindowSetBackdropPolicyParams;
    "window.setBackgroundTransparency": WindowSetBackgroundTransparencyParams;
    "window.setBlur": WindowSetBlurParams;
    "window.setBounds": WindowSetBoundsParams;
    "window.setClickThrough": WindowSetClickThroughParams;
    "window.setClickThroughExcludeRegions": WindowSetClickThroughExcludeRegionsParams;
    "window.setCornerPreference": WindowSetCornerPreferenceParams;
    "window.setDarkMode": WindowSetDarkModeParams;
    "window.setDevServerConfig": WindowSetDevServerConfigParams;
    "window.setDragRegions": WindowSetDragRegionsParams;
    "window.setFrameless": WindowSetFramelessParams;
    "window.setFullscreen": WindowSetFullscreenParams;
    "window.setMaximizeButtonRegion": WindowSetMaximizeButtonRegionParams;
    "window.setMaxSize": WindowSetMaxSizeParams;
    "window.setMica": WindowSetMicaParams;
    "window.setMicaEffect": WindowSetMicaEffectParams;
    "window.setMinSize": WindowSetMinSizeParams;
    "window.setNoDragRegions": WindowSetNoDragRegionsParams;
    "window.setPopupBehavior": WindowSetPopupBehaviorParams;
    "window.setPosition": WindowSetPositionParams;
    "window.setResizable": WindowSetResizableParams;
    "window.setSize": WindowSetSizeParams;
    "window.setTitle": WindowSetTitleParams;
    "window.setTitlebarHeight": WindowSetTitlebarHeightParams;
    "window.setZoom": WindowSetZoomParams;
    "window.setZoomForDpi": WindowSetZoomForDpiParams;
    "window.showSystemMenu": WindowShowSystemMenuParams;
    "window.startDrag": WindowStartDragParams;
    "window.startResize": WindowStartResizeParams;
    "window.toggleAlwaysOnTop": WindowToggleAlwaysOnTopParams;
    "window.toggleFullscreen": WindowToggleFullscreenParams;
    "window.toggleMaximize": WindowToggleMaximizeParams;
}
