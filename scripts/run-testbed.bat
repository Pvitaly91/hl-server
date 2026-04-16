@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "MODE=%~1"
set "RUN_SWITCHES="
set "FORWARDED_ARGS="
set "TARGET_SCRIPT=%SCRIPT_DIR%run-server.ps1"
set "DEFAULT_ARGS=-Detached"

if /I "%MODE%"=="glock-session" goto mode_glock_session
if /I "%MODE%"=="glock-profiles" goto mode_glock_profiles
if /I "%MODE%"=="glock-profile" goto mode_glock_profile
if /I "%MODE%"=="glock-report" goto mode_glock_report
if /I "%MODE%"=="experimental-debug" goto mode_experimental_debug
if /I "%MODE%"=="experimental" goto mode_experimental
if /I "%MODE%"=="vanilla" goto mode_vanilla
goto collect_args

:mode_glock_session
set "TARGET_SCRIPT=%SCRIPT_DIR%run-glock-test-session.ps1"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_glock_profiles
set "TARGET_SCRIPT=%SCRIPT_DIR%list-glock-profiles.ps1"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_glock_profile
set "RUN_SWITCHES=-EnableExperimentalGlock -EnableExperimentalGlockDebug -GlockProfile"
shift
goto collect_args

:mode_glock_report
set "TARGET_SCRIPT=%SCRIPT_DIR%analyze-weapon-log.ps1"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_experimental_debug
set "RUN_SWITCHES=-EnableExperimentalGlock -EnableExperimentalGlockDebug"
shift
goto collect_args

:mode_experimental
set "RUN_SWITCHES=-EnableExperimentalGlock"
shift
goto collect_args

:mode_vanilla
shift
goto collect_args

:collect_args
if "%~1"=="" goto launch
set FORWARDED_ARGS=%FORWARDED_ARGS% "%~1"
shift
goto collect_args

:launch
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%TARGET_SCRIPT%" %DEFAULT_ARGS% %RUN_SWITCHES% %FORWARDED_ARGS%
exit /b %ERRORLEVEL%
