# Makefile for fam65xx processor tests runner (now with parallel execution)
CC = g++
CFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
INCLUDES = -I../src -I.
LIBS = -pthread

# Source files
RUNNER_SRC = fam65xx_processor_tests_runner.cpp json_parser.c
RUNNER_OBJ = $(RUNNER_SRC:.cpp=.o)
RUNNER_OBJ := $(RUNNER_OBJ:.c=.o)

# Header dependencies - any change to these headers triggers rebuild
HEADER_DEPS = ../src/chip/cpu/fam65xx/fam65xx.hpp \
              ../src/chip/cpu/fam65xx/fam65xx_core.hpp \
              ../src/chip/cpu/fam65xx/fam65xx_impl.hpp \
              ../src/chip/cpu/fam65xx/fam65xx_tables.hpp \
              ../src/chip/cpu/fam65xx/fam65xx_addrmodes.hpp \
              ../src/chip/cpu/fam65xx/fam65xx_ops.hpp \
              ../src/chip/cpu/fam65xx/fam65xx_ops_part2.hpp \
              ../src/chip/cpu/fam65xx/fam65xx_ops_part3.hpp \
              ../src/chip/cpu/fam65xx/fam65xx_ops_rmw.hpp \
              ../src/chip/cpu/fam65xx/fam65xx_ops_illegal.hpp \
              ../src/chip/cpu/fam65xx/fam65xx_ops_65c02.hpp \
              json_parser.h

# Target executables
PROCESSOR_TESTS_TARGET = fam65xx_processor_tests_runner
KLAUS_TARGET = fam65xx_klaus_test_runner
LORENZ_TARGET = fam65xx_lorenz_test_harness

# Klaus test runner source files
KLAUS_SRC = fam65xx_klaus_test_runner.cpp
KLAUS_OBJ = $(KLAUS_SRC:.cpp=.o)

# Lorenz test harness source files
LORENZ_SRC = fam65xx_lorenz_test_harness.cpp
LORENZ_OBJ = $(LORENZ_SRC:.cpp=.o)

# Default target - build all test runners
all: $(PROCESSOR_TESTS_TARGET) $(KLAUS_TARGET) $(LORENZ_TARGET)

# Build processor tests runner - depends on headers too
$(PROCESSOR_TESTS_TARGET): $(RUNNER_OBJ) $(HEADER_DEPS)
	$(CC) $(CFLAGS) -o $@ $(RUNNER_OBJ) $(LIBS)

# Build Klaus test runner
$(KLAUS_TARGET): $(KLAUS_OBJ) $(HEADER_DEPS)
	$(CC) $(CFLAGS) -o $@ $(KLAUS_OBJ) $(LIBS)

# Build Lorenz test harness
$(LORENZ_TARGET): $(LORENZ_OBJ) $(HEADER_DEPS)
	$(CC) $(CFLAGS) -o $@ $(LORENZ_OBJ) $(LIBS)

# Compile C++ files - depend on headers
%.o: %.cpp $(HEADER_DEPS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile C files - depend on headers
%.o: %.c $(HEADER_DEPS)
	gcc -std=c99 -O2 -Wall -Wextra $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f *.o $(PROCESSOR_TESTS_TARGET) $(KLAUS_TARGET) $(LORENZ_TARGET)

# Test with sample data (if available)
test: $(PROCESSOR_TESTS_TARGET)
	@echo "Running parallel processor tests..."
	@if [ -d "processor_tests" ]; then \
		./$(PROCESSOR_TESTS_TARGET) -v processor_tests/; \
	else \
		echo "No processor_tests directory found. Please provide test data."; \
	fi

# Run Klaus functional tests
test-klaus: $(KLAUS_TARGET)
	@echo "Running Klaus2m5 functional tests..."
	@if [ -f "/home/patrick/Git/aiemu/external/6502-tests/6502_65C02_functional_tests/bin_files/6502_functional_test.bin" ]; then \
		./$(KLAUS_TARGET) --functional; \
	else \
		echo "Klaus test binary not found. Please check external/6502-tests/ directory."; \
	fi

# Run Lorenz tests (example)
test-lorenz: $(LORENZ_TARGET)
	@echo "Running Wolfgang Lorenz tests..."
	@echo "Note: Lorenz tests require test binaries in ../external/lorenz-tests/bin/"
	@echo "Usage: ./$(LORENZ_TARGET) <test_name.prg> [expected_results_file]"

# Performance test with multiple worker counts
perf: $(PROCESSOR_TESTS_TARGET)
	@echo "Performance testing with different worker counts:"
	@echo "=============================================="
	@if [ -d "processor_tests" ]; then \
		echo "Single-threaded:"; \
		time ./$(PROCESSOR_TESTS_TARGET) -j 1 processor_tests/; \
		echo ""; \
		echo "Multi-threaded (auto):"; \
		time ./$(PROCESSOR_TESTS_TARGET) processor_tests/; \
		echo ""; \
		echo "Multi-threaded (4 workers):"; \
		time ./$(PROCESSOR_TESTS_TARGET) -j 4 processor_tests/; \
	else \
		echo "No processor_tests directory found for performance testing."; \
	fi

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: all

# Release build with maximum optimization
release: CFLAGS = -std=c++17 -O3 -DNDEBUG -march=native -pthread
release: all

# Install (copy to parent directory)
install: all
	cp $(PROCESSOR_TESTS_TARGET) $(KLAUS_TARGET) $(LORENZ_TARGET) ../

.PHONY: all clean test test-klaus test-lorenz perf debug release install
