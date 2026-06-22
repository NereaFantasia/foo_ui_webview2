<#
.SYNOPSIS
    运行 clang-tidy 扫描 src/ 目录
.DESCRIPTION
    使用 VS 2022 内置 clang-tidy 对 src/ 下的 .cpp 文件执行静态分析。
    支持并行扫描（PowerShell 7+）、增量缓存、快速/深度模式。
.PARAMETER Files
    要扫描的文件列表（默认：src/ 下全部 .cpp）
.PARAMETER Fix
    是否自动修复（默认否，并行模式下强制禁用）
.PARAMETER Checks
    覆盖 .clang-tidy 中的 Checks 设置
.PARAMETER GitDiff
    仅扫描 git diff 中已修改的 src/*.cpp 文件（增量模式）
.PARAMETER Fast
    快速模式：跳过 clang-analyzer-*（最耗时的检查类），单文件提速 2~5 倍
.PARAMETER Deep
    深度模式：强制包含 clang-analyzer-*（默认行为，与 .clang-tidy 一致）
.PARAMETER NoCache
    禁用哈希缓存，强制重新扫描所有文件
.PARAMETER TimeoutSec
    单文件扫描超时秒数（默认 120）
.PARAMETER Jobs
    并行扫描线程数（默认自动检测：CPU 核心数 / 2，上限 16）。设为 1 禁用并行。
.EXAMPLE
    .\scripts\run-clang-tidy.ps1              # 全量扫描（自动并行）
    .\scripts\run-clang-tidy.ps1 -Fast        # 快速模式（跳过 analyzer）
    .\scripts\run-clang-tidy.ps1 -GitDiff     # 增量扫描
    .\scripts\run-clang-tidy.ps1 -NoCache     # 跳过缓存
    .\scripts\run-clang-tidy.ps1 -Jobs 8      # 指定线程数
#>
param(
    [string[]]$Files,
    [switch]$Fix,
    [string]$Checks,
    [switch]$GitDiff,
    [switch]$Fast,
    [switch]$Deep,
    [switch]$NoCache,
    [int]$TimeoutSec = 120,
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path $PSScriptRoot -Parent

# ── 自动检测线程数 ──
if ($Jobs -le 0) {
    $cpuCores = [Environment]::ProcessorCount
    $Jobs = [Math]::Max(1, [Math]::Min([Math]::Floor($cpuCores / 4), 4))
}
Write-Host "并行线程: $Jobs (CPU 核心: $([Environment]::ProcessorCount))" -ForegroundColor Cyan

# ── Fast/Deep 模式处理 ──
if ($Fast -and $Deep) {
    Write-Error "-Fast 和 -Deep 不能同时使用"
    exit 1
}
if ($Fast -and -not $Checks) {
    # 快速模式：复制 .clang-tidy 的 Checks 但去掉 clang-analyzer-*
    $Checks = '-*,bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-implicit-widening-of-multiplication-result,-bugprone-narrowing-conversions,-bugprone-empty-catch,-bugprone-macro-parentheses,-bugprone-reserved-identifier,-bugprone-switch-missing-default-case,performance-*,-performance-enum-size,-performance-no-int-to-ptr,modernize-*,-modernize-use-trailing-return-type,-modernize-avoid-c-arrays,-modernize-use-nodiscard,-modernize-concat-nested-namespaces,-modernize-use-ranges,-modernize-use-designated-initializers,-modernize-use-auto,-modernize-use-integer-sign-comparison,-modernize-return-braced-init-list,-modernize-raw-string-literal,readability-*,-readability-magic-numbers,-readability-identifier-length,-readability-implicit-bool-conversion,-readability-function-cognitive-complexity,-readability-braces-around-statements,-readability-else-after-return,-readability-uppercase-literal-suffix,-readability-math-missing-parentheses,-readability-static-definition-in-anonymous-namespace,-readability-isolate-declaration,-readability-qualified-auto,-readability-redundant-inline-specifier,-readability-convert-member-functions-to-static,-readability-use-std-min-max,-readability-avoid-nested-conditional-operator,-readability-const-return-type,-readability-named-parameter,-readability-use-anyofallof,-readability-redundant-casting'
    Write-Host "[FAST] 跳过 clang-analyzer-*（深度分析）" -ForegroundColor Yellow
}

# ── 哈希缓存设置 ──
$cacheDir = Join-Path $projectRoot '.clang-tidy-cache'
$cacheFile = Join-Path $cacheDir 'file-hashes.json'
$cacheEnabled = -not $NoCache
$cacheHits = 0
$cacheData = @{}

if ($cacheEnabled) {
    if (-not (Test-Path $cacheDir)) {
        New-Item -ItemType Directory -Path $cacheDir -Force | Out-Null
    }
    # 加载已有缓存
    if (Test-Path $cacheFile) {
        try {
            $cacheData = Get-Content $cacheFile -Raw | ConvertFrom-Json -AsHashtable
        } catch {
            $cacheData = @{}
        }
    }
    # 计算当前检查配置的指纹（Checks 参数 + .clang-tidy 文件哈希）
    $clangTidyConfig = Join-Path $projectRoot '.clang-tidy'
    $configHash = ''
    if (Test-Path $clangTidyConfig) {
        $configHash = (Get-FileHash $clangTidyConfig -Algorithm MD5).Hash
    }
    $checksFingerprint = "checks:$Checks|config:$configHash"
}

function Get-FileCacheKey {
    param([string]$FilePath)
    $hash = (Get-FileHash $FilePath -Algorithm MD5).Hash
    return "$hash|$checksFingerprint"
}

# ── 查找 clang-tidy（三层 fallback）──
#   1. vswhere（-requires ClangToolset -find）返回 clang-tidy.exe 绝对路径
#   2. 常见 VS 安装根目录探测（覆盖 vswhere 未注册的安装）
#   3. PATH 上的 clang-tidy.exe（独立 LLVM 安装）
$clangTidy = $null
$searchLog = @()

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    # vswhere -find 返回所有匹配文件，取第一条即可
    $vsFound = & $vswhere -latest -prerelease `
        -requires Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset `
        -find "VC\Tools\Llvm\x64\bin\clang-tidy.exe" 2>$null |
        Select-Object -First 1
    if ($vsFound -and (Test-Path $vsFound)) {
        $clangTidy = $vsFound
        $searchLog += "[vswhere] $vsFound"
    } else {
        $searchLog += "[vswhere] no result (VS install with ClangToolset component not registered)"
    }
} else {
    $searchLog += "[vswhere] not found at $vswhere"
}

if (-not $clangTidy) {
    # 常见 VS 安装根目录候选
    $vsRootCandidates = @(
        "E:\Microsoft Visual Studio\18\Community",
        "E:\Microsoft Visual Studio\18\Professional",
        "E:\Microsoft Visual Studio\18\Enterprise",
        "E:\Microsoft Visual Studio\2026\Community",
        "E:\Microsoft Visual Studio\2022\Community",
        "C:\Program Files\Microsoft Visual Studio\2026\Community",
        "C:\Program Files\Microsoft Visual Studio\2022\Community",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise",
        "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
    )
    foreach ($vsRoot in $vsRootCandidates) {
        $candidate = Join-Path $vsRoot "VC\Tools\Llvm\x64\bin\clang-tidy.exe"
        if (Test-Path $candidate) {
            $clangTidy = $candidate
            $searchLog += "[probe] $candidate"
            break
        }
    }
    if (-not $clangTidy) {
        $searchLog += "[probe] no clang-tidy.exe under common VS install roots"
    }
}

if (-not $clangTidy) {
    # PATH 上的独立 LLVM 安装
    $pathCmd = Get-Command clang-tidy.exe -ErrorAction SilentlyContinue
    if ($pathCmd) {
        $clangTidy = $pathCmd.Source
        $searchLog += "[PATH] $($pathCmd.Source)"
    } else {
        $searchLog += "[PATH] clang-tidy.exe not on PATH"
    }
}

if (-not $clangTidy) {
    Write-Error ("clang-tidy.exe not found. Search trace:`n  " + ($searchLog -join "`n  "))
    exit 1
}
Write-Host "Using clang-tidy: $clangTidy" -ForegroundColor Cyan
& $clangTidy --version

# ── 从 vcxproj 提取项目真实编译单元 ──
function Get-ProjectTUs {
    $vcxprojPath = Join-Path $projectRoot "foo_ui_webview2.vcxproj"
    if (-not (Test-Path $vcxprojPath)) {
        Write-Error "vcxproj not found: $vcxprojPath"
        exit 1
    }
    [xml]$proj = Get-Content $vcxprojPath -Encoding UTF8
    $nsMgr = [System.Xml.XmlNamespaceManager]::new($proj.NameTable)
    $nsMgr.AddNamespace('ms', 'http://schemas.microsoft.com/developer/msbuild/2003')
    $proj.SelectNodes('//ms:ClCompile/@Include', $nsMgr) | ForEach-Object {
        $rel = $_.Value
        if ($rel -match '^src\\' -and $rel -ne 'src\pch.cpp') {
            Join-Path $projectRoot $rel
        }
    } | Where-Object { $_ }
}

# ── 收集文件 ──
if ($GitDiff) {
    # 收集 .cpp 和 .h 差异
    $diffCpp = git diff --name-only --diff-filter=d HEAD -- 'src/*.cpp' 'src/**/*.cpp'
    $diffH   = git diff --name-only --diff-filter=d HEAD -- 'src/*.h' 'src/**/*.h'
    $diffCpp = @($diffCpp | Where-Object { $_ -and $_ -ne "src/pch.cpp" })
    $diffH   = @($diffH   | Where-Object { $_ })

    if ($diffCpp.Count -eq 0 -and $diffH.Count -eq 0) {
        Write-Host "No changed .cpp/.h files in src/." -ForegroundColor Green
        exit 0
    }

    if ($diffH.Count -gt 0) {
        # 头文件变更 → 全量扫描项目 TU（因为头文件可能被多个 TU 引用）
        Write-Host "Detected $($diffH.Count) changed header(s) — expanding to full project scan" -ForegroundColor Yellow
        $diffH | ForEach-Object { Write-Host "  [.h] $_" -ForegroundColor Yellow }
        $Files = @(Get-ProjectTUs)
    } else {
        # 纯 .cpp 变更 → 只扫改动文件（必须在项目 TU 中）
        $projectTUs = @(Get-ProjectTUs)
        $Files = $diffCpp | ForEach-Object { Join-Path $projectRoot $_ } |
            Where-Object { $_ -in $projectTUs }
        $skipped = $diffCpp | ForEach-Object { Join-Path $projectRoot $_ } |
            Where-Object { $_ -notin $projectTUs }
        if ($skipped) {
            Write-Warning "Skipped non-project files: $($skipped -join ', ')"
        }
    }

    if (-not $Files -or $Files.Count -eq 0) {
        Write-Host "No project TU files to scan." -ForegroundColor Green
        exit 0
    }
    Write-Host "Git diff mode: $($Files.Count) files to scan" -ForegroundColor Cyan
}
elseif (-not $Files) {
    $Files = @(Get-ProjectTUs)
    Write-Host "Scanning $($Files.Count) project TUs..." -ForegroundColor Cyan
}
else {
    # -Files 显式传入：先检查存在性，再校验是否在项目 TU 中
    $projectTUs = @(Get-ProjectTUs)
    $resolvedFiles = @()
    $hasError = $false
    foreach ($f in $Files) {
        $absPath = if ([System.IO.Path]::IsPathRooted($f)) { $f } else { Join-Path $projectRoot $f }
        $absPath = [System.IO.Path]::GetFullPath($absPath)
        if (-not (Test-Path $absPath)) {
            Write-Host "[ERROR] File not found: $f" -ForegroundColor Red
            $hasError = $true
        }
        elseif ($absPath -notin $projectTUs) {
            Write-Warning "Skipping non-project TU (not in vcxproj): $f"
        }
        else {
            $resolvedFiles += $absPath
        }
    }
    if ($hasError) {
        exit 1
    }
    if ($resolvedFiles.Count -eq 0) {
        Write-Host "[SKIP] All specified files are non-project TUs, nothing to scan." -ForegroundColor Yellow
        exit 0
    }
    $Files = $resolvedFiles
    Write-Host "Scanning $($Files.Count) specified files..." -ForegroundColor Cyan
}

