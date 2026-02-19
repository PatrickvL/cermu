@echo off
REM Quick Build Script for C64 Emulator
REM Usage: build.bat [Release|Debug]

set "CONFIGURATION=%1"
if "%CONFIGURATION%"=="" set "CONFIGURATION=Release"

REM Detect project path relative to script location
set "SCRIPT_DIR=%~dp0"
set "PROJECT_PATH=%SCRIPT_DIR%.."
set "SOLUTION_FILE=cermu.sln"

REM Try multiple MSBuild locations
set "MSBUILD_PATH="
for %%P in (
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
) do (
    if exist "%%~P" (
        set "MSBUILD_PATH=%%~P"
        goto :found_msbuild
    )
)
:found_msbuild

echo Building C64 Emulator (%CONFIGURATION%)...

REM Check if MSBuild exists
if "%MSBUILD_PATH%"=="" (
    echo ERROR: MSBuild not found. Tried:
    echo   - "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    echo   - "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    echo   - "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
    echo   - "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    echo.
    echo Please install one of:
    echo   - Visual Studio Build Tools 2022
    echo   - Visual Studio 2022 (any edition)
    exit /b 1
)

REM Check if project directory exists
if not exist "%PROJECT_PATH%" (
    echo ERROR: Project directory not found: %PROJECT_PATH%
    echo Please run this script from the project root directory
    exit /b 1
)

REM Change to project directory and build
cd /d "%PROJECT_PATH%"
if errorlevel 1 (
    echo ERROR: Could not change to project directory: %PROJECT_PATH%
    exit /b 1
)

echo Using MSBuild: %MSBUILD_PATH%
"%MSBUILD_PATH%" "%SOLUTION_FILE%" /p:Configuration=%CONFIGURATION% /verbosity:minimal

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo Build completed successfully!
echo Executables available in: bin\%CONFIGURATION%\

if exist "bin\%CONFIGURATION%" (
    echo Available executables:
    dir /b "bin\%CONFIGURATION%\*.exe"
)

rem pause
