@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "MODE=%~1"
set "RUN_SWITCHES="
set "FORWARDED_ARGS="

if /I "%MODE%"=="experimental-debug" (
    set "RUN_SWITCHES=-EnableExperimentalGlock -EnableExperimentalGlockDebug"
    shift
) else if /I "%MODE%"=="experimental" (
    set "RUN_SWITCHES=-EnableExperimentalGlock"
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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%run-server.ps1" -Detached %RUN_SWITCHES% %FORWARDED_ARGS%
exit /b %ERRORLEVEL%
