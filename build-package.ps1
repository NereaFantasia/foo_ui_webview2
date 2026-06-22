<#
.SYNOPSIS
    构建并打包 foo_ui_webview2 为 fb2k-component，并单独输出 SDK ZIP

.DESCRIPTION
    1. 编译 Win32 (x86) 和 x64 版本
    2. 创建符合 foobar2000 规范的 fb2k-component 包
    3. 单独创建 SDK ZIP（仅包含 sdk/ 目录）
    
    fb2k-component 包结构:
    foo_ui_webview2.fb2k-component (ZIP)
    ├── foo_ui_webview2.dll          # x86 版本 (根目录)
    ├── WebView2Loader.dll           # x86 WebView2 加载器
    ├── x64/
    │   ├── foo_ui_webview2.dll      # x64 版本
    │   └── WebView2Loader.dll       # x64 WebView2 加载器
    └── foo_ui_webview2_resources/   # 前端资源 (可选)

    SDK ZIP 包结构 (-IncludeSDK):
    foo_ui_webview2-sdk-X.X.X.zip
    └── sdk/                         # JS SDK (TypeScript 类型定义 + Bridge)

.PARAMETER Configuration
    编译配置: Debug 或 Release (默认 Release)

.PARAMETER SkipBuild
    跳过编译步骤，仅打包

.PARAMETER IncludeResources
    是否包含前端资源文件夹 (默认 false)

.PARAMETER IncludeSDK
    是否打包 JS SDK 为独立 ZIP (默认 true)

.PARAMETER OutputDir
    输出目录 (默认 dist/)

.AUTHOR
    NereaFantasia

.LICENSE
    GPL-3.0-or-later
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    
    [Parameter(Mandatory=$false)]
    [switch]$SkipBuild,
    
    [Parameter(Mandatory=$false)]
    [switch]$IncludeResources,
    
    [Parameter(Mandatory=$false)]
    [bool]$IncludeSDK = $true,
    
    [Parameter(Mandatory=$false)]
    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# 路径配置
$SolutionPath = Join-Path $ScriptDir "foo_ui_webview2.sln"
$BinDir = Join-Path $ScriptDir "bin"

# 使用 vswhere 动态查找 MSBuild
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -property installationPath -prerelease
    if ($vsPath) {
        $MSBuildPath = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
    }
}
if (-not $MSBuildPath -or -not (Test-Path $MSBuildPath)) {
    # 尝试从 PATH 中查找
    $MSBuildPath = (Get-Command msbuild -ErrorAction SilentlyContinue).Source
}
$TempDir = Join-Path $ScriptDir "temp_package"
$DistDir = Join-Path $ScriptDir $OutputDir

# 动态查找最新 WebView2 NuGet 包
$WebView2PkgDir = Get-ChildItem -Path (Join-Path $ScriptDir "packages") -Directory -Filter "Microsoft.Web.WebView2.*" |
    Sort-Object Name -Descending | Select-Object -First 1
if ($WebView2PkgDir) {
    $WebView2NuGetBase = $WebView2PkgDir.FullName
    Write-Host "  WebView2 NuGet: $($WebView2PkgDir.Name)" -ForegroundColor Gray
} else {
    $WebView2NuGetBase = $null
    Write-Host "  警告: 未找到 WebView2 NuGet 包" -ForegroundColor DarkYellow
}

# 获取版本号 - 从 VERSION 文件读取 (Single Source of Truth)
function Get-ComponentVersion {
    $VersionFile = Join-Path $ScriptDir "VERSION"
    if (Test-Path $VersionFile) {
        $version = (Get-Content $VersionFile -Raw).Trim()
        if ($version -match '^\d+\.\d+\.\d+$') {
            Write-Host "  检测到版本号: $version" -ForegroundColor Gray
            return $version
        }
    }
    Write-Host "  警告: 无法从 VERSION 文件提取版本号，使用默认值" -ForegroundColor DarkYellow
    return "0.0.0"
}

$Version = Get-ComponentVersion
$PackageName = "foo_ui_webview2-$Version.fb2k-component"
$SdkZipName = "foo_ui_webview2-sdk-$Version.zip"

# 计算步骤总数
$totalSteps = 4
if ($IncludeSDK) { $totalSteps = 5 }

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║       foo_ui_webview2 Component Packager                    ║" -ForegroundColor Cyan
Write-Host "║       Version: $($Version.PadRight(45))║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# ============================================
# 步骤 1: 编译
# ============================================

