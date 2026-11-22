CC = cc
ROOT := ../..
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -g -O0
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -I$(ROOT)/src
SOURCES := main.c parse.c execute.c $(ROOT)/src/module_runtime.c
HEADERS := lean.h $(wildcard $(ROOT)/src/*.h)

.PHONY: all modules test test-demo sanitize clean
all: build/lean-shell modules

build/lean-shell: $(SOURCES) $(HEADERS) Makefile
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(SOURCES) -ldl -o $@

modules:
	$(MAKE) -C $(ROOT) build/modules/.stamp
	mkdir -p build/modules
	cp $(ROOT)/build/modules/*.so build/modules/

build/probe.so: tests/probe.c
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< -o $@

build/lean-shell-fault: $(SOURCES) tests/faults.c $(HEADERS) Makefile
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(SOURCES) tests/faults.c \
	    -Wl,--wrap=pipe,--wrap=fork,--wrap=dup2,--wrap=waitpid -ldl -o $@

test: all build/probe.so build/lean-shell-fault
	$(MAKE) -C $(ROOT) package-hello
	python3 tests/test_shell.py

test-demo: all
	bash -n demo.sh
	python3 tests/test_demo.py

build/lean-shell-sanitize: $(SOURCES) $(HEADERS) Makefile
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined \
	    -fno-omit-frame-pointer -no-pie $(SOURCES) -ldl -o $@

build/lean-shell-fault-sanitize: $(SOURCES) tests/faults.c $(HEADERS) Makefile
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -fsanitize=address,undefined \
	    -fno-omit-frame-pointer -no-pie $(SOURCES) tests/faults.c \
	    -Wl,--wrap=pipe,--wrap=fork,--wrap=dup2,--wrap=waitpid -ldl -o $@

sanitize: test build/lean-shell-sanitize build/lean-shell-fault-sanitize
	ASAN_OPTIONS=detect_leaks=1 \
	    LEAN_SHELL_UNDER_TEST=build/lean-shell-sanitize \
	    LEAN_FAULT_SHELL_UNDER_TEST=build/lean-shell-fault-sanitize \
	    python3 tests/test_shell.py

clean:
	rm -rf build
