@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "MODE=%~1"
set "EXPERIMENTAL_SWITCH="
set "FORWARDED_ARGS="

if /I "%MODE%"=="experimental" (
    set "EXPERIMENTAL_SWITCH=-EnableExperimentalGlock"
    shift
) else if /I "%MODE%"=="vanilla" (
    shift
)

:collect_args
if "%~1"=="" goto launch
set FORWARDED_ARGS=%FORWARDED_ARGS% "%~1"
shift
goto collect_args

:launch
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%run-server.ps1" -Detached %EXPERIMENTAL_SWITCH% %FORWARDED_ARGS%
exit /b %ERRORLEVEL%
