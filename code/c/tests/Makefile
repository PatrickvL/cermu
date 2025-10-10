# Makefile for fam65xx processor tests runner (now with parallel execution)
CC = g++
CFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
INCLUDES = -I../src -I.
LIBS = -pthread

# Source files
RUNNER_SRC = fam65xx_processor_tests_runner.cpp json_parser.c
RUNNER_OBJ = $(RUNNER_SRC:.cpp=.o)
RUNNER_OBJ := $(RUNNER_OBJ:.c=.o)

# Target executable
TARGET = fam65xx_processor_tests_runner

# Default target
all: $(TARGET)

# Build test runner
$(TARGET): $(RUNNER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile C++ files
%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Compile C files  
%.o: %.c
	gcc $(CFLAGS) $(INCLUDES) -c $< -o $@

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
