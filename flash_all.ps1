param(
    [switch]$IncludeFS
)

$pio  = "C:\Users\Pidge\.platformio\penv\Scripts\platformio.exe"
$root = $PSScriptRoot

# Build firmware once
Write-Host "Building firmware..." -ForegroundColor Cyan
& $pio run -e device1
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed." -ForegroundColor Red; exit 1 }

# Optionally build filesystem
if ($IncludeFS) {
    Write-Host "Building filesystem..." -ForegroundColor Cyan
    & $pio run -e device1 --target buildfs
    if ($LASTEXITCODE -ne 0) { Write-Host "Filesystem build failed." -ForegroundColor Red; exit 1 }
}

# Copy artifacts into device2's build dir so it skips recompilation
$src = "$root\.pio\build\device1"
$dst = "$root\.pio\build\device2"
if (-not (Test-Path $dst)) { New-Item -ItemType Directory -Path $dst | Out-Null }
$artifacts = @("firmware.bin", "firmware.elf", "partitions.bin")
if ($IncludeFS) { $artifacts += "littlefs.bin" }
foreach ($f in $artifacts) {
    if (Test-Path "$src\$f") { Copy-Item "$src\$f" $dst -Force }
}

# Check which devices are present
$availablePorts = [System.IO.Ports.SerialPort]::GetPortNames()
$device1Present = $availablePorts -contains "COM16"
$device2Present = $availablePorts -contains "COM17"

if (-not $device1Present) { Write-Host "COM16 not found — skipping device1." -ForegroundColor Yellow }
if (-not $device2Present) { Write-Host "COM17 not found — skipping device2." -ForegroundColor Yellow }

if (-not $device1Present -and -not $device2Present) {
    Write-Host "No devices found. Nothing to flash." -ForegroundColor Red
    exit 1
}

Write-Host "`nUploading to available devices in parallel..." -ForegroundColor Cyan

$jobs = @()

if ($device1Present) {
    $jobs += Start-Job -ScriptBlock {
        Set-Location $using:root
        & $using:pio run -e device1 --target upload 2>&1
        if ($using:IncludeFS) { & $using:pio run -e device1 --target uploadfs 2>&1 }
    }
} else {
    $jobs += $null
}

if ($device2Present) {
    $jobs += Start-Job -ScriptBlock {
        Set-Location $using:root
        & $using:pio run -e device2 --target upload 2>&1
        if ($using:IncludeFS) { & $using:pio run -e device2 --target uploadfs 2>&1 }
    }
} else {
    $jobs += $null
}

$runningJobs = $jobs | Where-Object { $_ -ne $null }
if ($runningJobs) { Wait-Job $runningJobs | Out-Null }

if ($device1Present) {
    Write-Host "`n=== device1 (COM16) ===" -ForegroundColor Yellow
    Receive-Job $jobs[0]
} else {
    Write-Host "`n=== device1 (COM16) — SKIPPED (not connected) ===" -ForegroundColor DarkYellow
}

if ($device2Present) {
    Write-Host "`n=== device2 (COM17) ===" -ForegroundColor Yellow
    Receive-Job $jobs[1]
} else {
    Write-Host "`n=== device2 (COM17) — SKIPPED (not connected) ===" -ForegroundColor DarkYellow
}

$failed = $false
foreach ($j in $runningJobs) {
    if ($j.State -eq 'Failed') { $failed = $true }
}
Remove-Job $runningJobs
if ($failed) { exit 1 }
