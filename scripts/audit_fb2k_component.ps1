<#
.SYNOPSIS
    Validate a foo_ui_webview2 .fb2k-component package after build (R9 / A6)

.DESCRIPTION
    Performs four post-build checks on the produced .fb2k-component archive:
      [1] File inventory whitelist  -- required files must exist (x86 + x64 dll, WebView2Loader)
      [2] File inventory blacklist  -- forbidden artefacts (PDB / ILK / LIB / OBJ / MAP /
                                       any *test*.dll / *_test.* / docs leak / sourcemap)
      [3] Size regression           -- total archive must fit a budget (default 20 MB);
                                       SDK ZIP must fit a smaller one (default 5 MB)
      [4] Archive integrity         -- .fb2k-component must be a valid ZIP and openable

    Exit codes (POLICY-style):
      0  pass
      1  --strict and at least one violation
      2  script error (target missing, ZIP unreadable etc.)

    Designed to be invoked by build-package.ps1 immediately after the
    Compress-Archive step. Can also be run standalone:
      .\scripts\audit_fb2k_component.ps1 -Package dist\foo_ui_webview2-1.7.0.fb2k-component -Strict

.PARAMETER Package
    Path to the .fb2k-component archive to validate. Required.

.PARAMETER SdkZip
    Optional path to the companion SDK ZIP for additional size + content checks.

.PARAMETER MaxSizeMB
    Component archive size budget in MB (default 20).

.PARAMETER MaxSdkSizeMB
    SDK ZIP size budget in MB (default 5).

.PARAMETER Strict
    Exit 1 on any violation. Without --strict the script reports violations
    but exits 0 (warn-only mode).

.PARAMETER Json
    Emit machine-readable JSON report to stdout instead of the human table.
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$Package,

    [Parameter(Mandatory=$false)]
    [string]$SdkZip,

    [Parameter(Mandatory=$false)]
    [int]$MaxSizeMB = 20,

    [Parameter(Mandatory=$false)]
    [int]$MaxSdkSizeMB = 5,

    [switch]$Strict,
    [switch]$Json
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Package)) {
    Write-Error "Package not found: $Package"
    exit 2
}

# ----------------------------------------------------------------------------
# Helpers
# ----------------------------------------------------------------------------

Add-Type -AssemblyName System.IO.Compression.FileSystem

function Open-Zip {
    param([string]$Path)
    try {
        return [System.IO.Compression.ZipFile]::OpenRead($Path)
    } catch {
        Write-Error "Cannot open ZIP: $Path -- $($_.Exception.Message)"
        exit 2
    }
}

