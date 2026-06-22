/**
 * Playlist extended tools
 *
 * Bridge API: playlist.* methods not covered by playlist.ts
 * playlist.ts already covers: getAll, getActive, setActive, getTracks, playTrack, create, remove
 */

import type { ToolDefinition } from "../types.js";

export const playlistExtTools: ToolDefinition[] = [
    // ========== Playlist info queries ==========
    {
        name: "fb2k_playlist_get_count",
        description: "Get total number of playlists",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playlist_get_playing",
        description: "Get currently playing playlist details",
        inputSchema: { type: "object", properties: {} },
    },
    {
        name: "fb2k_playlist_get_track_count",
        description: "Get track count of a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index (0-based), defaults to active playlist",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_get_available_columns",
        description: "Get available DUI playlist column definitions (ID, name, format, sort pattern)",
        inputSchema: { type: "object", properties: {} },
    },

    // ========== Playlist management ==========
    {
        name: "fb2k_playlist_rename",
        description: "Rename a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index (0-based)",
                    minimum: 0,
                },
                name: {
                    type: "string",
                    description: "New name",
                },
            },
            required: ["playlist", "name"],
        },
    },
    {
        name: "fb2k_playlist_clear",
        description: "Clear all tracks from a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active playlist",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_duplicate",
        description: "Duplicate a playlist (with all tracks)",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Source playlist index, defaults to active",
                    minimum: 0,
                },
                name: {
                    type: "string",
                    description: "New playlist name, defaults to 'OriginalName (Copy)'",
                },
            },
        },
    },
    {
        name: "fb2k_playlist_reorder_playlists",
        description: "Reorder all playlists",
        inputSchema: {
            type: "object",
            properties: {
                newOrder: {
                    type: "array",
                    description: "New order array, length must equal total playlists, elements are original indices",
                    items: { type: "integer", minimum: 0 },
                },
            },
            required: ["newOrder"],
        },
    },

    // ========== Track operations ==========
    {
        name: "fb2k_playlist_insert_tracks",
        description: "Insert tracks into a playlist by handle",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                position: {
                    type: "integer",
                    description: "Insert position index, defaults to 0",
                    minimum: 0,
                },
                handles: {
                    type: "array",
                    description: "Track handle array",
                    items: { type: "string" },
                },
            },
            required: ["handles"],
        },
    },
    {
        name: "fb2k_playlist_remove_tracks",
        description: "Remove tracks by index from a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                items: {
                    type: "array",
                    description: "Track indices to remove",
                    items: { type: "integer", minimum: 0 },
                },
            },
            required: ["items"],
        },
    },
    {
        name: "fb2k_playlist_remove_selected_tracks",
        description: "Remove all selected tracks from a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_move_tracks",
        description: "Move selected tracks in a playlist (up/down)",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                items: {
                    type: "array",
                    description: "Track indices to move (optional, moves current selection if empty)",
                    items: { type: "integer", minimum: 0 },
                },
                delta: {
                    type: "integer",
                    description: "Move direction and distance (positive=down, negative=up)",
                },
            },
            required: ["delta"],
        },
    },
    {
        name: "fb2k_playlist_reorder",
        description: "Reorder tracks in a playlist by custom order",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                newOrder: {
                    type: "array",
                    description: "New order array, length must equal track count, elements are original indices",
                    items: { type: "integer", minimum: 0 },
                },
            },
            required: ["newOrder"],
        },
    },
    {
        name: "fb2k_playlist_reverse",
        description: "Reverse track order in a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_sort",
        description: "Sort playlist by title format pattern",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                pattern: {
                    type: "string",
                    description: "Sort format string (foobar2000 title format), default \"%title%\"",
                    default: "%title%",
                },
                descending: {
                    type: "boolean",
                    description: "Descending order, default false",
                    default: false,
                },
                selectedOnly: {
                    type: "boolean",
                    description: "Sort selected items only, default false",
                    default: false,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_shuffle",
        description: "Shuffle track order in a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },

    // ========== Add tracks (file path) ==========
    {
        name: "fb2k_playlist_add_paths",
        description: "Add tracks by file/folder paths to a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                paths: {
                    type: "array",
                    description: "File or folder path array",
                    items: { type: "string" },
                },
            },
            required: ["paths"],
        },
    },
    {
        name: "fb2k_playlist_add_handles",
        description: "Add tracks by handle list (no auto CUE expansion)",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                handles: {
                    type: "array",
                    description: "Track handle array (supports path|subsong:N format)",
                    items: { type: "string" },
                },
            },
            required: ["handles"],
        },
    },
    {
        name: "fb2k_playlist_add_paths_sequential",
        description: "Add file paths to a playlist sequentially",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                paths: {
                    type: "array",
                    description: "File path array",
                    items: { type: "string" },
                },
            },
            required: ["paths"],
        },
    },
    {
        name: "fb2k_playlist_add_paths_async",
        description: "Add file paths to a playlist asynchronously, notifies via playlist:addComplete event",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                paths: {
                    type: "array",
                    description: "File path array",
                    items: { type: "string" },
                },
            },
            required: ["paths"],
        },
    },
    {
        name: "fb2k_playlist_replace_all_and_play",
        description: "Atomic operation: clear playlist, add new tracks and play",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                paths: {
                    type: "array",
                    description: "File path array",
                    items: { type: "string" },
                },
                playIndex: {
                    type: "integer",
                    description: "Track index to play after adding, default 0",
                    minimum: 0,
                    default: 0,
                },
                stopFirst: {
                    type: "boolean",
                    description: "Stop current playback first, default true",
                    default: true,
                },
                autoPlay: {
                    type: "boolean",
                    description: "Auto-start playback, default true",
                    default: true,
                },
            },
            required: ["paths"],
        },
    },

    // ========== Selection ==========
    {
        name: "fb2k_playlist_get_selected_tracks",
        description: "Get info of all selected tracks in a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_get_selection",
        description: "Get indices of selected tracks in a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_set_selection",
        description: "Set selected items in a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                indices: {
                    type: "array",
                    description: "Track indices to select",
                    items: { type: "integer", minimum: 0 },
                },
                clearOthers: {
                    type: "boolean",
                    description: "Clear other selections, default true",
                    default: true,
                },
            },
            required: ["indices"],
        },
    },
    {
        name: "fb2k_playlist_select_all",
        description: "Select all tracks in a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_deselect_all",
        description: "Deselect all tracks in a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },

    // ========== Focus track ==========
    {
        name: "fb2k_playlist_get_focused_track",
        description: "Get focused track index in a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_set_focused_track",
        description: "Set focused track in a playlist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                index: {
                    type: "integer",
                    description: "Target track index",
                    minimum: 0,
                },
            },
            required: ["index"],
        },
    },
    {
        name: "fb2k_playlist_focus_track",
        description: "[Deprecated] Set focus track, use fb2k_playlist_set_focused_track instead",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                index: {
                    type: "integer",
                    description: "Target track index",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_get_focus_track",
        description: "[Deprecated] Get focus track index, use fb2k_playlist_get_focused_track instead",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },

    // ========== Undo / Redo ==========
    {
        name: "fb2k_playlist_undo",
        description: "Undo last playlist operation",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_redo",
        description: "Redo last undone playlist operation",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },

    // ========== Lock status ==========
    {
        name: "fb2k_playlist_get_lock_info",
        description: "Get playlist lock information",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_is_locked",
        description: "Check if a playlist is locked",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },

    // ========== Autoplaylist ==========
    {
        name: "fb2k_playlist_is_autoplaylist",
        description: "Check if a playlist is an autoplaylist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_create_autoplaylist",
        description: "Create an autoplaylist (smart playlist) that auto-populates based on a query",
        inputSchema: {
            type: "object",
            properties: {
                name: {
                    type: "string",
                    description: "Playlist name, default 'New Autoplaylist'",
                    default: "New Autoplaylist",
                },
                query: {
                    type: "string",
                    description: "foobar2000 query expression (e.g. 'artist IS Beatles')",
                },
                sort: {
                    type: "string",
                    description: "Sort format string, default empty",
                },
                keepSorted: {
                    type: "boolean",
                    description: "Keep sorted, default false",
                    default: false,
                },
            },
            required: ["query"],
        },
    },
    {
        name: "fb2k_playlist_convert_to_autoplaylist",
        description: "Convert an existing playlist to an autoplaylist",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
                query: {
                    type: "string",
                    description: "foobar2000 query expression",
                },
                sort: {
                    type: "string",
                    description: "Sort format string, default empty",
                },
                keepSorted: {
                    type: "boolean",
                    description: "Keep sorted, default false",
                    default: false,
                },
            },
            required: ["query"],
        },
    },
    {
        name: "fb2k_playlist_remove_autoplaylist",
        description: "Remove autoplaylist status (convert back to normal playlist)",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_get_autoplaylist_info",
        description: "Get autoplaylist details (flags, source, etc.)",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
    {
        name: "fb2k_playlist_get_autoplaylist_query",
        description: "Get autoplaylist query string",
        inputSchema: {
            type: "object",
            properties: {
                playlist: {
                    type: "integer",
                    description: "Playlist index, defaults to active",
                    minimum: 0,
                },
            },
        },
    },
];

