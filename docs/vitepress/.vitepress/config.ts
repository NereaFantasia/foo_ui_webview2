import { defineConfig, type DefaultTheme, type HeadConfig } from 'vitepress'

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

function prefixLinks<T>(items: T[], prefix: string): T[] {
  return items.map((item) => {
    if (item && typeof item === 'object') {
      const next: any = { ...(item as any) }
      if (
        typeof next.link === 'string' &&
        next.link.startsWith('/') &&
        !next.link.startsWith(prefix)
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
  { text: 'Guide', link: '/guide/overview' },
  { text: 'SDK', link: '/sdk/overview' },
  { text: 'Low-level API', link: '/api/overview' },
  { text: 'Components', link: '/components/' },
  { text: 'Reference', link: '/reference/events' },
  { text: 'MCP', link: '/mcp/overview' },
  {
    text: 'v1.12.0',
    items: [{ text: 'Changelog', link: '/changelog' }],
  },
]

const chineseNav: DefaultTheme.NavItem[] = [
  { text: '入门', link: '/guide/overview' },
  { text: 'SDK', link: '/sdk/overview' },
  { text: '底层 API', link: '/api/overview' },
  { text: '组件', link: '/components/' },
  { text: '参考', link: '/reference/events' },
  { text: 'MCP', link: '/mcp/overview' },
  {
    text: 'v1.12.0',
    items: [{ text: '更新日志', link: '/changelog' }],
  },
]

const englishSidebar: DefaultTheme.Sidebar = {
  '/guide/': [
    {
      text: 'Guide',
      items: [
        { text: 'Overview', link: '/guide/overview' },
        { text: 'Installation', link: '/guide/installation' },
        { text: 'Quick Start', link: '/guide/quickstart' },
        { text: 'Panel Modes', link: '/guide/panel-modes' },
        { text: 'Bridge Protocol', link: '/guide/bridge' },
        { text: 'Use Cases', link: '/guide/use-cases' },
      ],
    },
  ],
  '/sdk/': [
    {
      text: 'SDK (recommended)',
      items: [
        { text: 'SDK Overview & Install', link: '/sdk/overview' },
        { text: 'Quick Start', link: '/sdk/quickstart' },
        { text: 'Namespaces', link: '/sdk/namespaces' },
        { text: 'Events', link: '/sdk/events' },
      ],
    },
    {
      text: 'SDK API reference',
      items: [
        { text: 'fb.player Playback', link: '/sdk/player' },
        { text: 'fb.playlist Playlists', link: '/sdk/playlist' },
        { text: 'fb.library Media Library', link: '/sdk/library' },
        { text: 'fb.artwork Artwork', link: '/sdk/artwork' },
        { text: 'fb.config Config', link: '/sdk/config' },
        { text: 'fb.ui Window', link: '/sdk/ui' },
        { text: 'fb.system System', link: '/sdk/system' },
        { text: 'fb.audio Audio', link: '/sdk/audio' },
        { text: 'fb.shell Shell', link: '/sdk/shell' },
        { text: 'fb.tray Tray', link: '/sdk/tray' },
        { text: 'fb.menu Menu', link: '/sdk/menu' },
        { text: 'fb.state Reactive State', link: '/sdk/state' },
        { text: 'fb.utils Utils', link: '/sdk/utils' },
      ],
    },
    {
      text: 'Extended API reference',
      items: [
        { text: 'File & Network', link: '/sdk/file-io' },
        { text: 'Audio Extensions', link: '/sdk/audio-ext' },
        { text: 'Queue & Discovery', link: '/sdk/navigation' },
        { text: 'Data & Metadata', link: '/sdk/data' },
        { text: 'Menu & Misc', link: '/sdk/misc' },
      ],
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
        { text: 'Taskbar & Tray', link: '/api/taskbar-tray' },
        { text: 'Config', link: '/api/config' },
        { text: 'Cursor', link: '/api/cursor' },
      ],
    },
    {
      text: 'Extended API',
      items: [
        { text: 'Metadata', link: '/api/metadata' },
        { text: 'Titleformat', link: '/api/titleformat' },
        { text: 'Playcount', link: '/api/playcount' },
        { text: 'Audio & DSP & Output', link: '/api/audio' },
        { text: 'Queue & Selection', link: '/api/queue' },
        { text: 'Discovery', link: '/api/discovery' },
        { text: 'Port / Event / State', link: '/api/port' },
        { text: 'Events', link: '/api/events' },
      ],
    },
    {
      text: 'Utility API',
      items: [
        { text: 'File & Dialog & Shell', link: '/api/file' },
        { text: 'HTTP', link: '/api/http' },
        { text: 'UI & Keyboard & DnD', link: '/api/ui-interaction' },
        { text: 'Misc (Clipboard/Console/...)', link: '/api/misc' },
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
      ],
    },
    {
      text: 'Bridge tools',
      items: [
        { text: 'Playback', link: '/mcp/tools-playback' },
        { text: 'Playback Ext', link: '/mcp/tools-playback-ext' },
        { text: 'Playlist', link: '/mcp/tools-playlist' },
        { text: 'Playlist Ext', link: '/mcp/tools-playlist-ext' },
        { text: 'Library', link: '/mcp/tools-library' },
        { text: 'Artwork', link: '/mcp/tools-artwork' },
        { text: 'Queue', link: '/mcp/tools-queue' },
        { text: 'Metadata', link: '/mcp/tools-metadata' },
      ],
    },
    {
      text: 'UI tools',
      items: [
        { text: 'Screenshot & Debug', link: '/mcp/tools-ui' },
      ],
    },
  ],
  '/reference/': [
    {
      text: 'Reference',
      items: [
        { text: 'Events', link: '/reference/events' },
        { text: 'Error Handling', link: '/reference/errors' },
        { text: 'Security Limits', link: '/reference/security' },
        { text: 'Permissions', link: '/reference/permissions' },
        { text: 'Playcount & Rating', link: '/reference/stats' },
        { text: 'Titleformat & ReplayGain', link: '/reference/titleformat-replaygain' },
        { text: 'SMP Compatibility', link: '/reference/smp-compat' },
        { text: 'Test API', link: '/reference/test' },
      ],
    },
  ],
}

const chineseSidebar: DefaultTheme.Sidebar = {
  '/guide/': [
    {
      text: '入门',
      items: [
        { text: '概述', link: '/guide/overview' },
        { text: '安装配置', link: '/guide/installation' },
        { text: '快速开始', link: '/guide/quickstart' },
        { text: '运行模式', link: '/guide/panel-modes' },
        { text: 'Bridge 协议', link: '/guide/bridge' },
        { text: '常用场景', link: '/guide/use-cases' },
      ],
    },
  ],
  '/sdk/': [
    {
      text: 'SDK（推荐）',
      items: [
        { text: 'SDK 概述 & 安装', link: '/sdk/overview' },
        { text: '快速入门', link: '/sdk/quickstart' },
        { text: '命名空间', link: '/sdk/namespaces' },
        { text: '事件系统', link: '/sdk/events' },
      ],
    },
    {
      text: 'SDK API 参考',
      items: [
        { text: 'fb.player 播放控制', link: '/sdk/player' },
        { text: 'fb.playlist 播放列表', link: '/sdk/playlist' },
        { text: 'fb.library 媒体库', link: '/sdk/library' },
        { text: 'fb.artwork 封面', link: '/sdk/artwork' },
        { text: 'fb.config 配置', link: '/sdk/config' },
        { text: 'fb.ui 窗口', link: '/sdk/ui' },
        { text: 'fb.system 系统', link: '/sdk/system' },
        { text: 'fb.audio 音频', link: '/sdk/audio' },
        { text: 'fb.shell 系统集成', link: '/sdk/shell' },
        { text: 'fb.tray 托盘', link: '/sdk/tray' },
        { text: 'fb.menu 菜单', link: '/sdk/menu' },
        { text: 'fb.state 响应式状态', link: '/sdk/state' },
        { text: 'fb.utils 工具', link: '/sdk/utils' },
      ],
    },
    {
      text: '扩展 API 参考',
      items: [
        { text: '文件与网络', link: '/sdk/file-io' },
        { text: '音频扩展', link: '/sdk/audio-ext' },
        { text: '队列与发现', link: '/sdk/navigation' },
        { text: '数据与元信息', link: '/sdk/data' },
        { text: '菜单与杂项', link: '/sdk/misc' },
      ],
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
        { text: 'Taskbar & Tray', link: '/api/taskbar-tray' },
        { text: 'Config 配置', link: '/api/config' },
        { text: 'Cursor 光标', link: '/api/cursor' },
      ],
    },
    {
      text: '扩展 API',
      items: [
        { text: 'Metadata 元数据', link: '/api/metadata' },
        { text: 'Titleformat 格式化', link: '/api/titleformat' },
        { text: 'Playcount 播放统计', link: '/api/playcount' },
        { text: 'Audio & DSP & Output', link: '/api/audio' },
        { text: 'Queue & Selection', link: '/api/queue' },
        { text: 'Discovery 服务发现', link: '/api/discovery' },
        { text: 'Port / Event / State', link: '/api/port' },
        { text: 'Events 事件系统', link: '/api/events' },
      ],
    },
    {
      text: '工具 API',
      items: [
        { text: 'File & Dialog & Shell', link: '/api/file' },
        { text: 'HTTP 网络请求', link: '/api/http' },
        { text: 'UI & Keyboard & DnD', link: '/api/ui-interaction' },
        { text: '其他 (Clipboard/Console/...)', link: '/api/misc' },
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
      ],
    },
    {
      text: 'Bridge 工具',
      items: [
        { text: 'Playback', link: '/mcp/tools-playback' },
        { text: 'Playback Ext', link: '/mcp/tools-playback-ext' },
        { text: 'Playlist', link: '/mcp/tools-playlist' },
        { text: 'Playlist Ext', link: '/mcp/tools-playlist-ext' },
        { text: 'Library', link: '/mcp/tools-library' },
        { text: 'Artwork', link: '/mcp/tools-artwork' },
        { text: 'Queue', link: '/mcp/tools-queue' },
        { text: 'Metadata', link: '/mcp/tools-metadata' },
      ],
    },
    {
      text: 'UI 工具',
      items: [
        { text: '截图与调试', link: '/mcp/tools-ui' },
      ],
    },
  ],
  '/reference/': [
    {
      text: '参考',
      items: [
        { text: '事件系统', link: '/reference/events' },
        { text: '错误处理', link: '/reference/errors' },
        { text: '安全限制', link: '/reference/security' },
        { text: '权限系统', link: '/reference/permissions' },
        { text: 'Playcount & Rating', link: '/reference/stats' },
        { text: 'Titleformat & ReplayGain', link: '/reference/titleformat-replaygain' },
        { text: 'SMP 兼容层', link: '/reference/smp-compat' },
        { text: 'Test API', link: '/reference/test' },
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
})
