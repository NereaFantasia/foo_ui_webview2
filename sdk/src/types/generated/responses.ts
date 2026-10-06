// ─────────────────────────────────────────────────────────────
// GENERATED FILE — DO NOT EDIT
// File: sdk/src/types/generated/responses.ts
// Source: src/api/schema/*.ts
// Emitter: scripts/gen_sdk_types.mjs
// Regenerate: npm run gen:types (in sdk/) or `node scripts/gen_sdk_types.mjs --all`
// ─────────────────────────────────────────────────────────────

/* eslint-disable */

import type { JsonValue } from '../json.js';

import type { ApiFailure } from '../responses.js';

import type { AdvancedConfigItem, AlbumInfo, ArtistInfo, ArtworkAvailableEntry, ArtworkBatchRow, ArtworkFolderImage, ArtworkUrlRow, ConfigComponentInfo, ConfigDspPreset, ConfigLibraryFilePattern, ConfigOutputDevice, ConfigPreferencesPage, ContainerAttachment, ContainerChapter, ContainerTrack, DiscoveryComponentInfo, DiscoveryContextMenuCommand, DiscoveryContextMenuTreeNode, DiscoveryDspEntryInfo, DiscoveryInputFormatType, DiscoveryMainMenuCommand, DiscoveryMainMenuGroup, DiscoveryOutputDeviceEntry, DiscoveryPreferencePageInfo, DiscoverySearchResult, DiscoveryServiceCounts, DiscoveryUIElementInfo, DspAvailableEntry, DspChainEntry, DspPresetEntry, EvalBatchRow, EvalFieldsBatchRow, KeyboardHotkey, LibraryDirectoryNodeInfo, LibraryRootInfo, LibraryTrack, LibraryTrackPartial, LibraryValueCount, MenuTreeNode, MetadataReadBatchItem, MetadataWriteBatchError, OutputDevice, OutputEntry, PanelConfig, PcmDecodeState, PcmRuntimeState, PcmStreamState, PlaycountRow, PlaylistColumnDefinition, PlaylistGroupRun, PlaylistInfo, PlaylistTrack, PlaylistTrackPartial, PortInfo, QueueItem, RecentLibraryTrack, ReplayGainTrackInfo, SpectrumDebugSubscription, SpectrumDebugTarget, SystemApiInfo, SystemPluginInfo, Track, TrackTechnicalInfo, TrayMenuItem, WindowBackdropPolicyState, WindowInfo, WindowPopupBehaviorState } from './schema-types.js';

/**
 * Successful response from `artwork.getAvailableArtwork`.
 */
export interface ArtworkGetAvailableArtworkSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the file embeds at least one picture. */
    available: boolean;
    /** The embedded pictures, in probe order. */
    artworks: ArtworkAvailableEntry[];
    /** `embedded` once when any picture is embedded, then `folder:<name>` for each cover file (`cover`, `folder`, `front`, `album` as `.jpg` or `.png`) found next to the file. */
    sources: string[];
}

/**
 * Response from `artwork.getAvailableArtwork`: `ArtworkGetAvailableArtworkSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetAvailableArtworkResponse = ArtworkGetAvailableArtworkSuccess | ApiFailure;

/**
 * Successful response from `artwork.getAvailableTypes`.
 */
export interface ArtworkGetAvailableTypesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The embedded picture types, in probe order. */
    types: string[];
}

/**
 * Response from `artwork.getAvailableTypes`: `ArtworkGetAvailableTypesSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetAvailableTypesResponse = ArtworkGetAvailableTypesSuccess | ApiFailure;

/**
 * Successful response from `artwork.getBatch`.
 */
export interface ArtworkGetBatchSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** One row per path, in the given order. */
    artworks: ArtworkBatchRow[];
}

/**
 * Response from `artwork.getBatch`: `ArtworkGetBatchSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetBatchResponse = ArtworkGetBatchSuccess | ApiFailure;

/**
 * Successful response from `artwork.getByPath`.
 */
export interface ArtworkGetByPathSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the file has the picture. */
    available: boolean;
    /** The requested type. */
    type: string;
    /** The path as given. */
    path: string;
    /** MIME type detected from the bytes. */
    mimeType?: string;
    /** Picture size in bytes. */
    size?: number;
    /** `data:<mime>;base64,...` URL of the picture. */
    dataUrl?: string;
}

/**
 * Response from `artwork.getByPath`: `ArtworkGetByPathSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetByPathResponse = ArtworkGetByPathSuccess | ApiFailure;

/**
 * Successful response from `artwork.getByPlaylistItem`.
 */
export interface ArtworkGetByPlaylistItemSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether a picture was found. */
    available: boolean;
    /** The requested type. */
    type: string;
    /** The playlist actually read. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** The row actually read. */
    index: number;
    /** MIME type detected from the bytes. */
    mimeType?: string;
    /** Picture size in bytes. */
    size?: number;
    /** `data:<mime>;base64,...` URL of the picture. */
    dataUrl?: string;
}

/**
 * Response from `artwork.getByPlaylistItem`: `ArtworkGetByPlaylistItemSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetByPlaylistItemResponse = ArtworkGetByPlaylistItemSuccess | ApiFailure;

/**
 * Successful response from `artwork.getCurrent`.
 */
export interface ArtworkGetCurrentSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether a picture was found. */
    available: boolean;
    /** The requested type. */
    type: string;
    /** Lookup that answered: the now-playing cache, the album art manager, or the extractor. */
    source?: "now_playing_manager" | "album_art_manager_v2" | "extractor";
    /** MIME type detected from the bytes. */
    mimeType?: string;
    /** Picture size in bytes. */
    size?: number;
    /** `data:<mime>;base64,...` URL of the picture. */
    dataUrl?: string;
    /** Why no picture was found. */
    reason?: "no_track" | "not_found";
    /** Path of the playing track; reported when no picture was found for it. */
    path?: string;
}

/**
 * Response from `artwork.getCurrent`: `ArtworkGetCurrentSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetCurrentResponse = ArtworkGetCurrentSuccess | ApiFailure;

/**
 * Successful response from `artwork.getFb2kUrl`.
 */
export interface ArtworkGetFb2kUrlSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether a track is playing. */
    available: boolean;
    /** The type the URL carries: `front` or `back` for their `cover_` spellings. */
    type: string;
    /** The `fb2k://artwork/?path=...` URL; not a data URL, only the component's WebView2 resource handler resolves it. */
    dataUrl?: string;
    /** Why there is no URL; only `no_track` occurs here. */
    reason?: "no_track" | "not_found";
}

/**
 * Response from `artwork.getFb2kUrl`: `ArtworkGetFb2kUrlSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetFb2kUrlResponse = ArtworkGetFb2kUrlSuccess | ApiFailure;

/**
 * Successful response from `artwork.getFb2kUrlByPath`.
 */
export interface ArtworkGetFb2kUrlByPathSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Always `true`: the URL is built without opening the file. */
    available: boolean;
    /** The type the URL carries: `front` or `back` for their `cover_` spellings. */
    type: string;
    /** The path as given. */
    path: string;
    /** The `fb2k://artwork/?path=...` URL; not a data URL, only the component's WebView2 resource handler resolves it. */
    dataUrl: string;
}

/**
 * Response from `artwork.getFb2kUrlByPath`: `ArtworkGetFb2kUrlByPathSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetFb2kUrlByPathResponse = ArtworkGetFb2kUrlByPathSuccess | ApiFailure;

/**
 * Successful response from `artwork.getFb2kUrlByPathBatch`.
 */
export interface ArtworkGetFb2kUrlByPathBatchSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** One row per entry, in the given order. */
    artworks: ArtworkUrlRow[];
}

/**
 * Response from `artwork.getFb2kUrlByPathBatch`: `ArtworkGetFb2kUrlByPathBatchSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetFb2kUrlByPathBatchResponse = ArtworkGetFb2kUrlByPathBatchSuccess | ApiFailure;

/**
 * Successful response from `artwork.getFolderImages`.
 */
export interface ArtworkGetFolderImagesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The image files, in directory order. */
    images: ArtworkFolderImage[];
}

/**
 * Response from `artwork.getFolderImages`: `ArtworkGetFolderImagesSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetFolderImagesResponse = ArtworkGetFolderImagesSuccess | ApiFailure;

/**
 * Successful response from `artwork.getForTrack`.
 */
export interface ArtworkGetForTrackSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether a picture was found. */
    available: boolean;
    /** The requested type. */
    type: string;
    /** The path as given. */
    path: string;
    /** MIME type detected from the bytes. */
    mimeType?: string;
    /** Width in pixels from the PNG header; `0` for other formats. */
    width?: number;
    /** Height in pixels from the PNG header; `0` for other formats. */
    height?: number;
    /** Picture size in bytes. */
    size?: number;
    /** `data:<mime>;base64,...` URL of the picture. */
    dataUrl?: string;
}

/**
 * Response from `artwork.getForTrack`: `ArtworkGetForTrackSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetForTrackResponse = ArtworkGetForTrackSuccess | ApiFailure;

/**
 * Successful response from `artwork.getLyrics`.
 */
export interface ArtworkGetLyricsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether a non-empty lyrics tag was found. */
    available: boolean;
    /** The tag that supplied the lyrics. */
    tag?: string;
    /** The lyrics text. */
    lyrics?: string;
    /** `true` when the tag name marks the lyrics as synced. */
    synced?: boolean;
}

/**
 * Response from `artwork.getLyrics`: `ArtworkGetLyricsSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetLyricsResponse = ArtworkGetLyricsSuccess | ApiFailure;

/**
 * Successful response from `artwork.getMetadata`.
 */
export interface ArtworkGetMetadataSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Always `true`: the track was resolved, whether or not any of its tags could be read. */
    available: boolean;
    /** `ALBUM` tag; empty when absent. */
    album: string;
    /** `ARTIST` values joined with `, ` in tag order. */
    artist: string;
    /** `ALBUM ARTIST` values joined with `, ` in tag order. */
    albumArtist: string;
    /** `TITLE` tag; empty when absent. */
    title: string;
    /** `DATE` tag as written; empty when absent. */
    year: string;
    /** `GENRE` values joined with `, ` in tag order. */
    genre: string;
    /** `TRACKNUMBER` tag as written, such as `3` or `3/12`; empty when absent. */
    trackNumber: string;
    /** `DISCNUMBER` tag as written; empty when absent. */
    discNumber: string;
    /** Whether the file embeds any of the five picture types. */
    hasEmbedded: boolean;
    /** Whether any of the five tags recognized by `artwork.getLyrics` exists, even an empty one. */
    hasLyrics: boolean;
}

/**
 * Response from `artwork.getMetadata`: `ArtworkGetMetadataSuccess`, or `ApiFailure` when the call failed.
 */
export type ArtworkGetMetadataResponse = ArtworkGetMetadataSuccess | ApiFailure;

/**
 * Successful response from `audio.analyzeBPM`.
 */
export interface AudioAnalyzeBPMSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Value of the `BPM` tag, between 0 and 500 (exclusive); a tag outside that range fails with `NOT_FOUND`. */
    bpm: number;
    /** Always `1`. */
    confidence: number;
    /** Always `metadata`. */
    source: "metadata";
}

/**
 * Response from `audio.analyzeBPM`: `AudioAnalyzeBPMSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioAnalyzeBPMResponse = AudioAnalyzeBPMSuccess | ApiFailure;

/**
 * Successful response from `audio.cancelDecodePcm`.
 * @experimental
 */
export interface AudioCancelDecodePcmSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `false` when the task has already ended, does not exist or belongs to another page; these cases are not told apart. A cancelled task receives one `audio:pcmFailed` with `code: 'CANCELLED'`. */
    cancelled: boolean;
}

/**
 * Response from `audio.cancelDecodePcm`: `AudioCancelDecodePcmSuccess`, or `ApiFailure` when the call failed.
 * @experimental
 */
export type AudioCancelDecodePcmResponse = AudioCancelDecodePcmSuccess | ApiFailure;

/**
 * Successful response from `audio.cancelFullWaveform`.
 */
export interface AudioCancelFullWaveformSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `false` when the task has already ended, does not exist or belongs to another page; these cases are not told apart. A cancelled task receives one `audio:fullWaveformFailed` with `code: 'CANCELLED'`. */
    cancelled: boolean;
}

/**
 * Response from `audio.cancelFullWaveform`: `AudioCancelFullWaveformSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioCancelFullWaveformResponse = AudioCancelFullWaveformSuccess | ApiFailure;

/**
 * Successful response from `audio.decodePcm`.
 * @experimental
 */
export interface AudioDecodePcmSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Identifies the task in events, shared-buffer metadata and cancelDecodePcm; `pcm_N`. */
    taskId: string;
    /** Always `pending`: decoded PCM is never cached. */
    status: "pending";
}

/**
 * Response from `audio.decodePcm`: `AudioDecodePcmSuccess`, or `ApiFailure` when the call failed.
 * @experimental
 */
export type AudioDecodePcmResponse = AudioDecodePcmSuccess | ApiFailure;

/**
 * Successful response from `audio.generateFullWaveform`.
 */
export interface AudioGenerateFullWaveformSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `ready`: the waveform is in this answer. `pending`: it arrives as an event for `taskId`. */
    status: "ready" | "pending";
    /** The answer came from the cache. */
    cached: boolean;
    /** Identifies the task in events and cancelFullWaveform; `pending` answers only. */
    taskId?: string;
    /** `ready` answers: the points, normalized by `maxAmplitude`. */
    waveform?: number[];
    /** `ready` answers: the largest value of the selected sequence before normalization, in linear full-scale units. On the `linear` scale `waveform[i] * maxAmplitude` restores the level; on `db`, dBFS is `(v * 60 - 60) + 20 * log10(maxAmplitude)`. */
    maxAmplitude?: number;
    /** `ready` answers: track length in seconds. */
    duration?: number;
    /** `ready` answers: sample rate in Hz. */
    sampleRate?: number;
    /** `ready` answers: channel count. */
    channels?: number;
    /** Point count in use. */
    resolution: number;
    /** As requested. */
    method: "rms" | "peak";
    /** As requested. */
    scale: "linear" | "db";
    /** As requested. */
    signed: boolean;
    /** The requested path. */
    path: string;
}

/**
 * Response from `audio.generateFullWaveform`: `AudioGenerateFullWaveformSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioGenerateFullWaveformResponse = AudioGenerateFullWaveformSuccess | ApiFailure;

/**
 * Successful response from `audio.getOutputInfo`.
 */
export interface AudioGetOutputInfoSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Volume in dB, `-100` (silence) to `0` (full). */
    volume: number;
    /** The same volume as a linear percentage, `100 * 10^(volume / 20)`. */
    volumePercent: number;
}

/**
 * Response from `audio.getOutputInfo`: `AudioGetOutputInfoSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioGetOutputInfoResponse = AudioGetOutputInfoSuccess | ApiFailure;

/**
 * Successful response from `audio.getPcmDebugState`.
 * @experimental
 */
export interface AudioGetPcmDebugStateSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Shared-buffer support of the calling page; both flags are `false` when the page cannot be located. */
    runtime: PcmRuntimeState;
    /** The decodePcm task queue. */
    decode: PcmDecodeState;
    /** The stream capture callback and every stream subscription of every page. */
    stream: PcmStreamState;
}

/**
 * Response from `audio.getPcmDebugState`: `AudioGetPcmDebugStateSuccess`, or `ApiFailure` when the call failed.
 * @experimental
 */
export type AudioGetPcmDebugStateResponse = AudioGetPcmDebugStateSuccess | ApiFailure;

/**
 * Successful response from `audio.getSpectrum`.
 */
export interface AudioGetSpectrumSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Subscription the frame belongs to; absent when the request named none. */
    subscriptionId?: string;
    /** What the frame carries. */
    output: "bands" | "bins";
    /** Band frames: one value per band in the frame's `scale`; `weighted` values lie in `[0, 1]`, `db` values have a floor of `-160`. Bin frames with `channels: 'mix'`: one power value in dB per bin. */
    spectrum?: number[];
    /** Bin frames with `channels: 'stereo'`: the first channel, one dB value per bin. */
    left?: number[];
    /** Bin frames with `channels: 'stereo'`: the second channel, or the first again for a mono stream. */
    right?: number[];
    /** Band frames: length of `spectrum`. */
    bands?: number;
    /** Bin frames: FFT bin index of the first value; value `i` is centred at `(firstBin + i) * sampleRate / fftSize` Hz. `0`, with empty arrays, when the range holds no bin. */
    firstBin?: number;
    /** FFT size actually used. */
    fftSize: number;
    /** Scale of the values; always `db` for bin frames. */
    scale: "weighted" | "db";
    /** Bin frames: the subscription's channel layout. */
    channels?: "mix" | "stereo";
    /** Bin frames: channel count of the visualization stream; `0` when unknown. */
    channelCount?: number;
    /** Sample rate of the visualization stream in Hz; `0` when unknown. */
    sampleRate: number;
    /** Lower edge of the frequency range in Hz. */
    minFrequency: number;
    /** Upper edge in Hz: the requested one capped at `sampleRate / 2`, or `sampleRate / 2` when none was given. */
    maxFrequency: number;
    /** Paused or stopped frames are silence. */
    state: "playing" | "paused" | "stopped";
    /** Seconds since the visualization stream last started; the frame is computed from the `fftSize` samples ending at this time. */
    streamTime: number;
    /** Host system time when the frame was computed, Unix epoch milliseconds. */
    hostTime: number;
}

/**
 * Response from `audio.getSpectrum`: `AudioGetSpectrumSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioGetSpectrumResponse = AudioGetSpectrumSuccess | ApiFailure;

/**
 * Successful response from `audio.getSpectrumDebugState`.
 */
export interface AudioGetSpectrumDebugStateSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** At least one spectrum subscription is registered. */
    active: boolean;
    /** The beat thread that pushes frames is running. */
    timerRunning: boolean;
    /** Deprecated; always `0`. */
    timerHwnd: number;
    /** Timer of the beat thread; `null` while it is not running. */
    beatSource: "high-resolution" | "standard" | null;
    /** Current beat in milliseconds, 1000 / `effectiveFps`; `null` while the beat thread is not running. */
    beatIntervalMs: number | null;
    /** Beats dropped because the previous one had not reached the main thread yet; cumulative. */
    beatsCoalesced: number;
    /** Largest FFT size requested among all subscriptions; each still computes with its own. */
    effectiveFftSize: number;
    /** Largest frame rate requested among all subscriptions. */
    effectiveFps: number;
    /** Largest band count requested among all subscriptions. */
    effectiveBands: number;
    /** Ticks the pushed frames will skip because the last one ran over its interval. */
    skipFrames: number;
    /** FFTs computed since the host started; flat while playback is paused or stopped. */
    framesComputed: number;
    /** The visualization stream exists. */
    streamReady: boolean;
    /** Length of `subscriptions`. */
    subscriptionCount: number;
    /** Length of `dispatchTargets`. */
    dispatchTargetCount: number;
    /** Every spectrum subscription of every page. */
    subscriptions: SpectrumDebugSubscription[];
    /** Windows the pushed frames are delivered to. */
    dispatchTargets: SpectrumDebugTarget[];
    /** WebView2 instances in the component. */
    instanceCount: number;
    /** Window handle of the calling page, as a number; `0` when unknown. */
    callerHwnd: number;
    /** Window id of the calling page; empty when unknown. */
    callerWindowId: string;
    /** The calling page owns at least one subscription. */
    callerOwnsSubscription: boolean;
    /** Handle of the foreground window, as a number. */
    foregroundHwnd: number;
    /** Process id of the foreground window. */
    foregroundPid: number;
    /** The foreground window belongs to another process, so the background frame limit applies. */
    foregroundIsExternal: boolean;
    /** Title of the foreground window. */
    foregroundTitle: string;
}

/**
 * Response from `audio.getSpectrumDebugState`: `AudioGetSpectrumDebugStateSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioGetSpectrumDebugStateResponse = AudioGetSpectrumDebugStateSuccess | ApiFailure;

/**
 * Successful response from `audio.getStreamInfo`.
 */
export interface AudioGetStreamInfoSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** A track is playing or paused; the other fields are present only then. */
    playing: boolean;
    /** Sample rate in Hz. */
    sampleRate?: number;
    /** Channel count. */
    channels?: number;
    /** Bitrate in kbps. */
    bitrate?: number;
    /** Codec name as foobar2000 reports it, or `unknown`. */
    codec?: string;
    /** Track length in seconds. */
    duration?: number;
}

/**
 * Response from `audio.getStreamInfo`: `AudioGetStreamInfoSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioGetStreamInfoResponse = AudioGetStreamInfoSuccess | ApiFailure;

/**
 * Successful response from `audio.getWaveform`.
 */
export interface AudioGetWaveformSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `mix` answers: one value per sample. */
    waveform?: number[];
    /** `stereo` answers: the first channel. */
    left?: number[];
    /** `stereo` answers: the second channel; a copy of `left` when the stream has one channel. */
    right?: number[];
    /** Window length in seconds, as requested. */
    duration: number;
    /** As requested. */
    signed: boolean;
    /** As requested. */
    channels: "mix" | "stereo";
    /** Sample rate of the visualization stream in Hz. */
    sampleRate: number;
    /** Channel count of the visualization stream; `1` after setChannelMode with `mono`. */
    channelCount: number;
}

/**
 * Response from `audio.getWaveform`: `AudioGetWaveformSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioGetWaveformResponse = AudioGetWaveformSuccess | ApiFailure;

/**
 * Successful response from `audio.isVisualizationAvailable`.
 */
export interface AudioIsVisualizationAvailableSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** foobar2000 offers a visualization stream. */
    available: boolean;
}

/**
 * Response from `audio.isVisualizationAvailable`: `AudioIsVisualizationAvailableSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioIsVisualizationAvailableResponse = AudioIsVisualizationAvailableSuccess | ApiFailure;

/**
 * Successful response from `audio.setChannelMode`.
 */
export interface AudioSetChannelModeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The mode now in effect. */
    mode: "default" | "mono" | "front" | "back";
}

/**
 * Response from `audio.setChannelMode`: `AudioSetChannelModeSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioSetChannelModeResponse = AudioSetChannelModeSuccess | ApiFailure;

/**
 * Successful response from `audio.subscribeSpectrum`.
 */
export interface AudioSubscribeSpectrumSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The subscription's key, as given or derived. */
    subscriptionId: string;
    /** Requested FFT size; frames report the size actually used. */
    fftSize: number;
    /** Band count after clamping. */
    bands: number;
    /** Frame rate after clamping. */
    fps: number;
    /** Scale in use; always `db` for bin output. */
    scale: "weighted" | "db";
    /** As requested. */
    backgroundThrottle: boolean;
    /** Lower edge of the range in Hz, as requested. */
    minFrequency: number;
    /** Requested upper edge in Hz; `null` when the range follows half the stream's sample rate. */
    maxFrequency: number | null;
    /** Output registered. */
    output: "bands" | "bins";
    /** Channel layout registered. */
    channels: "mix" | "stereo";
    /** Event name the frames arrive under. */
    event: string;
    /** `true` once the visualization stream exists, which includes the stopped state; whether audio flows shows in the frames' `state`. */
    streamReady: boolean;
}

/**
 * Response from `audio.subscribeSpectrum`: `AudioSubscribeSpectrumSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioSubscribeSpectrumResponse = AudioSubscribeSpectrumSuccess | ApiFailure;

/**
 * Successful response from `audio.subscribeStream`.
 * @experimental
 */
export interface AudioSubscribeStreamSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The subscription's id, as given or generated. */
    subscriptionId: string;
    /** Interval requested from the core; absent when the core's default is used. */
    interval?: number;
    /** Ring buffer length registered, in seconds. */
    bufferSeconds: number;
}

/**
 * Response from `audio.subscribeStream`: `AudioSubscribeStreamSuccess`, or `ApiFailure` when the call failed.
 * @experimental
 */
export type AudioSubscribeStreamResponse = AudioSubscribeStreamSuccess | ApiFailure;

/**
 * Successful response from `audio.unsubscribeSpectrum`.
 */
export interface AudioUnsubscribeSpectrumSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Subscriptions removed: 0 or 1 with a `subscriptionId`, any number without. Subscriptions of other pages are never counted or removed. */
    removed: number;
    /** The requested id; empty when none was given. */
    subscriptionId: string;
}

/**
 * Response from `audio.unsubscribeSpectrum`: `AudioUnsubscribeSpectrumSuccess`, or `ApiFailure` when the call failed.
 */
export type AudioUnsubscribeSpectrumResponse = AudioUnsubscribeSpectrumSuccess | ApiFailure;

/**
 * Successful response from `audio.unsubscribeStream`.
 * @experimental
 */
export interface AudioUnsubscribeStreamSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Subscriptions removed: 0 or 1 with a `subscriptionId`, any number without. Subscriptions of other pages are never counted or removed. */
    removed: number;
}

/**
 * Response from `audio.unsubscribeStream`: `AudioUnsubscribeStreamSuccess`, or `ApiFailure` when the call failed.
 * @experimental
 */
export type AudioUnsubscribeStreamResponse = AudioUnsubscribeStreamSuccess | ApiFailure;

/**
 * Successful response from `clipboard.read`.
 */
export interface ClipboardReadSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The clipboard holds text (`CF_UNICODETEXT` or `CF_TEXT`). */
    hasText: boolean;
    /** The clipboard holds a bitmap (`CF_DIB` or `CF_BITMAP`). The image itself is not returned. */
    hasImage: boolean;
    /** The clipboard holds a file list (`CF_HDROP`) with at least one file. */
    hasFiles: boolean;
    /** The text; empty when the clipboard holds none. */
    text: string;
    /** The file paths, present only when `hasFiles` is `true`. */
    files?: string[];
}

/**
 * Response from `clipboard.read`: `ClipboardReadSuccess`, or `ApiFailure` when the call failed.
 */
export type ClipboardReadResponse = ClipboardReadSuccess | ApiFailure;

/**
 * Successful response from `clipboard.write`.
 */
export interface ClipboardWriteSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `clipboard.write`: `ClipboardWriteSuccess`, or `ApiFailure` when the call failed.
 */
export type ClipboardWriteResponse = ClipboardWriteSuccess | ApiFailure;

/**
 * Successful response from `clipboard.writeFiles`.
 */
export interface ClipboardWriteFilesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of paths placed on the clipboard. */
    fileCount: number;
}

/**
 * Response from `clipboard.writeFiles`: `ClipboardWriteFilesSuccess`, or `ApiFailure` when the call failed.
 */
export type ClipboardWriteFilesResponse = ClipboardWriteFilesSuccess | ApiFailure;

/**
 * Successful response from `clipboard.writeHTML`.
 */
export interface ClipboardWriteHTMLSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The `HTML Format` data was placed on the clipboard. */
    htmlWritten: boolean;
    /** The plain-text fallback was placed on the clipboard. */
    textWritten: boolean;
}

/**
 * Response from `clipboard.writeHTML`: `ClipboardWriteHTMLSuccess`, or `ApiFailure` when the call failed.
 */
export type ClipboardWriteHTMLResponse = ClipboardWriteHTMLSuccess | ApiFailure;

/**
 * Successful response from `config.export`.
 */
export interface ConfigExportSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Every stored key with its value; the map `config.getAll` reports. */
    data: Record<string, JsonValue>;
    /** `data` serialized as one compact JSON text. */
    json: string;
    /** Number of keys in the store. */
    count: number;
}

/**
 * Response from `config.export`: `ConfigExportSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigExportResponse = ConfigExportSuccess | ApiFailure;

/**
 * Successful response from `config.get`.
 */
export interface ConfigGetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The key asked for. */
    key: string;
    /** The stored value; when the key is absent, `default` if it was given, otherwise `null`. */
    value: JsonValue;
    /** Whether the key is in the store. */
    found: boolean;
}

/**
 * Response from `config.get`: `ConfigGetSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetResponse = ConfigGetSuccess | ApiFailure;

/**
 * Successful response from `config.getActiveDspPreset`.
 */
export interface ConfigGetActiveDspPresetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Position of the selected preset; `null` when none is selected. */
    index: number | null;
    /** Name of the selected preset; `null` when none is selected. */
    name: string | null;
    /** Whether a preset is selected. */
    isActive: boolean;
}

/**
 * Response from `config.getActiveDspPreset`: `ConfigGetActiveDspPresetSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetActiveDspPresetResponse = ConfigGetActiveDspPresetSuccess | ApiFailure;

/**
 * Successful response from `config.getAdvancedConfig`.
 */
export interface ConfigGetAdvancedConfigSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Entries directly below the requested branch; each branch carries its own entries in `children`. */
    entries: AdvancedConfigItem[];
    /** Number of entries in `entries`; nested entries are not counted. */
    count: number;
}

/**
 * Response from `config.getAdvancedConfig`: `ConfigGetAdvancedConfigSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetAdvancedConfigResponse = ConfigGetAdvancedConfigSuccess | ApiFailure;

/**
 * Successful response from `config.getAdvancedConfigValue`.
 */
export interface ConfigGetAdvancedConfigValueSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Display name of the entry. */
    name: string;
    /** The GUID as it was sent; its letter case is not normalized. */
    guid: string;
    /** Kind of an Advanced preferences entry: `branch` holds other entries, `checkbox` and `radio` hold a boolean, `integer` and `string` hold text, `unknown` is any other kind. */
    type: "branch" | "checkbox" | "radio" | "integer" | "string" | "unknown";
    /** Current value: a boolean on `checkbox` and `radio`, a number on `integer` (`0` when the stored text does not start with a number), a string on `string`, `null` on `branch` and `unknown`. */
    value: JsonValue;
}

/**
 * Response from `config.getAdvancedConfigValue`: `ConfigGetAdvancedConfigValueSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetAdvancedConfigValueResponse = ConfigGetAdvancedConfigValueSuccess | ApiFailure;

/**
 * Successful response from `config.getAll`.
 */
export interface ConfigGetAllSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Every stored key with its value. */
    items: Record<string, JsonValue>;
    /** The same map as `items`, under its older name. */
    configs: Record<string, JsonValue>;
    /** Number of keys in the store. */
    count: number;
}

/**
 * Response from `config.getAll`: `ConfigGetAllSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetAllResponse = ConfigGetAllSuccess | ApiFailure;

/**
 * Successful response from `config.getComponents`.
 */
export interface ConfigGetComponentsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Every installed component, in enumeration order. */
    components: ConfigComponentInfo[];
    /** Number of entries in `components`. */
    count: number;
}

/**
 * Response from `config.getComponents`: `ConfigGetComponentsSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetComponentsResponse = ConfigGetComponentsSuccess | ApiFailure;

/**
 * Successful response from `config.getCursorFollowPlayback`.
 */
export interface ConfigGetCursorFollowPlaybackSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the setting is on. */
    enabled: boolean;
    /** The same as `enabled`. */
    value: boolean;
}

/**
 * Response from `config.getCursorFollowPlayback`: `ConfigGetCursorFollowPlaybackSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetCursorFollowPlaybackResponse = ConfigGetCursorFollowPlaybackSuccess | ApiFailure;

/**
 * Successful response from `config.getDspPresets`.
 */