export const playlistExtMethodMap: Record<string, string> = {
    // Playlist info
    fb2k_playlist_get_count: "playlist.getCount",
    fb2k_playlist_get_playing: "playlist.getPlaying",
    fb2k_playlist_get_track_count: "playlist.getTrackCount",
    fb2k_playlist_get_available_columns: "playlist.getAvailableColumns",
    // Playlist management
    fb2k_playlist_rename: "playlist.rename",
    fb2k_playlist_clear: "playlist.clear",
    fb2k_playlist_duplicate: "playlist.duplicate",
    fb2k_playlist_reorder_playlists: "playlist.reorderPlaylists",
    // Track operations
    fb2k_playlist_insert_tracks: "playlist.insertTracks",
    fb2k_playlist_remove_tracks: "playlist.removeTracks",
    fb2k_playlist_remove_selected_tracks: "playlist.removeSelectedTracks",
    fb2k_playlist_move_tracks: "playlist.moveTracks",
    fb2k_playlist_reorder: "playlist.reorder",
    fb2k_playlist_reverse: "playlist.reverse",
    fb2k_playlist_sort: "playlist.sort",
    fb2k_playlist_shuffle: "playlist.shuffle",
    // Add tracks
    fb2k_playlist_add_paths: "playlist.addPaths",
    fb2k_playlist_add_handles: "playlist.addHandles",
    fb2k_playlist_add_paths_sequential: "playlist.addPathsSequential",
    fb2k_playlist_add_paths_async: "playlist.addPathsAsync",
    fb2k_playlist_replace_all_and_play: "playlist.replaceAllAndPlay",
    // Selection
    fb2k_playlist_get_selected_tracks: "playlist.getSelectedTracks",
    fb2k_playlist_get_selection: "playlist.getSelection",
    fb2k_playlist_set_selection: "playlist.setSelection",
    fb2k_playlist_select_all: "playlist.selectAll",
    fb2k_playlist_deselect_all: "playlist.deselectAll",
    // Focus track
    fb2k_playlist_get_focused_track: "playlist.getFocusedTrack",
    fb2k_playlist_set_focused_track: "playlist.setFocusedTrack",
    fb2k_playlist_focus_track: "playlist.focusTrack",
    fb2k_playlist_get_focus_track: "playlist.getFocusTrack",
    // Undo / Redo
    fb2k_playlist_undo: "playlist.undo",
    fb2k_playlist_redo: "playlist.redo",
    // Lock status
    fb2k_playlist_get_lock_info: "playlist.getLockInfo",
    fb2k_playlist_is_locked: "playlist.isLocked",
    // Autoplaylist
    fb2k_playlist_is_autoplaylist: "playlist.isAutoplaylist",
    fb2k_playlist_create_autoplaylist: "playlist.createAutoplaylist",
    fb2k_playlist_convert_to_autoplaylist: "playlist.convertToAutoplaylist",
    fb2k_playlist_remove_autoplaylist: "playlist.removeAutoplaylist",
    fb2k_playlist_get_autoplaylist_info: "playlist.getAutoplaylistInfo",
    fb2k_playlist_get_autoplaylist_query: "playlist.getAutoplaylistQuery",
};
