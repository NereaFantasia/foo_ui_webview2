# Permissions reference

Path-bearing Bridge endpoints are validated by BridgeCore path-security specs before the handler body runs. Rejected requests return `PERMISSION_DENIED` and never reach the filesystem or foobar2000 SDK path side effects.

Authority counts are taken from current `RegisterApi` path-security specs of the form `{ param, SecurityLevel::... }` in `src/api/**`:

| Level | Spec count | Meaning |
| --- | ---: | --- |
| `Read` | 9 | Ordinary filesystem read checks |
| `Write` | 1 | Strict write destinations (config/temp style policy) |
| `MediaRead` | 40 | Media-context read checks |
| `MediaWrite` | 17 | Media-context write checks |
| **Total** | **67** | **64 unique APIs** |

## Five-level model

| Level | Description | Validation summary |
| --- | --- | --- |
| `None` | No file-path parameter | No path validation |
| `Read` | Read-only filesystem operations | Blocks system protected directories, device paths, and `..` traversal |
| `Write` | Write destinations under the strict write policy | Allowed only under foobar2000 profile / temp destinations enforced by PathSecurity |
| `MediaRead` | Media metadata/content reads | Read rules plus media-library / playlist context trust |
| `MediaWrite` | Media mutation (tags, lyrics, artwork, counts) | MediaRead rules plus write blacklist and no free non-system-drive bypass |

::: tip Level relationships
`None < Read < Write` forms the ordinary filesystem channel.
`None < Read < MediaRead < MediaWrite` forms the media channel.
`Write` and `MediaWrite` are independent write channels.
:::

## Error response

```json
{
  "success": false,
  "error": "Path rejected by security policy: C:\\Windows\\System32\\config.ini",
  "code": "PERMISSION_DENIED"
}
```

```javascript
const result = await fb2k.invoke('file.read', { path: somePath });
if (!result.success && result.code === 'PERMISSION_DENIED') {
  console.warn('Path rejected by security policy:', result.error);
}
```

## API permission matrix

### Read — filesystem read (9 specs)

| API | Parameter | Array | Nested key | Notes |
| --- | --- | --- | --- | --- |
| `artwork.getFolderImages` | `directory` | — | — | Runtime authority: `ArtworkApi.cpp` |
| `clipboard.writeFiles` | `paths` | yes | — | Runtime authority: `ClipboardApi.cpp` |
| `file.copy` | `source` | — | — | Runtime authority: `FileApi.cpp` |
| `file.exists` | `path` | — | — | Runtime authority: `FileApi.cpp` |
| `file.getInfo` | `path` | — | — | Runtime authority: `FileApi.cpp` |
| `file.list` | `path` | — | — | Runtime authority: `FileApi.cpp` |
| `file.read` | `path` | — | — | Runtime authority: `FileApi.cpp` |
| `shell.openWith` | `path` | — | — | Runtime authority: `ShellApi.cpp` |
| `shell.showInExplorer` | `path` | — | — | Runtime authority: `ShellApi.cpp` |

### Write — strict write destinations (1 specs)

| API | Parameter | Array | Nested key | Notes |
| --- | --- | --- | --- | --- |
| `http.download` | `saveTo` | — | — | Runtime authority: `HttpApi.cpp` |

### MediaRead — media reads (40 specs)

| API | Parameter | Array | Nested key | Notes |
| --- | --- | --- | --- | --- |
| `artwork.getAvailableArtwork` | `path` | — | — | Runtime authority: `ArtworkApi.cpp` |
| `artwork.getAvailableTypes` | `path` | — | — | Runtime authority: `ArtworkApi.cpp` |
| `artwork.getBatch` | `paths` | yes | — | Runtime authority: `ArtworkApi.cpp` |
| `artwork.getByPath` | `path` | — | — | Runtime authority: `ArtworkApi.cpp` |
| `artwork.getFb2kUrlByPath` | `path` | — | — | Runtime authority: `ArtworkApi.cpp` |
| `artwork.getFb2kUrlByPathBatch` | `items` | yes | `path` | Runtime authority: `ArtworkApi.cpp` |
| `artwork.getFb2kUrlByPathBatch` | `paths` | yes | — | Runtime authority: `ArtworkApi.cpp` |
| `artwork.getForTrack` | `path` | — | — | Runtime authority: `ArtworkApi.cpp` |
| `artwork.getLyrics` | `path` | — | — | Runtime authority: `ArtworkApi.cpp` |
| `artwork.getMetadata` | `path` | — | — | Runtime authority: `ArtworkApi.cpp` |
| `audio.analyzeBPM` | `path` | — | — | Runtime authority: `AudioApi.cpp` |
| `audio.generateFullWaveform` | `path` | — | — | Runtime authority: `AudioApi.cpp` |
| `audio.generateWaveform` | `path` | — | — | Runtime authority: `AudioApi.cpp` |
| `discovery.executeContextMenuByPath` | `trackPath` | — | — | Runtime authority: `DiscoveryApi.cpp` |
| `jitQueue.enqueueNext` | `url` | — | — | Runtime authority: `QueueApi.cpp` |
| `jitQueue.playNow` | `url` | — | — | Runtime authority: `QueueApi.cpp` |
| `jitQueue.preloadBatch` | `urls` | yes | — | Runtime authority: `QueueApi.cpp` |
| `library.getByPath` | `path` | — | — | Runtime authority: `LibraryApi.cpp` |
| `lyrics.exists` | `path` | — | — | Runtime authority: `LyricsApi.cpp` |
| `lyrics.get` | `path` | — | — | Runtime authority: `LyricsApi.cpp` |
| `metadata.read` | `path` | — | — | Runtime authority: `MetadataApi.cpp` |
| `metadata.readBatch` | `paths` | yes | — | Runtime authority: `MetadataApi.cpp` |
| `metadata.readByPath` | `path` | — | — | Runtime authority: `MetadataApi.cpp` |
| `metadata.readRaw` | `path` | — | — | Runtime authority: `MetadataApi.cpp` |
| `playback.playPath` | `path` | — | — | Runtime authority: `PlaybackApi.cpp` |
| `playback.playPaths` | `paths` | yes | — | Runtime authority: `PlaybackApi.cpp` |
| `playcount.get` | `paths` | yes | — | Runtime authority: `PlaycountApi.cpp` |
| `playcount.getBatch` | `paths` | yes | — | Runtime authority: `PlaycountApi.cpp` |
| `playlist.addPaths` | `paths` | yes | — | Runtime authority: `PlaylistApi.cpp` |
| `playlist.addPathsAsync` | `paths` | yes | — | Runtime authority: `PlaylistApi.cpp` |
| `playlist.addPathsSequential` | `paths` | yes | — | Runtime authority: `PlaylistApi.cpp` |
| `playlist.replaceAllAndPlay` | `paths` | yes | — | Runtime authority: `PlaylistApi.cpp` |
| `queue.addPaths` | `paths` | yes | — | Runtime authority: `QueueApi.cpp` |
| `rating.get` | `path` | — | — | Runtime authority: `MetadataApi.cpp` |
| `replaygain.get` | `paths` | yes | — | Runtime authority: `ReplayGainApi.cpp` |
| `replaygain.scan` | `paths` | yes | — | Runtime authority: `ReplayGainApi.cpp` |
| `titleformat.eval` | `path` | — | — | Runtime authority: `TitleformatApi.cpp` |
| `titleformat.evalBatch` | `paths` | yes | — | Runtime authority: `TitleformatApi.cpp` |
| `titleformat.evalFields` | `path` | — | — | Runtime authority: `TitleformatApi.cpp` |
| `titleformat.evalFieldsBatch` | `paths` | yes | — | Runtime authority: `TitleformatApi.cpp` |

