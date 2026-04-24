@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup-improved-hldm.ps1" %*
if errorlevel 1 pause
