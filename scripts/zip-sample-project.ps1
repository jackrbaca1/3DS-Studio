# Zip a clean sample Studio project (no build/ intermediates) for "open in Studio".

param(
    [string]$Version = "0.1.0",
    [string]$OutDir = "",
    [string]$ProjectPath = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent

if (-not $OutDir) {
    $OutDir = Join-Path $ProjectRoot "dist\release\v$Version"
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not $ProjectPath) {
    $docs = [Environment]::GetFolderPath("MyDocuments")
    $ProjectPath = Join-Path $docs "3DSStudio\ExamplePlatformer"
}

if (-not (Test-Path (Join-Path $ProjectPath "Makefile"))) {
    Write-Host "Sample project not found at $ProjectPath - skipping zip." -ForegroundColor Yellow
    return
}

$staging = Join-Path $env:TEMP "3ds-studio-sample-$Version"
if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
New-Item -ItemType Directory -Force -Path $staging | Out-Null

$destProj = Join-Path $staging "ExamplePlatformer"
robocopy $ProjectPath $destProj /E /XD build .git /XF *.o *.d *.map *.elf *.smdh *.bnr *.icn /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
if ($LASTEXITCODE -ge 8) {
    throw "robocopy failed with exit $LASTEXITCODE"
}

$zipPath = Join-Path $OutDir "3DSStudio-ExamplePlatformer-v$Version-project.zip"
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }

Compress-Archive -Path $destProj -DestinationPath $zipPath -Force
Remove-Item -Recurse -Force $staging
Write-Host "Wrote $zipPath" -ForegroundColor Green