export interface ConfigGetDspPresetsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Every stored preset, in list order. */
    presets: ConfigDspPreset[];
    /** Number of entries in `presets`. */
    count: number;
}

/**
 * Response from `config.getDspPresets`: `ConfigGetDspPresetsSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetDspPresetsResponse = ConfigGetDspPresetsSuccess | ApiFailure;

/**
 * Successful response from `config.getLibraryFilePatterns`.
 */
export interface ConfigGetLibraryFilePatternsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Pattern for newly encoded, copied or moved tracks; absent when not configured. */
    tracks?: ConfigLibraryFilePattern;
    /** Pattern for newly encoded, copied or moved album images; absent when not configured. */
    images?: ConfigLibraryFilePattern;
}

/**
 * Response from `config.getLibraryFilePatterns`: `ConfigGetLibraryFilePatternsSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetLibraryFilePatternsResponse = ConfigGetLibraryFilePatternsSuccess | ApiFailure;

/**
 * Successful response from `config.getLibraryStatus`.
 */
export interface ConfigGetLibraryStatusSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the media library is enabled, which foobar2000 takes to mean that at least one library folder is configured. */
    enabled: boolean;
    /** Number of items in the media library, counted on every call. */
    itemCount: number;
    /** Whether the media library has finished loading; `true` on a host that cannot tell. */
    initialized: boolean;
}

/**
 * Response from `config.getLibraryStatus`: `ConfigGetLibraryStatusSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetLibraryStatusResponse = ConfigGetLibraryStatusSuccess | ApiFailure;

/**
 * Successful response from `config.getOutputConfig`.
 */
export interface ConfigGetOutputConfigSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** GUID of the output module in effect, rendered as `{...}`. */
    outputId: string;
    /** GUID of the device in effect, rendered as `{...}`. */
    deviceId: string;
    /** Output buffer length in seconds. */
    bufferLength: number;
    /** Output bit depth setting, in bits. */
    bitDepth: number;
    /** Whether dithering is on. */
    useDither: boolean;
    /** Whether fades are on. */
    useFades: boolean;
    /** Display name of the output module; absent when no installed module has that GUID. */
    outputName?: string;
    /** Display name of the device; absent when the module does not name it. */
    deviceName?: string;
}

/**
 * Response from `config.getOutputConfig`: `ConfigGetOutputConfigSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetOutputConfigResponse = ConfigGetOutputConfigSuccess | ApiFailure;

/**
 * Successful response from `config.getOutputDevices`.
 */
export interface ConfigGetOutputDevicesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Devices of all output modules. */
    devices: ConfigOutputDevice[];
    /** Number of entries in `devices`. */
    count: number;
}

/**
 * Response from `config.getOutputDevices`: `ConfigGetOutputDevicesSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetOutputDevicesResponse = ConfigGetOutputDevicesSuccess | ApiFailure;

/**
 * Successful response from `config.getPlaybackFollowCursor`.
 */
export interface ConfigGetPlaybackFollowCursorSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the setting is on. */
    enabled: boolean;
    /** The same as `enabled`. */
    value: boolean;
}

/**
 * Response from `config.getPlaybackFollowCursor`: `ConfigGetPlaybackFollowCursorSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetPlaybackFollowCursorResponse = ConfigGetPlaybackFollowCursorSuccess | ApiFailure;

/**
 * Successful response from `config.getPreferencesPages`.
 */
export interface ConfigGetPreferencesPagesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Every page, then every branch. */
    pages: ConfigPreferencesPage[];
    /** Number of entries in `pages`. */
    count: number;
}

/**
 * Response from `config.getPreferencesPages`: `ConfigGetPreferencesPagesSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetPreferencesPagesResponse = ConfigGetPreferencesPagesSuccess | ApiFailure;

/**
 * Successful response from `config.getPreferencesStandardGuids`.
 */
export interface ConfigGetPreferencesStandardGuidsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `preferences_page::guid_root`, rendered as `{...}`: the root of the preferences tree. */
    root: string;
    /** `preferences_page::guid_hidden`, rendered as `{...}`. */
    hidden: string;
    /** `preferences_page::guid_tools`, rendered as `{...}`. */
    tools: string;
    /** `preferences_page::guid_core`, rendered as `{...}`. */
    core: string;
    /** `preferences_page::guid_display`, rendered as `{...}`. */
    display: string;
    /** `preferences_page::guid_playback`, rendered as `{...}`. */
    playback: string;
    /** `preferences_page::guid_visualisations`, rendered as `{...}`. */
    visualisations: string;
    /** `preferences_page::guid_input`, rendered as `{...}`. */
    input: string;
    /** `preferences_page::guid_tag_writing`, rendered as `{...}`. */
    tagWriting: string;
    /** `preferences_page::guid_media_library`, rendered as `{...}`. */
    mediaLibrary: string;
    /** `preferences_page::guid_tagging`, rendered as `{...}`. */
    tagging: string;
    /** `preferences_page::guid_output`, rendered as `{...}`. */
    output: string;
    /** `preferences_page::guid_advanced`, rendered as `{...}`. */
    advanced: string;
    /** `preferences_page::guid_components`, rendered as `{...}`. */
    components: string;
    /** `preferences_page::guid_dsp`, rendered as `{...}`. */
    dsp: string;
    /** `preferences_page::guid_shell`, rendered as `{...}`. */
    shell: string;
    /** `preferences_page::guid_keyboard_shortcuts`, rendered as `{...}`. */
    keyboardShortcuts: string;
}

/**
 * Response from `config.getPreferencesStandardGuids`: `ConfigGetPreferencesStandardGuidsSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetPreferencesStandardGuidsResponse = ConfigGetPreferencesStandardGuidsSuccess | ApiFailure;

/**
 * Successful response from `config.getReplaygainMode`.
 */
export interface ConfigGetReplaygainModeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Source mode: `0` none, `1` track, `2` album, `3` by playback order. */
    mode: number;
    /** The same as `mode`. */
    value: number;
}

/**
 * Response from `config.getReplaygainMode`: `ConfigGetReplaygainModeSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetReplaygainModeResponse = ConfigGetReplaygainModeSuccess | ApiFailure;

/**
 * Successful response from `config.getVersionInfo`.
 */
export interface ConfigGetVersionInfoSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** foobar2000's version string as the core reports it; it includes the product name. */
    version: string;
    /** The same as `version`. */
    foobar2000: string;
    /** What `core_version_info_v2::get_name()` reports, which the foobar2000 SDK documents as the product name `foobar2000`; despite the key, not a longer form of `version`. */
    versionFull: string;
    /** Whether this is a 64-bit build. */
    is64bit: boolean;
    /** Whether foobar2000 runs as a portable installation. */
    isPortable: boolean;
    /** This component. */
    plugin: {
        /** Always `foo_ui_webview2`. */
        name: string;
        /** This component's version, `major.minor.patch`. */
        version: string;
    };
    /** The profile folder as a display path. */
    profilePath: string;
}

/**
 * Response from `config.getVersionInfo`: `ConfigGetVersionInfoSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigGetVersionInfoResponse = ConfigGetVersionInfoSuccess | ApiFailure;

/**
 * Successful response from `config.remove`.
 */
export interface ConfigRemoveSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The key asked for. */
    key: string;
    /** Whether the key was in the store before the call. */
    existed: boolean;
}

/**
 * Response from `config.remove`: `ConfigRemoveSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigRemoveResponse = ConfigRemoveSuccess | ApiFailure;

/**
 * Successful response from `config.resetAdvancedConfig`.
 */
export interface ConfigResetAdvancedConfigSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `config.resetAdvancedConfig`: `ConfigResetAdvancedConfigSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigResetAdvancedConfigResponse = ConfigResetAdvancedConfigSuccess | ApiFailure;

/**
 * Successful response from `config.set`.
 */
export interface ConfigSetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The key written. */
    key: string;
}

/**
 * Response from `config.set`: `ConfigSetSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigSetResponse = ConfigSetSuccess | ApiFailure;

/**
 * Successful response from `config.setActiveDspPreset`.
 */
export interface ConfigSetActiveDspPresetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `config.setActiveDspPreset`: `ConfigSetActiveDspPresetSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigSetActiveDspPresetResponse = ConfigSetActiveDspPresetSuccess | ApiFailure;

/**
 * Successful response from `config.setAdvancedConfigValue`.
 */
export interface ConfigSetAdvancedConfigValueSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `config.setAdvancedConfigValue`: `ConfigSetAdvancedConfigValueSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigSetAdvancedConfigValueResponse = ConfigSetAdvancedConfigValueSuccess | ApiFailure;

/**
 * Successful response from `config.setCursorFollowPlayback`.
 */
export interface ConfigSetCursorFollowPlaybackSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The value written. */
    enabled: boolean;
}

/**
 * Response from `config.setCursorFollowPlayback`: `ConfigSetCursorFollowPlaybackSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigSetCursorFollowPlaybackResponse = ConfigSetCursorFollowPlaybackSuccess | ApiFailure;

/**
 * Successful response from `config.setOutputBuffer`.
 */
export interface ConfigSetOutputBufferSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `config.setOutputBuffer`: `ConfigSetOutputBufferSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigSetOutputBufferResponse = ConfigSetOutputBufferSuccess | ApiFailure;

/**
 * Successful response from `config.setOutputDevice`.
 */
export interface ConfigSetOutputDeviceSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `config.setOutputDevice`: `ConfigSetOutputDeviceSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigSetOutputDeviceResponse = ConfigSetOutputDeviceSuccess | ApiFailure;

/**
 * Successful response from `config.setPlaybackFollowCursor`.
 */
export interface ConfigSetPlaybackFollowCursorSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The value written. */
    enabled: boolean;
}

/**
 * Response from `config.setPlaybackFollowCursor`: `ConfigSetPlaybackFollowCursorSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigSetPlaybackFollowCursorResponse = ConfigSetPlaybackFollowCursorSuccess | ApiFailure;

/**
 * Successful response from `config.setReplaygainMode`.
 */
export interface ConfigSetReplaygainModeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The source mode now in effect, as a number. */
    mode: number;
    /** The same as `mode`. */
    value: number;
}

/**
 * Response from `config.setReplaygainMode`: `ConfigSetReplaygainModeSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigSetReplaygainModeResponse = ConfigSetReplaygainModeSuccess | ApiFailure;

/**
 * Successful response from `config.showLibraryPreferences`.
 */
export interface ConfigShowLibraryPreferencesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `config.showLibraryPreferences`: `ConfigShowLibraryPreferencesSuccess`, or `ApiFailure` when the call failed.
 */
export type ConfigShowLibraryPreferencesResponse = ConfigShowLibraryPreferencesSuccess | ApiFailure;

/**
 * Successful response from `console.error`.
 */
export interface ConsoleErrorSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `console.error`: `ConsoleErrorSuccess`, or `ApiFailure` when the call failed.
 */
export type ConsoleErrorResponse = ConsoleErrorSuccess | ApiFailure;

/**
 * Successful response from `console.log`.
 */
export interface ConsoleLogSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `console.log`: `ConsoleLogSuccess`, or `ApiFailure` when the call failed.
 */
export type ConsoleLogResponse = ConsoleLogSuccess | ApiFailure;

/**
 * Successful response from `console.warn`.
 */
export interface ConsoleWarnSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `console.warn`: `ConsoleWarnSuccess`, or `ApiFailure` when the call failed.
 */
export type ConsoleWarnResponse = ConsoleWarnSuccess | ApiFailure;

/**
 * Successful response from `cursor.isHidden`.
 */
export interface CursorIsHiddenSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the cursor is hidden. */
    hidden: boolean;
}

/**
 * Response from `cursor.isHidden`: `CursorIsHiddenSuccess`, or `ApiFailure` when the call failed.
 */
export type CursorIsHiddenResponse = CursorIsHiddenSuccess | ApiFailure;

/**
 * Successful response from `cursor.setHidden`.
 */
export interface CursorSetHiddenSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the state changed; `false` when it was already as requested. */
    changed: boolean;
}

/**
 * Response from `cursor.setHidden`: `CursorSetHiddenSuccess`, or `ApiFailure` when the call failed.
 */
export type CursorSetHiddenResponse = CursorSetHiddenSuccess | ApiFailure;

/**
 * Successful response from `dialog.confirm`.
 */
export interface DialogConfirmSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Zero-based index of the clicked button in `buttons`. `-1` only when not even the fallback message box could be shown. */
    response: number;
}

/**
 * Response from `dialog.confirm`: `DialogConfirmSuccess`, or `ApiFailure` when the call failed.
 */
export type DialogConfirmResponse = DialogConfirmSuccess | ApiFailure;

/**
 * Successful response from `dialog.openFile`.
 */
export interface DialogOpenFileSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `true` when the user closed the dialog without choosing. */
    canceled: boolean;
    /** Chosen files; empty when canceled. */
    filePaths: string[];
}

/**
 * Response from `dialog.openFile`: `DialogOpenFileSuccess`, or `ApiFailure` when the call failed.
 */
export type DialogOpenFileResponse = DialogOpenFileSuccess | ApiFailure;

/**
 * Successful response from `dialog.openFolder`.
 */
export interface DialogOpenFolderSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `true` when the user closed the dialog without choosing. */
    canceled: boolean;
    /** Chosen folder, which need not be `defaultPath`; empty when canceled. */
    folderPath: string;
}

/**
 * Response from `dialog.openFolder`: `DialogOpenFolderSuccess`, or `ApiFailure` when the call failed.
 */
export type DialogOpenFolderResponse = DialogOpenFolderSuccess | ApiFailure;

/**
 * Successful response from `dialog.saveFile`.
 */
export interface DialogSaveFileSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `true` when the user closed the dialog without choosing. */
    canceled: boolean;
    /** Chosen file; empty when canceled. The system asks before an existing file is overwritten. */
    filePath: string;
}

/**
 * Response from `dialog.saveFile`: `DialogSaveFileSuccess`, or `ApiFailure` when the call failed.
 */
export type DialogSaveFileResponse = DialogSaveFileSuccess | ApiFailure;

/**
 * Successful response from `discovery.executeContextMenuByPath`.
 */
export interface DiscoveryExecuteContextMenuByPathSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given. */
    path: string;
    /** Full name of the command that ran. */
    foundName: string;
    /** Always `unique` on success: exactly one command matched. */
    match: "unique";
    /** Number of commands the path matched; `1` on success. */
    candidateCount: number;
    /** How many tracks the command ran against. */
    itemCount: number;
}

/**
 * Response from `discovery.executeContextMenuByPath`: `DiscoveryExecuteContextMenuByPathSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryExecuteContextMenuByPathResponse = DiscoveryExecuteContextMenuByPathSuccess | ApiFailure;

/**
 * Successful response from `discovery.executeContextMenuCommand`.
 */
export interface DiscoveryExecuteContextMenuCommandSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The GUID that was executed. */
    guid: string;
    /** Label of the command. */
    name: string;
    /** Whether the host would never draw the command (`FORCE_OFF`). */
    hidden: boolean;
    /** Whether a registered item owns the GUID. */
    resolved: boolean;
    /** The `force` that applied. */
    force: boolean;
    /** How many tracks the command ran against. */
    itemCount: number;
}

/**
 * Response from `discovery.executeContextMenuCommand`: `DiscoveryExecuteContextMenuCommandSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryExecuteContextMenuCommandResponse = DiscoveryExecuteContextMenuCommandSuccess | ApiFailure;

/**
 * Successful response from `discovery.executeMainMenuCommand`.
 */
export interface DiscoveryExecuteMainMenuCommandSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The GUID that was executed. */
    guid: string;
    /** The `subGuid` that was executed; only on the dynamic path. */
    subGuid?: string;
    /** Whether the dynamic execution path was taken. */
    dynamic: boolean;
}

/**
 * Response from `discovery.executeMainMenuCommand`: `DiscoveryExecuteMainMenuCommandSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryExecuteMainMenuCommandResponse = DiscoveryExecuteMainMenuCommandSuccess | ApiFailure;

/**
 * Successful response from `discovery.getAllServices`.
 */
export interface DiscoveryGetAllServicesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Count per service family. */
    services: DiscoveryServiceCounts;
    /** How many context-menu entries the "would the host show it" filter removed. */
    contextMenuHiddenFiltered: number;
    /** Whether context-menu state was observable; false when nothing was selected or playing. */
    stateKnown: boolean;
    /** Sum of every family except `mainMenuDynamicCommands`, which `mainMenuCommands` already includes. */
    totalServices: number;
}

/**
 * Response from `discovery.getAllServices`: `DiscoveryGetAllServicesSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryGetAllServicesResponse = DiscoveryGetAllServicesSuccess | ApiFailure;

/**
 * Successful response from `discovery.getComponents`.
 */
export interface DiscoveryGetComponentsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The components. */
    components: DiscoveryComponentInfo[];
    /** Number of entries in `components`. */
    count: number;
}

/**
 * Response from `discovery.getComponents`: `DiscoveryGetComponentsSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryGetComponentsResponse = DiscoveryGetComponentsSuccess | ApiFailure;

/**
 * Successful response from `discovery.getContextMenuCommands`.
 */
export interface DiscoveryGetContextMenuCommandsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The commands. */
    commands: DiscoveryContextMenuCommand[];
    /** Number of entries in `commands`. */
    count: number;
    /** The `includeHidden` that applied. */
    includeHidden: boolean;
    /** How many entries the filter removed; `0` when `includeHidden` is set. */
    hiddenFiltered: number;
    /** Whether `enabled` and `checked` were observed: true only with a track selected or playing. */
    stateKnown: boolean;
    /** How many tracks the state was evaluated against. */
    selectionCount: number;
}

/**
 * Response from `discovery.getContextMenuCommands`: `DiscoveryGetContextMenuCommandsSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryGetContextMenuCommandsResponse = DiscoveryGetContextMenuCommandsSuccess | ApiFailure;

/**
 * Successful response from `discovery.getContextMenuTree`.
 */
export interface DiscoveryGetContextMenuTreeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The root node. */
    tree: DiscoveryContextMenuTreeNode;
    /** Whether anything in the tree was clipped. */
    truncated: boolean;
    /** Whether the depth limit clipped something. */
    depthExceeded: boolean;
    /** Whether the per-node children limit clipped something. */
    childrenExceeded: boolean;
    /** The depth limit that applied. */
    maxDepth: number;
    /** The per-node children limit that applied. */
    maxChildrenPerNode: number;
    /** How many tracks the menu was built for. */
    itemCount: number;
}

/**
 * Response from `discovery.getContextMenuTree`: `DiscoveryGetContextMenuTreeSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryGetContextMenuTreeResponse = DiscoveryGetContextMenuTreeSuccess | ApiFailure;

/**
 * Successful response from `discovery.getDspEntries`.
 */
export interface DiscoveryGetDspEntriesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The DSPs. */
    entries: DiscoveryDspEntryInfo[];
    /** Number of entries in `entries`. */
    count: number;
}

/**
 * Response from `discovery.getDspEntries`: `DiscoveryGetDspEntriesSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryGetDspEntriesResponse = DiscoveryGetDspEntriesSuccess | ApiFailure;

/**
 * Successful response from `discovery.getInputFormats`.
 */
export interface DiscoveryGetInputFormatsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The file types. */
    fileTypes: DiscoveryInputFormatType[];
    /** Number of entries in `fileTypes`. */
    count: number;
}

/**
 * Response from `discovery.getInputFormats`: `DiscoveryGetInputFormatsSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryGetInputFormatsResponse = DiscoveryGetInputFormatsSuccess | ApiFailure;

/**
 * Successful response from `discovery.getMainMenuCommands`.
 */
export interface DiscoveryGetMainMenuCommandsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The commands, static slots first within each service, followed by their expanded children. */
    commands: DiscoveryMainMenuCommand[];
    /** Number of entries in `commands`. */
    count: number;
    /** The `expandDynamic` that applied. */
    expandDynamic: boolean;
    /** The `includeHidden` that applied. */
    includeHidden: boolean;
    /** How many entries were expanded from dynamic submenus. */
    dynamicCount: number;
}

/**
 * Response from `discovery.getMainMenuCommands`: `DiscoveryGetMainMenuCommandsSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryGetMainMenuCommandsResponse = DiscoveryGetMainMenuCommandsSuccess | ApiFailure;

/**
 * Successful response from `discovery.getMainMenuGroups`.
 */
export interface DiscoveryGetMainMenuGroupsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The groups. */
    groups: DiscoveryMainMenuGroup[];
    /** Number of entries in `groups`. */
    count: number;
}

/**
 * Response from `discovery.getMainMenuGroups`: `DiscoveryGetMainMenuGroupsSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryGetMainMenuGroupsResponse = DiscoveryGetMainMenuGroupsSuccess | ApiFailure;

/**
 * Successful response from `discovery.getOutputDevices`.
 */
export interface DiscoveryGetOutputDevicesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The backends. */
    devices: DiscoveryOutputDeviceEntry[];
    /** Number of entries in `devices`. */
    count: number;
}

/**
 * Response from `discovery.getOutputDevices`: `DiscoveryGetOutputDevicesSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryGetOutputDevicesResponse = DiscoveryGetOutputDevicesSuccess | ApiFailure;

/**
 * Successful response from `discovery.getPreferencePages`.
 */
export interface DiscoveryGetPreferencePagesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The pages. */
    pages: DiscoveryPreferencePageInfo[];
    /** Number of entries in `pages`. */
    count: number;
}

/**
 * Response from `discovery.getPreferencePages`: `DiscoveryGetPreferencePagesSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryGetPreferencePagesResponse = DiscoveryGetPreferencePagesSuccess | ApiFailure;

/**
 * Successful response from `discovery.getUIElements`.
 */
export interface DiscoveryGetUIElementsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The elements. */
    elements: DiscoveryUIElementInfo[];
    /** Number of entries in `elements`. */
    count: number;
}

/**
 * Response from `discovery.getUIElements`: `DiscoveryGetUIElementsSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoveryGetUIElementsResponse = DiscoveryGetUIElementsSuccess | ApiFailure;

/**
 * Successful response from `discovery.searchCommands`.
 */
export interface DiscoverySearchCommandsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The query as given. */
    query: string;
    /** The hits, main menu first. */
    results: DiscoverySearchResult[];
    /** Number of entries in `results`. */
    count: number;
    /** The `expandDynamic` that applied. */
    expandDynamic: boolean;
    /** The `scope` that applied. */
    scope: "all" | "mainmenu" | "contextmenu";
    /** The `includeHidden` that applied. */
    includeHidden: boolean;
    /** Hits from the main menu. */
    mainMenuHits: number;
    /** Hits from the context menu. */
    contextMenuHits: number;
    /** Whether context-menu state was observed. False when the context family was searched with nothing selected or playing; the hits' `enabled` and `checked` must not be filtered on then. */
    stateKnown: boolean;
}

/**
 * Response from `discovery.searchCommands`: `DiscoverySearchCommandsSuccess`, or `ApiFailure` when the call failed.
 */
export type DiscoverySearchCommandsResponse = DiscoverySearchCommandsSuccess | ApiFailure;

/**
 * Successful response from `dnd.getCapabilities`.
 */
export interface DndGetCapabilitiesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The page receives standard HTML5 drag events; `false` means the window has no drag-drop support at all. */
    html5: boolean;
    /** Real filesystem paths are obtainable through `dnd.getPathsAsync` and the `dnd:*` events. Not fixed for the window's lifetime: a navigation or Chromium re-registering its own drop target can withdraw it, announced by `dnd:capabilitiesChanged`. */
    paths: boolean;
    /** How the window hosts its WebView: `visual` for the main and popup windows, `standard` for a DUI or CUI panel. */
    hosting: "visual" | "standard";
    /** Why `paths` is `false`; its presence always means `paths` is `false`. */
    pathsUnavailableReason?: "register-failed" | "forward-unavailable" | "inner-target-not-found" | "chain-failed" | "displaced" | "origin-untrusted";
    /** Files can be dragged out of the window through `dnd.prepareDrag`. Independent of `paths`. */
    dragOut: boolean;
    /** Why `dragOut` is `false`; its presence always means `dragOut` is `false`. */
    dragOutUnavailableReason?: "not-visual-hosting" | "runtime-too-old" | "register-failed";
}

/**
 * Response from `dnd.getCapabilities`: `DndGetCapabilitiesSuccess`, or `ApiFailure` when the call failed.
 */
export type DndGetCapabilitiesResponse = DndGetCapabilitiesSuccess | ApiFailure;

/**
 * Successful response from `dnd.getPathsAsync`.
 */
export interface DndGetPathsAsyncSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Session the paths belong to; an empty string when no session was found, so a caller can tell "nothing to report" from "a session with no files". */
    sessionId: string;
    /** Real paths in `DataTransfer.files` order; empty when the session expired, carried no file list, or the document origin is not trusted with paths. */
    paths: string[];
    /** Target of the `.lnk` shortcut at the same index of `paths`, or `null` when the entry is not a shortcut, points at a shell object rather than a file, has a target too long to read back intact, or could not be resolved; never an empty string. A broken shortcut still reports the path it recorded. Always the same length as `paths`. */
    resolvedPaths: Array<string | null>;
    /** Where the session's drag came from, as in `dnd:enter`; absent when no session was found. Reported even when the paths are withheld, since it reveals no path. */
    source?: "self" | "other-window" | "external";
}

/**
 * Response from `dnd.getPathsAsync`: `DndGetPathsAsyncSuccess`, or `ApiFailure` when the call failed.
 */
export type DndGetPathsAsyncResponse = DndGetPathsAsyncSuccess | ApiFailure;

/**
 * Successful response from `dnd.prepareDrag`.
 */
export interface DndPrepareDragSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Opaque one-shot token of 32 lowercase hex characters, valid for 30 seconds, bound to the requesting window and superseded by the next successful call. */
    token: string;
}

/**
 * Response from `dnd.prepareDrag`: `DndPrepareDragSuccess`, or `ApiFailure` when the call failed.
 */
export type DndPrepareDragResponse = DndPrepareDragSuccess | ApiFailure;

/**
 * Successful response from `dnd.startDrag`.
 */
export interface DndStartDragSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `dnd.startDrag`: `DndStartDragSuccess`, or `ApiFailure` when the call failed.
 */
export type DndStartDragResponse = DndStartDragSuccess | ApiFailure;

/**
 * Successful response from `dsp.addDsp`.
 */
export interface DspAddDspSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Display name of the processor added. */
    addedDsp: string;
    /** Position the entry landed at. */
    position: number;
}

/**
 * Response from `dsp.addDsp`: `DspAddDspSuccess`, or `ApiFailure` when the call failed.
 */
export type DspAddDspResponse = DspAddDspSuccess | ApiFailure;

/**
 * Successful response from `dsp.applyPreset`.
 */
export interface DspApplyPresetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Name of the preset now active. */
    appliedPreset: string;
    /** Index of the preset now active. */
    appliedIndex: number;
}

/**
 * Response from `dsp.applyPreset`: `DspApplyPresetSuccess`, or `ApiFailure` when the call failed.
 */
export type DspApplyPresetResponse = DspApplyPresetSuccess | ApiFailure;

/**
 * Successful response from `dsp.getAvailable`.
 */
export interface DspGetAvailableSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Every installed processor, in service enumeration order. */
    dsps: DspAvailableEntry[];
    /** Number of entries in `dsps`. */
    count: number;
}

/**
 * Response from `dsp.getAvailable`: `DspGetAvailableSuccess`, or `ApiFailure` when the call failed.
 */
export type DspGetAvailableResponse = DspGetAvailableSuccess | ApiFailure;

/**
 * Successful response from `dsp.getChain`.
 */
export interface DspGetChainSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The chain in processing order. */
    dsps: DspChainEntry[];
    /** Name of the selected preset; `null` when the chain matches no preset, as after any edit through `addDsp`, `removeDsp`, `moveDsp` or `setChain`. */
    activePreset: string | null;
    /** Index of the selected preset in `dsp.getPresets`; `-1` when none is selected. */
    activePresetIndex: number;
}

/**
 * Response from `dsp.getChain`: `DspGetChainSuccess`, or `ApiFailure` when the call failed.
 */
export type DspGetChainResponse = DspGetChainSuccess | ApiFailure;

/**
 * Successful response from `dsp.getPresets`.
 */
export interface DspGetPresetsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Every stored preset, in list order. */
    presets: DspPresetEntry[];
    /** Number of entries in `presets`. */
    count: number;
    /** Index of the selected preset; `-1` when none is selected. */
    selectedIndex: number;
}

/**
 * Response from `dsp.getPresets`: `DspGetPresetsSuccess`, or `ApiFailure` when the call failed.
 */
export type DspGetPresetsResponse = DspGetPresetsSuccess | ApiFailure;

/**
 * Successful response from `dsp.moveDsp`.
 */
export interface DspMoveDspSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Display name of the processor moved; absent when `from` equals `to` and nothing moved. */
    movedDsp?: string;
    /** The position it came from. */
    from: number;
    /** The position it is at now. */
    to: number;
    /** `No change needed` when `from` equals `to`. */
    message?: string;
}

/**
 * Response from `dsp.moveDsp`: `DspMoveDspSuccess`, or `ApiFailure` when the call failed.
 */
export type DspMoveDspResponse = DspMoveDspSuccess | ApiFailure;

/**
 * Successful response from `dsp.removeDsp`.
 */
export interface DspRemoveDspSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Display name of the processor removed. */
    removedDsp: string;
    /** Position it was removed from. */
    removedIndex: number;
}

/**
 * Response from `dsp.removeDsp`: `DspRemoveDspSuccess`, or `ApiFailure` when the call failed.
 */
export type DspRemoveDspResponse = DspRemoveDspSuccess | ApiFailure;

/**
 * Successful response from `dsp.setChain`.
 */
export interface DspSetChainSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of entries in the chain now. */
    count: number;
}

/**
 * Response from `dsp.setChain`: `DspSetChainSuccess`, or `ApiFailure` when the call failed.
 */
export type DspSetChainResponse = DspSetChainSuccess | ApiFailure;

/**
 * Successful response from `event.emit`.
 */
export interface EventEmitSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** How many windows the event was sent to, counted from the open windows. */
    recipients: number;
}

/**
 * Response from `event.emit`: `EventEmitSuccess`, or `ApiFailure` when the call failed.
 */
export type EventEmitResponse = EventEmitSuccess | ApiFailure;

/**
 * Successful response from `event.emitTo`.
 */
export interface EventEmitToSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `event.emitTo`: `EventEmitToSuccess`, or `ApiFailure` when the call failed.
 */
export type EventEmitToResponse = EventEmitToSuccess | ApiFailure;

/**
 * Successful response from `file.cancelOp`.
 */
export interface FileCancelOpSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `true` when the operation was still running and has been told to stop; `false` when it had already finished or never existed, which are deliberately indistinguishable. */
    cancelled: boolean;
}

/**
 * Response from `file.cancelOp`: `FileCancelOpSuccess`, or `ApiFailure` when the call failed.
 */
export type FileCancelOpResponse = FileCancelOpSuccess | ApiFailure;

