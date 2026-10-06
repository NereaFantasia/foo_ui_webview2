# Media 媒体 API

实验性的 `media` 命名空间提供本地媒体字节与容器元数据，解码由浏览器完成。让静音视频跟随 foobar2000 播放，见 [SDK 媒体工具](../sdk/media.md)。

## media.getStreamUrl

<!-- api-schema:begin media.getStreamUrl -->
实验性 API，后续版本可能发生变化。

签发供调用方文档使用的本地媒体地址，导航、文件改变或令牌被淘汰后失效。要求可信 HTTP(S) 文档和 WebView2 ICoreWebView2_22，旧运行时返回 NOT_SUPPORTED。媒体元素须设置 crossorigin="anonymous"。GET 支持单段 Range，每段最多 2 MiB；超过 2 MiB 的文件不带 Range 时回 HTTP 416；HEAD 只回元数据。只接受本地盘符、UNC、foobar2000 的 file:// 与 file-relative:// 路径，子曲目后缀取回容器，不换算章节时间。网络与归档协议报 INVALID_PARAMS，权限拒绝报 PERMISSION_DENIED，文件不存在报 NOT_FOUND，文件读取或资源限制失败报 OPERATION_FAILED。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 本地容器路径；访问文件前去掉子曲目后缀。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `url` | `string` | 绑定调用方文档的不透明地址。 |
| `size` | `integer` | 文件字节数，在 JavaScript 安全整数范围内。 |
| `mimeType` | `string` | 按文件内容识别的 MIME，未知时为 application/octet-stream。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

地址只供签发时的文档使用。刷新、导航（包括被取消的导航）、关闭窗口或文件改变都会使它失效。每个文档保留最多 64 个地址，超出后淘汰最久未使用的地址，不设置固定有效秒数。地址只包含随机令牌，不包含文件路径。

页面须来自可信的 HTTP(S) 来源，且必须先设置 `crossOrigin = 'anonymous'`，再设置 `src`。`about:blank`、`file:` 等不透明来源无法在 CORS 请求中标识签发页面，因此不能签发媒体地址；其他 Bridge 调用仍沿用原来的来源名单。

| 请求 | 响应 |
| --- | --- |
| GET，带合法单段 Range | 206，每段最多 2 MiB，浏览器可接着请求下一段 |
| GET，不带 Range，文件不超过 2 MiB | 200，返回完整文件 |
| 大文件不带 Range、多段或不可满足的 Range | 416，含 `Content-Range: bytes */size` |
| HEAD | 200，返回完整文件大小，不含正文 |
| OPTIONS | 校验具体来源、GET/HEAD 方法和 Range 请求头 |
| 未知令牌、其他文档或权限拒绝 | 403 |
| 已签发文件被删除，或大小、mtime 改变 | 410，需重新签发 |
| 队列已满、正在退出或共享冲突重试失败 | 503 |

响应带 `Cache-Control: no-store`、由大小与 mtime 组成的 ETag，以及字节范围头。CORS 只允许签发页面的具体可信来源，错误响应也遵循相同规则。地址不能转交其他窗口使用。每次请求及返回字节前都会重新检查权限。

## media.getContainerInfo

<!-- api-schema:begin media.getContainerInfo -->
实验性 API，后续版本可能发生变化。