$SdkZipPath = $null
if (-not $SkipBuild) {
    Write-Host "[1/$totalSteps] 编译组件..." -ForegroundColor Yellow

    # 检查 MSBuild
    if (-not $MSBuildPath -or -not (Test-Path $MSBuildPath)) {
        throw "找不到 MSBuild，请确保 Visual Studio 2017 或更高版本已安装"
    }
    Write-Host "      使用 MSBuild: $MSBuildPath" -ForegroundColor Gray

    # 编译 Win32 (x86)
    Write-Host "      编译 Win32 (x86)..." -ForegroundColor Gray
    & $MSBuildPath $SolutionPath /p:Configuration=$Configuration /p:Platform=Win32 /m /nologo /v:m
    if ($LASTEXITCODE -ne 0) {
        throw "Win32 编译失败"
    }

    # 编译 x64
    Write-Host "      编译 x64..." -ForegroundColor Gray
    & $MSBuildPath $SolutionPath /p:Configuration=$Configuration /p:Platform=x64 /m /nologo /v:m
    if ($LASTEXITCODE -ne 0) {
        throw "x64 编译失败"
    }

    Write-Host "      编译完成！" -ForegroundColor Green
} else {
    Write-Host "[1/$totalSteps] 跳过编译 (SkipBuild)" -ForegroundColor Gray
}

# ============================================
# 步骤 2: 准备打包目录
# ============================================

Write-Host "[2/$totalSteps] 准备打包目录..." -ForegroundColor Yellow

# 清理并创建临时目录
if (Test-Path $TempDir) {
    Remove-Item $TempDir -Recurse -Force
}
New-Item -Path $TempDir -ItemType Directory -Force | Out-Null

# 创建 x64 子目录
$TempX64Dir = Join-Path $TempDir "x64"
New-Item -Path $TempX64Dir -ItemType Directory -Force | Out-Null
# ============================================
# 步骤 3: 复制文件
# ============================================

Write-Host "[3/$totalSteps] 复制文件..." -ForegroundColor Yellow

# 确定 DLL 路径
$X86DllPath = Join-Path $BinDir "$Configuration\foo_ui_webview2.dll"
$X64DllPath = Join-Path $BinDir "${Configuration}_x64\foo_ui_webview2.dll"

# 检查文件存在
if (-not (Test-Path $X86DllPath)) {
    throw "找不到 x86 DLL: $X86DllPath"
}
if (-not (Test-Path $X64DllPath)) {
    throw "找不到 x64 DLL: $X64DllPath"
}

# 复制 x86 DLL 到根目录
Copy-Item $X86DllPath $TempDir -Force
Write-Host "      复制 x86 DLL" -ForegroundColor Gray

# 复制 x86 WebView2Loader.dll
$X86LoaderPath = Join-Path $BinDir "$Configuration\WebView2Loader.dll"
if (Test-Path $X86LoaderPath) {
    Copy-Item $X86LoaderPath $TempDir -Force
    Write-Host "      复制 x86 WebView2Loader.dll" -ForegroundColor Gray
} elseif ($WebView2NuGetBase) {
    $X86LoaderNuGet = Join-Path $WebView2NuGetBase "build\native\x86\WebView2Loader.dll"
    if (Test-Path $X86LoaderNuGet) {
        Copy-Item $X86LoaderNuGet $TempDir -Force
        Write-Host "      复制 x86 WebView2Loader.dll (从 NuGet)" -ForegroundColor Gray
    } else {
        Write-Host "      警告: 找不到 x86 WebView2Loader.dll" -ForegroundColor DarkYellow
    }
} else {
    Write-Host "      警告: 找不到 x86 WebView2Loader.dll" -ForegroundColor DarkYellow
}

# 复制 x64 DLL 到 x64 子目录
Copy-Item $X64DllPath $TempX64Dir -Force
Write-Host "      复制 x64 DLL" -ForegroundColor Gray

# 复制 x64 WebView2Loader.dll
$X64LoaderPath = Join-Path $BinDir "${Configuration}_x64\WebView2Loader.dll"
if (Test-Path $X64LoaderPath) {
    Copy-Item $X64LoaderPath $TempX64Dir -Force
    Write-Host "      复制 x64 WebView2Loader.dll" -ForegroundColor Gray
} elseif ($WebView2NuGetBase) {
    $X64LoaderNuGet = Join-Path $WebView2NuGetBase "build\native\x64\WebView2Loader.dll"
    if (Test-Path $X64LoaderNuGet) {
        Copy-Item $X64LoaderNuGet $TempX64Dir -Force
        Write-Host "      复制 x64 WebView2Loader.dll (从 NuGet)" -ForegroundColor Gray
    } else {
        Write-Host "      警告: 找不到 x64 WebView2Loader.dll" -ForegroundColor DarkYellow
    }
} else {
    Write-Host "      警告: 找不到 x64 WebView2Loader.dll" -ForegroundColor DarkYellow
}

