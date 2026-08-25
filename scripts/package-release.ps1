# Copy NSIS installer + legal files into dist/release/<version> and write SHA256SUMS.txt.
# Run after: npm run tauri:build

param(
    [string]$Version = "0.1.0",
    [switch]$SkipDemo,
    [switch]$SkipSampleZip
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent
Set-Location $ProjectRoot

$outDir = Join-Path $ProjectRoot "dist\release\v$Version"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$cargoTarget = Join-Path $env:LOCALAPPDATA "3ds-studio-cargo-target"
$nsisDir = Join-Path $cargoTarget "release\bundle\nsis"
if (-not (Test-Path $nsisDir)) {
    $fallback = Join-Path $ProjectRoot "src-tauri\target\release\bundle\nsis"
    if (Test-Path $fallback) { $nsisDir = $fallback }
}

if (-not (Test-Path $nsisDir)) {
    throw "NSIS bundle not found under LOCALAPPDATA\3ds-studio-cargo-target\release\bundle\nsis. Run npm run tauri:build first."
}

$installers = @(Get-ChildItem -Path $nsisDir -Filter "*.exe" -File -ErrorAction SilentlyContinue)
if (-not $installers.Count) {
    throw "No .exe installer in $nsisDir"
}

foreach ($f in $installers) {
    Copy-Item -Force $f.FullName (Join-Path $outDir $f.Name)
    Write-Host "Copied installer: $($f.Name)" -ForegroundColor Cyan
}

Copy-Item -Force (Join-Path $ProjectRoot "LICENSE") (Join-Path $outDir "LICENSE")
Copy-Item -Force (Join-Path $ProjectRoot "THIRD_PARTY.md") (Join-Path $outDir "THIRD_PARTY.md")
$notes = Join-Path $ProjectRoot "docs\release\RELEASE_NOTES_v$Version.md"
if (Test-Path $notes) {
    Copy-Item -Force $notes (Join-Path $outDir "RELEASE_NOTES.md")
}

if (-not $SkipDemo) {
    & (Join-Path $PSScriptRoot "build-demo-3dsx.ps1") -Version $Version -OutDir $outDir
}

if (-not $SkipSampleZip) {
    & (Join-Path $PSScriptRoot "zip-sample-project.ps1") -Version $Version -OutDir $outDir
}

# SHA256 for all files in outDir except the sums file itself
$sumsPath = Join-Path $outDir "SHA256SUMS.txt"
$lines = @()
Get-ChildItem -Path $outDir -File |
    Where-Object { $_.Name -ne "SHA256SUMS.txt" } |
    Sort-Object Name |
    ForEach-Object {
        $hash = (Get-FileHash -Algorithm SHA256 -Path $_.FullName).Hash.ToLowerInvariant()
        $lines += "$hash  $($_.Name)"
        Write-Host "SHA256 $($_.Name)" -ForegroundColor DarkGray
    }
$utf8 = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllLines($sumsPath, $lines, $utf8)
Write-Host "Wrote $sumsPath" -ForegroundColor Green
Write-Host "Release folder: $outDir" -ForegroundColor Green
