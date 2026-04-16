@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "MODE=%~1"
set "RUN_SWITCHES="
set "FORWARDED_ARGS="
set "TARGET_SCRIPT=%SCRIPT_DIR%run-server.ps1"
set "DEFAULT_ARGS=-Detached"

if /I "%MODE%"=="glock-session" goto mode_glock_session
if /I "%MODE%"=="glock-session-profile" goto mode_glock_session_profile
if /I "%MODE%"=="glock-lab" goto mode_glock_lab
if /I "%MODE%"=="glock-lab-targets" goto mode_glock_lab_targets
if /I "%MODE%"=="glock-lab-target" goto mode_glock_lab_target
if /I "%MODE%"=="glock-lab-profile" goto mode_glock_lab_profile
if /I "%MODE%"=="glock-profiles" goto mode_glock_profiles
if /I "%MODE%"=="glock-profile" goto mode_glock_profile
if /I "%MODE%"=="glock-matrices" goto mode_glock_matrices
if /I "%MODE%"=="glock-matrix" goto mode_glock_matrix
if /I "%MODE%"=="glock-compare" goto mode_glock_compare
if /I "%MODE%"=="mp5-profiles" goto mode_mp5_profiles
if /I "%MODE%"=="mp5-profile" goto mode_mp5_profile
if /I "%MODE%"=="mp5-session" goto mode_mp5_session
if /I "%MODE%"=="mp5-lab" goto mode_mp5_lab
if /I "%MODE%"=="mp5-lab-profile" goto mode_mp5_lab_profile
if /I "%MODE%"=="doctor" goto mode_doctor
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

:mode_glock_session_profile
set "TARGET_SCRIPT=%SCRIPT_DIR%run-glock-test-session.ps1"
set "RUN_SWITCHES=-GlockProfile"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_glock_lab
set "TARGET_SCRIPT=%SCRIPT_DIR%run-glock-test-session.ps1"
set "RUN_SWITCHES=-LabDummy"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_glock_lab_targets
set "TARGET_SCRIPT=%SCRIPT_DIR%list-glock-lab-targets.ps1"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_glock_lab_target
set "TARGET_SCRIPT=%SCRIPT_DIR%run-glock-test-session.ps1"
set "RUN_SWITCHES=-LabDummy -LabTargetProfile"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_glock_lab_profile
set "TARGET_SCRIPT=%SCRIPT_DIR%run-glock-test-session.ps1"
set "RUN_SWITCHES=-LabDummy -GlockProfile"
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

:mode_glock_matrices
set "TARGET_SCRIPT=%SCRIPT_DIR%list-glock-comparison-matrices.ps1"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_glock_matrix
set "TARGET_SCRIPT=%SCRIPT_DIR%run-glock-comparison-matrix.ps1"
set "RUN_SWITCHES=-Matrix"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_glock_compare
set "TARGET_SCRIPT=%SCRIPT_DIR%compare-weapon-reports.ps1"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_mp5_profiles
set "TARGET_SCRIPT=%SCRIPT_DIR%list-mp5-profiles.ps1"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_mp5_profile
set "TARGET_SCRIPT=%SCRIPT_DIR%run-mp5-test-session.ps1"
set "RUN_SWITCHES=-Mp5Profile"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_mp5_session
set "TARGET_SCRIPT=%SCRIPT_DIR%run-mp5-test-session.ps1"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_mp5_lab
set "TARGET_SCRIPT=%SCRIPT_DIR%run-mp5-test-session.ps1"
set "RUN_SWITCHES=-LabDummy"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_mp5_lab_profile
set "TARGET_SCRIPT=%SCRIPT_DIR%run-mp5-test-session.ps1"
set "RUN_SWITCHES=-LabDummy -Mp5Profile"
set "DEFAULT_ARGS="
shift
goto collect_args

:mode_doctor
set "TARGET_SCRIPT=%SCRIPT_DIR%doctor-testbed.ps1"
set "DEFAULT_ARGS="
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
