# ============================================================================
# foo_ui_webview2 构建脚本
# ============================================================================
# 用途: 标准化构建流程，避免路径和配置错误
# 使用: .\build.ps1 -Config Release -Platform x64 -Clean
# ============================================================================

param(
    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release',
    
    [ValidateSet('Win32', 'x64')]
    [string]$Platform = 'x64',
    
    [string]$PlatformToolset,
    
    [switch]$Clean,
    [switch]$Rebuild
)

# ============================================================================
# 1. 查找 MSBuild (永远不要硬编码路径！)
# ============================================================================
Write-Host "🔍 Searching for MSBuild..." -ForegroundColor Cyan

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Host "❌ vswhere.exe not found. Please install Visual Studio 2017 or later." -ForegroundColor Red
    exit 1
}

$vsPath = & $vswhere -latest -products * -property installationPath -prerelease
if (-not $vsPath) {
    Write-Host "❌ Visual Studio not found!" -ForegroundColor Red
    exit 1
}

$msbuild = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    Write-Host "❌ MSBuild.exe not found at: $msbuild" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Found MSBuild: $msbuild" -ForegroundColor Green
Write-Host "✓ Found VS at: $vsPath" -ForegroundColor Green

# ============================================================================
# 2. 验证项目文件
# ============================================================================
$sln = Join-Path $PSScriptRoot "foo_ui_webview2.sln"
if (-not (Test-Path $sln)) {
    Write-Host "❌ Solution file not found: $sln" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Solution: $sln" -ForegroundColor Green

# ============================================================================
# 3. 预编译头文件验证 (防止配置错误)
# ============================================================================
$pchHeader = Join-Path $PSScriptRoot "src\pch.h"
$pchSource = Join-Path $PSScriptRoot "src\pch.cpp"

if (-not (Test-Path $pchHeader)) {
    Write-Host "❌ Precompiled header not found: $pchHeader" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $pchSource)) {
    Write-Host "❌ Precompiled source not found: $pchSource" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Precompiled headers OK" -ForegroundColor Green

# ============================================================================
# 3.5 依赖预检 (columns_ui-sdk + NuGet 包)
# ============================================================================
# lib/columns_ui-sdk 是第三方 SDK，不入库；干净检出时按 CI 同款方式获取。
$cuiSdkHeader = Join-Path $PSScriptRoot "lib\columns_ui-sdk\ui_extension.h"
if (-not (Test-Path $cuiSdkHeader)) {
    Write-Host "`n📥 lib\columns_ui-sdk missing - cloning (same as CI)..." -ForegroundColor Yellow
    git clone --depth=1 https://github.com/reupen/columns_ui-sdk.git (Join-Path $PSScriptRoot "lib\columns_ui-sdk")
    if ($LASTEXITCODE -eq 0 -and (Test-Path $cuiSdkHeader)) {
        # 该 SDK 的 include 以兄弟目录布局引用 pfc/foobar2000，本仓库位于 lib/foobar2000_sdk 下，需改写
        (Get-Content $cuiSdkHeader) `
            -replace '\.\./pfc/', '../foobar2000_sdk/pfc/' `
            -replace '\.\./foobar2000/', '../foobar2000_sdk/foobar2000/' |
            Set-Content $cuiSdkHeader
        Write-Host "✓ columns_ui-sdk ready (include paths patched)" -ForegroundColor Green
    } else {
        Write-Host "❌ Failed to clone columns_ui-sdk. Fetch it manually:" -ForegroundColor Red
        Write-Host '     git clone --depth=1 https://github.com/reupen/columns_ui-sdk.git lib/columns_ui-sdk' -ForegroundColor White
        Write-Host '     (Get-Content lib/columns_ui-sdk/ui_extension.h) `' -ForegroundColor White
        Write-Host "         -replace '\.\./pfc/', '../foobar2000_sdk/pfc/' ``" -ForegroundColor White
        Write-Host "         -replace '\.\./foobar2000/', '../foobar2000_sdk/foobar2000/' |" -ForegroundColor White
        Write-Host '         Set-Content lib/columns_ui-sdk/ui_extension.h' -ForegroundColor White
        exit 1
    }
}

# NuGet 包检查：解析根与 tests 的 packages.config，缺失时自动恢复。
$missingPkgs = @()
foreach ($cfgRel in @("packages.config", "tests\packages.config")) {
    $cfgPath = Join-Path $PSScriptRoot $cfgRel
    if (-not (Test-Path $cfgPath)) { continue }
    [xml]$cfgXml = Get-Content $cfgPath
    foreach ($p in $cfgXml.packages.package) {
        $pkgDir = Join-Path $PSScriptRoot "packages\$($p.id).$($p.version)"
        if (-not (Test-Path $pkgDir)) {
            $missingPkgs += [pscustomobject]@{ Id = $p.id; Version = $p.version; Dir = $pkgDir }
        }
    }
}

if ($missingPkgs.Count -gt 0) {
    Write-Host "`n📦 NuGet packages missing ($($missingPkgs.Count)) - restoring..." -ForegroundColor Yellow
    $nugetCmd = Get-Command nuget -ErrorAction SilentlyContinue
    if ($nugetCmd) {
        & $nugetCmd.Source restore $sln
    } else {
        # msbuild 的 restore target 只覆盖主工程的 packages.config
        & $msbuild $sln /t:Restore /p:RestorePackagesConfig=true /v:minimal
    }

    # 复查；仍缺失的包（如 tests 的 googletest）从 nuget.org 直接下载解压
    foreach ($pkg in $missingPkgs) {
        if (Test-Path $pkg.Dir) { continue }
        Write-Host "  ⬇ Downloading $($pkg.Id) $($pkg.Version) from nuget.org..." -ForegroundColor Yellow
        $tmpNupkg = Join-Path ([System.IO.Path]::GetTempPath()) "$($pkg.Id).$($pkg.Version).nupkg.zip"
        try {
            Invoke-WebRequest "https://www.nuget.org/api/v2/package/$($pkg.Id)/$($pkg.Version)" -OutFile $tmpNupkg -UseBasicParsing
            Expand-Archive $tmpNupkg -DestinationPath $pkg.Dir -Force
            Remove-Item $tmpNupkg -ErrorAction SilentlyContinue
        } catch {
            Write-Host "❌ Failed to download $($pkg.Id): $_" -ForegroundColor Red
        }
    }

    $stillMissing = @($missingPkgs | Where-Object { -not (Test-Path $_.Dir) })
    if ($stillMissing.Count -gt 0) {
        Write-Host "❌ NuGet restore incomplete: $(($stillMissing | ForEach-Object { $_.Id }) -join ', ')" -ForegroundColor Red
        Write-Host "   Install nuget.exe and run: nuget restore foo_ui_webview2.sln" -ForegroundColor Yellow
        exit 1
    }
    Write-Host "✓ NuGet packages restored" -ForegroundColor Green
}

# ============================================================================
# 4. 清理 (如果需要)
# ============================================================================
if ($Clean -or $Rebuild) {
    Write-Host "`n🧹 Cleaning solution..." -ForegroundColor Yellow
    $cleanArgs = @($sln, "/t:Clean", "/p:Configuration=$Config", "/p:Platform=$Platform", "/v:minimal")
    if ($PlatformToolset) { $cleanArgs += "/p:PlatformToolset=$PlatformToolset" }
    & $msbuild @cleanArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "❌ Clean failed!" -ForegroundColor Red
        exit $LASTEXITCODE
    }
    Write-Host "✓ Clean completed" -ForegroundColor Green
}