function Inspect-Archive {
    param([string]$Path)
    $zip = Open-Zip -Path $Path
    try {
        $entries = @()
        foreach ($e in $zip.Entries) {
            $entries += [PSCustomObject]@{
                # Preserve raw entry name: foobar2000 requires ZIP APPNOTE
                # forward-slash separators to recognize x64/ architecture dirs.
                RawName = $e.FullName
                Name = $e.FullName.Replace('\','/')
                Length = $e.Length
                Compressed = $e.CompressedLength
            }
        }
        return $entries
    } finally {
        $zip.Dispose()
    }
}

# ----------------------------------------------------------------------------
# Required / Forbidden patterns
# ----------------------------------------------------------------------------

$REQUIRED_FILES = @(
    "foo_ui_webview2.dll",
    "WebView2Loader.dll",
    "x64/foo_ui_webview2.dll",
    "x64/WebView2Loader.dll"
)

# Pattern -> reason mapping (regex, case-insensitive on Windows paths)
$FORBIDDEN_PATTERNS = @{
    "\.pdb$"        = "debug symbol (PDB) must not ship in release component"
    "\.ilk$"        = "incremental link file (ILK) is a build artefact"
    "\.exp$"        = "import library export file (EXP) is a build artefact"
    "\.lib$"        = "static library file (LIB) must not ship"
    "\.obj$"        = "object file (OBJ) must not ship"
    "\.map$"        = "linker map file (MAP) leaks symbol layout"
    "_test\."       = "test artefact in release package"
    "[\\/]tests?[\\/]" = "tests directory must not ship"
    "\.tlog$"       = "build tracker log (TLOG)"
    "\.iobj$"       = "incremental object file (IOBJ)"
    "\.ipdb$"       = "incremental PDB (IPDB)"
}

# ----------------------------------------------------------------------------
# Run checks
# ----------------------------------------------------------------------------

$violations = @()
$pkgEntries = Inspect-Archive -Path $Package
$pkgFileInfo = Get-Item $Package
$pkgSizeMB = [math]::Round($pkgFileInfo.Length / 1MB, 3)

# Check 1: required files
$pkgNames = $pkgEntries | ForEach-Object { $_.Name }
$pkgNamesSet = @{}
foreach ($n in $pkgNames) { $pkgNamesSet[$n] = $true }
foreach ($req in $REQUIRED_FILES) {
    if (-not $pkgNamesSet.ContainsKey($req)) {
        $violations += [PSCustomObject]@{
            Kind = "missing_required"
            Target = "component"
            File = $req
            Reason = "required file missing from .fb2k-component"
        }
    }
}

# Check 2: forbidden patterns
foreach ($e in $pkgEntries) {
    foreach ($pattern in $FORBIDDEN_PATTERNS.Keys) {
        if ($e.Name -match $pattern) {
            $violations += [PSCustomObject]@{
                Kind = "forbidden_file"
                Target = "component"
                File = $e.Name
                Reason = $FORBIDDEN_PATTERNS[$pattern]
            }
            break
        }
    }
}

# Check 2b: ZIP entry path separators must be forward slash (APPNOTE).
# Backslash entries (from ZipFile.CreateFromDirectory on Windows) prevent
# foobar2000 from promoting x64/ payloads over the root x86 DLLs on install.
foreach ($e in $pkgEntries) {
    if ($e.RawName.Contains('\')) {
        $violations += [PSCustomObject]@{
            Kind = "zip_path_separator"
            Target = "component"
            File = $e.RawName
            Reason = "ZIP entry uses backslash; foobar2000 requires forward slash (x64/foo.dll) for architecture subdirs"
        }
    }
}

# Check 3: size regression (component)
if ($pkgSizeMB -gt $MaxSizeMB) {
    $violations += [PSCustomObject]@{
        Kind = "size_regression"
        Target = "component"
        File = $Package
        Reason = "$pkgSizeMB MB exceeds budget $MaxSizeMB MB"
    }
}

# Check 4: SDK ZIP (optional)
$sdkSizeMB = $null
$sdkEntryCount = 0
if ($SdkZip -and (Test-Path $SdkZip)) {
    $sdkEntries = Inspect-Archive -Path $SdkZip
    $sdkSizeMB = [math]::Round((Get-Item $SdkZip).Length / 1MB, 3)
    $sdkEntryCount = $sdkEntries.Count
    if ($sdkSizeMB -gt $MaxSdkSizeMB) {
        $violations += [PSCustomObject]@{
            Kind = "size_regression"
            Target = "sdk-zip"
            File = $SdkZip
            Reason = "$sdkSizeMB MB exceeds SDK budget $MaxSdkSizeMB MB"
        }
    }
    # Forbidden patterns for SDK ZIP (looser; only PDB / test fixtures)
    foreach ($e in $sdkEntries) {
        if ($e.Name -match "\.pdb$" -or $e.Name -match "_test\." -or $e.Name -match "[\\/]tests?[\\/]") {
            $violations += [PSCustomObject]@{
                Kind = "forbidden_file"
                Target = "sdk-zip"
                File = $e.Name
                Reason = "forbidden artefact in SDK ZIP"
            }
        }
    }
}

# ----------------------------------------------------------------------------
# Report
# ----------------------------------------------------------------------------

if ($Json) {
    $report = [PSCustomObject]@{
        ok = ($violations.Count -eq 0)
        package = $Package
        packageSizeMB = $pkgSizeMB
        packageEntries = $pkgEntries.Count
        sdkZip = $SdkZip
        sdkZipSizeMB = $sdkSizeMB
        sdkZipEntries = $sdkEntryCount
        budgetMaxSizeMB = $MaxSizeMB
        budgetMaxSdkSizeMB = $MaxSdkSizeMB
        violations = $violations
        violationCount = $violations.Count
    }
    $report | ConvertTo-Json -Depth 5
} else {
    Write-Host ""
    Write-Host "=== fb2k-component audit ===" -ForegroundColor Cyan
    Write-Host "  Component : $Package" -ForegroundColor Gray
    Write-Host "  Size      : $pkgSizeMB MB / budget $MaxSizeMB MB" -ForegroundColor Gray
    Write-Host "  Entries   : $($pkgEntries.Count)" -ForegroundColor Gray
    if ($SdkZip) {
        Write-Host "  SDK ZIP   : $SdkZip" -ForegroundColor Gray
        Write-Host "  SDK Size  : $sdkSizeMB MB / budget $MaxSdkSizeMB MB" -ForegroundColor Gray
    }
    Write-Host ""

    if ($violations.Count -eq 0) {
        Write-Host "  OK - no violations" -ForegroundColor Green
    } else {
        Write-Host "  Violations: $($violations.Count)" -ForegroundColor Yellow
        foreach ($v in $violations) {
            Write-Host "    [$($v.Kind)] $($v.Target):$($v.File)" -ForegroundColor Yellow
            Write-Host "        $($v.Reason)" -ForegroundColor DarkYellow
        }
    }
    Write-Host ""
}

if ($Strict -and $violations.Count -gt 0) {
    exit 1
}
exit 0