<#
.SYNOPSIS
    一键更新项目版本号 (Single Source of Truth)

.DESCRIPTION
    更新 VERSION 文件后同步到所有消费方:
      VERSION → src/version.h → sdk/package.json → sdk/package-lock.json
              → docs/vitepress/.vitepress/config.ts (导航栏版本号)
    
    API 文档中的 {{VERSION}} 占位符由 build-package.ps1 打包时自动替换。

.PARAMETER Version
    直接指定版本号，如 "1.2.0"

.PARAMETER Bump
    自动递增版本号: patch (1.1.17→1.1.18), minor (1.1.17→1.2.0), major (1.1.17→2.0.0)

.EXAMPLE
    .\bump-version.ps1 -Version "1.2.0"
    .\bump-version.ps1 -Bump patch
    .\bump-version.ps1 -Bump minor

.AUTHOR
    NereaFantasia
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$Version,
    
    [Parameter(Mandatory=$false)]
    [ValidateSet("patch", "minor", "major")]
    [string]$Bump
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# ============================================
# 读取当前版本
# ============================================

$VersionFile = Join-Path $ScriptDir "VERSION"
if (-not (Test-Path $VersionFile)) {
    throw "VERSION 文件不存在: $VersionFile"
}

$CurrentVersion = (Get-Content $VersionFile -Raw).Trim()
if ($CurrentVersion -notmatch '^\d+\.\d+\.\d+$') {
    throw "VERSION 文件格式无效: '$CurrentVersion' (期望: X.Y.Z)"
}

$parts = $CurrentVersion.Split('.')
$major = [int]$parts[0]
$minor = [int]$parts[1]
$patch = [int]$parts[2]

Write-Host ""
Write-Host "  当前版本: $CurrentVersion" -ForegroundColor Cyan

# ============================================
# 计算新版本
# ============================================

if ($Version -and $Bump) {
    throw "不能同时指定 -Version 和 -Bump"
}

