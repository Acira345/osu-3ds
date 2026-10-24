@echo off
title Servidor QR osu! 3DS (FBI)
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0instalar_qr_local.ps1"
pause
