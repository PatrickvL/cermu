#!/bin/bash

echo "Building MOS6510 Comprehensive Test Suite with CMake..."

# Create build directory if it doesn't exist
mkdir -p build

# Change to build directory
cd build

echo "Configuring CMake..."
cmake ..

if [ $? -ne 0 ]; then
    echo "CMake configuration failed!"
    exit 1
fi

echo "Building tests..."
cmake --build .

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Running tests..."
ctest --verbose

if [ $? -ne 0 ]; then
    echo "Some tests failed!"
    exit 1
fi

echo "All tests passed successfully!"