/**
 * Successful response from `file.copy`.
 */
export interface FileCopySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The source as given. */
    source: string;
    /** The destination as given. */
    destination: string;
}

/**
 * Response from `file.copy`: `FileCopySuccess`, or `ApiFailure` when the call failed.
 */
export type FileCopyResponse = FileCopySuccess | ApiFailure;

/**
 * Successful response from `file.copyAsync`.
 */
export interface FileCopyAsyncSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the operation, starting with `fileop_`. The events of the operation carry it, and `file.cancelOp` takes it. */
    operationId: string;
    /** Number of entries accepted; the events report it as `total`. */
    totalCount: number;
}

/**
 * Response from `file.copyAsync`: `FileCopyAsyncSuccess`, or `ApiFailure` when the call failed.
 */
export type FileCopyAsyncResponse = FileCopyAsyncSuccess | ApiFailure;

/**
 * Successful response from `file.delete`.
 */
export interface FileDeleteSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `file.delete`: `FileDeleteSuccess`, or `ApiFailure` when the call failed.
 */
export type FileDeleteResponse = FileDeleteSuccess | ApiFailure;

/**
 * Successful response from `file.deleteAsync`.
 */
export interface FileDeleteAsyncSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the operation, starting with `fileop_`. The events of the operation carry it, and `file.cancelOp` takes it. */
    operationId: string;
    /** Number of entries accepted; the events report it as `total`. */
    totalCount: number;
}

/**
 * Response from `file.deleteAsync`: `FileDeleteAsyncSuccess`, or `ApiFailure` when the call failed.
 */
export type FileDeleteAsyncResponse = FileDeleteAsyncSuccess | ApiFailure;

/**
 * Successful response from `file.exists`.
 */
export interface FileExistsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the path exists. */
    exists: boolean;
    /** Whether the path is a regular file. */
    isFile: boolean;
    /** Whether the path is a directory. */
    isDirectory: boolean;
}

/**
 * Response from `file.exists`: `FileExistsSuccess`, or `ApiFailure` when the call failed.
 */
export type FileExistsResponse = FileExistsSuccess | ApiFailure;

/**
 * Successful response from `file.getInfo`.
 */
export interface FileGetInfoSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the path exists; when `false`, no other field is present. */
    exists: boolean;
    /** Whether the path is a directory. */
    isDirectory?: boolean;
    /** Whether the path is a regular file. */
    isFile?: boolean;
    /** Size in bytes; `0` for a directory. */
    size?: number;
    /** Last write time as a JavaScript timestamp in milliseconds, truncated to whole seconds. */
    modified?: number;
    /** File name with its extension. */
    name?: string;
    /** Extension with its leading dot, such as `.flac`; empty when there is none. */
    extension?: string;
    /** Parent directory of the path after `%variable%` expansion. The path is not normalized, so a doubled separator left by the expansion stays. */
    parent?: string;
}

/**
 * Response from `file.getInfo`: `FileGetInfoSuccess`, or `ApiFailure` when the call failed.
 */
export type FileGetInfoResponse = FileGetInfoSuccess | ApiFailure;

/**
 * Successful response from `file.list`.
 */
export interface FileListSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Files that match `pattern`: names, or full paths when `recursive` is set. */
    files: string[];
    /** Subdirectories, whatever `pattern` says: names, or full paths when `recursive` is set. */
    directories: string[];
    /** The same list as `files`; kept for callers that read this name. */
    items: string[];
}

/**
 * Response from `file.list`: `FileListSuccess`, or `ApiFailure` when the call failed.
 */
export type FileListResponse = FileListSuccess | ApiFailure;

/**
 * Successful response from `file.mkdir`.
 */
export interface FileMkdirSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether this call created the directory; `false` when it already existed. */
    created: boolean;
    /** `Directory already exists` when the directory was already there; absent otherwise. */
    message?: string;
}

/**
 * Response from `file.mkdir`: `FileMkdirSuccess`, or `ApiFailure` when the call failed.
 */
export type FileMkdirResponse = FileMkdirSuccess | ApiFailure;

/**
 * Successful response from `file.move`.
 */
export interface FileMoveSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The source as given. */
    source: string;
    /** The destination as given. */
    destination: string;
}

/**
 * Response from `file.move`: `FileMoveSuccess`, or `ApiFailure` when the call failed.
 */
export type FileMoveResponse = FileMoveSuccess | ApiFailure;

/**
 * Successful response from `file.moveAsync`.
 */
export interface FileMoveAsyncSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the operation, starting with `fileop_`. The events of the operation carry it, and `file.cancelOp` takes it. */
    operationId: string;
    /** Number of entries accepted; the events report it as `total`. */
    totalCount: number;
}

/**
 * Response from `file.moveAsync`: `FileMoveAsyncSuccess`, or `ApiFailure` when the call failed.
 */
export type FileMoveAsyncResponse = FileMoveAsyncSuccess | ApiFailure;

/**
 * Successful response from `file.read`.
 */
export interface FileReadSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The file's text; for a binary read, its bytes as plain Base64 without a `base64:` prefix. */
    content: string;
    /** Size of the file on disk in bytes; for a text read it can differ from the length of `content`. */
    size: number;
    /** Present only for a binary read, and then always `base64`. */
    encoding?: string;
}

/**
 * Response from `file.read`: `FileReadSuccess`, or `ApiFailure` when the call failed.
 */
export type FileReadResponse = FileReadSuccess | ApiFailure;

/**
 * Successful response from `file.rename`.
 */
export interface FileRenameSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given. */
    oldPath: string;
    /** The parent directory of the expanded `path`, joined with `newName`. */
    newPath: string;
}

/**
 * Response from `file.rename`: `FileRenameSuccess`, or `ApiFailure` when the call failed.
 */
export type FileRenameResponse = FileRenameSuccess | ApiFailure;

/**
 * Successful response from `file.write`.
 */
export interface FileWriteSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Size of the file after the write, in bytes; with `append` it is the whole file, not just the part added. */
    bytesWritten: number;
}

/**
 * Response from `file.write`: `FileWriteSuccess`, or `ApiFailure` when the call failed.
 */
export type FileWriteResponse = FileWriteSuccess | ApiFailure;

/**
 * Successful response from `http.abort`.
 */
export interface HttpAbortSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The id, as given. */
    requestId: string;
    /** `true` when the request was still running and has been told to stop; `false` when it had already ended or never existed. */
    cancelled: boolean;
}

/**
 * Response from `http.abort`: `HttpAbortSuccess`, or `ApiFailure` when the call failed.
 */
export type HttpAbortResponse = HttpAbortSuccess | ApiFailure;

/**
 * Successful response from `http.delete`.
 */
export interface HttpDeleteSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the request; present in an async receipt. `http:response` and `http.abort` use it. */
    requestId?: string;
    /** `true` in an async receipt. */
    async?: boolean;
    /** HTTP status code; present in a synchronous response. */
    status?: number;
    /** Response headers by name; of repeated headers the last one wins. Present in a synchronous response. */
    headers?: Record<string, string>;
    /** Response body, as text or Base64 according to `responseType`; present in a synchronous response. */
    body?: string;
    /** `text` or `base64`, the encoding of `body`; present in a synchronous response. */
    responseType?: "text" | "base64";
}

/**
 * Response from `http.delete`: `HttpDeleteSuccess`, or `ApiFailure` when the call failed.
 */
export type HttpDeleteResponse = HttpDeleteSuccess | ApiFailure;

/**
 * Successful response from `http.download`.
 */
export interface HttpDownloadSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the download; present in an async receipt. `http:downloadComplete` and `http.abort` use it. */
    requestId?: string;
    /** `true` in an async receipt. */
    async?: boolean;
    /** `Download started` in an async receipt. */
    message?: string;
    /** HTTP status code; present when the download finished synchronously. */
    status?: number;
    /** Bytes written; present when the download finished synchronously. */
    bytesWritten?: number;
    /** The file written, with path variables expanded; present when the download finished synchronously. */
    path?: string;
}

/**
 * Response from `http.download`: `HttpDownloadSuccess`, or `ApiFailure` when the call failed.
 */
export type HttpDownloadResponse = HttpDownloadSuccess | ApiFailure;

/**
 * Successful response from `http.get`.
 */
export interface HttpGetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the request; present in an async receipt. `http:response` and `http.abort` use it. */
    requestId?: string;
    /** `true` in an async receipt. */
    async?: boolean;
    /** HTTP status code; present in a synchronous response. */
    status?: number;
    /** Response headers by name; of repeated headers the last one wins. Present in a synchronous response. */
    headers?: Record<string, string>;
    /** Response body, as text or Base64 according to `responseType`; present in a synchronous response. */
    body?: string;
    /** `text` or `base64`, the encoding of `body`; present in a synchronous response. */
    responseType?: "text" | "base64";
}

/**
 * Response from `http.get`: `HttpGetSuccess`, or `ApiFailure` when the call failed.
 */
export type HttpGetResponse = HttpGetSuccess | ApiFailure;

/**
 * Successful response from `http.head`.
 */
export interface HttpHeadSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the request; present in an async receipt. `http:response` and `http.abort` use it. */
    requestId?: string;
    /** `true` in an async receipt. */
    async?: boolean;
    /** HTTP status code; present in a synchronous response. */
    status?: number;
    /** Response headers by name; of repeated headers the last one wins. Present in a synchronous response. */
    headers?: Record<string, string>;
    /** Always empty; present in a synchronous response. */
    body?: string;
    /** Always `text`; present in a synchronous response. */
    responseType?: "text" | "base64";
    /** The `Content-Length` header as a number; present in a synchronous response when the server sends one that parses. */
    contentLength?: number;
}

/**
 * Response from `http.head`: `HttpHeadSuccess`, or `ApiFailure` when the call failed.
 */
export type HttpHeadResponse = HttpHeadSuccess | ApiFailure;

/**
 * Successful response from `http.patch`.
 */
export interface HttpPatchSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the request; present in an async receipt. `http:response` and `http.abort` use it. */
    requestId?: string;
    /** `true` in an async receipt. */
    async?: boolean;
    /** HTTP status code; present in a synchronous response. */
    status?: number;
    /** Response headers by name; of repeated headers the last one wins. Present in a synchronous response. */
    headers?: Record<string, string>;
    /** Response body, as text or Base64 according to `responseType`; present in a synchronous response. */
    body?: string;
    /** `text` or `base64`, the encoding of `body`; present in a synchronous response. */
    responseType?: "text" | "base64";
}

/**
 * Response from `http.patch`: `HttpPatchSuccess`, or `ApiFailure` when the call failed.
 */
export type HttpPatchResponse = HttpPatchSuccess | ApiFailure;

/**
 * Successful response from `http.post`.
 */
export interface HttpPostSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the request; present in an async receipt. `http:response` and `http.abort` use it. */
    requestId?: string;
    /** `true` in an async receipt. */
    async?: boolean;
    /** HTTP status code; present in a synchronous response. */
    status?: number;
    /** Response headers by name; of repeated headers the last one wins. Present in a synchronous response. */
    headers?: Record<string, string>;
    /** Response body, as text or Base64 according to `responseType`; present in a synchronous response. */
    body?: string;
    /** `text` or `base64`, the encoding of `body`; present in a synchronous response. */
    responseType?: "text" | "base64";
}

/**
 * Response from `http.post`: `HttpPostSuccess`, or `ApiFailure` when the call failed.
 */
export type HttpPostResponse = HttpPostSuccess | ApiFailure;

/**
 * Successful response from `http.put`.
 */
export interface HttpPutSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the request; present in an async receipt. `http:response` and `http.abort` use it. */
    requestId?: string;
    /** `true` in an async receipt. */
    async?: boolean;
    /** HTTP status code; present in a synchronous response. */
    status?: number;
    /** Response headers by name; of repeated headers the last one wins. Present in a synchronous response. */
    headers?: Record<string, string>;
    /** Response body, as text or Base64 according to `responseType`; present in a synchronous response. */
    body?: string;
    /** `text` or `base64`, the encoding of `body`; present in a synchronous response. */
    responseType?: "text" | "base64";
}

/**
 * Response from `http.put`: `HttpPutSuccess`, or `ApiFailure` when the call failed.
 */
export type HttpPutResponse = HttpPutSuccess | ApiFailure;

/**
 * Successful response from `jitQueue.clear`.
 */
export interface JitQueueClearSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `jitQueue.clear`: `JitQueueClearSuccess`, or `ApiFailure` when the call failed.
 */
export type JitQueueClearResponse = JitQueueClearSuccess | ApiFailure;

/**
 * Successful response from `jitQueue.enqueueNext`.
 */
export interface JitQueueEnqueueNextSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The identifier given. */
    trackId: string;
    /** Tracks in the buffer afterwards. */
    bufferSize: number;
}

/**
 * Response from `jitQueue.enqueueNext`: `JitQueueEnqueueNextSuccess`, or `ApiFailure` when the call failed.
 */
export type JitQueueEnqueueNextResponse = JitQueueEnqueueNextSuccess | ApiFailure;

/**
 * Successful response from `jitQueue.getState`.
 */
export interface JitQueueGetStateSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether a JIT session is active. */
    isActive: boolean;
    /** The session state. */
    state: "Idle" | "Active" | "WaitingNext" | "Exhausted" | "Unknown";
    /** Identifier of the playing track; empty when none. */
    currentTrackId: string;
    /** Identifier of the buffered next track; empty when none. */
    nextTrackId: string;
    /** Tracks in the buffer. */
    bufferSize: number;
    /** Index of the shadow playlist; `-1` until a `jitQueue.playNow` or `jitQueue.preloadBatch` of this foobar2000 session creates or takes it over, even when one is left from an earlier session, and again after the playlist is removed, which also ends the JIT session. Stopping or clearing keeps the index of the emptied playlist. */
    shadowPlaylist: number;
}

/**
 * Response from `jitQueue.getState`: `JitQueueGetStateSuccess`, or `ApiFailure` when the call failed.
 */
export type JitQueueGetStateResponse = JitQueueGetStateSuccess | ApiFailure;

/**
 * Successful response from `jitQueue.notifyEmpty`.
 */
export interface JitQueueNotifyEmptySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `jitQueue.notifyEmpty`: `JitQueueNotifyEmptySuccess`, or `ApiFailure` when the call failed.
 */
export type JitQueueNotifyEmptyResponse = JitQueueNotifyEmptySuccess | ApiFailure;

/**
 * Successful response from `jitQueue.playNow`.
 */
export interface JitQueuePlayNowSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The identifier given. */
    trackId: string;
    /** Index of the shadow playlist, which the call creates or takes over when this session has none; `-1` only if foobar2000 could not create it. */
    shadowPlaylist: number;
}

/**
 * Response from `jitQueue.playNow`: `JitQueuePlayNowSuccess`, or `ApiFailure` when the call failed.
 */
export type JitQueuePlayNowResponse = JitQueuePlayNowSuccess | ApiFailure;

/**
 * Successful response from `jitQueue.preloadBatch`.
 */
export interface JitQueuePreloadBatchSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** How many tracks were inserted. */
    tracksAdded: number;
    /** How many entries were dropped for being too long. */
    invalidCount: number;
}

/**
 * Response from `jitQueue.preloadBatch`: `JitQueuePreloadBatchSuccess`, or `ApiFailure` when the call failed.
 */
export type JitQueuePreloadBatchResponse = JitQueuePreloadBatchSuccess | ApiFailure;

/**
 * Successful response from `jitQueue.skip`.
 */
export interface JitQueueSkipSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Identifier of the track playing after the skip. */
    currentTrackId: string;
}

/**
 * Response from `jitQueue.skip`: `JitQueueSkipSuccess`, or `ApiFailure` when the call failed.
 */
export type JitQueueSkipResponse = JitQueueSkipSuccess | ApiFailure;

/**
 * Successful response from `jitQueue.stop`.
 */
export interface JitQueueStopSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `jitQueue.stop`: `JitQueueStopSuccess`, or `ApiFailure` when the call failed.
 */
export type JitQueueStopResponse = JitQueueStopSuccess | ApiFailure;

/**
 * Successful response from `keyboard.getRegisteredHotkeys`.
 */
export interface KeyboardGetRegisteredHotkeysSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Hotkeys first, then shortcuts. */
    hotkeys: KeyboardHotkey[];
}

/**
 * Response from `keyboard.getRegisteredHotkeys`: `KeyboardGetRegisteredHotkeysSuccess`, or `ApiFailure` when the call failed.
 */
export type KeyboardGetRegisteredHotkeysResponse = KeyboardGetRegisteredHotkeysSuccess | ApiFailure;

/**
 * Successful response from `keyboard.registerHotkey`.
 */
export interface KeyboardRegisterHotkeySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the hotkey, from `1`; the key `unregisterHotkey` takes. */
    id: number;
}

/**
 * Response from `keyboard.registerHotkey`: `KeyboardRegisterHotkeySuccess`, or `ApiFailure` when the call failed.
 */
export type KeyboardRegisterHotkeyResponse = KeyboardRegisterHotkeySuccess | ApiFailure;

/**
 * Successful response from `keyboard.registerShortcut`.
 */
export interface KeyboardRegisterShortcutSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `keyboard.registerShortcut`: `KeyboardRegisterShortcutSuccess`, or `ApiFailure` when the call failed.
 */
export type KeyboardRegisterShortcutResponse = KeyboardRegisterShortcutSuccess | ApiFailure;

/**
 * Successful response from `keyboard.unregisterHotkey`.
 */
export interface KeyboardUnregisterHotkeySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `keyboard.unregisterHotkey`: `KeyboardUnregisterHotkeySuccess`, or `ApiFailure` when the call failed.
 */
export type KeyboardUnregisterHotkeyResponse = KeyboardUnregisterHotkeySuccess | ApiFailure;

/**
 * Successful response from `library.addToPlaylist`.
 */
export interface LibraryAddToPlaylistSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Tracks added. */
    added: number;
}

/**
 * Response from `library.addToPlaylist`: `LibraryAddToPlaylistSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryAddToPlaylistResponse = LibraryAddToPlaylistSuccess | ApiFailure;

/**
 * Successful response from `library.browseDirectory`.
 */
export interface LibraryBrowseDirectorySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Folders one level below `path`, as stored paths, in byte order. */
    directories: string[];
    /** The same list as `directories`. */
    items: string[];
    /** Tracks under `path` in library order; `index` is the position in the library. Empty without `includeFiles`. */
    files: LibraryTrack[];
}

/**
 * Response from `library.browseDirectory`: `LibraryBrowseDirectorySuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryBrowseDirectoryResponse = LibraryBrowseDirectorySuccess | ApiFailure;

/**
 * Successful response from `library.browseTree`.
 */
export interface LibraryBrowseTreeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The root. */
    root: LibraryRootInfo;
    /** The `pathId` opened. */
    pathId: string;
    /** Local path of the folder. */
    absolutePath: string;
    /** Subfolders directly inside, ordered by `displayName` and then `absolutePath`, ignoring case. */
    directories: LibraryDirectoryNodeInfo[];
    /** The folder's tracks in library order, followed with `recursiveFiles` by those of the folders below in no fixed order; empty without `includeFiles`. `index` is the position in the library when the tree index was built. */
    files: LibraryTrack[];
    /** `true` when the tree index already existed before this call. */
    fromCache: boolean;
}

/**
 * Response from `library.browseTree`: `LibraryBrowseTreeSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryBrowseTreeResponse = LibraryBrowseTreeSuccess | ApiFailure;

/**
 * Successful response from `library.getAlbums`.
 */
export interface LibraryGetAlbumsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The albums of this page, sorted by `sort`. */
    albums: AlbumInfo[];
    /** Albums matching `query`, before `offset` and `limit`. */
    total: number;
    /** The `offset` applied. */
    offset: number;
    /** The `limit` applied. */
    limit: number;
    /** Whether albums remain after this page. */
    hasMore: boolean;
    /** The `includeCover` applied. */
    includeCover: boolean;
    /** `true` when the answer came from the kept list. */
    fromCache: boolean;
}

/**
 * Response from `library.getAlbums`: `LibraryGetAlbumsSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetAlbumsResponse = LibraryGetAlbumsSuccess | ApiFailure;

/**
 * Successful response from `library.getAlbumTracks`.
 */
export interface LibraryGetAlbumTracksSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The album name, as given. */
    album: string;
    /** The album artist, as given. */
    albumArtist: string;
    /** The album's row as `library.getAlbums` returns it, without `coverDataUrl` and `tracks`; absent when no row has this name and album artist. */
    row?: AlbumInfo;
    /** The tracks, sorted by disc number, then track number, then library order; `index` is the position in this list. */
    tracks: LibraryTrack[];
    /** The same list as `tracks`. */
    items: LibraryTrack[];
    /** Number of tracks. */
    total: number;
}

/**
 * Response from `library.getAlbumTracks`: `LibraryGetAlbumTracksSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetAlbumTracksResponse = LibraryGetAlbumTracksSuccess | ApiFailure;

/**
 * Successful response from `library.getAll`.
 */
export interface LibraryGetAllSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `true` when the page will arrive as the `library:getAllResult` event; the page fields are then absent. */
    pending?: boolean;
    /** Id the `library:getAllResult` event carries; present with `pending`. */
    requestId?: string;
    /** The page in library order; `index` is the position in the library. */
    tracks?: LibraryTrack[];
    /** The same list as `tracks`. */
    items?: LibraryTrack[];
    /** Tracks in the library. */
    total?: number;
    /** The `offset` applied. */
    offset?: number;
    /** The `limit` applied. */
    limit?: number;
    /** `true` when the page came from the kept list. */
    fromCache?: boolean;
}

/**
 * Response from `library.getAll`: `LibraryGetAllSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetAllResponse = LibraryGetAllSuccess | ApiFailure;

/**
 * Successful response from `library.getArtistAlbums`.
 */
export interface LibraryGetArtistAlbumsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The artist, as given. */
    artist: string;
    /** The albums, sorted by `sort`. Rows never carry `coverDataUrl` or `tracks`. */
    albums: AlbumInfo[];
    /** Albums found, before `limit`. */
    total: number;
    /** Whether `limit` cut the list short. */
    hasMore: boolean;
}

/**
 * Response from `library.getArtistAlbums`: `LibraryGetArtistAlbumsSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetArtistAlbumsResponse = LibraryGetArtistAlbumsSuccess | ApiFailure;

/**
 * Successful response from `library.getArtists`.
 */
export interface LibraryGetArtistsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The artists, sorted by `sort`. */
    items: ArtistInfo[];
    /** Rows returned, after `limit`; there is no total, so a full page means there may be more. */
    count: number;
}

/**
 * Response from `library.getArtists`: `LibraryGetArtistsSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetArtistsResponse = LibraryGetArtistsSuccess | ApiFailure;

/**
 * Successful response from `library.getArtistTracks`.
 */
export interface LibraryGetArtistTracksSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The artist, as given. */
    artist: string;
    /** The tracks in library order; `index` is the position in this list. */
    tracks: LibraryTrack[];
    /** The same list as `tracks`. */
    items: LibraryTrack[];
    /** The same as `count`; there is no count before `limit`, so a full page means there may be more. */
    total: number;
    /** Tracks returned. */
    count: number;
}

/**
 * Response from `library.getArtistTracks`: `LibraryGetArtistTracksSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetArtistTracksResponse = LibraryGetArtistTracksSuccess | ApiFailure;

/**
 * Successful response from `library.getByPath`.
 */
export interface LibraryGetByPathSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the file is in the library; the other fields besides `path` are present only when it is. */
    found: boolean;
    /** The track path as foobar2000 stores it when found; otherwise the path as given. */
    path: string;
    /** The track path as a native file path. */
    absolutePath?: string;
    /** The first `title` value; empty when absent. */
    title?: string;
    /** Every `artist` value joined with `, ` in their original order. */
    artist?: string;
    /** Every `artist` value; `artists.join(', ')` equals `artist`. */
    artists?: string[];
    /** The first `album` value; empty when absent. */
    album?: string;
    /** Length in seconds. */
    duration?: number;
    /** The first `tracknumber` value as written in the tag, such as `2` or `02/12`; empty when absent. */
    trackNumber?: string;
    /** Every `genre` value joined with `, `. */
    genre?: string;
    /** The first `date` value; empty when absent. */
    date?: string;
}

/**
 * Response from `library.getByPath`: `LibraryGetByPathSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetByPathResponse = LibraryGetByPathSuccess | ApiFailure;

/**
 * Successful response from `library.getCacheStats`.
 */
export interface LibraryGetCacheStatsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether any library result was kept since the cache was last dropped. */
    valid: boolean;
    /** When the cache was last dropped, in milliseconds since the Unix epoch. */
    lastModified: number;
    /** Kept `library.getAlbums` lists, one per `query`, `sort` and `includeCover`. */
    albumsCacheEntries: number;
    /** Whether a full `library.getAll` result is kept. */
    tracksCached: boolean;
    /** Whether the `library.getArtists` scan is kept. */
    artistsCached: boolean;
    /** Always `false`; genres are not kept. */
    genresCached: boolean;
    /** Whether the `library.getStatus` answer is kept. */
    statsCached: boolean;
    /** Always `0`; covers are not kept. */
    coversCached: number;
    /** Always `0`; covers are not kept. */
    coverCacheBytes: number;
    /** Always `0`; covers are not kept. */
    coverCacheMB: number;
    /** Lookups answered from a kept result since the host started. */
    cacheHits: number;
    /** Lookups that found nothing kept since the host started. */
    cacheMisses: number;
    /** Whether the directory tree index is built. */
    treeIndexValid: boolean;
    /** Roots in the tree index; `0` when it is not built. */
    rootsCached: number;
    /** Tracks placed in the tree index at its last build. */
    treeIndexedTracks: number;
    /** Tracks left out of the tree index at its last build. */
    treeSkippedTracks: number;
    /** When the tree index was last built, in milliseconds since the Unix epoch; `0` before the first build. */
    treeLastBuilt: number;
}

/**
 * Response from `library.getCacheStats`: `LibraryGetCacheStatsSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetCacheStatsResponse = LibraryGetCacheStatsSuccess | ApiFailure;

/**
 * Successful response from `library.getCount`.
 */
export interface LibraryGetCountSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Tracks in the library. */
    count: number;
}

/**
 * Response from `library.getCount`: `LibraryGetCountSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetCountResponse = LibraryGetCountSuccess | ApiFailure;

/**
 * Successful response from `library.getFieldValues`.
 */
export interface LibraryGetFieldValuesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The values, most tracks first. */
    values: LibraryValueCount[];
    /** Distinct values found, before `limit`. */
    total: number;
    /** The tag name, as given. */
    field: string;
}

/**
 * Response from `library.getFieldValues`: `LibraryGetFieldValuesSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetFieldValuesResponse = LibraryGetFieldValuesSuccess | ApiFailure;

/**
 * Successful response from `library.getGenres`.
 */
export interface LibraryGetGenresSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Every genre with its track count. */
    genres: LibraryValueCount[];
}

/**
 * Response from `library.getGenres`: `LibraryGetGenresSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetGenresResponse = LibraryGetGenresSuccess | ApiFailure;

/**
 * Successful response from `library.getRandomTracks`.
 */
export interface LibraryGetRandomTracksSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The drawn tracks; `index` is the position in this list. */
    tracks: LibraryTrack[];
    /** Tracks returned. */
    count: number;
}

/**
 * Response from `library.getRandomTracks`: `LibraryGetRandomTracksSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetRandomTracksResponse = LibraryGetRandomTracksSuccess | ApiFailure;

/**
 * Successful response from `library.getRecentlyAdded`.
 */
export interface LibraryGetRecentlyAddedSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The tracks, newest first; `index` is the position in the library. */
    tracks: RecentLibraryTrack[];
    /** Tracks in the library, not the number returned. */
    total: number;
    /** The `limit` applied. */
    limit: number;
    /** The order used: `modified` when `added` was asked for and no track has `%added%`. */
    sortBy: "added" | "modified";
    /** Whether the call fell back from `added` to `modified`. */
    fallback: boolean;
}

/**
 * Response from `library.getRecentlyAdded`: `LibraryGetRecentlyAddedSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetRecentlyAddedResponse = LibraryGetRecentlyAddedSuccess | ApiFailure;

/**
 * Successful response from `library.getRoots`.
 */
export interface LibraryGetRootsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the media library is enabled. */
    enabled: boolean;
    /** The root folders. */
    roots: LibraryRootInfo[];
    /** Number of roots. */
    total: number;
    /** Tracks placed under a root. */
    indexedTracks: number;
    /** Tracks left out because they have no stable local path. */
    skippedTracks: number;
    /** `true` when the index already existed before this call. */
    fromCache: boolean;
}

/**
 * Response from `library.getRoots`: `LibraryGetRootsSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetRootsResponse = LibraryGetRootsSuccess | ApiFailure;

/**
 * Successful response from `library.getStats`.
 */
export interface LibraryGetStatsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Tracks in the library. */
    totalTracks: number;
    /** Albums, counted by album name plus album artist, as `library.getAlbums` groups them. */
    totalAlbums: number;
    /** Credited artists: every value of a multi-value `artist` counts, matching the entry count of `library.getArtists`. */
    totalArtists: number;
    /** Summed length of all tracks, in seconds. */
    totalDuration: number;
    /** Summed file size, in bytes. */
    totalSize: number;
    /** Whether the host holds cached library results written since the cache was last dropped. */
    cacheValid: boolean;
    /** When the cache was last dropped, in milliseconds since the Unix epoch. */
    lastModified: number;
}

/**
 * Response from `library.getStats`: `LibraryGetStatsSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetStatsResponse = LibraryGetStatsSuccess | ApiFailure;

/**
 * Successful response from `library.getStatus`.
 */
export interface LibraryGetStatusSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the media library is enabled. */
    enabled: boolean;
    /** Always the same as `enabled`. */
    initialized: boolean;
    /** Always `false`; the host does not report scanning. */
    scanning: boolean;
    /** Tracks in the library; `0` when it is disabled. */
    itemCount: number;
    /** The same as `itemCount`. */
    count: number;
}

/**
 * Response from `library.getStatus`: `LibraryGetStatusSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryGetStatusResponse = LibraryGetStatusSuccess | ApiFailure;

/**
 * Successful response from `library.invalidateCache`.
 */
export interface LibraryInvalidateCacheSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** When the cache was dropped, in milliseconds since the Unix epoch. */
    timestamp: number;
}

/**
 * Response from `library.invalidateCache`: `LibraryInvalidateCacheSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryInvalidateCacheResponse = LibraryInvalidateCacheSuccess | ApiFailure;

/**
 * Successful response from `library.isEnabled`.
 */
export interface LibraryIsEnabledSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the media library is enabled. */
    enabled: boolean;
}

/**
 * Response from `library.isEnabled`: `LibraryIsEnabledSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryIsEnabledResponse = LibraryIsEnabledSuccess | ApiFailure;

/**
 * Successful response from `library.query`.
 */
export interface LibraryQuerySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The matches, sorted by `sort` or in library order; `index` is the position in this list. Each row carries exactly the `fields` asked for. */
    tracks: LibraryTrackPartial[];
    /** Matches, before `limit`. */
    total: number;
}

