@echo off
echo Building MOS6510 Comprehensive Test Suite with CMake...

REM Create build directory if it doesn't exist
if not exist build mkdir build

REM Change to build directory
cd build

echo Configuring CMake...
cmake ..

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    rem pause
    exit /b 1
)

echo Building tests...
cmake --build . --config Debug

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    rem pause
    exit /b 1
)

echo Running tests...
ctest --verbose -C Debug

if %ERRORLEVEL% NEQ 0 (
    echo Some tests failed, but executables were built successfully.
    echo You can run tests manually:
    echo   Debug\test_mos6510_basic.exe
    echo   Debug\test_mos6510_comprehensive.exe
    echo.
    echo Running comprehensive test directly...
    Debug\test_mos6510_comprehensive.exe
) else (
    echo All tests passed successfully!
)

REM pause