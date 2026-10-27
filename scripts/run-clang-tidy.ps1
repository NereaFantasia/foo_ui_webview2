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
.PARAMETER Since
    与 -GitDiff 相同，但比较基准是给定的提交或分支，而不是 HEAD：
    扫描自那以后提交过的与尚未提交的改动。例如 -Since dev 扫描当前分支相对 dev 改过的文件。
.PARAMETER Fast
    快速模式：跳过 clang-analyzer-*（最耗时的检查类），单文件提速 2~5 倍
.PARAMETER Deep
    深度模式：强制包含 clang-analyzer-*（默认行为，与 .clang-tidy 一致）
.PARAMETER NoCache
    禁用哈希缓存，强制重新扫描所有文件
.PARAMETER TimeoutSec
    单文件扫描超时秒数（默认 120）
.PARAMETER Jobs
    并行扫描线程数。默认在大小核混合的 CPU 上等于能效核数，否则为逻辑处理器数 / 3（上限 12）。
    设为 1 禁用并行。
.PARAMETER NoAffinity
    不把 clang-tidy 进程限定在能效核上。并行扫描时子进程默认以低于正常的优先级运行，
    在大小核混合的 CPU 上只用能效核，性能核留给前台。
.EXAMPLE
    .\scripts\run-clang-tidy.ps1              # 全量扫描（自动并行）
    .\scripts\run-clang-tidy.ps1 -Fast        # 快速模式（跳过 analyzer）
    .\scripts\run-clang-tidy.ps1 -GitDiff     # 增量扫描
    .\scripts\run-clang-tidy.ps1 -Since dev   # 当前分支相对 dev 的全部改动
    .\scripts\run-clang-tidy.ps1 -NoCache     # 跳过缓存
    .\scripts\run-clang-tidy.ps1 -Jobs 8      # 指定线程数
#>
param(
    [string[]]$Files,
    [switch]$Fix,
    [string]$Checks,
    [switch]$GitDiff,
    [string]$Since,
    [switch]$Fast,
    [switch]$Deep,
    [switch]$NoCache,
    [int]$TimeoutSec = 120,
    [int]$Jobs = 0,
    [switch]$NoAffinity
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path $PSScriptRoot -Parent
$headerExtensions = @('.h', '.hpp', '.hh', '.hxx', '.inl', '.inc', '.ipp', '.tpp')

# ── 线程数与能效核 ──
# 大小核混合时返回能效等级最低那组逻辑处理器的掩码；同构 CPU、多处理器组或查询失败时返回 0。
function Get-EfficiencyCoreMask {
    try {
        if (-not ('ClangTidyCpuSets' -as [type])) {
            Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ClangTidyCpuSets {
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool GetSystemCpuSetInformation(IntPtr info, uint length, out uint returned, IntPtr process, uint flags);
    // 每个 CPU 集依次给出：处理器组、逻辑处理器号、能效等级。
    public static int[] Read() {
        uint length;
        GetSystemCpuSetInformation(IntPtr.Zero, 0, out length, IntPtr.Zero, 0);
        IntPtr buffer = Marshal.AllocHGlobal((int)length);
        try {
            if (!GetSystemCpuSetInformation(buffer, length, out length, IntPtr.Zero, 0)) return new int[0];
            var values = new System.Collections.Generic.List<int>();
            for (int offset = 0; offset < length; offset += Marshal.ReadInt32(buffer, offset)) {
                values.Add(Marshal.ReadInt16(buffer, offset + 12));
                values.Add(Marshal.ReadByte(buffer, offset + 14));
                values.Add(Marshal.ReadByte(buffer, offset + 18));
            }
            return values.ToArray();
        } finally {
            Marshal.FreeHGlobal(buffer);
        }
    }
}
'@
        }
        $values = [ClangTidyCpuSets]::Read()
        $sets = for ($i = 0; $i + 2 -lt $values.Count; $i += 3) {
            [pscustomobject]@{ Group = $values[$i]; Processor = $values[$i + 1]; Efficiency = $values[$i + 2] }
        }
        if (-not $sets -or ($sets | Where-Object Group -ne 0)) { return 0L }
        $lowest = ($sets | Measure-Object Efficiency -Minimum).Minimum
        if (($sets | Where-Object Efficiency -ne $lowest).Count -eq 0) { return 0L }
        $mask = 0L
        $sets | Where-Object Efficiency -eq $lowest | ForEach-Object { $mask = $mask -bor (1L -shl $_.Processor) }
        return $mask
    } catch {
        return 0L
    }
}

$affinityMask = if ($NoAffinity) { 0L } else { Get-EfficiencyCoreMask }
$efficiencyCores = 0
for ($rest = $affinityMask; $rest -ne 0; $rest = $rest -band ($rest - 1)) { $efficiencyCores++ }
if ($Jobs -le 0) {
    $Jobs = if ($efficiencyCores -gt 0) { $efficiencyCores }
            else { [Math]::Max(1, [Math]::Min([Math]::Floor([Environment]::ProcessorCount / 3), 12)) }
}
$coreNote = if ($efficiencyCores -gt 0) { "，限定在 $efficiencyCores 个能效核" } else { '' }
Write-Host "并行线程: $Jobs (逻辑处理器: $([Environment]::ProcessorCount)$coreNote)" -ForegroundColor Cyan

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
if ($cacheEnabled -and $PSVersionTable.PSVersion.Major -lt 6) {
    # 旧版目录枚举不能跟随链接；保留扫描，避免复用缺少依赖信息的结果。
    $cacheEnabled = $false
    Write-Host "[CACHE] PowerShell 5 does not support linked dependency traversal; scanning without cache." -ForegroundColor Yellow
}
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

function Get-Digest {
    param([string[]]$Records)
    $hasher = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes(($Records -join "`n"))
        return [System.BitConverter]::ToString($hasher.ComputeHash($bytes))
    } finally {
        $hasher.Dispose()
    }
}

