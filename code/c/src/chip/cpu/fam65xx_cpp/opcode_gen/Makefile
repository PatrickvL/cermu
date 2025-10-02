# Makefile for fam65xx opcode generation and compilation
# Automatically regenerates decoder when generator script changes

PYTHON := python3
GEN_SCRIPT := fam65xx_gen.py
DECODER_HEADER := _fam65xx_decoder.h
FAM65XX_HEADER := fam65xx.h
TEST_RUNNER := fam65xx_processor_tests_runner_debug
TEST_RUNNER_SRC := ../../../../../../c/tests/fam65xx_processor_tests_runner.c
JSON_PARSER_SRC := ../../../../../../c/tests/json_parser.c

# Compiler settings
CC := gcc
CFLAGS := -I../../../.. -DCHIPS_IMPL -std=c99 -Wall -g

.PHONY: all clean test regenerate

# Default target
all: $(TEST_RUNNER)

# Generate decoder header from Python script
$(DECODER_HEADER): $(GEN_SCRIPT)
	@echo "Regenerating decoder header..."
	$(PYTHON) $(GEN_SCRIPT) > $(DECODER_HEADER)

# fam65xx.h depends on the generated decoder
$(FAM65XX_HEADER): $(DECODER_HEADER)
	@echo "Decoder regenerated, fam65xx.h is now up to date"
	@touch $(FAM65XX_HEADER)

# Test runner depends on fam65xx.h
$(TEST_RUNNER): $(TEST_RUNNER_SRC) $(JSON_PARSER_SRC) $(FAM65XX_HEADER)
	@echo "Compiling test runner..."
	$(CC) -o $(TEST_RUNNER) $(TEST_RUNNER_SRC) $(JSON_PARSER_SRC) $(CFLAGS)

# Force regeneration
regenerate:
	@echo "Force regenerating decoder..."
	$(PYTHON) $(GEN_SCRIPT) > $(DECODER_HEADER)
	@touch $(FAM65XX_HEADER)
	@echo "Recompiling test runner..."
	$(CC) -o $(TEST_RUNNER) $(TEST_RUNNER_SRC) $(JSON_PARSER_SRC) $(CFLAGS)

# Run test with NOP
test: $(TEST_RUNNER)
	@echo "Running NOP test..."
	./fam65xx_processor_tests_runner_debug -v ../../../../../../c/tests/nop_test.json

# Clean generated files
clean:
	rm -f $(TEST_RUNNER)

# Show dependency info
info:
	@echo "Dependency chain:"
	@echo "1. $(GEN_SCRIPT) -> $(DECODER_HEADER)"
	@echo "2. $(DECODER_HEADER) -> $(FAM65XX_HEADER)"
	@echo "3. $(FAM65XX_HEADER) -> $(TEST_RUNNER)"
	@echo ""
	@echo "Usage:"
	@echo "  make          - Build everything (regenerates if needed)"
	@echo "  make test     - Build and run NOP test"
	@echo "  make regenerate - Force regeneration and rebuild"
	@echo "  make clean    - Clean built files"