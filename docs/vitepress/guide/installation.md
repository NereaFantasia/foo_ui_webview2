# 安装配置 

## 系统要求 

| 项目 | 要求 |
| --- | --- |
| 操作系统 | Windows 10/11 (64-bit) |
| foobar2000 | v2.0 或更高版本 (64-bit) |
| WebView2 Runtime | 通常已预装于 Windows 10/11；如未安装，请从 Microsoft 官网 下载 |

::: tip 提示
foobar2000 v2.0 是 64 位版本的分水岭。如果你使用的是 v1.x 版本，请先升级到 v2.0+。
:::

1. **下载安装包** — 从发布页面下载 `foo_ui_webview2.fb2k-component` 文件
2. **双击安装** — foobar2000 会自动弹出安装确认对话框
3. **确认安装** — 点击 `Yes`，foobar2000 会自动将组件复制到正确位置
4. **重启 foobar2000**
5. **选择 UI** — `File` → `Preferences` → `Display` → `Default User Interface` → 选择 `WebView UI`

::: info INFO
`.fb2k-component` 是 foobar2000 官方的组件安装包格式，实质是一个包含 DLL 文件的压缩包。
:::

## 安装方式二：手动安装 DLL 

1. 下载 `foo_ui_webview2.dll`
2. 复制到 foobar2000 的 `components` 子目录（目标路径：`<foobar2000>\components\foo_ui_webview2.dll`）
3. 重启 foobar2000 并选择 WebView UI

## 验证安装 

安装成功后，你应该看到：

- foobar2000 界面变为 WebView UI 的默认皮肤
- 菜单栏出现 `View` → `WebView UI` 选项
- `File` → `Preferences` → `Tools` → `WebView UI` 偏好设置页面

## 目录结构 

```text
foobar2000/
├── foobar2000.exe
├── components/
│   └── foo_ui_webview2.dll      # 插件主文件
└── profile/
    └── foo_ui_webview2/
        ├── index.html           # 前端入口页面
        ├── sdk/
        │   ├── bridge.js        # SDK 核心（fb.* 命名空间）
        │   ├── components.js    # Web Components
        │   └── smp-compat.js    # SMP 兼容层
        ├── configuration/
        │   ├── settings.json    # 用户设置
        │   └── cache/           # 缓存目录
        └── ...                  # 你的前端资产（CSS、图片等）
```

::: tip SDK 文件部署
插件安装后会自动在 `profile/foo_ui_webview2/` 目录创建默认的 `index.html` 和 `sdk/` 目录。你的前端文件也放在此目录下，HTML 中通过相对路径引用 SDK 文件：

```html
<!-- index.html -->
<!doctype html>
<html>
  <head>
    <meta charset="utf-8" />
    <title>My WebView UI Theme</title>
  </head>
  <body>
    <!-- 加载 SDK -->
    <script src="./sdk/bridge.js"></script>
    <script src="./sdk/components.js"></script>
    <!-- 你的主题脚本 -->
    <script src="./theme.js"></script>
  </body>
</html>
```
:::

## 后台模式 (Background Mode) 

后台模式允许 WebView UI 与 Default UI 或 Columns UI 同时使用。

**启用步骤：**

1. `Preferences` → `Advanced` → `Tools` → `WebView UI` → `Enable Background Mode`
2. `Preferences` → `Display` → `Default User Interface` → 选择其他 UI
3. 重启 foobar2000
4. `View` → `WebView UI` → `Show/Hide Window`

**使用场景：**

- 将 WebView UI 作为歌词、封面等辅助信息窗口
- 使用 Default UI 管理播放列表，同时用 WebView UI 显示可视化效果
- 在后台运行自定义脚本
