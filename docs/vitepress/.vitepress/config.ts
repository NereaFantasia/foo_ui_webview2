import { defineConfig, type DefaultTheme, type HeadConfig } from 'vitepress'
import { writeRedirectPages } from './published-urls.mjs'

// Public origin for GitHub Pages under /foo_ui_webview2/
const SITE_URL = 'https://nereafantasia.github.io/foo_ui_webview2/'
const SITE_ORIGIN = 'https://nereafantasia.github.io'
const SITE_BASE = '/foo_ui_webview2/'

const EN_DESCRIPTION =
  'foobar2000 WebView2 UI component: build custom foobar2000 interfaces with HTML/CSS/JavaScript + Vue/React, Windows 11 Mica/Acrylic, 400+ APIs, Web Components, and MCP AI integration.'

const ZH_DESCRIPTION =
  'foobar2000 WebView2 UI 插件：用 HTML/CSS/JavaScript + Vue/React 构建 foobar2000 自定义界面，支持 Windows 11 Mica/Acrylic 原生效果、400+ API、Web Components 与 MCP AI 集成。'

/**
 * Complete locale contract:
 * - root: English / en-US (default public entry)
 * - zh: 简体中文 / zh-CN under /zh/
 * - Per-page canonical / hreflang / x-default / og:url via transformHead
 * VitePress 1.6.4: i18nRouting is boolean only (not a callback).
 */

// Static pages under public/ (TypeDoc output) are shared by both locales; they carry an
// explicit `target` so the SPA router leaves them alone, and that also opts them out of the
// locale prefix.
const SDK_REFERENCE_LINK = { link: '/sdk-reference/index.html', target: '_self' }

function prefixLinks<T>(items: T[], prefix: string): T[] {
  return items.map((item) => {
    if (item && typeof item === 'object') {
      const next: any = { ...(item as any) }
      if (
        typeof next.link === 'string' &&
        next.link.startsWith('/') &&
        !next.link.startsWith(prefix) &&
        !next.target
      ) {
        next.link = prefix.replace(/\/$/, '') + next.link
      }
      if (Array.isArray(next.items)) next.items = prefixLinks(next.items, prefix)
      return next as T
    }
    return item
  })
}

function prefixSidebar(sidebar: DefaultTheme.Sidebar, prefix: string): DefaultTheme.Sidebar {
  if (Array.isArray(sidebar)) return prefixLinks(sidebar, prefix)
  const out: DefaultTheme.Sidebar = {}
  for (const [key, value] of Object.entries(sidebar)) {
    const nextKey =
      key.startsWith('/') && !key.startsWith(prefix)
        ? prefix.replace(/\/$/, '') + key
        : key
    out[nextKey] = Array.isArray(value) ? prefixLinks(value, prefix) : value
  }
  return out
}

const englishNav: DefaultTheme.NavItem[] = [
  { text: 'Tutorial', link: '/tutorials/first-theme' },
  { text: 'How-to', link: '/how-to/install' },
  { text: 'Concepts', link: '/concepts/architecture' },
  { text: 'SDK', link: '/sdk/overview' },
  { text: 'Low-level API', link: '/api/overview' },
  { text: 'Components', link: '/components/' },
  { text: 'Reference', link: '/reference/types' },
  { text: 'MCP', link: '/mcp/overview' },
  {
    text: 'v2.0.0',
    items: [{ text: 'Changelog', link: '/changelog' }],
  },
]

const chineseNav: DefaultTheme.NavItem[] = [
  { text: '教程', link: '/tutorials/first-theme' },
  { text: '指南', link: '/how-to/install' },
  { text: '概念', link: '/concepts/architecture' },
  { text: 'SDK', link: '/sdk/overview' },
  { text: '底层 API', link: '/api/overview' },
  { text: '组件', link: '/components/' },
  { text: '参考', link: '/reference/types' },
  { text: 'MCP', link: '/mcp/overview' },
  {
    text: 'v2.0.0',
    items: [{ text: '更新日志', link: '/changelog' }],
  },
]

