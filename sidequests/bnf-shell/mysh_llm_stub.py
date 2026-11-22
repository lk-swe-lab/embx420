#!/usr/bin/python3
"""Fixed-response teaching stub. No AI, query interpretation, or network."""
import sys

if len(sys.argv) != 2 or not 1 <= len(sys.argv[1].encode()) <= 4096:
    raise SystemExit(2)

# Receive the query as argv[1], but do not interpret it.
print('echo hello')