### MediaWrite — media mutation (17 specs)

| API | Parameter | Array | Nested key | Notes |
| --- | --- | --- | --- | --- |
| `file.copy` | `destination` | — | — | Runtime authority: `FileApi.cpp` |
| `file.delete` | `path` | — | — | Runtime authority: `FileApi.cpp` |
| `file.mkdir` | `path` | — | — | Runtime authority: `FileApi.cpp` |
| `file.move` | `destination` | — | — | Runtime authority: `FileApi.cpp` |
| `file.move` | `source` | — | — | Runtime authority: `FileApi.cpp` |
| `file.rename` | `path` | — | — | Runtime authority: `FileApi.cpp` |
| `file.write` | `path` | — | — | Runtime authority: `FileApi.cpp` |
| `lyrics.save` | `path` | — | — | Runtime authority: `LyricsApi.cpp` |
| `metadata.embedArtwork` | `path` | — | — | Runtime authority: `MetadataApi.cpp` |
| `metadata.removeEmbeddedArt` | `path` | — | — | Runtime authority: `MetadataApi.cpp` |
| `metadata.removeField` | `path` | — | — | Runtime authority: `MetadataApi.cpp` |
| `metadata.removeTag` | `path` | — | — | Runtime authority: `MetadataApi.cpp` |
| `metadata.write` | `path` | — | — | Runtime authority: `MetadataApi.cpp` |
| `metadata.writeBatch` | `items` | yes | `path` | Runtime authority: `MetadataApi.cpp` |
| `playcount.set` | `path` | — | — | Runtime authority: `PlaycountApi.cpp` |
| `rating.set` | `path` | — | — | Runtime authority: `MetadataApi.cpp` |
| `replaygain.clear` | `paths` | yes | — | Runtime authority: `ReplayGainApi.cpp` |

::: info Nested array validation
`metadata.writeBatch` validates each object in `items` by reading the nested `path` key.
:::

## Custom / non-decorator policy

These endpoints manage their own policy outside ordinary decorator specs:

| API | Policy notes |
| --- | --- |
| `shell.exec` | No executable whitelist; optional `cwd` still goes through PathSecurity |
| `shell.spawn` | No executable whitelist; absolute executable path and `cwd` are path-checked |
| `console.log` | Log directory restriction, reserved device names, and `.log` / `.txt` extension allowlist |
| `playlist.insertTracks` | Operates on playlist handles rather than raw filesystem paths |

## Path security details

### Common rejections

- Device paths: `\\.\...` and `\\?\...`
- Directory traversal containing `..`
- Empty or relative paths (absolute paths required)

### Read

System-drive protected directories include:

| Directory | Reason |
| --- | --- |
| `C:\\Windows\\` | OS files |
| `C:\\Program Files\\` | Installed programs |
| `C:\\Program Files (x86)\\` | 32-bit programs |
| `C:\\ProgramData\\` | System configuration data |

Non-system drives are generally allowed for portable / NAS media workflows under Read.

### Write

Only destinations accepted by the strict write policy succeed. In practice this is the foobar2000 profile directory and the system temporary directory.

### MediaRead

In addition to Read rules, the target must resolve into:

- the foobar2000 media library, or
- any playlist item (playlist scan is bounded by the runtime implementation).

### MediaWrite

In addition to MediaRead:

- system-protected directories remain blocked even if the item appears in a library/playlist context
- non-system-drive automatic bypass is not applied

## Counts summary

| Level | Specs | Unique APIs in this level table |
| --- | ---: | ---: |
| Read | 9 | 9 |
| Write | 1 | 1 |
| MediaRead | 40 | 39 |
| MediaWrite | 17 | 16 |
| **Total** | **67** | **64** |

These counts are regenerated from the C++ `RegisterApi` path-security specs in the component source.
