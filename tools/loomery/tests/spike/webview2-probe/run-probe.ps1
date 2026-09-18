# run-probe.ps1 -- orchestrate the WebView2 host-cost probe.
#
# ASCII-only on purpose (see prepare-sdk.ps1 for why).
#
# What it answers (see README.md for how to read the numbers):
#   1. cold/warm startup, broken into stages
#   2. resident memory: host process + the whole msedgewebview2.exe descendant tree
#   3. ORPHANS: whether msedgewebview2.exe survives, in three shutdown scenarios
#
# Orphan detection is a PID diff against a baseline snapshot, because this machine
# already has unrelated msedgewebview2.exe processes running -- a plain process
# count would be meaningless.

[CmdletBinding()]
param(
    [string]$Exe = "",
    [string]$Config = "Release",
    [int]$KillAfterReadyMs = 2000,
    [int]$SettleMs = 4000,
    [int]$DiagramCount = 3,
    [int]$ProbeTimeoutMs = 30000,
    [switch]$NoShow,
    [switch]$SkipJobObjectScenarios
)

$ErrorActionPreference = "Stop"

# --- locate the probe ----------------------------------------------------------
if (-not $Exe) {
    $candidates = @(
        # CMakeLists.txt sets RUNTIME_OUTPUT_DIRECTORY_<CONFIG> to <build>/bin, so a
        # multi-config generator does NOT insert a per-config subdirectory here.
        (Join-Path $PSScriptRoot "build\bin\webview2_probe.exe"),
        (Join-Path $PSScriptRoot "build\bin\$Config\webview2_probe.exe"),
        (Join-Path $PSScriptRoot "build\bin\Release\webview2_probe.exe"),
        (Join-Path $PSScriptRoot "build\bin\Debug\webview2_probe.exe")
    )
    $Exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Exe -or -not (Test-Path $Exe)) {
    throw "probe executable not found. Build it first:`n  cmake -S . -B build -G `"Visual Studio 18 2026`" -A x64`n  cmake --build build --config $Config"
}
$Exe = (Resolve-Path $Exe).Path
Write-Host "[probe] executable: $Exe"
Write-Host "[probe] host msedgewebview2.exe runtime is shared with Edge; version follows the installed runtime."

$outDir = Join-Path $PSScriptRoot "out"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

function Get-ChromiumPids {
    @(Get-Process msedgewebview2 -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty Id)
}

$script:baseline = Get-ChromiumPids
Write-Host "[probe] baseline msedgewebview2.exe processes: $($script:baseline.Count)"
Write-Host ""

$results = New-Object System.Collections.ArrayList

