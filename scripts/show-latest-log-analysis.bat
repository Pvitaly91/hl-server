@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%show-latest-analysis.ps1" -Weapon all %*
exit /b %ERRORLEVEL%
