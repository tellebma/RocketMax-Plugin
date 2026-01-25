# ===========================================
# RocketMax Plugin - Script d'installation
# ===========================================
# Ce script telecharge et installe la derniere
# version du plugin RocketMax pour BakkesMod
# ===========================================

$ErrorActionPreference = "Stop"

# Configuration
$repoOwner = "tellebma"
$repoName = "RocketMax-Plugin"
$pluginName = "RocketMax"
$dllName = "RocketMax.dll"

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   RocketMax Plugin - Installation" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 1. Recuperer la derniere release depuis GitHub
Write-Host "[1/4] Recherche de la derniere version..." -ForegroundColor Yellow
try {
    $releaseUrl = "https://api.github.com/repos/$repoOwner/$repoName/releases/latest"
    $release = Invoke-RestMethod -Uri $releaseUrl -Headers @{ "User-Agent" = "PowerShell" }
    $latestVersion = $release.tag_name
    $downloadUrl = $release.assets | Where-Object { $_.name -eq $dllName } | Select-Object -ExpandProperty browser_download_url

    if (-not $downloadUrl) {
        Write-Host "  ERREUR: Fichier $dllName non trouve dans la release" -ForegroundColor Red
        Write-Host "  Assets disponibles:" -ForegroundColor Red
        $release.assets | ForEach-Object { Write-Host "    - $($_.name)" -ForegroundColor Gray }
        Read-Host "Appuyez sur Entree pour quitter"
        exit 1
    }

    Write-Host "  Version disponible: $latestVersion" -ForegroundColor Green
    Write-Host "  URL: $downloadUrl" -ForegroundColor Gray
}
catch {
    Write-Host "  ERREUR: Impossible de contacter GitHub" -ForegroundColor Red
    Write-Host "  $($_.Exception.Message)" -ForegroundColor Gray
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

# 2. Trouver le dossier BakkesMod
Write-Host ""
Write-Host "[2/4] Detection du dossier BakkesMod..." -ForegroundColor Yellow
$defaultBakkesmodFolder = "$env:APPDATA\bakkesmod\bakkesmod"

if (Test-Path -Path $defaultBakkesmodFolder) {
    $bakkesmodFolder = $defaultBakkesmodFolder
    Write-Host "  Dossier trouve: $bakkesmodFolder" -ForegroundColor Green
}
else {
    Write-Host "  Dossier par defaut non trouve: $defaultBakkesmodFolder" -ForegroundColor Yellow

    # Boucle jusqu'a obtenir un chemin valide
    while ($true) {
        $bakkesmodFolder = Read-Host "  Veuillez saisir le chemin du dossier BakkesMod"

        if ([string]::IsNullOrWhiteSpace($bakkesmodFolder)) {
            Write-Host "  Installation annulee." -ForegroundColor Red
            exit 1
        }

        if (Test-Path -Path $bakkesmodFolder) {
            Write-Host "  Dossier valide: $bakkesmodFolder" -ForegroundColor Green
            break
        }
        else {
            Write-Host "  ERREUR: Chemin introuvable. Reessayez ou appuyez sur Entree pour annuler." -ForegroundColor Red
        }
    }
}

$pluginsFolder = Join-Path $bakkesmodFolder "plugins"
$pluginPath = Join-Path $pluginsFolder $dllName
$pluginCfg = Join-Path $bakkesmodFolder "cfg\plugins.cfg"

# Creer le dossier plugins si necessaire
if (-not (Test-Path -Path $pluginsFolder)) {
    New-Item -ItemType Directory -Path $pluginsFolder -Force | Out-Null
    Write-Host "  Dossier plugins cree" -ForegroundColor Gray
}

# 3. Verifier si une version est deja installee
Write-Host ""
Write-Host "[3/4] Verification de l'installation existante..." -ForegroundColor Yellow

$isUpdate = $false
if (Test-Path $pluginPath) {
    $isUpdate = $true
    $existingSize = (Get-Item $pluginPath).Length
    Write-Host "  Plugin existant detecte (taille: $existingSize octets)" -ForegroundColor Yellow
    Write-Host "  -> Mise a jour vers $latestVersion" -ForegroundColor Cyan
}
else {
    Write-Host "  Aucune installation existante" -ForegroundColor Gray
    Write-Host "  -> Nouvelle installation $latestVersion" -ForegroundColor Cyan
}

# 4. Telecharger et installer
Write-Host ""
Write-Host "[4/4] Telechargement et installation..." -ForegroundColor Yellow

try {
    # Telecharger dans un fichier temporaire d'abord
    $tempFile = Join-Path $env:TEMP "$dllName.tmp"
    Write-Host "  Telechargement en cours..." -ForegroundColor Gray

    # Utiliser WebClient pour avoir une barre de progression
    $webClient = New-Object System.Net.WebClient
    $webClient.Headers.Add("User-Agent", "PowerShell")
    $webClient.DownloadFile($downloadUrl, $tempFile)

    if (Test-Path $tempFile) {
        $newSize = (Get-Item $tempFile).Length
        Write-Host "  Telecharge: $newSize octets" -ForegroundColor Gray

        # Deplacer vers le dossier plugins (ecrase si existant)
        Move-Item -Path $tempFile -Destination $pluginPath -Force
        Write-Host "  Plugin installe: $pluginPath" -ForegroundColor Green
    }
    else {
        throw "Le fichier telecharge est introuvable"
    }
}
catch {
    Write-Host "  ERREUR: Echec du telechargement" -ForegroundColor Red
    Write-Host "  $($_.Exception.Message)" -ForegroundColor Gray

    # Nettoyer le fichier temporaire si present
    if (Test-Path $tempFile) {
        Remove-Item $tempFile -Force
    }

    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

# 5. Configurer le chargement automatique
Write-Host ""
Write-Host "[5/5] Configuration du chargement automatique..." -ForegroundColor Yellow

$pluginLoadLine = "plugin load rocketmax"

if (Test-Path $pluginCfg) {
    $cfgContent = Get-Content $pluginCfg -Raw -ErrorAction SilentlyContinue

    if ($cfgContent -match "plugin load rocketmax") {
        Write-Host "  Chargement automatique deja configure" -ForegroundColor Green
    }
    else {
        Add-Content -Path $pluginCfg -Value "`n$pluginLoadLine"
        Write-Host "  Ligne ajoutee a plugins.cfg" -ForegroundColor Green
    }
}
else {
    # Creer le fichier cfg si necessaire
    $cfgFolder = Split-Path $pluginCfg -Parent
    if (-not (Test-Path $cfgFolder)) {
        New-Item -ItemType Directory -Path $cfgFolder -Force | Out-Null
    }
    Set-Content -Path $pluginCfg -Value $pluginLoadLine
    Write-Host "  Fichier plugins.cfg cree avec chargement automatique" -ForegroundColor Green
}

# Resume final
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
if ($isUpdate) {
    Write-Host "   Mise a jour terminee !" -ForegroundColor Green
}
else {
    Write-Host "   Installation terminee !" -ForegroundColor Green
}
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Version installee: $latestVersion" -ForegroundColor White
Write-Host "Emplacement: $pluginPath" -ForegroundColor Gray
Write-Host ""
Write-Host "Pour utiliser le plugin:" -ForegroundColor Yellow
Write-Host "  1. Lancez Rocket League" -ForegroundColor White
Write-Host "  2. Le plugin se charge automatiquement avec BakkesMod" -ForegroundColor White
Write-Host "  3. Jouez un match ranked pour enregistrer vos stats" -ForegroundColor White
Write-Host ""
Write-Host "Site web: https://rocketmax.tellebma.fr/" -ForegroundColor Cyan
Write-Host ""

Read-Host "Appuyez sur Entree pour fermer"
