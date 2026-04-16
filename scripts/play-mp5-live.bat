@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%play-live-session.ps1" -Weapon mp5 %*
exit /b %ERRORLEVEL%
