# Local media

`media` reads local container metadata and issues document-bound URLs. `MediaElementFollower` connects a muted browser media element to foobar2000's playback clock. These APIs are experimental.

```ts
import { media, unwrap, canPlay, MediaElementFollower } from 'foo-webview-sdk/bridge';

const info = unwrap(await media.getContainerInfo('E:\\Music\\clip.mp4'));
const videoTrack = info.tracks.find(track => track.type === 'video');
if (videoTrack) console.log(await canPlay(videoTrack));

const element = document.createElement('video');
document.body.append(element);
const follower = new MediaElementFollower(element);
const unsubscribe = follower.onError(error => console.error(error));
await follower.setSource('E:\\Music\\clip.mp4');

// When the view is removed:
unsubscribe();
follower.dispose();
```

`media.getStreamUrl(path)` and `media.getContainerInfo(path)` return the host's success/failure envelope; use `unwrap` or check `success`. Paths, permissions, supported containers, URL lifetime and HTTP responses are described in [Media API](../api/media.md). Stream URLs require a trusted HTTP(S) page and a WebView2 runtime exposing `ICoreWebView2_22`.

## Capability hints

`canPlay(track)`, also available as `media.canPlay(track)`, resolves with:

| Field | Meaning |
| --- | --- |
| `supported` | MediaCapabilities decoding support, or `'unknown'` |
| `smooth` | Expected smooth decoding, or `'unknown'` |
| `powerEfficient` | Expected efficient decoding, or `'unknown'` |
| `canPlayType` | The media element's `''`, `'maybe'` or `'probably'` answer, or `'unknown'` |

Video queries require codec configuration, dimensions and caller-verified limits: `canPlay(track, { maxFrameRate, maxBitrate })`. The limits are the maximum frames per second and maximum bits per second; bitrate must be a positive safe integer. Container frame rate and bitrate can be averages, so the helper does not use them as maxima. Constant frame rate and bitrate may be supplied when the caller has verified them. Without both limits, MediaCapabilities results stay unknown and `canPlayType` is still queried.

Audio queries use the track's average bitrate, channel count, sample rate and codec configuration. Missing inputs keep the result unknown. A positive `canPlayType` answer does not override a negative MediaCapabilities answer. Matroska hints are advisory: actual playback can still fail.

## Following playback

Create `new MediaElementFollower(element, { clock?, offsetSeconds? })`. A supplied `PlaybackClock` remains owned by the caller; otherwise the follower creates and disposes its own. A positive offset places the element ahead of the foobar2000 clock by that many seconds.

Pass an element with no `srcObject` or `<source>` children; the constructor rejects these competing sources. While the follower owns the element, do not change its source from outside. The helper sets `preload="metadata"`, `muted` and anonymous CORS before assigning the source.

Every 100 ms, it leaves errors up to 50 ms alone, uses a playback rate between 0.95 and 1.05 below 250 ms, and seeks for larger errors. Explicit host seeks align immediately unless a fresh clock snapshot is still pending. Pausing and stopping pause the element even during a clock read. Initial playback and returning to a visible page wait for a complete, fresh host state and position, including when a new source arrives during that read. A supplied clock must honor `resync({ rejectOnFailure: true })`: either read both values successfully or reject without partially updating its snapshot.

- `setSource(path, { timelineOffset? })` issues a URL and loads that file. `setSource(null)` clears it. `timelineOffset` is where the track starts in the file, in seconds; it is added to `offsetSeconds`.
- A track change pauses the element until the theme selects the next path. Selecting the same file again, with or without another `|subsong:N` suffix, keeps a loaded resource and only moves to the new `timelineOffset`; another file replaces it. A resource still loading when the track changes is dropped.
- `resync()` pauses the element, reads the host clock and aligns it after success. A failed clock read keeps playback paused; a later successful call can recover. It also clears a recoverable `play()` rejection; terminal source failures require `setSource`.
- `error` contains the current error or `null`. `onError(listener)` returns an unsubscribe function.
- `dispose()` releases the timer, subscriptions and DOM listeners, clears the source and pauses the element. Subsequent source or resync calls reject.

Browsers expose a media network error without its HTTP status. The follower therefore tries at most two new URLs for `MEDIA_ERR_NETWORK` per explicit source selection. It resumes at the latest clock position; permission failures and decode failures stop recovery. Stale source requests and old `play()` rejections cannot alter a newer source.

The `<script>` bundle exposes the same helpers as `fb.media`, `fb.MediaElementFollower` and `fb.canPlay`. The follower does not look up chapters itself, switch foobar2000's track, select browser audio tracks, or unmute the element.

### Chapter subsongs

foobar2000 plays each chapter of an MP4 or Matroska file as its own subsong, timed from the chapter start. Find the chapter in [`media.getContainerInfo`](../api/media.md#chapters) and pass its start, so consecutive chapters keep using one loaded file:

```ts
import { media, unwrap, MediaElementFollower } from 'foo-webview-sdk/bridge';

const follower = new MediaElementFollower(document.createElement('video'));

async function followTrack(path: string, subsong: number) {
    const info = unwrap(await media.getContainerInfo(path));
    const chapter = info.chapters?.find(item => item.subsong === subsong);
    await follower.setSource(`${path}|subsong:${subsong}`, { timelineOffset: chapter?.start ?? 0 });
    // Chapters of other editions belong to other subsongs.
    return info.chapters?.filter(item => item.edition === chapter?.edition) ?? [];
}
```

```ts
import * as fb from 'foo-webview-sdk/bridge';

const path = 'E:\\Music\\clip.mp4';
const url = fb.unwrap(await fb.media.getStreamUrl(path)).url;
const container = fb.unwrap(await fb.media.getContainerInfo(path));
console.log(url, container.tracks);
```
