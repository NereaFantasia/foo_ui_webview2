/**
 * Metadata tools
 *
 * Bridge API: metadata.* / rating.* -> MCP tools
 * Source: src/api/MetadataApi.cpp - RegisterMetadataApi()
 */

import type { ToolDefinition } from "../types.js";

export const metadataTools: ToolDefinition[] = [
    {
        name: "fb2k_metadata_read",
        description: "Read all metadata tags of a file (structured: tags + info separated)",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description: "Audio file path",
                },
            },
            required: ["path"],
        },
    },
    {
        name: "fb2k_metadata_read_by_path",
        description: "Read all metadata tags of a file (flat: all tags and info merged)",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description: "Audio file path",
                },
            },
            required: ["path"],
        },
    },
    {
        name: "fb2k_metadata_read_raw",
        description: "Read metadata directly from file, bypassing metadb cache (source: file)",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description: "Audio file path (supports |subsong:N or #N for CUE sub-tracks)",
                },
                cueIndex: {
                    type: "integer",
                    description: "CUE sub-track index (overrides subsong marker in path)",
                },
            },
            required: ["path"],
        },
    },
    {
        name: "fb2k_metadata_read_batch",
        description: "Batch read metadata tags of multiple files (flat format)",
        inputSchema: {
            type: "object",
            properties: {
                paths: {
                    type: "array",
                    description: "Array of audio file paths",
                    items: { type: "string" },
                },
            },
            required: ["paths"],
        },
    },
    {
        name: "fb2k_metadata_write",
        description: "Write/update metadata tags of a file (null or empty string removes the tag)",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description: "Audio file path",
                },
                tags: {
                    type: "object",
                    description: "Tag key-value pairs (e.g. {\"TITLE\": \"...\", \"ARTIST\": \"...\"})",
                },
            },
            required: ["path", "tags"],
        },
    },
    {
        name: "fb2k_metadata_write_batch",
        description: "Batch write metadata tags for multiple files",
        inputSchema: {
            type: "object",
            properties: {
                items: {
                    type: "array",
                    description: "Array of write items, each containing path and tags",
                    items: {
                        type: "object",
                    },
                },
            },
            required: ["items"],
        },
    },
    {
        name: "fb2k_metadata_embed_artwork",
        description:
            "Write artwork into an audio file's tag container or as a sidecar in its directory. " +
            "Use target='file' to save a fb2k-recognized external image (e.g. cover.jpg) next " +
            "to the audio — works for CUE / container formats where album_art_editor cannot " +
            "embed. CUE / |subsong:N paths share one sidecar per directory.",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description:
                        "Audio file path (supports |subsong:N; for file target the sidecar " +
                        "lands in the directory of the underlying audio file)",
                },
                imageData: {
                    type: "string",
                    description: "Base64 encoded image data (JPEG / PNG / WebP / GIF / BMP auto-detected via magic bytes)",
                },
                type: {
                    type: "string",
                    description: "Artwork type",
                    enum: ["front", "back", "disc", "icon", "artist"],
                    default: "front",
                },
                target: {
                    type: "string",
                    description:
                        "Where to save: 'embedded' (default, write into file tags via " +
                        "album_art_editor) / 'file' (write a sibling image in the audio " +
                        "directory — cover.<ext> for front, <type>.<ext> otherwise) / " +
                        "'all' (run both)",
                    enum: ["embedded", "file", "all"],
                    default: "embedded",
                },
                filename: {
                    type: "string",
                    description:
                        "Custom filename when target includes 'file' (e.g. 'cover.jpg'). " +
                        "Must be a plain filename — path separators and '..' are rejected. " +
                        "Omit to auto-generate from type + detected image format.",
                },
            },
            required: ["path", "imageData"],
        },
    },
    {
        name: "fb2k_metadata_remove_embedded_art",
        description: "Remove embedded artwork from an audio file",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description: "Audio file path",
                },
                type: {
                    type: "string",
                    description: "Artwork type to remove (omit to remove all)",
                    enum: ["front", "back", "disc", "icon", "artist"],
                },
                removeAll: {
                    type: "boolean",
                    description: "Remove all artwork types",
                    default: false,
                },
            },
            required: ["path"],
        },
    },
    {
        name: "fb2k_metadata_remove_tag",
        description: "Remove specified metadata tags from a file",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description: "Audio file path",
                },
                tags: {
                    type: "array",
                    description: "Tag names to remove (e.g. [\"COMMENT\", \"LYRICS\"])",
                    items: { type: "string" },
                },
            },
            required: ["path", "tags"],
        },
    },
    {
        name: "fb2k_metadata_remove_field",
        description: "Remove specified metadata fields from a file (alias for removeTag)",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description: "Audio file path",
                },
                tags: {
                    type: "array",
                    description: "Field names to remove",
                    items: { type: "string" },
                },
            },
            required: ["path", "tags"],
        },
    },
    {
        name: "fb2k_rating_set",
        description: "Set track rating (0-5, 0=unrated). Uses foo_playcount context menu first, falls back to RATING tag",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description: "Track file path (omit to use current playing or selected track)",
                },
                rating: {
                    type: "integer",
                    description: "Rating value (0=unrated, 1-5)",
                    minimum: 0,
                    maximum: 5,
                },
                cueIndex: {
                    type: "integer",
                    description: "CUE sub-track index",
                },
            },
            required: ["rating"],
        },
    },
    {
        name: "fb2k_rating_get",
        description: "Get track rating (reads foo_playcount stats first, falls back to RATING tag)",
        inputSchema: {
            type: "object",
            properties: {
                path: {
                    type: "string",
                    description: "Track file path",
                },
                cueIndex: {
                    type: "integer",
                    description: "CUE sub-track index",
                },
            },
            required: ["path"],
        },
    },
];

export const metadataMethodMap: Record<string, string> = {
    fb2k_metadata_read: "metadata.read",
    fb2k_metadata_read_by_path: "metadata.readByPath",
    fb2k_metadata_read_raw: "metadata.readRaw",
    fb2k_metadata_read_batch: "metadata.readBatch",
    fb2k_metadata_write: "metadata.write",
    fb2k_metadata_write_batch: "metadata.writeBatch",
    fb2k_metadata_embed_artwork: "metadata.embedArtwork",
    fb2k_metadata_remove_embedded_art: "metadata.removeEmbeddedArt",
    fb2k_metadata_remove_tag: "metadata.removeTag",
    fb2k_metadata_remove_field: "metadata.removeField",
    fb2k_rating_set: "rating.set",
    fb2k_rating_get: "rating.get",
};
