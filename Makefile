# Build the CLI with diagnostics and debug symbols.
CC = cc
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion -Wstrict-prototypes -g3 -O0
CUNIT_CPPFLAGS ?= -Ibuild/cunit/include
CUNIT_LDLIBS ?= -Lbuild/cunit/lib -lcunit
CLANG_TIDY ?= clang-tidy
CLANG_FORMAT ?= clang-format
C_SOURCES = $(wildcard src/*.c tests/*.c)
C_FILES = $(C_SOURCES) $(wildcard src/*.h tests/*.h)
# Exclude blanket Annex K replacement advice; keep other analyzer checks.
CLANG_TIDY_CHECKS = -*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling

.PHONY: all test lint format-check format clean
all: build/main

build/main: src/main.c src/traverse.c src/traverse.h Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/main.c src/traverse.c $(LDLIBS)

build/test_help: tests/test_help.c tests/test_output.h Makefile | build
	$(CC) $(CPPFLAGS) $(CUNIT_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(CUNIT_LDLIBS) $(LDLIBS)

build/test_recursive: tests/test_recursive.c tests/test_output.h Makefile | build
	$(CC) $(CPPFLAGS) $(CUNIT_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(CUNIT_LDLIBS) $(LDLIBS)

test: build/main build/test_help build/test_recursive
	./build/test_help
	./build/test_recursive

lint:
	$(CLANG_TIDY) --checks='$(CLANG_TIDY_CHECKS)' --warnings-as-errors='*' $(C_SOURCES) -- $(CPPFLAGS) $(CUNIT_CPPFLAGS) $(CFLAGS)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(C_FILES)

format:
	$(CLANG_FORMAT) -i $(C_FILES)

build:
	mkdir -p $@

clean:
	$(RM) build/main build/test_help build/test_recursive build/help.stdout build/help.stderr
