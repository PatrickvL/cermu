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
              json_parser.h

# Target executable
TARGET = fam65xx_processor_tests_runner

# Default target
all: $(TARGET)

# Build test runner - depends on headers too
$(TARGET): $(RUNNER_OBJ) $(HEADER_DEPS)
	$(CC) $(CFLAGS) -o $@ $(RUNNER_OBJ) $(LIBS)

# Compile C++ files - depend on headers
%.o: %.cpp $(HEADER_DEPS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile C files - depend on headers
%.o: %.c $(HEADER_DEPS)
	gcc -std=c99 -O2 -Wall -Wextra $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f *.o $(TARGET)

# Test with sample data (if available)
test: $(TARGET)
	@echo "Running parallel processor tests..."
	@if [ -d "processor_tests" ]; then \
		./$(TARGET) -v processor_tests/; \
	else \
		echo "No processor_tests directory found. Please provide test data."; \
	fi

# Performance test with multiple worker counts
perf: $(TARGET)
	@echo "Performance testing with different worker counts:"
	@echo "=============================================="
	@if [ -d "processor_tests" ]; then \
		echo "Single-threaded:"; \
		time ./$(TARGET) -j 1 processor_tests/; \
		echo ""; \
		echo "Multi-threaded (auto):"; \
		time ./$(TARGET) processor_tests/; \
		echo ""; \
		echo "Multi-threaded (4 workers):"; \
		time ./$(TARGET) -j 4 processor_tests/; \
	else \
		echo "No processor_tests directory found for performance testing."; \
	fi

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# Release build with maximum optimization
release: CFLAGS = -std=c++17 -O3 -DNDEBUG -march=native -pthread
release: $(TARGET)

# Install (copy to parent directory)
install: $(TARGET)
	cp $(TARGET) ../

.PHONY: all clean test perf debug release install
