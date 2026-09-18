# diag-nav.ps1 -- narrow down WHY NavigationCompleted never arrives.
#
# ASCII-only on purpose (see prepare-sdk.ps1 for why).
#
# Four combinations, in increasing order of what they depend on. The FIRST one that
# fails localises the break:
#
#   1 about        about:blank. No filesystem, no page JS, no subresources.
#   2 string/plain NavigateToString with a page that has ZERO external subresources.
#                  Adds the real renderer only.
#   3 string/probe Same, but the page has one <script src="mermaid.min.js"> that
#                  resolves to a file:// URL (via the injected <base>).
#                  The ONLY difference from step 2 is that subresource.
#   4 url/plain    The real file:/// document request, page has no subresources.
#                  The ONLY difference from step 2 is how the document is fetched.
#
# Step 2 vs 3 isolates "a file:// SUBRESOURCE hangs".
# Step 2 vs 4 isolates "the file:// DOCUMENT request hangs".
#
# Orphan attribution uses the descendant PIDs the probe recorded for ITSELF, not a
# global PID diff. A global diff is wrong on any machine running other WebView2
# applications, which keep spawning msedgewebview2.exe on their own.

[CmdletBinding()]
param(
    [string]$Exe = "",
    [string]$Config = "Release",
    [int]$TimeoutMs = 15000,
    [int]$SettleMs = 3000,
    [switch]$NoShow
)

$ErrorActionPreference = "Stop"

if (-not $Exe) {
    $candidates = @(
        (Join-Path $PSScriptRoot "build\bin\webview2_probe.exe"),
        (Join-Path $PSScriptRoot "build\bin\$Config\webview2_probe.exe")
    )
    $Exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Exe -or -not (Test-Path $Exe)) {
    throw "probe executable not found. Build it first."
}
$Exe = (Resolve-Path $Exe).Path

$outDir = Join-Path $PSScriptRoot "out\diag"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Write-Host "[diag] executable: $Exe"
Write-Host "[diag] four navigation paths, about two minutes total"
Write-Host ""

$rows = New-Object System.Collections.ArrayList
$script:lastDescendants = @()

function Stop-Pids {
    param([int[]]$Pids)
    foreach ($procId in $Pids) {
        try { Stop-Process -Id $procId -Force -ErrorAction Stop } catch { }
    }
}

function Test-PidsAlive {
    param([int[]]$Pids)
    $alive = @()
    foreach ($procId in $Pids) {
        if (Get-Process -Id $procId -ErrorAction SilentlyContinue) { $alive += $procId }
    }
    return ,$alive
}

function Invoke-Mode {
    param(
        [string]$Id,
        [string]$Label,
        [string]$Mode,
        [string]$Page,
        [int]$RunTimeoutMs
    )

    Write-Host "=== $Id : $Label ==="
    $reportPath = Join-Path $outDir "$Id.json"
    Remove-Item $reportPath -Force -ErrorAction SilentlyContinue
    $profileDir = Join-Path $outDir "profile-$Id"
    Remove-Item $profileDir -Recurse -Force -ErrorAction SilentlyContinue

    $args = @("--mode", "clean")
    $args += @("--nav-mode", $Mode)
    $args += @("--report", $reportPath)
    $args += @("--page", (Join-Path $PSScriptRoot "page\$Page"))
    $args += @("--page-query", "blocks=200&diagrams=0")
    $args += @("--user-data-dir", $profileDir)
    $args += @("--fresh-profile")
    $args += @("--timeout-ms", "$RunTimeoutMs")
    if (-not $NoShow) { $args += "--show" }

    $proc = Start-Process -FilePath $Exe -ArgumentList $args -PassThru -WindowStyle Hidden
    $deadline = (Get-Date).AddMilliseconds($RunTimeoutMs + 25000)
    while (-not $proc.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 200
    }
    if (-not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        Write-Host "  host had to be killed"
    }

    $row = [ordered]@{ id = $Id; mode = $Mode; page = $Page; nav_ok = $false }

    if (Test-Path $reportPath) {
        try {
            $r = Get-Content $reportPath -Raw | ConvertFrom-Json
            $row.nav_ok  = ([int]$r.ms_navigation_completed -gt 0) -and (-not $r.navigation_failed)
            $row.nav_ms  = [int]$r.ms_navigation_completed
            $row.seen    = [bool]$r.nav_starting_seen
            if ($r.error) { $row.error = $r.error }
            if ($r.process_failed_kind) { $row.proc_failed = $r.process_failed_kind }
            if ($r.diag_script_result) { $row.page_state = $r.diag_script_result }
            $script:lastDescendants = @($r.descendant_pids)
        } catch {
            $row.error = "report parse failed: $($_.Exception.Message)"
            $script:lastDescendants = @()
        }
    } else {
        $row.error = "no report file"
        $script:lastDescendants = @()
    }

    Start-Sleep -Milliseconds $SettleMs
    $desc = @($script:lastDescendants)
    $alive = @(Test-PidsAlive -Pids $desc)
    $row.descendants = $desc.Count
    $row.orphans = $alive.Count

    Stop-Pids -Pids $desc
    Start-Sleep -Milliseconds 1500
    $row.orphans_after_kill = @(Test-PidsAlive -Pids $desc).Count

    [void]$rows.Add([pscustomobject]$row)
    Write-Host "  nav_ok=$($row.nav_ok)  nav_ms=$($row.nav_ms)  seen=$($row.seen)  descendants=$($row.descendants)  orphans=$($row.orphans)"
    if ($row.error)      { Write-Host "  error     : $($row.error)" }
    if ($row.page_state) { Write-Host "  page_state: $($row.page_state)" }
    if ($row.proc_failed){ Write-Host "  procFailed: $($row.proc_failed)" }
    Write-Host ""
}