/**
 * Response from `library.query`: `LibraryQuerySuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryQueryResponse = LibraryQuerySuccess | ApiFailure;

/**
 * Successful response from `library.refresh`.
 */
export interface LibraryRefreshSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `library.refresh`: `LibraryRefreshSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryRefreshResponse = LibraryRefreshSuccess | ApiFailure;

/**
 * Successful response from `library.rescan`.
 */
export interface LibraryRescanSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `library.rescan`: `LibraryRescanSuccess`, or `ApiFailure` when the call failed.
 */
export type LibraryRescanResponse = LibraryRescanSuccess | ApiFailure;

/**
 * Successful response from `library.search`.
 */
export interface LibrarySearchSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** This page of the matches in library order; `index` is the position among all matches. Each row carries exactly the `fields` asked for. */
    tracks: LibraryTrackPartial[];
    /** Matches, before `offset` and `limit`. */
    total: number;
    /** The `offset` applied. */
    offset: number;
    /** The `limit` applied. */
    limit: number;
    /** Whether matches remain after this page. */
    hasMore: boolean;
}

/**
 * Response from `library.search`: `LibrarySearchSuccess`, or `ApiFailure` when the call failed.
 */
export type LibrarySearchResponse = LibrarySearchSuccess | ApiFailure;

/**
 * Successful response from `log.clear`.
 */
export interface LogClearSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `log.clear`: `LogClearSuccess`, or `ApiFailure` when the call failed.
 */
export type LogClearResponse = LogClearSuccess | ApiFailure;

/**
 * Successful response from `log.read`.
 */
export interface LogReadSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The returned lines joined with `\n`. */
    content: string;
    /** The returned lines. Absent when the file does not exist. */
    lines?: string[];
    /** Number of lines returned. */
    lineCount: number;
    /** Number of lines in the whole file. Absent when the file does not exist. */
    totalLines?: number;
}

/**
 * Response from `log.read`: `LogReadSuccess`, or `ApiFailure` when the call failed.
 */
export type LogReadResponse = LogReadSuccess | ApiFailure;

/**
 * Successful response from `log.write`.
 */
export interface LogWriteSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Full path of the file that was written. */
    path: string;
}

/**
 * Response from `log.write`: `LogWriteSuccess`, or `ApiFailure` when the call failed.
 */
export type LogWriteResponse = LogWriteSuccess | ApiFailure;

/**
 * Successful response from `lyrics.exists`.
 */
export interface LyricsExistsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether any source has lyrics. */
    exists: boolean;
    /** Every source found: `embedded` for a lyrics tag, then `file:<name>` for each sidecar file, in the same candidate order as `lyrics.get` with `format=any`, or just the explicit file. */
    sources: string[];
}

/**
 * Response from `lyrics.exists`: `LyricsExistsSuccess`, or `ApiFailure` when the call failed.
 */
export type LyricsExistsResponse = LyricsExistsSuccess | ApiFailure;

/**
 * Successful response from `lyrics.get`.
 */
export interface LyricsGetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether lyrics were found. */
    available: boolean;
    /** The path looked up: as given, or the playing track's path including its subsong suffix. */
    path: string;
    /** Where the lyrics came from; present when `available` is `true`. */
    source?: "embedded" | "file";
    /** Full path of the sidecar file; present when `source` is `file`. */
    sourcePath?: string;
    /** The tag that matched a `type` filter, one of `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`, `SYNCEDLYRICS`, `SYNCED LYRICS`; present when `source` is `embedded`, `type` was not `any` and a known tag matched. */
    tagName?: string;
    /** The lyrics text; present when `available` is `true`. */
    lyrics?: string;
    /** Whether the text carries LRC timestamps; present when `available` is `true`. */
    synced?: boolean;
}

/**
 * Response from `lyrics.get`: `LyricsGetSuccess`, or `ApiFailure` when the call failed.
 */
export type LyricsGetResponse = LyricsGetSuccess | ApiFailure;

/**
 * Successful response from `lyrics.save`.
 */
export interface LyricsSaveSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Full path of the file written; present for a single `file` target. */
    savedTo?: string;
    /** `true` when a single `embedded` target dispatched the tag write; the final outcome arrives as `metadata:writeComplete`. */
    dispatched?: boolean;
    /** The path as given; present for a single `embedded` target. */
    path?: string;
    /** Path of the handle the tag write targets; present for a single `embedded` target. */
    handlePath?: string;
    /** Subsong the tag write targets; present for a single `embedded` target. */
    subsong?: number;
    /** The tag written, keyed by its upper-case name; present for a single `embedded` target. */
    tagsApplied?: Record<string, string>;
    /** Number of tags set; present for a single `embedded` target. */
    tagsSet?: number;
    /** Number of tags removed; present for a single `embedded` target. */
    tagsRemoved?: number;
    /** A note about the dispatch; present for a single `embedded` target. */
    note?: string;
    /** With several targets: each target's own envelope (`{ success, savedTo }`, the tag write receipt, or `{ success: false, error, code }`) under `file` or `embedded`. */
    results?: Record<string, JsonValue>;
}

/**
 * Response from `lyrics.save`: `LyricsSaveSuccess`, or `ApiFailure` when the call failed.
 */
export type LyricsSaveResponse = LyricsSaveSuccess | ApiFailure;

/**
 * Successful response from `media.getContainerInfo`.
 * @experimental
 */
export interface MediaGetContainerInfoSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the format belongs to a supported container family. */
    recognized: boolean;
    /** Container identified by its file structure. */
    container?: "mp4" | "mov" | "matroska" | "webm";
    /** MIME type of this container. */
    mimeType?: string;
    /** Declared presentation duration in seconds, when known. */
    duration?: number;
    /** Declared tracks; empty for unrecognized formats. */
    tracks: ContainerTrack[];
    /** Attachment metadata without file contents. */
    attachments: ContainerAttachment[];
    /** Chapters in container order, which is foobar2000's subsong order, not sorted by start. MP4 uses the QuickTime chapter track when present, otherwise Nero chpl; Matroska lists the top-level chapters of every edition, hidden ones included. Empty when the container declares none; omitted when the chapter structure is malformed or links other files. */
    chapters?: ContainerChapter[];
}

/**
 * Response from `media.getContainerInfo`: `MediaGetContainerInfoSuccess`, or `ApiFailure` when the call failed.
 * @experimental
 */
export type MediaGetContainerInfoResponse = MediaGetContainerInfoSuccess | ApiFailure;

/**
 * Successful response from `media.getStreamUrl`.
 * @experimental
 */
export interface MediaGetStreamUrlSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Opaque URL bound to the calling document. */
    url: string;
    /** File size in bytes, within JavaScript's safe integer range. */
    size: number;
    /** MIME type identified from file content; application/octet-stream when unknown. */
    mimeType: string;
}

/**
 * Response from `media.getStreamUrl`: `MediaGetStreamUrlSuccess`, or `ApiFailure` when the call failed.
 * @experimental
 */
export type MediaGetStreamUrlResponse = MediaGetStreamUrlSuccess | ApiFailure;

/**
 * Successful response from `menu.close`.
 */
export interface MenuCloseSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `menu.close`: `MenuCloseSuccess`, or `ApiFailure` when the call failed.
 */
export type MenuCloseResponse = MenuCloseSuccess | ApiFailure;

/**
 * Successful response from `menu.getContextMenu`.
 */
export interface MenuGetContextMenuSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The mode used; `auto` resolves to one of the others. */
    mode: "handles" | "playlist" | "nowPlaying" | "selection";
    /** The `locale` used. */
    locale: string;
    /** The `i18n` used. */
    i18n: boolean;
    /** The `withAvailability` used. */
    withAvailability: boolean;
    /** The top-level nodes. */
    items: MenuTreeNode[];
}

/**
 * Response from `menu.getContextMenu`: `MenuGetContextMenuSuccess`, or `ApiFailure` when the call failed.
 */
export type MenuGetContextMenuResponse = MenuGetContextMenuSuccess | ApiFailure;

/**
 * Successful response from `menu.getMainMenu`.
 */
export interface MenuGetMainMenuSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Label of the submenu returned; empty for the whole menu. */
    root: string;
    /** The `root` that was asked for. */
    requestedRoot: string;
    /** Whether `root` was found; true when none was asked for, except on the flat list, which is always false. */
    rootMatched: boolean;
    /** The `locale` used. */
    locale: string;
    /** The `i18n` used. */
    i18n: boolean;
    /** The `withAvailability` used. */
    withAvailability: boolean;
    /** The top-level nodes. */
    items: MenuTreeNode[];
    /** `v1-hmenu` when the tree was built from the Win32 menus. */
    source?: "v1-hmenu";
    /** `flat-mainmenu-commands` when only the flat command list could be built. */
    fallback?: "flat-mainmenu-commands";
}

/**
 * Response from `menu.getMainMenu`: `MenuGetMainMenuSuccess`, or `ApiFailure` when the call failed.
 */
export type MenuGetMainMenuResponse = MenuGetMainMenuSuccess | ApiFailure;

/**
 * Successful response from `menu.runContextCommand`.
 */
export interface MenuRunContextCommandSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** GUID of the command that ran; absent when it was reached by name and has no GUID. */
    guid?: string;
    /** How many tracks the command ran on. */
    itemCount: number;
    /** `false` when the command has no GUID and was handed to a host entry point that reports nothing back, so whether it ran is unknown. */
    executionConfirmed: boolean;
}

/**
 * Response from `menu.runContextCommand`: `MenuRunContextCommandSuccess`, or `ApiFailure` when the call failed.
 */
export type MenuRunContextCommandResponse = MenuRunContextCommandSuccess | ApiFailure;

/**
 * Successful response from `menu.runContextCommandById`.
 */
export interface MenuRunContextCommandByIdSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `menu.runContextCommandById`: `MenuRunContextCommandByIdSuccess`, or `ApiFailure` when the call failed.
 */
export type MenuRunContextCommandByIdResponse = MenuRunContextCommandByIdSuccess | ApiFailure;

/**
 * Successful response from `menu.runMainMenuCommand`.
 */
export interface MenuRunMainMenuCommandSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** GUID of the command that ran; absent when the menu tree ran it by name or path. */
    guid?: string;
    /** Whether it was a dynamic child command. */
    dynamic?: boolean;
    /** GUID of the dynamic child that ran. */
    subGuid?: string;
    /** How a name or path was resolved: `v2-tree` by the menu tree, `index` by the command list. Absent for the GUID form. */
    source?: "v2-tree" | "index";
}

/**
 * Response from `menu.runMainMenuCommand`: `MenuRunMainMenuCommandSuccess`, or `ApiFailure` when the call failed.
 */
export type MenuRunMainMenuCommandResponse = MenuRunMainMenuCommandSuccess | ApiFailure;

/**
 * Successful response from `menu.show`.
 */
export interface MenuShowSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the menu; `menu:select`, `menu:dismiss` and `menu:valueChanged` carry it. */
    menuId: string;
}

/**
 * Response from `menu.show`: `MenuShowSuccess`, or `ApiFailure` when the call failed.
 */
export type MenuShowResponse = MenuShowSuccess | ApiFailure;

/**
 * Successful response from `menu.showNativePopup`.
 */
export interface MenuShowNativePopupSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `menu.showNativePopup`: `MenuShowNativePopupSuccess`, or `ApiFailure` when the call failed.
 */
export type MenuShowNativePopupResponse = MenuShowNativePopupSuccess | ApiFailure;

/**
 * Successful response from `metadata.cancelProbe`.
 */
export interface MetadataCancelProbeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `true` when the probe was still running and has been told to stop; `false` when it had already finished or never existed, which are deliberately indistinguishable. */
    cancelled: boolean;
}

/**
 * Response from `metadata.cancelProbe`: `MetadataCancelProbeSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataCancelProbeResponse = MetadataCancelProbeSuccess | ApiFailure;

/**
 * Successful response from `metadata.embedArtwork`.
 */
export interface MetadataEmbedArtworkSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given. */
    path: string;
    /** The requested type, as given. */
    type: string;
    /** Picture size in bytes after decoding; present for a single target. */
    size?: number;
    /** Full path of the image file written; present for a single `file` target. */
    savedTo?: string;
    /** With both targets: each target's own envelope (`{ success, path, type, size }`, plus `savedTo` for `file`, or `{ success: false, error, code }`) under `embedded` and `file`. */
    results?: Record<string, JsonValue>;
}

/**
 * Response from `metadata.embedArtwork`: `MetadataEmbedArtworkSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataEmbedArtworkResponse = MetadataEmbedArtworkSuccess | ApiFailure;

/**
 * Successful response from `metadata.probeBatchAsync`.
 */
export interface MetadataProbeBatchAsyncSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the operation, starting with `probe_`. The events of the operation carry it, and `metadata.cancelProbe` takes it. */
    operationId: string;
    /** Number of paths accepted; the events report it as `total`. */
    totalCount: number;
}

/**
 * Response from `metadata.probeBatchAsync`: `MetadataProbeBatchAsyncSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataProbeBatchAsyncResponse = MetadataProbeBatchAsyncSuccess | ApiFailure;

/**
 * Successful response from `metadata.read`.
 */
export interface MetadataReadSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given, suffix included. */
    path: string;
    /** Every tag of the track, keyed by its name as the file spells it (case kept): a tag with one value is a string, one with several values a string array. */
    tags: Record<string, JsonValue>;
    /** Technical info of the track. */
    info: TrackTechnicalInfo;
}

/**
 * Response from `metadata.read`: `MetadataReadSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataReadResponse = MetadataReadSuccess | ApiFailure;

/**
 * Successful response from `metadata.readBatch`.
 */
export interface MetadataReadBatchSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of paths received. */
    total: number;
    /** Rows that were read. */
    successCount: number;
    /** Rows that failed. */
    errorCount: number;
    /** One row per path, in request order. */
    results: MetadataReadBatchItem[];
}

/**
 * Response from `metadata.readBatch`: `MetadataReadBatchSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataReadBatchResponse = MetadataReadBatchSuccess | ApiFailure;

/**
 * Successful response from `metadata.readByPath`.
 */
export interface MetadataReadByPathSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given, suffix included. */
    path: string;
    /** One tag or technical-info field of the track under its upper-case name. A tag with one value is a string and one with several values a string array; technical-info fields are strings. `DURATION` is always present: the length in seconds with three decimals, such as `215.400`. `FILESIZE` is the size in bytes and appears only when the host knows it, so a file the library has not indexed yet has none. `TRACKNUMBER`, when the file has no such tag, is taken from a leading number of up to three digits in the file name (`07 - Song.flac`, `(07) Song.flac`, `[07] Song.flac`) and is absent when there is none. */
    [key: string]: true | string | JsonValue;
}

/**
 * Response from `metadata.readByPath`: `MetadataReadByPathSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataReadByPathResponse = MetadataReadByPathSuccess | ApiFailure;

/**
 * Successful response from `metadata.readRaw`.
 */
export interface MetadataReadRawSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given, suffix included. */
    path: string;
    /** Every tag of the track, as in `metadata.read`. */
    tags: Record<string, JsonValue>;
    /** Technical info of the track. */
    info: TrackTechnicalInfo;
    /** Always `file`: the tags were read from the file itself, not from the host's cache. */
    source: string;
}

/**
 * Response from `metadata.readRaw`: `MetadataReadRawSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataReadRawResponse = MetadataReadRawSuccess | ApiFailure;

/**
 * Successful response from `metadata.removeEmbeddedArt`.
 */
export interface MetadataRemoveEmbeddedArtSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given. */
    path: string;
    /** What was removed: `["all"]` when every picture went in one step, otherwise the types removed one by one (`front`, `back`, `disc`, `icon`, `artist`), or the single requested `type` as given. */
    removedTypes: string[];
}

/**
 * Response from `metadata.removeEmbeddedArt`: `MetadataRemoveEmbeddedArtSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataRemoveEmbeddedArtResponse = MetadataRemoveEmbeddedArtSuccess | ApiFailure;

/**
 * Successful response from `metadata.removeField`.
 */
export interface MetadataRemoveFieldSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given. */
    path: string;
    /** The tag names queued for removal, upper-cased, in the order given. */
    removedTags: string[];
    /** Number of names in `removedTags`. */
    removedCount: number;
    /** `true` when the removal was dispatched; absent for an empty `tags`. */
    dispatched?: boolean;
    /** Subsong the removal targets; present when dispatched. */
    subsong?: number;
    /** A reminder that the outcome arrives as `metadata:writeComplete`; present when dispatched. */
    note?: string;
}

/**
 * Response from `metadata.removeField`: `MetadataRemoveFieldSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataRemoveFieldResponse = MetadataRemoveFieldSuccess | ApiFailure;

/**
 * Successful response from `metadata.removeTag`.
 */
export interface MetadataRemoveTagSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given. */
    path: string;
    /** The tag names queued for removal, upper-cased, in the order given. */
    removedTags: string[];
    /** Number of names in `removedTags`. */
    removedCount: number;
    /** `true` when the removal was dispatched; absent for an empty `tags`. */
    dispatched?: boolean;
    /** Subsong the removal targets; present when dispatched. */
    subsong?: number;
    /** A reminder that the outcome arrives as `metadata:writeComplete`; present when dispatched. */
    note?: string;
}

/**
 * Response from `metadata.removeTag`: `MetadataRemoveTagSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataRemoveTagResponse = MetadataRemoveTagSuccess | ApiFailure;

/**
 * Successful response from `metadata.write`.
 */
export interface MetadataWriteSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given. */
    path: string;
    /** `No tags to update` when nothing was left to write; otherwise a reminder that the outcome arrives as `metadata:writeComplete`. */
    note: string;
    /** `true` when the write was dispatched; absent when there was nothing to write. */
    dispatched?: boolean;
    /** Path of the handle the write targets, as foobar2000 stores it; present when dispatched. */
    handlePath?: string;
    /** Subsong the write targets; present when dispatched. */
    subsong?: number;
    /** Every tag queued, keyed by its upper-case name: the string, number or non-empty string array given for a write, `null` for a removal (including an empty array). Present when dispatched. */
    tagsApplied?: Record<string, JsonValue>;
    /** Number of tag fields set, not the number of values; present when dispatched. */
    tagsSet?: number;
    /** Number of tag fields removed, including those given as empty arrays; present when dispatched. */
    tagsRemoved?: number;
}

/**
 * Response from `metadata.write`: `MetadataWriteSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataWriteResponse = MetadataWriteSuccess | ApiFailure;

/**
 * Successful response from `metadata.writeBatch`.
 */
export interface MetadataWriteBatchSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Entries whose write was dispatched or had nothing to write; each dispatched write reports its outcome as `metadata:writeComplete`. */
    successCount: number;
    /** Entries that failed; always `0` on success, since a failed entry fails the call. */
    failCount: number;
    /** The failed entries; always empty on success, since a failed entry fails the call. */
    errors: MetadataWriteBatchError[];
}

/**
 * Response from `metadata.writeBatch`: `MetadataWriteBatchSuccess`, or `ApiFailure` when the call failed.
 */
export type MetadataWriteBatchResponse = MetadataWriteBatchSuccess | ApiFailure;

/**
 * Successful response from `misc.exit`.
 */
export interface MiscExitSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `misc.exit`: `MiscExitSuccess`, or `ApiFailure` when the call failed.
 */
export type MiscExitResponse = MiscExitSuccess | ApiFailure;

/**
 * Successful response from `misc.getComponentPath`.
 */
export interface MiscGetComponentPathSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The directory, as a native path. */
    path: string;
    /** The same directory; kept for themes that read `value`. */
    value: string;
}

/**
 * Response from `misc.getComponentPath`: `MiscGetComponentPathSuccess`, or `ApiFailure` when the call failed.
 */
export type MiscGetComponentPathResponse = MiscGetComponentPathSuccess | ApiFailure;

/**
 * Successful response from `misc.getFoobarPath`.
 */
export interface MiscGetFoobarPathSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The directory, as a native path. */
    path: string;
    /** The same directory; kept for themes that read `value`. */
    value: string;
}

/**
 * Response from `misc.getFoobarPath`: `MiscGetFoobarPathSuccess`, or `ApiFailure` when the call failed.
 */
export type MiscGetFoobarPathResponse = MiscGetFoobarPathSuccess | ApiFailure;

/**
 * Successful response from `misc.getProfilePath`.
 */
export interface MiscGetProfilePathSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The directory, as a native path. */
    path: string;
    /** The same directory; kept for themes that read `value`. */
    value: string;
}

/**
 * Response from `misc.getProfilePath`: `MiscGetProfilePathSuccess`, or `ApiFailure` when the call failed.
 */
export type MiscGetProfilePathResponse = MiscGetProfilePathSuccess | ApiFailure;

/**
 * Successful response from `misc.restart`.
 */
export interface MiscRestartSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `misc.restart`: `MiscRestartSuccess`, or `ApiFailure` when the call failed.
 */
export type MiscRestartResponse = MiscRestartSuccess | ApiFailure;

/**
 * Successful response from `misc.showConsole`.
 */
export interface MiscShowConsoleSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `misc.showConsole`: `MiscShowConsoleSuccess`, or `ApiFailure` when the call failed.
 */
export type MiscShowConsoleResponse = MiscShowConsoleSuccess | ApiFailure;

/**
 * Successful response from `misc.showLibrarySearch`.
 */
export interface MiscShowLibrarySearchSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The query the window opened with. */
    query: string;
}

/**
 * Response from `misc.showLibrarySearch`: `MiscShowLibrarySearchSuccess`, or `ApiFailure` when the call failed.
 */
export type MiscShowLibrarySearchResponse = MiscShowLibrarySearchSuccess | ApiFailure;

/**
 * Successful response from `misc.showPopupMessage`.
 */
export interface MiscShowPopupMessageSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `misc.showPopupMessage`: `MiscShowPopupMessageSuccess`, or `ApiFailure` when the call failed.
 */
export type MiscShowPopupMessageResponse = MiscShowPopupMessageSuccess | ApiFailure;

/**
 * Successful response from `misc.showPreferences`.
 */
export interface MiscShowPreferencesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `misc.showPreferences`: `MiscShowPreferencesSuccess`, or `ApiFailure` when the call failed.
 */
export type MiscShowPreferencesResponse = MiscShowPreferencesSuccess | ApiFailure;

/**
 * Successful response from `output.getDevices`.
 */
export interface OutputGetDevicesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Devices of all output modules, module by module. */
    devices: OutputDevice[];
    /** Number of entries in `devices`. */
    count: number;
}

/**
 * Response from `output.getDevices`: `OutputGetDevicesSuccess`, or `ApiFailure` when the call failed.
 */
export type OutputGetDevicesResponse = OutputGetDevicesSuccess | ApiFailure;

/**
 * Successful response from `output.getEntries`.
 */
export interface OutputGetEntriesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Every output module. */
    entries: OutputEntry[];
    /** Number of entries in `entries`. */
    count: number;
}

/**
 * Response from `output.getEntries`: `OutputGetEntriesSuccess`, or `ApiFailure` when the call failed.
 */
export type OutputGetEntriesResponse = OutputGetEntriesSuccess | ApiFailure;

/**
 * Successful response from `output.getSettings`.
 */
export interface OutputGetSettingsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Fixed English sentence saying where output settings are edited. */
    note: string;
    /** Display names of the output modules. Names can repeat or be empty, and the order is not stable between calls; prefer `output.getEntries`, which pairs each name with its GUID. */
    availableOutputs: string[];
}

/**
 * Response from `output.getSettings`: `OutputGetSettingsSuccess`, or `ApiFailure` when the call failed.
 */
export type OutputGetSettingsResponse = OutputGetSettingsSuccess | ApiFailure;

/**
 * Successful response from `panel.getConfig`.
 */
export interface PanelGetConfigSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The configuration. */
    config: PanelConfig;
}

/**
 * Response from `panel.getConfig`: `PanelGetConfigSuccess`, or `ApiFailure` when the call failed.
 */
export type PanelGetConfigResponse = PanelGetConfigSuccess | ApiFailure;

/**
 * Successful response from `panel.setConfig`.
 */
export interface PanelSetConfigSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether anything changed. */
    changed: boolean;
}

/**
 * Response from `panel.setConfig`: `PanelSetConfigSuccess`, or `ApiFailure` when the call failed.
 */
export type PanelSetConfigResponse = PanelSetConfigSuccess | ApiFailure;

/**
 * Successful response from `playback.getCurrentTrack`.
 */
export interface PlaybackGetCurrentTrackSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether a track is loaded. */
    found: boolean;
    /** The loaded track; absent when `found` is `false`. */
    track?: Track;
}

/**
 * Response from `playback.getCurrentTrack`: `PlaybackGetCurrentTrackSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackGetCurrentTrackResponse = PlaybackGetCurrentTrackSuccess | ApiFailure;

/**
 * Successful response from `playback.getCurrentTrackIndex`.
 */
export interface PlaybackGetCurrentTrackIndexSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the playing track has a playlist location. */
    found: boolean;
    /** Playlist index; `null` when there is no location. */
    playlist: number | null;
    /** GUID of that playlist, as `playlistGuid` takes it; `null` when there is no location. */
    playlistGuid: string | null;
    /** Row in that playlist; `null` when there is no location. */
    index: number | null;
    /** The track at that location; only with `includeTrackInfo`, and only when the row still exists. */
    track?: Track;
}

/**
 * Response from `playback.getCurrentTrackIndex`: `PlaybackGetCurrentTrackIndexSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackGetCurrentTrackIndexResponse = PlaybackGetCurrentTrackIndexSuccess | ApiFailure;

/**
 * Successful response from `playback.getPlaybackOrder`.
 */
export interface PlaybackGetPlaybackOrderSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the active order, `0` to `6`. */
    order: number;
    /** Name of the active order. */
    orderName: "default" | "repeat-playlist" | "repeat-track" | "random" | "shuffle-tracks" | "shuffle-albums" | "shuffle-folders";
    /** The same as `orderName`. */
    name: "default" | "repeat-playlist" | "repeat-track" | "random" | "shuffle-tracks" | "shuffle-albums" | "shuffle-folders";
    /** The same as `order`. */
    orderIndex: number;
}

/**
 * Response from `playback.getPlaybackOrder`: `PlaybackGetPlaybackOrderSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackGetPlaybackOrderResponse = PlaybackGetPlaybackOrderSuccess | ApiFailure;

/**
 * Successful response from `playback.getPlayingPlaylist`.
 */
export interface PlaybackGetPlayingPlaylistSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether a playing playlist is known. */
    found: boolean;
    /** Playlist index; `null` when none is known. */
    playlist: number | null;
    /** GUID of that playlist, as `playlistGuid` takes it; `null` when none is known. */
    playlistGuid: string | null;
    /** Name of that playlist; absent when none is known. */
    name?: string;
}

/**
 * Response from `playback.getPlayingPlaylist`: `PlaybackGetPlayingPlaylistSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackGetPlayingPlaylistResponse = PlaybackGetPlayingPlaylistSuccess | ApiFailure;

/**
 * Successful response from `playback.getPosition`.
 */
export interface PlaybackGetPositionSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Audible position in seconds from the start of the track, already adjusted for output-buffer and DSP latency; do not subtract that latency again. The host advances this reading in steps, typically 10–30 ms apart. It can briefly stall or jump after starting, seeking or resuming, and during crossfades; it is not a sample-accurate clock. */
    position: number;
    /** Length of the track in seconds; `0` when nothing is playing or the length is unknown. */
    duration: number;
    /** Subsong identifier of the track; `0` for a whole file. */
    subsong: number;
    /** Path as foobar2000 stores it, without a subsong suffix; empty when nothing is playing. */
    path: string;
    /** Host system time when `position` was read, Unix epoch milliseconds with a fractional part, the same clock as `Date.now()`. While playing and not paused, `position + (Date.now() - hostTime) / 1000` estimates the current position. */
    hostTime: number;
}

/**
 * Response from `playback.getPosition`: `PlaybackGetPositionSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackGetPositionResponse = PlaybackGetPositionSuccess | ApiFailure;

/**
 * Successful response from `playback.getState`.
 */
export interface PlaybackGetStateSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Transport state. */
    state: "stopped" | "playing" | "paused";
    /** Whether the current track can be seeked; `false` when stopped and for streams that do not support it. */
    canSeek: boolean;
    /** Whether the current track can be paused; always `true`. */
    canPause: boolean;
}

/**
 * Response from `playback.getState`: `PlaybackGetStateSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackGetStateResponse = PlaybackGetStateSuccess | ApiFailure;

/**
 * Successful response from `playback.getStopAfterCurrent`.
 */
export interface PlaybackGetStopAfterCurrentSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether playback stops after the current track. */
    enabled: boolean;
}

/**
 * Response from `playback.getStopAfterCurrent`: `PlaybackGetStopAfterCurrentSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackGetStopAfterCurrentResponse = PlaybackGetStopAfterCurrentSuccess | ApiFailure;

/**
 * Successful response from `playback.getVolume`.
 */
export interface PlaybackGetVolumeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Volume as a percentage from `0` to `100`, converted from the decibel value; `0` when muted. */
    volume: number;
    /** Volume in decibels, `-100` (silence) to `0` (full). */
    volumeDb: number;
    /** Whether output is muted; volume `0` counts as muted. */
    muted: boolean;
    /** The same as `muted`. */
    isMuted: boolean;
}

/**
 * Response from `playback.getVolume`: `PlaybackGetVolumeSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackGetVolumeResponse = PlaybackGetVolumeSuccess | ApiFailure;

/**
 * Successful response from `playback.mute`.
 */
export interface PlaybackMuteSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playback.mute`: `PlaybackMuteSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackMuteResponse = PlaybackMuteSuccess | ApiFailure;

/**
 * Successful response from `playback.next`.
 */
export interface PlaybackNextSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playback.next`: `PlaybackNextSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackNextResponse = PlaybackNextSuccess | ApiFailure;

/**
 * Successful response from `playback.pause`.
 */
export interface PlaybackPauseSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playback.pause`: `PlaybackPauseSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackPauseResponse = PlaybackPauseSuccess | ApiFailure;

/**
 * Successful response from `playback.play`.
 */
export interface PlaybackPlaySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playback.play`: `PlaybackPlaySuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackPlayResponse = PlaybackPlaySuccess | ApiFailure;

/**
 * Successful response from `playback.playOrPause`.
 */
export interface PlaybackPlayOrPauseSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether playback is running after the toggle. */
    isPlaying: boolean;
}

/**
 * Response from `playback.playOrPause`: `PlaybackPlayOrPauseSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackPlayOrPauseResponse = PlaybackPlayOrPauseSuccess | ApiFailure;

/**
 * Successful response from `playback.playPath`.
 */
export interface PlaybackPlayPathSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of playlist rows added; `1`. */
    tracksAdded: number;
    /** The file path without the subsong suffix. */
    path: string;
    /** Subsong identifier of the track that plays: the one asked for with `|subsong:N`, even when foobar2000 did not list it among the file's subsongs; without the suffix, the file's first. */
    subsong: number;
}

