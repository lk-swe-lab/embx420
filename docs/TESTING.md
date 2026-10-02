# C tests

Run `make test` from the assignment root. Install CUnit on Ubuntu with `sudo apt-get install libcunit1-dev`.

This session uses locally extracted Ubuntu CUnit headers and a static library under Git-ignored `build/cunit/`; no privileged installation succeeded. Make also supports system-installed CUnit. Override `CUNIT_CPPFLAGS` and `CUNIT_LDLIBS` for another location.

`tests/test_help.c` runs `build/main -h` and captures stdout in `build/help.stdout`. One assertion compares its first line exactly with `Usage: main [-h|--help]` followed by a newline. Execution and file-open failures fail the test. Additional help lines are allowed; stderr is not checked. This is a CLI-level test, not an isolated function unit test.

Verified red: compilation succeeded without warnings; only the usage assertion failed. Leave production behavior unchanged until the human implements help.

`make clean` removes executables and captured output, preserving local CUnit dependencies.
