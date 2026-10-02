"""Offline tests; fixture messages are synthetic, not assignment records."""
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).with_name('ai_log.py')


class LogTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source.db'
        self.db = self.root / 'log.db'
        with sqlite3.connect(self.source) as conn:
            conn.executescript('''
                CREATE TABLE sessions (id TEXT PRIMARY KEY, title TEXT, cwd TEXT, started_at REAL);
                CREATE TABLE messages (id INTEGER PRIMARY KEY, session_id TEXT, role TEXT,
                    content TEXT, timestamp REAL, _compressed_summary INTEGER DEFAULT 0,
                    active INTEGER DEFAULT 1, compacted INTEGER DEFAULT 0);
                INSERT INTO sessions VALUES ('selected', 'Test session', '/assignment', 1);
                INSERT INTO sessions VALUES ('other', 'Other session', '/elsewhere', 2);
                INSERT INTO messages VALUES (1,'selected','user','Exact prompt\nwith newline',1,0,1,0);
                INSERT INTO messages VALUES (2,'selected','assistant','First response',2,0,1,0);
                INSERT INTO messages VALUES (3,'selected','tool','Private tool output',3,0,1,0);
                INSERT INTO messages VALUES (4,'selected','assistant','Final response',4,0,1,0);
                INSERT INTO messages VALUES (5,'other','user','Unrelated prompt',5,0,1,0);
            ''')

    def run_cli(self, *args, success=True):
        result = subprocess.run([sys.executable, str(SCRIPT), '--db', str(self.db), *args],
                                capture_output=True, text=True)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def import_selected(self):
        return self.run_cli('import', '--source', str(self.source), '--session', 'selected')

    def test_sessions_discovery_and_errors(self):
        output = self.run_cli('sessions', '--source', str(self.source), '--cwd', '/assignment').stdout
        self.assertIn('selected', output)
        self.assertNotIn('other', output)
        self.run_cli('import', '--source', str(self.source), '--session', 'missing', success=False)
        self.assertFalse(self.db.exists())
        self.run_cli('import', '--source', str(self.source), success=False)
        self.run_cli('show', '99', success=False)
        self.assertFalse(self.db.exists())
        self.import_selected()
        self.run_cli('annotate', '99', '--notes', 'No such row', success=False)
        self.run_cli('annotate', '1', success=False)
        self.run_cli('annotate', '1', '--category', 'invalid', success=False)
        self.run_cli('export', '--output', str(self.db), success=False)

    def test_late_responses_and_compacted_originals(self):
        with sqlite3.connect(self.source) as conn:
            conn.execute('UPDATE messages SET active=0,compacted=1 WHERE id=1')
            conn.execute("INSERT INTO messages VALUES (6,'selected','user','Summary',6,1,1,0)")
        self.import_selected()
        with sqlite3.connect(self.source) as conn:
            conn.execute("INSERT INTO messages VALUES (7,'selected','assistant','Late ``` text',7,0,1,0)")
            conn.execute("INSERT INTO messages VALUES (8,'selected','user','Next prompt',8,0,1,0)")
            conn.execute("INSERT INTO messages VALUES (9,'selected','user','Rewound',9,0,0,0)")
        self.import_selected()
        with sqlite3.connect(self.db) as conn:
            rows = conn.execute('SELECT prompt,response FROM ai_log ORDER BY id').fetchall()
        self.assertEqual(len(rows), 2)
        self.assertEqual(rows[1], ('Next prompt', ''))
        self.assertIn('Late ``` text', rows[0][1])
        self.run_cli('annotate', '1', '--category', 'unrelated')
        self.run_cli('annotate', '2', '--category', 'unrelated')
        self.run_cli('export', '--output', str(self.source), success=False)
        out = self.root / 'export.md'
        self.run_cli('export', '--output', str(out), '--include-unrelated')
        self.assertIn('````text\nFirst response', out.read_text())

    def test_document_find_export_preserves_history(self):
        self.import_selected()
        self.run_cli('annotate', '1', '--category', 'implementation',
                     '--changes', 'cli.py: added --help; edited AI wording.',
                     '--result', 'Untested', '--notes', 'Follow-up to planning.')
        self.import_selected()
        shown = self.run_cli('show', '1').stdout
        self.assertIn('cli.py: added --help', shown)
        self.assertIn('Exact prompt', self.run_cli('list', '--search', 'newline').stdout)
        out = self.root / 'AI_LOG.md'
        self.run_cli('export', '--output', str(out))
        text = out.read_text()
        self.assertIn('Exact prompt\nwith newline', text)
        self.assertIn('Final response', text)
        self.assertIn('Untested', text)
        self.assertNotIn('Private tool output', text)
        self.run_cli('annotate', '1', '--category', 'unrelated')
        self.run_cli('export', '--output', str(out))
        self.assertNotIn('Exact prompt', out.read_text())
        self.run_cli('export', '--output', str(out), '--include-unrelated')
        self.assertIn('Exact prompt', out.read_text())

    def test_import_exact_selected_turn_idempotently(self):
        self.import_selected()
        self.import_selected()
        with sqlite3.connect(self.db) as conn:
            rows = conn.execute('SELECT prompt,response,category FROM ai_log').fetchall()
        self.assertEqual(rows, [('Exact prompt\nwith newline',
                                 'First response\n\nFinal response', 'unclassified')])
        with sqlite3.connect(self.source) as conn:
            self.assertEqual(conn.execute('SELECT COUNT(*) FROM messages').fetchone()[0], 5)


if __name__ == '__main__':
    unittest.main()
