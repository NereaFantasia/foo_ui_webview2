/**
 * Artwork tools - P0
 *
 * Wraps artwork.* bridge API as MCP tools.
 */

import type { ToolDefinition } from "../types.js";

export const artworkTools: ToolDefinition[] = [
    {
        name: "fb2k_artwork_get_current",
        description: "Get the cover art of the currently playing track (returns base64 data URL)",
        inputSchema: {
            type: "object",
            properties: {
                type: {
                    type: "string",
                    description: "Art type",
                    enum: ["front", "back", "disc", "icon", "artist"],
                },
            },
        },
    },
    {
        name: "fb2k_artwork_get_for_track",
        description: "Get the cover art of a specific track file",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description: "Track file path",
                },
                type: {
                    type: "string",
                    description: "Art type",
                    enum: ["front", "back", "disc", "icon", "artist"],
                },
            },
            required: ["path"],
        },
    },
];

export const artworkMethodMap: Record<string, string> = {
    fb2k_artwork_get_current: "artwork.getCurrent",
    fb2k_artwork_get_for_track: "artwork.getForTrack",
};
