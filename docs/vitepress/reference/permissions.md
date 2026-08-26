# Permissions reference

Path-bearing Bridge endpoints are validated by BridgeCore path-security specs before the handler body runs. Rejected requests never reach the filesystem or foobar2000 SDK path side effects. A path the policy refuses returns `PERMISSION_DENIED`; a parameter of the wrong shape or type returns `INVALID_PARAMS`.

Authority counts are taken from current `RegisterApi` path-security specs of the form `{ param, SecurityLevel::... }` in `src/api/**`:

| Level | Spec count | Meaning |
| --- | ---: | --- |
| `Read` | 10 | Ordinary filesystem read checks |
| `Write` | 1 | Strict write destinations (config/temp style policy) |
| `MediaRead` | 41 | Media-context read checks |
| `MediaWrite` | 10 | Media-context write checks |
| `FileWrite` | 11 | General file writes (`file.*`) |
| **Total** | **73** | **68 unique APIs** |

## Six-level model

| Level | Description | Validation summary |
| --- | --- | --- |
| `None` | No file-path parameter | No path validation |
| `Read` | Read-only filesystem operations | Blocks system protected directories, device paths, and `..` traversal |
| `Write` | Write destinations under the strict write policy | Allowed only under foobar2000 profile / temp destinations enforced by PathSecurity |
| `MediaRead` | Media metadata/content reads | Read rules first; media-library / playlist trust is a fallback used only when Read rejects the path |
| `MediaWrite` | Media mutation (tags, lyrics, artwork, counts) | Own chain: protected-directory blacklist, then strict write destination, media-library / playlist membership, media-library watch folders, or a sidecar sharing the directory of a trusted audio file. A non-system drive alone does **not** grant write access |
| `FileWrite` | General file writes (`file.*`) | Own chain, not a superset of `MediaWrite`: blacklist, then strict write destination, media-library watch folders, non-system drive (drive letter only, UNC excluded), or media-library / playlist membership — no sidecar step. The watch-folder and non-system-drive steps are what let `file.mkdir` and `file.write` create brand-new paths |

::: tip Level relationships
`None < Read < Write` forms the ordinary filesystem channel.
`None < Read < MediaRead < MediaWrite` forms the media channel.
`Write`, `MediaWrite` and `FileWrite` are independent write channels.
:::

::: warning FileWrite is broader than MediaWrite
`FileWrite` accepts any path on a non-system drive, and also any path inside a media-library watch folder (including on the system drive), so it is the widest write channel currently exposed. It exists because `file.*` operates on arbitrary files rather than on media-context files, and applying media-write rules would make creating new files or directories impossible. Treat it as the channel to audit first when reviewing a theme.
:::

## Error response

```json
{
  "success": false,
  "error": "file.read: path security denied for 'path': Access denied: protected system path",
  "code": "PERMISSION_DENIED"
}
```

The rejected path is never echoed back. The message names the method, the offending parameter (plus its index for array parameters, as in `items[2].destination`) and the policy reason, so a caller can tell which argument was refused without the host leaking a filesystem location into a payload a page may forward elsewhere.

```json
{
  "success": false,
  "error": "file.read: param 'path' must be a string",
  "code": "INVALID_PARAMS"
}
```

```javascript
const result = await fb2k.invoke('file.read', { path: somePath });
if (!result.success) {
  if (result.code === 'PERMISSION_DENIED') {
    console.warn('Path rejected by security policy:', result.error);
  } else if (result.code === 'INVALID_PARAMS') {
    console.warn('Parameter rejected before the handler ran:', result.error);
  }
}
```

## API permission matrix

### Read — filesystem read (10 specs)

| API | Parameter | Array | Nested key | Notes |
| --- | --- | --- | --- | --- |
| `artwork.getFolderImages` | `directory` | — | — | Runtime authority: `ArtworkApi.cpp` |
| `clipboard.writeFiles` | `paths` | yes | — | Runtime authority: `ClipboardApi.cpp` |
| `file.copy` | `source` | — | — | Runtime authority: `FileApi.cpp` |
| `file.copyAsync` | `items` | yes | `source` | Runtime authority: `FileApi.cpp` |
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

### MediaRead — media reads (41 specs)

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
| `metadata.probeBatchAsync` | `paths` | yes | — | Runtime authority: `MetadataApi.cpp` |
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

### MediaWrite — media mutation (10 specs)

| API | Parameter | Array | Nested key | Notes |
| --- | --- | --- | --- | --- |
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

### FileWrite — general file writes (11 specs)

