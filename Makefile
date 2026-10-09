# Build the CLI with diagnostics and debug symbols.
CC = cc
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Wformat=2 -Wshadow -Wconversion -Wstrict-prototypes -g3 -O0
CUNIT_CPPFLAGS ?= -Ibuild/cunit/include
CUNIT_LDLIBS ?= -Lbuild/cunit/lib -lcunit
ARGTABLE3_CPPFLAGS ?= -Ibuild/argtable3/include
ARGTABLE3_LDLIBS ?= -Lbuild/argtable3/lib -largtable3 -lm
CAT_UNDER_TEST ?= /usr/bin/cat
CAT_TEST_ARGS ?=
CLANG_TIDY ?= clang-tidy
CLANG_FORMAT ?= clang-format
C_SOURCES = $(wildcard src/*.c tests/*.c)
C_FILES = $(C_SOURCES) $(wildcard src/*.h tests/*.h)
# Exclude blanket Annex K replacement advice; keep other analyzer checks.
CLANG_TIDY_CHECKS = -*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling

.PHONY: all test test-cat test-cat-impl test-head test-head-impl test-tail test-tail-impl lint format-check format clean
all: build/main build/cat build/head build/tail build/pwd build/stat build/ls build/mkdir build/touch build/clear build/cp build/rg build/edit-replace-line build/edit-insert-line build/edit-delete-line build/edit-replace build/mv build/env

build/touch: src/touch_main.c src/cmd_touch.c src/cmd_touch.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/touch_main.c src/cmd_touch.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/mkdir: src/mkdir_main.c src/cmd_mkdir.c src/cmd_mkdir.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/mkdir_main.c src/cmd_mkdir.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/head: src/head_main.c src/cmd_head.c src/cmd_head.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/head_main.c src/cmd_head.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/tail: src/tail_main.c src/cmd_tail.c src/cmd_tail.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/tail_main.c src/cmd_tail.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/pwd: src/pwd_main.c src/cmd_pwd.c src/cmd_pwd.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/pwd_main.c src/cmd_pwd.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/ls: src/ls_main.c src/cmd_ls.c src/cmd_ls.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/ls_main.c src/cmd_ls.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

.PHONY: test-touch test-touch-impl
test-touch:
	python3 tests/test_touch.py /usr/bin/touch --reference

build/test_touch_module: tests/test_touch_module.c src/cmd_touch.c src/cmd_touch.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_touch_module.c src/cmd_touch.c src/cmd_lines.c -Wl,--wrap=arg_lit0,--wrap=arg_filen,--wrap=arg_end,--wrap=arg_parse,--wrap=malloc,--wrap=calloc,--wrap=free,--wrap=utimensat,--wrap=open,--wrap=futimens,--wrap=close,--wrap=write,--wrap=strerror $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-touch-impl: build/touch build/test_touch_module
	python3 tests/test_touch.py ./build/touch
	./build/test_touch_module

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

SHELL_SOURCES = src/shell.c src/registry.c src/cmd_cat.c src/cmd_head.c src/cmd_tail.c src/cmd_pwd.c src/cmd_stat.c src/cmd_ls.c src/cmd_mkdir.c src/cmd_touch.c src/cmd_clear.c src/cmd_cd.c src/cmd_cp.c src/cmd_rg.c src/cmd_edit_replace_line.c src/cmd_edit_insert_line.c src/cmd_lines.c src/cmd_edit_delete_line.c src/cmd_edit_replace.c src/cmd_mv.c src/cmd_env.c
SHELL_HEADERS = src/shell.h src/registry.h src/cmd_spec.h src/cmd_cat.h src/cmd_head.h src/cmd_tail.h src/cmd_pwd.h src/cmd_stat.h src/cmd_ls.h src/cmd_mkdir.h src/cmd_touch.h src/cmd_clear.h src/cmd_cd.h src/cmd_cp.h src/cmd_rg.h src/cmd_edit_replace_line.h src/cmd_edit_insert_line.h src/cmd_lines.h src/cmd_edit_delete_line.h src/cmd_edit_replace.h src/cmd_mv.h src/cmd_env.h

build/main: src/main.c src/traverse.c src/traverse.h $(SHELL_SOURCES) $(SHELL_HEADERS) Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/main.c src/traverse.c $(SHELL_SOURCES) $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/test_shell_module: tests/test_shell_module.c $(SHELL_SOURCES) $(SHELL_HEADERS) Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -Isrc -o $@ tests/test_shell_module.c $(SHELL_SOURCES) -Wl,--wrap=malloc,--wrap=realloc,--wrap=read $(ARGTABLE3_LDLIBS) $(LDLIBS)

.PHONY: test-shell
test-shell: build/main build/test_shell_module build/cat build/head build/tail build/pwd build/stat build/ls build/mkdir build/touch build/clear build/cp build/rg build/edit-replace-line build/edit-insert-line build/edit-delete-line build/edit-replace build/mv build/env
	python3 tests/test_shell.py --program ./build/main --standalone-dir ./build
	./build/test_shell_module

test: test-shell test-clear test-cd test-cp-impl test-rg-impl test-edit-replace-line-impl test-edit-insert-line-impl test-edit-delete-line-impl

build/cat: src/cat_main.c src/cmd_cat.c src/cmd_cat.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/cat_main.c src/cmd_cat.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/test_help: tests/test_help.c tests/test_output.h Makefile | build
	$(CC) $(CPPFLAGS) $(CUNIT_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(CUNIT_LDLIBS) $(LDLIBS)

build/test_recursive: tests/test_recursive.c tests/test_output.h Makefile | build
	$(CC) $(CPPFLAGS) $(CUNIT_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(CUNIT_LDLIBS) $(LDLIBS)

build/test_cat: tests/test_cat.c tests/test_output.h Makefile | build
	$(CC) $(CPPFLAGS) $(CUNIT_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(CUNIT_LDLIBS) $(LDLIBS)

build/test_cat_module: tests/test_cat_module.c tests/test_output.h src/cmd_cat.c src/cmd_cat.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(CUNIT_CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_cat_module.c src/cmd_cat.c -Wl,--wrap=read,--wrap=write $(CUNIT_LDLIBS) $(ARGTABLE3_LDLIBS) $(LDLIBS)

# Reference-first suite; deliberately separate from project regression tests.
test-cat: build/test_cat
	./build/test_cat "$(CAT_UNDER_TEST)" $(CAT_TEST_ARGS)

# Build and test only our cat implementation, including direct module tests.
test-cat-impl: build/cat build/test_cat build/test_cat_module
	./build/test_cat ./build/cat --project
	./build/test_cat_module

test-head:
	python3 tests/test_head.py --program /usr/bin/head --reference

build/test_head_module: tests/test_head_module.c src/cmd_head.c src/cmd_head.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_head_module.c src/cmd_head.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-head-impl: build/head build/test_head_module
	python3 tests/test_head.py --program ./build/head
	./build/test_head_module

test-tail:
	python3 tests/test_tail.py --program /usr/bin/tail --reference

build/test_tail_module: tests/test_tail_module.c src/cmd_tail.c src/cmd_tail.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_tail_module.c src/cmd_tail.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-tail-impl: build/tail build/test_tail_module
	python3 tests/test_tail.py --program ./build/tail
	./build/test_tail_module

.PHONY: test-lines-random
test-lines-random: build/head build/tail
	python3 tests/test_lines_random.py

.PHONY: test-pwd test-pwd-impl
test-pwd:
	python3 tests/test_pwd.py /usr/bin/pwd --reference

build/test_pwd_module: tests/test_pwd_module.c src/cmd_pwd.c src/cmd_pwd.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_pwd_module.c src/cmd_pwd.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-pwd-impl: build/pwd build/test_pwd_module
	python3 tests/test_pwd.py ./build/pwd
	./build/test_pwd_module

.PHONY: test-stat test-stat-impl
test-stat:
	python3 tests/test_stat.py /usr/bin/stat --reference

build/test_stat_module: tests/test_stat_module.c src/cmd_stat.c src/cmd_stat.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_stat_module.c src/cmd_stat.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-stat-impl: build/stat build/test_stat_module
	python3 tests/test_stat.py ./build/stat
	./build/test_stat_module

test: build/touch build/test_touch_module build/mkdir build/test_mkdir_module build/ls build/test_ls_module build/stat build/test_stat_module build/main build/cat build/head build/tail build/pwd build/test_help build/test_recursive build/test_cat build/test_cat_module build/test_head_module build/test_tail_module build/test_pwd_module
	./build/test_help
	./build/test_recursive
	./build/test_cat ./build/cat --project
	./build/test_cat_module
	python3 tests/test_head.py --program ./build/head
	./build/test_head_module
	python3 tests/test_tail.py --program ./build/tail
	./build/test_tail_module
	python3 tests/test_lines_random.py
	python3 tests/test_pwd.py ./build/pwd
	./build/test_pwd_module
	python3 tests/test_stat.py ./build/stat
	./build/test_stat_module
	python3 tests/test_ls.py ./build/ls
	./build/test_ls_module
	python3 tests/test_mkdir.py ./build/mkdir
	./build/test_mkdir_module
	python3 tests/test_touch.py ./build/touch
	./build/test_touch_module

lint:
	$(CLANG_TIDY) --checks='$(CLANG_TIDY_CHECKS)' --warnings-as-errors='*' $(C_SOURCES) -- $(CPPFLAGS) $(CUNIT_CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(C_FILES)

format:
	$(CLANG_FORMAT) -i $(C_FILES)

build:
	mkdir -p $@

clean:
	$(RM) build/env build/test_env_module build/cmd_env_test.o
	$(RM) build/mv build/test_mv_module build/cmd_mv_test.o
	$(RM) build/edit-replace build/test_edit_replace_module
	$(RM) build/edit-delete-line build/test_edit_delete_line_module
	$(RM) build/edit-insert-line build/test_edit_insert_line_module
	$(RM) build/edit-replace-line build/test_edit_replace_line_module
	$(RM) build/rg build/test_rg_module
	$(RM) build/cp build/test_cp_module
	$(RM) build/test_cd_module
	$(RM) build/clear build/test_clear_module
	$(RM) build/test_shell_module
	$(RM) build/touch build/test_touch_module
	$(RM) build/mkdir build/test_mkdir_module
	$(RM) build/ls build/test_ls_module
	$(RM) build/stat build/test_stat_module
	$(RM) build/pwd build/test_pwd_module
	$(RM) build/main build/cat build/head build/tail build/test_head_module build/test_tail_module build/test_help build/test_recursive build/test_cat build/test_cat_module build/help.stdout build/help.stderr

.PHONY: test-clear
build/clear: src/clear_main.c src/cmd_clear.c src/cmd_clear.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/clear_main.c src/cmd_clear.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/test_clear_module: tests/test_clear_module.c src/cmd_clear.c src/cmd_clear.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_clear_module.c src/cmd_clear.c src/cmd_lines.c -Wl,--wrap=write,--wrap=isatty,--wrap=malloc,--wrap=arg_lit0,--wrap=arg_end $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-clear: build/clear build/test_clear_module build/main
	python3 tests/test_clear.py --program ./build/clear --main ./build/main
	./build/test_clear_module

.PHONY: test-cd
build/test_cd_module: tests/test_cd_module.c src/cmd_cd.c src/cmd_cd.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_cd_module.c src/cmd_cd.c -Wl,--wrap=arg_lit0,--wrap=arg_file0,--wrap=arg_end,--wrap=arg_parse,--wrap=malloc,--wrap=free,--wrap=chdir $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-cd: build/main build/test_cd_module
	python3 tests/test_cd.py ./build/main
	./build/test_cd_module

.PHONY: test-cp test-cp-impl
build/cp: src/cp_main.c src/cmd_cp.c src/cmd_cp.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/cp_main.c src/cmd_cp.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/test_cp_module: tests/test_cp_module.c src/cmd_cp.c src/cmd_cp.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_cp_module.c src/cmd_cp.c src/cmd_lines.c -Wl,--wrap=mmap,--wrap=munmap,--wrap=msync,--wrap=ftruncate,--wrap=open,--wrap=close,--wrap=read,--wrap=write,--wrap=arg_parse,--wrap=arg_lit0,--wrap=malloc $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-cp:
	python3 tests/test_cp.py /usr/bin/cp --reference

test-cp-impl: build/cp build/test_cp_module
	python3 tests/test_cp.py ./build/cp
	./build/test_cp_module

.PHONY: test-rg test-rg-impl
build/rg: src/rg_main.c src/cmd_rg.c src/cmd_rg.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/rg_main.c src/cmd_rg.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/test_rg_module: tests/test_rg_module.c src/cmd_rg.c src/cmd_rg.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_rg_module.c src/cmd_rg.c src/cmd_lines.c -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=strdup,--wrap=arg_lit0,--wrap=arg_end,--wrap=arg_parse,--wrap=read,--wrap=write,--wrap=open,--wrap=close,--wrap=fstat,--wrap=lstat,--wrap=fdopendir,--wrap=readdir,--wrap=closedir,--wrap=regexec $(ARGTABLE3_LDLIBS) $(LDLIBS)

.PHONY: test-edit-replace-line-impl
build/edit-replace-line: src/edit_replace_line_main.c src/cmd_edit_replace_line.c src/cmd_edit_replace_line.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/edit_replace_line_main.c src/cmd_edit_replace_line.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/test_edit_replace_line_module: tests/test_edit_replace_line_module.c src/cmd_edit_replace_line.c src/cmd_edit_replace_line.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_edit_replace_line_module.c src/cmd_edit_replace_line.c src/cmd_lines.c -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free,--wrap=arg_lit0,--wrap=arg_file0,--wrap=arg_str0,--wrap=arg_end,--wrap=arg_parse,--wrap=open,--wrap=lstat,--wrap=fstat,--wrap=mkstemp,--wrap=fcntl,--wrap=fchown,--wrap=fchmod,--wrap=fsync,--wrap=close,--wrap=read,--wrap=write,--wrap=rename,--wrap=unlink $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-edit-replace-line-impl: build/edit-replace-line build/test_edit_replace_line_module
	python3 tests/test_edit_replace_line.py --program ./build/edit-replace-line
	./build/test_edit_replace_line_module

RG_REFERENCE ?= rg
test-rg:
	python3 tests/test_rg.py "$$(command -v $(RG_REFERENCE))" --reference

test-rg-impl: build/rg build/test_rg_module
	python3 tests/test_rg.py ./build/rg
	./build/test_rg_module

.PHONY: test-edit-insert-line-impl
build/edit-insert-line: src/edit_insert_line_main.c src/cmd_edit_insert_line.c src/cmd_edit_insert_line.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/edit_insert_line_main.c src/cmd_edit_insert_line.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/test_edit_insert_line_module: tests/test_edit_insert_line_module.c src/cmd_edit_insert_line.c src/cmd_edit_insert_line.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_edit_insert_line_module.c src/cmd_edit_insert_line.c src/cmd_lines.c -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free,--wrap=arg_lit0,--wrap=arg_file0,--wrap=arg_str0,--wrap=arg_end,--wrap=arg_parse,--wrap=open,--wrap=lstat,--wrap=fstat,--wrap=mkstemp,--wrap=fcntl,--wrap=fchown,--wrap=fchmod,--wrap=fsync,--wrap=close,--wrap=read,--wrap=write,--wrap=rename,--wrap=unlink $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-edit-insert-line-impl: build/edit-insert-line build/test_edit_insert_line_module
	python3 tests/test_edit_insert_line.py --program ./build/edit-insert-line
	./build/test_edit_insert_line_module

.PHONY: test-edit-delete-line-impl
build/edit-delete-line: src/edit_delete_line_main.c src/cmd_edit_delete_line.c src/cmd_edit_delete_line.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/edit_delete_line_main.c src/cmd_edit_delete_line.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/test_edit_delete_line_module: tests/test_edit_delete_line_module.c src/cmd_edit_delete_line.c src/cmd_edit_delete_line.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_edit_delete_line_module.c src/cmd_edit_delete_line.c src/cmd_lines.c -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free,--wrap=arg_lit0,--wrap=arg_file0,--wrap=arg_str0,--wrap=arg_end,--wrap=arg_parse,--wrap=open,--wrap=lstat,--wrap=fstat,--wrap=mkstemp,--wrap=fcntl,--wrap=fchown,--wrap=fchmod,--wrap=fsync,--wrap=close,--wrap=read,--wrap=write,--wrap=rename,--wrap=unlink $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-edit-delete-line-impl: build/edit-delete-line build/test_edit_delete_line_module
	python3 tests/test_edit_delete_line.py --program ./build/edit-delete-line
	./build/test_edit_delete_line_module

build/test_edit_replace_module: tests/test_edit_replace_module.c src/cmd_edit_replace.c src/cmd_edit_replace.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_edit_replace_module.c src/cmd_edit_replace.c src/cmd_lines.c -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free,--wrap=arg_lit0,--wrap=arg_file0,--wrap=arg_str0,--wrap=arg_end,--wrap=arg_parse,--wrap=open,--wrap=lstat,--wrap=fstat,--wrap=mkstemp,--wrap=fcntl,--wrap=fchown,--wrap=fchmod,--wrap=fsync,--wrap=close,--wrap=read,--wrap=write,--wrap=rename,--wrap=unlink,--wrap=regcomp,--wrap=regexec,--wrap=newlocale,--wrap=uselocale $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-edit-replace-impl: build/edit-replace build/test_edit_replace_module
	python3 tests/test_edit_replace.py --program ./build/edit-replace
	./build/test_edit_replace_module

test: test-edit-replace-impl test-mv-impl

.PHONY: test-mv test-mv-impl
build/mv: src/mv_main.c src/cmd_mv.c src/cmd_mv.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/mv_main.c src/cmd_mv.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/cmd_mv_test.o: src/cmd_mv.c src/cmd_mv.h src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) -Dmalloc=mv_test_malloc -Dfree=mv_test_free -c -o $@ src/cmd_mv.c

build/test_mv_module: tests/test_mv_module.c build/cmd_mv_test.o src/cmd_lines.c src/cmd_lines.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_mv_module.c build/cmd_mv_test.o src/cmd_lines.c -Wl,--wrap=arg_lit0,--wrap=arg_file0,--wrap=arg_end,--wrap=arg_parse,--wrap=rename,--wrap=stat,--wrap=lstat,--wrap=fcntl,--wrap=write $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-mv:
	python3 tests/test_mv.py /usr/bin/mv --reference

test-mv-impl: build/mv build/test_mv_module
	python3 tests/test_mv.py ./build/mv
	./build/test_mv_module

.PHONY: test-edit-replace-impl
build/edit-replace: src/edit_replace_main.c src/cmd_edit_replace.c src/cmd_edit_replace.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/edit_replace_main.c src/cmd_edit_replace.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

.PHONY: test-env test-env-impl
build/env: src/env_main.c src/cmd_env.c src/cmd_env.h src/cmd_lines.c src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/env_main.c src/cmd_env.c src/cmd_lines.c $(ARGTABLE3_LDLIBS) $(LDLIBS)

build/cmd_env_test.o: src/cmd_env.c src/cmd_env.h src/cmd_lines.h src/cmd_spec.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) -Dmalloc=env_test_malloc -Dfree=env_test_free -c -o $@ src/cmd_env.c

build/test_env_module: tests/test_env_module.c build/cmd_env_test.o src/cmd_lines.c src/cmd_lines.h Makefile | build
	$(CC) $(CPPFLAGS) $(ARGTABLE3_CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ tests/test_env_module.c build/cmd_env_test.o src/cmd_lines.c -Wl,--wrap=arg_lit0,--wrap=arg_end,--wrap=arg_parse,--wrap=write $(ARGTABLE3_LDLIBS) $(LDLIBS)

test-env:
	python3 tests/test_env.py /usr/bin/env --reference

test-env-impl: build/env build/main build/test_env_module
	python3 tests/test_env.py ./build/env --main ./build/main
	./build/test_env_module

test: test-env-impl
