/**
 * Queue tools
 *
 * Bridge API: queue.* -> MCP tools
 */

import type { ToolDefinition } from "../types.js";

export const queueTools: ToolDefinition[] = [
    {
        name: "fb2k_queue_get",
        description: "Get current playback queue contents",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_queue_add",
        description: "Add playlist tracks to the playback queue",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active playlist",
                },
                tracks: {
                    type: "array",
                    description: "Track indices to add",
                    items: { type: "integer" },
                },
                track: {
                    type: "integer",
                    description: "Single track index (alternative to tracks)",
                },
            },
        },
    },
    {
        name: "fb2k_queue_add_paths",
        description: "Add tracks by file path/URL to the playback queue",
        inputSchema: {
            type: "object",
            properties: {
                paths: {
                    type: "array",
                    description: "File paths or URLs, supports path|subsong:N format",
                    items: { type: "string" },
                },
                useQueuePlaylist: {
                    type: "boolean",
                    description: "Use dedicated queue playlist (default true)",
                    default: true,
                },
                playlist: {
                    type: "integer",
                    description: "Target playlist index (only when useQueuePlaylist=false)",
                },
            },
            required: ["paths"],
        },
    },
    {
        name: "fb2k_queue_remove",
        description: "Remove items from the playback queue",
        inputSchema: {
            type: "object",
            properties: {
                index: {
                    type: "integer",
                    description: "Queue item index to remove",
                    minimum: 0,
                },
                indices: {
                    type: "array",
                    description: "Queue item indices to remove (alternative to index)",
                    items: { type: "integer" },
                },
            },
        },
    },
    {
        name: "fb2k_queue_clear",
        description: "Clear the entire playback queue",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_queue_get_count",
        description: "Get playback queue item count",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_queue_move_to_top",
        description: "Move a queue item to the top (play next)",
        inputSchema: {
            type: "object",
            properties: {
                index: {
                    type: "integer",
                    description: "Queue item index to move to top",
                    minimum: 0,
                },
            },
            required: ["index"],
        },
    },
    {
        name: "fb2k_queue_flush",
        description: "Flush (clear) the playback queue (alias for queue.clear)",
        inputSchema: { type: "object", properties: {} },
    },
];

export const queueMethodMap: Record<string, string> = {
    fb2k_queue_get: "queue.get",
    fb2k_queue_add: "queue.add",
    fb2k_queue_add_paths: "queue.addPaths",
    fb2k_queue_remove: "queue.remove",
    fb2k_queue_clear: "queue.clear",
    fb2k_queue_get_count: "queue.getCount",
    fb2k_queue_move_to_top: "queue.moveToTop",
    fb2k_queue_flush: "queue.flush",
};
