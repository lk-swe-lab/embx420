"""Bounded differential checks supplement independently specified fixtures."""
import json
from pathlib import Path
import random
import subprocess

ROOT = Path(__file__).resolve().parents[1]
rng = random.Random(420)
checks = 0
for name in ("head", "tail"):
    for case in range(60):
        size = rng.choice((0, 1, 255, 8191, 8192, 8193, 17000))
        alphabet = b'abc\n\r\t\x00"\\' if case % 2 else bytes(range(256))
        data = bytes(rng.choice(alphabet) for _ in range(size))
        count = rng.choice((0, 1, 2, 10, 100, 1000000))
        args = ["-n", str(count)]
        reference = subprocess.run(["/usr/bin/" + name, *args], input=data,
                                   capture_output=True, timeout=5)
        actual = subprocess.run([str(ROOT / "build" / name), *args],
                                input=data, capture_output=True, timeout=5)
        assert reference.returncode == actual.returncode == 0, (name, case)
        assert actual.stdout == reference.stdout, (name, case, "raw bytes")
        assert actual.stderr == reference.stderr == b"", (name, case)
        encoded = subprocess.run([str(ROOT / "build" / name), "--json", *args],
                                 input=data, capture_output=True, timeout=5)
        try:
            selected = reference.stdout.decode("utf-8")
        except UnicodeDecodeError:
            assert encoded.returncode == 1 and not encoded.stdout, (name, case)
            assert encoded.stderr, (name, case)
        else:
            # LF alone is the delimiter; splitlines() would also split CR.
            lines = selected.split("\n") if selected else []
            if selected.endswith("\n"):
                lines.pop()
            assert encoded.returncode == 0 and not encoded.stderr, (name, case)
            assert json.loads(encoded.stdout) == {"path": "-", "lines": lines}
        checks += 1
print(f"head/tail differential: {checks} raw + JSON scenarios passed")