# ============================================================================
# 5. 构建
# ============================================================================
$target = if ($Rebuild) { "Rebuild" } else { "Build" }
Write-Host "`n🔨 Building ($target) - Config: $Config, Platform: $Platform..." -ForegroundColor Cyan

$logFile = Join-Path $PSScriptRoot "build_log.txt"
$buildArgs = @(
    $sln
    "/t:$target"
    "/p:Configuration=$Config"
    "/p:Platform=$Platform"
    "/m"  # 多核编译
    "/v:minimal"
    "/fl"
    "/flp:logfile=$logFile;verbosity=normal"
)

if ($PlatformToolset) {
    $buildArgs += "/p:PlatformToolset=$PlatformToolset"
    Write-Host "⚙️ Overriding PlatformToolset: $PlatformToolset" -ForegroundColor Yellow
}

Write-Host "Command: $msbuild $($buildArgs -join ' ')" -ForegroundColor Gray

& $msbuild @buildArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n✅ Build succeeded!" -ForegroundColor Green
    
    # 显示输出文件
    $outDir = Join-Path $PSScriptRoot "bin\${Config}_${Platform}"
    if (Test-Path $outDir) {
        Write-Host "`n📦 Output files:" -ForegroundColor Cyan
        Get-ChildItem $outDir -Filter "*.dll" | ForEach-Object {
            $size = [math]::Round($_.Length / 1MB, 2)
            Write-Host "  • $($_.Name) ($size MB)" -ForegroundColor White
        }
    }
} else {
    Write-Host "`n❌ Build failed! Error code: $LASTEXITCODE" -ForegroundColor Red
    Write-Host "📄 Check log: $logFile" -ForegroundColor Yellow
    
    # 显示最后20行错误
    if (Test-Path $logFile) {
        Write-Host "`n🔍 Last errors:" -ForegroundColor Yellow
        Get-Content $logFile -Tail 20 | Write-Host -ForegroundColor Red
    }
    
    exit $LASTEXITCODE
}
