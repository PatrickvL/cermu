@echo off
setlocal enabledelayedexpansion

REM Build script for fam65xx processor tests runner (now with parallel execution)
REM Usage: build.bat [debug|release]

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=default

echo Building fam65xx processor tests runner (Parallel Edition)...
echo Build type: %BUILD_TYPE%
echo ==============================================================

REM Check if required files exist
if not exist "fam65xx_processor_tests_runner.cpp" (
    echo ERROR: fam65xx_processor_tests_runner.cpp not found!
    exit /b 1
)

if not exist "json_parser.c" (
    echo ERROR: json_parser.c not found!
    exit /b 1
)

if not exist "json_parser.h" (
    echo ERROR: json_parser.h not found!
    exit /b 1
)

REM Check for CPU implementation header
if not exist "..\src\chip\cpu\fam65xx_cpp\opcode_gen\fam65xx_callbacks.h" (
    echo ERROR: fam65xx_callbacks.h not found!
    echo Expected location: ..\src\chip\cpu\fam65xx_cpp\opcode_gen\fam65xx_callbacks.h
    exit /b 1
)

REM Set compiler flags based on build type
set COMMON_FLAGS=-std=c++17 -Wall -Wextra -pthread -I../src -I.
set TARGET=fam65xx_processor_tests_runner.exe

if "%BUILD_TYPE%"=="debug" (
    set CFLAGS=%COMMON_FLAGS% -g -DDEBUG -O0
    echo Building DEBUG version...
) else (
    if "%BUILD_TYPE%"=="release" (
        set CFLAGS=%COMMON_FLAGS% -O3 -DNDEBUG -march=native
        echo Building RELEASE version (optimized)...
    ) else (
        set CFLAGS=%COMMON_FLAGS% -O2
        echo Building DEFAULT version...
    )
)

REM Clean previous build
if exist "%TARGET%" del "%TARGET%"
if exist "*.o" del "*.o"

REM Compile
echo Compiling processor tests runner...
g++ %CFLAGS% -c fam65xx_processor_tests_runner.cpp -o fam65xx_processor_tests_runner.o
if !errorlevel! neq 0 (
    echo ERROR: Failed to compile C++ source!
    exit /b 1
)

gcc %CFLAGS% -c json_parser.c -o json_parser.o
if !errorlevel! neq 0 (
    echo ERROR: Failed to compile C source!
    exit /b 1
)

REM Link
echo Linking...
g++ %CFLAGS% -o %TARGET% fam65xx_processor_tests_runner.o json_parser.o -pthread
if !errorlevel! neq 0 (
    echo ERROR: Failed to link executable!
    exit /b 1
)

REM Check if build was successful
if exist "%TARGET%" (
    echo.
    echo ✅ Build successful!
    echo Executable: %TARGET%
    echo.
    echo Usage examples:
    echo   %TARGET% -h                           # Show help
    echo   %TARGET% processor_tests\             # Run with auto-detected cores
    echo   %TARGET% -j 4 -v tests\              # 4 workers, verbose
    echo   %TARGET% -q -c tests\                # Quiet, continue on failures
    echo.
    
    REM Show system info for optimal threading
    for /f "tokens=2 delims==" %%i in ('wmic cpu get NumberOfLogicalProcessors /value ^| find "="') do set CORES=%%i
    set /a RECOMMENDED=CORES-1
    if !RECOMMENDED! lss 1 set RECOMMENDED=1
    
    echo System info:
    echo   CPU cores detected: !CORES!
    echo   Recommended workers: !RECOMMENDED! (cores - 1)
    echo.
    
    REM Show performance improvement note
    echo 🚀 Performance improvements:
    echo   ✓ Parallel execution (up to !RECOMMENDED!x faster on this system)
    echo   ✓ Compacted debug output for speed
    echo   ✓ Thread-safe output (no mixed stdout)
    echo   ✓ Smart core usage (leaves 1 core for system)
    echo.
) else (
    echo.
    echo ❌ Build failed!
    exit /b 1
)

REM Clean up object files
if exist "*.o" del "*.o"

endlocal