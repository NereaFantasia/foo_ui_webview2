import { defineConfig } from 'vitepress'

export default defineConfig({
  lang: 'zh-CN',
  title: 'foo_ui_webview2',
  description: 'foobar2000 WebView2 UI Plugin API 文档',

  // 使用绝对路径，确保子目录页面的 CSS/JS 引用正确
  base: '/foo_ui_webview2/',

  // 构建输出目录
  outDir: './dist',

  // 死链检测 - 忽略 localhost 链接
  ignoreDeadLinks: [
    /localhost/,
  ],

  // 最后更新时间（需要 git）
  lastUpdated: true,

  // head 标签
  head: [
    ['meta', { name: 'theme-color', content: '#0ea5e9' }],
    ['meta', { name: 'theme-color', content: '#0d1117', media: '(prefers-color-scheme: dark)' }],
    ['meta', { property: 'og:type', content: 'website' }],
    ['meta', { property: 'og:title', content: 'foo_ui_webview2 文档' }],
    ['meta', { property: 'og:description', content: 'foobar2000 WebView2 UI Plugin — 使用 HTML/CSS/JavaScript 构建自定义界面' }],
    // 注意：VitePress 不会给 head 内的 href 自动加 base 前缀，必须手写完整路径，
    // 否则部署到 GitHub Pages 子路径时 favicon 404
    ['link', { rel: 'icon', type: 'image/svg+xml', href: '/foo_ui_webview2/favicon.svg' }],
  ],

  themeConfig: {
    // 站点标题
    siteTitle: 'foo_ui_webview2 API',

    // Logo
    logo: '/favicon.svg',

    // 导航栏
    nav: [
      { text: '入门', link: '/guide/overview' },
      { text: 'SDK', link: '/sdk/overview' },
      { text: '底层 API', link: '/api/overview' },
      { text: '组件', link: '/components/' },
      { text: '参考', link: '/reference/events' },
      { text: 'MCP', link: '/mcp/overview' },
      {
        text: 'v1.9.0',
        items: [
          { text: '更新日志', link: '/changelog' },
        ]
      }
    ],

    // 侧边栏
    sidebar: {
      // 入门指南
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
          ]
        }
      ],

      // SDK 文档
      '/sdk/': [
        {
          text: 'SDK（推荐）',
          items: [
            { text: 'SDK 概述 & 安装', link: '/sdk/overview' },
            { text: '快速入门', link: '/sdk/quickstart' },
            { text: '命名空间', link: '/sdk/namespaces' },
            { text: '事件系统', link: '/sdk/events' },
          ]
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
            { text: 'fb.state 响应式状态', link: '/sdk/state' },
            { text: 'fb.utils 工具', link: '/sdk/utils' },
          ]
        },
        {
          text: '扩展 API 参考',
          items: [
            { text: '文件与网络', link: '/sdk/file-io' },
            { text: '音频扩展', link: '/sdk/audio-ext' },
            { text: '队列与发现', link: '/sdk/navigation' },
            { text: '数据与元信息', link: '/sdk/data' },
            { text: '菜单与杂项', link: '/sdk/misc' },
          ]
        }
      ],

      // 底层 API
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
          ]
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
          ]
        },
        {
          text: '工具 API',
          items: [
            { text: 'File & Dialog & Shell', link: '/api/file' },
            { text: 'HTTP 网络请求', link: '/api/http' },
            { text: 'UI & Keyboard & DnD', link: '/api/ui-interaction' },
            { text: '其他 (Clipboard/Console/...)', link: '/api/misc' },
          ]
        }
      ],

      // Web Components
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
          ]
        }
      ],

      // MCP Server
      '/mcp/': [
        {
          text: 'MCP Server',
          items: [
            { text: '概述', link: '/mcp/overview' },
            { text: '安装与配置', link: '/mcp/setup' },
          ]
        },
        {
          text: 'Bridge 工具',
          items: [
            { text: 'Playback 播放', link: '/mcp/tools-playback' },
            { text: 'Playback 扩展', link: '/mcp/tools-playback-ext' },
            { text: 'Playlist 播放列表', link: '/mcp/tools-playlist' },
            { text: 'Playlist 扩展', link: '/mcp/tools-playlist-ext' },
            { text: 'Library 媒体库', link: '/mcp/tools-library' },
            { text: 'Artwork 封面', link: '/mcp/tools-artwork' },
            { text: 'Queue 队列', link: '/mcp/tools-queue' },
            { text: 'Metadata 元数据', link: '/mcp/tools-metadata' },
          ]
        },
        {
          text: 'UI 工具',
          items: [
            { text: '截图与调试', link: '/mcp/tools-ui' },
          ]
        }
      ],

      // 参考
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
          ]
        }
      ],
    },

    // 搜索
    search: {
      provider: 'local',
      options: {
        translations: {
          button: { buttonText: '搜索文档', buttonAriaLabel: '搜索文档' },
          modal: {
            noResultsText: '无法找到相关结果',
            resetButtonTitle: '清除查询条件',
            footer: { selectText: '选择', navigateText: '切换', closeText: '关闭' }
          }
        }
      }
    },

    // 上一页/下一页文字
    docFooter: {
      prev: '上一页',
      next: '下一页'
    },

    // 大纲标题
    outline: {
      label: '页面导航',
      level: [2, 3]
    },

    // 最后更新时间
    lastUpdated: {
      text: '最后更新于'
    },

    // 返回顶部
    returnToTopLabel: '回到顶部',

    // 外观切换
    darkModeSwitchLabel: '主题',
    lightModeSwitchTitle: '切换到浅色模式',
    darkModeSwitchTitle: '切换到深色模式',
  },

  // Markdown 配置
  markdown: {
    lineNumbers: true,
    theme: {
      light: 'github-light',
      dark: 'github-dark'
    }
  },

  // 外观配置
  appearance: {
    initialValue: 'dark', // 默认深色模式
    toggleButtonTitle: '切换主题'
  }
})