/**
 * Response from `playback.playPath`: `PlaybackPlayPathSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackPlayPathResponse = PlaybackPlayPathSuccess | ApiFailure;

/**
 * Successful response from `playback.playPaths`.
 */
export interface PlaybackPlayPathsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of paths the security check dropped before the call ran; present only when at least one was dropped. */
    skippedPaths?: number;
    /** Number of tracks added to the playlist. */
    tracksAdded: number;
    /** The `startIndex` given, echoed even when it was past the end and playback started at the first added track. */
    startedAt: number;
}

/**
 * Response from `playback.playPaths`: `PlaybackPlayPathsSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackPlayPathsResponse = PlaybackPlayPathsSuccess | ApiFailure;

/**
 * Successful response from `playback.playPause`.
 */
export interface PlaybackPlayPauseSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether playback is running after the toggle. */
    isPlaying: boolean;
}

/**
 * Response from `playback.playPause`: `PlaybackPlayPauseSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackPlayPauseResponse = PlaybackPlayPauseSuccess | ApiFailure;

/**
 * Successful response from `playback.previous`.
 */
export interface PlaybackPreviousSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playback.previous`: `PlaybackPreviousSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackPreviousResponse = PlaybackPreviousSuccess | ApiFailure;

/**
 * Successful response from `playback.random`.
 */
export interface PlaybackRandomSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playback.random`: `PlaybackRandomSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackRandomResponse = PlaybackRandomSuccess | ApiFailure;

/**
 * Successful response from `playback.setPlaybackOrder`.
 */
export interface PlaybackSetPlaybackOrderSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the order in effect after the call. */
    order: number;
    /** Name of the order in effect after the call. */
    orderName: "default" | "repeat-playlist" | "repeat-track" | "random" | "shuffle-tracks" | "shuffle-albums" | "shuffle-folders";
}

/**
 * Response from `playback.setPlaybackOrder`: `PlaybackSetPlaybackOrderSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackSetPlaybackOrderResponse = PlaybackSetPlaybackOrderSuccess | ApiFailure;

/**
 * Successful response from `playback.setPosition`.
 */
export interface PlaybackSetPositionSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The position asked for, before clamping. */
    requestedPosition: number;
    /** The position handed to the host after clamping to the track. */
    actualPosition: number;
    /** Position read before the seek, in seconds, with the same latency compensation as `playback.getPosition`. */
    oldPosition: number;
    /** Position read right after the seek, in seconds, with the same latency compensation as `playback.getPosition`; the engine may still be settling, so this can still be the old position. */
    newPosition: number;
    /** Host system time when `newPosition` was read, Unix epoch milliseconds with a fractional part, the same clock as `Date.now()`. */
    hostTime: number;
    /** Length of the track in seconds; `0` or less when the length is unknown. */
    duration: number;
    /** Subsong identifier of the track. */
    subsong: number;
}

/**
 * Response from `playback.setPosition`: `PlaybackSetPositionSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackSetPositionResponse = PlaybackSetPositionSuccess | ApiFailure;

/**
 * Successful response from `playback.setStopAfterCurrent`.
 */
export interface PlaybackSetStopAfterCurrentSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The value now in effect. */
    enabled: boolean;
}

/**
 * Response from `playback.setStopAfterCurrent`: `PlaybackSetStopAfterCurrentSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackSetStopAfterCurrentResponse = PlaybackSetStopAfterCurrentSuccess | ApiFailure;

/**
 * Successful response from `playback.setVolume`.
 */
export interface PlaybackSetVolumeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playback.setVolume`: `PlaybackSetVolumeSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackSetVolumeResponse = PlaybackSetVolumeSuccess | ApiFailure;

/**
 * Successful response from `playback.stop`.
 */
export interface PlaybackStopSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playback.stop`: `PlaybackStopSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackStopResponse = PlaybackStopSuccess | ApiFailure;

/**
 * Successful response from `playback.toggleMute`.
 */
export interface PlaybackToggleMuteSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether output is muted after the toggle. */
    muted: boolean;
}

/**
 * Response from `playback.toggleMute`: `PlaybackToggleMuteSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackToggleMuteResponse = PlaybackToggleMuteSuccess | ApiFailure;

/**
 * Successful response from `playback.toggleStopAfterCurrent`.
 */
export interface PlaybackToggleStopAfterCurrentSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The value after the toggle. */
    enabled: boolean;
}

/**
 * Response from `playback.toggleStopAfterCurrent`: `PlaybackToggleStopAfterCurrentSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackToggleStopAfterCurrentResponse = PlaybackToggleStopAfterCurrentSuccess | ApiFailure;

/**
 * Successful response from `playback.volumeDown`.
 */
export interface PlaybackVolumeDownSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playback.volumeDown`: `PlaybackVolumeDownSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackVolumeDownResponse = PlaybackVolumeDownSuccess | ApiFailure;

/**
 * Successful response from `playback.volumeUp`.
 */
export interface PlaybackVolumeUpSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playback.volumeUp`: `PlaybackVolumeUpSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaybackVolumeUpResponse = PlaybackVolumeUpSuccess | ApiFailure;

/**
 * Successful response from `playcount.get`.
 */
export interface PlaycountGetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of rows in `results`. */
    count: number;
    /** One row per requested path, in request order. */
    results: PlaycountRow[];
}

/**
 * Response from `playcount.get`: `PlaycountGetSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaycountGetResponse = PlaycountGetSuccess | ApiFailure;

/**
 * Successful response from `playcount.getBatch`.
 */
export interface PlaycountGetBatchSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of rows in `results`. */
    count: number;
    /** One row per requested path, in request order. */
    results: PlaycountRow[];
}

/**
 * Response from `playcount.getBatch`: `PlaycountGetBatchSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaycountGetBatchResponse = PlaycountGetBatchSuccess | ApiFailure;

/**
 * Successful response from `playcount.getStats`.
 */
export interface PlaycountGetStatsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Tracks in the media library. */
    totalTracks: number;
    /** Tracks played at least once. */
    playedTracks: number;
    /** Tracks never played. */
    unplayedTracks: number;
    /** Tracks with a rating. */
    ratedTracks: number;
    /** Sum of all play counts. */
    totalPlayCount: number;
    /** Highest play count of any track. */
    maxPlayCount: number;
    /** Mean play count of the played tracks; `0` when none was played. */
    averagePlayCount: number;
    /** Mean rating of the rated tracks; `0` when none is rated. */
    averageRating: number;
}

/**
 * Response from `playcount.getStats`: `PlaycountGetStatsSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaycountGetStatsResponse = PlaycountGetStatsSuccess | ApiFailure;

/**
 * Successful response from `playcount.set`.
 */
export interface PlaycountSetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playcount.set`: `PlaycountSetSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaycountSetResponse = PlaycountSetSuccess | ApiFailure;

/**
 * Successful response from `playlist.addHandles`.
 */
export interface PlaylistAddHandlesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Number of entries in `handles`. */
    requestedCount: number;
    /** Tracks appended. */
    addedCount: number;
    /** Entries that were not usable. */
    invalidCount: number;
    /** Number of tracks before the addition. */
    countBefore: number;
    /** Number of tracks after the addition. */
    totalCount: number;
}

/**
 * Response from `playlist.addHandles`: `PlaylistAddHandlesSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistAddHandlesResponse = PlaylistAddHandlesSuccess | ApiFailure;

/**
 * Successful response from `playlist.addPaths`.
 */
export interface PlaylistAddPathsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Number of entries in `paths`. */
    requestedPaths: number;
    /** Tracks added; a folder counts every track it holds. */
    addedCount: number;
    /** Entries that were not usable. */
    invalidCount: number;
    /** Number of tracks before the addition. */
    countBefore: number;
    /** Number of tracks after the addition. */
    totalCount: number;
}

/**
 * Response from `playlist.addPaths`: `PlaylistAddPathsSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistAddPathsResponse = PlaylistAddPathsSuccess | ApiFailure;

/**
 * Successful response from `playlist.addPathsAsync`.
 */
export interface PlaylistAddPathsAsyncSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the operation; `playlist:addComplete` carries the same id. */
    operationId: string;
    /** GUID of the playlist the paths go to; `playlist:addComplete` carries the same one. */
    playlistGuid: string;
    /** Always `pending`. */
    status: "pending";
    /** Entries that were accepted for processing. */
    totalCount: number;
    /** Entries refused before processing: empty or longer than 2048 characters. */
    invalidCount: number;
}

/**
 * Response from `playlist.addPathsAsync`: `PlaylistAddPathsAsyncSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistAddPathsAsyncResponse = PlaylistAddPathsAsyncSuccess | ApiFailure;

/**
 * Successful response from `playlist.addPathsSequential`.
 */
export interface PlaylistAddPathsSequentialSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Tracks added; `0` when nothing resolved, which still succeeds. */
    addedCount: number;
    /** Row of each added track, in the order they were added. */
    order: number[];
}

/**
 * Response from `playlist.addPathsSequential`: `PlaylistAddPathsSequentialSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistAddPathsSequentialResponse = PlaylistAddPathsSequentialSuccess | ApiFailure;

/**
 * Successful response from `playlist.clear`.
 */
export interface PlaylistClearSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Number of tracks it held before. */
    clearedCount: number;
    /** Number of tracks left; `0` on success. */
    remainingCount: number;
}

/**
 * Response from `playlist.clear`: `PlaylistClearSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistClearResponse = PlaylistClearSuccess | ApiFailure;

/**
 * Successful response from `playlist.convertToAutoplaylist`.
 */
export interface PlaylistConvertToAutoplaylistSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
}

/**
 * Response from `playlist.convertToAutoplaylist`: `PlaylistConvertToAutoplaylistSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistConvertToAutoplaylistResponse = PlaylistConvertToAutoplaylistSuccess | ApiFailure;

/**
 * Successful response from `playlist.create`.
 */
export interface PlaylistCreateSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the new playlist. */
    index: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    guid: string;
}

/**
 * Response from `playlist.create`: `PlaylistCreateSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistCreateResponse = PlaylistCreateSuccess | ApiFailure;

/**
 * Successful response from `playlist.createAutoplaylist`.
 */
export interface PlaylistCreateAutoplaylistSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the new playlist. */
    index: number;
    /** Index of the new playlist; the same as `index`. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    guid: string;
    /** Name of the new playlist. */
    name: string;
    /** The query, as given. */
    query: string;
}

/**
 * Response from `playlist.createAutoplaylist`: `PlaylistCreateAutoplaylistSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistCreateAutoplaylistResponse = PlaylistCreateAutoplaylistSuccess | ApiFailure;

/**
 * Successful response from `playlist.deselectAll`.
 */
export interface PlaylistDeselectAllSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.deselectAll`: `PlaylistDeselectAllSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistDeselectAllResponse = PlaylistDeselectAllSuccess | ApiFailure;

/**
 * Successful response from `playlist.duplicate`.
 */
export interface PlaylistDuplicateSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the copy. */
    index: number;
    /** Index of the playlist that was copied. */
    sourcePlaylist: number;
    /** GUID of the playlist that was copied, as `playlistGuid` takes it. */
    sourcePlaylistGuid: string;
    /** Index of the copy; the same as `index`. */
    newPlaylist: number;
    /** GUID of the copy, as `playlistGuid` takes it. */
    guid: string;
    /** Name of the copy. */
    name: string;
    /** Number of tracks copied. */
    trackCount: number;
}

/**
 * Response from `playlist.duplicate`: `PlaylistDuplicateSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistDuplicateResponse = PlaylistDuplicateSuccess | ApiFailure;

/**
 * Successful response from `playlist.focusTrack`.
 */
export interface PlaylistFocusTrackSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.focusTrack`: `PlaylistFocusTrackSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistFocusTrackResponse = PlaylistFocusTrackSuccess | ApiFailure;

/**
 * Successful response from `playlist.getActive`.
 */
export interface PlaylistGetActiveSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether there is an active playlist. */
    found: boolean;
    /** Index of the active playlist. */
    index?: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    guid?: string;
    /** Its name. */
    name?: string;
    /** Number of tracks. */
    trackCount?: number;
    /** Always `true` here. */
    isActive?: boolean;
    /** Whether it is also the playing playlist. */
    isPlaying?: boolean;
    /** Whether it carries a lock. */
    isLocked?: boolean;
    /** Total length of its tracks in seconds; tracks of unknown length count as `0`. */
    duration?: number;
}

/**
 * Response from `playlist.getActive`: `PlaylistGetActiveSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetActiveResponse = PlaylistGetActiveSuccess | ApiFailure;

/**
 * Successful response from `playlist.getAll`.
 */
export interface PlaylistGetAllSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Every playlist, in playlist order. */
    playlists: PlaylistInfo[];
    /** Number of entries in `playlists`. */
    count: number;
}

/**
 * Response from `playlist.getAll`: `PlaylistGetAllSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetAllResponse = PlaylistGetAllSuccess | ApiFailure;

/**
 * Successful response from `playlist.getAutoplaylistInfo`.
 */
export interface PlaylistGetAutoplaylistInfoSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the playlist is an autoplaylist; the remaining fields other than `playlist` are present only when it is. */
    isAutoplaylist: boolean;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Whether the autoplaylist keeps its tracks sorted; always `false` when `source` is `dui`. */
    keepSorted?: boolean;
    /** `sdk` for an autoplaylist foobar2000 runs, `dui` for one another component manages. */
    source?: "sdk" | "dui";
    /** Name of the playlist's lock, present when `source` is `dui`. */
    lockName?: string;
}

/**
 * Response from `playlist.getAutoplaylistInfo`: `PlaylistGetAutoplaylistInfoSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetAutoplaylistInfoResponse = PlaylistGetAutoplaylistInfoSuccess | ApiFailure;

/**
 * Successful response from `playlist.getAutoplaylistQuery`.
 */
export interface PlaylistGetAutoplaylistQuerySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the playlist is an autoplaylist; `keepSorted`, `source`, `note` and `lockName` are present only when it is. */
    isAutoplaylist: boolean;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Always `null`: foobar2000 does not expose the query. */
    query: string | null;
    /** Whether the autoplaylist keeps its tracks sorted; always `false` when `source` is `dui`. */
    keepSorted?: boolean;
    /** `sdk` for an autoplaylist foobar2000 runs, `dui` for one another component manages. */
    source?: "sdk" | "dui";
    /** Says that the query is not available. */
    note?: string;
    /** Name of the playlist's lock, present when `source` is `dui`. */
    lockName?: string;
}

/**
 * Response from `playlist.getAutoplaylistQuery`: `PlaylistGetAutoplaylistQuerySuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetAutoplaylistQueryResponse = PlaylistGetAutoplaylistQuerySuccess | ApiFailure;

/**
 * Successful response from `playlist.getAvailableColumns`.
 */
export interface PlaylistGetAvailableColumnsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Every column, component by component. */
    columns: PlaylistColumnDefinition[];
    /** Number of entries in `columns`. */
    count: number;
}

/**
 * Response from `playlist.getAvailableColumns`: `PlaylistGetAvailableColumnsSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetAvailableColumnsResponse = PlaylistGetAvailableColumnsSuccess | ApiFailure;

/**
 * Successful response from `playlist.getCount`.
 */
export interface PlaylistGetCountSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of playlists. */
    count: number;
}

/**
 * Response from `playlist.getCount`: `PlaylistGetCountSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetCountResponse = PlaylistGetCountSuccess | ApiFailure;

/**
 * Successful response from `playlist.getFocusedTrack`.
 */
export interface PlaylistGetFocusedTrackSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist; absent when there is no such playlist. */
    playlist?: number;
    /** GUID of the playlist, as `playlistGuid` takes it; absent when there is no such playlist. */
    playlistGuid?: string;
    /** Focused row; `-1` when the playlist has no focus or does not exist. */
    index: number;
}

/**
 * Response from `playlist.getFocusedTrack`: `PlaylistGetFocusedTrackSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetFocusedTrackResponse = PlaylistGetFocusedTrackSuccess | ApiFailure;

/**
 * Successful response from `playlist.getFocusTrack`.
 */
export interface PlaylistGetFocusTrackSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Focused row; `-1` when the playlist has no focus. */
    index: number;
}

/**
 * Response from `playlist.getFocusTrack`: `PlaylistGetFocusTrackSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetFocusTrackResponse = PlaylistGetFocusTrackSuccess | ApiFailure;

/**
 * Successful response from `playlist.getGroupRuns`.
 */
export interface PlaylistGetGroupRunsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The playlist grouped. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Rows in the playlist; the runs' `count` values add up to it. */
    total: number;
    /** The runs in playlist order, the first starting at row `0`; empty for an empty playlist. */
    runs: PlaylistGroupRun[];
}

/**
 * Response from `playlist.getGroupRuns`: `PlaylistGetGroupRunsSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetGroupRunsResponse = PlaylistGetGroupRunsSuccess | ApiFailure;

/**
 * Successful response from `playlist.getLockInfo`.
 */
export interface PlaylistGetLockInfoSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Whether it carries a lock. */
    isLocked: boolean;
}

/**
 * Response from `playlist.getLockInfo`: `PlaylistGetLockInfoSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetLockInfoResponse = PlaylistGetLockInfoSuccess | ApiFailure;

/**
 * Successful response from `playlist.getMatchingRows`.
 */
export interface PlaylistGetMatchingRowsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The playlist searched. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Rows in the playlist when the query ran. Equal to the `total` of a later `playlist.getTracks` does not mean the playlist is unchanged: reordering and tag edits keep the count. */
    total: number;
    /** The matching rows, ascending. */
    items: number[];
    /** Number of entries in `items`. */
    count: number;
}

/**
 * Response from `playlist.getMatchingRows`: `PlaylistGetMatchingRowsSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetMatchingRowsResponse = PlaylistGetMatchingRowsSuccess | ApiFailure;

/**
 * Successful response from `playlist.getPlaying`.
 */
export interface PlaylistGetPlayingSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether there is a playing playlist. */
    found: boolean;
    /** Index of the playing playlist. */
    index?: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    guid?: string;
    /** Its name. */
    name?: string;
    /** Number of tracks. */
    trackCount?: number;
    /** Whether it is also the active playlist. */
    isActive?: boolean;
    /** Always `true` here. */
    isPlaying?: boolean;
    /** Whether it carries a lock. */
    isLocked?: boolean;
    /** Total length of its tracks in seconds; tracks of unknown length count as `0`. */
    duration?: number;
}

/**
 * Response from `playlist.getPlaying`: `PlaylistGetPlayingSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetPlayingResponse = PlaylistGetPlayingSuccess | ApiFailure;

/**
 * Successful response from `playlist.getSelectedTracks`.
 */
export interface PlaylistGetSelectedTracksSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The playlist read. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** The selected rows in playlist order. */
    tracks: PlaylistTrack[];
    /** Rows returned. */
    count: number;
}

/**
 * Response from `playlist.getSelectedTracks`: `PlaylistGetSelectedTracksSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetSelectedTracksResponse = PlaylistGetSelectedTracksSuccess | ApiFailure;

/**
 * Successful response from `playlist.getSelection`.
 */
export interface PlaylistGetSelectionSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Selected rows, in playlist order. */
    items: number[];
    /** Number of entries in `items`. */
    count: number;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
}

/**
 * Response from `playlist.getSelection`: `PlaylistGetSelectionSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetSelectionResponse = PlaylistGetSelectionSuccess | ApiFailure;

/**
 * Successful response from `playlist.getTrackCount`.
 */
export interface PlaylistGetTrackCountSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of tracks. */
    count: number;
}

/**
 * Response from `playlist.getTrackCount`: `PlaylistGetTrackCountSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetTrackCountResponse = PlaylistGetTrackCountSuccess | ApiFailure;

/**
 * Successful response from `playlist.getTracks`.
 */
export interface PlaylistGetTracksSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The playlist read, as given or resolved. `-1` when there is none to name: `playlist` was omitted and there is no active playlist, or no playlist has the `playlistGuid`. An index past the last playlist comes back as given. */
    playlist: number;
    /** GUID of the playlist read, as `playlistGuid` takes it; absent when the playlist does not exist, so its absence is what tells a missing playlist from an empty one. */
    playlistGuid?: string;
    /** The `start` applied. */
    start: number;
    /** Rows returned. */
    count: number;
    /** Rows in the playlist; `0` when it does not exist. */
    total: number;
    /** The rows from `start`: whole rows, or `index` plus the `fields` asked for. */
    tracks: PlaylistTrackPartial[];
}

/**
 * Response from `playlist.getTracks`: `PlaylistGetTracksSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetTracksResponse = PlaylistGetTracksSuccess | ApiFailure;

/**
 * Successful response from `playlist.getTracksAt`.
 */
export interface PlaylistGetTracksAtSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The playlist read, as `playlist` in the result of `playlist.getTracks`. */
    playlist: number;
    /** GUID of the playlist read; absent when it does not exist, as in `playlist.getTracks`. */
    playlistGuid?: string;
    /** Rows in the playlist; `0` when it does not exist. */
    total: number;
    /** Rows returned. */
    count: number;
    /** The rows in the order asked for, skipping those past the last row. */
    tracks: PlaylistTrackPartial[];
}

/**
 * Response from `playlist.getTracksAt`: `PlaylistGetTracksAtSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistGetTracksAtResponse = PlaylistGetTracksAtSuccess | ApiFailure;

/**
 * Successful response from `playlist.insertTracks`.
 */
export interface PlaylistInsertTracksSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** The `position` that was asked for, before it was limited to the end of the playlist. */
    insertIndex: number;
    /** Number of entries in `handles`. */
    requestedCount: number;
    /** Tracks inserted. */
    addedCount: number;
    /** Entries that were not usable. */
    invalidCount: number;
    /** Number of tracks before the insertion. */
    countBefore: number;
    /** Number of tracks after the insertion. */
    totalCount: number;
}

/**
 * Response from `playlist.insertTracks`: `PlaylistInsertTracksSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistInsertTracksResponse = PlaylistInsertTracksSuccess | ApiFailure;

/**
 * Successful response from `playlist.isAutoplaylist`.
 */
export interface PlaylistIsAutoplaylistSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Whether it is an autoplaylist. */
    isAutoplaylist: boolean;
    /** Name of the playlist's lock, present when the playlist carries a named lock and is not an autoplaylist foobar2000 runs, whether or not the lock makes it count as one. */
    lockName?: string;
}

/**
 * Response from `playlist.isAutoplaylist`: `PlaylistIsAutoplaylistSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistIsAutoplaylistResponse = PlaylistIsAutoplaylistSuccess | ApiFailure;

/**
 * Successful response from `playlist.isLocked`.
 */
export interface PlaylistIsLockedSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the playlist carries a lock. */
    isLocked: boolean;
}

/**
 * Response from `playlist.isLocked`: `PlaylistIsLockedSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistIsLockedResponse = PlaylistIsLockedSuccess | ApiFailure;

/**
 * Successful response from `playlist.moveTracks`.
 */
export interface PlaylistMoveTracksSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.moveTracks`: `PlaylistMoveTracksSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistMoveTracksResponse = PlaylistMoveTracksSuccess | ApiFailure;

/**
 * Successful response from `playlist.playTrack`.
 */
export interface PlaylistPlayTrackSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.playTrack`: `PlaylistPlayTrackSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistPlayTrackResponse = PlaylistPlayTrackSuccess | ApiFailure;

/**
 * Successful response from `playlist.redo`.
 */
export interface PlaylistRedoSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.redo`: `PlaylistRedoSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistRedoResponse = PlaylistRedoSuccess | ApiFailure;

/**
 * Successful response from `playlist.remove`.
 */
export interface PlaylistRemoveSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.remove`: `PlaylistRemoveSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistRemoveResponse = PlaylistRemoveSuccess | ApiFailure;

/**
 * Successful response from `playlist.removeAutoplaylist`.
 */
export interface PlaylistRemoveAutoplaylistSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** `sdk` when foobar2000's autoplaylist was released, `dui` when the playlist is an autoplaylist another component manages and nothing changed. */
    source: "sdk" | "dui";
    /** Explanation, present when `source` is `dui`. */
    note?: string;
}

/**
 * Response from `playlist.removeAutoplaylist`: `PlaylistRemoveAutoplaylistSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistRemoveAutoplaylistResponse = PlaylistRemoveAutoplaylistSuccess | ApiFailure;

/**
 * Successful response from `playlist.removeSelectedTracks`.
 */
export interface PlaylistRemoveSelectedTracksSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.removeSelectedTracks`: `PlaylistRemoveSelectedTracksSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistRemoveSelectedTracksResponse = PlaylistRemoveSelectedTracksSuccess | ApiFailure;

/**
 * Successful response from `playlist.removeTracks`.
 */
export interface PlaylistRemoveTracksSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.removeTracks`: `PlaylistRemoveTracksSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistRemoveTracksResponse = PlaylistRemoveTracksSuccess | ApiFailure;

/**
 * Successful response from `playlist.rename`.
 */
export interface PlaylistRenameSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.rename`: `PlaylistRenameSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistRenameResponse = PlaylistRenameSuccess | ApiFailure;

/**
 * Successful response from `playlist.reorder`.
 */
export interface PlaylistReorderSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Number of tracks reordered. */
    itemCount: number;
}

/**
 * Response from `playlist.reorder`: `PlaylistReorderSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistReorderResponse = PlaylistReorderSuccess | ApiFailure;

/**
 * Successful response from `playlist.reorderPlaylists`.
 */
export interface PlaylistReorderPlaylistsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of playlists. */
    count: number;
}

/**
 * Response from `playlist.reorderPlaylists`: `PlaylistReorderPlaylistsSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistReorderPlaylistsResponse = PlaylistReorderPlaylistsSuccess | ApiFailure;

/**
 * Successful response from `playlist.replaceAllAndPlay`.
 */
export interface PlaylistReplaceAllAndPlaySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Index of the playlist. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Number of tracks removed. */
    clearedCount: number;
    /** Tracks added. */
    addedCount: number;
    /** Number of tracks afterwards. */
    totalCount: number;
    /** Row that was played or focused. */
    playIndex: number;
}

/**
 * Response from `playlist.replaceAllAndPlay`: `PlaylistReplaceAllAndPlaySuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistReplaceAllAndPlayResponse = PlaylistReplaceAllAndPlaySuccess | ApiFailure;

/**
 * Successful response from `playlist.reverse`.
 */
export interface PlaylistReverseSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.reverse`: `PlaylistReverseSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistReverseResponse = PlaylistReverseSuccess | ApiFailure;

/**
 * Successful response from `playlist.selectAll`.
 */
export interface PlaylistSelectAllSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.selectAll`: `PlaylistSelectAllSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistSelectAllResponse = PlaylistSelectAllSuccess | ApiFailure;

/**
 * Successful response from `playlist.setActive`.
 */
export interface PlaylistSetActiveSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.setActive`: `PlaylistSetActiveSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistSetActiveResponse = PlaylistSetActiveSuccess | ApiFailure;

/**
 * Successful response from `playlist.setFocusedTrack`.
 */
export interface PlaylistSetFocusedTrackSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.setFocusedTrack`: `PlaylistSetFocusedTrackSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistSetFocusedTrackResponse = PlaylistSetFocusedTrackSuccess | ApiFailure;

/**
 * Successful response from `playlist.setSelection`.
 */
export interface PlaylistSetSelectionSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.setSelection`: `PlaylistSetSelectionSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistSetSelectionResponse = PlaylistSetSelectionSuccess | ApiFailure;

/**
 * Successful response from `playlist.shuffle`.
 */
export interface PlaylistShuffleSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.shuffle`: `PlaylistShuffleSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistShuffleResponse = PlaylistShuffleSuccess | ApiFailure;

/**
 * Successful response from `playlist.sort`.
 */
export interface PlaylistSortSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.sort`: `PlaylistSortSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistSortResponse = PlaylistSortSuccess | ApiFailure;

/**
 * Successful response from `playlist.undo`.
 */
export interface PlaylistUndoSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `playlist.undo`: `PlaylistUndoSuccess`, or `ApiFailure` when the call failed.
 */
export type PlaylistUndoResponse = PlaylistUndoSuccess | ApiFailure;

/**
 * Successful response from `port.connect`.
 */
export interface PortConnectSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the new port; the other port methods take it. */
    portId: string;
    /** The channel name. */
    name: string;
    /** Id of the calling window, which the port belongs to. */
    windowId: string;
}

/**
 * Response from `port.connect`: `PortConnectSuccess`, or `ApiFailure` when the call failed.
 */
export type PortConnectResponse = PortConnectSuccess | ApiFailure;

/**
 * Successful response from `port.disconnect`.
 */
export interface PortDisconnectSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `port.disconnect`: `PortDisconnectSuccess`, or `ApiFailure` when the call failed.
 */
export type PortDisconnectResponse = PortDisconnectSuccess | ApiFailure;

/**
 * Successful response from `port.getPorts`.
 */
export interface PortGetPortsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The ports, in no particular order. */
    ports: PortInfo[];
}

/**
 * Response from `port.getPorts`: `PortGetPortsSuccess`, or `ApiFailure` when the call failed.
 */
export type PortGetPortsResponse = PortGetPortsSuccess | ApiFailure;

/**
 * Successful response from `port.postMessage`.
 */
export interface PortPostMessageSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** How many of the other ports' windows took the event. */
    recipients: number;
}

/**
 * Response from `port.postMessage`: `PortPostMessageSuccess`, or `ApiFailure` when the call failed.
 */
export type PortPostMessageResponse = PortPostMessageSuccess | ApiFailure;

/**
 * Successful response from `port.postMessageTo`.
 */
export interface PortPostMessageToSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `port.postMessageTo`: `PortPostMessageToSuccess`, or `ApiFailure` when the call failed.
 */
export type PortPostMessageToResponse = PortPostMessageToSuccess | ApiFailure;

/**
 * Successful response from `queue.add`.
 */
export interface QueueAddSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** How many rows were queued. */
    addedCount: number;
    /** Queue length afterwards. */
    queueCount: number;
}

/**
 * Response from `queue.add`: `QueueAddSuccess`, or `ApiFailure` when the call failed.
 */
export type QueueAddResponse = QueueAddSuccess | ApiFailure;

/**
 * Successful response from `queue.addPaths`.
 */
export interface QueueAddPathsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** How many tracks were appended and queued. */
    addedCount: number;
    /** Entries of `paths` dropped: empty ones, ones longer than 2048 characters, a `|subsong:N` no track could be made for, and the plain paths when none of them resolved. A plain path that resolves to nothing while another plain path of the call resolves is not counted, so a call that succeeds can have dropped more than this. */
    invalidCount: number;
    /** The playlist the tracks were appended to. */
    playlist: number;
    /** The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. */
    playlistGuid: string;
    /** Queue length afterwards. */
    queueCount: number;
}

/**
 * Response from `queue.addPaths`: `QueueAddPathsSuccess`, or `ApiFailure` when the call failed.
 */
export type QueueAddPathsResponse = QueueAddPathsSuccess | ApiFailure;

/**
 * Successful response from `queue.clear`.
 */
