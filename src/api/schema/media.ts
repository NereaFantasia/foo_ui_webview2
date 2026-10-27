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
   * @zh 签发供调用方文档使用的本地媒体流地址（导航、文件修改或令牌淘汰后自动失效；要求可信 HTTP(S) 文档环境与 WebView2 ICoreWebView2_22 运行时接口，旧版运行时返回 NOT_SUPPORTED；媒体标签须设置 crossorigin="anonymous"；GET 支持单段 Range 请求且每段上限 2 MiB，超过 2 MiB 的文件在无 Range 时返回 HTTP 416，HEAD 仅返回元数据头；仅接受本地盘符、UNC、foobar2000 file:// 与 file-relative:// 路径，子曲目后缀定位容器而非时间轴；网络与归档协议返回 INVALID_PARAMS，权限拒绝返回 PERMISSION_DENIED，文件不存在返回 NOT_FOUND，IO 或资源限制错误返回 OPERATION_FAILED）
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
   * @zh 无需解码读取 MP4/QuickTime 或 Matroska/WebM 容器元数据（未知字段自动省略，不识别格式返回 recognized:false；已识别容器损坏或超出解析资源限制返回 OPERATION_FAILED 而非空轨道；路径规则与 getStreamUrl 一致，不受子曲目后缀影响并描述完整容器；foobar2000 将 MP4 与 Matroska 每章作为独立子曲目播放，章节 subsong 标识子曲目序号，章节结构损坏时仅省略 chapters；要求可信 HTTP(S) 文档环境；QuickTime cmov 压缩头与 rmra 引用电影不支持且返回 OPERATION_FAILED）
   * @experimental
   * @effect read
   */
  getContainerInfo(params: MediaPathParams): GetContainerInfoResult;
}

interface MediaPathParams {
  /**
   * Local container path; a subsong suffix is stripped before file access.
   * @zh 本地容器路径（访问文件前去掉子曲目后缀）
   * @minLength 1
   * @security MediaRead
   */
  path: string;
}

interface GetStreamUrlResult {
  /** Opaque URL bound to the calling document. @zh 绑定调用方文档的不透明地址 */
  url: string;
  /** File size in bytes, within JavaScript's safe integer range. @zh 文件字节数（在 JavaScript 安全整数范围内） */
  size: Int;
  /** MIME type identified from file content; application/octet-stream when unknown. @zh 按文件内容识别的 MIME（未知时为 application/octet-stream） */
  mimeType: string;
}

interface GetContainerInfoResult {
  /** Whether the format belongs to a supported container family. @zh 格式是否属于受支持的容器格式 */
  recognized: boolean;
  /** Container identified by its file structure. @zh 根据文件结构识别的容器类型 */
  container?: 'mp4' | 'mov' | 'matroska' | 'webm';
  /** MIME type of this container. @zh 容器 MIME 类型 */
  mimeType?: string;
  /** Declared presentation duration in seconds, when known. @zh 容器声明的呈现时长（单位秒，未知时省略） */
  duration?: number;
  /** Declared tracks; empty for unrecognized formats. @zh 容器声明的轨道列表（不识别格式时为空数组） */
  tracks: ContainerTrack[];
  /** Attachment metadata without file contents. @zh 附件元数据列表（不含附件内容） */
  attachments: ContainerAttachment[];
  /**
   * Chapters in container order, which is foobar2000's subsong order, not sorted by start.
   * MP4 uses the QuickTime chapter track when present, otherwise Nero chpl; Matroska lists
   * the top-level chapters of every edition, hidden ones included. Empty when the container
   * declares none; omitted when the chapter structure is malformed or links other files.
   * @zh 按容器顺序排列的章节列表（对应 foobar2000 子曲目顺序，不按起点时间排序；MP4 优先使用 QuickTime 章节轨，缺失时回退至 Nero chpl；Matroska 列出各版本的顶层章节，含隐藏章节；容器未声明章节时为空数组，章节损坏或指向外部文件时省略）
   */
  chapters?: ContainerChapter[];
}