const englishSidebar: DefaultTheme.Sidebar = {
  '/tutorials/': [
    {
      text: 'Tutorials',
      items: [{ text: 'Build your first theme', link: '/tutorials/first-theme' }],
    },
  ],
  '/how-to/': [
    {
      text: 'Set up',
      items: [
        { text: 'Install the component', link: '/how-to/install' },
        { text: 'Install a theme', link: '/how-to/install-theme' },
        { text: 'Put a page in a panel', link: '/how-to/panels' },
        { text: 'Run next to another interface', link: '/how-to/background-mode' },
      ],
    },
    {
      text: 'Build themes',
      items: [
        { text: 'Debug with the development server', link: '/how-to/dev-server' },
        { text: 'Load the SDK', link: '/how-to/load-sdk' },
        { text: 'Write a theme without a build step', link: '/how-to/plain-html' },
        { text: 'Style the components', link: '/how-to/style-components' },
        { text: 'Common tasks', link: '/how-to/common-tasks' },
      ],
    },
  ],
  '/concepts/': [
    {
      text: 'Concepts',
      items: [
        { text: 'How it works', link: '/concepts/architecture' },
        { text: 'How a page is found', link: '/concepts/page-loading' },
        { text: 'Run modes', link: '/concepts/run-modes' },
        { text: 'How the components work', link: '/concepts/components' },
      ],
    },
  ],
  '/sdk/': [
    {
      text: 'SDK (recommended)',
      items: [
        { text: 'SDK Overview', link: '/sdk/overview' },
        { text: 'Namespaces', link: '/sdk/namespaces' },
        { text: 'Events', link: '/sdk/events' },
        { text: 'fb.state Reactive State', link: '/sdk/state' },
      ],
    },
    // One page per namespace, grouped as on the Namespaces page and sorted by name.
    {
      text: 'Core media and UI',
      collapsed: false,
      items: [
        { text: 'fb.artwork Artwork', link: '/sdk/artwork' },
        { text: 'fb.audio Audio Analysis', link: '/sdk/audio' },
        { text: 'fb.config Config', link: '/sdk/config' },
        { text: 'fb.dsp DSP Chain', link: '/sdk/dsp' },
        { text: 'fb.jitQueue Just-in-time Queue', link: '/sdk/jit-queue' },
        { text: 'fb.library Media Library', link: '/sdk/library' },
        { text: 'fb.output Audio Output', link: '/sdk/output' },
        { text: 'fb.player Playback', link: '/sdk/player' },
        { text: 'fb.media Local Media', link: '/sdk/media' },
        { text: 'fb.playlist Playlists', link: '/sdk/playlist' },
        { text: 'fb.queue Playback Queue', link: '/sdk/queue' },
        { text: 'fb.replaygain ReplayGain', link: '/sdk/replaygain' },
        { text: 'fb.ui Window', link: '/sdk/ui' },
      ],
    },
    {
      text: 'Metadata and data access',
      collapsed: false,
      items: [
        { text: 'fb.clipboard Clipboard', link: '/sdk/clipboard' },
        { text: 'fb.dialog Dialogs', link: '/sdk/dialog' },
        { text: 'fb.file File System', link: '/sdk/file' },
        { text: 'fb.http HTTP', link: '/sdk/http' },
        { text: 'fb.metadata Metadata', link: '/sdk/metadata' },
        { text: 'fb.playcount Playback Statistics', link: '/sdk/playcount' },
        { text: 'fb.rating Ratings', link: '/sdk/rating' },
        { text: 'fb.selection Selection', link: '/sdk/selection' },
        { text: 'fb.titleformat Title Formatting', link: '/sdk/titleformat' },
      ],
    },
    {
      text: 'Cross-window and desktop integration',
      collapsed: false,
      items: [
        { text: 'fb.cursor Cursor', link: '/sdk/cursor' },
        { text: 'fb.discovery Service Discovery', link: '/sdk/discovery' },
        { text: 'fb.event Cross-window Events', link: '/sdk/event' },
        { text: 'fb.keyboard Hotkeys', link: '/sdk/keyboard' },
        { text: 'fb.port Cross-window Ports', link: '/sdk/port' },
        { text: 'fb.sharedState Shared State', link: '/sdk/shared-state' },
        { text: 'fb.shell Shell', link: '/sdk/shell' },
        { text: 'fb.taskbar Taskbar', link: '/sdk/taskbar' },
        { text: 'fb.tray Tray', link: '/sdk/tray' },
      ],
    },
    {
      text: 'Utilities and host services',
      collapsed: false,
      items: [
        { text: 'fb.console Console', link: '/sdk/console' },
        { text: 'fb.dnd Drag and Drop', link: '/sdk/dnd' },
        { text: 'fb.log Log File', link: '/sdk/log' },
        { text: 'fb.lyrics Lyrics', link: '/sdk/lyrics' },
        { text: 'fb.menu Menu', link: '/sdk/menu' },
        { text: 'fb.misc Host Actions', link: '/sdk/misc' },
        { text: 'fb.notification Notifications', link: '/sdk/notification' },
        { text: 'fb.panel Panel Config', link: '/sdk/panel' },
        { text: 'fb.system System', link: '/sdk/system' },
        { text: 'fb.utils Utils', link: '/sdk/utils' },
        { text: 'fb.webview WebView', link: '/sdk/webview' },
      ],
    },
    {
      text: 'Generated reference',
      items: [{ text: 'SDK type reference (TypeDoc)', ...SDK_REFERENCE_LINK }],
    },
  ],
  '/api/': [
    {
      text: 'Core API',
      items: [
        { text: 'Overview', link: '/api/overview' },
        { text: 'Playback', link: '/api/playback' },
        { text: 'Playlist', link: '/api/playlist' },
        { text: 'Library', link: '/api/library' },
        { text: 'Artwork', link: '/api/artwork' },
        { text: 'Lyrics', link: '/api/lyrics' },
        { text: 'Window', link: '/api/window' },
        { text: 'Taskbar', link: '/api/taskbar' },
        { text: 'Tray', link: '/api/tray' },
        { text: 'Config', link: '/api/config' },
        { text: 'Cursor', link: '/api/cursor' },
      ],
    },
    {
      text: 'Extended API',
      items: [
        { text: 'Metadata', link: '/api/metadata' },
        { text: 'Rating', link: '/api/rating' },
        { text: 'Titleformat', link: '/api/titleformat' },
        { text: 'Playcount', link: '/api/playcount' },
        { text: 'Audio', link: '/api/audio' },
        { text: 'Media', link: '/api/media' },
        { text: 'DSP', link: '/api/dsp' },
        { text: 'Output', link: '/api/output' },
        { text: 'ReplayGain', link: '/api/replaygain' },
        { text: 'Queue', link: '/api/queue' },
        { text: 'JIT Queue', link: '/api/jit-queue' },
        { text: 'Selection', link: '/api/selection' },
        { text: 'Discovery', link: '/api/discovery' },
        { text: 'Port', link: '/api/port' },
        { text: 'Event', link: '/api/event' },
        { text: 'State', link: '/api/state' },
        { text: 'Events', link: '/api/events' },
      ],
    },
    {
      text: 'Utility API',
      items: [
        { text: 'File', link: '/api/file' },
        { text: 'Dialog', link: '/api/dialog' },
        { text: 'Shell', link: '/api/shell' },
        { text: 'HTTP', link: '/api/http' },
        { text: 'UI', link: '/api/ui' },
        { text: 'Keyboard', link: '/api/keyboard' },
        { text: 'Drag and Drop', link: '/api/dnd' },
        { text: 'Clipboard', link: '/api/clipboard' },
        { text: 'Console', link: '/api/console' },
        { text: 'Log', link: '/api/log' },
        { text: 'Menu', link: '/api/menu' },
        { text: 'Panel', link: '/api/panel' },
        { text: 'WebView', link: '/api/webview' },
        { text: 'System', link: '/api/system' },
        { text: 'Misc', link: '/api/misc' },
        { text: 'Test', link: '/api/test' },
      ],
    },
  ],
  '/components/': [
    {
      text: 'Web Components',
      items: [
        { text: 'Overview', link: '/components/' },
        { text: 'A. Playback Controls', link: '/components/play-controls' },
        { text: 'B. Progress & Volume', link: '/components/progress' },
        { text: 'C. Track Info', link: '/components/track-info' },
        { text: 'D. Playlist', link: '/components/playlist' },
        { text: 'E. Window', link: '/components/window' },
        { text: 'F. Lyrics & Visualization', link: '/components/media' },
        { text: 'G. Rating & Audio Settings', link: '/components/audio' },
        { text: 'H. Metadata & Search', link: '/components/metadata' },
        { text: 'I. Media Library', link: '/components/library' },
      ],
    },
  ],
  '/mcp/': [
    {
      text: 'MCP Server',
      items: [
        { text: 'Overview', link: '/mcp/overview' },
        { text: 'Setup', link: '/mcp/setup' },
        { text: 'Tools', link: '/mcp/tools' },
      ],
    },
  ],
  '/reference/': [
    {
      text: 'Reference',
      items: [
        { text: 'Bridge Protocol', link: '/reference/bridge' },
        { text: 'Preferences', link: '/reference/preferences' },
        { text: 'Events', link: '/reference/events' },
        { text: 'Shared Types', link: '/reference/types' },
        { text: 'Error Handling', link: '/reference/errors' },
        { text: 'Security Limits', link: '/reference/security' },
        { text: 'Permissions', link: '/reference/permissions' },
        { text: 'Playcount & Rating', link: '/reference/stats' },
        { text: 'Titleformat & ReplayGain', link: '/reference/titleformat-replaygain' },
        { text: 'SMP Compatibility', link: '/reference/smp-compat' },
        { text: 'Test API', link: '/reference/test' },
        { text: 'Versioning & Compatibility', link: '/reference/versioning' },
      ],
    },
  ],
}

