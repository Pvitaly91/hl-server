@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0play-improved-hldm.ps1" %*
if errorlevel 1 pause