/** One chapter on the container timeline; absent fields are unknown. */
interface ContainerChapter {
  /** Start on the container timeline, in seconds. @zh 容器时间轴上的起点时间（单位秒） */
  start: number;
  /**
   * End in seconds: the declared end, else the next chapter's start in the same edition,
   * else the container duration for the last chapter.
   * @zh 终点时间（单位秒；容器声明的终点，未声明时取同版本下一章起点，最后一章取容器总时长）
   */
  end?: number;
  /** Chapter title as stored in the container. @zh 容器记录的章节标题 */
  title?: string;
  /** Language of the title as stored in the container. @zh 容器记录的标题语言 */
  language?: string;
  /**
   * Position of the chapter's Matroska edition in the container, from 0; present only when
   * the file has more than one edition.
   * @zh 章节所属 Matroska 版本在容器中的序号（从 0 起，仅在包含多套版本时提供）
   */
  edition?: Int;
  /**
   * foobar2000 subsong that plays this chapter, for `path|subsong:N`. Present only when
   * foobar2000 splits the file into as many subsongs as there are chapters and their
   * durations agree.
   * @zh 播放当前章节的 foobar2000 子曲目序号（用于 path\|subsong:N；仅在 foobar2000 拆分的子曲目数与章节数相同且时长匹配时提供）
   */
  subsong?: Int;
}

/** Metadata of one declared container track; absence means unknown, not zero. */
interface ContainerTrack {
  /** Container track identifier, preserved as a string. @zh 容器轨道标识（保留为字符串） */
  id: string;
  /** Content category; cover images are not video tracks. @zh 轨道内容类别（封面图片不作为视频轨） */
  type: 'video' | 'audio' | 'subtitle' | 'image' | 'other';
  /** Original sample entry or Matroska CodecID. @zh 原始 sample entry 或 Matroska CodecID */
  codec: string;
  /** RFC 6381 codecs value when fully determined. @zh RFC 6381 codecs 字符串（可完整确定时提供） */
  codecs?: string;
  /** Container MIME applicable to this track. @zh 适用于当前轨道的容器 MIME 类型 */
  mimeType?: string;
  /** Declared track duration in seconds. @zh 轨道声明的时长（单位秒） */
  duration?: number;
  /** First presentation time on the container timeline, in seconds, when determined. @zh 容器时间轴上的首次呈现时刻（单位秒，可确定时提供） */
  startTime?: number;
  /** Track language as stored in the container. @zh 容器记录的轨道语言 */
  language?: string;
  /** Track display name stored in the container. @zh 容器记录的轨道名称 */
  name?: string;
  /** Container default-track flag, when the format defines it. @zh 格式定义默认轨时的默认轨标记 */
  default?: boolean;
  /** Container forced-track flag, when the format defines it. @zh 格式定义强制轨时的强制轨标记 */
  forced?: boolean;
  /** Coded width in pixels. @zh 编码宽度（单位像素） */
  width?: Int;
  /** Coded height in pixels. @zh 编码高度（单位像素） */
  height?: Int;
  /** Display width divided by display height. @zh 显示宽高比（显示宽度 / 显示高度） */
  displayAspectRatio?: number;
  /** Clockwise display rotation in degrees when the transform is a pure rotation. @zh 顺时针显示旋转角度（变换为纯旋转时提供） */
  rotation?: number;
  /** Declared or average frame rate, in frames per second; not a VFR maximum. @zh 声明帧率或平均帧率（单位 fps，不表示可变帧率峰值） */
  frameRate?: number;
  /** Declared average bit rate in bits per second. @zh 声明的平均码率（单位 bit/s） */
  bitrate?: Int;
  /** Declared sample precision in bits. @zh 声明的采样精度（单位 bit） */
  bitDepth?: Int;
  /** Colour primaries identifier from the container or codec configuration. @zh 容器或编码配置中的色彩原色标识 */
  colorPrimaries?: Int;
  /** Transfer characteristic identifier, including HDR transfer functions. @zh 传递特性标识（含 HDR 传递函数） */
  colorTransfer?: Int;
  /** Matrix coefficients identifier. @zh 矩阵系数标识 */
  colorMatrix?: Int;
  /** Full-range video flag when explicitly present. @zh 视频全范围标记（显式存在时提供） */
  fullRange?: boolean;
  /** Audio sampling frequency in hertz. @zh 音频采样频率（单位 Hz） */
  sampleRate?: number;
  /** Declared audio channel count. @zh 声明的音频声道数 */
  channels?: Int;
}

/** One container attachment, with no content or extraction URL. */
interface ContainerAttachment {
  /** Container attachment identifier. @zh 容器附件标识 */
  id: string;
  /** Stored filename or descriptive name. @zh 存储的文件名或描述名称 */
  name: string;
  /** Declared attachment MIME. @zh 声明的附件 MIME 类型 */
  mimeType?: string;
  /** Attachment content length in bytes. @zh 附件内容字节数 */
  size?: Int;
}
