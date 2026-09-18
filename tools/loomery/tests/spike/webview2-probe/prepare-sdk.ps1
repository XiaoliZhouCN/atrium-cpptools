# prepare-sdk.ps1 -- download and extract the WebView2 SDK (NuGet package).
#
# ASCII-only on purpose: PowerShell 5.1 reads BOM-less scripts using the ANSI code
# page, so non-ASCII characters here would be mis-decoded on some hosts.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File prepare-sdk.ps1
#   powershell -ExecutionPolicy Bypass -File prepare-sdk.ps1 -Version 1.0.4191.47

[CmdletBinding()]
param(
    [string]$Version = "1.0.4191.47",
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"

if (-not $Root) {
    $Root = Join-Path $PSScriptRoot ".sdk"
}

$target = Join-Path $Root $Version
$extractDir = Join-Path $target "extracted"

if (Test-Path (Join-Path $extractDir "WebView2.h")) {
    Write-Host "[sdk] already prepared: $extractDir"
} else {
    New-Item -ItemType Directory -Force -Path $extractDir | Out-Null

    $nupkg = Join-Path $target "microsoft.web.webview2.$Version.nupkg"

    # api.nuget.org redirects to a regional mirror (nuget.azure.cn in CN), so try
    # several endpoints and keep whichever answers first.
    $urls = @(
        "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/$Version/microsoft.web.webview2.$Version.nupkg",
        "https://nuget.azure.cn/v3-flatcontainer/microsoft.web.webview2/$Version/microsoft.web.webview2.$Version.nupkg",
        "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/$Version",
        "https://globalcdn.nuget.org/packages/microsoft.web.webview2.$Version.nupkg"
    )

    $downloaded = $false
    foreach ($url in $urls) {
        try {
            Write-Host "[sdk] downloading $url"
            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
            Invoke-WebRequest -Uri $url -OutFile $nupkg -UseBasicParsing -TimeoutSec 120
            $size = (Get-Item $nupkg).Length
            if ($size -gt 100000) {
                Write-Host "[sdk] downloaded $([math]::Round($size / 1KB, 1)) KB"
                $downloaded = $true
                break
            }
            Write-Host "[sdk] response too small ($size bytes), trying next endpoint"
        } catch {
            Write-Host "[sdk] failed: $($_.Exception.Message)"
        }
    }

    if (-not $downloaded) {
        throw "Could not download the WebView2 SDK. Download it manually from https://www.nuget.org/packages/Microsoft.Web.WebView2/$Version and extract it into $extractDir"
    }

    $zip = Join-Path $target "sdk.zip"
    Copy-Item -Path $nupkg -Destination $zip -Force
    Write-Host "[sdk] extracting to $extractDir"

    # bsdtar (shipped with Windows 10+) instead of Expand-Archive: measured 216 ms
    # vs a multi-minute hang on the same 8.8 MB package.
    $tar = Get-Command tar.exe -ErrorAction SilentlyContinue
    if ($tar) {
        & tar.exe -xf $zip -C $extractDir
        if ($LASTEXITCODE -ne 0) { throw "tar extraction failed with exit code $LASTEXITCODE" }
    } else {
        Expand-Archive -Path $zip -DestinationPath $extractDir -Force
    }
    Remove-Item -Path $zip -Force -ErrorAction SilentlyContinue
}

# --- locate the pieces the CMake project needs ---------------------------------
$header = Get-ChildItem -Path $extractDir -Recurse -Filter "WebView2.h" -File |
    Select-Object -First 1
$lib = Get-ChildItem -Path $extractDir -Recurse -Filter "WebView2LoaderStatic.lib" -File |
    Where-Object { $_.FullName -match "x64" } |
    Select-Object -First 1

if (-not $header) { throw "WebView2.h not found under $extractDir" }
if (-not $lib)    { throw "WebView2LoaderStatic.lib (x64) not found under $extractDir" }

$sha = (Get-FileHash -Path (Join-Path $target "microsoft.web.webview2.$Version.nupkg") -Algorithm SHA256).Hash

Write-Host ""
Write-Host "[sdk] OK"
Write-Host "  version : $Version"
Write-Host "  header  : $($header.FullName)"
Write-Host "  lib     : $($lib.FullName)"
Write-Host "  nupkg   : sha256=$sha"
Write-Host ""
Write-Host "  Record this sha256 in the dependency manifest if the SDK is ever vendored."
