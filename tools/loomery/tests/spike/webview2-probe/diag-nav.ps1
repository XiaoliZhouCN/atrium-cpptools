# diag-nav.ps1 -- narrow down WHY NavigationCompleted never arrives.
#
# ASCII-only on purpose (see prepare-sdk.ps1 for why).
#
# The full suite (run-probe.ps1) takes minutes and needs navigation to work before
# its numbers mean anything. This script takes about a minute and answers the one
# question that everything else depends on, by trying three navigation paths in
# increasing order of dependency:
#
#   about   -> about:blank. No filesystem, no page JS. Tests only the event plumbing.
#   string  -> NavigateToString with the real page HTML. Adds the real renderer,
#              still no filesystem / network serve.
#   url     -> the real file:/// URI with the query string. Adds the filesystem path.
#
# Whichever level first fails tells you where the break is.
#
# Orphan attribution uses the descendant PIDs the probe recorded for ITSELF, not a
# global PID diff. A global diff is wrong on any machine running other WebView2
# applications, which keep spawning msedgewebview2.exe on their own.

[CmdletBinding()]
param(
    [string]$Exe = "",
    [string]$Config = "Release",
    [int]$AboutTimeoutMs = 10000,
    [int]$StringTimeoutMs = 20000,
    [int]$UrlTimeoutMs = 20000,
    [int]$SettleMs = 4000,
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
$pagePath = Join-Path $PSScriptRoot "page\probe.html"

Write-Host "[diag] executable: $Exe"
Write-Host "[diag] three navigation paths, about a minute total"
Write-Host ""

$rows = New-Object System.Collections.ArrayList

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
        [string]$Mode,
        [string]$Label,
        [int]$TimeoutMs,
        [string[]]$ExtraArgs
    )

    Write-Host "=== $Label ($Mode) ==="
    $reportPath = Join-Path $outDir "$Mode.json"
    Remove-Item $reportPath -Force -ErrorAction SilentlyContinue
    $profileDir = Join-Path $outDir "profile-$Mode"
    Remove-Item $profileDir -Recurse -Force -ErrorAction SilentlyContinue

    $args = @()
    $args += $ExtraArgs
    $args += @("--nav-mode", $Mode)
    $args += @("--report", $reportPath)
    $args += @("--page", $pagePath)
    $args += @("--page-query", "blocks=200&diagrams=0")
    $args += @("--user-data-dir", $profileDir)
    $args += @("--fresh-profile")
    $args += @("--timeout-ms", "$TimeoutMs")
    if (-not $NoShow) { $args += "--show" }

    $proc = Start-Process -FilePath $Exe -ArgumentList $args -PassThru -WindowStyle Hidden
    $deadline = (Get-Date).AddMilliseconds($TimeoutMs + 25000)
    while (-not $proc.HasExited -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 200
    }
    if (-not $proc.HasExited) {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        Write-Host "  host had to be killed"
    }

    $row = [ordered]@{ mode = $Mode; nav_ok = $false }

    if (Test-Path $reportPath) {
        try {
            $r = Get-Content $reportPath -Raw | ConvertFrom-Json
            $row.env_ms  = [int]$r.ms_environment_created
            $row.ctrl_ms = [int]$r.ms_controller_created
            $row.nav_ms  = [int]$r.ms_navigation_completed
            $row.nav_ok  = ([int]$r.ms_navigation_completed -gt 0) -and (-not $r.navigation_failed)
            $row.seen    = [bool]$r.nav_starting_seen
            $row.title   = $r.document_title
            if ($r.error) { $row.error = $r.error }
            if ($r.nav_error_status -ne 0) { $row.nav_err = $r.nav_error_status }
            if ($r.process_failed_kind) { $row.proc_failed = $r.process_failed_kind }
            $script:lastDescendants = @($r.descendant_pids)
        } catch {
            $row.error = "report parse failed: $($_.Exception.Message)"
            $script:lastDescendants = @()
        }
    } else {
        $row.error = "no report file"
        $script:lastDescendants = @()
    }

    # Precise orphan check: only the PIDs this probe recorded as its own descendants.
    Start-Sleep -Milliseconds $SettleMs
    $desc = @($script:lastDescendants)
    $alive = @(Test-PidsAlive -Pids $desc)
    $row.descendants = $desc.Count
    $row.orphans = $alive.Count
    $row.orphan_pids = ($alive -join ',')

    Stop-Pids -Pids $desc
    Start-Sleep -Milliseconds 1500
    $stillAlive = @(Test-PidsAlive -Pids $desc)
    $row.orphans_after_kill = $stillAlive.Count

    [void]$rows.Add([pscustomobject]$row)
    Write-Host "  nav_ok=$($row.nav_ok)  ctrl_ms=$($row.ctrl_ms)  nav_ms=$($row.nav_ms)  seen=$($row.seen)  descendants=$($row.descendants)  orphans=$($row.orphans)"
    if ($row.error) { Write-Host "  error: $($row.error)" }
    Write-Host ""
}

# Run the three levels. Options are identical except for the navigation mode, so a
# difference in outcome can only come from the navigation path itself.
Invoke-Mode -Mode "about"  -Label "1/3 about:blank (event plumbing only)" -TimeoutMs $AboutTimeoutMs  -ExtraArgs @("--mode", "clean")
Invoke-Mode -Mode "string" -Label "2/3 NavigateToString (adds the renderer)" -TimeoutMs $StringTimeoutMs -ExtraArgs @("--mode", "clean")
Invoke-Mode -Mode "url"    -Label "3/3 file:// URI (adds the filesystem path)" -TimeoutMs $UrlTimeoutMs -ExtraArgs @("--mode", "clean")

Write-Host "================ DIAGNOSIS ================"
$rows | Format-Table -AutoSize

$about  = $rows | Where-Object { $_.mode -eq "about" }
$string = $rows | Where-Object { $_.mode -eq "string" }
$url    = $rows | Where-Object { $_.mode -eq "url" }

Write-Host ""
if (-not $about.nav_ok) {
    Write-Host "VERDICT: even about:blank never completes." -ForegroundColor Red
    Write-Host "  The renderer/event pipeline itself is not working here, independent of our page."
    Write-Host "  Check 'error', 'proc_failed' and the WebView2 Runtime installation."
} elseif (-not $string.nav_ok) {
    Write-Host "VERDICT: about:blank works, NavigateToString does not." -ForegroundColor Yellow
    Write-Host "  Event plumbing is fine; rendering real HTML is not. Look at 'proc_failed' / 'nav_err'."
} elseif (-not $url.nav_ok) {
    Write-Host "VERDICT: about:blank and NavigateToString work; the file:// URI does not." -ForegroundColor Yellow
    Write-Host "  The break is in serving the page from disk. Workarounds: read the file in C++"
    Write-Host "  and use NavigateToString (plus a <base> for relative resources), or serve over a"
    Write-Host "  local custom scheme / loopback. This is a real architecture input."
} else {
    Write-Host "VERDICT: all three paths work. Re-run the full suite: .\run-probe.ps1" -ForegroundColor Green
}

Write-Host ""
Write-Host "JSON reports: $outDir"
$leftover = 0
foreach ($r in $rows) { $leftover += [int]$r.orphans_after_kill }
Write-Host "Probe-recorded descendant processes still alive after cleanup: $leftover"