const chineseSidebar: DefaultTheme.Sidebar = {
  '/tutorials/': [
    {
      text: '教程',
      items: [{ text: '构建第一个主题', link: '/tutorials/first-theme' }],
    },
  ],
  '/how-to/': [
    {
      text: '安装与设置',
      items: [
        { text: '安装组件', link: '/how-to/install' },
        { text: '安装主题', link: '/how-to/install-theme' },
        { text: '把页面放进面板', link: '/how-to/panels' },
        { text: '与其他界面同时运行', link: '/how-to/background-mode' },
      ],
    },
    {
      text: '开发主题',
      items: [
        { text: '用开发服务器调试', link: '/how-to/dev-server' },
        { text: '加载 SDK', link: '/how-to/load-sdk' },
        { text: '不用构建工具写主题', link: '/how-to/plain-html' },
        { text: '给组件加样式', link: '/how-to/style-components' },
        { text: '常见任务', link: '/how-to/common-tasks' },
      ],
    },
  ],
  '/concepts/': [
    {
      text: '概念',
      items: [
        { text: '工作原理', link: '/concepts/architecture' },
        { text: '页面从哪里加载', link: '/concepts/page-loading' },
        { text: '运行模式', link: '/concepts/run-modes' },
        { text: '组件的工作方式', link: '/concepts/components' },
      ],
    },
  ],
  '/sdk/': [
    {
      text: 'SDK（推荐）',
      items: [
        { text: 'SDK 概述', link: '/sdk/overview' },
        { text: '命名空间', link: '/sdk/namespaces' },
        { text: '事件系统', link: '/sdk/events' },
        { text: 'fb.state 响应式状态', link: '/sdk/state' },
      ],
    },
    {
      text: '核心命名空间',
      collapsed: false,
      items: [
        { text: 'fb.artwork 封面', link: '/sdk/artwork' },
        { text: 'fb.audio 音频分析', link: '/sdk/audio' },
        { text: 'fb.config 配置', link: '/sdk/config' },
        { text: 'fb.dsp DSP 链', link: '/sdk/dsp' },
        { text: 'fb.jitQueue JIT 即时队列', link: '/sdk/jit-queue' },
        { text: 'fb.library 媒体库', link: '/sdk/library' },
        { text: 'fb.output 音频输出', link: '/sdk/output' },
        { text: 'fb.player 播放控制', link: '/sdk/player' },
        { text: 'fb.media 本地媒体', link: '/sdk/media' },
        { text: 'fb.playlist 播放列表', link: '/sdk/playlist' },
        { text: 'fb.queue 播放队列', link: '/sdk/queue' },
        { text: 'fb.replaygain ReplayGain', link: '/sdk/replaygain' },
        { text: 'fb.ui 窗口', link: '/sdk/ui' },
      ],
    },
    {
      text: '元数据与数据访问',
      collapsed: false,
      items: [
        { text: 'fb.clipboard 剪贴板', link: '/sdk/clipboard' },
        { text: 'fb.dialog 对话框', link: '/sdk/dialog' },
        { text: 'fb.file 文件系统', link: '/sdk/file' },
        { text: 'fb.http HTTP 请求', link: '/sdk/http' },
        { text: 'fb.metadata 元数据', link: '/sdk/metadata' },
        { text: 'fb.playcount 播放统计', link: '/sdk/playcount' },
        { text: 'fb.rating 评分', link: '/sdk/rating' },
        { text: 'fb.selection 选择同步', link: '/sdk/selection' },
        { text: 'fb.titleformat 标题格式化', link: '/sdk/titleformat' },
      ],
    },
    {
      text: '跨窗口与桌面集成',
      collapsed: false,
      items: [
        { text: 'fb.cursor 光标', link: '/sdk/cursor' },
        { text: 'fb.discovery 服务发现', link: '/sdk/discovery' },
        { text: 'fb.event 跨窗口事件', link: '/sdk/event' },
        { text: 'fb.keyboard 热键', link: '/sdk/keyboard' },
        { text: 'fb.port 跨窗口端口', link: '/sdk/port' },
        { text: 'fb.sharedState 共享状态', link: '/sdk/shared-state' },
        { text: 'fb.shell 系统集成', link: '/sdk/shell' },
        { text: 'fb.taskbar 任务栏', link: '/sdk/taskbar' },
        { text: 'fb.tray 托盘', link: '/sdk/tray' },
      ],
    },
    {
      text: '工具与宿主服务',
      collapsed: false,
      items: [
        { text: 'fb.console 控制台', link: '/sdk/console' },
        { text: 'fb.dnd 拖放', link: '/sdk/dnd' },
        { text: 'fb.log 日志文件', link: '/sdk/log' },
        { text: 'fb.lyrics 歌词', link: '/sdk/lyrics' },
        { text: 'fb.menu 菜单', link: '/sdk/menu' },
        { text: 'fb.misc 杂项工具', link: '/sdk/misc' },
        { text: 'fb.notification 通知', link: '/sdk/notification' },
        { text: 'fb.panel 面板配置', link: '/sdk/panel' },
        { text: 'fb.system 系统', link: '/sdk/system' },
        { text: 'fb.utils 工具', link: '/sdk/utils' },
        { text: 'fb.webview WebView', link: '/sdk/webview' },
      ],
    },
    {
      text: '生成的参考',
      items: [{ text: 'SDK 类型参考（英文，TypeDoc）', ...SDK_REFERENCE_LINK }],
    },
  ],
  '/api/': [
    {
      text: '核心 API',
      items: [
        { text: '概述', link: '/api/overview' },
        { text: 'Playback 播放', link: '/api/playback' },
        { text: 'Playlist 播放列表', link: '/api/playlist' },
        { text: 'Library 媒体库', link: '/api/library' },
        { text: 'Artwork 封面', link: '/api/artwork' },
        { text: 'Lyrics 歌词', link: '/api/lyrics' },
        { text: 'Window 窗口', link: '/api/window' },
        { text: 'Taskbar 任务栏', link: '/api/taskbar' },
        { text: 'Tray 托盘', link: '/api/tray' },
        { text: 'Config 配置', link: '/api/config' },
        { text: 'Cursor 光标', link: '/api/cursor' },
      ],
    },
    {
      text: '扩展 API',
      items: [
        { text: 'Metadata 元数据', link: '/api/metadata' },
        { text: 'Rating 评分', link: '/api/rating' },
        { text: 'Titleformat 格式化', link: '/api/titleformat' },
        { text: 'Playcount 播放统计', link: '/api/playcount' },
        { text: 'Audio 音频', link: '/api/audio' },
        { text: 'Media 媒体', link: '/api/media' },
        { text: 'DSP 音效处理', link: '/api/dsp' },
        { text: 'Output 输出设备', link: '/api/output' },
        { text: 'ReplayGain 回放增益', link: '/api/replaygain' },
        { text: 'Queue 播放队列', link: '/api/queue' },
        { text: 'JIT Queue 即时队列', link: '/api/jit-queue' },
        { text: 'Selection 选择', link: '/api/selection' },
        { text: 'Discovery 服务发现', link: '/api/discovery' },
        { text: 'Port 跨窗口端口', link: '/api/port' },
        { text: 'Event 自定义事件', link: '/api/event' },
        { text: 'State 共享状态', link: '/api/state' },
        { text: 'Events 事件系统', link: '/api/events' },
      ],
    },
    {
      text: '工具 API',
      items: [
        { text: 'File 文件', link: '/api/file' },
        { text: 'Dialog 对话框', link: '/api/dialog' },
        { text: 'Shell 系统外壳', link: '/api/shell' },
        { text: 'HTTP 网络请求', link: '/api/http' },
        { text: 'UI 界面', link: '/api/ui' },
        { text: 'Keyboard 键盘', link: '/api/keyboard' },
        { text: 'DnD 拖放', link: '/api/dnd' },
        { text: 'Clipboard 剪贴板', link: '/api/clipboard' },
        { text: 'Console 控制台', link: '/api/console' },
        { text: 'Log 日志文件', link: '/api/log' },
        { text: 'Menu 菜单', link: '/api/menu' },
        { text: 'Panel 面板', link: '/api/panel' },
        { text: 'WebView', link: '/api/webview' },
        { text: 'System 系统', link: '/api/system' },
        { text: 'Misc 杂项', link: '/api/misc' },
        { text: 'Test 测试', link: '/api/test' },
      ],
    },
  ],
  '/components/': [
    {
      text: 'Web Components',
      items: [
        { text: '总览', link: '/components/' },
        { text: 'A. 播放控制', link: '/components/play-controls' },
        { text: 'B. 进度与音量', link: '/components/progress' },
        { text: 'C. 曲目信息', link: '/components/track-info' },
        { text: 'D. 播放列表', link: '/components/playlist' },
        { text: 'E. 窗口管理', link: '/components/window' },
        { text: 'F. 歌词与可视化', link: '/components/media' },
        { text: 'G. 评分与音频设置', link: '/components/audio' },
        { text: 'H. 元数据与搜索', link: '/components/metadata' },
        { text: 'I. 媒体库', link: '/components/library' },
      ],
    },
  ],
  '/mcp/': [
    {
      text: 'MCP Server',
      items: [
        { text: '概述', link: '/mcp/overview' },
        { text: '安装与配置', link: '/mcp/setup' },
        { text: '工具', link: '/mcp/tools' },
      ],
    },
  ],
  '/reference/': [
    {
      text: '参考',
      items: [
        { text: 'Bridge 协议', link: '/reference/bridge' },
        { text: '偏好设置', link: '/reference/preferences' },
        { text: '事件系统', link: '/reference/events' },
        { text: '共享类型', link: '/reference/types' },
        { text: '错误处理', link: '/reference/errors' },
        { text: '安全限制', link: '/reference/security' },
        { text: '权限系统', link: '/reference/permissions' },
        { text: 'Playcount & Rating', link: '/reference/stats' },
        { text: 'Titleformat & ReplayGain', link: '/reference/titleformat-replaygain' },
        { text: 'SMP 兼容层', link: '/reference/smp-compat' },
        { text: 'Test API', link: '/reference/test' },
        { text: '版本与兼容性', link: '/reference/versioning' },
      ],
    },
  ],
}

