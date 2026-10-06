# Media API

The experimental `media` namespace gives the calling page access to local media bytes and container metadata. Decoding remains the browser's responsibility. See [SDK media helpers](../sdk/media.md) for muted playback that follows foobar2000.

## media.getStreamUrl

<!-- api-schema:begin media.getStreamUrl -->
Experimental API; it may change in future releases.

Issue a URL for a local media file, usable by the calling document until navigation, file modification or eviction. Use crossorigin="anonymous" on media elements. Requires a trusted HTTP(S) document and WebView2 ICoreWebView2_22; older runtimes return NOT_SUPPORTED. GET supports one byte range, returning at most 2 MiB. GET without Range rejects files larger than 2 MiB with HTTP 416. HEAD returns metadata without a body. Only native local paths, UNC, foobar2000 file:// paths and file-relative:// are accepted. A subsong suffix selects the container, not a chapter timeline. Network and archive protocols fail with INVALID_PARAMS; denied files with PERMISSION_DENIED; missing files with NOT_FOUND. File IO or resource limits fail with OPERATION_FAILED.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Local container path; a subsong suffix is stripped before file access. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `url` | `string` | Opaque URL bound to the calling document. |
| `size` | `integer` | File size in bytes, within JavaScript's safe integer range. |
| `mimeType` | `string` | MIME type identified from file content; application/octet-stream when unknown. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Use the URL only in the document that requested it. Reloading, navigation (including a cancelled navigation), closing the window or changing the file invalidates it. Up to 64 URLs are retained per document; issuing more evicts the least recently used. There is no fixed time-to-live. URLs contain an opaque random token, not a file path.

The page must have a trusted HTTP(S) origin. Set `crossOrigin = 'anonymous'` **before** assigning `src`. Opaque origins such as `about:blank` and `file:` cannot identify the issuing page for CORS and cannot issue a media URL. This does not change their eligibility for other bridge calls.

| Request | Response |
| --- | --- |
| GET with one valid byte range | 206, at most 2 MiB; the browser can request the next segment |
| GET without Range, file at most 2 MiB | 200 with the whole file |
| GET without Range for a larger file, multiple ranges or an unsatisfiable range | 416 with `Content-Range: bytes */size` |
| HEAD | 200 with full file size, no body |
| OPTIONS | Checks the specific requesting origin, GET/HEAD and the Range header |
| Unknown token, another document or denied access | 403 |
| Issued file removed or its size/mtime changed | 410; request another URL |
| Queue full, shutdown or a persistent sharing conflict | 503 |

Responses use `Cache-Control: no-store`, a size/mtime ETag, and byte-range headers. CORS permits the specific trusted issuing origin, including on error responses. A URL cannot be transferred to another window. Permissions are checked again on every request and before returning bytes.

## media.getContainerInfo

<!-- api-schema:begin media.getContainerInfo -->
Experimental API; it may change in future releases.

