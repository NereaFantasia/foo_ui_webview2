# Media API

The experimental `media` namespace provides methods to access local media bytes and container metadata directly within WebView2 documents. Audio decoding remains the browser's responsibility. See [SDK media helpers](../sdk/media.md) for muted playback following foobar2000 state.

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

Use the URL only within the document that requested it. Reloading, navigation (including a cancelled navigation), closing the window, or modifying the target file invalidates the URL. Up to 64 URLs are retained per document; issuing more evicts the least recently used. URLs contain an opaque random token rather than a file path and have no fixed time-to-live.

The page must use a trusted HTTP(S) origin. Set `crossOrigin = 'anonymous'` **before** assigning `src`. Opaque origins such as `about:blank` and `file:` cannot identify the requesting origin for CORS and cannot request a media stream URL. This restriction does not affect their eligibility for other bridge calls.

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

The parser inspects MP4/QuickTime and Matroska/WebM structures without decoding audio or video streams. It skips media payloads and bounds parsing depth, nesting, and element counts. Unrecognized formats return `recognized: false`; a corrupt supported container or exceeded resource limit fails with `OPERATION_FAILED`. QuickTime compressed movie headers (`cmov`) and reference movies (`rmra`) also fail with `OPERATION_FAILED`; external references are not followed.

Track IDs are strings. Times are in seconds, dimensions in pixels, sample rates in hertz, and bitrates in bits per second. Omitted optional fields represent unknown values, as do strings that are not valid UTF-8. Cover art appears as image or attachment metadata rather than a video track. Attachments report names, MIME types, and byte sizes when available, without extraction URLs or payload contents.

### Chapters

foobar2000 plays each chapter of an MP4 or Matroska file as its own subsong, with playback position starting at 0 at chapter start. The `chapters` array lists entries in container order:

- MP4 uses the QuickTime chapter track when present (following foobar2000 behavior) and Nero `chpl` otherwise. The text track carrying chapter titles is reported with `type: 'other'` rather than subtitles.
- Matroska lists top-level chapters for each edition sequentially, including hidden and disabled entries; nested chapters are omitted. When multiple editions exist, `edition` identifies the owning edition index.
- `subsong` is present only when foobar2000 divides the file into as many subsongs as there are chapters, and each chapter (except the final one) matches subsong duration within 10 ms. foobar2000 extends the final subsong to the end of audio, so its duration is not compared.

To show the picture of a chapter subsong, look up the chapter whose `subsong` matches the playing track and start the video at its `start`:

```ts
import { media, unwrap } from 'foo-webview-sdk/bridge';

async function chapterStart(path: string, subsong: number): Promise<number> {
    const info = unwrap(await media.getContainerInfo(path));
    return info.chapters?.find(chapter => chapter.subsong === subsong)?.start ?? 0;
}
```

Current limits:

- `startTime` is omitted: edit lists, composition offsets, and initial Matroska blocks are not scanned. Chapter times are read as stored; edit lists on chapter tracks are not applied.
- Chapters referencing external Matroska files (`ChapterSegmentUID`) omit `chapters`. Ordered editions are listed by container order rather than as a combined playback timeline.
- MP4 enabled-track flags are not mapped to `default` or `forced`. AVC SPS color and precision fields are not decoded; explicit container color fields are returned.
- Matroska container duration is not propagated into track objects. Missing frame rate, bitrate, or codec configuration remains omitted.
- When multiple MP4 sample descriptions conflict, only common metadata is returned. Conflicting sample-entry codes produce `codec: 'unknown'`.
- Compressed MP4 audio entries may contain placeholder sample rates and channel counts. These fields are omitted unless confirmed by codec configuration. AAC configuration is parsed; PCE channel layouts and ER/ELD extensions are not supported.

Container recognition does not guarantee browser playback. Use SDK `canPlay(track)` as a capability hint and handle media element error events.
