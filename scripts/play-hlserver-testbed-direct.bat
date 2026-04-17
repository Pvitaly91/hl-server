@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%play-hlserver-testbed-direct.ps1" %*
exit /b %ERRORLEVEL%
