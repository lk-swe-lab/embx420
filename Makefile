# Build the CLI with diagnostics and debug symbols.
CC = cc
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion -Wstrict-prototypes -g3 -O0
CUNIT_CPPFLAGS ?= -Ibuild/cunit/include
CUNIT_LDLIBS ?= -Lbuild/cunit/lib -lcunit
ARGTABLE3_CPPFLAGS ?= -Ibuild/argtable3/include
ARGTABLE3_LDLIBS ?= -Lbuild/argtable3/lib -largtable3 -lm
CLANG_TIDY ?= clang-tidy
CLANG_FORMAT ?= clang-format
C_SOURCES = $(wildcard src/*.c tests/*.c)
C_FILES = $(C_SOURCES) $(wildcard src/*.h tests/*.h)
# Exclude blanket Annex K replacement advice; keep other analyzer checks.
CLANG_TIDY_CHECKS = -*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling

.PHONY: all test test-stat test-stat-impl lint format-check format clean
all: build/main build/stat build/ls build/mkdir

build/mkdir: src/mkdir_main.c src/cmd_mkdir.c src/cmd_mkdir.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/mkdir_main.c src/cmd_mkdir.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/ls: src/ls_main.c src/cmd_ls.c src/cmd_ls.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/ls_main.c src/cmd_ls.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

.PHONY: test-mkdir test-mkdir-impl
test-mkdir:
	python3 tests/test_mkdir.py /usr/bin/mkdir --reference

build/test_mkdir_module: tests/test_mkdir_module.c src/cmd_mkdir.c src/cmd_mkdir.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_mkdir_module.c src/cmd_mkdir.c src/cmd_lines.c -Wl,--wrap=arg_lit0,--wrap=arg_filen,--wrap=arg_end,--wrap=arg_parse,--wrap=malloc,--wrap=calloc,--wrap=strdup,--wrap=free,--wrap=mkdir,--wrap=stat,--wrap=chmod,--wrap=write $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-mkdir-impl: build/mkdir build/test_mkdir_module
	python3 tests/test_mkdir.py ./build/mkdir
	./build/test_mkdir_module

.PHONY: test-ls test-ls-impl
test-ls:
	python3 tests/test_ls.py /usr/bin/ls --reference

build/test_ls_module: tests/test_ls_module.c src/cmd_ls.c src/cmd_ls.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_ls_module.c src/cmd_ls.c src/cmd_lines.c -Wl,--wrap=arg_lit0,--wrap=realloc,--wrap=strdup,--wrap=opendir,--wrap=readdir,--wrap=fstatat,--wrap=closedir,--wrap=write $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-ls-impl: build/ls build/test_ls_module
	python3 tests/test_ls.py ./build/ls
	./build/test_ls_module

build/stat: src/stat_main.c src/cmd_stat.c src/cmd_stat.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/stat_main.c src/cmd_stat.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/test_stat_module: tests/test_stat_module.c src/cmd_stat.c src/cmd_stat.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_stat_module.c src/cmd_stat.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

# Reference checks remain separate from project regression tests.
test-stat:
	python3 tests/test_stat.py /usr/bin/stat --reference

test-stat-impl: build/stat build/test_stat_module
	python3 tests/test_stat.py ./build/stat
	./build/test_stat_module

build/main: src/main.c src/traverse.c src/traverse.h Makefile | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/main.c src/traverse.c $(LDLIBS)

build/test_help: tests/test_help.c tests/test_output.h Makefile | build
	$(CC) $(CPPFLAGS) $(CUNIT_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(CUNIT_LDLIBS) $(LDLIBS)

build/test_recursive: tests/test_recursive.c tests/test_output.h Makefile | build
	$(CC) $(CPPFLAGS) $(CUNIT_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(CUNIT_LDLIBS) $(LDLIBS)

test: build/main build/test_help build/test_recursive build/stat build/test_stat_module build/ls build/test_ls_module build/mkdir build/test_mkdir_module
	./build/test_help
	./build/test_recursive
	python3 tests/test_stat.py ./build/stat
	./build/test_stat_module
	python3 tests/test_ls.py ./build/ls
	./build/test_ls_module
	python3 tests/test_mkdir.py ./build/mkdir
	./build/test_mkdir_module

lint:
	$(CLANG_TIDY) --checks='$(CLANG_TIDY_CHECKS)' --warnings-as-errors='*' $(C_SOURCES) -- $(CPPFLAGS) $(CUNIT_CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(C_FILES)

format:
	$(CLANG_FORMAT) -i $(C_FILES)

build:
	mkdir -p $@

clean:
	$(RM) build/mkdir build/test_mkdir_module
	$(RM) build/ls build/test_ls_module
	$(RM) build/stat build/test_stat_module build/main build/test_help build/test_recursive build/help.stdout build/help.stderr