# 可选: 复制前端资源
if ($IncludeResources) {
    $ResourcesDir = Join-Path $ScriptDir "resources\dist"
    if (Test-Path $ResourcesDir) {
        $TargetResourcesDir = Join-Path $TempDir "foo_ui_webview2_resources\dist"
        New-Item -Path $TargetResourcesDir -ItemType Directory -Force | Out-Null
        Copy-Item "$ResourcesDir\*" $TargetResourcesDir -Recurse -Force
        Write-Host "      复制前端资源" -ForegroundColor Gray
        
        # 同样复制到 x64 目录 (确保两个版本都能找到资源)
        $TargetResourcesX64Dir = Join-Path $TempX64Dir "foo_ui_webview2_resources\dist"
        New-Item -Path $TargetResourcesX64Dir -ItemType Directory -Force | Out-Null
        Copy-Item "$ResourcesDir\*" $TargetResourcesX64Dir -Recurse -Force
    } else {
        Write-Host "      警告: 前端资源目录不存在，跳过" -ForegroundColor DarkYellow
    }
}

# 创建空的资源文件夹结构（供开发者放置自定义 WebUI）
$X86ResourcesDir = Join-Path $TempDir "foo_ui_webview2_resources\dist"
$X64ResourcesDir = Join-Path $TempX64Dir "foo_ui_webview2_resources\dist"

if (-not (Test-Path $X86ResourcesDir)) {
    New-Item -Path $X86ResourcesDir -ItemType Directory -Force | Out-Null
}
if (-not (Test-Path $X64ResourcesDir)) {
    New-Item -Path $X64ResourcesDir -ItemType Directory -Force | Out-Null
}

# 创建 README 说明文件 (使用数组拼接避免 here-string 在 LF 换行文件中的解析问题)
$ReadmeContent = @(
    '# foo_ui_webview2 前端资源目录',
    '',
    '将您的 WebUI 文件放置在此目录中。',
    '',
    '## 目录结构',
    '',
    '```',
    'foo_ui_webview2_resources/',
    '└── dist/',
    '    ├── index.html      ← 入口文件 (必需)',
    '    ├── app.js          ← JavaScript',
    '    ├── app.css         ← 样式表',
    '    └── assets/         ← 其他资源',
    '```',
    '',
    '## 开发说明',
    '',
    '1. 使用 Vue/React/纯 HTML 开发您的前端',
    '2. 构建后将产物复制到此目录',
    '3. 确保 index.html 存在于 dist/ 目录',
    '4. 重启 foobar2000 即可加载新界面',
    '',
    '## API 文档',
    '',
    '完整 API 文档请参考:',
    '- https://github.com/NereaFantasia/foo_ui_webview2',
    '',
    '## 示例',
    '',
    '```html',
    '<!DOCTYPE html>',
    '<html>',
    '<head>',
    '    <title>My foobar2000 UI</title>',
    '</head>',
    '<body>',
    '    <button onclick="fb2k.invoke(''playback.play'')">Play</button>',
    '    <script>',
    '        // 监听播放状态变化',
    '        fb2k.on(''playback:stateChanged'', (data) => {',
    '            console.log(''State:'', data.state);',
    '        });',
    '    </script>',
    '</body>',
    '</html>',
    '```'
) -join "`n"

$X86ReadmePath = Join-Path $TempDir "foo_ui_webview2_resources\README.md"
$X64ReadmePath = Join-Path $TempX64Dir "foo_ui_webview2_resources\README.md"
Set-Content -Path $X86ReadmePath -Value $ReadmeContent -Encoding UTF8
Set-Content -Path $X64ReadmePath -Value $ReadmeContent -Encoding UTF8
Write-Host "      创建资源文件夹和 README" -ForegroundColor Gray

# ============================================
# 步骤 4: 创建 fb2k-component 包
# ============================================

Write-Host "[4/$totalSteps] 创建 fb2k-component 包..." -ForegroundColor Yellow

# 确保输出目录存在
if (-not (Test-Path $DistDir)) {
    New-Item -Path $DistDir -ItemType Directory -Force | Out-Null
}

$PackagePath = Join-Path $DistDir $PackageName

# 如果存在旧包则删除
if (Test-Path $PackagePath) {
    Remove-Item $PackagePath -Force
}

