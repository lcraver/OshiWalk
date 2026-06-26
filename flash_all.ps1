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

# Upload to both devices in parallel
Write-Host "`nUploading to COM16 and COM17 in parallel..." -ForegroundColor Cyan

$j1 = Start-Job -ScriptBlock {
    Set-Location $using:root
    & $using:pio run -e device1 --target upload 2>&1
    if ($using:IncludeFS) { & $using:pio run -e device1 --target uploadfs 2>&1 }
}
$j2 = Start-Job -ScriptBlock {
    Set-Location $using:root
    & $using:pio run -e device2 --target upload 2>&1
    if ($using:IncludeFS) { & $using:pio run -e device2 --target uploadfs 2>&1 }
}

Wait-Job $j1, $j2 | Out-Null

Write-Host "`n=== device1 (COM16) ===" -ForegroundColor Yellow
Receive-Job $j1

Write-Host "`n=== device2 (COM17) ===" -ForegroundColor Yellow
Receive-Job $j2

$failed = ($j1.State -eq 'Failed') -or ($j2.State -eq 'Failed')
Remove-Job $j1, $j2
if ($failed) { exit 1 }
