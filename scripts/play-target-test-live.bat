@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
echo Live target test launcher
echo   default weapon  : glock
echo   target mode     : server-side standing dummy
echo   useful commands :
echo     exp_target_spawn
echo     exp_target_clear
echo     exp_target_respawn
echo     exp_target_status
echo     exp_target_tp_front
echo     exp_target_profile vest_headprotected
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%play-live-session.ps1" -Weapon glock %*
exit /b %ERRORLEVEL%
