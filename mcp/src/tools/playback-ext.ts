/**
 * Playback extended tools (P1)
 *
 * Bridge API: playback.* methods not covered by playback.ts (P0)
 * Includes: mute, volume steps, playback order, stop-after-current, track location, path play, random
 */

import type { ToolDefinition } from "../types.js";

export const playbackExtTools: ToolDefinition[] = [
    // ========== Mute ==========
    {
        name: "fb2k_playback_mute",
        description: "Set mute state",
        inputSchema: {
            type: "object",
            properties: {
                muted: {
                    type: "boolean",
                    description: "Whether to mute (default true)",
                    default: true,
                },
            },
        },
    },
    {
        name: "fb2k_playback_toggle_mute",
        description: "Toggle mute state",
        inputSchema: { type: "object", properties: {} },
    },

    // ========== Volume steps ==========
    {
        name: "fb2k_playback_volume_up",
        description: "Increase volume by one step",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_volume_down",
        description: "Decrease volume by one step",
        inputSchema: { type: "object", properties: {} },
    },

    // ========== Playback order ==========
    {
        name: "fb2k_playback_get_playback_order",
        description: "Get current playback order (default/repeat-playlist/repeat-track/random/shuffle-tracks/shuffle-albums/shuffle-folders)",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_set_playback_order",
        description: "Set playback order by index or name",
        inputSchema: {
            type: "object",
            properties: {
                order: {
                    type: "string",
                    description:
                        "Playback order: number(0-6) or name (default/repeat-playlist/repeat-track/random/shuffle-tracks/shuffle-albums/shuffle-folders)",
                    enum: [
                        "default",
                        "repeat-playlist",
                        "repeat-track",
                        "random",
                        "shuffle-tracks",
                        "shuffle-albums",
                        "shuffle-folders",
                    ],
                },
            },
            required: ["order"],
        },
    },

    // ========== Stop after current ==========
    {
        name: "fb2k_playback_get_stop_after_current",
        description: "Get stop-after-current toggle state",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_set_stop_after_current",
        description: "Set stop-after-current toggle",
        inputSchema: {
            type: "object",
            properties: {
                enabled: {
                    type: "boolean",
                    description: "Whether to enable stop-after-current",
                },
            },
            required: ["enabled"],
        },
    },
    {
        name: "fb2k_playback_toggle_stop_after_current",
        description: "Toggle stop-after-current",
        inputSchema: { type: "object", properties: {} },
    },

    // ========== Track location ==========
    {
        name: "fb2k_playback_get_current_track_index",
        description: "Get current playing track position (playlist index + item index)",
        inputSchema: {
            type: "object",
            properties: {
                includeTrackInfo: {
                    type: "boolean",
                    description: "Include track details (default false)",
                    default: false,
                },
            },
        },
    },
    {
        name: "fb2k_playback_get_playing_playlist",
        description: "Get the playing playlist index and name",
        inputSchema: { type: "object", properties: {} },
    },

    // ========== Path play ==========
    {
        name: "fb2k_playback_play_path",
        description: "Play a single track by file path, supports subsong format (path|subsong:N)",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description: "File path, supports subsong suffix (e.g. C:\\music\\album.flac|subsong:2)",
                },
            },
            required: ["path"],
        },
    },

    // ========== Random ==========
    {
        name: "fb2k_playback_random",
        description: "Play a random track",
        inputSchema: { type: "object", properties: {} },
    },
];

export const playbackExtMethodMap: Record<string, string> = {
    fb2k_playback_mute: "playback.mute",
    fb2k_playback_toggle_mute: "playback.toggleMute",
    fb2k_playback_volume_up: "playback.volumeUp",
    fb2k_playback_volume_down: "playback.volumeDown",
    fb2k_playback_get_playback_order: "playback.getPlaybackOrder",
    fb2k_playback_set_playback_order: "playback.setPlaybackOrder",
    fb2k_playback_get_stop_after_current: "playback.getStopAfterCurrent",
    fb2k_playback_set_stop_after_current: "playback.setStopAfterCurrent",
    fb2k_playback_toggle_stop_after_current: "playback.toggleStopAfterCurrent",
    fb2k_playback_get_current_track_index: "playback.getCurrentTrackIndex",
    fb2k_playback_get_playing_playlist: "playback.getPlayingPlaylist",
    fb2k_playback_play_path: "playback.playPath",
    fb2k_playback_random: "playback.random",
};
