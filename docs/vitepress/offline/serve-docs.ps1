$ErrorActionPreference = 'Stop'

$siteRoot = Join-Path $PSScriptRoot 'foo_ui_webview2'
$siteBase = '/foo_ui_webview2/'

if (-not (Test-Path (Join-Path $siteRoot 'index.html') -PathType Leaf)) {
    Write-Host 'The documentation files are missing.' -ForegroundColor Red
    Write-Host 'Extract the complete ZIP, then run open-docs.cmd again.'
    exit 1
}

function Get-ContentType([string]$path) {
    switch ([System.IO.Path]::GetExtension($path).ToLowerInvariant()) {
        '.html' { 'text/html; charset=utf-8' }
        '.css' { 'text/css; charset=utf-8' }
        '.js' { 'text/javascript; charset=utf-8' }
        '.mjs' { 'text/javascript; charset=utf-8' }
        '.json' { 'application/json; charset=utf-8' }
        '.xml' { 'application/xml; charset=utf-8' }
        '.txt' { 'text/plain; charset=utf-8' }
        '.svg' { 'image/svg+xml' }
        '.png' { 'image/png' }
        '.jpg' { 'image/jpeg' }
        '.jpeg' { 'image/jpeg' }
        '.gif' { 'image/gif' }
        '.webp' { 'image/webp' }
        '.ico' { 'image/x-icon' }
        '.woff2' { 'font/woff2' }
        '.woff' { 'font/woff' }
        '.map' { 'application/json' }
        default { 'application/octet-stream' }
    }
}

function Resolve-SiteFile([string]$requestPath) {
    $decoded = [System.Uri]::UnescapeDataString($requestPath)
    if (-not $decoded.StartsWith($siteBase, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $null
    }

    $relative = $decoded.Substring($siteBase.Length).TrimStart('/')
    if ([string]::IsNullOrWhiteSpace($relative)) {
        $relative = 'index.html'
    }

    $candidate = Join-Path $siteRoot ($relative.Replace('/', '\'))
    if (Test-Path $candidate -PathType Container) {
        $candidate = Join-Path $candidate 'index.html'
    } elseif (-not [System.IO.Path]::GetExtension($candidate)) {
        $htmlCandidate = "$candidate.html"
        if (Test-Path $htmlCandidate -PathType Leaf) {
            $candidate = $htmlCandidate
        }
    }

    try {
        $fullPath = [System.IO.Path]::GetFullPath($candidate)
        $rootPath = [System.IO.Path]::GetFullPath($siteRoot).TrimEnd('\') + '\'
    } catch {
        return $null
    }

    if (-not $fullPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $null
    }
    if (-not (Test-Path $fullPath -PathType Leaf)) {
        return $null
    }
    return $fullPath
}

function Get-CandidatePorts {
    # Windows reserves large blocks of the dynamic range (Hyper-V, WSL, WinNAT),
    # so a sequential scan from a fixed start port can fail hundreds of times.
    # Probe a randomized spread of the private range instead.
    $ports = New-Object System.Collections.Generic.List[int]
    $random = New-Object System.Random
    while ($ports.Count -lt 64) {
        $candidate = $random.Next(49152, 65536)
        if (-not $ports.Contains($candidate)) { $ports.Add($candidate) }
    }
    return $ports
}

$listener = $null
$port = $null
foreach ($candidatePort in Get-CandidatePorts) {
    # A failed Start() disposes the HttpListener, so every attempt needs a new
    # instance. Reusing one makes .Prefixes null on the next iteration.
    $candidateListener = New-Object System.Net.HttpListener
    try {
        $candidateListener.Prefixes.Add("http://127.0.0.1:$candidatePort/")
        $candidateListener.Start()
        $listener = $candidateListener
        $port = $candidatePort
        break
    } catch {
        try { $candidateListener.Close() } catch {}
    }
}

if ($null -eq $listener) {
    Write-Host 'No free local port was available for the documentation server.' -ForegroundColor Red
    Write-Host 'Close other local web servers and run open-docs.cmd again.'
    exit 1
}

$url = "http://127.0.0.1:$port$siteBase"
Write-Host "Opening foo_ui_webview2 documentation at $url" -ForegroundColor Cyan
Write-Host 'Keep this window open while reading. Press Ctrl+C here to stop.' -ForegroundColor DarkGray
Start-Process $url

try {
    while ($listener.IsListening) {
        $context = $listener.GetContext()
        try {
            $response = $context.Response
            $response.Headers['Cache-Control'] = 'no-store'
            $response.Headers['X-Content-Type-Options'] = 'nosniff'

            if ($context.Request.Url.AbsolutePath -eq '/') {
                $response.StatusCode = 302
                $response.RedirectLocation = $siteBase
                $response.Close()
                continue
            }

            $filePath = Resolve-SiteFile $context.Request.Url.AbsolutePath
            if ($null -eq $filePath) {
                $response.StatusCode = 404
                $body = [System.Text.Encoding]::UTF8.GetBytes('404 - File not found')
                $response.ContentType = 'text/plain; charset=utf-8'
            } else {
                $response.StatusCode = 200
                $response.ContentType = Get-ContentType $filePath
                $body = [System.IO.File]::ReadAllBytes($filePath)
            }

            $response.ContentLength64 = $body.Length
            if ($context.Request.HttpMethod -ne 'HEAD') {
                $response.OutputStream.Write($body, 0, $body.Length)
            }
            $response.Close()
        } catch {
            try { $context.Response.Abort() } catch {}
        }
    }
} finally {
    if ($listener.IsListening) { $listener.Stop() }
    $listener.Close()
}
