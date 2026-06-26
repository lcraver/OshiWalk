param(
    [string]$Notes = ""
)

$pio      = "C:\Users\Pidge\.platformio\penv\Scripts\platformio.exe"
$root     = $PSScriptRoot
$verFile  = "$root\src\version.h"
$binFile  = "$root\.pio\build\device1\firmware.bin"
$jsonFile = "$root\version.json"

# ── Build ──────────────────────────────────────────────────────────────────────
Write-Host "Building firmware..." -ForegroundColor Cyan
& $pio run -e device1
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed." -ForegroundColor Red; exit 1 }

# ── Read version bumped by increment_version.py ────────────────────────────────
$verContent = Get-Content $verFile -Raw
if ($verContent -match 'BUILD_NUMBER (\d+)') {
    $build   = $Matches[1]
    $tag     = "v1.$build"
} else {
    Write-Host "Could not read BUILD_NUMBER from version.h" -ForegroundColor Red; exit 1
}
Write-Host "Version: $tag" -ForegroundColor Green

# ── Update version.json ────────────────────────────────────────────────────────
$json = [ordered]@{
    build = [int]$build
    url   = "https://github.com/lcraver/OshiWalk/releases/latest/download/firmware.bin"
}
$json | ConvertTo-Json | Set-Content $jsonFile -Encoding utf8
Write-Host "Updated version.json" -ForegroundColor Cyan

# ── Commit & tag ───────────────────────────────────────────────────────────────
git -C $root add src/version.h version.json
git -C $root commit -m "Release $tag"
if ($LASTEXITCODE -ne 0) { Write-Host "Nothing to commit or commit failed." -ForegroundColor Yellow }

git -C $root tag $tag
git -C $root push origin main --tags
if ($LASTEXITCODE -ne 0) { Write-Host "Push failed." -ForegroundColor Red; exit 1 }

# ── Create GitHub release & upload firmware ────────────────────────────────────
Write-Host "Creating GitHub release $tag..." -ForegroundColor Cyan

$ghArgs = @("release", "create", $tag, $binFile,
            "--title", $tag,
            "--asset", "firmware.bin")
if ($Notes -ne "") { $ghArgs += @("--notes", $Notes) }
else               { $ghArgs += "--generate-notes" }

& gh @ghArgs
if ($LASTEXITCODE -ne 0) { Write-Host "gh release create failed." -ForegroundColor Red; exit 1 }

Write-Host "`nRelease $tag published successfully." -ForegroundColor Green