| API | Parameter | Array | Nested key | Notes |
| --- | --- | --- | --- | --- |
| `file.copy` | `destination` | — | — | Runtime authority: `FileApi.cpp` |
| `file.copyAsync` | `items` | yes | `destination` | Runtime authority: `FileApi.cpp` |
| `file.delete` | `path` | — | — | Runtime authority: `FileApi.cpp` |
| `file.deleteAsync` | `paths` | yes | — | Runtime authority: `FileApi.cpp` |
| `file.mkdir` | `path` | — | — | Runtime authority: `FileApi.cpp` |
| `file.move` | `destination` | — | — | Runtime authority: `FileApi.cpp` |
| `file.move` | `source` | — | — | Runtime authority: `FileApi.cpp` |
| `file.moveAsync` | `items` | yes | `destination` | Runtime authority: `FileApi.cpp` |
| `file.moveAsync` | `items` | yes | `source` | Runtime authority: `FileApi.cpp` |
| `file.rename` | `path` | — | — | Runtime authority: `FileApi.cpp` |
| `file.write` | `path` | — | — | Runtime authority: `FileApi.cpp` |

`file.copy` validates `source` as `Read` and `destination` as `FileWrite`; `file.move` validates both endpoints as `FileWrite`. The asynchronous family follows the same split: `file.copyAsync` checks every `items[].source` as `Read` and every `items[].destination` as `FileWrite`, `file.moveAsync` checks both nested keys as `FileWrite`, and `file.deleteAsync` checks every entry of `paths` as `FileWrite`. Validation is fail-fast per call: one rejected entry fails the whole batch with `PERMISSION_DENIED` and no operation is dispatched.

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

The Read rules run first and the path is accepted as soon as they pass. A
non-system-drive path — including a UNC / NAS share — is therefore allowed
**without** any media-library or playlist lookup.

Media-context trust is a fallback, reached only when the Read rules reject the
path (in practice, a system-drive path outside the whitelist). Any one of these
admits the path:

- it is addable to the foobar2000 media library, or
- it resolves to a media library item, or
- it matches a playlist item. This lookup is served by a membership index that
  covers every playlist in full, so a genuine match is always found.

### MediaWrite

MediaWrite does not delegate to the Read or MediaRead entry points; it runs its
own chain. After the common rejections, a protected-directory blacklist check
always applies, then the first of these to match admits the path:

- a strict write destination (profile / temp), or
- a path in media-library / playlist context, or
- a path under a media-library watch folder, which covers files already on disk
  but not yet scanned into the library, or
- a file in the same directory as a trusted context media file, which is how
  sidecar writes such as `.lrc` are permitted.

The blacklist is what MediaWrite adds over MediaRead: a protected system
directory stays blocked even when the item does appear in a library or playlist.

Being on a non-system drive does **not** by itself admit a MediaWrite path. The
read policy allows non-system drives, but inheriting that on the write side
would mean any theme could rewrite arbitrary audio files on `D:` or `E:`.

### FileWrite

FileWrite runs its own chain and is not a superset of MediaWrite. After the
common rejections and the protected-directory blacklist, the first of these to
match admits the path: a strict write destination (profile / temp), a
media-library watch folder (`is_path_addable` — configuration, not membership,
so it admits brand-new paths and UNC folders), a non-system drive addressed by
drive letter (UNC excluded), or media-library / playlist context. MediaWrite's
sidecar step is absent here.

::: warning FileWrite accepts any non-system drive
This is the widest write channel currently exposed. `file.delete` and
`file.write` will therefore accept arbitrary destinations on `D:`, `E:` and so
on. The allowance exists because `file.mkdir` and `file.write` create paths that
by definition cannot already be in a library or playlist, so the membership
steps alone would reject every call. Watch folders are different: since
`is_path_addable` checks configuration rather than membership, new paths inside
a configured folder are admitted without needing this allowance.

**The test applies to the drive letter after canonicalization, not to the one in
the path as passed in.** If a user directory is redirected onto a non-system
drive by a junction or symbolic link (for example `C:\Users\<user>` →
`E:\Users\<user>`, common on machines whose user profiles have been relocated),
its entire subtree is evaluated in its `E:` form and therefore falls inside this
allowance — even though the caller passed a path beginning with `C:`. The
allowance is independent of file extension. When reviewing a theme, do not rely
on the literal drive letter; check how the target machine actually redirects
these directories.
:::

The spec counts at the top of this page are maintained by hand; the `RegisterApi` path-security specs in the component source are the authority.
