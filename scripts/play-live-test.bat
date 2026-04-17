@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"

:menu
cls
echo Live disposable testbed menu
echo.
echo   1. Glock live lab ^(same-root live mod^)
echo   2. MP5 live lab ^(same-root live mod^)
echo   3. Glock live lab ^(same-root live mod, unarmored^)
echo   4. MP5 live lab ^(same-root live mod, vest_headprotected^)
echo   5. Open latest analysis
echo   6. Run doctor
echo   7. Exit
echo.
choice /c 1234567 /n /m "Select an option: "

if errorlevel 7 goto done
if errorlevel 6 goto doctor
if errorlevel 5 goto analysis
if errorlevel 4 goto mp5_protected
if errorlevel 3 goto glock_unarmored
if errorlevel 2 goto mp5_default
if errorlevel 1 goto glock_default

:glock_default
call "%SCRIPT_DIR%play-glock-live.bat"
goto finish

:mp5_default
call "%SCRIPT_DIR%play-mp5-live.bat"
goto finish

:glock_unarmored
call "%SCRIPT_DIR%play-glock-live.bat" -TargetProfile unarmored
goto finish

:mp5_protected
call "%SCRIPT_DIR%play-mp5-live.bat" -TargetProfile vest_headprotected
goto finish

:analysis
call "%SCRIPT_DIR%show-latest-log-analysis.bat"
goto finish

:doctor
call "%SCRIPT_DIR%run-testbed.bat" doctor
goto finish

:finish
echo.
pause
goto done

:done
exit /b %ERRORLEVEL%