# ── 哈希缓存过滤 ──
$totalFilesBeforeCache = $Files.Count
if ($cacheEnabled -and $Files.Count -gt 0) {
    $uncachedFiles = @()
    foreach ($f in $Files) {
        $key = Get-FileCacheKey -FilePath $f
        $relPath = $f.Replace("$projectRoot\", "")
        if ($cacheData.ContainsKey($relPath) -and $cacheData[$relPath] -eq $key) {
            $cacheHits++
        } else {
            $uncachedFiles += $f
        }
    }
    if ($cacheHits -gt 0) {
        Write-Host "[CACHE] 跳过 $cacheHits 个未变更文件，扫描 $($uncachedFiles.Count)/$totalFilesBeforeCache" -ForegroundColor Green
    }
    $Files = $uncachedFiles
    if ($Files.Count -eq 0) {
        Write-Host "[CACHE] 所有文件都与上次扫描一致，无需重新扫描" -ForegroundColor Green
        Write-Host "使用 -NoCache 强制重新扫描" -ForegroundColor Gray
        exit 0
    }
}

# ── 编译参数 ──
$compileArgs = @(
    "--driver-mode=cl",
    "/std:c++20",
    "/utf-8",
    "/EHsc",
    "/permissive-",
    "/W3",
    "/DNDEBUG",
    "/DWIN32",
    "/D_WINDOWS",
    "/D_USRDLL",
    "/DFOOBAR2000_TARGET_VERSION=82",
    "/DFOO_UI_WEBVIEW2_EXPORTS",
    "/D_UNICODE",
    "/DUNICODE",
    "/DWIN32_LEAN_AND_MEAN",
    "/DNOMINMAX",
    "/I$projectRoot\packages\Microsoft.Windows.ImplementationLibrary.1.0.240803.1\include",
    "/I$projectRoot\packages\Microsoft.Web.WebView2.1.0.3719.77\build\native\include",
    "/I$projectRoot\src",
    "/I$projectRoot\lib\foobar2000_sdk",
    "/I$projectRoot\lib\columns_ui-sdk",
    "/I$projectRoot\lib\json\include"
)

# ── 结果收集 ──
$totalWarnings = 0
$totalErrors = 0
$results = @()

# 安全检查：--fix 模式下强制串行
if ($Fix -and $Jobs -gt 1) {
    Write-Warning "--fix 模式下不可并行，已强制 -Jobs 1"
    $Jobs = 1
}

$useParallel = ($Jobs -gt 1) -and ($PSVersionTable.PSVersion.Major -ge 7)

if ($useParallel) {
    Write-Host "并行扫描: $Jobs 线程" -ForegroundColor Cyan

    $rawResults = $Files | ForEach-Object -ThrottleLimit $Jobs -Parallel {
        $file = $_
        $ct = $using:clangTidy
        $root = $using:projectRoot
        $cArgs = $using:compileArgs
        $chk = $using:Checks
        $tSec = $using:TimeoutSec

        $relPath = $file.Replace("$root\", "")

        $ctArgs = @($file, "--header-filter=src/.*")
        if ($chk) { $ctArgs += "--checks=$chk" }
        $ctArgs += "--"
        $ctArgs += $cArgs

        # 使用 System.Diagnostics.Process + ReadToEndAsync 避免 Start-Process 空格路径参数拆分问题
        $psi = [System.Diagnostics.ProcessStartInfo]::new()
        $psi.FileName = $ct
        # 逐个参数加引号拼接，避免路径含空格被拆分
        $quotedArgs = $ctArgs | ForEach-Object {
            if ($_ -match '\s') { "`"$_`"" } else { $_ }
        }
        $psi.Arguments = $quotedArgs -join ' '
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.CreateNoWindow = $true

        $p = [System.Diagnostics.Process]::new()
        $p.StartInfo = $psi
        $p.Start() | Out-Null

        # 异步读取 stdout/stderr 防死锁
        $stdoutTask = $p.StandardOutput.ReadToEndAsync()
        $stderrTask = $p.StandardError.ReadToEndAsync()

        $exited = $p.WaitForExit($tSec * 1000)
        if (-not $exited) {
            $p.Kill()
            $p.WaitForExit(5000)
            [PSCustomObject]@{ File = $relPath; Warnings = 0; Errors = 1; Output = "TIMEOUT after ${tSec}s"; Stderr = ""; ExitCode = -1; Timeout = $true }
        } else {
            $stdoutText = $stdoutTask.GetAwaiter().GetResult()
            $stderrText = $stderrTask.GetAwaiter().GetResult()
            $exitCode = $p.ExitCode
            $warnings = ([regex]::Matches($stdoutText, ':\d+:\d+: warning:')).Count
            $errors = ([regex]::Matches($stdoutText, ':\d+:\d+: error:')).Count

            # 退出码非零但未检测到诊断 → 可能 crash/parse failure
            if ($exitCode -ne 0 -and $warnings -eq 0 -and $errors -eq 0) {
                $errors = 1  # 标记为异常
                $stdoutText += "`n[PROCESS EXIT CODE: $exitCode]"
                if ($stderrText) { $stdoutText += "`n[STDERR]: $stderrText" }
            }
            # stderr 含 crash/fatal 关键词时也标记
            if ($stderrText -match 'crash|fatal|PLEASE submit|^error:') {
                if ($errors -eq 0) { $errors = 1 }
                $stdoutText += "`n[STDERR ANOMALY]: $stderrText"
            }

            [PSCustomObject]@{ File = $relPath; Warnings = $warnings; Errors = $errors; Output = $stdoutText; Stderr = $stderrText; ExitCode = $exitCode; Timeout = $false }
        }
        $p.Dispose()
    }

    # 合并结果
    foreach ($r in $rawResults) {
        $totalWarnings += $r.Warnings
        $totalErrors += $r.Errors
        if ($r.Warnings -gt 0 -or $r.Errors -gt 0) {
            $results += $r
        }
        $status = if ($r.Timeout) { "TIMEOUT" } elseif ($r.Errors -gt 0) { "ERROR" } elseif ($r.Warnings -gt 0) { "WARN" } else { "OK" }
        $color = switch ($status) { "OK" { "Green" } "WARN" { "Yellow" } default { "Red" } }
        Write-Host "  [$status] $($r.File) (W:$($r.Warnings) E:$($r.Errors))" -ForegroundColor $color
    }
}
else {
    # 串行模式（兼容 PowerShell 5.1 或 -Jobs 1）
    if ($Jobs -le 1) { Write-Host "串行扫描模式" -ForegroundColor Cyan }
    else { Write-Host "PowerShell < 7，回退串行模式" -ForegroundColor Yellow }

    foreach ($file in $Files) {
        $relPath = $file.Replace("$projectRoot\", "")
        Write-Host "  Checking: $relPath" -ForegroundColor Gray

        $ctArgs = @($file, "--header-filter=src/.*")
        if ($Checks) { $ctArgs += "--checks=$Checks" }
        if ($Fix) { $ctArgs += "--fix" }
        $ctArgs += "--"
        $ctArgs += $compileArgs

        # PS 5.1 下 clang-tidy 写 stderr（如 "N warnings generated."）会以 ErrorRecord
        # 形式进入管道；脚本全局 $ErrorActionPreference="Stop" 会把它升级为终止性错误，
        # 导致串行模式扫到第一个文件就中断。此处临时降级为 Continue，
        # 并用 try/finally 保证异常路径（如可执行文件中途失效）也能恢复原偏好。
        $prevEap = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $output = & $clangTidy @ctArgs 2>&1
            $exitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $prevEap
        }
        $stdout = $output | Where-Object { $_ -is [string] -or $_.GetType().Name -eq 'String' }
        $stderr = $output | Where-Object { $_ -is [System.Management.Automation.ErrorRecord] }
        $stdoutText = ($stdout -join "`n")

        $stderrText = ""
        if ($stderr) {
            $stderrText = ($stderr | ForEach-Object { $_.ToString() }) -join "`n"
            if ($stderrText -match 'crash|fatal|PLEASE submit|^error:') {
                Write-Warning "clang-tidy stderr for ${relPath}:`n$stderrText"
            }
        }

        $warnings = ($stdout | Select-String ":\d+:\d+: warning:" | Measure-Object).Count
        $errors = ($stdout | Select-String ":\d+:\d+: error:" | Measure-Object).Count

        # 退出码非零但无诊断 → crash / 文件不存在 / parse failure
        if ($exitCode -ne 0 -and $warnings -eq 0 -and $errors -eq 0) {
            $errors = 1
            $stdoutText += "`n[PROCESS EXIT CODE: $exitCode]"
            if ($stderrText) { $stdoutText += "`n[STDERR]: $stderrText" }
            Write-Host "  [ERROR] $relPath (exit code $exitCode)" -ForegroundColor Red
        }
        # stderr 含 crash/fatal 关键词也标记异常
        if ($stderrText -match 'crash|fatal|PLEASE submit|^error:') {
            if ($errors -eq 0) { $errors = 1 }
            $stdoutText += "`n[STDERR ANOMALY]: $stderrText"
        }

        $totalWarnings += $warnings
        $totalErrors += $errors

        if ($warnings -gt 0 -or $errors -gt 0) {
            $results += [PSCustomObject]@{
                File = $relPath
                Warnings = $warnings
                Errors = $errors
                Output = $stdoutText
            }
        }
    }
}

# ── 输出报告 ──
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "clang-tidy 扫描完成" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "文件数: $($Files.Count)"
Write-Host "警告数: $totalWarnings" -ForegroundColor $(if ($totalWarnings -gt 0) { "Yellow" } else { "Green" })
Write-Host "错误数: $totalErrors" -ForegroundColor $(if ($totalErrors -gt 0) { "Red" } else { "Green" })

if ($results.Count -gt 0) {
    Write-Host "`n--- 问题文件 ---" -ForegroundColor Yellow
    $results | Format-Table -Property File, Warnings, Errors -AutoSize

    # 写报告文件
    $reportPath = "$projectRoot\clang-tidy-report.txt"
    $results | ForEach-Object {
        "=== $($_.File) (W:$($_.Warnings) E:$($_.Errors)) ===`n$($_.Output)`n"
    } | Set-Content $reportPath -Encoding UTF8
    Write-Host "详细报告: $reportPath" -ForegroundColor Cyan
}

# ── 保存缓存（只缓存无错误的文件） ──
if ($cacheEnabled) {
    foreach ($file in $Files) {
        $relPath = $file.Replace("$projectRoot\", "")
        $fileHasIssue = $results | Where-Object { $_.File -eq $relPath -and $_.Errors -gt 0 }
        if (-not $fileHasIssue) {
            $key = Get-FileCacheKey -FilePath $file
            $cacheData[$relPath] = $key
        } else {
            # 有错误的文件从缓存中移除，确保下次重新扫描
            $cacheData.Remove($relPath) | Out-Null
        }
    }
    $cacheData | ConvertTo-Json -Depth 3 | Set-Content $cacheFile -Encoding UTF8
    Write-Host "[CACHE] 已更新缓存 ($($cacheData.Count) 条目)" -ForegroundColor Gray
}

if ($cacheHits -gt 0) {
    Write-Host "缓存命中: $cacheHits/$totalFilesBeforeCache (扫描 $($Files.Count))" -ForegroundColor Green
}

exit $(if ($totalErrors -gt 0) { 1 } else { 0 })