function Invoke-Scenario {
    param(
        [string]$Name,
        [string[]]$ExtraArgs,
        [switch]$ExternalKill
    )

    Write-Host "=== $Name ==="
    $reportPath = Join-Path $outDir "$Name.json"
    Remove-Item $reportPath -Force -ErrorAction SilentlyContinue

    $args = @()
    $args += $ExtraArgs
    $args += @("--report", $reportPath)
    $args += @("--page", (Join-Path $PSScriptRoot "page\probe.html") + "?blocks=2000&diagrams=$DiagramCount")
    $args += @("--timeout-ms", "$ProbeTimeoutMs")
    # A hidden WebView2 may skip rendering, which would UNDERSTATE the memory figure.
    # Default to a visible window so the numbers reflect a real session.
    if (-not $NoShow) { $args += "--show" }

    $before = Get-ChromiumPids
    $exitCode = "n/a"

    if ($ExternalKill) {
        $proc = Start-Process -FilePath $Exe -ArgumentList $args -PassThru -WindowStyle Hidden

        # Wait until new Chromium children appear = environment was created.
        $deadline = (Get-Date).AddSeconds(30)
        $sawChildren = $false
        while ((Get-Date) -lt $deadline) {
            $new = @(Get-ChromiumPids | Where-Object { $_ -notin $before })
            if ($new.Count -gt 0) { $sawChildren = $true; break }
            if ($proc.HasExited) { break }
            Start-Sleep -Milliseconds 200
        }
        if (-not $sawChildren) {
            Write-Host "  warning: never observed Chromium children appearing"
        }
        Start-Sleep -Milliseconds $KillAfterReadyMs

        if (-not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        }
        $exitCode = "killed"
        Write-Host "  killed host pid $($proc.Id) externally (simulates Steward terminate+kill)"
    } else {
        # Do NOT use Start-Process -Wait here: it waits for the whole process TREE,
        # and WebView2's Chromium children are exactly what may outlive the host.
        # Waiting on the tree therefore hangs forever precisely when the probe is
        # doing its job. Poll the host process itself instead.
        $proc = Start-Process -FilePath $Exe -ArgumentList $args -PassThru -WindowStyle Hidden
        $deadline = (Get-Date).AddMilliseconds($ProbeTimeoutMs + 20000)
        while (-not $proc.HasExited -and (Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 200
        }
        if (-not $proc.HasExited) {
            Write-Host "  host did not exit within the grace period; killing it"
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            $exitCode = "probe-timeout"
        } else {
            $exitCode = $proc.ExitCode
        }
        Write-Host "  host exited with code $exitCode"
    }

    Start-Sleep -Milliseconds $SettleMs
    $after = @(Get-ChromiumPids | Where-Object { $_ -notin $before })

    # Do they linger? Sample again after a further delay.
    Start-Sleep -Seconds 3
    $still = @(Get-ChromiumPids | Where-Object { $_ -notin $before })

    $row = [ordered]@{
        scenario        = $Name
        exit            = $exitCode
        orphans_now     = $after.Count
        orphans_after3s = $still.Count
    }

    if (Test-Path $reportPath) {
        try {
            $r = Get-Content $reportPath -Raw | ConvertFrom-Json
            $row.env_ms      = [int]$r.ms_environment_created
            $row.ctrl_ms     = [int]$r.ms_controller_created
            $row.nav_ms      = [int]$r.ms_navigation_completed
            $row.self_mb     = [int]$r.self_ws_mb_at_navigation
            $row.children_mb = [int]$r.children_ws_mb_at_navigation
            $row.total_mb    = [int]$r.self_ws_mb_at_navigation + [int]$r.children_ws_mb_at_navigation
            $row.child_procs = [int]$r.children_count_at_navigation
            if ($r.error) { $row.error = $r.error }
        } catch {
            $row.error = "report parse failed: $($_.Exception.Message)"
        }
    } else {
        $row.error = "no report file (expected for externally killed runs)"
    }

    # A run that never reached NavigationCompleted never exercised the real render
    # pipeline, so its orphan result is meaningless -- e.g. a controller that failed
    # to be created leaves Chromium children behind for reasons that have nothing to
    # do with the shutdown path under test. Such a run must not drive the verdict.
    $reachedNavigation = ($null -ne $row.nav_ms) -and ([int]$row.nav_ms -gt 0)
    $row["valid"] = $reachedNavigation

    [void]$results.Add([pscustomobject]$row)
    Write-Host ""
}

# --- scenarios -----------------------------------------------------------------
Invoke-Scenario -Name "clean-cold"         -ExtraArgs @("--mode", "clean", "--fresh-profile")
Invoke-Scenario -Name "clean-warm"         -ExtraArgs @("--mode", "clean")
Invoke-Scenario -Name "hard-selfkill"      -ExtraArgs @("--mode", "hard")
Invoke-Scenario -Name "kill-external"      -ExtraArgs @("--mode", "clean", "--hold-ms", "60000") -ExternalKill

if (-not $SkipJobObjectScenarios) {
    Invoke-Scenario -Name "clean-jobobject"    -ExtraArgs @("--mode", "clean", "--fresh-profile", "--job-object")
    Invoke-Scenario -Name "kill-external-job"  -ExtraArgs @("--mode", "clean", "--job-object", "--hold-ms", "60000") -ExternalKill
}

# --- report --------------------------------------------------------------------
Write-Host ""
Write-Host "================ SUMMARY ================"
$results | Format-Table -AutoSize

$invalid = @($results | Where-Object { -not $_.valid })
if ($invalid.Count -gt 0) {
    Write-Host "NOTE: $($invalid.Count) scenario(s) never reached NavigationCompleted, so they are" -ForegroundColor Yellow
    Write-Host "      EXCLUDED from the orphan verdict below (their numbers are not meaningful):" -ForegroundColor Yellow
    foreach ($r in $invalid) {
        $why = if ($null -ne $r.error -and $r.error) { $r.error } else { "no report / killed before navigation" }
        Write-Host "        - $($r.scenario): $why"
    }
    Write-Host ""
}

$validResults = @($results | Where-Object { $_.valid })
$orphanRisk = @($validResults | Where-Object { $_.orphans_after3s -gt 0 })

if ($validResults.Count -eq 0) {
    Write-Host "INCONCLUSIVE: no scenario reached NavigationCompleted." -ForegroundColor Yellow
    Write-Host "  Check the 'error' column in out/*.json, and make sure the WebView2 Runtime is" -ForegroundColor Yellow
    Write-Host "  usable in this session (Edge/WebView2 must be able to start)." -ForegroundColor Yellow
} elseif ($orphanRisk.Count -eq 0) {
    Write-Host "ORPHANS: none in any valid scenario. No Job Object mitigation appears necessary." -ForegroundColor Green
} else {
    Write-Host "ORPHANS DETECTED in $($orphanRisk.Count) valid scenario(s):" -ForegroundColor Red
    foreach ($r in $orphanRisk) {
        Write-Host "  - $($r.scenario): $($r.orphans_after3s) leftover msedgewebview2.exe"
    }
    Write-Host "  Compare clean vs clean-jobobject, and kill-external vs kill-external-job:"
    Write-Host "  if the --job-object variants are clean, the mitigation works and must be adopted."
}

Write-Host ""
Write-Host "JSON reports: $outDir"

# --- cleanup -------------------------------------------------------------------
$leftovers = @(Get-ChromiumPids | Where-Object { $script:baseline -notcontains $_ })
if ($leftovers.Count -gt 0) {
    Write-Host ""
    Write-Host "Cleaning up $($leftovers.Count) probe-generated msedgewebview2.exe process(es)..."
    # NOTE: $pid is a read-only automatic variable in PowerShell; do not use it here.
    foreach ($procId in $leftovers) {
        try { Stop-Process -Id $procId -Force -ErrorAction Stop } catch { }
    }
    Start-Sleep -Seconds 2
    $final = @(Get-ChromiumPids | Where-Object { $script:baseline -notcontains $_ })
    Write-Host "Remaining probe-generated processes after cleanup: $($final.Count)"
} else {
    Write-Host "Nothing to clean up."
}