Inspect MP4/QuickTime or Matroska/WebM container metadata without decoding. Unknown fields are omitted; an unrecognized format returns recognized:false. Malformed recognized containers and parser resource limits fail with OPERATION_FAILED, not an empty successful track list. Paths follow getStreamUrl. The result describes the whole container, whatever subsong suffix the path has. foobar2000 plays each chapter of an MP4 or Matroska file as its own subsong, timed from the chapter start; a chapter's subsong field names that subsong. Malformed chapter structures only omit chapters. Requires a trusted HTTP(S) document, but not the media routing runtime interface. QuickTime compressed movie headers (cmov) and reference movies (rmra) are recognized but unsupported and fail with OPERATION_FAILED.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Local container path; a subsong suffix is stripped before file access. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `recognized` | `boolean` | Whether the format belongs to a supported container family. |
| `container` | `"mp4" \| "mov" \| "matroska" \| "webm"` | Container identified by its file structure. |
| `mimeType` | `string` | MIME type of this container. |
| `duration` | `number` | Declared presentation duration in seconds, when known. |
| `tracks` | `ContainerTrack[]` | Declared tracks; empty for unrecognized formats. |
| `tracks[].id` | `string` | Container track identifier, preserved as a string. |
| `tracks[].type` | `"video" \| "audio" \| "subtitle" \| "image" \| "other"` | Content category; cover images are not video tracks. |
| `tracks[].codec` | `string` | Original sample entry or Matroska CodecID. |
| `tracks[].codecs` | `string` | RFC 6381 codecs value when fully determined. |
| `tracks[].mimeType` | `string` | Container MIME applicable to this track. |
| `tracks[].duration` | `number` | Declared track duration in seconds. |
| `tracks[].startTime` | `number` | First presentation time on the container timeline, in seconds, when determined. |
| `tracks[].language` | `string` | Track language as stored in the container. |
| `tracks[].name` | `string` | Track display name stored in the container. |
| `tracks[].default` | `boolean` | Container default-track flag, when the format defines it. |
| `tracks[].forced` | `boolean` | Container forced-track flag, when the format defines it. |
| `tracks[].width` | `integer` | Coded width in pixels. |
| `tracks[].height` | `integer` | Coded height in pixels. |
| `tracks[].displayAspectRatio` | `number` | Display width divided by display height. |
| `tracks[].rotation` | `number` | Clockwise display rotation in degrees when the transform is a pure rotation. |
| `tracks[].frameRate` | `number` | Declared or average frame rate, in frames per second; not a VFR maximum. |
| `tracks[].bitrate` | `integer` | Declared average bit rate in bits per second. |
| `tracks[].bitDepth` | `integer` | Declared sample precision in bits. |
| `tracks[].colorPrimaries` | `integer` | Colour primaries identifier from the container or codec configuration. |
| `tracks[].colorTransfer` | `integer` | Transfer characteristic identifier, including HDR transfer functions. |
| `tracks[].colorMatrix` | `integer` | Matrix coefficients identifier. |
| `tracks[].fullRange` | `boolean` | Full-range video flag when explicitly present. |
| `tracks[].sampleRate` | `number` | Audio sampling frequency in hertz. |
| `tracks[].channels` | `integer` | Declared audio channel count. |
| `attachments` | `ContainerAttachment[]` | Attachment metadata without file contents. |
| `attachments[].id` | `string` | Container attachment identifier. |
| `attachments[].name` | `string` | Stored filename or descriptive name. |
| `attachments[].mimeType` | `string` | Declared attachment MIME. |
| `attachments[].size` | `integer` | Attachment content length in bytes. |
| `chapters` | `ContainerChapter[]` | Chapters in container order, which is foobar2000's subsong order, not sorted by start. MP4 uses the QuickTime chapter track when present, otherwise Nero chpl; Matroska lists the top-level chapters of every edition, hidden ones included. Empty when the container declares none; omitted when the chapter structure is malformed or links other files. |
| `chapters[].start` | `number` | Start on the container timeline, in seconds. |
| `chapters[].end` | `number` | End in seconds: the declared end, else the next chapter's start in the same edition, else the container duration for the last chapter. |
| `chapters[].title` | `string` | Chapter title as stored in the container. |
| `chapters[].language` | `string` | Language of the title as stored in the container. |
| `chapters[].edition` | `integer` | Position of the chapter's Matroska edition in the container, from 0; present only when the file has more than one edition. |
| `chapters[].subsong` | `integer` | foobar2000 subsong that plays this chapter, for `path\|subsong:N`. Present only when foobar2000 splits the file into as many subsongs as there are chapters and their durations agree. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The parser reads MP4/QuickTime and Matroska/WebM structures without decoding. It skips media payloads and bounds reads, nesting and element counts. Unrecognized formats return `recognized: false`; a damaged supported container or an exceeded resource limit fails with `OPERATION_FAILED`. QuickTime compressed movie headers (`cmov`) and reference movies (`rmra`) also fail with `OPERATION_FAILED`; external references are not followed.

Track IDs are strings. Times are seconds, dimensions are pixels, sample rates are hertz and bitrates are bits per second. A missing optional field means unknown; so does a name, language or title that is not valid UTF-8. Cover art appears as image or attachment metadata, never as a video track. Attachments provide names, MIME types and sizes when available, without extraction URLs or file contents.

### Chapters

foobar2000 plays each chapter of an MP4 or Matroska file as its own subsong, and the playback position of that subsong starts at 0 at the chapter start. `chapters` lists them in the same order as the subsongs:

- MP4 uses the QuickTime chapter track when there is one, as foobar2000 does, and Nero `chpl` otherwise. The text track that carries chapter titles is reported with `type: 'other'`, not as subtitles.
- Matroska lists the top-level chapters of every edition, one after another, including hidden and disabled ones; nested chapters are not listed. With more than one edition, `edition` tells which edition a chapter belongs to.
- `subsong` appears only when foobar2000 splits the file into as many subsongs as there are chapters and every chapter except the last lasts as long as its subsong, within 10 ms. foobar2000 extends the last subsong to the end of the audio, so its length is not compared.

To show the picture of a chapter subsong, look up the chapter whose `subsong` matches the playing track and start the video at its `start`:

```ts
import { media, unwrap } from 'foo-webview-sdk/bridge';

async function chapterStart(path: string, subsong: number): Promise<number> {
    const info = unwrap(await media.getContainerInfo(path));
    return info.chapters?.find(chapter => chapter.subsong === subsong)?.start ?? 0;
}
```

Current limits:

- `startTime` is omitted: edit lists, composition offsets and the first Matroska block are not scanned. Chapter times are read as stored; the chapter track's edit list is not applied.
- Chapters that link to other Matroska files (`ChapterSegmentUID`) omit `chapters`. Ordered editions are listed by file position, not as a combined playback timeline.
- MP4 enabled-track flags are not interpreted as `default` or `forced`. AVC SPS colour and precision fields are not decoded; explicit container colour fields are returned.
- Matroska container duration is not copied into every track. Missing frame rate, bitrate or codec configuration stays missing.
- Where multiple MP4 sample descriptions disagree, only common metadata is returned. Different sample-entry codes produce `codec: 'unknown'`.
- Compressed MP4 audio entries can contain placeholder sample rates and channel counts. These fields are omitted unless codec configuration determines them. AAC configuration is parsed; PCE channel layouts and ER/ELD extensions are not.

Container recognition does not guarantee browser playback. Use the SDK's `canPlay(track)` as a capability hint and handle media-element errors.
