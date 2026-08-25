# Run 3DS Studio (Tauri) without devkitPro MSYS2 breaking the Rust linker.
# Requires: Visual Studio 2022 Build Tools with "Desktop development with C++"

param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$TauriArgs = @("dev")
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent
Set-Location $ProjectRoot

function Remove-GnuLinkFromPath {
    $clean = @()
    foreach ($p in ($env:PATH -split ';')) {
        if (-not $p) { continue }
        if ($p -match '(?i)(\\msys2\\usr\\bin|\\Git\\usr\\bin)$') { continue }
        $clean += $p
    }
    $env:PATH = ($clean -join ';')
}

function Test-MsvcLink {
    $link = Get-Command link.exe -ErrorAction SilentlyContinue
    if (-not $link) { return $false }
    # GNU link from MSYS prints "link: extra operand" for MSVC flags; MSVC link accepts /NOLOGO
    $help = & $link.Source /? 2>&1 | Out-String
    return $help -match "Microsoft" -or $help -match "Linker Version"
}

function Invoke-WithVcVars {
    param([string]$VcVarsBat, [string]$Command, [string]$ExtraEnv = "")
    $cmd = if ($ExtraEnv) { "$ExtraEnv && " } else { "" }
    $cmd += "call `"$VcVarsBat`" >nul && $Command"
    cmd /c $cmd
    return $LASTEXITCODE
}

# OneDrive sync often makes src-tauri/target non-writable (autocfg: "output path is not a writable directory")
function Set-LocalCargoTargetDir {
    $dir = Join-Path $env:LOCALAPPDATA "3ds-studio-cargo-target"
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $env:CARGO_TARGET_DIR = $dir
    Write-Host "Cargo target dir (outside OneDrive): $dir" -ForegroundColor DarkGray
    return "set `"CARGO_TARGET_DIR=$dir`""
}

Remove-GnuLinkFromPath
$cargoEnv = Set-LocalCargoTargetDir

$cargoBin = Join-Path $env:USERPROFILE ".cargo\bin"
if (Test-Path $cargoBin) {
    $env:PATH = "$cargoBin;$env:PATH"
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vcvars = $null

if (Test-Path $vswhere) {
    $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($install) {
        $candidate = Join-Path $install "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $candidate) { $vcvars = $candidate }
    }
}

$tauriCmd = "npx tauri " + ($TauriArgs -join " ")

if ($vcvars) {
    Write-Host "Using Visual Studio toolchain: $vcvars" -ForegroundColor Cyan
    $code = Invoke-WithVcVars -VcVarsBat $vcvars -Command $tauriCmd -ExtraEnv $cargoEnv
    exit $code
}

if (-not (Test-MsvcLink)) {
    Write-Host ""
    Write-Host "ERROR: Rust cannot link Tauri on this PC yet." -ForegroundColor Red
    Write-Host ""
    Write-Host "Cause 1: devkitPro MSYS2 puts GNU link.exe on PATH (conflicts with Rust)." -ForegroundColor Yellow
    Write-Host "        This script removed msys2 from PATH for this session." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Cause 2: Microsoft C++ linker (link.exe) + Windows SDK libs are not installed." -ForegroundColor Yellow
    Write-Host "        devkitPro builds 3DS games; building the editor needs MSVC separately." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Fix: Install Visual Studio 2022 Build Tools" -ForegroundColor Green
    Write-Host "  https://visualstudio.microsoft.com/visual-cpp-build-tools/" -ForegroundColor Green
    Write-Host "  Workload: Desktop development with C++" -ForegroundColor Green
    Write-Host ""
    Write-Host "Then run again: npm run tauri dev" -ForegroundColor Green
    Write-Host ""
    exit 1
}

Write-Host "Building (MSVC link found on PATH)..." -ForegroundColor Cyan
Invoke-Expression $tauriCmd
exit $LASTEXITCODE
