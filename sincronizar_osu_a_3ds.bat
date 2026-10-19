@echo off
chcp 65001 >nul
title Sincronizador de Canciones osu! para Nintendo 3DS

echo =======================================================
echo    Sincronizador osu! (Stable / Lazer) a Nintendo 3DS
echo =======================================================
echo.
echo Iniciando script de sincronizacion con permisos de ejecucion...
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0sincronizar_osu_a_3ds.ps1"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [AVISO] El script termino con codigo: %ERRORLEVEL%
)

echo.
pause