const englishThemeLabels = {
  editLink: {
    pattern: 'https://github.com/NereaFantasia/foo_ui_webview2/edit/main/docs/vitepress/:path',
    text: 'Edit this page on GitHub',
  },
  docFooter: { prev: 'Previous page', next: 'Next page' },
  outline: { label: 'On this page', level: [2, 3] as [number, number] },
  lastUpdated: { text: 'Last updated' },
  returnToTopLabel: 'Return to top',
  darkModeSwitchLabel: 'Appearance',
  lightModeSwitchTitle: 'Switch to light theme',
  darkModeSwitchTitle: 'Switch to dark theme',
  sidebarMenuLabel: 'Menu',
  langMenuLabel: 'Change language',
  skipToContentLabel: 'Skip to content',
  notFound: {
    title: 'PAGE NOT FOUND',
    quote: 'The page you are looking for does not exist or has been moved.',
    linkLabel: 'go to home',
    linkText: 'Take me home',
    code: '404',
  },
}

const chineseThemeLabels = {
  editLink: {
    pattern: 'https://github.com/NereaFantasia/foo_ui_webview2/edit/main/docs/vitepress/:path',
    text: '在 GitHub 上编辑此页',
  },
  docFooter: { prev: '上一页', next: '下一页' },
  outline: { label: '页面导航', level: [2, 3] as [number, number] },
  lastUpdated: { text: '最后更新于' },
  returnToTopLabel: '回到顶部',
  darkModeSwitchLabel: '主题',
  lightModeSwitchTitle: '切换到浅色模式',
  darkModeSwitchTitle: '切换到深色模式',
  sidebarMenuLabel: '菜单',
  langMenuLabel: '切换语言',
  skipToContentLabel: '跳到正文',
  notFound: {
    title: '页面未找到',
    quote: '你访问的页面不存在或已被移动。',
    linkLabel: '返回首页',
    linkText: '回到首页',
    code: '404',
  },
}