export interface QueueClearSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** How many entries the queue held. */
    clearedCount: number;
}

/**
 * Response from `queue.clear`: `QueueClearSuccess`, or `ApiFailure` when the call failed.
 */
export type QueueClearResponse = QueueClearSuccess | ApiFailure;

/**
 * Successful response from `queue.flush`.
 */
export interface QueueFlushSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** How many entries the queue held. */
    clearedCount: number;
}

/**
 * Response from `queue.flush`: `QueueFlushSuccess`, or `ApiFailure` when the call failed.
 */
export type QueueFlushResponse = QueueFlushSuccess | ApiFailure;

/**
 * Successful response from `queue.get`.
 */
export interface QueueGetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The entries, in play order. */
    items: QueueItem[];
    /** Number of entries. */
    count: number;
}

/**
 * Response from `queue.get`: `QueueGetSuccess`, or `ApiFailure` when the call failed.
 */
export type QueueGetResponse = QueueGetSuccess | ApiFailure;

/**
 * Successful response from `queue.getCount`.
 */
export interface QueueGetCountSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of entries. */
    count: number;
    /** Whether the queue holds anything; the same as `count > 0`. */
    hasItems: boolean;
}

/**
 * Response from `queue.getCount`: `QueueGetCountSuccess`, or `ApiFailure` when the call failed.
 */
export type QueueGetCountResponse = QueueGetCountSuccess | ApiFailure;

/**
 * Successful response from `queue.insertNext`.
 */
export interface QueueInsertNextSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Tracks newly queued. */
    insertedCount: number;
    /** Entries that were already queued and moved. */
    movedCount: number;
    /** Queue length afterwards. */
    queueCount: number;
    /** Input path count minus the tracks resolved from paths, floored at zero. A folder or playlist file resolves to several tracks, so this is not a count of the paths that failed. */
    invalidCount: number;
}

/**
 * Response from `queue.insertNext`: `QueueInsertNextSuccess`, or `ApiFailure` when the call failed.
 */
export type QueueInsertNextResponse = QueueInsertNextSuccess | ApiFailure;

/**
 * Successful response from `queue.moveToTop`.
 */
export interface QueueMoveToTopSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The position the entry came from. */
    movedIndex: number;
    /** Queue length afterwards. */
    queueCount: number;
}

/**
 * Response from `queue.moveToTop`: `QueueMoveToTopSuccess`, or `ApiFailure` when the call failed.
 */
export type QueueMoveToTopResponse = QueueMoveToTopSuccess | ApiFailure;

/**
 * Successful response from `queue.playNow`.
 */
export interface QueuePlayNowSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The position that was played. */
    playedIndex: number;
    /** Queue length read right after playback started; the host may consume the played entry before or after that read. */
    queueCount: number;
}

/**
 * Response from `queue.playNow`: `QueuePlayNowSuccess`, or `ApiFailure` when the call failed.
 */
export type QueuePlayNowResponse = QueuePlayNowSuccess | ApiFailure;

/**
 * Successful response from `queue.remove`.
 */
export interface QueueRemoveSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The position removed; only when `index` was given. */
    removedIndex?: number;
    /** How many entries were removed; only when `indices` was given. */
    removedCount?: number;
    /** Queue length afterwards. */
    queueCount: number;
}

/**
 * Response from `queue.remove`: `QueueRemoveSuccess`, or `ApiFailure` when the call failed.
 */
export type QueueRemoveResponse = QueueRemoveSuccess | ApiFailure;

/**
 * Successful response from `queue.setContents`.
 */
export interface QueueSetContentsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Queue length afterwards. */
    queueCount: number;
}

/**
 * Response from `queue.setContents`: `QueueSetContentsSuccess`, or `ApiFailure` when the call failed.
 */
export type QueueSetContentsResponse = QueueSetContentsSuccess | ApiFailure;

/**
 * Successful response from `rating.get`.
 */
export interface RatingGetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given, suffix included. */
    path: string;
    /** Rating from 0 to 5; `0` means unrated. */
    rating: number;
    /** `stats` for a foo_playcount rating; `file` for the `RATING` tag, and also when neither source has a rating. */
    storage: "stats" | "file";
}

/**
 * Response from `rating.get`: `RatingGetSuccess`, or `ApiFailure` when the call failed.
 */
export type RatingGetResponse = RatingGetSuccess | ApiFailure;

/**
 * Successful response from `rating.set`.
 */
export interface RatingSetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path as given; `(current)` when `path` was omitted and foo_playcount took the rating. */
    path: string;
    /** The rating that was set. */
    rating: number;
    /** `stats` when foo_playcount's menu ran; `file` when the `RATING` tag write was dispatched. */
    storage: "stats" | "file";
    /** The context-menu path that ran; present when `storage` is `stats`. */
    menuPath?: string;
    /** Why the tag was written instead; present when `storage` is `file`. */
    note?: string;
}

/**
 * Response from `rating.set`: `RatingSetSuccess`, or `ApiFailure` when the call failed.
 */
export type RatingSetResponse = RatingSetSuccess | ApiFailure;

/**
 * Successful response from `replaygain.clear`.
 */
export interface ReplaygainClearSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of files handed to the tag writer. */
    clearedCount: number;
}

/**
 * Response from `replaygain.clear`: `ReplaygainClearSuccess`, or `ApiFailure` when the call failed.
 */
export type ReplaygainClearResponse = ReplaygainClearSuccess | ApiFailure;

/**
 * Successful response from `replaygain.get`.
 */
export interface ReplaygainGetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of rows in `results`. */
    count: number;
    /** One row per requested path, in request order. */
    results: ReplayGainTrackInfo[];
}

/**
 * Response from `replaygain.get`: `ReplaygainGetSuccess`, or `ApiFailure` when the call failed.
 */
export type ReplaygainGetResponse = ReplaygainGetSuccess | ApiFailure;

/**
 * Successful response from `replaygain.getMode`.
 */
export interface ReplaygainGetModeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Where the gain applied during playback comes from: `none` applies no gain, `track` and `album` use the respective stored value, `auto` follows the playback order (track gain in shuffle, album gain otherwise). */
    sourceMode: "none" | "track" | "album" | "auto";
    /** What is applied: `gain` the gain only, `peak` peak limiting only, `gain_and_peak` both, `none` nothing. */
    processingMode: "none" | "gain" | "gain_and_peak" | "peak";
}

/**
 * Response from `replaygain.getMode`: `ReplaygainGetModeSuccess`, or `ApiFailure` when the call failed.
 */
export type ReplaygainGetModeResponse = ReplaygainGetModeSuccess | ApiFailure;

/**
 * Successful response from `replaygain.getPreamp`.
 */
export interface ReplaygainGetPreampSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Preamp applied to tracks that carry ReplayGain, in dB. */
    withRg: number;
    /** Preamp applied to tracks without ReplayGain, in dB. */
    withoutRg: number;
}

/**
 * Response from `replaygain.getPreamp`: `ReplaygainGetPreampSuccess`, or `ApiFailure` when the call failed.
 */
export type ReplaygainGetPreampResponse = ReplaygainGetPreampSuccess | ApiFailure;

/**
 * Successful response from `replaygain.getSettings`.
 */
export interface ReplaygainGetSettingsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Where the gain applied during playback comes from: `none` applies no gain, `track` and `album` use the respective stored value, `auto` follows the playback order (track gain in shuffle, album gain otherwise). */
    sourceMode: "none" | "track" | "album" | "auto";
    /** What is applied: `gain` the gain only, `peak` peak limiting only, `gain_and_peak` both, `none` nothing. */
    processingMode: "none" | "gain" | "gain_and_peak" | "peak";
    /** Preamp applied to tracks that carry ReplayGain, in dB. */
    preampWithRg: number;
    /** Preamp applied to tracks without ReplayGain, in dB. */
    preampWithoutRg: number;
    /** Whether ReplayGain changes the output: `false` when the source mode is `none` or `auto`, or the processing mode is `none`. */
    active: boolean;
}

/**
 * Response from `replaygain.getSettings`: `ReplaygainGetSettingsSuccess`, or `ApiFailure` when the call failed.
 */
export type ReplaygainGetSettingsResponse = ReplaygainGetSettingsSuccess | ApiFailure;

/**
 * Successful response from `replaygain.scan`.
 */
export interface ReplaygainScanSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of files handed to the scanner. */
    scannedCount: number;
    /** The mode the scan runs in. */
    mode: "track" | "album";
    /** A fixed English note saying the scan runs in the background. */
    note: string;
}

/**
 * Response from `replaygain.scan`: `ReplaygainScanSuccess`, or `ApiFailure` when the call failed.
 */
export type ReplaygainScanResponse = ReplaygainScanSuccess | ApiFailure;

/**
 * Successful response from `replaygain.setMode`.
 */
export interface ReplaygainSetModeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Where the gain applied during playback comes from: `none` applies no gain, `track` and `album` use the respective stored value, `auto` follows the playback order (track gain in shuffle, album gain otherwise). */
    sourceMode: "none" | "track" | "album" | "auto";
    /** What is applied: `gain` the gain only, `peak` peak limiting only, `gain_and_peak` both, `none` nothing. */
    processingMode: "none" | "gain" | "gain_and_peak" | "peak";
    /** Whether the call changed anything. */
    changed: boolean;
}

/**
 * Response from `replaygain.setMode`: `ReplaygainSetModeSuccess`, or `ApiFailure` when the call failed.
 */
export type ReplaygainSetModeResponse = ReplaygainSetModeSuccess | ApiFailure;

/**
 * Successful response from `replaygain.setPreamp`.
 */
export interface ReplaygainSetPreampSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Preamp applied to tracks that carry ReplayGain, in dB. */
    withRg: number;
    /** Preamp applied to tracks without ReplayGain, in dB. */
    withoutRg: number;
    /** Whether the call changed anything. */
    changed: boolean;
}

/**
 * Response from `replaygain.setPreamp`: `ReplaygainSetPreampSuccess`, or `ApiFailure` when the call failed.
 */
export type ReplaygainSetPreampResponse = ReplaygainSetPreampSuccess | ApiFailure;

/**
 * Successful response from `selection.get`.
 */
export interface SelectionGetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Size of the whole selection, not of this page. */
    count: number;
    /** Where the selection comes from. */
    type: "now_playing" | "active_playlist_selection" | "active_playlist" | "playlist_manager" | "media_library_viewer" | "unknown";
    /** Handles of this page: native paths with `|subsong:N` appended when the subsong is not `0`. */
    handles: string[];
    /** The `offset` that applied. */
    offset: number;
    /** Whether entries remain after this page. */
    hasMore: boolean;
    /** Present, and true, only when `limit` was omitted and the selection exceeds 100. */
    truncated?: boolean;
}

/**
 * Response from `selection.get`: `SelectionGetSuccess`, or `ApiFailure` when the call failed.
 */
export type SelectionGetResponse = SelectionGetSuccess | ApiFailure;

/**
 * Successful response from `selection.getType`.
 */
export interface SelectionGetTypeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Numeric index of the type: `0` now_playing, `1` active_playlist_selection, `2` active_playlist, `3` playlist_manager, `5` media_library_viewer, `0` when unknown. */
    type: number;
    /** Name of the type. */
    typeName: "now_playing" | "active_playlist_selection" | "active_playlist" | "playlist_manager" | "media_library_viewer" | "unknown";
}

/**
 * Response from `selection.getType`: `SelectionGetTypeSuccess`, or `ApiFailure` when the call failed.
 */
export type SelectionGetTypeResponse = SelectionGetTypeSuccess | ApiFailure;

/**
 * Successful response from `selection.getViewerMode`.
 */
export interface SelectionGetViewerModeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `prefer_playing` when the selection type is the playing track, else `prefer_selection`. */
    mode: "prefer_playing" | "prefer_selection";
}

/**
 * Response from `selection.getViewerMode`: `SelectionGetViewerModeSuccess`, or `ApiFailure` when the call failed.
 */
export type SelectionGetViewerModeResponse = SelectionGetViewerModeSuccess | ApiFailure;

/**
 * Successful response from `selection.getViewingTrack`.
 */
export interface SelectionGetViewingTrackSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether either source had a track. */
    found: boolean;
    /** The preference the resolution used. */
    mode: "prefer_playing" | "prefer_selection";
    /** Which source supplied the track; absent when `found` is false. */
    source?: "now_playing" | "selection";
    /** The track's handle (native path, `|subsong:N` appended when the subsong is not `0`); absent when `found` is false. */
    handle?: string;
    /** Playlist holding the track; absent when no playlist row is known for it. For the playing track this is where playback took it from; for the selection only the active playlist is looked through, so a selection made elsewhere (the Media Library, another playlist) has no row. Present exactly when `itemIndex` is. */
    playlistIndex?: number;
    /** GUID of that playlist, as `playlistGuid` takes it; present exactly when `playlistIndex` is. */
    playlistGuid?: string;
    /** Row of the track in that playlist, the first one holding it; absent when `playlistIndex` is. */
    itemIndex?: number;
    /** The full track row; only with `includeTrackInfo`. */
    track?: Track;
}

/**
 * Response from `selection.getViewingTrack`: `SelectionGetViewingTrackSuccess`, or `ApiFailure` when the call failed.
 */
export type SelectionGetViewingTrackResponse = SelectionGetViewingTrackSuccess | ApiFailure;

/**
 * Successful response from `selection.set`.
 */
export interface SelectionSetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** How many tracks were handed to the selection holder, i.e. the handles that resolved. Not a read-back: see `set` for how long the selection lasts. */
    count: number;
}

/**
 * Response from `selection.set`: `SelectionSetSuccess`, or `ApiFailure` when the call failed.
 */
export type SelectionSetResponse = SelectionSetSuccess | ApiFailure;

/**
 * Successful response from `selection.setPlaylistTracking`.
 */
export interface SelectionSetPlaylistTrackingSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The mode that applied. */
    mode: "selection" | "playlist";
}

/**
 * Response from `selection.setPlaylistTracking`: `SelectionSetPlaylistTrackingSuccess`, or `ApiFailure` when the call failed.
 */
export type SelectionSetPlaylistTrackingResponse = SelectionSetPlaylistTrackingSuccess | ApiFailure;

/**
 * Successful response from `shell.exec`.
 */
export interface ShellExecSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Windows process id of the started process. */
    processId: number;
}

/**
 * Response from `shell.exec`: `ShellExecSuccess`, or `ApiFailure` when the call failed.
 */
export type ShellExecResponse = ShellExecSuccess | ApiFailure;

/**
 * Successful response from `shell.openExternal`.
 */
export interface ShellOpenExternalSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `shell.openExternal`: `ShellOpenExternalSuccess`, or `ApiFailure` when the call failed.
 */
export type ShellOpenExternalResponse = ShellOpenExternalSuccess | ApiFailure;

/**
 * Successful response from `shell.openWith`.
 */
export interface ShellOpenWithSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `shell.openWith`: `ShellOpenWithSuccess`, or `ApiFailure` when the call failed.
 */
export type ShellOpenWithResponse = ShellOpenWithSuccess | ApiFailure;

/**
 * Successful response from `shell.showInExplorer`.
 */
export interface ShellShowInExplorerSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `shell.showInExplorer`: `ShellShowInExplorerSuccess`, or `ApiFailure` when the call failed.
 */
export type ShellShowInExplorerResponse = ShellShowInExplorerSuccess | ApiFailure;

/**
 * Successful response from `shell.spawn`.
 */
export interface ShellSpawnSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Windows process id of the started process. */
    processId: number;
    /** Whether the process exited within `waitForExitMs`; absent when no wait was requested. */
    exited?: boolean;
    /** The exit code, present when `exited` is `true`. */
    exitCode?: number;
}

/**
 * Response from `shell.spawn`: `ShellSpawnSuccess`, or `ApiFailure` when the call failed.
 */
export type ShellSpawnResponse = ShellSpawnSuccess | ApiFailure;

/**
 * Successful response from `state.delete`.
 */
export interface StateDeleteSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the key existed before the call. */
    existed: boolean;
}

/**
 * Response from `state.delete`: `StateDeleteSuccess`, or `ApiFailure` when the call failed.
 */
export type StateDeleteResponse = StateDeleteSuccess | ApiFailure;

/**
 * Successful response from `state.get`.
 */
export interface StateGetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the key exists. */
    exists: boolean;
    /** The value; `null` when the key does not exist. */
    value: JsonValue;
    /** The key; present when it exists. */
    key?: string;
    /** When the value expires, in milliseconds since the Unix epoch; present for a value stored with `ttlMs`. */
    expiresAt?: number;
}

/**
 * Response from `state.get`: `StateGetSuccess`, or `ApiFailure` when the call failed.
 */
export type StateGetResponse = StateGetSuccess | ApiFailure;

/**
 * Successful response from `state.keys`.
 */
export interface StateKeysSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The matching keys, in no particular order. */
    keys: string[];
}

/**
 * Response from `state.keys`: `StateKeysSuccess`, or `ApiFailure` when the call failed.
 */
export type StateKeysResponse = StateKeysSuccess | ApiFailure;

/**
 * Successful response from `state.set`.
 */
export interface StateSetSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** When the value expires, in milliseconds since the Unix epoch; present when `ttlMs` is positive. */
    expiresAt?: number;
}

/**
 * Response from `state.set`: `StateSetSuccess`, or `ApiFailure` when the call failed.
 */
export type StateSetResponse = StateSetSuccess | ApiFailure;

/**
 * Successful response from `system.getApisByNamespace`.
 */
export interface SystemGetApisByNamespaceSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The namespace's methods, in the order of `system.listAvailableApis`; empty for an unknown namespace. */
    apis: SystemApiInfo[];
}

/**
 * Response from `system.getApisByNamespace`: `SystemGetApisByNamespaceSuccess`, or `ApiFailure` when the call failed.
 */
export type SystemGetApisByNamespaceResponse = SystemGetApisByNamespaceSuccess | ApiFailure;

/**
 * Successful response from `system.getApiStats`.
 */
export interface SystemGetApiStatsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of registered methods. */
    totalApis: number;
    /** Number of built-in methods. */
    internalApis: number;
    /** Number of methods registered by external plugins. */
    externalApis: number;
    /** Number of registered external plugins. */
    pluginCount: number;
    /** Method count per namespace. */
    byNamespace: Record<string, number>;
}

/**
 * Response from `system.getApiStats`: `SystemGetApiStatsSuccess`, or `ApiFailure` when the call failed.
 */
export type SystemGetApiStatsResponse = SystemGetApiStatsSuccess | ApiFailure;

/**
 * Successful response from `system.getDPI`.
 */
export interface SystemGetDPISuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** DPI of the display; `96` when no window can be resolved. */
    dpi: number;
    /** `dpi / 96`. */
    scale: number;
}

/**
 * Response from `system.getDPI`: `SystemGetDPISuccess`, or `ApiFailure` when the call failed.
 */
export type SystemGetDPIResponse = SystemGetDPISuccess | ApiFailure;

/**
 * Successful response from `system.getLocale`.
 */
export interface SystemGetLocaleSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Locale tag such as `en-US`, the default when Windows reports none. */
    locale: string;
    /** Localized language name; empty when unavailable. */
    language: string;
    /** Localized country or region name; empty when unavailable. */
    country: string;
}

/**
 * Response from `system.getLocale`: `SystemGetLocaleSuccess`, or `ApiFailure` when the call failed.
 */
export type SystemGetLocaleResponse = SystemGetLocaleSuccess | ApiFailure;

/**
 * Successful response from `system.getRegisteredPlugins`.
 */
export interface SystemGetRegisteredPluginsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The plugins. */
    plugins: SystemPluginInfo[];
}

/**
 * Response from `system.getRegisteredPlugins`: `SystemGetRegisteredPluginsSuccess`, or `ApiFailure` when the call failed.
 */
export type SystemGetRegisteredPluginsResponse = SystemGetRegisteredPluginsSuccess | ApiFailure;

/**
 * Successful response from `system.getTheme`.
 */
export interface SystemGetThemeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `true` when Windows uses the dark app theme. */
    darkMode: boolean;
    /** Same as `darkMode`; kept for callers that read this name. */
    isDark: boolean;
    /** Accent color as `#RRGGBB`; `#0078D4` when DWM cannot report one. */
    accentColor: string;
    /** Whether transparency effects are enabled. */
    transparency: boolean;
}

/**
 * Response from `system.getTheme`: `SystemGetThemeSuccess`, or `ApiFailure` when the call failed.
 */
export type SystemGetThemeResponse = SystemGetThemeSuccess | ApiFailure;

/**
 * Successful response from `system.isPluginRegistered`.
 */
export interface SystemIsPluginRegisteredSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether an external plugin owns the namespace. */
    registered: boolean;
}

/**
 * Response from `system.isPluginRegistered`: `SystemIsPluginRegisteredSuccess`, or `ApiFailure` when the call failed.
 */
export type SystemIsPluginRegisteredResponse = SystemIsPluginRegisteredSuccess | ApiFailure;

/**
 * Successful response from `system.listAvailableApis`.
 */
export interface SystemListAvailableApisSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The methods, sorted by `fullName` in ascending byte order. */
    apis: SystemApiInfo[];
}

/**
 * Response from `system.listAvailableApis`: `SystemListAvailableApisSuccess`, or `ApiFailure` when the call failed.
 */
export type SystemListAvailableApisResponse = SystemListAvailableApisSuccess | ApiFailure;

/**
 * Successful response from `system.searchApis`.
 */
export interface SystemSearchApisSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The matching methods, in the order of `system.listAvailableApis`. */
    apis: SystemApiInfo[];
}

/**
 * Response from `system.searchApis`: `SystemSearchApisSuccess`, or `ApiFailure` when the call failed.
 */
export type SystemSearchApisResponse = SystemSearchApisSuccess | ApiFailure;

/**
 * Successful response from `taskbar.flash`.
 */
export interface TaskbarFlashSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `taskbar.flash`: `TaskbarFlashSuccess`, or `ApiFailure` when the call failed.
 */
export type TaskbarFlashResponse = TaskbarFlashSuccess | ApiFailure;

/**
 * Successful response from `taskbar.setOverlayIcon`.
 */
export interface TaskbarSetOverlayIconSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `taskbar.setOverlayIcon`: `TaskbarSetOverlayIconSuccess`, or `ApiFailure` when the call failed.
 */
export type TaskbarSetOverlayIconResponse = TaskbarSetOverlayIconSuccess | ApiFailure;

/**
 * Successful response from `taskbar.setProgress`.
 */
export interface TaskbarSetProgressSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `taskbar.setProgress`: `TaskbarSetProgressSuccess`, or `ApiFailure` when the call failed.
 */
export type TaskbarSetProgressResponse = TaskbarSetProgressSuccess | ApiFailure;

/**
 * Successful response from `taskbar.setThumbnailButtons`.
 */
export interface TaskbarSetThumbnailButtonsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `taskbar.setThumbnailButtons`: `TaskbarSetThumbnailButtonsSuccess`, or `ApiFailure` when the call failed.
 */
export type TaskbarSetThumbnailButtonsResponse = TaskbarSetThumbnailButtonsSuccess | ApiFailure;

/**
 * Successful response from `taskbar.updateButton`.
 */
export interface TaskbarUpdateButtonSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `taskbar.updateButton`: `TaskbarUpdateButtonSuccess`, or `ApiFailure` when the call failed.
 */
export type TaskbarUpdateButtonResponse = TaskbarUpdateButtonSuccess | ApiFailure;

/**
 * Successful response from `titleformat.eval`.
 */
export interface TitleformatEvalSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path that was evaluated; the playing track's path when the request gave none. */
    path: string;
    /** The pattern that was evaluated. */
    pattern: string;
    /** Formatted text. */
    result: string;
    /** `false` when the host could not get the track's info (a remote path with nothing cached, or a file that cannot be read), so tag-derived output is untrustworthy. A local file that foobar2000 has not loaded is read from disk and reports `true`. Does not cover foo_playcount fields such as `%rating%`. */
    infoAvailable: boolean;
}

/**
 * Response from `titleformat.eval`: `TitleformatEvalSuccess`, or `ApiFailure` when the call failed.
 */
export type TitleformatEvalResponse = TitleformatEvalSuccess | ApiFailure;

/**
 * Successful response from `titleformat.evalBatch`.
 */
export interface TitleformatEvalBatchSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The pattern that was evaluated. */
    pattern: string;
    /** Number of paths received. */
    total: number;
    /** Rows that evaluated. */
    successCount: number;
    /** Rows that failed. */
    errorCount: number;
    /** One row per path, in request order. */
    results: EvalBatchRow[];
}

/**
 * Response from `titleformat.evalBatch`: `TitleformatEvalBatchSuccess`, or `ApiFailure` when the call failed.
 */
export type TitleformatEvalBatchResponse = TitleformatEvalBatchSuccess | ApiFailure;

/**
 * Successful response from `titleformat.evalFields`.
 */
export interface TitleformatEvalFieldsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The path that was evaluated. */
    path: string;
    /** `false` under the same conditions as in `titleformat.eval`, so tag-derived values are untrustworthy. One flag covers the whole request. Absent when `fields` is empty. */
    infoAvailable?: boolean;
    /** One entry per key of `fields`, holding the formatted text. Keys named `success`, `path` or `infoAvailable` are dropped because the response fields of those names take precedence. Keys named `error` or `code` are kept, so check `success` to tell a failure. */
    [key: string]: true | string | boolean | undefined;
}

/**
 * Response from `titleformat.evalFields`: `TitleformatEvalFieldsSuccess`, or `ApiFailure` when the call failed.
 */
export type TitleformatEvalFieldsResponse = TitleformatEvalFieldsSuccess | ApiFailure;

/**
 * Successful response from `titleformat.evalFieldsBatch`.
 */
export interface TitleformatEvalFieldsBatchSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of paths received, or 0 when `fields` is empty. */
    total: number;
    /** Rows that evaluated. */
    successCount: number;
    /** Rows that failed. */
    errorCount: number;
    /** One row per path, in request order. */
    results: Array<EvalFieldsBatchRow & Record<string, string>>;
}

/**
 * Response from `titleformat.evalFieldsBatch`: `TitleformatEvalFieldsBatchSuccess`, or `ApiFailure` when the call failed.
 */
export type TitleformatEvalFieldsBatchResponse = TitleformatEvalFieldsBatchSuccess | ApiFailure;

/**
 * Successful response from `titleformat.getBuiltinFields`.
 */
export interface TitleformatGetBuiltinFieldsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Map from readable name to pattern, such as `"year": "$year(%date%)"`. Entries under playcount need the foo_playcount component. */
    fields: Record<string, string>;
}

/**
 * Response from `titleformat.getBuiltinFields`: `TitleformatGetBuiltinFieldsSuccess`, or `ApiFailure` when the call failed.
 */
export type TitleformatGetBuiltinFieldsResponse = TitleformatGetBuiltinFieldsSuccess | ApiFailure;

/**
 * Successful response from `tray.appendMenuItems`.
 */
export interface TrayAppendMenuItemsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `tray.appendMenuItems`: `TrayAppendMenuItemsSuccess`, or `ApiFailure` when the call failed.
 */
export type TrayAppendMenuItemsResponse = TrayAppendMenuItemsSuccess | ApiFailure;

/**
 * Successful response from `tray.clearMenuItems`.
 */
export interface TrayClearMenuItemsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `tray.clearMenuItems`: `TrayClearMenuItemsSuccess`, or `ApiFailure` when the call failed.
 */
export type TrayClearMenuItemsResponse = TrayClearMenuItemsSuccess | ApiFailure;

/**
 * Successful response from `tray.create`.
 */
export interface TrayCreateSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `tray.create`: `TrayCreateSuccess`, or `ApiFailure` when the call failed.
 */
export type TrayCreateResponse = TrayCreateSuccess | ApiFailure;

/**
 * Successful response from `tray.destroy`.
 */
export interface TrayDestroySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `tray.destroy`: `TrayDestroySuccess`, or `ApiFailure` when the call failed.
 */
export type TrayDestroyResponse = TrayDestroySuccess | ApiFailure;

/**
 * Successful response from `tray.getMenuItems`.
 */
export interface TrayGetMenuItemsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The stored user rows. */
    items: TrayMenuItem[];
}

/**
 * Response from `tray.getMenuItems`: `TrayGetMenuItemsSuccess`, or `ApiFailure` when the call failed.
 */
export type TrayGetMenuItemsResponse = TrayGetMenuItemsSuccess | ApiFailure;

/**
 * Successful response from `tray.isVisible`.
 */
export interface TrayIsVisibleSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the icon exists. */
    visible: boolean;
}

/**
 * Response from `tray.isVisible`: `TrayIsVisibleSuccess`, or `ApiFailure` when the call failed.
 */
export type TrayIsVisibleResponse = TrayIsVisibleSuccess | ApiFailure;

/**
 * Successful response from `tray.removeMenuItems`.
 */
export interface TrayRemoveMenuItemsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Number of rows removed; less than the number of ids when some did not exist. */
    removed: number;
}

/**
 * Response from `tray.removeMenuItems`: `TrayRemoveMenuItemsSuccess`, or `ApiFailure` when the call failed.
 */
export type TrayRemoveMenuItemsResponse = TrayRemoveMenuItemsSuccess | ApiFailure;

/**
 * Successful response from `tray.setCloseToTray`.
 */
export interface TraySetCloseToTraySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `tray.setCloseToTray`: `TraySetCloseToTraySuccess`, or `ApiFailure` when the call failed.
 */
export type TraySetCloseToTrayResponse = TraySetCloseToTraySuccess | ApiFailure;

/**
 * Successful response from `tray.setContextMenu`.
 */
export interface TraySetContextMenuSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `tray.setContextMenu`: `TraySetContextMenuSuccess`, or `ApiFailure` when the call failed.
 */
export type TraySetContextMenuResponse = TraySetContextMenuSuccess | ApiFailure;

/**
 * Successful response from `tray.setIcon`.
 */
export interface TraySetIconSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `tray.setIcon`: `TraySetIconSuccess`, or `ApiFailure` when the call failed.
 */
export type TraySetIconResponse = TraySetIconSuccess | ApiFailure;

/**
 * Successful response from `tray.setMenuItemState`.
 */
export interface TraySetMenuItemStateSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether a row with the id existed; `true` on success. */
    found: boolean;
}

/**
 * Response from `tray.setMenuItemState`: `TraySetMenuItemStateSuccess`, or `ApiFailure` when the call failed.
 */
export type TraySetMenuItemStateResponse = TraySetMenuItemStateSuccess | ApiFailure;

/**
 * Successful response from `tray.setMenuZones`.
 */
export interface TraySetMenuZonesSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `tray.setMenuZones`: `TraySetMenuZonesSuccess`, or `ApiFailure` when the call failed.
 */
export type TraySetMenuZonesResponse = TraySetMenuZonesSuccess | ApiFailure;

/**
 * Successful response from `tray.setMinimizeToTray`.
 */
export interface TraySetMinimizeToTraySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `tray.setMinimizeToTray`: `TraySetMinimizeToTraySuccess`, or `ApiFailure` when the call failed.
 */
