@echo off
:: ===========================================
:: RocketMax Plugin - Lanceur d'installation
:: ===========================================
:: Double-cliquez sur ce fichier pour installer
:: ou mettre a jour le plugin RocketMax
:: ===========================================

title RocketMax - Installation

:: Verifier si le script PowerShell local existe
if exist "%~dp0install-rocketmax.ps1" (
    echo Lancement de l'installation locale...
    powershell -ExecutionPolicy Bypass -File "%~dp0install-rocketmax.ps1"
    goto :end
)

:: Script local non trouve, telecharger depuis GitHub
echo.
echo Script local non trouve, telechargement depuis GitHub...
echo.
powershell -ExecutionPolicy Bypass -Command "& { try { iwr -useb https://raw.githubusercontent.com/tellebma/RocketMax-Plugin/main/install-rocketmax.ps1 | iex } catch { Write-Host 'Erreur: ' $_.Exception.Message -ForegroundColor Red; Read-Host 'Appuyez sur Entree pour quitter'; exit 1 } }"

:end