Invoke-Mode -Id "1-about"        -Label "about:blank (plumbing only)"                -Mode "about"  -Page "plain.html" -RunTimeoutMs $TimeoutMs
Invoke-Mode -Id "2-str-plain"    -Label "NavigateToString + page with NO subresources" -Mode "string" -Page "plain.html" -RunTimeoutMs $TimeoutMs
Invoke-Mode -Id "3-str-probe"    -Label "NavigateToString + page WITH a file:// script" -Mode "string" -Page "probe.html" -RunTimeoutMs $TimeoutMs
Invoke-Mode -Id "4-url-plain"    -Label "file:// document + page with no subresources" -Mode "url"    -Page "plain.html" -RunTimeoutMs $TimeoutMs

Write-Host "================ DIAGNOSIS ================"
$rows | Select-Object id, mode, page, nav_ok, nav_ms, seen, descendants, orphans | Format-Table -AutoSize

$r1 = $rows | Where-Object { $_.id -eq "1-about" }
$r2 = $rows | Where-Object { $_.id -eq "2-str-plain" }
$r3 = $rows | Where-Object { $_.id -eq "3-str-probe" }
$r4 = $rows | Where-Object { $_.id -eq "4-url-plain" }

Write-Host ""
if (-not $r1.nav_ok) {
    Write-Host "VERDICT A: even about:blank never completes." -ForegroundColor Red
    Write-Host "  The event pipeline itself is broken here; nothing else can be trusted."
} elseif (-not $r2.nav_ok) {
    Write-Host "VERDICT B: about:blank works, but rendering a plain inline page does not." -ForegroundColor Red
    Write-Host "  The renderer cannot commit real content. No file:// involved at all."
    Write-Host "  This is a hard blocker for the WebView2 architecture on this machine."
    Write-Host "  Worth checking: antivirus/EDR interfering with Chromium child processes,"
    Write-Host "  and whether other WebView2 apps on this machine actually render (they do"
    Write-Host "  spawn processes, but spawning is not rendering)."
} elseif (-not $r3.nav_ok) {
    Write-Host "VERDICT C: a file:// SUBRESOURCE hangs the page load." -ForegroundColor Yellow
    Write-Host "  Inline rendering works; adding one <script src> that resolves to file:// breaks it."
    Write-Host "  FIX: do not load subresources over file://. Read the assets in C++ and inject them,"
    Write-Host "  or map the asset folder with SetVirtualHostNameToFolderMapping and load over https."
} elseif (-not $r4.nav_ok) {
    Write-Host "VERDICT D: the file:// DOCUMENT request hangs (subresources are fine)." -ForegroundColor Yellow
    Write-Host "  FIX: never navigate to file://. Use SetVirtualHostNameToFolderMapping to serve the"
    Write-Host "  app folder over a virtual https host, or read the page in C++ and NavigateToString."
} else {
    Write-Host "VERDICT E: all four paths work." -ForegroundColor Green
    Write-Host "  Whatever breaks the full suite is specific to probe.html content or its query string."
    Write-Host "  Compare page_state between runs and re-run the full suite."
}

Write-Host ""
Write-Host "JSON reports: $outDir"
$leftover = 0
foreach ($row in $rows) { $leftover += [int]$row.orphans_after_kill }
Write-Host "Probe-recorded descendant processes still alive after cleanup: $leftover"