export type TraySetMinimizeToTrayResponse = TraySetMinimizeToTraySuccess | ApiFailure;

/**
 * Successful response from `tray.setTooltip`.
 */
export interface TraySetTooltipSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `tray.setTooltip`: `TraySetTooltipSuccess`, or `ApiFailure` when the call failed.
 */
export type TraySetTooltipResponse = TraySetTooltipSuccess | ApiFailure;

/**
 * Successful response from `tray.showBalloon`.
 */
export interface TrayShowBalloonSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `tray.showBalloon`: `TrayShowBalloonSuccess`, or `ApiFailure` when the call failed.
 */
export type TrayShowBalloonResponse = TrayShowBalloonSuccess | ApiFailure;

/**
 * Successful response from `ui.hideNotification`.
 */
export interface UiHideNotificationSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `ui.hideNotification`: `UiHideNotificationSuccess`, or `ApiFailure` when the call failed.
 */
export type UiHideNotificationResponse = UiHideNotificationSuccess | ApiFailure;

/**
 * Successful response from `ui.showContextMenu`.
 */
export interface UiShowContextMenuSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `ui.showContextMenu`: `UiShowContextMenuSuccess`, or `ApiFailure` when the call failed.
 */
export type UiShowContextMenuResponse = UiShowContextMenuSuccess | ApiFailure;

/**
 * Successful response from `ui.showCustomMenu`.
 */
export interface UiShowCustomMenuSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the chosen row; `null` when the menu was dismissed. */
    selectedId: string | null;
}

/**
 * Response from `ui.showCustomMenu`: `UiShowCustomMenuSuccess`, or `ApiFailure` when the call failed.
 */
export type UiShowCustomMenuResponse = UiShowCustomMenuSuccess | ApiFailure;

/**
 * Successful response from `ui.showNotification`.
 */
export interface UiShowNotificationSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Sequence number of the notification, from `1`. */
    id: number;
}

/**
 * Response from `ui.showNotification`: `UiShowNotificationSuccess`, or `ApiFailure` when the call failed.
 */
export type UiShowNotificationResponse = UiShowNotificationSuccess | ApiFailure;

/**
 * Successful response from `ui.showToast`.
 */
export interface UiShowToastSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `ui.showToast`: `UiShowToastSuccess`, or `ApiFailure` when the call failed.
 */
export type UiShowToastResponse = UiShowToastSuccess | ApiFailure;

/**
 * Successful response from `webview.getSource`.
 */
export interface WebviewGetSourceSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Where the page came from. `devServer`: the development server. `url`: a URL from the panel's configuration, or an absolute URL passed to `window.createPopup`. `panelTemplate`: the panel's own template. `activeTemplate`: the global active template. `componentDirectory`: `foo_ui_webview2_resources\dist` in the component's folder. `defaultTemplate`: `webview-ui\default`, reached because the folders before it had no `index.html`. `builtInPage`: the page shown when no folder had an `index.html`. */
    source: "devServer" | "url" | "panelTemplate" | "activeTemplate" | "componentDirectory" | "defaultTemplate" | "builtInPage";
    /** Folder mapped to `https://foo-ui-webview2.local/`; present for the four folder sources. */
    directory?: string;
    /** Name of the template folder that was loaded; present for `panelTemplate`, `activeTemplate` and `defaultTemplate`. */
    templateName?: string;
    /** Address the host navigated to; present for `devServer` and `url`. */
    url?: string;
    /** The global active template as configured now, which can differ from the one loaded. */
    activeTemplateName: string;
    /** Folder that holds the templates, `webview-ui` under the foobar2000 profile. */
    templatesDirectory: string;
}

/**
 * Response from `webview.getSource`: `WebviewGetSourceSuccess`, or `ApiFailure` when the call failed.
 */
export type WebviewGetSourceResponse = WebviewGetSourceSuccess | ApiFailure;

/**
 * Successful response from `window.blur`.
 */
export interface WindowBlurSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.blur`: `WindowBlurSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowBlurResponse = WindowBlurSuccess | ApiFailure;

/**
 * Successful response from `window.broadcast`.
 */
export interface WindowBroadcastSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.broadcast`: `WindowBroadcastSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowBroadcastResponse = WindowBroadcastSuccess | ApiFailure;

/**
 * Successful response from `window.cancelClose`.
 */
export interface WindowCancelCloseSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.cancelClose`: `WindowCancelCloseSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowCancelCloseResponse = WindowCancelCloseSuccess | ApiFailure;

/**
 * Successful response from `window.center`.
 */
export interface WindowCenterSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.center`: `WindowCenterSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowCenterResponse = WindowCenterSuccess | ApiFailure;

/**
 * Successful response from `window.clearClickThroughExcludeRegions`.
 */
export interface WindowClearClickThroughExcludeRegionsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the popup. */
    windowId: string;
}

/**
 * Response from `window.clearClickThroughExcludeRegions`: `WindowClearClickThroughExcludeRegionsSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowClearClickThroughExcludeRegionsResponse = WindowClearClickThroughExcludeRegionsSuccess | ApiFailure;

/**
 * Successful response from `window.clearDragRegions`.
 */
export interface WindowClearDragRegionsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.clearDragRegions`: `WindowClearDragRegionsSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowClearDragRegionsResponse = WindowClearDragRegionsSuccess | ApiFailure;

/**
 * Successful response from `window.clearNoDragRegions`.
 */
export interface WindowClearNoDragRegionsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.clearNoDragRegions`: `WindowClearNoDragRegionsSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowClearNoDragRegionsResponse = WindowClearNoDragRegionsSuccess | ApiFailure;

/**
 * Successful response from `window.close`.
 */
export interface WindowCloseSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.close`: `WindowCloseSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowCloseResponse = WindowCloseSuccess | ApiFailure;

/**
 * Successful response from `window.closeAllPopups`.
 */
export interface WindowCloseAllPopupsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.closeAllPopups`: `WindowCloseAllPopupsSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowCloseAllPopupsResponse = WindowCloseAllPopupsSuccess | ApiFailure;

/**
 * Successful response from `window.closePopup`.
 */
export interface WindowClosePopupSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.closePopup`: `WindowClosePopupSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowClosePopupResponse = WindowClosePopupSuccess | ApiFailure;

/**
 * Successful response from `window.confirmClose`.
 */
export interface WindowConfirmCloseSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.confirmClose`: `WindowConfirmCloseSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowConfirmCloseResponse = WindowConfirmCloseSuccess | ApiFailure;

/**
 * Successful response from `window.createPopup`.
 */
export interface WindowCreatePopupSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the new popup. */
    windowId: string;
}

/**
 * Response from `window.createPopup`: `WindowCreatePopupSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowCreatePopupResponse = WindowCreatePopupSuccess | ApiFailure;

/**
 * Successful response from `window.enterFullscreen`.
 */
export interface WindowEnterFullscreenSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Always `true`. */
    isFullscreen: boolean;
}

/**
 * Response from `window.enterFullscreen`: `WindowEnterFullscreenSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowEnterFullscreenResponse = WindowEnterFullscreenSuccess | ApiFailure;

/**
 * Successful response from `window.exitFullscreen`.
 */
export interface WindowExitFullscreenSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Always `false`. */
    isFullscreen: boolean;
}

/**
 * Response from `window.exitFullscreen`: `WindowExitFullscreenSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowExitFullscreenResponse = WindowExitFullscreenSuccess | ApiFailure;

/**
 * Successful response from `window.flash`.
 */
export interface WindowFlashSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.flash`: `WindowFlashSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowFlashResponse = WindowFlashSuccess | ApiFailure;

/**
 * Successful response from `window.flashTaskbar`.
 */
export interface WindowFlashTaskbarSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.flashTaskbar`: `WindowFlashTaskbarSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowFlashTaskbarResponse = WindowFlashTaskbarSuccess | ApiFailure;

/**
 * Successful response from `window.focus`.
 */
export interface WindowFocusSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.focus`: `WindowFocusSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowFocusResponse = WindowFocusSuccess | ApiFailure;

/**
 * Successful response from `window.getAllWindows`.
 */
export interface WindowGetAllWindowsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The main window first, then the popups. */
    items: WindowInfo[];
}

/**
 * Response from `window.getAllWindows`: `WindowGetAllWindowsSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetAllWindowsResponse = WindowGetAllWindowsSuccess | ApiFailure;

/**
 * Successful response from `window.getBackdropPolicy`.
 */
export interface WindowGetBackdropPolicySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the window the call acted on. */
    windowId: string;
    /** The backdrop policy overrides set on the window. */
    backdropPolicy: Record<string, JsonValue>;
    /** The backdrop policy in effect. */
    resolvedBackdropPolicy: WindowBackdropPolicyState;
}

/**
 * Response from `window.getBackdropPolicy`: `WindowGetBackdropPolicySuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetBackdropPolicyResponse = WindowGetBackdropPolicySuccess | ApiFailure;

/**
 * Successful response from `window.getBounds`.
 */
export interface WindowGetBoundsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Left edge in screen coordinates. */
    x: number;
    /** Top edge in screen coordinates. */
    y: number;
    /** Width in physical pixels, frame included. */
    width: number;
    /** Height in physical pixels, frame included. */
    height: number;
}

/**
 * Response from `window.getBounds`: `WindowGetBoundsSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetBoundsResponse = WindowGetBoundsSuccess | ApiFailure;

/**
 * Successful response from `window.getCaptionButtonsWidth`.
 */
export interface WindowGetCaptionButtonsWidthSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Width of the three buttons together. */
    width: number;
    /** Width of one button. */
    buttonWidth: number;
}

/**
 * Response from `window.getCaptionButtonsWidth`: `WindowGetCaptionButtonsWidthSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetCaptionButtonsWidthResponse = WindowGetCaptionButtonsWidthSuccess | ApiFailure;

/**
 * Successful response from `window.getCornerPreference`.
 */
export interface WindowGetCornerPreferenceSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The corner rounding as last set. */
    mode: string;
    /** Same value as `mode`. */
    preference: string;
}

/**
 * Response from `window.getCornerPreference`: `WindowGetCornerPreferenceSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetCornerPreferenceResponse = WindowGetCornerPreferenceSuccess | ApiFailure;

/**
 * Successful response from `window.getCurrentWindowId`.
 */
export interface WindowGetCurrentWindowIdSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the calling window. */
    windowId: string;
}

/**
 * Response from `window.getCurrentWindowId`: `WindowGetCurrentWindowIdSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetCurrentWindowIdResponse = WindowGetCurrentWindowIdSuccess | ApiFailure;

/**
 * Successful response from `window.getDevServerConfig`.
 */
export interface WindowGetDevServerConfigSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether pages load from the development server. */
    useDevServer: boolean;
    /** Address of the development server. */
    devServerUrl: string;
}

/**
 * Response from `window.getDevServerConfig`: `WindowGetDevServerConfigSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetDevServerConfigResponse = WindowGetDevServerConfigSuccess | ApiFailure;

/**
 * Successful response from `window.getDpiScale`.
 */
export interface WindowGetDpiScaleSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** DPI; `96` is 100 %. */
    dpi: number;
    /** `dpi / 96`. */
    scale: number;
}

/**
 * Response from `window.getDpiScale`: `WindowGetDpiScaleSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetDpiScaleResponse = WindowGetDpiScaleSuccess | ApiFailure;

/**
 * Successful response from `window.getMaxSize`.
 */
export interface WindowGetMaxSizeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Maximum width in physical pixels at the window's current DPI; `0` means no bound. */
    width: number;
    /** Maximum height in physical pixels at the window's current DPI; `0` means no bound. */
    height: number;
    /** Id of the window the call acted on. */
    windowId: string;
}

/**
 * Response from `window.getMaxSize`: `WindowGetMaxSizeSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetMaxSizeResponse = WindowGetMaxSizeSuccess | ApiFailure;

/**
 * Successful response from `window.getMinSize`.
 */
export interface WindowGetMinSizeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Minimum width in physical pixels at the window's current DPI. */
    width: number;
    /** Minimum height in physical pixels at the window's current DPI. */
    height: number;
    /** Id of the window the call acted on. */
    windowId: string;
}

/**
 * Response from `window.getMinSize`: `WindowGetMinSizeSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetMinSizeResponse = WindowGetMinSizeSuccess | ApiFailure;

/**
 * Successful response from `window.getMode`.
 */
export interface WindowGetModeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** `standalone`, `dui` or `cui`; `panel` when the page is in a panel that could not be told apart, `unknown` for a panel of an unrecognized kind. */
    mode: "standalone" | "dui" | "cui" | "panel" | "unknown";
    /** Whether the page is in a DUI or CUI panel; `false` for the main window and popups. */
    panelMode: boolean;
    /** Id of the calling window: `main` for the main window, the popup's actual id, or the panel's id. A panel without an id reports `panel`; an unmatched caller reports `main`. */
    windowId: string;
}

/**
 * Response from `window.getMode`: `WindowGetModeSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetModeResponse = WindowGetModeSuccess | ApiFailure;

/**
 * Successful response from `window.getPopupBehavior`.
 */
export interface WindowGetPopupBehaviorSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the popup. */
    windowId: string;
    /** The behavior preset; `legacy` when the popup was created without one. */
    profile: "legacy" | "standard" | "miniPlayer" | "desktopLyrics";
    /** The behavior overrides set on the popup. */
    behavior: Record<string, JsonValue>;
    /** The behavior in effect. */
    resolvedBehavior: WindowPopupBehaviorState;
}

/**
 * Response from `window.getPopupBehavior`: `WindowGetPopupBehaviorSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetPopupBehaviorResponse = WindowGetPopupBehaviorSuccess | ApiFailure;

/**
 * Successful response from `window.getState`.
 */
export interface WindowGetStateSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the window is maximized. */
    maximized: boolean;
    /** Whether the window is minimized. */
    minimized: boolean;
    /** Whether the window is fullscreen. */
    fullscreen: boolean;
    /** Whether the window is kept above other windows. */
    alwaysOnTop: boolean;
    /** Whether the window is the foreground window. */
    focused: boolean;
    /** Same value as `maximized`. */
    isMaximized: boolean;
    /** Same value as `minimized`. */
    isMinimized: boolean;
    /** Same value as `fullscreen`. */
    isFullscreen: boolean;
    /** Same value as `alwaysOnTop`. */
    isAlwaysOnTop: boolean;
    /** Same value as `focused`. */
    isFocused: boolean;
    /** Width in physical pixels, frame included. */
    width: number;
    /** Height in physical pixels, frame included. */
    height: number;
    /** Left edge in screen coordinates. */
    x: number;
    /** Top edge in screen coordinates. */
    y: number;
}

/**
 * Response from `window.getState`: `WindowGetStateSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetStateResponse = WindowGetStateSuccess | ApiFailure;

/**
 * Successful response from `window.getTitle`.
 */
export interface WindowGetTitleSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The window title. */
    title: string;
}

/**
 * Response from `window.getTitle`: `WindowGetTitleSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetTitleResponse = WindowGetTitleSuccess | ApiFailure;

/**
 * Successful response from `window.getTitlebarHeight`.
 */
export interface WindowGetTitlebarHeightSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Title bar height in physical pixels. */
    height: number;
}

/**
 * Response from `window.getTitlebarHeight`: `WindowGetTitlebarHeightSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetTitlebarHeightResponse = WindowGetTitlebarHeightSuccess | ApiFailure;

/**
 * Successful response from `window.getTitlebarInfo`.
 */
export interface WindowGetTitlebarInfoSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Title bar height in physical pixels. */
    height: number;
    /** Width of the three caption buttons together. */
    captionButtonsWidth: number;
    /** Width of one caption button. */
    captionButtonWidth: number;
    /** Whether the main window is maximized. */
    isMaximized: boolean;
}

/**
 * Response from `window.getTitlebarInfo`: `WindowGetTitlebarInfoSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetTitlebarInfoResponse = WindowGetTitlebarInfoSuccess | ApiFailure;

/**
 * Successful response from `window.getZoom`.
 */
export interface WindowGetZoomSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The zoom factor; `1` is 100 %. */
    zoom: number;
    /** The window's DPI; left out without a WebView. */
    dpi?: number;
    /** `dpi / 96`; left out without a WebView. */
    dpiScale?: number;
}

/**
 * Response from `window.getZoom`: `WindowGetZoomSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowGetZoomResponse = WindowGetZoomSuccess | ApiFailure;

/**
 * Successful response from `window.hasSavedBounds`.
 */
export interface WindowHasSavedBoundsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether a position from an earlier session is saved. */
    hasSavedBounds: boolean;
    /** An English sentence restating `hasSavedBounds`, for logs. */
    description: string;
}

/**
 * Response from `window.hasSavedBounds`: `WindowHasSavedBoundsSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowHasSavedBoundsResponse = WindowHasSavedBoundsSuccess | ApiFailure;

/**
 * Successful response from `window.isAlwaysOnTop`.
 */
export interface WindowIsAlwaysOnTopSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the window is kept above other windows. */
    enabled: boolean;
    /** Same value as `enabled`. */
    isAlwaysOnTop: boolean;
}

/**
 * Response from `window.isAlwaysOnTop`: `WindowIsAlwaysOnTopSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowIsAlwaysOnTopResponse = WindowIsAlwaysOnTopSuccess | ApiFailure;

/**
 * Successful response from `window.isClickThrough`.
 */
export interface WindowIsClickThroughSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether mouse input passes through. */
    clickThrough: boolean;
}

/**
 * Response from `window.isClickThrough`: `WindowIsClickThroughSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowIsClickThroughResponse = WindowIsClickThroughSuccess | ApiFailure;

/**
 * Successful response from `window.isFullscreen`.
 */
export interface WindowIsFullscreenSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the window is fullscreen. */
    fullscreen: boolean;
    /** Same value as `fullscreen`. */
    isFullscreen: boolean;
    /** Id of the window the call acted on. */
    windowId: string;
}

/**
 * Response from `window.isFullscreen`: `WindowIsFullscreenSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowIsFullscreenResponse = WindowIsFullscreenSuccess | ApiFailure;

/**
 * Successful response from `window.isMaximized`.
 */
export interface WindowIsMaximizedSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the window is maximized. */
    maximized: boolean;
    /** Same value as `maximized`. */
    isMaximized: boolean;
}

/**
 * Response from `window.isMaximized`: `WindowIsMaximizedSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowIsMaximizedResponse = WindowIsMaximizedSuccess | ApiFailure;

/**
 * Successful response from `window.isMinimized`.
 */
export interface WindowIsMinimizedSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the window is minimized. */
    minimized: boolean;
}

/**
 * Response from `window.isMinimized`: `WindowIsMinimizedSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowIsMinimizedResponse = WindowIsMinimizedSuccess | ApiFailure;

/**
 * Successful response from `window.isResizable`.
 */
export interface WindowIsResizableSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the user can resize the window by its frame. */
    resizable: boolean;
    /** Id of the window the call acted on. */
    windowId: string;
}

/**
 * Response from `window.isResizable`: `WindowIsResizableSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowIsResizableResponse = WindowIsResizableSuccess | ApiFailure;

/**
 * Successful response from `window.maximize`.
 */
export interface WindowMaximizeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.maximize`: `WindowMaximizeSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowMaximizeResponse = WindowMaximizeSuccess | ApiFailure;

/**
 * Successful response from `window.minimize`.
 */
export interface WindowMinimizeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.minimize`: `WindowMinimizeSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowMinimizeResponse = WindowMinimizeSuccess | ApiFailure;

/**
 * Successful response from `window.refreshWebView`.
 */
export interface WindowRefreshWebViewSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.refreshWebView`: `WindowRefreshWebViewSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowRefreshWebViewResponse = WindowRefreshWebViewSuccess | ApiFailure;

/**
 * Successful response from `window.reload`.
 */
export interface WindowReloadSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.reload`: `WindowReloadSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowReloadResponse = WindowReloadSuccess | ApiFailure;

/**
 * Successful response from `window.resetZoom`.
 */
export interface WindowResetZoomSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Always `1`. */
    zoom: number;
}

/**
 * Response from `window.resetZoom`: `WindowResetZoomSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowResetZoomResponse = WindowResetZoomSuccess | ApiFailure;

/**
 * Successful response from `window.restore`.
 */
export interface WindowRestoreSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.restore`: `WindowRestoreSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowRestoreResponse = WindowRestoreSuccess | ApiFailure;

/**
 * Successful response from `window.sendMessage`.
 */
export interface WindowSendMessageSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.sendMessage`: `WindowSendMessageSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSendMessageResponse = WindowSendMessageSuccess | ApiFailure;

/**
 * Successful response from `window.setAcrylic`.
 */
export interface WindowSetAcrylicSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Echo of `enabled`. */
    enabled: boolean;
    /** Echo of `darkMode`; present when it was given. */
    darkMode?: boolean;
}

/**
 * Response from `window.setAcrylic`: `WindowSetAcrylicSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetAcrylicResponse = WindowSetAcrylicSuccess | ApiFailure;

/**
 * Successful response from `window.setAlwaysOnTop`.
 */
export interface WindowSetAlwaysOnTopSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.setAlwaysOnTop`: `WindowSetAlwaysOnTopSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetAlwaysOnTopResponse = WindowSetAlwaysOnTopSuccess | ApiFailure;

/**
 * Successful response from `window.setBackdropPolicy`.
 */
export interface WindowSetBackdropPolicySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the window the call acted on. */
    windowId: string;
    /** The backdrop policy overrides after the merge. */
    backdropPolicy: Record<string, JsonValue>;
    /** The backdrop policy in effect. */
    resolvedBackdropPolicy: WindowBackdropPolicyState;
}

/**
 * Response from `window.setBackdropPolicy`: `WindowSetBackdropPolicySuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetBackdropPolicyResponse = WindowSetBackdropPolicySuccess | ApiFailure;

/**
 * Successful response from `window.setBackgroundTransparency`.
 */
export interface WindowSetBackgroundTransparencySuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Echo of `transparent`. */
    transparent: boolean;
    /** An English sentence describing the new state, for logs. */
    description: string;
}

/**
 * Response from `window.setBackgroundTransparency`: `WindowSetBackgroundTransparencySuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetBackgroundTransparencyResponse = WindowSetBackgroundTransparencySuccess | ApiFailure;

/**
 * Successful response from `window.setBlur`.
 */
export interface WindowSetBlurSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Echo of `enabled`. */
    enabled: boolean;
}

/**
 * Response from `window.setBlur`: `WindowSetBlurSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetBlurResponse = WindowSetBlurSuccess | ApiFailure;

/**
 * Successful response from `window.setBounds`.
 */
export interface WindowSetBoundsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.setBounds`: `WindowSetBoundsSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetBoundsResponse = WindowSetBoundsSuccess | ApiFailure;

/**
 * Successful response from `window.setClickThrough`.
 */
export interface WindowSetClickThroughSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether mouse input passes through now. */
    clickThrough: boolean;
}

/**
 * Response from `window.setClickThrough`: `WindowSetClickThroughSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetClickThroughResponse = WindowSetClickThroughSuccess | ApiFailure;

/**
 * Successful response from `window.setClickThroughExcludeRegions`.
 */
export interface WindowSetClickThroughExcludeRegionsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the popup. */
    windowId: string;
    /** How many rectangles were kept. */
    count: number;
    /** Scale applied to the rectangles, the popup's DPI divided by 96. */
    dpiScale: number;
    /** `regions truncated to 32` when more than 32 rectangles were given. */
    warning?: string;
}

/**
 * Response from `window.setClickThroughExcludeRegions`: `WindowSetClickThroughExcludeRegionsSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetClickThroughExcludeRegionsResponse = WindowSetClickThroughExcludeRegionsSuccess | ApiFailure;

/**
 * Successful response from `window.setCornerPreference`.
 */
export interface WindowSetCornerPreferenceSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.setCornerPreference`: `WindowSetCornerPreferenceSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetCornerPreferenceResponse = WindowSetCornerPreferenceSuccess | ApiFailure;

/**
 * Successful response from `window.setDarkMode`.
 */
export interface WindowSetDarkModeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Echo of `enabled`. */
    enabled: boolean;
}

/**
 * Response from `window.setDarkMode`: `WindowSetDarkModeSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetDarkModeResponse = WindowSetDarkModeSuccess | ApiFailure;

/**
 * Successful response from `window.setDevServerConfig`.
 */
export interface WindowSetDevServerConfigSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether pages load from the development server, as stored. */
    useDevServer: boolean;
    /** Address of the development server, as stored. */
    devServerUrl: string;
}

/**
 * Response from `window.setDevServerConfig`: `WindowSetDevServerConfigSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetDevServerConfigResponse = WindowSetDevServerConfigSuccess | ApiFailure;

/**
 * Successful response from `window.setDragRegions`.
 */
export interface WindowSetDragRegionsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** How many rectangles were kept. */
    count: number;
    /** Scale applied to the rectangles, the window's DPI divided by 96. */
    dpiScale: number;
}

/**
 * Response from `window.setDragRegions`: `WindowSetDragRegionsSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetDragRegionsResponse = WindowSetDragRegionsSuccess | ApiFailure;

/**
 * Successful response from `window.setFrameless`.
 */
export interface WindowSetFramelessSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the window is frameless now. */
    frameless: boolean;
}

/**
 * Response from `window.setFrameless`: `WindowSetFramelessSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetFramelessResponse = WindowSetFramelessSuccess | ApiFailure;

/**
 * Successful response from `window.setFullscreen`.
 */
export interface WindowSetFullscreenSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the window is fullscreen now. */
    fullscreen: boolean;
}

/**
 * Response from `window.setFullscreen`: `WindowSetFullscreenSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetFullscreenResponse = WindowSetFullscreenSuccess | ApiFailure;

/**
 * Successful response from `window.setMaximizeButtonRegion`.
 */
export interface WindowSetMaximizeButtonRegionSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether a rectangle is now set: `false` after removing it, or when the rectangle had no positive width and height. */
    hasRegion: boolean;
    /** The system offers Snap layouts for a maximize button, which means Windows 11 or later. Where it is `false` the host never answers the rectangle as the maximize button. Where it is `true` the button's `title` tooltip, which the page still shows on hover, would cover the Snap layouts flyout, so leave the title off and name the button with `aria-label`. */
    snapLayouts: boolean;
    /** Scale applied to the rectangle: the window's DPI divided by 96, times the page zoom. */
    scale: number;
}

/**
 * Response from `window.setMaximizeButtonRegion`: `WindowSetMaximizeButtonRegionSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetMaximizeButtonRegionResponse = WindowSetMaximizeButtonRegionSuccess | ApiFailure;

/**
 * Successful response from `window.setMaxSize`.
 */
export interface WindowSetMaxSizeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the window the call acted on. */
    windowId: string;
}

/**
 * Response from `window.setMaxSize`: `WindowSetMaxSizeSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetMaxSizeResponse = WindowSetMaxSizeSuccess | ApiFailure;

/**
 * Successful response from `window.setMica`.
 */
export interface WindowSetMicaSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Echo of `enabled`. */
    enabled: boolean;
    /** The variant used, `mica` or `mica-alt`. */
    variant: string;
    /** Echo of `darkMode`; present when it was given. */
    darkMode?: boolean;
}

/**
 * Response from `window.setMica`: `WindowSetMicaSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetMicaResponse = WindowSetMicaSuccess | ApiFailure;

/**
 * Successful response from `window.setMicaEffect`.
 */
export interface WindowSetMicaEffectSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Echo of `enabled`. */
    enabled: boolean;
    /** The variant used, `mica` or `mica-alt`. */
    variant: string;
    /** Echo of `darkMode`; present when it was given. */
    darkMode?: boolean;
}

/**
 * Response from `window.setMicaEffect`: `WindowSetMicaEffectSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetMicaEffectResponse = WindowSetMicaEffectSuccess | ApiFailure;

/**
 * Successful response from `window.setMinSize`.
 */
export interface WindowSetMinSizeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the window the call acted on. */
    windowId: string;
}

/**
 * Response from `window.setMinSize`: `WindowSetMinSizeSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetMinSizeResponse = WindowSetMinSizeSuccess | ApiFailure;

/**
 * Successful response from `window.setNoDragRegions`.
 */
export interface WindowSetNoDragRegionsSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** How many rectangles were kept. */
    count: number;
    /** Scale applied to the rectangles, the window's DPI divided by 96. */
    dpiScale: number;
}

/**
 * Response from `window.setNoDragRegions`: `WindowSetNoDragRegionsSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetNoDragRegionsResponse = WindowSetNoDragRegionsSuccess | ApiFailure;

/**
 * Successful response from `window.setPopupBehavior`.
 */
export interface WindowSetPopupBehaviorSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the popup. */
    windowId: string;
    /** The behavior preset. */
    profile: "legacy" | "standard" | "miniPlayer" | "desktopLyrics";
    /** The behavior overrides after the merge. */
    behavior: Record<string, JsonValue>;
    /** The behavior in effect. */
    resolvedBehavior: WindowPopupBehaviorState;
}

/**
 * Response from `window.setPopupBehavior`: `WindowSetPopupBehaviorSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetPopupBehaviorResponse = WindowSetPopupBehaviorSuccess | ApiFailure;

/**
 * Successful response from `window.setPosition`.
 */
export interface WindowSetPositionSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.setPosition`: `WindowSetPositionSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetPositionResponse = WindowSetPositionSuccess | ApiFailure;

/**
 * Successful response from `window.setResizable`.
 */
export interface WindowSetResizableSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Id of the window the call acted on. */
    windowId: string;
}

/**
 * Response from `window.setResizable`: `WindowSetResizableSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetResizableResponse = WindowSetResizableSuccess | ApiFailure;

/**
 * Successful response from `window.setSize`.
 */
export interface WindowSetSizeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.setSize`: `WindowSetSizeSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetSizeResponse = WindowSetSizeSuccess | ApiFailure;

/**
 * Successful response from `window.setTitle`.
 */
export interface WindowSetTitleSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.setTitle`: `WindowSetTitleSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetTitleResponse = WindowSetTitleSuccess | ApiFailure;

/**
 * Successful response from `window.setTitlebarHeight`.
 */
export interface WindowSetTitlebarHeightSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The height that was set. */
    height: number;
}

/**
 * Response from `window.setTitlebarHeight`: `WindowSetTitlebarHeightSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetTitlebarHeightResponse = WindowSetTitlebarHeightSuccess | ApiFailure;

/**
 * Successful response from `window.setZoom`.
 */
export interface WindowSetZoomSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The zoom factor in effect after the call. */
    zoom: number;
}

/**
 * Response from `window.setZoom`: `WindowSetZoomSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetZoomResponse = WindowSetZoomSuccess | ApiFailure;

/**
 * Successful response from `window.setZoomForDpi`.
 */
export interface WindowSetZoomForDpiSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** The DPI used. */
    dpi: number;
    /** The zoom factor in effect after the call. */
    zoom: number;
}

/**
 * Response from `window.setZoomForDpi`: `WindowSetZoomForDpiSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowSetZoomForDpiResponse = WindowSetZoomForDpiSuccess | ApiFailure;

/**
 * Successful response from `window.showSystemMenu`.
 */
