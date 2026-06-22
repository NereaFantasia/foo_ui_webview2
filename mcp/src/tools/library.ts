/**
 * Library tools - P0
 *
 * Wraps library.* bridge API as MCP tools.
 */

import type { ToolDefinition } from "../types.js";

export const libraryTools: ToolDefinition[] = [
    {
        name: "fb2k_library_search",
        description: "Search tracks in the media library (supports foobar2000 query syntax, e.g. artist IS Beatles)",
        inputSchema: {
            type: "object",
            properties: {
                query: {
                    type: "string",
                    description: "Search query string",
                },
                offset: {
                    type: "integer",
                    description: "Result offset, default 0",
                    minimum: 0,
                },
                limit: {
                    type: "integer",
                    description: "Maximum number of results, default 50",
                    minimum: 1,
                    maximum: 500,
                },
            },
            required: ["query"],
        },
    },
    {
        name: "fb2k_library_get_albums",
        description: "Get the list of all albums in the media library",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_library_get_artists",
        description: "Get the list of all artists in the media library",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_library_get_stats",
        description: "Get media library statistics (total track count, etc.)",
        inputSchema: { type: "object", properties: {} },
    },
];

export const libraryMethodMap: Record<string, string> = {
    fb2k_library_search: "library.search",
    fb2k_library_get_albums: "library.getAlbums",
    fb2k_library_get_artists: "library.getArtists",
    fb2k_library_get_stats: "library.getStats",
};
