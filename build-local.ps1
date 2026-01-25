# Script de build pour environnement local
# Usage: .\build-local.ps1

$ErrorActionPreference = "Stop"

# Chemins
$PluginDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$HeaderFile = Join-Path $PluginDir "RocketMax\RocketMax.h"
$SolutionFile = Join-Path $PluginDir "RocketMax.sln"
$OutputDll = Join-Path $PluginDir "plugins\RocketMax.dll"
$BakkesModPlugins = "$env:APPDATA\bakkesmod\bakkesmod\plugins"
$BakkesModCfg = "$env:APPDATA\bakkesmod\bakkesmod\cfg\plugins.cfg"

Write-Host "=== RocketMax Local Build ===" -ForegroundColor Cyan

# 1. Modifier l'endpoint vers localhost
Write-Host "`n[1/4] Configuration de l'endpoint local..." -ForegroundColor Yellow
$headerContent = Get-Content $HeaderFile -Raw
if ($headerContent -match 'API_ENDPOINT "https://rocketmax.tellebma.fr"') {
    $headerContent = $headerContent -replace '#define API_ENDPOINT "https://rocketmax.tellebma.fr"', '#define API_ENDPOINT "http://localhost:8080"'
    Set-Content $HeaderFile $headerContent -NoNewline
    Write-Host "  Endpoint change vers http://localhost:8080" -ForegroundColor Green
} elseif ($headerContent -match 'API_ENDPOINT "http://localhost:8080"') {
    Write-Host "  Endpoint deja configure en local" -ForegroundColor Green
} else {
    Write-Host "  ATTENTION: Endpoint non trouve dans le header" -ForegroundColor Red
}

# 2. Trouver MSBuild
Write-Host "`n[2/4] Recherche de MSBuild..." -ForegroundColor Yellow

$msbuild = $null

# Methode 1: Utiliser vswhere (recommande)
$vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswherePath) {
    $msbuild = & $vswherePath -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
}

# Methode 2: Chemins connus
if (-not $msbuild) {
    $msbuildPaths = @(
        # VS 2022
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        # VS 2019
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($path in $msbuildPaths) {
        if (Test-Path $path) {
            $msbuild = $path
            break
        }
    }
}

if (-not $msbuild) {
    Write-Host "  ERREUR: MSBuild non trouve." -ForegroundColor Red
    Write-Host "  Solutions possibles:" -ForegroundColor Yellow
    Write-Host "    1. Installez Visual Studio 2022 avec 'Developpement Desktop en C++'" -ForegroundColor White
    Write-Host "    2. Ou installez 'Build Tools for Visual Studio 2022'" -ForegroundColor White
    Write-Host "       https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022" -ForegroundColor Gray
    Write-Host "    3. Puis redemarrez votre terminal" -ForegroundColor White
    exit 1
}
Write-Host "  MSBuild trouve: $msbuild" -ForegroundColor Green

# 3. Compiler le plugin
Write-Host "`n[3/4] Compilation du plugin..." -ForegroundColor Yellow
& $msbuild $SolutionFile /p:Configuration=Release /p:Platform=x64 /verbosity:minimal

if ($LASTEXITCODE -ne 0) {
    Write-Host "  ERREUR: La compilation a echoue" -ForegroundColor Red
    exit 1
}
Write-Host "  Compilation reussie" -ForegroundColor Green

# 4. Copier le DLL
Write-Host "`n[4/4] Installation du plugin..." -ForegroundColor Yellow
if (Test-Path $OutputDll) {
    if (-not (Test-Path $BakkesModPlugins)) {
        New-Item -ItemType Directory -Path $BakkesModPlugins -Force | Out-Null
    }
    Copy-Item $OutputDll $BakkesModPlugins -Force
    Write-Host "  DLL copie vers $BakkesModPlugins" -ForegroundColor Green

    # Ajouter au plugins.cfg si pas deja present
    if (Test-Path $BakkesModCfg) {
        $cfgContent = Get-Content $BakkesModCfg -Raw
        if ($cfgContent -notmatch "plugin load rocketmax") {
            Add-Content $BakkesModCfg "`nplugin load rocketmax"
            Write-Host "  Plugin ajoute a plugins.cfg" -ForegroundColor Green
        }
    }
} else {
    Write-Host "  ERREUR: DLL non trouve apres compilation" -ForegroundColor Red
    exit 1
}

Write-Host "`n=== Build termine ===" -ForegroundColor Cyan
Write-Host "N'oubliez pas de lancer le backend Flask:" -ForegroundColor White
Write-Host "  cd RocketMax-FrontEnd && python app.py" -ForegroundColor Gray
Write-Host "Puis redemarrez Rocket League ou rechargez le plugin dans BakkesMod (F6)" -ForegroundColor White
