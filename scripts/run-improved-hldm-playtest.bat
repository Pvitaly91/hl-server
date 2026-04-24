@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-improved-hldm-playtest.ps1" %*
if errorlevel 1 pause
