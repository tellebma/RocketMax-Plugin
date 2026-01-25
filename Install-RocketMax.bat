@echo off
:: ===========================================
:: RocketMax Plugin - Lanceur d'installation
:: ===========================================
:: Double-cliquez sur ce fichier pour installer
:: ou mettre a jour le plugin RocketMax
:: ===========================================

title RocketMax - Installation

:: Lancer le script PowerShell avec les bonnes permissions
powershell -ExecutionPolicy Bypass -File "%~dp0install-rocketmax.ps1"

:: Si PowerShell n'est pas disponible localement, essayer depuis le web
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Tentative d'installation depuis le web...
    powershell -ExecutionPolicy Bypass -Command "& { iwr -useb https://raw.githubusercontent.com/tellebma/RocketMax-Plugin/main/install-rocketmax.ps1 | iex }"
)
