@echo off
REM Quick Build Script for C64 Emulator
REM Usage: build.bat [Release|Debug]

set "CONFIGURATION=%1"
if "%CONFIGURATION%"=="" set "CONFIGURATION=Release"

set "MSBUILD_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
set "PROJECT_PATH=c:\Workspaces\Mine\aiemu\code\c"
set "SOLUTION_FILE=aiemuc.sln"

echo Building C64 Emulator (%CONFIGURATION%)...

REM Check if MSBuild exists
if not exist "%MSBUILD_PATH%" (
    echo ERROR: MSBuild not found at: %MSBUILD_PATH%
    echo Please install Visual Studio Build Tools 2022
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

pause
