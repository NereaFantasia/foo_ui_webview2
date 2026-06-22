/**
 * Playlist tools - P0
 *
 * Wraps playlist.* bridge API as MCP tools.
 */

import type { ToolDefinition } from "../types.js";

export const playlistTools: ToolDefinition[] = [
    {
        name: "fb2k_playlist_get_all",
        description: "Get names, track counts, and active/playing status of all playlists",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playlist_get_active",
        description: "Get details of the currently active playlist",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playlist_set_active",
        description: "Set the active playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index (0-based)",
                    minimum: 0,
                },
            },
            required: ["playlist"],
        },
    },
    {
        name: "fb2k_playlist_get_tracks",
        description: "Get tracks from a playlist (supports pagination)",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to the active playlist",
                    minimum: 0,
                },
                start: {
                    type: "integer",
                    description: "Start offset, default 0",
                    minimum: 0,
                },
                count: {
                    type: "integer",
                    description: "Number of tracks to return, default 50",
                    minimum: 1,
                    maximum: 500,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_play_track",
        description: "Play a specific track in a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to the active playlist",
                    minimum: 0,
                },
                index: {
                    type: "integer",
                    description: "Track index within the playlist (0-based)",
                    minimum: 0,
                },
                deferred: {
                    type: "boolean",
                    description: "Whether to defer playback",
                },
            },
            required: ["index"],
        },
    },
    {
        name: "fb2k_playlist_create",
        description: "Create a new empty playlist",
        inputSchema: {
            type: "object",
            properties: {
                name: {
                    type: "string",
                    description: "Playlist name",
                },
            },
            required: ["name"],
        },
    },
    {
        name: "fb2k_playlist_remove",
        description: "Remove a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index (0-based)",
                    minimum: 0,
                },
            },
            required: ["playlist"],
        },
    },
];

export const playlistMethodMap: Record<string, string> = {
    fb2k_playlist_get_all: "playlist.getAll",
    fb2k_playlist_get_active: "playlist.getActive",
    fb2k_playlist_set_active: "playlist.setActive",
    fb2k_playlist_get_tracks: "playlist.getTracks",
    fb2k_playlist_play_track: "playlist.playTrack",
    fb2k_playlist_create: "playlist.create",
    fb2k_playlist_remove: "playlist.remove",
};
