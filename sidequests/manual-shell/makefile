CC ?= cc
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -g
TOKENIZER ?= manual
ifeq ($(TOKENIZER),manual)
TOKENIZER_CPPFLAGS = -DUSE_STRTOK=0
else ifeq ($(TOKENIZER),strtok)
TOKENIZER_CPPFLAGS = -DUSE_STRTOK=1
else
$(error TOKENIZER must be manual or strtok)
endif
# Pin the sibling programs at build time; cd and PATH cannot change selection.
COMMAND_DIR := $(abspath ../../build)
CPPFLAGS += -DCOMMAND_DIR='"$(COMMAND_DIR)"'
OBJECTS = Unix_Shell.o token.o command.o builtin.o

.PHONY: all clean test FORCE
all: Unix_Shell

Unix_Shell: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

Unix_Shell.o: Unix_Shell.c token.h command.h builtin.h makefile
builtin.o: builtin.c builtin.h command.h
command.o: command.c command.h
# Rebuild this small module so changing TOKENIZER never reuses the other mode.
token.o: token.c token.h makefile FORCE
token.o: CPPFLAGS += $(TOKENIZER_CPPFLAGS)
FORCE:

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: Unix_Shell
	python3 tests/test_shell.py -v

clean:
	$(RM) $(OBJECTS) Unix_Shell
