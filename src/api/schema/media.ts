import type { Int } from './common.js';

export interface Api {
  /**
   * Issue a URL for a local media file, usable by the calling document until navigation,
   * file modification or eviction. Use crossorigin="anonymous" on media elements.
   * Requires a trusted HTTP(S) document and WebView2 ICoreWebView2_22; older
   * runtimes return NOT_SUPPORTED.
   * GET supports one byte range, returning at most 2 MiB. GET without Range rejects
   * files larger than 2 MiB with HTTP 416. HEAD returns metadata without a body.
   * Only native local paths, UNC, foobar2000 file:// paths and file-relative:// are accepted.
   * A subsong suffix selects the container, not a chapter timeline. Network and archive
   * protocols fail with INVALID_PARAMS; denied files with PERMISSION_DENIED; missing
   * files with NOT_FOUND. File IO or resource limits fail with OPERATION_FAILED.
   * @zh 签发供调用方文档使用的本地媒体地址，导航、文件改变或令牌被淘汰后失效。要求可信 HTTP(S) 文档和 WebView2 ICoreWebView2_22，旧运行时返回 NOT_SUPPORTED。媒体元素须设置 crossorigin="anonymous"。GET 支持单段 Range，每段最多 2 MiB；超过 2 MiB 的文件不带 Range 时回 HTTP 416；HEAD 只回元数据。只接受本地盘符、UNC、foobar2000 的 file:// 与 file-relative:// 路径，子曲目后缀取回容器，不换算章节时间。网络与归档协议报 INVALID_PARAMS，权限拒绝报 PERMISSION_DENIED，文件不存在报 NOT_FOUND，文件读取或资源限制失败报 OPERATION_FAILED。
   * @experimental
   * @effect read
   */
  getStreamUrl(params: MediaPathParams): GetStreamUrlResult;

  /**
   * Inspect MP4/QuickTime or Matroska/WebM container metadata without decoding.
   * Unknown fields are omitted; an unrecognized format returns recognized:false.
   * Malformed recognized containers and parser resource limits fail with
   * OPERATION_FAILED, not an empty successful track list. Paths follow getStreamUrl.
   * The result describes the whole container, whatever subsong suffix the path has.
   * foobar2000 plays each chapter of an MP4 or Matroska file as its own subsong, timed
   * from the chapter start; a chapter's subsong field names that subsong. Malformed
   * chapter structures only omit chapters.
   * Requires a trusted HTTP(S) document, but not the media routing runtime interface.
   * QuickTime compressed movie headers (cmov) and reference movies (rmra) are
   * recognized but unsupported and fail with OPERATION_FAILED.
   * @zh 不解码，读取 MP4/QuickTime 或 Matroska/WebM 的容器元数据。未知字段省略，不识别的格式返回 recognized:false；已识别容器损坏或解析超过资源限制时报 OPERATION_FAILED，不用空轨道列表表示成功。路径规则同 getStreamUrl。无论路径带什么子曲目后缀，结果都描述整个容器。foobar2000 把 MP4 与 Matroska 文件的每一章当作一个子曲目播放，位置从章节起点算；章节的 subsong 字段指明是哪个子曲目。章节结构损坏时只省略 chapters。要求可信 HTTP(S) 文档，但不依赖媒体路由所需的运行时接口。QuickTime 压缩电影头（cmov）和引用电影（rmra）虽能识别，但尚不支持，返回 OPERATION_FAILED。
   * @experimental
   * @effect read
   */
  getContainerInfo(params: MediaPathParams): GetContainerInfoResult;
}

interface MediaPathParams {
  /**
   * Local container path; a subsong suffix is stripped before file access.
   * @zh 本地容器路径；访问文件前去掉子曲目后缀。
   * @minLength 1
   * @security MediaRead
   */
  path: string;
}

interface GetStreamUrlResult {
  /** Opaque URL bound to the calling document. @zh 绑定调用方文档的不透明地址。 */
  url: string;
  /** File size in bytes, within JavaScript's safe integer range. @zh 文件字节数，在 JavaScript 安全整数范围内。 */
  size: Int;
  /** MIME type identified from file content; application/octet-stream when unknown. @zh 按文件内容识别的 MIME，未知时为 application/octet-stream。 */
  mimeType: string;
}

interface GetContainerInfoResult {
  /** Whether the format belongs to a supported container family. @zh 格式是否属于支持的容器家族。 */
  recognized: boolean;
  /** Container identified by its file structure. @zh 根据文件结构识别的容器。 */
  container?: 'mp4' | 'mov' | 'matroska' | 'webm';
  /** MIME type of this container. @zh 容器的 MIME。 */
  mimeType?: string;
  /** Declared presentation duration in seconds, when known. @zh 已知时，容器声明的呈现时长，单位秒。 */
  duration?: number;
  /** Declared tracks; empty for unrecognized formats. @zh 容器声明的轨道；不识别的格式为空。 */
  tracks: ContainerTrack[];
  /** Attachment metadata without file contents. @zh 附件元数据，不含附件内容。 */
  attachments: ContainerAttachment[];
  /**
   * Chapters in container order, which is foobar2000's subsong order, not sorted by start.
   * MP4 uses the QuickTime chapter track when present, otherwise Nero chpl; Matroska lists
   * the top-level chapters of every edition, hidden ones included. Empty when the container
   * declares none; omitted when the chapter structure is malformed or links other files.
   * @zh 按容器顺序排列的章节，也就是 foobar2000 子曲目的顺序，不按起点排序。MP4 有 QuickTime 章节轨时用它，否则用 Nero chpl；Matroska 列出每一套版本的顶层章节，隐藏的也列。容器没有声明章节时为空数组；章节结构损坏或指向别的文件时省略。
   */
  chapters?: ContainerChapter[];
}

