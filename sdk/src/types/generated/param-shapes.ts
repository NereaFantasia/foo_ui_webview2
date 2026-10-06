// ─────────────────────────────────────────────────────────────
// GENERATED FILE — DO NOT EDIT
// File: sdk/src/types/generated/param-shapes.ts
// Source: src/api/schema/*.ts
// Emitter: scripts/gen_sdk_types.mjs
// Regenerate: npm run gen:types (in sdk/) or `node scripts/gen_sdk_types.mjs --all`
// ─────────────────────────────────────────────────────────────

/* eslint-disable */

import type { ApiParamsMap } from './params.js';
/**
 * The keys a parameter object takes, as the host checks them before a handler runs.
 */
export interface ParamShape {
    /**
     * Every key the object takes. A shape describes the value, or each element when the value
     * is an array; a string names a shape in {@link PARAM_SHAPE_TYPES}; `null` means the value
     * is not checked further.
     */
    readonly keys: { readonly [key: string]: ParamShape | string | null };
    /** Keys that have to be present and not `null`. */
    readonly required?: readonly string[];
    /** Whether keys other than {@link keys} are taken as well, as by a map. */
    readonly open?: boolean;
}
/** Shapes of the parameter types that contain themselves, by name. */
export const PARAM_SHAPE_TYPES: { readonly [name: string]: ParamShape } = {
    "TrayMenuItem": {
        "keys": {
            "id": null,
            "label": null,
            "type": null,
            "enabled": null,
            "visible": null,
            "checked": null,
            "icon": null,
            "iconSvg": {
                "keys": {
                    "viewBox": null,
                    "content": null
                },
                "required": [
                    "viewBox",
                    "content"
                ]
            },
            "cover": null,
            "title": null,
            "subtitle": null,
            "value": null,
            "min": null,
            "max": null,
            "orientation": null,
            "segments": {
                "keys": {
                    "label": null,
                    "iconSvg": {
                        "keys": {
                            "viewBox": null,
                            "content": null
                        },
                        "required": [
                            "viewBox",
                            "content"
                        ]
                    },
                    "enabled": null
                }
            },
            "submenu": "TrayMenuItem",
            "playbackAction": null
        }
    },
    "UiMenuItem": {
        "keys": {
            "id": null,
            "label": null,
            "type": null,
            "enabled": null,
            "checked": null,
            "shortcut": null,
            "submenu": "UiMenuItem"
        }
    }
};
/** The shape of the parameter object of every declared method. */
export const API_PARAM_SHAPES: { readonly [M in keyof ApiParamsMap]: ParamShape } = {
    "artwork.getAvailableArtwork": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "artwork.getAvailableTypes": {
        "keys": {
            "path": null
        }
    },
    "artwork.getBatch": {
        "keys": {
            "paths": null,
            "type": null
        },
        "required": [
            "paths"
        ]
    },
    "artwork.getByPath": {
        "keys": {
            "path": null,
            "type": null
        },
        "required": [
            "path"
        ]
    },
    "artwork.getByPlaylistItem": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "index": null,
            "type": null
        }
    },
    "artwork.getCurrent": {
        "keys": {
            "type": null
        }
    },
    "artwork.getFb2kUrl": {
        "keys": {
            "type": null,
            "maxSize": null
        }
    },
    "artwork.getFb2kUrlByPath": {
        "keys": {
            "path": null,
            "type": null,
            "maxSize": null
        },
        "required": [
            "path"
        ]
    },
    "artwork.getFb2kUrlByPathBatch": {
        "keys": {
            "paths": null,
            "items": {
                "keys": {
                    "path": null,
                    "type": null,
                    "maxSize": null
                },
                "required": [
                    "path"
                ]
            },
            "type": null,
            "maxSize": null
        }
    },
    "artwork.getFolderImages": {
        "keys": {
            "directory": null
        },
        "required": [
            "directory"
        ]
    },
    "artwork.getForTrack": {
        "keys": {
            "path": null,
            "type": null
        },
        "required": [
            "path"
        ]
    },
    "artwork.getLyrics": {
        "keys": {
            "path": null
        }
    },
    "artwork.getMetadata": {
        "keys": {
            "path": null
        }
    },
    "audio.analyzeBPM": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "audio.cancelDecodePcm": {
        "keys": {
            "taskId": null
        },
        "required": [
            "taskId"
        ]
    },
    "audio.cancelFullWaveform": {
        "keys": {
            "taskId": null
        },
        "required": [
            "taskId"
        ]
    },
    "audio.decodePcm": {
        "keys": {
            "path": null,
            "cueIndex": null,
            "start": null,
            "end": null,
            "sampleRate": null,
            "mono": null
        },
        "required": [
            "path"
        ]
    },
    "audio.generateFullWaveform": {
        "keys": {
            "path": null,
            "cueIndex": null,
            "resolution": null,
            "method": null,
            "scale": null,
            "signed": null,
            "preferCache": null
        },
        "required": [
            "path"
        ]
    },
    "audio.getOutputInfo": {
        "keys": {}
    },
    "audio.getPcmDebugState": {
        "keys": {}
    },
    "audio.getSpectrum": {
        "keys": {
            "subscriptionId": null,
            "bands": null,
            "scale": null,
            "minFrequency": null,
            "maxFrequency": null,
            "output": null,
            "channels": null
        }
    },
    "audio.getSpectrumDebugState": {
        "keys": {}
    },
    "audio.getStreamInfo": {
        "keys": {}
    },
    "audio.getWaveform": {
        "keys": {
            "duration": null,
            "signed": null,
            "channels": null,
            "points": null
        }
    },
    "audio.isVisualizationAvailable": {
        "keys": {}
    },
    "audio.setChannelMode": {
        "keys": {
            "mode": null
        }
    },
    "audio.subscribeSpectrum": {
        "keys": {
            "subscriptionId": null,
            "fftSize": null,
            "bands": null,
            "fps": null,
            "scale": null,
            "output": null,
            "channels": null,
            "backgroundThrottle": null,
            "minFrequency": null,
            "maxFrequency": null,
            "event": null
        }
    },
    "audio.subscribeStream": {
        "keys": {
            "subscriptionId": null,
            "interval": null,
            "bufferSeconds": null
        }
    },
    "audio.unsubscribeSpectrum": {
        "keys": {
            "subscriptionId": null
        }
    },
    "audio.unsubscribeStream": {
        "keys": {
            "subscriptionId": null
        }
    },
    "clipboard.read": {
        "keys": {}
    },
    "clipboard.write": {
        "keys": {
            "text": null
        },
        "required": [
            "text"
        ]
    },
    "clipboard.writeFiles": {
        "keys": {
            "paths": null
        },
        "required": [
            "paths"
        ]
    },
    "clipboard.writeHTML": {
        "keys": {
            "html": null,
            "plainText": null
        },
        "required": [
            "html"
        ]
    },
    "config.export": {
        "keys": {}
    },
    "config.get": {
        "keys": {
            "key": null,
            "default": null
        },
        "required": [
            "key"
        ]
    },
    "config.getActiveDspPreset": {
        "keys": {}
    },
    "config.getAdvancedConfig": {
        "keys": {
            "parentGuid": null
        }
    },
    "config.getAdvancedConfigValue": {
        "keys": {
            "guid": null
        },
        "required": [
            "guid"
        ]
    },
    "config.getAll": {
        "keys": {}
    },
    "config.getComponents": {
        "keys": {}
    },
    "config.getCursorFollowPlayback": {
        "keys": {}
    },
    "config.getDspPresets": {
        "keys": {}
    },
    "config.getLibraryFilePatterns": {
        "keys": {}
    },
    "config.getLibraryStatus": {
        "keys": {}
    },
    "config.getOutputConfig": {
        "keys": {}
    },
    "config.getOutputDevices": {
        "keys": {}
    },
    "config.getPlaybackFollowCursor": {
        "keys": {}
    },
    "config.getPreferencesPages": {
        "keys": {}
    },
    "config.getPreferencesStandardGuids": {
        "keys": {}
    },
    "config.getReplaygainMode": {
        "keys": {}
    },
    "config.getVersionInfo": {
        "keys": {}
    },
    "config.remove": {
        "keys": {
            "key": null
        },
        "required": [
            "key"
        ]
    },
    "config.resetAdvancedConfig": {
        "keys": {
            "guid": null
        },
        "required": [
            "guid"
        ]
    },
    "config.set": {
        "keys": {
            "key": null,
            "value": null
        },
        "required": [
            "key",
            "value"
        ]
    },
    "config.setActiveDspPreset": {
        "keys": {
            "index": null
        },
        "required": [
            "index"
        ]
    },
    "config.setAdvancedConfigValue": {
        "keys": {
            "guid": null,
            "value": null
        },
        "required": [
            "guid",
            "value"
        ]
    },
    "config.setCursorFollowPlayback": {
        "keys": {
            "enabled": null
        },
        "required": [
            "enabled"
        ]
    },
    "config.setOutputBuffer": {
        "keys": {
            "bufferLength": null,
            "milliseconds": null
        }
    },
    "config.setOutputDevice": {
        "keys": {
            "outputId": null,
            "deviceId": null
        },
        "required": [
            "outputId",
            "deviceId"
        ]
    },
    "config.setPlaybackFollowCursor": {
        "keys": {
            "enabled": null
        },
        "required": [
            "enabled"
        ]
    },
    "config.setReplaygainMode": {
        "keys": {
            "mode": null,
            "sourceMode": null
        }
    },
    "config.showLibraryPreferences": {
        "keys": {}
    },
    "console.error": {
        "keys": {
            "message": null,
            "args": null
        }
    },
    "console.log": {
        "keys": {
            "message": null,
            "args": null
        }
    },
    "console.warn": {
        "keys": {
            "message": null,
            "args": null
        }
    },
    "cursor.isHidden": {
        "keys": {}
    },
    "cursor.setHidden": {
        "keys": {
            "hidden": null
        },
        "required": [
            "hidden"
        ]
    },
    "dialog.confirm": {
        "keys": {
            "title": null,
            "message": null,
            "type": null,
            "buttons": null,
            "defaultButton": null
        }
    },
    "dialog.openFile": {
        "keys": {
            "title": null,
            "multiple": null,
            "defaultPath": null,
            "filters": {
                "keys": {
                    "name": null,
                    "extensions": null
                }
            }
        }
    },
    "dialog.openFolder": {
        "keys": {
            "title": null,
            "defaultPath": null
        }
    },
    "dialog.saveFile": {
        "keys": {
            "title": null,
            "defaultName": null,
            "filters": {
                "keys": {
                    "name": null,
                    "extensions": null
                }
            }
        }
    },
    "discovery.executeContextMenuByPath": {
        "keys": {
            "path": null,
            "trackPath": null
        },
        "required": [
            "path"
        ]
    },
    "discovery.executeContextMenuCommand": {
        "keys": {
            "guid": null,
            "force": null
        },
        "required": [
            "guid"
        ]
    },
    "discovery.executeMainMenuCommand": {
        "keys": {
            "guid": null,
            "subGuid": null
        },
        "required": [
            "guid"
        ]
    },
    "discovery.getAllServices": {
        "keys": {}
    },
    "discovery.getComponents": {
        "keys": {}
    },
    "discovery.getContextMenuCommands": {
        "keys": {
            "includeHidden": null
        }
    },
    "discovery.getContextMenuTree": {
        "keys": {}
    },
    "discovery.getDspEntries": {
        "keys": {}
    },
    "discovery.getInputFormats": {
        "keys": {}
    },
    "discovery.getMainMenuCommands": {
        "keys": {
            "expandDynamic": null,
            "includeHidden": null
        }
    },
    "discovery.getMainMenuGroups": {
        "keys": {}
    },
    "discovery.getOutputDevices": {
        "keys": {}
    },
    "discovery.getPreferencePages": {
        "keys": {}
    },
    "discovery.getUIElements": {
        "keys": {}
    },
    "discovery.searchCommands": {
        "keys": {
            "query": null,
            "expandDynamic": null,
            "scope": null,
            "includeHidden": null
        },
        "required": [
            "query"
        ]
    },
    "dnd.getCapabilities": {
        "keys": {}
    },
    "dnd.getPathsAsync": {
        "keys": {
            "sessionId": null
        }
    },
    "dnd.prepareDrag": {
        "keys": {
            "paths": null
        },
        "required": [
            "paths"
        ]
    },
    "dnd.startDrag": {
        "keys": {}
    },
    "dsp.addDsp": {
        "keys": {
            "guid": null,
            "position": null
        },
        "required": [
            "guid"
        ]
    },
    "dsp.applyPreset": {
        "keys": {
            "index": null,
            "name": null
        }
    },
    "dsp.getAvailable": {
        "keys": {}
    },
    "dsp.getChain": {
        "keys": {}
    },
    "dsp.getPresets": {
        "keys": {}
    },
    "dsp.moveDsp": {
        "keys": {
            "from": null,
            "to": null
        },
        "required": [
            "from",
            "to"
        ]
    },
    "dsp.removeDsp": {
        "keys": {
            "index": null
        },
        "required": [
            "index"
        ]
    },
    "dsp.setChain": {
        "keys": {
            "dsps": {
                "keys": {
                    "guid": null
                },
                "required": [
                    "guid"
                ]
            }
        },
        "required": [
            "dsps"
        ]
    },
    "event.emit": {
        "keys": {
            "event": null,
            "payload": null,
            "excludeSelf": null
        },
        "required": [
            "event"
        ]
    },
    "event.emitTo": {
        "keys": {
            "event": null,
            "targetWindowId": null,
            "payload": null
        },
        "required": [
            "event",
            "targetWindowId"
        ]
    },
    "file.cancelOp": {
        "keys": {
            "operationId": null
        },
        "required": [
            "operationId"
        ]
    },
    "file.copy": {
        "keys": {
            "source": null,
            "destination": null,
            "overwrite": null
        },
        "required": [
            "source",
            "destination"
        ]
    },
    "file.copyAsync": {
        "keys": {
            "items": {
                "keys": {
                    "source": null,
                    "destination": null
                },
                "required": [
                    "source",
                    "destination"
                ]
            },
            "overwrite": null
        },
        "required": [
            "items"
        ]
    },
    "file.delete": {
        "keys": {
            "path": null,
            "moveToTrash": null
        },
        "required": [
            "path"
        ]
    },
    "file.deleteAsync": {
        "keys": {
            "paths": null,
            "moveToTrash": null
        },
        "required": [
            "paths"
        ]
    },
    "file.exists": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "file.getInfo": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "file.list": {
        "keys": {
            "path": null,
            "pattern": null,
            "recursive": null
        },
        "required": [
            "path"
        ]
    },
    "file.mkdir": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "file.move": {
        "keys": {
            "source": null,
            "destination": null
        },
        "required": [
            "source",
            "destination"
        ]
    },
    "file.moveAsync": {
        "keys": {
            "items": {
                "keys": {
                    "source": null,
                    "destination": null
                },
                "required": [
                    "source",
                    "destination"
                ]
            },
            "overwrite": null
        },
        "required": [
            "items"
        ]
    },
    "file.read": {
        "keys": {
            "path": null,
            "encoding": null
        },
        "required": [
            "path"
        ]
    },
    "file.rename": {
        "keys": {
            "path": null,
            "newName": null
        },
        "required": [
            "path",
            "newName"
        ]
    },
    "file.write": {
        "keys": {
            "path": null,
            "content": null,
            "encoding": null,
            "append": null,
            "atomic": null
        },
        "required": [
            "path"
        ]
    },
    "http.abort": {
        "keys": {
            "requestId": null
        },
        "required": [
            "requestId"
        ]
    },
    "http.delete": {
        "keys": {
            "url": null,
            "body": null,
            "headers": null,
            "timeout": null,
            "async": null,
            "redirect": null,
            "responseType": null,
            "insecureTls": null
        },
        "required": [
            "url"
        ]
    },
    "http.download": {
        "keys": {
            "url": null,
            "saveTo": null,
            "headers": null,
            "timeout": null,
            "async": null,
            "redirect": null,
            "insecureTls": null
        },
        "required": [
            "url",
            "saveTo"
        ]
    },
    "http.get": {
        "keys": {
            "url": null,
            "headers": null,
            "timeout": null,
            "async": null,
            "redirect": null,
            "responseType": null,
            "insecureTls": null
        },
        "required": [
            "url"
        ]
    },
    "http.head": {
        "keys": {
            "url": null,
            "headers": null,
            "timeout": null,
            "async": null,
            "redirect": null,
            "insecureTls": null
        },
        "required": [
            "url"
        ]
    },
    "http.patch": {
        "keys": {
            "url": null,
            "body": null,
            "headers": null,
            "timeout": null,
            "async": null,
            "redirect": null,
            "responseType": null,
            "insecureTls": null
        },
        "required": [
            "url"
        ]
    },
    "http.post": {
        "keys": {
            "url": null,
            "body": null,
            "headers": null,
            "timeout": null,
            "async": null,
            "redirect": null,
            "responseType": null,
            "insecureTls": null
        },
        "required": [
            "url"
        ]
    },
    "http.put": {
        "keys": {
            "url": null,
            "body": null,
            "headers": null,
            "timeout": null,
            "async": null,
            "redirect": null,
            "responseType": null,
            "insecureTls": null
        },
        "required": [
            "url"
        ]
    },
    "jitQueue.clear": {
        "keys": {}
    },
    "jitQueue.enqueueNext": {
        "keys": {
            "trackId": null,
            "title": null,
            "url": null
        },
        "required": [
            "trackId",
            "url"
        ]
    },
    "jitQueue.getState": {
        "keys": {}
    },
    "jitQueue.notifyEmpty": {
        "keys": {}
    },
    "jitQueue.playNow": {
        "keys": {
            "trackId": null,
            "title": null,
            "url": null
        },
        "required": [
            "trackId",
            "url"
        ]
    },
    "jitQueue.preloadBatch": {
        "keys": {
            "urls": null,
            "startIndex": null,
            "replace": null
        }
    },
    "jitQueue.skip": {
        "keys": {}
    },
    "jitQueue.stop": {
        "keys": {
            "clearBuffer": null
        }
    },
    "keyboard.getRegisteredHotkeys": {
        "keys": {}
    },
    "keyboard.registerHotkey": {
        "keys": {
            "key": null,
            "action": null,
            "global": null
        },
        "required": [
            "key",
            "action"
        ]
    },
    "keyboard.registerShortcut": {
        "keys": {
            "key": null,
            "action": null
        },
        "required": [
            "key",
            "action"
        ]
    },
    "keyboard.unregisterHotkey": {
        "keys": {
            "id": null,
            "key": null
        }
    },
    "library.addToPlaylist": {
        "keys": {
            "paths": null,
            "playlist": null,
            "playlistGuid": null
        },
        "required": [
            "paths"
        ]
    },
    "library.browseDirectory": {
        "keys": {
            "path": null,
            "includeFiles": null
        }
    },
    "library.browseTree": {
        "keys": {
            "rootId": null,
            "pathId": null,
            "includeFiles": null,
            "recursiveFiles": null
        },
        "required": [
            "rootId"
        ]
    },
    "library.getAlbums": {
        "keys": {
            "sort": null,
            "query": null,
            "offset": null,
            "limit": null,
            "includeTracks": null,
            "includeCover": null,
            "coverMaxSize": null,
            "useCache": null
        }
    },
    "library.getAlbumTracks": {
        "keys": {
            "album": null,
            "albumArtist": null
        },
        "required": [
            "album",
            "albumArtist"
        ]
    },
    "library.getAll": {
        "keys": {
            "offset": null,
            "limit": null,
            "useCache": null,
            "asyncResult": null
        }
    },
    "library.getArtistAlbums": {
        "keys": {
            "artist": null,
            "limit": null,
            "sort": null,
            "match": null
        },
        "required": [
            "artist"
        ]
    },
    "library.getArtists": {
        "keys": {
            "sort": null,
            "limit": null,
            "includeAlbums": null
        }
    },
    "library.getArtistTracks": {
        "keys": {
            "artist": null,
            "limit": null
        }
    },
    "library.getByPath": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "library.getCacheStats": {
        "keys": {}
    },
    "library.getCount": {
        "keys": {}
    },
    "library.getFieldValues": {
        "keys": {
            "field": null,
            "separator": null,
            "limit": null
        },
        "required": [
            "field"
        ]
    },
    "library.getGenres": {
        "keys": {}
    },
    "library.getRandomTracks": {
        "keys": {
            "count": null
        }
    },
    "library.getRecentlyAdded": {
        "keys": {
            "limit": null,
            "sortBy": null
        }
    },
    "library.getRoots": {
        "keys": {}
    },
    "library.getStats": {
        "keys": {}
    },
    "library.getStatus": {
        "keys": {}
    },
    "library.invalidateCache": {
        "keys": {}
    },
    "library.isEnabled": {
        "keys": {}
    },
    "library.query": {
        "keys": {
            "query": null,
            "sort": null,
            "limit": null,
            "fields": null
        },
        "required": [
            "query"
        ]
    },
    "library.refresh": {
        "keys": {}
    },
    "library.rescan": {
        "keys": {}
    },
    "library.search": {
        "keys": {
            "query": null,
            "offset": null,
            "limit": null,
            "fields": null
        }
    },
    "log.clear": {
        "keys": {}
    },
    "log.read": {
        "keys": {
            "lines": null
        }
    },
    "log.write": {
        "keys": {
            "message": null,
            "args": null,
            "level": null,
            "append": null,
            "timestamp": null,
            "file": null
        }
    },
    "lyrics.exists": {
        "keys": {
            "path": null,
            "filename": null
        },
        "required": [
            "path"
        ]
    },
    "lyrics.get": {
        "keys": {
            "path": null,
            "filename": null,
            "source": null,
            "type": null,
            "format": null
        }
    },
    "lyrics.save": {
        "keys": {
            "path": null,
            "lyrics": null,
            "target": null,
            "filename": null,
            "tagName": null,
            "format": null
        },
        "required": [
            "path",
            "lyrics"
        ]
    },
    "media.getContainerInfo": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "media.getStreamUrl": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "menu.close": {
        "keys": {
            "reason": null
        }
    },
    "menu.getContextMenu": {
        "keys": {
            "mode": null,
            "locale": null,
            "i18n": null,
            "withAvailability": null,
            "handles": null
        }
    },
    "menu.getMainMenu": {
        "keys": {
            "root": null,
            "locale": null,
            "i18n": null,
            "withAvailability": null
        }
    },
    "menu.runContextCommand": {
        "keys": {
            "command": null,
            "subGuid": null
        },
        "required": [
            "command"
        ]
    },
    "menu.runContextCommandById": {
        "keys": {
            "id": null,
            "mode": null,
            "handles": null
        },
        "required": [
            "id"
        ]
    },
    "menu.runMainMenuCommand": {
        "keys": {
            "command": null,
            "subGuid": null
        },
        "required": [
            "command"
        ]
    },
    "menu.show": {
        "keys": {
            "items": null,
            "x": null,
            "y": null,
            "windowModel": null,
            "css": null,
            "cssReplace": null,
            "backdrop": null,
            "backdropDarkMode": null,
            "closeAnimationMs": null
        }
    },
    "menu.showNativePopup": {
        "keys": {
            "mode": null,
            "handles": null,
            "x": null,
            "y": null
        }
    },
    "metadata.cancelProbe": {
        "keys": {
            "operationId": null
        },
        "required": [
            "operationId"
        ]
    },
    "metadata.embedArtwork": {
        "keys": {
            "path": null,
            "imageData": null,
            "type": null,
            "target": null,
            "filename": null
        },
        "required": [
            "path",
            "imageData"
        ]
    },
    "metadata.probeBatchAsync": {
        "keys": {
            "paths": null,
            "includeTags": null
        },
        "required": [
            "paths"
        ]
    },
    "metadata.read": {
        "keys": {
            "path": null,
            "cueIndex": null
        },
        "required": [
            "path"
        ]
    },
    "metadata.readBatch": {
        "keys": {
            "paths": null
        },
        "required": [
            "paths"
        ]
    },
    "metadata.readByPath": {
        "keys": {
            "path": null,
            "cueIndex": null
        },
        "required": [
            "path"
        ]
    },
    "metadata.readRaw": {
        "keys": {
            "path": null,
            "cueIndex": null
        },
        "required": [
            "path"
        ]
    },
    "metadata.removeEmbeddedArt": {
        "keys": {
            "path": null,
            "type": null,
            "removeAll": null
        },
        "required": [
            "path"
        ]
    },
    "metadata.removeField": {
        "keys": {
            "path": null,
            "tags": null,
            "cueIndex": null
        },
        "required": [
            "path",
            "tags"
        ]
    },
    "metadata.removeTag": {
        "keys": {
            "path": null,
            "tags": null,
            "cueIndex": null
        },
        "required": [
            "path",
            "tags"
        ]
    },
    "metadata.write": {
        "keys": {
            "path": null,
            "tags": null,
            "cueIndex": null
        },
        "required": [
            "path",
            "tags"
        ]
    },
    "metadata.writeBatch": {
        "keys": {
            "items": {
                "keys": {
                    "path": null,
                    "tags": null,
                    "cueIndex": null
                },
                "required": [
                    "path"
                ]
            }
        },
        "required": [
            "items"
        ]
    },
    "misc.exit": {
        "keys": {}
    },
    "misc.getComponentPath": {
        "keys": {}
    },
    "misc.getFoobarPath": {
        "keys": {}
    },
    "misc.getProfilePath": {
        "keys": {}
    },
    "misc.restart": {
        "keys": {}
    },
    "misc.showConsole": {
        "keys": {}
    },
    "misc.showLibrarySearch": {
        "keys": {
            "query": null
        }
    },
    "misc.showPopupMessage": {
        "keys": {
            "message": null,
            "title": null
        },
        "required": [
            "message"
        ]
    },
    "misc.showPreferences": {
        "keys": {}
    },
    "output.getDevices": {
        "keys": {}
    },
    "output.getEntries": {
        "keys": {}
    },
    "output.getSettings": {
        "keys": {}
    },
    "panel.getConfig": {
        "keys": {}
    },
    "panel.setConfig": {
        "keys": {
            "panelName": null,
            "transparentBackground": null,
            "grabFocus": null,
            "enableDragDrop": null
        }
    },
    "playback.getCurrentTrack": {
        "keys": {}
    },
    "playback.getCurrentTrackIndex": {
        "keys": {
            "includeTrackInfo": null
        }
    },
    "playback.getPlaybackOrder": {
        "keys": {}
    },
    "playback.getPlayingPlaylist": {
        "keys": {}
    },
    "playback.getPosition": {
        "keys": {}
    },
    "playback.getState": {
        "keys": {}
    },
    "playback.getStopAfterCurrent": {
        "keys": {}
    },
    "playback.getVolume": {
        "keys": {}
    },
    "playback.mute": {
        "keys": {
            "muted": null
        }
    },
    "playback.next": {
        "keys": {}
    },
    "playback.pause": {
        "keys": {}
    },
    "playback.play": {
        "keys": {}
    },
    "playback.playOrPause": {
        "keys": {}
    },
    "playback.playPath": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "playback.playPaths": {
        "keys": {
            "paths": null,
            "startIndex": null,
            "replace": null
        },
        "required": [
            "paths"
        ]
    },
    "playback.playPause": {
        "keys": {}
    },
    "playback.previous": {
        "keys": {}
    },
    "playback.random": {
        "keys": {}
    },
    "playback.setPlaybackOrder": {
        "keys": {
            "order": null,
            "name": null
        }
    },
    "playback.setPosition": {
        "keys": {
            "position": null
        },
        "required": [
            "position"
        ]
    },
    "playback.setStopAfterCurrent": {
        "keys": {
            "enabled": null
        },
        "required": [
            "enabled"
        ]
    },
    "playback.setVolume": {
        "keys": {
            "volume": null
        },
        "required": [
            "volume"
        ]
    },
    "playback.stop": {
        "keys": {}
    },
    "playback.toggleMute": {
        "keys": {}
    },
    "playback.toggleStopAfterCurrent": {
        "keys": {}
    },
    "playback.volumeDown": {
        "keys": {}
    },
    "playback.volumeUp": {
        "keys": {}
    },
    "playcount.get": {
        "keys": {
            "paths": null
        },
        "required": [
            "paths"
        ]
    },
    "playcount.getBatch": {
        "keys": {
            "paths": null
        },
        "required": [
            "paths"
        ]
    },
    "playcount.getStats": {
        "keys": {}
    },
    "playcount.set": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "playlist.addHandles": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "handles": null
        },
        "required": [
            "handles"
        ]
    },
    "playlist.addPaths": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "paths": null
        },
        "required": [
            "paths"
        ]
    },
    "playlist.addPathsAsync": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "paths": null
        },
        "required": [
            "paths"
        ]
    },
    "playlist.addPathsSequential": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "paths": null
        },
        "required": [
            "paths"
        ]
    },
    "playlist.clear": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.convertToAutoplaylist": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "query": null,
            "sort": null,
            "keepSorted": null
        },
        "required": [
            "query"
        ]
    },
    "playlist.create": {
        "keys": {
            "name": null,
            "position": null
        }
    },
    "playlist.createAutoplaylist": {
        "keys": {
            "name": null,
            "query": null,
            "sort": null,
            "keepSorted": null
        },
        "required": [
            "query"
        ]
    },
    "playlist.deselectAll": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.duplicate": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "name": null
        }
    },
    "playlist.focusTrack": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "index": null
        }
    },
    "playlist.getActive": {
        "keys": {}
    },
    "playlist.getAll": {
        "keys": {}
    },
    "playlist.getAutoplaylistInfo": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.getAutoplaylistQuery": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.getAvailableColumns": {
        "keys": {}
    },
    "playlist.getCount": {
        "keys": {}
    },
    "playlist.getFocusedTrack": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.getFocusTrack": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.getGroupRuns": {
        "keys": {
            "patterns": null,
            "playlist": null,
            "playlistGuid": null
        },
        "required": [
            "patterns"
        ]
    },
    "playlist.getLockInfo": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.getMatchingRows": {
        "keys": {
            "query": null,
            "playlist": null,
            "playlistGuid": null
        },
        "required": [
            "query"
        ]
    },
    "playlist.getPlaying": {
        "keys": {}
    },
    "playlist.getSelectedTracks": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.getSelection": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.getTrackCount": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.getTracks": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "start": null,
            "count": null,
            "formats": null,
            "fields": null
        }
    },
    "playlist.getTracksAt": {
        "keys": {
            "rows": null,
            "playlist": null,
            "playlistGuid": null,
            "formats": null,
            "fields": null
        },
        "required": [
            "rows"
        ]
    },
    "playlist.insertTracks": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "position": null,
            "handles": null
        },
        "required": [
            "handles"
        ]
    },
    "playlist.isAutoplaylist": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.isLocked": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.moveTracks": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "items": null,
            "delta": null
        },
        "required": [
            "delta"
        ]
    },
    "playlist.playTrack": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "index": null,
            "deferred": null,
            "muted": null
        }
    },
    "playlist.redo": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.remove": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.removeAutoplaylist": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.removeSelectedTracks": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.removeTracks": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "items": null
        },
        "required": [
            "items"
        ]
    },
    "playlist.rename": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "name": null
        },
        "required": [
            "name"
        ]
    },
    "playlist.reorder": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "newOrder": null
        },
        "required": [
            "newOrder"
        ]
    },
    "playlist.reorderPlaylists": {
        "keys": {
            "newOrder": null,
            "newOrderGuids": null
        }
    },
    "playlist.replaceAllAndPlay": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "paths": null,
            "playIndex": null,
            "stopFirst": null,
            "autoPlay": null
        },
        "required": [
            "paths"
        ]
    },
    "playlist.reverse": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.selectAll": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.setActive": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.setFocusedTrack": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "index": null
        }
    },
    "playlist.setSelection": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "indices": null,
            "clearOthers": null
        },
        "required": [
            "indices"
        ]
    },
    "playlist.shuffle": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "playlist.sort": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "pattern": null,
            "descending": null,
            "selectedOnly": null
        }
    },
    "playlist.undo": {
        "keys": {
            "playlist": null,
            "playlistGuid": null
        }
    },
    "port.connect": {
        "keys": {
            "name": null
        },
        "required": [
            "name"
        ]
    },
    "port.disconnect": {
        "keys": {
            "portId": null
        },
        "required": [
            "portId"
        ]
    },
    "port.getPorts": {
        "keys": {
            "name": null
        }
    },
    "port.postMessage": {
        "keys": {
            "portId": null,
            "message": null
        },
        "required": [
            "portId",
            "message"
        ]
    },
    "port.postMessageTo": {
        "keys": {
            "portId": null,
            "targetPortId": null,
            "message": null
        },
        "required": [
            "portId",
            "targetPortId",
            "message"
        ]
    },
    "queue.add": {
        "keys": {
            "playlist": null,
            "playlistGuid": null,
            "tracks": null,
            "track": null
        }
    },
    "queue.addPaths": {
        "keys": {
            "paths": null,
            "useQueuePlaylist": null,
            "playlist": null,
            "playlistGuid": null
        },
        "required": [
            "paths"
        ]
    },
    "queue.clear": {
        "keys": {}
    },
    "queue.flush": {
        "keys": {}
    },
    "queue.get": {
        "keys": {}
    },
    "queue.getCount": {
        "keys": {}
    },
    "queue.insertNext": {
        "keys": {
            "paths": null,
            "items": {
                "keys": {
                    "playlist": null,
                    "playlistGuid": null,
                    "item": null
                },
                "required": [
                    "item"
                ]
            },
            "position": null
        }
    },
    "queue.moveToTop": {
        "keys": {
            "index": null
        },
        "required": [
            "index"
        ]
    },
    "queue.playNow": {
        "keys": {
            "index": null
        }
    },
    "queue.remove": {
        "keys": {
            "index": null,
            "indices": null
        }
    },
    "queue.setContents": {
        "keys": {
            "items": {
                "keys": {
                    "queueIndex": null,
                    "playlist": null,
                    "playlistGuid": null,
                    "item": null
                }
            }
        },
        "required": [
            "items"
        ]
    },
    "rating.get": {
        "keys": {
            "path": null,
            "cueIndex": null
        },
        "required": [
            "path"
        ]
    },
    "rating.set": {
        "keys": {
            "path": null,
            "rating": null,
            "cueIndex": null
        },
        "required": [
            "rating"
        ]
    },
    "replaygain.clear": {
        "keys": {
            "paths": null
        },
        "required": [
            "paths"
        ]
    },
    "replaygain.get": {
        "keys": {
            "paths": null
        },
        "required": [
            "paths"
        ]
    },
    "replaygain.getMode": {
        "keys": {}
    },
    "replaygain.getPreamp": {
        "keys": {}
    },
    "replaygain.getSettings": {
        "keys": {}
    },
    "replaygain.scan": {
        "keys": {
            "paths": null,
            "mode": null
        },
        "required": [
            "paths"
        ]
    },
    "replaygain.setMode": {
        "keys": {
            "sourceMode": null,
            "processingMode": null
        }
    },
    "replaygain.setPreamp": {
        "keys": {
            "withRg": null,
            "withoutRg": null
        }
    },
    "selection.get": {
        "keys": {
            "offset": null,
            "limit": null
        }
    },
    "selection.getType": {
        "keys": {}
    },
    "selection.getViewerMode": {
        "keys": {}
    },
    "selection.getViewingTrack": {
        "keys": {
            "includeTrackInfo": null
        }
    },
    "selection.set": {
        "keys": {
            "handles": null
        },
        "required": [
            "handles"
        ]
    },
    "selection.setPlaylistTracking": {
        "keys": {
            "mode": null
        }
    },
    "shell.exec": {
        "keys": {
            "command": null,
            "args": null,
            "cwd": null,
            "hidden": null
        },
        "required": [
            "command"
        ]
    },
    "shell.openExternal": {
        "keys": {
            "url": null
        },
        "required": [
            "url"
        ]
    },
    "shell.openWith": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "shell.showInExplorer": {
        "keys": {
            "path": null
        },
        "required": [
            "path"
        ]
    },
    "shell.spawn": {
        "keys": {
            "executable": null,
            "args": null,
            "cwd": null,
            "hidden": null,
            "waitForExitMs": null
        },
        "required": [
            "executable"
        ]
    },
    "state.delete": {
        "keys": {
            "key": null
        },
        "required": [
            "key"
        ]
    },
    "state.get": {
        "keys": {
            "key": null
        },
        "required": [
            "key"
        ]
    },
    "state.keys": {
        "keys": {
            "pattern": null
        }
    },
    "state.set": {
        "keys": {
            "key": null,
            "value": null,
            "silent": null,
            "ttlMs": null
        },
        "required": [
            "key",
            "value"
        ]
    },
    "system.getApisByNamespace": {
        "keys": {
            "namespace": null
        },
        "required": [
            "namespace"
        ]
    },
    "system.getApiStats": {
        "keys": {}
    },
    "system.getDPI": {
        "keys": {}
    },
    "system.getLocale": {
        "keys": {}
    },
    "system.getRegisteredPlugins": {
        "keys": {}
    },
    "system.getTheme": {
        "keys": {}
    },
    "system.isPluginRegistered": {
        "keys": {
            "namespace": null
        },
        "required": [
            "namespace"
        ]
    },
    "system.listAvailableApis": {
        "keys": {
            "includeInternal": null,
            "includeExternal": null
        }
    },
    "system.searchApis": {
        "keys": {
            "query": null
        },
        "required": [
            "query"
        ]
    },
    "taskbar.flash": {
        "keys": {
            "count": null,
            "interval": null
        }
    },
    "taskbar.setOverlayIcon": {
        "keys": {
            "icon": null,
            "description": null
        }
    },
    "taskbar.setProgress": {
        "keys": {
            "state": null,
            "value": null
        }
    },
    "taskbar.setThumbnailButtons": {
        "keys": {
            "buttons": {
                "keys": {
                    "id": null,
                    "icon": null,
                    "tooltip": null,
                    "enabled": null,
                    "visible": null,
                    "dismissOnClick": null
                },
                "required": [
                    "id"
                ]
            }
        },
        "required": [
            "buttons"
        ]
    },
    "taskbar.updateButton": {
        "keys": {
            "id": null,
            "icon": null,
            "tooltip": null,
            "enabled": null,
            "visible": null
        },
        "required": [
            "id"
        ]
    },
    "titleformat.eval": {
        "keys": {
            "path": null,
            "pattern": null
        },
        "required": [
            "pattern"
        ]
    },
    "titleformat.evalBatch": {
        "keys": {
            "paths": null,
            "pattern": null
        },
        "required": [
            "paths",
            "pattern"
        ]
    },
    "titleformat.evalFields": {
        "keys": {
            "path": null,
            "fields": null
        },
        "required": [
            "path",
            "fields"
        ]
    },
    "titleformat.evalFieldsBatch": {
        "keys": {
            "paths": null,
            "fields": null
        },
        "required": [
            "paths",
            "fields"
        ]
    },
    "titleformat.getBuiltinFields": {
        "keys": {}
    },
    "tray.appendMenuItems": {
        "keys": {
            "items": {
                "keys": {
                    "id": null,
                    "label": null,
                    "type": null,
                    "enabled": null,
                    "visible": null,
                    "checked": null,
                    "icon": null,
                    "iconSvg": {
                        "keys": {
                            "viewBox": null,
                            "content": null
                        },
                        "required": [
                            "viewBox",
                            "content"
                        ]
                    },
                    "cover": null,
                    "title": null,
                    "subtitle": null,
                    "value": null,
                    "min": null,
                    "max": null,
                    "orientation": null,
                    "segments": {
                        "keys": {
                            "label": null,
                            "iconSvg": {
                                "keys": {
                                    "viewBox": null,
                                    "content": null
                                },
                                "required": [
                                    "viewBox",
                                    "content"
                                ]
                            },
                            "enabled": null
                        }
                    },
                    "submenu": "TrayMenuItem",
                    "playbackAction": null
                }
            },
            "position": null
        },
        "required": [
            "items"
        ]
    },
    "tray.clearMenuItems": {
        "keys": {
            "position": null
        }
    },
    "tray.create": {
        "keys": {
            "icon": null,
            "tooltip": null
        }
    },
    "tray.destroy": {
        "keys": {}
    },
    "tray.getMenuItems": {
        "keys": {}
    },
    "tray.isVisible": {
        "keys": {}
    },
    "tray.removeMenuItems": {
        "keys": {
            "ids": null
        },
        "required": [
            "ids"
        ]
    },
    "tray.setCloseToTray": {
        "keys": {
            "enabled": null
        },
        "required": [
            "enabled"
        ]
    },
    "tray.setContextMenu": {
        "keys": {
            "items": {
                "keys": {
                    "id": null,
                    "label": null,
                    "type": null,
                    "enabled": null,
                    "visible": null,
                    "checked": null,
                    "icon": null,
                    "iconSvg": {
                        "keys": {
                            "viewBox": null,
                            "content": null
                        },
                        "required": [
                            "viewBox",
                            "content"
                        ]
                    },
                    "cover": null,
                    "title": null,
                    "subtitle": null,
                    "value": null,
                    "min": null,
                    "max": null,
                    "orientation": null,
                    "segments": {
                        "keys": {
                            "label": null,
                            "iconSvg": {
                                "keys": {
                                    "viewBox": null,
                                    "content": null
                                },
                                "required": [
                                    "viewBox",
                                    "content"
                                ]
                            },
                            "enabled": null
                        }
                    },
                    "submenu": "TrayMenuItem",
                    "playbackAction": null
                }
            },
            "config": {
                "keys": {
                    "showPlaybackControls": null,
                    "showSystemItems": null,
                    "customPosition": null,
                    "render": null,
                    "autoNowPlaying": null,
                    "css": null,
                    "cssReplace": null,
                    "backdrop": null,
                    "backdropDarkMode": null,
                    "closeAnimationMs": null,
                    "layoutMode": null
                }
            }
        },
        "required": [
            "items"
        ]
    },
    "tray.setIcon": {
        "keys": {
            "icon": null
        }
    },
    "tray.setMenuItemState": {
        "keys": {
            "id": null,
            "checked": null,
            "enabled": null
        },
        "required": [
            "id"
        ]
    },
    "tray.setMenuZones": {
        "keys": {
            "top": {
                "keys": {
                    "id": null,
                    "label": null,
                    "type": null,
                    "enabled": null,
                    "visible": null,
                    "checked": null,
                    "icon": null,
                    "iconSvg": {
                        "keys": {
                            "viewBox": null,
                            "content": null
                        },
                        "required": [
                            "viewBox",
                            "content"
                        ]
                    },
                    "cover": null,
                    "title": null,
                    "subtitle": null,
                    "value": null,
                    "min": null,
                    "max": null,
                    "orientation": null,
                    "segments": {
                        "keys": {
                            "label": null,
                            "iconSvg": {
                                "keys": {
                                    "viewBox": null,
                                    "content": null
                                },
                                "required": [
                                    "viewBox",
                                    "content"
                                ]
                            },
                            "enabled": null
                        }
                    },
                    "submenu": "TrayMenuItem",
                    "playbackAction": null
                }
            },
            "playback": {
                "keys": {
                    "id": null,
                    "label": null,
                    "type": null,
                    "enabled": null,
                    "visible": null,
                    "checked": null,
                    "icon": null,
                    "iconSvg": {
                        "keys": {
                            "viewBox": null,
                            "content": null
                        },
                        "required": [
                            "viewBox",
                            "content"
                        ]
                    },
                    "cover": null,
                    "title": null,
                    "subtitle": null,
                    "value": null,
                    "min": null,
                    "max": null,
                    "orientation": null,
                    "segments": {
                        "keys": {
                            "label": null,
                            "iconSvg": {
                                "keys": {
                                    "viewBox": null,
                                    "content": null
                                },
                                "required": [
                                    "viewBox",
                                    "content"
                                ]
                            },
                            "enabled": null
                        }
                    },
                    "submenu": "TrayMenuItem",
                    "playbackAction": null
                }
            },
            "bottom": {
                "keys": {
                    "id": null,
                    "label": null,
                    "type": null,
                    "enabled": null,
                    "visible": null,
                    "checked": null,
                    "icon": null,
                    "iconSvg": {
                        "keys": {
                            "viewBox": null,
                            "content": null
                        },
                        "required": [
                            "viewBox",
                            "content"
                        ]
                    },
                    "cover": null,
                    "title": null,
                    "subtitle": null,
                    "value": null,
                    "min": null,
                    "max": null,
                    "orientation": null,
                    "segments": {
                        "keys": {
                            "label": null,
                            "iconSvg": {
                                "keys": {
                                    "viewBox": null,
                                    "content": null
                                },
                                "required": [
                                    "viewBox",
                                    "content"
                                ]
                            },
                            "enabled": null
                        }
                    },
                    "submenu": "TrayMenuItem",
                    "playbackAction": null
                }
            },
            "config": {
                "keys": {
                    "showPlaybackControls": null,
                    "showSystemItems": null,
                    "customPosition": null,
                    "render": null,
                    "autoNowPlaying": null,
                    "css": null,
                    "cssReplace": null,
                    "backdrop": null,
                    "backdropDarkMode": null,
                    "closeAnimationMs": null,
                    "layoutMode": null
                }
            }
        }
    },
    "tray.setMinimizeToTray": {
        "keys": {
            "enabled": null
        },
        "required": [
            "enabled"
        ]
    },
    "tray.setTooltip": {
        "keys": {
            "tooltip": null
        }
    },
    "tray.showBalloon": {
        "keys": {
            "title": null,
            "message": null,
            "icon": null
        }
    },
    "ui.hideNotification": {
        "keys": {}
    },
    "ui.showContextMenu": {
        "keys": {
            "x": null,
            "y": null
        }
    },
    "ui.showCustomMenu": {
        "keys": {
            "items": {
                "keys": {
                    "id": null,
                    "label": null,
                    "type": null,
                    "enabled": null,
                    "checked": null,
                    "shortcut": null,
                    "submenu": "UiMenuItem"
                }
            },
            "x": null,
            "y": null,
            "w": null,
            "h": null,
            "suppressDefault": null
        },
        "required": [
            "items"
        ]
    },
    "ui.showNotification": {
        "keys": {
            "title": null,
            "body": null,
            "silent": null,
            "timeout": null
        }
    },
    "ui.showToast": {
        "keys": {
            "message": null,
            "duration": null,
            "type": null,
            "position": null
        },
        "required": [
            "message"
        ]
    },
    "webview.getSource": {
        "keys": {}
    },
    "window.blur": {
        "keys": {}
    },
    "window.broadcast": {
        "keys": {
            "message": null
        },
        "required": [
            "message"
        ]
    },
    "window.cancelClose": {
        "keys": {}
    },
    "window.center": {
        "keys": {}
    },
    "window.clearClickThroughExcludeRegions": {
        "keys": {
            "windowId": null
        }
    },
    "window.clearDragRegions": {
        "keys": {}
    },
    "window.clearNoDragRegions": {
        "keys": {}
    },
    "window.close": {
        "keys": {}
    },
    "window.closeAllPopups": {
        "keys": {}
    },
    "window.closePopup": {
        "keys": {
            "windowId": null
        },
        "required": [
            "windowId"
        ]
    },
    "window.confirmClose": {
        "keys": {}
    },
    "window.createPopup": {
        "keys": {
            "url": null,
            "title": null,
            "x": null,
            "y": null,
            "width": null,
            "height": null,
            "minWidth": null,
            "minHeight": null,
            "maxWidth": null,
            "maxHeight": null,
            "resizable": null,
            "frame": null,
            "transparent": null,
            "alwaysOnTop": null,
            "showInTaskbar": null,
            "clickThrough": null,
            "beforeClose": null,
            "profile": null,
            "behavior": null,
            "backdropPolicy": null
        }
    },
    "window.enterFullscreen": {
        "keys": {
            "windowId": null
        }
    },
    "window.exitFullscreen": {
        "keys": {
            "windowId": null
        }
    },
    "window.flash": {
        "keys": {
            "enabled": null,
            "count": null
        }
    },
    "window.flashTaskbar": {
        "keys": {
            "count": null
        }
    },
    "window.focus": {
        "keys": {
            "windowId": null
        }
    },
    "window.getAllWindows": {
        "keys": {}
    },
    "window.getBackdropPolicy": {
        "keys": {
            "windowId": null
        }
    },
    "window.getBounds": {
        "keys": {}
    },
    "window.getCaptionButtonsWidth": {
        "keys": {}
    },
    "window.getCornerPreference": {
        "keys": {}
    },
    "window.getCurrentWindowId": {
        "keys": {}
    },
    "window.getDevServerConfig": {
        "keys": {}
    },
    "window.getDpiScale": {
        "keys": {}
    },
    "window.getMaxSize": {
        "keys": {
            "windowId": null
        }
    },
    "window.getMinSize": {
        "keys": {
            "windowId": null
        }
    },
    "window.getMode": {
        "keys": {}
    },
    "window.getPopupBehavior": {
        "keys": {
            "windowId": null
        }
    },
    "window.getState": {
        "keys": {}
    },
    "window.getTitle": {
        "keys": {}
    },
    "window.getTitlebarHeight": {
        "keys": {}
    },
    "window.getTitlebarInfo": {
        "keys": {}
    },
    "window.getZoom": {
        "keys": {}
    },
    "window.hasSavedBounds": {
        "keys": {}
    },
    "window.isAlwaysOnTop": {
        "keys": {}
    },
    "window.isClickThrough": {
        "keys": {
            "windowId": null
        }
    },
    "window.isFullscreen": {
        "keys": {
            "windowId": null
        }
    },
    "window.isMaximized": {
        "keys": {}
    },
    "window.isMinimized": {
        "keys": {}
    },
    "window.isResizable": {
        "keys": {
            "windowId": null
        }
    },
    "window.maximize": {
        "keys": {}
    },
    "window.minimize": {
        "keys": {}
    },
    "window.refreshWebView": {
        "keys": {}
    },
    "window.reload": {
        "keys": {}
    },
    "window.resetZoom": {
        "keys": {}
    },
    "window.restore": {
        "keys": {}
    },
    "window.sendMessage": {
        "keys": {
            "targetWindowId": null,
            "message": null
        },
        "required": [
            "targetWindowId",
            "message"
        ]
    },
    "window.setAcrylic": {
        "keys": {
            "windowId": null,
            "enabled": null,
            "darkMode": null
        }
    },
    "window.setAlwaysOnTop": {
        "keys": {
            "enabled": null
        }
    },
    "window.setBackdropPolicy": {
        "keys": {
            "windowId": null,
            "backdropPolicy": null
        },
        "required": [
            "backdropPolicy"
        ]
    },
    "window.setBackgroundTransparency": {
        "keys": {
            "windowId": null,
            "transparent": null
        }
    },
    "window.setBlur": {
        "keys": {
            "windowId": null,
            "enabled": null
        }
    },
    "window.setBounds": {
        "keys": {
            "x": null,
            "y": null,
            "width": null,
            "height": null
        }
    },
    "window.setClickThrough": {
        "keys": {
            "windowId": null,
            "enabled": null
        }
    },
    "window.setClickThroughExcludeRegions": {
        "keys": {
            "windowId": null,
            "regions": {
                "keys": {
                    "x": null,
                    "y": null,
                    "width": null,
                    "height": null
                }
            }
        }
    },
    "window.setCornerPreference": {
        "keys": {
            "mode": null
        }
    },
    "window.setDarkMode": {
        "keys": {
            "windowId": null,
            "enabled": null
        }
    },
    "window.setDevServerConfig": {
        "keys": {
            "useDevServer": null,
            "devServerUrl": null
        }
    },
    "window.setDragRegions": {
        "keys": {
            "regions": {
                "keys": {
                    "x": null,
                    "y": null,
                    "width": null,
                    "height": null
                }
            }
        }
    },
    "window.setFrameless": {
        "keys": {
            "windowId": null,
            "frameless": null
        }
    },
    "window.setFullscreen": {
        "keys": {
            "windowId": null,
            "enabled": null
        }
    },
    "window.setMaximizeButtonRegion": {
        "keys": {
            "region": {
                "keys": {
                    "x": null,
                    "y": null,
                    "width": null,
                    "height": null
                }
            }
        }
    },
    "window.setMaxSize": {
        "keys": {
            "windowId": null,
            "width": null,
            "height": null
        }
    },
    "window.setMica": {
        "keys": {
            "windowId": null,
            "enabled": null,
            "variant": null,
            "darkMode": null
        }
    },
    "window.setMicaEffect": {
        "keys": {
            "windowId": null,
            "enabled": null,
            "variant": null,
            "darkMode": null
        }
    },
    "window.setMinSize": {
        "keys": {
            "windowId": null,
            "width": null,
            "height": null
        }
    },
    "window.setNoDragRegions": {
        "keys": {
            "regions": {
                "keys": {
                    "x": null,
                    "y": null,
                    "width": null,
                    "height": null
                }
            }
        }
    },
    "window.setPopupBehavior": {
        "keys": {
            "windowId": null,
            "profile": null,
            "behavior": null
        }
    },
    "window.setPosition": {
        "keys": {
            "x": null,
            "y": null
        }
    },
    "window.setResizable": {
        "keys": {
            "windowId": null,
            "resizable": null
        }
    },
    "window.setSize": {
        "keys": {
            "width": null,
            "height": null
        }
    },
    "window.setTitle": {
        "keys": {
            "title": null
        }
    },
    "window.setTitlebarHeight": {
        "keys": {
            "height": null
        }
    },
    "window.setZoom": {
        "keys": {
            "zoom": null
        }
    },
    "window.setZoomForDpi": {
        "keys": {
            "dpi": null
        }
    },
    "window.showSystemMenu": {
        "keys": {
            "x": null,
            "y": null,
            "w": null,
            "h": null
        }
    },
    "window.startDrag": {
        "keys": {}
    },
    "window.startResize": {
        "keys": {
            "edge": null
        }
    },
    "window.toggleAlwaysOnTop": {
        "keys": {}
    },
    "window.toggleFullscreen": {
        "keys": {
            "windowId": null
        }
    },
    "window.toggleMaximize": {
        "keys": {}
    }
};