if (-not $Version -and -not $Bump) {
    throw "请指定 -Version `"X.Y.Z`" 或 -Bump patch|minor|major"
}

if ($Bump) {
    switch ($Bump) {
        "patch" { $patch++ }
        "minor" { $minor++; $patch = 0 }
        "major" { $major++; $minor = 0; $patch = 0 }
    }
    $NewVersion = "$major.$minor.$patch"
} else {
    if ($Version -notmatch '^\d+\.\d+\.\d+$') {
        throw "版本号格式无效: '$Version' (期望: X.Y.Z)"
    }
    $NewVersion = $Version
    $parts = $NewVersion.Split('.')
    $major = [int]$parts[0]
    $minor = [int]$parts[1]
    $patch = [int]$parts[2]
}

Write-Host "  新版本号: $NewVersion" -ForegroundColor Green
Write-Host ""

# ============================================
# 编码策略（跨 PS5.1/PS7 确定性输出）
# ============================================
# 统一用 [System.IO.File]::WriteAllText + 显式 Encoding，
# 不依赖 Set-Content -Encoding UTF8 的版本差异（PS5.1 带 BOM / PS7 无 BOM）：
#   version.h            → UTF-8 with BOM（§4.3 中文注释 C++ 文件约定）
#   JSON / TS / VERSION  → UTF-8 无 BOM（npm / vite 生态约定）
# 注意：禁止用 `u{FEFF} 转义内嵌 BOM——PS5.1 不支持该语法，会把字面量写进文件
# （曾导致 version.h 首行变成 u{FEFF}#pragma once，编译直接失败）。
$Utf8Bom   = New-Object System.Text.UTF8Encoding $true
$Utf8NoBom = New-Object System.Text.UTF8Encoding $false

# ============================================
# 1. 更新 VERSION 文件
# ============================================

Write-Host "  [1/5] 更新 VERSION" -ForegroundColor Yellow
[System.IO.File]::WriteAllText($VersionFile, $NewVersion, $Utf8NoBom)
Write-Host "        $VersionFile" -ForegroundColor Gray

# ============================================
# 2. 更新 src/version.h
# ============================================

Write-Host "  [2/5] 更新 src/version.h" -ForegroundColor Yellow
$VersionH = Join-Path $ScriptDir "src\version.h"

$versionHContent = @"
#pragma once

// ============================================
// 版本号定义 — Single Source of Truth
// ============================================
//
// 本文件由 bump-version.ps1 自动生成/更新
// 手动修改前请确保同步更新 VERSION 文件
//
// 版本号管理规则:
//   1. 唯一真相源: 根目录 VERSION 文件
//   2. bump-version.ps1 负责同步更新:
//      VERSION -> src/version.h -> sdk/package.json
//              -> sdk/package-lock.json -> vitepress config.ts
//   3. 禁止在其他文件中硬编码版本号
//

#define PLUGIN_VERSION_MAJOR  $major
#define PLUGIN_VERSION_MINOR  $minor
#define PLUGIN_VERSION_PATCH  $patch

#define PLUGIN_VERSION_STR    "$NewVersion"
#define PLUGIN_VERSION_WSTR   L"$NewVersion"

// SDK 版本与插件版本保持统一
#define SDK_VERSION_STR       PLUGIN_VERSION_STR
"@

[System.IO.File]::WriteAllText($VersionH, $versionHContent, $Utf8Bom)
Write-Host "        $VersionH" -ForegroundColor Gray

# ============================================
# 3. 更新 sdk/package.json
# ============================================

Write-Host "  [3/5] 更新 sdk/package.json" -ForegroundColor Yellow
$PackageJson = Join-Path $ScriptDir "sdk\package.json"

if (Test-Path $PackageJson) {
    $content = Get-Content $PackageJson -Raw -Encoding UTF8
    $content = $content -replace '"version"\s*:\s*"[^"]*"', "`"version`": `"$NewVersion`""
    [System.IO.File]::WriteAllText($PackageJson, $content, $Utf8NoBom)
    Write-Host "        $PackageJson" -ForegroundColor Gray
} else {
    Write-Host "        警告: sdk/package.json 不存在，跳过" -ForegroundColor DarkYellow
}

# ============================================
# 4. 更新 sdk/package-lock.json（仅根包自身的两处 version 字段）
# ============================================
# npm 的 lockfile v3 在根条目 "" 和顶层各冗余一份本包版本号；
# 只精确替换这两处，不触碰依赖树中第三方包的 version。

Write-Host "  [4/5] 更新 sdk/package-lock.json" -ForegroundColor Yellow
$PackageLock = Join-Path $ScriptDir "sdk\package-lock.json"

if (Test-Path $PackageLock) {
    # 注意：不要对 lockfile 用 ConvertFrom-Json——PS5.1 无法处理 "packages" 下的空字符串键 ""
    $pkgName = (Get-Content $PackageJson -Raw -Encoding UTF8 | ConvertFrom-Json).name
    $content = Get-Content $PackageLock -Raw -Encoding UTF8
    # 顶层: "name": "<pkg>",\n  "version": "x.y.z"
    $content = $content -replace "(`"name`":\s*`"$([regex]::Escape($pkgName))`",\s*\r?\n\s*)`"version`":\s*`"[^`"]*`"", "`${1}`"version`": `"$NewVersion`""
    [System.IO.File]::WriteAllText($PackageLock, $content, $Utf8NoBom)
    Write-Host "        $PackageLock" -ForegroundColor Gray
} else {
    Write-Host "        警告: sdk/package-lock.json 不存在，跳过" -ForegroundColor DarkYellow
}

# ============================================
# 5. 更新 VitePress 导航栏版本号
# ============================================

Write-Host "  [5/5] 更新 docs/vitepress/.vitepress/config.ts" -ForegroundColor Yellow
$VitepressConfig = Join-Path $ScriptDir "docs\vitepress\.vitepress\config.ts"

if (Test-Path $VitepressConfig) {
    $content = Get-Content $VitepressConfig -Raw -Encoding UTF8
    # 结构锚定：仅替换后随 items 块的导航版本条目（text: 'vX.Y.Z',\n items:），
    # 避免将来侧栏/归档菜单出现第二个版本样式 text 时被误改
    $content = [regex]::Replace($content,
        "text:\s*'v\d+\.\d+\.\d+'(,\s*\r?\n\s*items:)",
        "text: 'v$NewVersion'`$1")
    [System.IO.File]::WriteAllText($VitepressConfig, $content, $Utf8NoBom)
    Write-Host "        $VitepressConfig" -ForegroundColor Gray
} else {
    Write-Host "        警告: vitepress config.ts 不存在，跳过" -ForegroundColor DarkYellow
}

# ============================================
# 完成
# ============================================

Write-Host ""
Write-Host "  ✅ 版本已更新: $CurrentVersion → $NewVersion" -ForegroundColor Green
Write-Host ""
Write-Host "  已同步文件:" -ForegroundColor Cyan
Write-Host "    • VERSION" -ForegroundColor White
Write-Host "    • src/version.h" -ForegroundColor White
Write-Host "    • sdk/package.json" -ForegroundColor White
Write-Host "    • sdk/package-lock.json" -ForegroundColor White
Write-Host "    • docs/vitepress/.vitepress/config.ts" -ForegroundColor White
Write-Host ""
Write-Host "  提醒: docs/vitepress/changelog.md 需手动补充本版本条目" -ForegroundColor DarkYellow
Write-Host ""