# 同一个头文件被许多单元包含，一次运行内只算一次摘要。
$contentHashes = @{}
function Get-ContentHash {
    param([string]$Path)
    if (-not $contentHashes.ContainsKey($Path)) {
        $contentHashes[$Path] = if (Test-Path -LiteralPath $Path) {
            (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
        } else {
            'missing'
        }
    }
    return $contentHashes[$Path]
}

# 有逐单元依赖时，键里含该单元上次构建读过的每个项目文件的摘要。
function Get-FileCacheKey {
    param([string]$FilePath)
    $key = "$(Get-ContentHash $FilePath)|$checksFingerprint"
    if ($tuDependencies) {
        $records = foreach ($dependency in $tuDependencies[$FilePath.ToUpperInvariant()]) {
            "$dependency|$(Get-ContentHash $dependency)"
        }
        $key += "|deps:$(Get-Digest @($records))"
    }
    return $key
}

# 项目内全部头文件，含经目录联接引入的依赖库。
function Get-HeaderInputs {
    param([string]$Root)
    foreach ($directory in @('src', 'lib', 'packages')) {
        $dependencyRoot = Join-Path $Root $directory
        if (Test-Path -LiteralPath $dependencyRoot) {
            Get-ChildItem -LiteralPath $dependencyRoot -Recurse -File -FollowSymlink |
                Where-Object { $_.Extension -in $headerExtensions }
        }
    }
}

# 没有逐单元依赖时，所有头文件的内容共同参与缓存键；路径也计入摘要，
# 使文件新增、删除和同内容改名都能使旧结果失效。有依赖时头文件改由逐单元的键负责。
function Get-ScanInputsFingerprint {
    param([string]$Root, [string]$ToolPath, [System.IO.FileInfo[]]$Headers)
    $inputs = [System.Collections.Generic.List[string]]::new()
    $inputs.Add($ToolPath)
    $inputs.Add((Join-Path $Root 'scripts/run-clang-tidy.ps1'))
    $inputs.Add((Join-Path $Root 'foo_ui_webview2.vcxproj'))
    foreach ($header in $Headers) { $inputs.Add($header.FullName) }
    $records = foreach ($inputFile in ($inputs | Sort-Object -Unique)) {
        "$inputFile|$(Get-ContentHash $inputFile)"
    }
    return Get-Digest @($records)
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

# 每个编译单元在上次构建中读过的项目内文件（大写完整路径），取自 MSBuild 的 CL.read.1.tlog。
# 记录缺失、比任一项目文件旧（之后可能改过包含关系）或缺少某个单元时返回 $null。
function Get-TuDependencies {
    param([string]$Root, [string[]]$ProjectTUs, [System.IO.FileInfo[]]$Headers)
    $objRoot = Join-Path $Root 'obj\foo_ui_webview2'
    if (-not (Test-Path -LiteralPath $objRoot)) { return $null }
    $tlog = Get-ChildItem -LiteralPath $objRoot -Recurse -File -Filter 'CL.read.1.tlog' |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if (-not $tlog) { return $null }
    $inputs = @($Headers) + @($ProjectTUs | ForEach-Object { Get-Item -LiteralPath $_ }) +
        @(Get-Item -LiteralPath (Join-Path $Root 'foo_ui_webview2.vcxproj'))
    if ($inputs | Where-Object { $_.LastWriteTimeUtc -gt $tlog.LastWriteTimeUtc } | Select-Object -First 1) {
        return $null
    }

    $recorded = @{}
    $current = $null
    foreach ($line in [System.IO.File]::ReadLines($tlog.FullName, [System.Text.Encoding]::Unicode)) {
        if ($line.StartsWith('^')) {
            $current = [System.Collections.Generic.List[string]]::new()
            foreach ($unit in $line.Substring(1).Split('|')) { $recorded[$unit] = $current }
        } elseif ($null -ne $current -and $line) {
            $current.Add($line)
        }
    }
    $rootPrefix = $Root.TrimEnd('\').ToUpperInvariant() + '\'
    $objPrefix = $objRoot.ToUpperInvariant() + '\'
    # 使用预编译头的单元只记下 .pch 文件，补上 pch.cpp 读过的头文件。
    $pchReads = $recorded[$rootPrefix + 'SRC\PCH.CPP']
    $dependencies = @{}
    foreach ($tu in $ProjectTUs) {
        $unit = $tu.ToUpperInvariant()
        if (-not $recorded.ContainsKey($unit)) { return $null }
        $reads = @($recorded[$unit])
        if ($pchReads -and ($reads | Where-Object { $_.EndsWith('.PCH') })) { $reads += $pchReads }
        $dependencies[$unit] = @($reads | Where-Object {
            $_.StartsWith($rootPrefix) -and -not $_.StartsWith($objPrefix) -and $_ -ne $unit
        } | Sort-Object -Unique)
    }
    return $dependencies
}

# ── 逐单元依赖与缓存指纹 ──
$tuDependencies = $null
$analysisInputsChanged = $false
if ($cacheEnabled) {
    $headerInputs = @(Get-HeaderInputs -Root $projectRoot)
    $tuDependencies = Get-TuDependencies -Root $projectRoot -ProjectTUs @(Get-ProjectTUs) -Headers $headerInputs
    if ($tuDependencies) {
        Write-Host "[DEPS] 按上次构建的包含记录逐单元判断改动" -ForegroundColor Gray
        $inputsFingerprint = Get-ScanInputsFingerprint -Root $projectRoot -ToolPath $clangTidy
        $checksFingerprint += "|inputs:deps:$inputsFingerprint"
    } else {
        Write-Host "[DEPS] 没有与当前源码对应的构建记录，头文件改动按全量处理" -ForegroundColor Gray
        $inputsFingerprint = Get-ScanInputsFingerprint -Root $projectRoot -ToolPath $clangTidy -Headers $headerInputs
        $checksFingerprint += "|inputs:$inputsFingerprint"
    }
    $analysisInputsChanged = $cacheData['@inputs'] -ne $checksFingerprint
}

# ── 收集文件 ──
if ($GitDiff -or $Since) {
    # 收集 .cpp 和 .h 差异。git diff <基准> 比较基准与工作区，已提交与未提交的改动都在内。
    $base = 'HEAD'
    if ($Since) {
        git -C $projectRoot rev-parse --verify --quiet "$Since^{commit}" | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[ERROR] -Since: '$Since' is not a commit or branch" -ForegroundColor Red
            exit 2
        }
        $base = $Since
    }
    $diffCpp = git -C $projectRoot diff --name-only --diff-filter=d $base -- 'src/*.cpp' 'src/**/*.cpp'
    $diffInputs = @(git -C $projectRoot diff --name-only $base -- src lib packages .clang-tidy foo_ui_webview2.vcxproj scripts/run-clang-tidy.ps1)
    $diffInputs += @(git -C $projectRoot ls-files --others --exclude-standard -- src lib packages)
    $diffH = @($diffInputs | Where-Object { $_ -and [System.IO.Path]::GetExtension($_) -in $headerExtensions } | Sort-Object -Unique)
    $diffConfiguration = @($diffInputs | Where-Object {
        $_ -in @('.clang-tidy', 'foo_ui_webview2.vcxproj', 'scripts/run-clang-tidy.ps1') -or $_ -match '^(lib|packages)/'
    })
    $diffCpp = @($diffCpp | Where-Object { $_ -and $_ -ne "src/pch.cpp" })
    $diffH   = @($diffH   | Where-Object { $_ })

    if ($diffCpp.Count -eq 0 -and $diffH.Count -eq 0 -and $diffConfiguration.Count -eq 0 -and -not $analysisInputsChanged) {
        Write-Host "No changed source, header or analysis configuration files." -ForegroundColor Green
        exit 0
    }

    if ($diffConfiguration.Count -gt 0 -or $analysisInputsChanged -or ($diffH.Count -gt 0 -and -not $tuDependencies)) {
        # 包含文件删除后仍须检查使用方，不能仅按当前存在的文件收集差异。
        # Git 可能信任未变的时间戳和大小，因此也核对缓存中的内容摘要。
        Write-Host "Detected changed headers or analysis inputs — expanding to full project scan" -ForegroundColor Yellow
        $diffH | ForEach-Object { Write-Host "  [header] $_" -ForegroundColor Yellow }
        $Files = @(Get-ProjectTUs)
    } elseif ($diffH.Count -gt 0) {
        # 删除的头文件仍在旧记录里，读过它的单元照样会被选中。
        $changedHeaders = [System.Collections.Generic.HashSet[string]]::new(
            [string[]]@($diffH | ForEach-Object { (Join-Path $projectRoot $_).ToUpperInvariant() }))
        $changedCpp = @($diffCpp | ForEach-Object { Join-Path $projectRoot $_ })
        $Files = @(Get-ProjectTUs | Where-Object {
            $_ -in $changedCpp -or $changedHeaders.Overlaps([string[]]$tuDependencies[$_.ToUpperInvariant()])
        })
        Write-Host "Changed headers — scanning the TUs that read them in the last build" -ForegroundColor Yellow
        $diffH | ForEach-Object { Write-Host "  [header] $_" -ForegroundColor Yellow }
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
    Write-Host "Git diff mode (base $base): $($Files.Count) files to scan" -ForegroundColor Cyan
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
$scanCacheKeys = @{}
if ($cacheEnabled -and $Files.Count -gt 0) {
    $uncachedFiles = @()
    foreach ($f in $Files) {
        $key = Get-FileCacheKey -FilePath $f
        $relPath = $f.Replace("$projectRoot\", "")
        $scanCacheKeys[$relPath] = $key
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
        # 启动后立即降优先级并限定核心；进程已经退出时设置会失败，不影响结果。
        try {
            $p.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal
            if ($using:affinityMask -ne 0) { $p.ProcessorAffinity = [IntPtr]$using:affinityMask }
        } catch {
            Write-Verbose "clang-tidy exited before scheduling could be adjusted: $_"
        }

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

# 有诊断的文件继续复扫，避免缓存命中把既有告警显示成零告警。
if ($cacheEnabled) {
    foreach ($file in $Files) {
        $relPath = $file.Replace("$projectRoot\", "")
        $fileHasIssue = $results | Where-Object { $_.File -eq $relPath -and ($_.Errors -gt 0 -or $_.Warnings -gt 0) }
        if (-not $fileHasIssue) {
            # 保存扫描前的输入；检查期间的改动留到下一次识别，不能替它背书。
            $cacheData[$relPath] = $scanCacheKeys[$relPath]
        } else {
            # 清除旧的成功记录，确保下次重新扫描。
            $cacheData.Remove($relPath) | Out-Null
        }
    }
    $cacheData['@inputs'] = $checksFingerprint
    $cacheData | ConvertTo-Json -Depth 3 | Set-Content $cacheFile -Encoding UTF8
    Write-Host "[CACHE] 已更新缓存 ($($cacheData.Count - 1) 条目)" -ForegroundColor Gray
}

if ($cacheHits -gt 0) {
    Write-Host "缓存命中: $cacheHits/$totalFilesBeforeCache (扫描 $($Files.Count))" -ForegroundColor Green
}

exit $(if ($totalErrors -gt 0) { 1 } else { 0 })
