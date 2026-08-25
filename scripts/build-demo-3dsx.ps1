# Build demo .3dsx (and optional .cia) from a Studio project and copy into the release folder.

param(
    [string]$Version = "0.1.0",
    [string]$OutDir = "",
    [string]$ProjectPath = "",
    [switch]$AlsoCia
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
    Write-Host "Demo project not found at $ProjectPath - skipping .3dsx (create Example via Studio first)." -ForegroundColor Yellow
    return
}

function Resolve-DevkitPro {
    $cfgPath = Join-Path $env:APPDATA "3ds-studio\config.json"
    if (Test-Path $cfgPath) {
        try {
            $cfg = Get-Content $cfgPath -Raw | ConvertFrom-Json
            if ($cfg.devkitpro_path -and (Test-Path $cfg.devkitpro_path)) {
                return [string]$cfg.devkitpro_path
            }
        } catch { }
    }
    if ($env:DEVKITPRO -and (Test-Path $env:DEVKITPRO) -and ($env:DEVKITPRO -notmatch '^/opt/')) {
        return $env:DEVKITPRO
    }
    if (Test-Path "C:\devkitPro") { return "C:\devkitPro" }
    return $null
}

$dk = Resolve-DevkitPro
if (-not $dk) {
    Write-Host "devkitPro not found - skipping demo .3dsx build." -ForegroundColor Yellow
    return
}

$bash = Join-Path $dk "msys2\usr\bin\bash.exe"
if (-not (Test-Path $bash)) {
    Write-Host "MSYS2 bash not found under $dk - skipping demo .3dsx." -ForegroundColor Yellow
    return
}

function To-MsysPath([string]$winPath) {
    $full = (Resolve-Path $winPath).Path
    if ($full -match '^([A-Za-z]):\\(.*)$') {
        $drive = $Matches[1].ToLowerInvariant()
        $rest = ($Matches[2] -replace '\\', '/')
        return "/$drive/$rest"
    }
    return $full -replace '\\', '/'
}

$msysProj = To-MsysPath $ProjectPath
$msysDk = To-MsysPath $dk

Write-Host "Building demo .3dsx in $ProjectPath ..." -ForegroundColor Cyan
# Pass env + make via bash -lc; keep as one argument so PowerShell does not split on &&
$bashCmd = 'export DEVKITPRO="{0}"; export DEVKITARM="$DEVKITPRO/devkitARM"; export PATH="$DEVKITPRO/tools/bin:$DEVKITPRO/devkitARM/bin:$PATH"; cd "{1}"; make -j4' -f $msysDk, $msysProj
& $bash -lc $bashCmd
if ($LASTEXITCODE -ne 0) {
    Write-Host "make failed (exit $LASTEXITCODE) - skipping copy." -ForegroundColor Yellow
    return
}

$threeDsx = @(Get-ChildItem -Path $ProjectPath -Filter "*.3dsx" -File -ErrorAction SilentlyContinue)
if (-not $threeDsx.Count) {
    Write-Host "No .3dsx produced in project root - skipping." -ForegroundColor Yellow
    return
}

$destName = "3DSStudio-ExamplePlatformer-v$Version.3dsx"
Copy-Item -Force $threeDsx[0].FullName (Join-Path $OutDir $destName)
Write-Host "Copied $destName" -ForegroundColor Green

if ($AlsoCia) {
    $ciaCmd = 'export DEVKITPRO="{0}"; export DEVKITARM="$DEVKITPRO/devkitARM"; export PATH="$DEVKITPRO/tools/bin:$DEVKITPRO/devkitARM/bin:$PATH"; cd "{1}"; make -j4 cia' -f $msysDk, $msysProj
    & $bash -lc $ciaCmd
    $cia = @(Get-ChildItem -Path $ProjectPath -Filter "*.cia" -File -ErrorAction SilentlyContinue)
    if ($cia.Count) {
        $ciaName = "3DSStudio-ExamplePlatformer-v$Version.cia"
        Copy-Item -Force $cia[0].FullName (Join-Path $OutDir $ciaName)
        Write-Host "Copied $ciaName (unique title ID - do not redistribute casually)" -ForegroundColor Yellow
    }
}
