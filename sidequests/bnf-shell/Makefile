BNFC ?= bnfc
CC = gcc-13
CFLAGS = -std=c11 -Wall -Wextra -D_POSIX_C_SOURCE=200809L
PYTHON ?= python3

# Use the generated Makefile without a change to generated files.
GENERATED_FLAGS = $(CFLAGS) -Wno-unused-function -Wno-unused-parameter
OBJECTS = build/Absyn.o build/Buffer.o build/Lexer.o \
          build/Parser.o build/Printer.o

.PHONY: all generate parser test clean

all: build/shell

# Generate the parser when the grammar changes.
build/Makefile: Grammar.cf
	$(BNFC) --c -m -o build Grammar.cf

generate: build/Makefile

# Let the generated Makefile check its source and object dependencies.
parser: build/Makefile
	$(MAKE) -C build CC="$(CC)" CCFLAGS="$(GENERATED_FLAGS)"

# Link the driver after the generated objects are ready.
build/shell: shell.c parser
	$(CC) $(CFLAGS) -pthread -Ibuild shell.c $(OBJECTS) -o $@

test: build/shell
	$(PYTHON) -m unittest -v test_grammar test_driver

# Remove only the generated build directory.
clean:
	rm -rf build
