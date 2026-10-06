# 本地媒体

`media` 读取本地容器信息并签发绑定当前文档的地址；`MediaElementFollower` 让静音的浏览器媒体元素跟随 foobar2000 播放。这些接口目前标记为实验性。

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

// 移除视图时释放资源。
unsubscribe();
follower.dispose();
```

`media.getStreamUrl(path)` 和 `media.getContainerInfo(path)` 返回宿主的成功或失败信封，可用 `unwrap` 或检查 `success`。路径权限、容器范围、地址寿命和 HTTP 响应见 [Media API](../api/media.md)。媒体地址要求可信的 HTTP(S) 页面，以及提供 `ICoreWebView2_22` 的 WebView2 运行时。

## 判断浏览器能力

`canPlay(track)`（也可调用 `media.canPlay(track)`）返回：

| 字段 | 含义 |
| --- | --- |
| `supported` | MediaCapabilities 的解码支持判断，未知时为 `'unknown'` |
| `smooth` | 预计是否流畅，未知时为 `'unknown'` |
| `powerEfficient` | 预计是否节能，未知时为 `'unknown'` |
| `canPlayType` | 媒体元素给出的 `''`、`'maybe'`、`'probably'`，或 `'unknown'` |

视频查询需要编码配置、尺寸以及调用方已确认的最大值：`canPlay(track, { maxFrameRate, maxBitrate })`。两个最大值分别以帧每秒和 bit/s 为单位，码率须为正的安全整数。容器中的帧率和码率可能是平均值，不能直接代作最大值；若调用方已确认素材使用恒定帧率和码率，可以传入该恒定值。缺少任一最大值时，MediaCapabilities 结果保持未知，仍会查询 `canPlayType`。

音频查询使用轨道的平均码率、声道数、采样率和编码配置。缺少必要输入时，结果保持未知。`canPlayType` 的乐观回答不会覆盖 MediaCapabilities 的否定回答。Matroska 的能力判断只供参考，实际播放仍可能失败。

## 跟随播放

用 `new MediaElementFollower(element, { clock?, offsetSeconds? })` 创建。传入的 `PlaybackClock` 仍由调用方释放；不传时，跟随器自建并负责释放。正的偏移量表示元素比 foobar2000 时钟提前相应秒数。

传入的元素不能带有 `srcObject` 或 `<source>` 子元素，构造器会拒绝这些竞争来源。跟随器持有元素期间，外部代码不要修改媒体源。跟随器在设置源之前设定 `preload="metadata"`、静音和匿名 CORS。

每 100 ms 检查一次：误差不超过 50 ms 时不动，小于 250 ms 时以 0.95–1.05 的速度追齐，更大时直接跳转。宿主明确 seek 时立即对齐，仍在等待新时钟快照时则先等待读取完成；暂停和停止在读取期间也会立即暂停元素。首次播放和页面恢复可见时，先完整读取宿主状态与位置再继续播放，期间换源也要等待读数完成。借用的时钟必须遵守 `resync({ rejectOnFailure: true })`：两项读数都成功才更新快照，否则拒绝，不能只更新其中一项。

- `setSource(path, { timelineOffset? })` 签发地址并加载文件；`setSource(null)` 清空。`timelineOffset` 是这首曲目在文件里的起点，单位秒，与 `offsetSeconds` 相加。
- 换曲时元素暂停，直到主题选择下一个路径。再次选择同一个文件（带不带、带哪个 `|subsong:N` 后缀都算）时，保留已加载的资源，只移到新的 `timelineOffset`；换成别的文件才替换资源。换曲时仍在加载的资源会被丢弃。
- `resync()` 先暂停元素，再读取宿主时钟并在成功后对齐。读取失败会保持暂停，之后成功重试可以恢复。它也可清除能恢复的 `play()` 拒绝；终止性的媒体源错误需要重新 `setSource`。
- `error` 为当前错误或 `null`；`onError(listener)` 返回取消订阅函数。
- `dispose()` 释放定时器、订阅和 DOM 监听，清空源并暂停元素；之后调用设置源或重新同步的方法会拒绝。

浏览器的媒体网络错误不提供 HTTP 状态码，因此每次明确设置源后，跟随器最多为 `MEDIA_ERR_NETWORK` 重新签发两次。恢复使用最新时钟位置；权限拒绝和解码错误会停止恢复。旧的签发请求和 `play()` 拒绝不会影响较新的源。

`<script>` 包也提供 `fb.media`、`fb.MediaElementFollower`、`fb.canPlay`。跟随器自己不查章节，不切换 foobar2000 的曲目，不选择浏览器音轨，也不会取消静音。

### 章节子曲目

foobar2000 把 MP4 与 Matroska 文件的每一章当作一个子曲目播放，位置从章节起点算。在 [`media.getContainerInfo`](../api/media.md#章节) 里找到这一章并传入它的起点，连续的几章就会一直用同一个已加载的文件：

```ts
import { media, unwrap, MediaElementFollower } from 'foo-webview-sdk/bridge';

const follower = new MediaElementFollower(document.createElement('video'));

async function followTrack(path: string, subsong: number) {
    const info = unwrap(await media.getContainerInfo(path));
    const chapter = info.chapters?.find(item => item.subsong === subsong);
    await follower.setSource(`${path}|subsong:${subsong}`, { timelineOffset: chapter?.start ?? 0 });
    // 其他版本的章节对应别的子曲目。
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