const englishSearchTranslations = {
  button: { buttonText: 'Search', buttonAriaLabel: 'Search docs' },
  modal: {
    displayDetails: 'Display detailed list',
    resetButtonTitle: 'Reset search',
    backButtonTitle: 'Close search',
    noResultsText: 'No results for',
    footer: { selectText: 'to select', navigateText: 'to navigate', closeText: 'to close' },
  },
}

const chineseSearchTranslations = {
  button: { buttonText: '搜索文档', buttonAriaLabel: '搜索文档' },
  modal: {
    displayDetails: '显示详细列表',
    resetButtonTitle: '清除查询条件',
    backButtonTitle: '关闭搜索',
    noResultsText: '无法找到相关结果',
    footer: { selectText: '选择', navigateText: '切换', closeText: '关闭' },
  },
}

function staticLocaleHead(description: string, locale: 'en_US' | 'zh_CN'): HeadConfig[] {
  const title =
    locale === 'en_US'
      ? 'foo_ui_webview2 — foobar2000 WebView2 UI Component'
      : 'foo_ui_webview2 — foobar2000 WebView2 UI 插件'
  return [
    ['meta', { name: 'theme-color', content: '#0ea5e9' }],
    ['meta', { name: 'theme-color', content: '#0d1117', media: '(prefers-color-scheme: dark)' }],
    [
      'meta',
      {
        name: 'keywords',
        content:
          'foobar2000, foobar2000 webview2, foobar2000 web ui, foobar2000 custom ui, foobar2000 theme, foobar2000 skin, webview2, web components, mica, acrylic, windows 11, foobar2000 sdk, foobar2000 mcp, music player ui',
      },
    ],
    ['meta', { name: 'author', content: 'NereaFantasia' }],
    ['meta', { property: 'og:type', content: 'website' }],
    ['meta', { property: 'og:site_name', content: 'foo_ui_webview2' }],
    ['meta', { property: 'og:title', content: title }],
    ['meta', { property: 'og:description', content: description }],
    // og:url is injected per-page in transformHead (must not be static homepage only)
    ['meta', { property: 'og:locale', content: locale }],
    ['meta', { name: 'twitter:card', content: 'summary' }],
    ['meta', { name: 'twitter:title', content: title }],
    ['meta', { name: 'twitter:description', content: description }],
    ['link', { rel: 'icon', type: 'image/svg+xml', href: '/foo_ui_webview2/favicon.svg' }],
    ['meta', { name: 'google-site-verification', content: 'kqICJvELP7gyAV9Ol8QqbRhYUP0KQ3jBIiA94foqztY' }],
  ]
}