/** One chapter on the container timeline; absent fields are unknown. */
interface ContainerChapter {
  /** Start on the container timeline, in seconds. @zh 在容器时间轴上的起点，单位秒。 */
  start: number;
  /**
   * End in seconds: the declared end, else the next chapter's start in the same edition,
   * else the container duration for the last chapter.
   * @zh 终点，单位秒：容器声明的终点；没有时取同一套版本中下一章的起点；最后一章取容器时长。
   */
  end?: number;
  /** Chapter title as stored in the container. @zh 容器记录的章节标题。 */
  title?: string;
  /** Language of the title as stored in the container. @zh 容器记录的标题语言。 */
  language?: string;
  /**
   * Position of the chapter's Matroska edition in the container, from 0; present only when
   * the file has more than one edition.
   * @zh 章节所属 Matroska 版本在容器中的序号，从 0 起；只在文件有多套版本时给出。
   */
  edition?: Int;
  /**
   * foobar2000 subsong that plays this chapter, for `path|subsong:N`. Present only when
   * foobar2000 splits the file into as many subsongs as there are chapters and their
   * durations agree.
   * @zh 播放这一章的 foobar2000 子曲目，用于 `path|subsong:N`。只在 foobar2000 把文件拆成与章节同样多的子曲目、且时长相符时给出。
   */
  subsong?: Int;
}

/** Metadata of one declared container track; absence means unknown, not zero. */
interface ContainerTrack {
  /** Container track identifier, preserved as a string. @zh 容器轨道标识，保留为字符串。 */
  id: string;
  /** Content category; cover images are not video tracks. @zh 内容类别；封面图片不作为视频轨。 */
  type: 'video' | 'audio' | 'subtitle' | 'image' | 'other';
  /** Original sample entry or Matroska CodecID. @zh 原始 sample entry 或 Matroska CodecID。 */
  codec: string;
  /** RFC 6381 codecs value when fully determined. @zh 能完整确定时，RFC 6381 codecs 值。 */
  codecs?: string;
  /** Container MIME applicable to this track. @zh 适用于该轨的容器 MIME。 */
  mimeType?: string;
  /** Declared track duration in seconds. @zh 轨道声明的时长，单位秒。 */
  duration?: number;
  /** First presentation time on the container timeline, in seconds, when determined. @zh 可确定时，容器时间轴上的首次呈现时刻，单位秒。 */
  startTime?: number;
  /** Track language as stored in the container. @zh 容器记录的轨道语言。 */
  language?: string;
  /** Track display name stored in the container. @zh 容器记录的轨道名称。 */
  name?: string;
  /** Container default-track flag, when the format defines it. @zh 格式定义了默认轨标记时，给出该标记。 */
  default?: boolean;
  /** Container forced-track flag, when the format defines it. @zh 格式定义了强制轨标记时，给出该标记。 */
  forced?: boolean;
  /** Coded width in pixels. @zh 编码宽度，单位像素。 */
  width?: Int;
  /** Coded height in pixels. @zh 编码高度，单位像素。 */
  height?: Int;
  /** Display width divided by display height. @zh 显示宽度除以显示高度。 */
  displayAspectRatio?: number;
  /** Clockwise display rotation in degrees when the transform is a pure rotation. @zh 变换为纯旋转时，顺时针显示旋转角度。 */
  rotation?: number;
  /** Declared or average frame rate, in frames per second; not a VFR maximum. @zh 声明帧率或平均帧率，单位帧每秒，不表示可变帧率的最大值。 */
  frameRate?: number;
  /** Declared average bit rate in bits per second. @zh 声明的平均码率，单位 bit/s。 */
  bitrate?: Int;
  /** Declared sample precision in bits. @zh 声明的采样精度，单位位。 */
  bitDepth?: Int;
  /** Colour primaries identifier from the container or codec configuration. @zh 容器或编码配置中的色彩原色标识。 */
  colorPrimaries?: Int;
  /** Transfer characteristic identifier, including HDR transfer functions. @zh 传递特性标识，包括 HDR 传递函数。 */
  colorTransfer?: Int;
  /** Matrix coefficients identifier. @zh 矩阵系数标识。 */
  colorMatrix?: Int;
  /** Full-range video flag when explicitly present. @zh 显式存在时的视频全范围标记。 */
  fullRange?: boolean;
  /** Audio sampling frequency in hertz. @zh 音频采样频率，单位 Hz。 */
  sampleRate?: number;
  /** Declared audio channel count. @zh 声明的音频声道数。 */
  channels?: Int;
}

/** One container attachment, with no content or extraction URL. */
interface ContainerAttachment {
  /** Container attachment identifier. @zh 容器附件标识。 */
  id: string;
  /** Stored filename or descriptive name. @zh 存储的文件名或描述名称。 */
  name: string;
  /** Declared attachment MIME. @zh 声明的附件 MIME。 */
  mimeType?: string;
  /** Attachment content length in bytes. @zh 附件内容字节数。 */
  size?: Int;
}