export interface WindowShowSystemMenuSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.showSystemMenu`: `WindowShowSystemMenuSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowShowSystemMenuResponse = WindowShowSystemMenuSuccess | ApiFailure;

/**
 * Successful response from `window.startDrag`.
 */
export interface WindowStartDragSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.startDrag`: `WindowStartDragSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowStartDragResponse = WindowStartDragSuccess | ApiFailure;

/**
 * Successful response from `window.startResize`.
 */
export interface WindowStartResizeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
}

/**
 * Response from `window.startResize`: `WindowStartResizeSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowStartResizeResponse = WindowStartResizeSuccess | ApiFailure;

/**
 * Successful response from `window.toggleAlwaysOnTop`.
 */
export interface WindowToggleAlwaysOnTopSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the window is now kept above other windows. */
    enabled: boolean;
}

/**
 * Response from `window.toggleAlwaysOnTop`: `WindowToggleAlwaysOnTopSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowToggleAlwaysOnTopResponse = WindowToggleAlwaysOnTopSuccess | ApiFailure;

/**
 * Successful response from `window.toggleFullscreen`.
 */
export interface WindowToggleFullscreenSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the window is fullscreen now. */
    fullscreen: boolean;
}

/**
 * Response from `window.toggleFullscreen`: `WindowToggleFullscreenSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowToggleFullscreenResponse = WindowToggleFullscreenSuccess | ApiFailure;

/**
 * Successful response from `window.toggleMaximize`.
 */
export interface WindowToggleMaximizeSuccess {
    /** Always `true` here; a failed call resolves with `ApiFailure` instead. */
    success: true;
    /** Whether the window is maximized once the posted request lands. After leaving fullscreen it is the state the window returned to. */
    maximized: boolean;
}

/**
 * Response from `window.toggleMaximize`: `WindowToggleMaximizeSuccess`, or `ApiFailure` when the call failed.
 */
export type WindowToggleMaximizeResponse = WindowToggleMaximizeSuccess | ApiFailure;
// ── API-name → Response map ──────────────────────────────────────────────
/**
 * Map from each declared method name to its `*Response` type.
 */
export interface ApiResponseMap {
    "artwork.getAvailableArtwork": ArtworkGetAvailableArtworkResponse;
    "artwork.getAvailableTypes": ArtworkGetAvailableTypesResponse;
    "artwork.getBatch": ArtworkGetBatchResponse;
    "artwork.getByPath": ArtworkGetByPathResponse;
    "artwork.getByPlaylistItem": ArtworkGetByPlaylistItemResponse;
    "artwork.getCurrent": ArtworkGetCurrentResponse;
    "artwork.getFb2kUrl": ArtworkGetFb2kUrlResponse;
    "artwork.getFb2kUrlByPath": ArtworkGetFb2kUrlByPathResponse;
    "artwork.getFb2kUrlByPathBatch": ArtworkGetFb2kUrlByPathBatchResponse;
    "artwork.getFolderImages": ArtworkGetFolderImagesResponse;
    "artwork.getForTrack": ArtworkGetForTrackResponse;
    "artwork.getLyrics": ArtworkGetLyricsResponse;
    "artwork.getMetadata": ArtworkGetMetadataResponse;
    "audio.analyzeBPM": AudioAnalyzeBPMResponse;
    /** @experimental */
    "audio.cancelDecodePcm": AudioCancelDecodePcmResponse;
    "audio.cancelFullWaveform": AudioCancelFullWaveformResponse;
    /** @experimental */
    "audio.decodePcm": AudioDecodePcmResponse;
    "audio.generateFullWaveform": AudioGenerateFullWaveformResponse;
    "audio.getOutputInfo": AudioGetOutputInfoResponse;
    /** @experimental */
    "audio.getPcmDebugState": AudioGetPcmDebugStateResponse;
    "audio.getSpectrum": AudioGetSpectrumResponse;
    "audio.getSpectrumDebugState": AudioGetSpectrumDebugStateResponse;
    "audio.getStreamInfo": AudioGetStreamInfoResponse;
    "audio.getWaveform": AudioGetWaveformResponse;
    "audio.isVisualizationAvailable": AudioIsVisualizationAvailableResponse;
    "audio.setChannelMode": AudioSetChannelModeResponse;
    "audio.subscribeSpectrum": AudioSubscribeSpectrumResponse;
    /** @experimental */
    "audio.subscribeStream": AudioSubscribeStreamResponse;
    "audio.unsubscribeSpectrum": AudioUnsubscribeSpectrumResponse;
    /** @experimental */
    "audio.unsubscribeStream": AudioUnsubscribeStreamResponse;
    "clipboard.read": ClipboardReadResponse;
    "clipboard.write": ClipboardWriteResponse;
    "clipboard.writeFiles": ClipboardWriteFilesResponse;
    "clipboard.writeHTML": ClipboardWriteHTMLResponse;
    "config.export": ConfigExportResponse;
    "config.get": ConfigGetResponse;
    "config.getActiveDspPreset": ConfigGetActiveDspPresetResponse;
    "config.getAdvancedConfig": ConfigGetAdvancedConfigResponse;
    "config.getAdvancedConfigValue": ConfigGetAdvancedConfigValueResponse;
    "config.getAll": ConfigGetAllResponse;
    "config.getComponents": ConfigGetComponentsResponse;
    "config.getCursorFollowPlayback": ConfigGetCursorFollowPlaybackResponse;
    "config.getDspPresets": ConfigGetDspPresetsResponse;
    "config.getLibraryFilePatterns": ConfigGetLibraryFilePatternsResponse;
    "config.getLibraryStatus": ConfigGetLibraryStatusResponse;
    "config.getOutputConfig": ConfigGetOutputConfigResponse;
    "config.getOutputDevices": ConfigGetOutputDevicesResponse;
    "config.getPlaybackFollowCursor": ConfigGetPlaybackFollowCursorResponse;
    "config.getPreferencesPages": ConfigGetPreferencesPagesResponse;
    "config.getPreferencesStandardGuids": ConfigGetPreferencesStandardGuidsResponse;
    "config.getReplaygainMode": ConfigGetReplaygainModeResponse;
    "config.getVersionInfo": ConfigGetVersionInfoResponse;
    "config.remove": ConfigRemoveResponse;
    "config.resetAdvancedConfig": ConfigResetAdvancedConfigResponse;
    "config.set": ConfigSetResponse;
    "config.setActiveDspPreset": ConfigSetActiveDspPresetResponse;
    "config.setAdvancedConfigValue": ConfigSetAdvancedConfigValueResponse;
    "config.setCursorFollowPlayback": ConfigSetCursorFollowPlaybackResponse;
    "config.setOutputBuffer": ConfigSetOutputBufferResponse;
    "config.setOutputDevice": ConfigSetOutputDeviceResponse;
    "config.setPlaybackFollowCursor": ConfigSetPlaybackFollowCursorResponse;
    "config.setReplaygainMode": ConfigSetReplaygainModeResponse;
    "config.showLibraryPreferences": ConfigShowLibraryPreferencesResponse;
    "console.error": ConsoleErrorResponse;
    "console.log": ConsoleLogResponse;
    "console.warn": ConsoleWarnResponse;
    "cursor.isHidden": CursorIsHiddenResponse;
    "cursor.setHidden": CursorSetHiddenResponse;
    "dialog.confirm": DialogConfirmResponse;
    "dialog.openFile": DialogOpenFileResponse;
    "dialog.openFolder": DialogOpenFolderResponse;
    "dialog.saveFile": DialogSaveFileResponse;
    "discovery.executeContextMenuByPath": DiscoveryExecuteContextMenuByPathResponse;
    "discovery.executeContextMenuCommand": DiscoveryExecuteContextMenuCommandResponse;
    "discovery.executeMainMenuCommand": DiscoveryExecuteMainMenuCommandResponse;
    "discovery.getAllServices": DiscoveryGetAllServicesResponse;
    "discovery.getComponents": DiscoveryGetComponentsResponse;
    "discovery.getContextMenuCommands": DiscoveryGetContextMenuCommandsResponse;
    "discovery.getContextMenuTree": DiscoveryGetContextMenuTreeResponse;
    "discovery.getDspEntries": DiscoveryGetDspEntriesResponse;
    "discovery.getInputFormats": DiscoveryGetInputFormatsResponse;
    "discovery.getMainMenuCommands": DiscoveryGetMainMenuCommandsResponse;
    "discovery.getMainMenuGroups": DiscoveryGetMainMenuGroupsResponse;
    "discovery.getOutputDevices": DiscoveryGetOutputDevicesResponse;
    "discovery.getPreferencePages": DiscoveryGetPreferencePagesResponse;
    "discovery.getUIElements": DiscoveryGetUIElementsResponse;
    "discovery.searchCommands": DiscoverySearchCommandsResponse;
    "dnd.getCapabilities": DndGetCapabilitiesResponse;
    "dnd.getPathsAsync": DndGetPathsAsyncResponse;
    "dnd.prepareDrag": DndPrepareDragResponse;
    "dnd.startDrag": DndStartDragResponse;
    "dsp.addDsp": DspAddDspResponse;
    "dsp.applyPreset": DspApplyPresetResponse;
    "dsp.getAvailable": DspGetAvailableResponse;
    "dsp.getChain": DspGetChainResponse;
    "dsp.getPresets": DspGetPresetsResponse;
    "dsp.moveDsp": DspMoveDspResponse;
    "dsp.removeDsp": DspRemoveDspResponse;
    "dsp.setChain": DspSetChainResponse;
    "event.emit": EventEmitResponse;
    "event.emitTo": EventEmitToResponse;
    "file.cancelOp": FileCancelOpResponse;
    "file.copy": FileCopyResponse;
    "file.copyAsync": FileCopyAsyncResponse;
    "file.delete": FileDeleteResponse;
    "file.deleteAsync": FileDeleteAsyncResponse;
    "file.exists": FileExistsResponse;
    "file.getInfo": FileGetInfoResponse;
    "file.list": FileListResponse;
    "file.mkdir": FileMkdirResponse;
    "file.move": FileMoveResponse;
    "file.moveAsync": FileMoveAsyncResponse;
    "file.read": FileReadResponse;
    "file.rename": FileRenameResponse;
    "file.write": FileWriteResponse;
    "http.abort": HttpAbortResponse;
    "http.delete": HttpDeleteResponse;
    "http.download": HttpDownloadResponse;
    "http.get": HttpGetResponse;
    "http.head": HttpHeadResponse;
    "http.patch": HttpPatchResponse;
    "http.post": HttpPostResponse;
    "http.put": HttpPutResponse;
    "jitQueue.clear": JitQueueClearResponse;
    "jitQueue.enqueueNext": JitQueueEnqueueNextResponse;
    "jitQueue.getState": JitQueueGetStateResponse;
    "jitQueue.notifyEmpty": JitQueueNotifyEmptyResponse;
    "jitQueue.playNow": JitQueuePlayNowResponse;
    "jitQueue.preloadBatch": JitQueuePreloadBatchResponse;
    "jitQueue.skip": JitQueueSkipResponse;
    "jitQueue.stop": JitQueueStopResponse;
    "keyboard.getRegisteredHotkeys": KeyboardGetRegisteredHotkeysResponse;
    "keyboard.registerHotkey": KeyboardRegisterHotkeyResponse;
    "keyboard.registerShortcut": KeyboardRegisterShortcutResponse;
    "keyboard.unregisterHotkey": KeyboardUnregisterHotkeyResponse;
    "library.addToPlaylist": LibraryAddToPlaylistResponse;
    "library.browseDirectory": LibraryBrowseDirectoryResponse;
    "library.browseTree": LibraryBrowseTreeResponse;
    "library.getAlbums": LibraryGetAlbumsResponse;
    "library.getAlbumTracks": LibraryGetAlbumTracksResponse;
    "library.getAll": LibraryGetAllResponse;
    "library.getArtistAlbums": LibraryGetArtistAlbumsResponse;
    "library.getArtists": LibraryGetArtistsResponse;
    "library.getArtistTracks": LibraryGetArtistTracksResponse;
    "library.getByPath": LibraryGetByPathResponse;
    "library.getCacheStats": LibraryGetCacheStatsResponse;
    "library.getCount": LibraryGetCountResponse;
    "library.getFieldValues": LibraryGetFieldValuesResponse;
    "library.getGenres": LibraryGetGenresResponse;
    "library.getRandomTracks": LibraryGetRandomTracksResponse;
    "library.getRecentlyAdded": LibraryGetRecentlyAddedResponse;
    "library.getRoots": LibraryGetRootsResponse;
    "library.getStats": LibraryGetStatsResponse;
    "library.getStatus": LibraryGetStatusResponse;
    "library.invalidateCache": LibraryInvalidateCacheResponse;
    "library.isEnabled": LibraryIsEnabledResponse;
    "library.query": LibraryQueryResponse;
    "library.refresh": LibraryRefreshResponse;
    "library.rescan": LibraryRescanResponse;
    "library.search": LibrarySearchResponse;
    "log.clear": LogClearResponse;
    "log.read": LogReadResponse;
    "log.write": LogWriteResponse;
    "lyrics.exists": LyricsExistsResponse;
    "lyrics.get": LyricsGetResponse;
    "lyrics.save": LyricsSaveResponse;
    /** @experimental */
    "media.getContainerInfo": MediaGetContainerInfoResponse;
    /** @experimental */
    "media.getStreamUrl": MediaGetStreamUrlResponse;
    "menu.close": MenuCloseResponse;
    "menu.getContextMenu": MenuGetContextMenuResponse;
    "menu.getMainMenu": MenuGetMainMenuResponse;
    "menu.runContextCommand": MenuRunContextCommandResponse;
    "menu.runContextCommandById": MenuRunContextCommandByIdResponse;
    "menu.runMainMenuCommand": MenuRunMainMenuCommandResponse;
    "menu.show": MenuShowResponse;
    "menu.showNativePopup": MenuShowNativePopupResponse;
    "metadata.cancelProbe": MetadataCancelProbeResponse;
    "metadata.embedArtwork": MetadataEmbedArtworkResponse;
    "metadata.probeBatchAsync": MetadataProbeBatchAsyncResponse;
    "metadata.read": MetadataReadResponse;
    "metadata.readBatch": MetadataReadBatchResponse;
    "metadata.readByPath": MetadataReadByPathResponse;
    "metadata.readRaw": MetadataReadRawResponse;
    "metadata.removeEmbeddedArt": MetadataRemoveEmbeddedArtResponse;
    "metadata.removeField": MetadataRemoveFieldResponse;
    "metadata.removeTag": MetadataRemoveTagResponse;
    "metadata.write": MetadataWriteResponse;
    "metadata.writeBatch": MetadataWriteBatchResponse;
    "misc.exit": MiscExitResponse;
    "misc.getComponentPath": MiscGetComponentPathResponse;
    "misc.getFoobarPath": MiscGetFoobarPathResponse;
    "misc.getProfilePath": MiscGetProfilePathResponse;
    "misc.restart": MiscRestartResponse;
    "misc.showConsole": MiscShowConsoleResponse;
    "misc.showLibrarySearch": MiscShowLibrarySearchResponse;
    "misc.showPopupMessage": MiscShowPopupMessageResponse;
    "misc.showPreferences": MiscShowPreferencesResponse;
    "output.getDevices": OutputGetDevicesResponse;
    "output.getEntries": OutputGetEntriesResponse;
    "output.getSettings": OutputGetSettingsResponse;
    "panel.getConfig": PanelGetConfigResponse;
    "panel.setConfig": PanelSetConfigResponse;
    "playback.getCurrentTrack": PlaybackGetCurrentTrackResponse;
    "playback.getCurrentTrackIndex": PlaybackGetCurrentTrackIndexResponse;
    "playback.getPlaybackOrder": PlaybackGetPlaybackOrderResponse;
    "playback.getPlayingPlaylist": PlaybackGetPlayingPlaylistResponse;
    "playback.getPosition": PlaybackGetPositionResponse;
    "playback.getState": PlaybackGetStateResponse;
    "playback.getStopAfterCurrent": PlaybackGetStopAfterCurrentResponse;
    "playback.getVolume": PlaybackGetVolumeResponse;
    "playback.mute": PlaybackMuteResponse;
    "playback.next": PlaybackNextResponse;
    "playback.pause": PlaybackPauseResponse;
    "playback.play": PlaybackPlayResponse;
    "playback.playOrPause": PlaybackPlayOrPauseResponse;
    "playback.playPath": PlaybackPlayPathResponse;
    "playback.playPaths": PlaybackPlayPathsResponse;
    "playback.playPause": PlaybackPlayPauseResponse;
    "playback.previous": PlaybackPreviousResponse;
    "playback.random": PlaybackRandomResponse;
    "playback.setPlaybackOrder": PlaybackSetPlaybackOrderResponse;
    "playback.setPosition": PlaybackSetPositionResponse;
    "playback.setStopAfterCurrent": PlaybackSetStopAfterCurrentResponse;
    "playback.setVolume": PlaybackSetVolumeResponse;
    "playback.stop": PlaybackStopResponse;
    "playback.toggleMute": PlaybackToggleMuteResponse;
    "playback.toggleStopAfterCurrent": PlaybackToggleStopAfterCurrentResponse;
    "playback.volumeDown": PlaybackVolumeDownResponse;
    "playback.volumeUp": PlaybackVolumeUpResponse;
    "playcount.get": PlaycountGetResponse;
    "playcount.getBatch": PlaycountGetBatchResponse;
    "playcount.getStats": PlaycountGetStatsResponse;
    "playcount.set": PlaycountSetResponse;
    "playlist.addHandles": PlaylistAddHandlesResponse;
    "playlist.addPaths": PlaylistAddPathsResponse;
    "playlist.addPathsAsync": PlaylistAddPathsAsyncResponse;
    "playlist.addPathsSequential": PlaylistAddPathsSequentialResponse;
    "playlist.clear": PlaylistClearResponse;
    "playlist.convertToAutoplaylist": PlaylistConvertToAutoplaylistResponse;
    "playlist.create": PlaylistCreateResponse;
    "playlist.createAutoplaylist": PlaylistCreateAutoplaylistResponse;
    "playlist.deselectAll": PlaylistDeselectAllResponse;
    "playlist.duplicate": PlaylistDuplicateResponse;
    "playlist.focusTrack": PlaylistFocusTrackResponse;
    "playlist.getActive": PlaylistGetActiveResponse;
    "playlist.getAll": PlaylistGetAllResponse;
    "playlist.getAutoplaylistInfo": PlaylistGetAutoplaylistInfoResponse;
    "playlist.getAutoplaylistQuery": PlaylistGetAutoplaylistQueryResponse;
    "playlist.getAvailableColumns": PlaylistGetAvailableColumnsResponse;
    "playlist.getCount": PlaylistGetCountResponse;
    "playlist.getFocusedTrack": PlaylistGetFocusedTrackResponse;
    "playlist.getFocusTrack": PlaylistGetFocusTrackResponse;
    "playlist.getGroupRuns": PlaylistGetGroupRunsResponse;
    "playlist.getLockInfo": PlaylistGetLockInfoResponse;
    "playlist.getMatchingRows": PlaylistGetMatchingRowsResponse;
    "playlist.getPlaying": PlaylistGetPlayingResponse;
    "playlist.getSelectedTracks": PlaylistGetSelectedTracksResponse;
    "playlist.getSelection": PlaylistGetSelectionResponse;
    "playlist.getTrackCount": PlaylistGetTrackCountResponse;
    "playlist.getTracks": PlaylistGetTracksResponse;
    "playlist.getTracksAt": PlaylistGetTracksAtResponse;
    "playlist.insertTracks": PlaylistInsertTracksResponse;
    "playlist.isAutoplaylist": PlaylistIsAutoplaylistResponse;
    "playlist.isLocked": PlaylistIsLockedResponse;
    "playlist.moveTracks": PlaylistMoveTracksResponse;
    "playlist.playTrack": PlaylistPlayTrackResponse;
    "playlist.redo": PlaylistRedoResponse;
    "playlist.remove": PlaylistRemoveResponse;
    "playlist.removeAutoplaylist": PlaylistRemoveAutoplaylistResponse;
    "playlist.removeSelectedTracks": PlaylistRemoveSelectedTracksResponse;
    "playlist.removeTracks": PlaylistRemoveTracksResponse;
    "playlist.rename": PlaylistRenameResponse;
    "playlist.reorder": PlaylistReorderResponse;
    "playlist.reorderPlaylists": PlaylistReorderPlaylistsResponse;
    "playlist.replaceAllAndPlay": PlaylistReplaceAllAndPlayResponse;
    "playlist.reverse": PlaylistReverseResponse;
    "playlist.selectAll": PlaylistSelectAllResponse;
    "playlist.setActive": PlaylistSetActiveResponse;
    "playlist.setFocusedTrack": PlaylistSetFocusedTrackResponse;
    "playlist.setSelection": PlaylistSetSelectionResponse;
    "playlist.shuffle": PlaylistShuffleResponse;
    "playlist.sort": PlaylistSortResponse;
    "playlist.undo": PlaylistUndoResponse;
    "port.connect": PortConnectResponse;
    "port.disconnect": PortDisconnectResponse;
    "port.getPorts": PortGetPortsResponse;
    "port.postMessage": PortPostMessageResponse;
    "port.postMessageTo": PortPostMessageToResponse;
    "queue.add": QueueAddResponse;
    "queue.addPaths": QueueAddPathsResponse;
    "queue.clear": QueueClearResponse;
    "queue.flush": QueueFlushResponse;
    "queue.get": QueueGetResponse;
    "queue.getCount": QueueGetCountResponse;
    "queue.insertNext": QueueInsertNextResponse;
    "queue.moveToTop": QueueMoveToTopResponse;
    "queue.playNow": QueuePlayNowResponse;
    "queue.remove": QueueRemoveResponse;
    "queue.setContents": QueueSetContentsResponse;
    "rating.get": RatingGetResponse;
    "rating.set": RatingSetResponse;
    "replaygain.clear": ReplaygainClearResponse;
    "replaygain.get": ReplaygainGetResponse;
    "replaygain.getMode": ReplaygainGetModeResponse;
    "replaygain.getPreamp": ReplaygainGetPreampResponse;
    "replaygain.getSettings": ReplaygainGetSettingsResponse;
    "replaygain.scan": ReplaygainScanResponse;
    "replaygain.setMode": ReplaygainSetModeResponse;
    "replaygain.setPreamp": ReplaygainSetPreampResponse;
    "selection.get": SelectionGetResponse;
    "selection.getType": SelectionGetTypeResponse;
    "selection.getViewerMode": SelectionGetViewerModeResponse;
    "selection.getViewingTrack": SelectionGetViewingTrackResponse;
    "selection.set": SelectionSetResponse;
    "selection.setPlaylistTracking": SelectionSetPlaylistTrackingResponse;
    "shell.exec": ShellExecResponse;
    "shell.openExternal": ShellOpenExternalResponse;
    "shell.openWith": ShellOpenWithResponse;
    "shell.showInExplorer": ShellShowInExplorerResponse;
    "shell.spawn": ShellSpawnResponse;
    "state.delete": StateDeleteResponse;
    "state.get": StateGetResponse;
    "state.keys": StateKeysResponse;
    "state.set": StateSetResponse;
    "system.getApisByNamespace": SystemGetApisByNamespaceResponse;
    "system.getApiStats": SystemGetApiStatsResponse;
    "system.getDPI": SystemGetDPIResponse;
    "system.getLocale": SystemGetLocaleResponse;
    "system.getRegisteredPlugins": SystemGetRegisteredPluginsResponse;
    "system.getTheme": SystemGetThemeResponse;
    "system.isPluginRegistered": SystemIsPluginRegisteredResponse;
    "system.listAvailableApis": SystemListAvailableApisResponse;
    "system.searchApis": SystemSearchApisResponse;
    "taskbar.flash": TaskbarFlashResponse;
    "taskbar.setOverlayIcon": TaskbarSetOverlayIconResponse;
    "taskbar.setProgress": TaskbarSetProgressResponse;
    "taskbar.setThumbnailButtons": TaskbarSetThumbnailButtonsResponse;
    "taskbar.updateButton": TaskbarUpdateButtonResponse;
    "titleformat.eval": TitleformatEvalResponse;
    "titleformat.evalBatch": TitleformatEvalBatchResponse;
    "titleformat.evalFields": TitleformatEvalFieldsResponse;
    "titleformat.evalFieldsBatch": TitleformatEvalFieldsBatchResponse;
    "titleformat.getBuiltinFields": TitleformatGetBuiltinFieldsResponse;
    "tray.appendMenuItems": TrayAppendMenuItemsResponse;
    "tray.clearMenuItems": TrayClearMenuItemsResponse;
    "tray.create": TrayCreateResponse;
    "tray.destroy": TrayDestroyResponse;
    "tray.getMenuItems": TrayGetMenuItemsResponse;
    "tray.isVisible": TrayIsVisibleResponse;
    "tray.removeMenuItems": TrayRemoveMenuItemsResponse;
    "tray.setCloseToTray": TraySetCloseToTrayResponse;
    "tray.setContextMenu": TraySetContextMenuResponse;
    "tray.setIcon": TraySetIconResponse;
    "tray.setMenuItemState": TraySetMenuItemStateResponse;
    "tray.setMenuZones": TraySetMenuZonesResponse;
    "tray.setMinimizeToTray": TraySetMinimizeToTrayResponse;
    "tray.setTooltip": TraySetTooltipResponse;
    "tray.showBalloon": TrayShowBalloonResponse;
    "ui.hideNotification": UiHideNotificationResponse;
    "ui.showContextMenu": UiShowContextMenuResponse;
    "ui.showCustomMenu": UiShowCustomMenuResponse;
    "ui.showNotification": UiShowNotificationResponse;
    "ui.showToast": UiShowToastResponse;
    "webview.getSource": WebviewGetSourceResponse;
    "window.blur": WindowBlurResponse;
    "window.broadcast": WindowBroadcastResponse;
    "window.cancelClose": WindowCancelCloseResponse;
    "window.center": WindowCenterResponse;
    "window.clearClickThroughExcludeRegions": WindowClearClickThroughExcludeRegionsResponse;
    "window.clearDragRegions": WindowClearDragRegionsResponse;
    "window.clearNoDragRegions": WindowClearNoDragRegionsResponse;
    "window.close": WindowCloseResponse;
    "window.closeAllPopups": WindowCloseAllPopupsResponse;
    "window.closePopup": WindowClosePopupResponse;
    "window.confirmClose": WindowConfirmCloseResponse;
    "window.createPopup": WindowCreatePopupResponse;
    "window.enterFullscreen": WindowEnterFullscreenResponse;
    "window.exitFullscreen": WindowExitFullscreenResponse;
    "window.flash": WindowFlashResponse;
    "window.flashTaskbar": WindowFlashTaskbarResponse;
    "window.focus": WindowFocusResponse;
    "window.getAllWindows": WindowGetAllWindowsResponse;
    "window.getBackdropPolicy": WindowGetBackdropPolicyResponse;
    "window.getBounds": WindowGetBoundsResponse;
    "window.getCaptionButtonsWidth": WindowGetCaptionButtonsWidthResponse;
    "window.getCornerPreference": WindowGetCornerPreferenceResponse;
    "window.getCurrentWindowId": WindowGetCurrentWindowIdResponse;
    "window.getDevServerConfig": WindowGetDevServerConfigResponse;
    "window.getDpiScale": WindowGetDpiScaleResponse;
    "window.getMaxSize": WindowGetMaxSizeResponse;
    "window.getMinSize": WindowGetMinSizeResponse;
    "window.getMode": WindowGetModeResponse;
    "window.getPopupBehavior": WindowGetPopupBehaviorResponse;
    "window.getState": WindowGetStateResponse;
    "window.getTitle": WindowGetTitleResponse;
    "window.getTitlebarHeight": WindowGetTitlebarHeightResponse;
    "window.getTitlebarInfo": WindowGetTitlebarInfoResponse;
    "window.getZoom": WindowGetZoomResponse;
    "window.hasSavedBounds": WindowHasSavedBoundsResponse;
    "window.isAlwaysOnTop": WindowIsAlwaysOnTopResponse;
    "window.isClickThrough": WindowIsClickThroughResponse;
    "window.isFullscreen": WindowIsFullscreenResponse;
    "window.isMaximized": WindowIsMaximizedResponse;
    "window.isMinimized": WindowIsMinimizedResponse;
    "window.isResizable": WindowIsResizableResponse;
    "window.maximize": WindowMaximizeResponse;
    "window.minimize": WindowMinimizeResponse;
    "window.refreshWebView": WindowRefreshWebViewResponse;
    "window.reload": WindowReloadResponse;
    "window.resetZoom": WindowResetZoomResponse;
    "window.restore": WindowRestoreResponse;
    "window.sendMessage": WindowSendMessageResponse;
    "window.setAcrylic": WindowSetAcrylicResponse;
    "window.setAlwaysOnTop": WindowSetAlwaysOnTopResponse;
    "window.setBackdropPolicy": WindowSetBackdropPolicyResponse;
    "window.setBackgroundTransparency": WindowSetBackgroundTransparencyResponse;
    "window.setBlur": WindowSetBlurResponse;
    "window.setBounds": WindowSetBoundsResponse;
    "window.setClickThrough": WindowSetClickThroughResponse;
    "window.setClickThroughExcludeRegions": WindowSetClickThroughExcludeRegionsResponse;
    "window.setCornerPreference": WindowSetCornerPreferenceResponse;
    "window.setDarkMode": WindowSetDarkModeResponse;
    "window.setDevServerConfig": WindowSetDevServerConfigResponse;
    "window.setDragRegions": WindowSetDragRegionsResponse;
    "window.setFrameless": WindowSetFramelessResponse;
    "window.setFullscreen": WindowSetFullscreenResponse;
    "window.setMaximizeButtonRegion": WindowSetMaximizeButtonRegionResponse;
    "window.setMaxSize": WindowSetMaxSizeResponse;
    "window.setMica": WindowSetMicaResponse;
    "window.setMicaEffect": WindowSetMicaEffectResponse;
    "window.setMinSize": WindowSetMinSizeResponse;
    "window.setNoDragRegions": WindowSetNoDragRegionsResponse;
    "window.setPopupBehavior": WindowSetPopupBehaviorResponse;
    "window.setPosition": WindowSetPositionResponse;
    "window.setResizable": WindowSetResizableResponse;
    "window.setSize": WindowSetSizeResponse;
    "window.setTitle": WindowSetTitleResponse;
    "window.setTitlebarHeight": WindowSetTitlebarHeightResponse;
    "window.setZoom": WindowSetZoomResponse;
    "window.setZoomForDpi": WindowSetZoomForDpiResponse;
    "window.showSystemMenu": WindowShowSystemMenuResponse;
    "window.startDrag": WindowStartDragResponse;
    "window.startResize": WindowStartResizeResponse;
    "window.toggleAlwaysOnTop": WindowToggleAlwaysOnTopResponse;
    "window.toggleFullscreen": WindowToggleFullscreenResponse;
    "window.toggleMaximize": WindowToggleMaximizeResponse;
}
