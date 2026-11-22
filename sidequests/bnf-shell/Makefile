BNFC ?= bnfc
CC = gcc-13
CFLAGS = -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
PYTHON ?= python3

# Use the generated Makefile without a change to generated files.
GENERATED_FLAGS = $(CFLAGS) -Wno-unused-function -Wno-unused-parameter
OBJECTS = build/Absyn.o build/Buffer.o build/Lexer.o \
          build/Parser.o build/Printer.o

.PHONY: all generate parser test sanitize demo clean

all: build/shell

# Generate the parser when the grammar changes.
build/Makefile: Grammar.cf prepare_parser.py
	$(BNFC) --c -m -o build Grammar.cf
	$(PYTHON) prepare_parser.py

generate: build/Makefile

# Let the generated Makefile check its source and object dependencies.
parser: build/Makefile
	$(MAKE) -C build CC="$(CC)" CCFLAGS="$(GENERATED_FLAGS)"

# Link the driver after the generated objects are ready.
build/shell: shell.c nl_input.c nl_input.h parser
	$(CC) $(CFLAGS) -pthread -Ibuild shell.c nl_input.c $(OBJECTS) -o $@

test: build/shell
	$(PYTHON) -m unittest -v test_grammar test_driver test_runtime \
	  test_build test_nl_input

demo: build/shell
	$(PYTHON) demo.py

# Instrument generated code as well as the hand-written runtime.
sanitize: parser
	$(CC) $(GENERATED_FLAGS) -g -fno-omit-frame-pointer -no-pie \
	  -fsanitize=address,undefined -pthread -Ibuild shell.c nl_input.c \
	  build/Absyn.c build/Buffer.c build/Lexer.c build/Parser.c \
	  -o build/shell-sanitize
	ASAN_OPTIONS=detect_leaks=1:exitcode=99 UBSAN_OPTIONS=halt_on_error=1 \
	  BNF_SHELL=$(CURDIR)/build/shell-sanitize \
	  $(PYTHON) -m unittest -v test_driver test_runtime test_nl_input

# Remove only the generated build directory.
clean:
	rm -rf build
