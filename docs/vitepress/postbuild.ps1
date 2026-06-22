# postbuild.ps1 - 让 VitePress 构建产物支持 file:// 协议直接打开
# 用法: 在 npx vitepress build 之后运行此脚本
$ErrorActionPreference = 'Stop'

$distDir = Join-Path $PSScriptRoot "dist"
if (-not (Test-Path $distDir)) {
    Write-Error "dist 目录不存在，请先运行 'npx vitepress build'"
    exit 1
}

Write-Host "正在处理 dist 文件以支持 file:// 协议..."

# 获取所有 HTML 文件
$htmlFiles = Get-ChildItem -Path $distDir -Filter "*.html" -Recurse

# 站点 base 前缀（从 .vitepress/config.ts 动态读取，避免双处维护）。
# 构建产物内的绝对路径都带 base 段（如 /foo_ui_webview2/assets/...、
# /foo_ui_webview2/favicon.svg），file:// 模式下 dist 根目录就是站点根，
# 必须先剥掉 base 段，再交由下方绝对路径规则统一加相对前缀。
$basePrefix = ''
$configTs = Join-Path $PSScriptRoot ".vitepress\config.ts"
if (Test-Path $configTs) {
    $baseMatch = [regex]::Match((Get-Content $configTs -Raw), "base:\s*'([^']+)'")
    if ($baseMatch.Success -and $baseMatch.Groups[1].Value -ne '/') {
        $basePrefix = $baseMatch.Groups[1].Value
        Write-Host "检测到站点 base: $basePrefix （file:// 模式将剥离此前缀）"
    }
}

$count = 0
foreach ($file in $htmlFiles) {
    # 不用 [System.IO.Path]::GetRelativePath —— 那是 .NET Core(PS7) 专属 API，
    # PS5.1 (.NET Framework) 下不存在；改用前缀截断，跨版本一致
    $relPath = $file.FullName.Substring($distDir.Length).TrimStart('\', '/')
    $depth = ($relPath.Replace('\', '/').Split('/').Length - 1)

    # 根据目录深度计算相对路径前缀
    $prefix = if ($depth -eq 0) { "./" } else { ("../" * $depth) }

    $content = [System.IO.File]::ReadAllText($file.FullName, [System.Text.Encoding]::UTF8)

    # 移除 <script type="module"> (ES modules 在 file:// 下被 CORS 阻止)
    $content = [regex]::Replace($content, '<script type="module"[^>]*></script>', '')

    # 移除 <link rel="modulepreload">
    $content = [regex]::Replace($content, '<link rel="modulepreload"[^>]*/?>', '')

    # 移除字体 preload (file:// 下 CORS 问题)
    $content = [regex]::Replace($content, '<link rel="preload" href="[^"]*\.woff2"[^>]*/?>', '')

    # 剥离 base 段: href/src="/foo_ui_webview2/..." → "/..."（再由下方规则加相对前缀）
    if ($basePrefix) {
        $content = $content.Replace('href="' + $basePrefix, 'href="/')
        $content = $content.Replace('src="' + $basePrefix, 'src="/')
        # base 根链接（href="/foo_ui_webview2/"）剥离后变成 href="/"，由下方首页规则接管
    }

    # 修复首页链接: href="/" → href="./index.html" 或 "../index.html"
    $homeLink = 'href="' + $prefix + 'index.html"'
    $content = $content.Replace('href="/"', $homeLink)

    # 修复绝对路径: href="/path" → href="${prefix}path"
    $hrefReplacement = 'href="' + $prefix + '$1"'
    $content = [regex]::Replace($content, 'href="/([^"]+)"', $hrefReplacement)

    # 修复 src 绝对路径: src="/path" → src="${prefix}path"
    $srcReplacement = 'src="' + $prefix + '$1"'
    $content = [regex]::Replace($content, 'src="/([^"]+)"', $srcReplacement)

    [System.IO.File]::WriteAllText($file.FullName, $content, (New-Object System.Text.UTF8Encoding $false))
    $count++
}

Write-Host "完成! 已处理 $count 个 HTML 文件。"
Write-Host "注意: 搜索和主题切换功能需要 HTTP 服务器 (npx vitepress preview)。"