# 创建 ZIP (fb2k-component 就是 ZIP 格式)
# 较新 PowerShell 的 Compress-Archive 仅接受 .zip 目标扩展名，会拒绝 .fb2k-component；
# 改用 .NET ZipFile（不校验扩展名，输出同为标准 zip，行为等效）。
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $TempDir, $PackagePath, [System.IO.Compression.CompressionLevel]::Optimal, $false)

# 清理 fb2k-component 临时目录
Remove-Item $TempDir -Recurse -Force

# ============================================
# 步骤 5: 创建 SDK ZIP
# ============================================

if ($IncludeSDK) {
    Write-Host "[5/$totalSteps] 创建 SDK ZIP..." -ForegroundColor Yellow

    $SdkTempDir = Join-Path $ScriptDir "temp_sdk"
    if (Test-Path $SdkTempDir) {
        Remove-Item $SdkTempDir -Recurse -Force
    }
    New-Item -Path $SdkTempDir -ItemType Directory -Force | Out-Null

    # 复制 SDK 目录到临时目录，保持 zip 内层级为 sdk/
    $SdkSourceDir = Join-Path $ScriptDir "sdk"
    if (Test-Path $SdkSourceDir) {
        $SdkTargetDir = Join-Path $SdkTempDir "sdk"
        Copy-Item $SdkSourceDir $SdkTargetDir -Recurse -Force

        # 排除测试文件和非发布文件
        $SdkExcludePatterns = @("test_*", "*.test.*", "*.tgz", "*.zip")
        foreach ($pattern in $SdkExcludePatterns) {
            Get-ChildItem $SdkTargetDir -Recurse -File -Filter $pattern | Remove-Item -Force
        }

        $sdkFiles = (Get-ChildItem $SdkTargetDir -Recurse -File).Count
        Write-Host "      复制 sdk/ ($sdkFiles 个文件，已排除测试文件)" -ForegroundColor Gray
    } else {
        Write-Host "      警告: sdk/ 目录不存在，跳过 SDK ZIP" -ForegroundColor DarkYellow
        Remove-Item $SdkTempDir -Recurse -Force
        $IncludeSDK = $false
    }

    if ($IncludeSDK) {
        # 创建 SDK ZIP
        $SdkZipPath = Join-Path $DistDir $SdkZipName
        if (Test-Path $SdkZipPath) {
            Remove-Item $SdkZipPath -Force
        }
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::CreateFromDirectory(
            $SdkTempDir, $SdkZipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
        Write-Host "      SDK ZIP 创建完成" -ForegroundColor Green
    }

    # 清理
    if (Test-Path $SdkTempDir) {
        Remove-Item $SdkTempDir -Recurse -Force
    }
}

# ============================================
# 完成
# ============================================

$PackageInfo = Get-Item $PackagePath
$SizeMB = [math]::Round($PackageInfo.Length / 1MB, 2)

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                     打包完成！                              ║" -ForegroundColor Green
Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host ""
Write-Host "  [Component]" -ForegroundColor Cyan
Write-Host "    文件: " -NoNewline
Write-Host $PackagePath -ForegroundColor White
Write-Host "    大小: " -NoNewline
Write-Host "${SizeMB} MB" -ForegroundColor White
Write-Host "    内容:" -ForegroundColor Gray
Write-Host "      ├── foo_ui_webview2.dll      + WebView2Loader.dll  (x86)" -ForegroundColor Gray
Write-Host "      └── x64/" -ForegroundColor Gray
Write-Host "          └── foo_ui_webview2.dll  + WebView2Loader.dll  (x64)" -ForegroundColor Gray

if ($SdkZipPath -and (Test-Path $SdkZipPath)) {
    $SdkInfo = Get-Item $SdkZipPath
    $SdkSizeMB = [math]::Round($SdkInfo.Length / 1MB, 2)
    Write-Host ""
    Write-Host "  [SDK ZIP]" -ForegroundColor Cyan
    Write-Host "    文件: " -NoNewline
    Write-Host $SdkZipPath -ForegroundColor White
    Write-Host "    大小: " -NoNewline
    Write-Host "${SdkSizeMB} MB" -ForegroundColor White
    Write-Host "    内容:" -ForegroundColor Gray
    Write-Host "      └── sdk/            (JS SDK + TypeScript 类型定义)" -ForegroundColor Gray
}

Write-Host ""
Write-Host "  安装方法:" -ForegroundColor Cyan
Write-Host "    1. 双击 $PackageName" -ForegroundColor White
Write-Host "    2. 或拖放到 foobar2000 主窗口" -ForegroundColor White
Write-Host "    3. 或在 Preferences > Components > Install 选择该文件" -ForegroundColor White
Write-Host ""