不解码，读取 MP4/QuickTime 或 Matroska/WebM 的容器元数据。未知字段省略，不识别的格式返回 recognized:false；已识别容器损坏或解析超过资源限制时报 OPERATION_FAILED，不用空轨道列表表示成功。路径规则同 getStreamUrl。无论路径带什么子曲目后缀，结果都描述整个容器。foobar2000 把 MP4 与 Matroska 文件的每一章当作一个子曲目播放，位置从章节起点算；章节的 subsong 字段指明是哪个子曲目。章节结构损坏时只省略 chapters。要求可信 HTTP(S) 文档，但不依赖媒体路由所需的运行时接口。QuickTime 压缩电影头（cmov）和引用电影（rmra）虽能识别，但尚不支持，返回 OPERATION_FAILED。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 本地容器路径；访问文件前去掉子曲目后缀。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `recognized` | `boolean` | 格式是否属于支持的容器家族。 |
| `container` | `"mp4" \| "mov" \| "matroska" \| "webm"` | 根据文件结构识别的容器。 |
| `mimeType` | `string` | 容器的 MIME。 |
| `duration` | `number` | 已知时，容器声明的呈现时长，单位秒。 |
| `tracks` | `ContainerTrack[]` | 容器声明的轨道；不识别的格式为空。 |
| `tracks[].id` | `string` | 容器轨道标识，保留为字符串。 |
| `tracks[].type` | `"video" \| "audio" \| "subtitle" \| "image" \| "other"` | 内容类别；封面图片不作为视频轨。 |
| `tracks[].codec` | `string` | 原始 sample entry 或 Matroska CodecID。 |
| `tracks[].codecs` | `string` | 能完整确定时，RFC 6381 codecs 值。 |
| `tracks[].mimeType` | `string` | 适用于该轨的容器 MIME。 |
| `tracks[].duration` | `number` | 轨道声明的时长，单位秒。 |
| `tracks[].startTime` | `number` | 可确定时，容器时间轴上的首次呈现时刻，单位秒。 |
| `tracks[].language` | `string` | 容器记录的轨道语言。 |
| `tracks[].name` | `string` | 容器记录的轨道名称。 |
| `tracks[].default` | `boolean` | 格式定义了默认轨标记时，给出该标记。 |
| `tracks[].forced` | `boolean` | 格式定义了强制轨标记时，给出该标记。 |
| `tracks[].width` | `integer` | 编码宽度，单位像素。 |
| `tracks[].height` | `integer` | 编码高度，单位像素。 |
| `tracks[].displayAspectRatio` | `number` | 显示宽度除以显示高度。 |
| `tracks[].rotation` | `number` | 变换为纯旋转时，顺时针显示旋转角度。 |
| `tracks[].frameRate` | `number` | 声明帧率或平均帧率，单位帧每秒，不表示可变帧率的最大值。 |
| `tracks[].bitrate` | `integer` | 声明的平均码率，单位 bit/s。 |
| `tracks[].bitDepth` | `integer` | 声明的采样精度，单位位。 |
| `tracks[].colorPrimaries` | `integer` | 容器或编码配置中的色彩原色标识。 |
| `tracks[].colorTransfer` | `integer` | 传递特性标识，包括 HDR 传递函数。 |
| `tracks[].colorMatrix` | `integer` | 矩阵系数标识。 |
| `tracks[].fullRange` | `boolean` | 显式存在时的视频全范围标记。 |
| `tracks[].sampleRate` | `number` | 音频采样频率，单位 Hz。 |
| `tracks[].channels` | `integer` | 声明的音频声道数。 |
| `attachments` | `ContainerAttachment[]` | 附件元数据，不含附件内容。 |
| `attachments[].id` | `string` | 容器附件标识。 |
| `attachments[].name` | `string` | 存储的文件名或描述名称。 |
| `attachments[].mimeType` | `string` | 声明的附件 MIME。 |
| `attachments[].size` | `integer` | 附件内容字节数。 |
| `chapters` | `ContainerChapter[]` | 按容器顺序排列的章节，也就是 foobar2000 子曲目的顺序，不按起点排序。MP4 有 QuickTime 章节轨时用它，否则用 Nero chpl；Matroska 列出每一套版本的顶层章节，隐藏的也列。容器没有声明章节时为空数组；章节结构损坏或指向别的文件时省略。 |
| `chapters[].start` | `number` | 在容器时间轴上的起点，单位秒。 |
| `chapters[].end` | `number` | 终点，单位秒：容器声明的终点；没有时取同一套版本中下一章的起点；最后一章取容器时长。 |
| `chapters[].title` | `string` | 容器记录的章节标题。 |
| `chapters[].language` | `string` | 容器记录的标题语言。 |
| `chapters[].edition` | `integer` | 章节所属 Matroska 版本在容器中的序号，从 0 起；只在文件有多套版本时给出。 |
| `chapters[].subsong` | `integer` | 播放这一章的 foobar2000 子曲目，用于 `path\|subsong:N`。只在 foobar2000 把文件拆成与章节同样多的子曲目、且时长相符时给出。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

解析器直接读取 MP4/QuickTime 与 Matroska/WebM 结构，不做解码；跳过媒体数据，并限制读取量、嵌套深度与结构数量。未知格式返回 `recognized: false`，已识别容器损坏或超过资源限制则报 `OPERATION_FAILED`。QuickTime 压缩电影头（`cmov`）与引用电影（`rmra`）也返回 `OPERATION_FAILED`，不会跟随外部引用读取文件。

轨道 ID 是字符串。时间单位为秒，尺寸为像素，采样率为 Hz，码率为 bit/s；可选字段缺失表示未知，名称、语言或标题不是合法 UTF-8 时同样省略。封面列为图像或附件元数据，不当作视频轨。附件只给出已知的名称、MIME 与大小，不提供提取地址或文件内容。

### 章节

foobar2000 把 MP4 与 Matroska 文件的每一章当作一个子曲目播放，子曲目的播放位置从这一章的起点算 0。`chapters` 按与子曲目相同的顺序列出章节：

- MP4 有 QuickTime 章节轨时用它，与 foobar2000 一致，否则用 Nero `chpl`。承载章节标题的文本轨报告为 `type: 'other'`，不算字幕。
- Matroska 依次列出每一套版本的顶层章节，隐藏、停用的也列，不列嵌套章节。有多套版本时，`edition` 指明章节属于哪一套。
- 只有 foobar2000 把文件拆成与章节同样多的子曲目，且除最后一章外每章时长与对应子曲目相差不超过 10 ms 时，才给出 `subsong`。foobar2000 把最后一个子曲目延到音频末尾，所以不比它的时长。

要显示章节子曲目的画面，找 `subsong` 与正在播放的曲目相同的那一章，让视频从它的 `start` 开始：

```ts
import { media, unwrap } from 'foo-webview-sdk/bridge';

async function chapterStart(path: string, subsong: number): Promise<number> {
    const info = unwrap(await media.getContainerInfo(path));
    return info.chapters?.find(chapter => chapter.subsong === subsong)?.start ?? 0;
}
```

当前限制：

- 不扫描 edit list、composition offset 和 Matroska 首个 Block，因此省略 `startTime`。章节时间按容器记录读取，不套用章节轨的 edit list。
- Matroska 章节指向别的文件（`ChapterSegmentUID`）时省略 `chapters`。有序版本按章节在文件里的位置列出，不拼成按播放顺序的时间轴。
- MP4 的 enabled 标记不等同于默认或强制轨，所以不据此填写 `default`、`forced`。不解析 AVC SPS 中的色彩与精度字段，但会读取容器明确提供的色彩字段。
- 不把 Matroska 的容器时长当作每条轨道的时长；缺少帧率、码率或编码配置时保留未知。
- MP4 多份 sample description 不一致时，只保留共同字段；sample-entry 编码不同则返回 `codec: 'unknown'`。
- 压缩 MP4 音轨的 sample entry 可能使用占位采样率和声道数，只有编码配置能确定这些值时才返回。目前解析 AAC 配置，不解析 PCE 声道布局及 ER/ELD 专用扩展。

能识别容器不代表浏览器能播放。SDK 的 `canPlay(track)` 可提供能力参考，调用方仍须处理媒体元素报错。
