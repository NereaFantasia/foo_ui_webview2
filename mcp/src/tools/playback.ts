/**
 * Playback tools.
 *
 * Wraps the `playback.*` bridge API as MCP tools.
 */

import type { ToolDefinition } from "../types.js";

export const playbackTools: ToolDefinition[] = [
    {
        name: "fb2k_playback_play",
        description: "Start playback of the current track",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_pause",
        description: "Pause playback",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_stop",
        description: "Stop playback",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_next",
        description: "Skip to the next track",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_previous",
        description: "Go to the previous track",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_play_pause",
        description: "Toggle play/pause state",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_get_state",
        description: "Get current playback state (stopped/paused/playing) and whether seeking/pausing is available",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_get_current_track",
        description: "Get detailed info of the currently playing track (title, artist, album, duration, path, etc.)",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_get_position",
        description: "Get current playback position and total duration in seconds",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_set_position",
        description: "Seek to a specific playback position",
        inputSchema: {
            type: "object",
            properties: {
                seconds: {
                    type: "number",
                    description: "Target position in seconds",
                    minimum: 0,
                },
            },
            required: ["seconds"],
        },
    },
    {
        name: "fb2k_playback_get_volume",
        description: "Get current volume (0-100 linear scale) and mute state",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playback_set_volume",
        description: "Set the playback volume",
        inputSchema: {
            type: "object",
            properties: {
                volume: {
                    type: "number",
                    description: "Volume level 0-100 (linear)",
                    minimum: 0,
                    maximum: 100,
                },
            },
            required: ["volume"],
        },
    },
];

/**
 * Maps each playback tool name to its corresponding bridge API method.
 */
export const playbackMethodMap: Record<string, string> = {
    fb2k_playback_play: "playback.play",
    fb2k_playback_pause: "playback.pause",
    fb2k_playback_stop: "playback.stop",
    fb2k_playback_next: "playback.next",
    fb2k_playback_previous: "playback.previous",
    fb2k_playback_play_pause: "playback.playPause",
    fb2k_playback_get_state: "playback.getState",
    fb2k_playback_get_current_track: "playback.getCurrentTrack",
    fb2k_playback_get_position: "playback.getPosition",
    fb2k_playback_set_position: "playback.setPosition",
    fb2k_playback_get_volume: "playback.getVolume",
    fb2k_playback_set_volume: "playback.setVolume",
};
