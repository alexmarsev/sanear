@echo off
cd /d "%~dp0"

regsvr32.exe sanear.ax /s
if %ERRORLEVEL% neq 0 goto fail

if "%PROCESSOR_ARCHITECTURE%" == "x86" goto ok
regsvr32.exe sanear64.ax /s
if %ERRORLEVEL% neq 0 goto fail

:ok
echo.
echo   Registration succeeded
echo.
goto done

:fail
echo.
echo   Registration failed!
echo.
echo   Try to right-click on %~nx0 and select "Run as administrator"
echo.

:done
pause >nul