/** Map relative page path to public URL path under SITE_BASE. */
function pageToPublicPath(relativePath: string): string {
  let p = String(relativePath || '').replaceAll('\\', '/')
  if (!p || p === '404.md') return ''
  if (p.endsWith('/index.md')) p = p.slice(0, -'/index.md'.length) + '/'
  else if (p === 'index.md') p = ''
  else if (p.endsWith('.md')) p = p.slice(0, -3) + '.html'
  if (p && !p.startsWith('/') && !p.endsWith('/') && !p.endsWith('.html')) {
    // directory-like components path already handled
  }
  return p.replace(/^\/+/, '')
}

function publicUrl(relativePath: string, isZh: boolean): string {
  const localeRelativePath = isZh
    ? String(relativePath || '').replaceAll('\\', '/').replace(/^zh\//, '')
    : relativePath
  const page = pageToPublicPath(localeRelativePath)
  const localePrefix = isZh ? 'zh/' : ''
  const joined = `${SITE_BASE}${localePrefix}${page}`.replace(/\/{2,}/g, '/')
  // Ensure trailing slash only for home-like paths already ending with /
  return SITE_ORIGIN + joined
}

function correspondingRelativePath(relativePath: string, toZh: boolean): string {
  let p = String(relativePath || '').replaceAll('\\', '/')
  const isZh = p === 'zh' || p.startsWith('zh/')
  if (toZh && !isZh) return p === 'index.md' ? 'zh/index.md' : `zh/${p}`
  if (!toZh && isZh) return p.replace(/^zh\//, '') || 'index.md'
  return p
}

export default defineConfig({
  // Site-level default language is English (root).
  lang: 'en-US',
  title: 'foo_ui_webview2',
  description: EN_DESCRIPTION,
  sitemap: { hostname: SITE_URL },
  base: SITE_BASE,
  outDir: './dist',
  ignoreDeadLinks: [/localhost/],
  lastUpdated: true,
  appearance: {
    initialValue: 'dark',
    // VitePress 1.6.4 appearance toggle title
  },
  markdown: {
    lineNumbers: true,
    container: {
      tipLabel: 'TIP',
      warningLabel: 'WARNING',
      dangerLabel: 'DANGER',
      infoLabel: 'INFO',
      detailsLabel: 'Details',
    },
    theme: {
      light: 'github-light',
      dark: 'github-dark',
    },
  },

  locales: {
    root: {
      label: 'English',
      lang: 'en-US',
      title: 'foo_ui_webview2',
      description: EN_DESCRIPTION,
      head: staticLocaleHead(EN_DESCRIPTION, 'en_US'),
      themeConfig: {
        siteTitle: 'foo_ui_webview2 API',
        logo: '/favicon.svg',
        socialLinks: [{ icon: 'github', link: 'https://github.com/NereaFantasia/foo_ui_webview2' }],
        nav: englishNav,
        sidebar: englishSidebar,
        ...englishThemeLabels,
        // VitePress 1.6.4: boolean only (not a callback)
        i18nRouting: true,
      },
    },
    zh: {
      label: '简体中文',
      lang: 'zh-CN',
      link: '/zh/',
      title: 'foo_ui_webview2',
      description: ZH_DESCRIPTION,
      head: staticLocaleHead(ZH_DESCRIPTION, 'zh_CN'),
      themeConfig: {
        siteTitle: 'foo_ui_webview2 API',
        logo: '/favicon.svg',
        socialLinks: [{ icon: 'github', link: 'https://github.com/NereaFantasia/foo_ui_webview2' }],
        nav: prefixLinks(chineseNav, '/zh'),
        sidebar: prefixSidebar(chineseSidebar, '/zh'),
        ...chineseThemeLabels,
        i18nRouting: true,
      },
    },
  },

  themeConfig: {
    search: {
      provider: 'local',
      options: {
        locales: {
          root: { translations: englishSearchTranslations },
          zh: { translations: chineseSearchTranslations },
        },
      },
    },
  },

  /**
   * Per-page SEO: canonical, hreflang (en / zh-CN / x-default), og:url.
   * Must use public origin + base; never preview/localhost hosts.
   */
  transformHead({ pageData }) {
    const rel = String(pageData.relativePath || '').replaceAll('\\', '/')
    if (!rel || pageData.isNotFound || rel === '404.md') return []

    const isZh = rel === 'zh' || rel.startsWith('zh/')
    const enRel = correspondingRelativePath(rel, false)
    const zhRel = correspondingRelativePath(rel, true)
    const selfUrl = publicUrl(rel, isZh)
    const enUrl = publicUrl(enRel, false)
    const zhUrl = publicUrl(zhRel, true)

    const tags: HeadConfig[] = [
      ['link', { rel: 'canonical', href: selfUrl }],
      ['link', { rel: 'alternate', hreflang: 'en', href: enUrl }],
      ['link', { rel: 'alternate', hreflang: 'zh-CN', href: zhUrl }],
      ['link', { rel: 'alternate', hreflang: 'x-default', href: enUrl }],
      ['meta', { property: 'og:url', content: selfUrl }],
    ]
    return tags
  },

  // Old URLs of moved pages get redirect pages; see published-urls.mjs.
  buildEnd(siteConfig) {
    writeRedirectPages({
      docsRoot: siteConfig.srcDir,
      outDir: siteConfig.outDir,
      base: siteConfig.site.base,
      origin: SITE_ORIGIN,
    })
  },
})
