"""Exercise the real module entry against required and optional ABI-v1 sizes."""
import ctypes
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class HostABI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        scratch = Path(cls.temp.name)
        (scratch / 'minimal.h').write_text('''#include "cmd_spec.h"
static const cmd_spec_t minimal_spec = {.name = "minimal"};
''')
        binary = scratch / 'entry.so'
        subprocess.run([
            'cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC',
            '-Isrc', '-I' + str(scratch), '-DMODULE_HEADER="minimal.h"',
            '-DMODULE_SPEC=minimal_spec', 'src/module_entry.c', '-o', str(binary)
        ], cwd=ROOT, check=True)
        cls.module = ctypes.CDLL(str(binary))
        cls.entry = cls.module.aishell_module_entry_v1
        cls.entry.argtypes = [ctypes.c_void_p]
        cls.entry.restype = ctypes.c_void_p

    def host(self, size, version=1):
        # Allocate exactly the advertised size, including an old host with no tail.
        storage = ctypes.create_string_buffer(size)
        header = (ctypes.c_uint32 * 2).from_buffer(storage)
        header[:] = size, version
        return storage

    def test_previous_80_byte_required_prefix_accepted(self):
        self.assertIsNotNone(self.entry(self.host(80)))

    def test_current_88_byte_host_accepted(self):
        self.assertIsNotNone(self.entry(self.host(88)))

    def test_truncated_required_prefix_rejected(self):
        for size in (72, 79):
            with self.subTest(size=size):
                self.assertIsNone(self.entry(self.host(size)))

    def test_wrong_version_rejected(self):
        self.assertIsNone(self.entry(self.host(88, version=2)))

    def test_null_host_rejected(self):
        self.assertIsNone(self.entry(None))


if __name__ == '__main__':
    unittest.main()
