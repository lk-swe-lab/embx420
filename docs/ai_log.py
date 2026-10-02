#!/usr/bin/env python3
"""Assignment AI log. Python standard library only."""
import argparse
from datetime import datetime, timezone
import os
from pathlib import Path
import sqlite3
import sys

HERE = Path(__file__).resolve().parent
SCHEMA = '''
CREATE TABLE IF NOT EXISTS ai_log (
    id INTEGER PRIMARY KEY,
    source_db TEXT NOT NULL,
    session_id TEXT NOT NULL,
    message_id INTEGER NOT NULL,
    created_at TEXT NOT NULL,
    prompt TEXT NOT NULL,
    response TEXT NOT NULL DEFAULT '',
    category TEXT NOT NULL DEFAULT 'unclassified'
        CHECK(category IN ('unclassified','discussion','implementation','debugging','unrelated')),
    changes TEXT NOT NULL DEFAULT '',
    result TEXT NOT NULL DEFAULT '',
    notes TEXT NOT NULL DEFAULT '',
    UNIQUE(source_db, session_id, message_id)
);
'''


def source_connection(path):
    path = Path(path).expanduser().resolve()
    conn = sqlite3.connect(path.as_uri() + '?mode=ro', uri=True)
    conn.row_factory = sqlite3.Row
    return conn


def log_connection(path, create=False):
    path = Path(path).expanduser().resolve()
    if create:
        path.parent.mkdir(parents=True, exist_ok=True)
        conn = sqlite3.connect(path)
        conn.executescript(SCHEMA)
    else:
        conn = sqlite3.connect(path.as_uri() + '?mode=rw', uri=True)
    conn.row_factory = sqlite3.Row
    return conn


def import_prompts(args):
    source_path = Path(args.source).expanduser().resolve()
    if source_path == Path(args.db).expanduser().resolve():
        raise ValueError('Source and log database must be different files.')
    entries = []
    with source_connection(source_path) as source:
        columns = {r['name'] for r in source.execute('PRAGMA table_info(messages)')}
        required = {'id', 'session_id', 'role', 'content', 'timestamp'}
        if not required.issubset(columns):
            raise ValueError('Unsupported source: missing Hermes message columns.')
        filters = ''
        if '_compressed_summary' in columns:
            filters += ' AND COALESCE(_compressed_summary, 0) = 0'
        if {'active', 'compacted'}.issubset(columns):
            filters += ' AND (active = 1 OR compacted = 1)'
        for session in dict.fromkeys(args.session):
            if not source.execute('SELECT 1 FROM sessions WHERE id = ?', (session,)).fetchone():
                raise ValueError(f'Unknown session: {session}')
            rows = source.execute(
                'SELECT id,role,content,timestamp FROM messages WHERE session_id = ?'
                + filters + ' ORDER BY id', (session,)).fetchall()
            current = None
            responses = []
            for row in rows:
                if row['role'] == 'user':
                    if current is not None:
                        entries.append((session, current, '\n\n'.join(responses)))
                    current, responses = row, []
                elif row['role'] == 'assistant' and current is not None and row['content']:
                    responses.append(row['content'])
            if current is not None:
                entries.append((session, current, '\n\n'.join(responses)))
    with log_connection(args.db, create=True) as log:
        for session, row, response in entries:
            timestamp = datetime.fromtimestamp(row['timestamp'], timezone.utc).isoformat()
            log.execute('''INSERT INTO ai_log
                (source_db,session_id,message_id,created_at,prompt,response)
                VALUES (?,?,?,?,?,?) ON CONFLICT(source_db,session_id,message_id)
                DO UPDATE SET response=excluded.response''',
                (str(source_path), session, row['id'], timestamp, row['content'] or '', response))
    print(f'Synced {len(entries)} prompt entries (repeat imports do not duplicate).')


def annotate(args):
    fields = {name: getattr(args, name) for name in ('category', 'changes', 'result', 'notes')
              if getattr(args, name) is not None}
    if not fields:
        raise ValueError('Provide at least one annotation field.')
    with log_connection(args.db) as log:
        assignments = ', '.join(f'{name} = ?' for name in fields)
        changed = log.execute(f'UPDATE ai_log SET {assignments} WHERE id = ?',
                              (*fields.values(), args.id))
        if not changed.rowcount:
            raise ValueError(f'Unknown entry: {args.id}')
    show(args)


def show(args):
    with log_connection(args.db) as log:
        row = log.execute('SELECT * FROM ai_log WHERE id = ?', (args.id,)).fetchone()
    if row is None:
        raise ValueError(f'Unknown entry: {args.id}')
    for name in row.keys():
        print(f'{name}: {row[name]}')


def list_entries(args):
    conditions, values = [], []
    if args.search:
        conditions.append("instr(lower(prompt || char(10) || response || char(10) || changes"
                          " || char(10) || result || char(10) || notes), lower(?)) > 0")
        values.append(args.search)
    if args.category:
        conditions.append('category = ?')
        values.append(args.category)
    where = ' WHERE ' + ' AND '.join(conditions) if conditions else ''
    with log_connection(args.db) as log:
        rows = log.execute('SELECT id,category,prompt FROM ai_log' + where +
                           ' ORDER BY created_at,id', values).fetchall()
    for row in rows:
        preview = ' '.join(row['prompt'].split())[:100]
        print(f"{row['id']}\t{row['category']}\t{preview}")
    print(f'{len(rows)} entries.')


def fenced(text):
    # A fence longer than any run in the original prevents embedded Markdown escaping.
    import re
    length = max([2] + [len(run) for run in re.findall(r'`+', text)]) + 1
    fence = '`' * length
    return f'{fence}text\n{text}\n{fence}'


def export_md(args):
    output = args.output.expanduser().resolve()
    if output == Path(args.db).expanduser().resolve():
        raise ValueError('Output must not overwrite the database.')
    where = '' if args.include_unrelated else " WHERE category != 'unrelated'"
    with log_connection(args.db) as log:
        rows = log.execute('SELECT * FROM ai_log' + where + ' ORDER BY created_at,id').fetchall()
        if any(output == Path(row['source_db']).resolve() for row in rows):
            raise ValueError('Output must not overwrite a source database.')
    parts = ['# AI log', 'Review for private information before publishing. Unclassified entries are included.']
    for row in rows:
        parts.append(f"## Entry {row['id']} — {row['category']}\n\n"
                     f"UTC: {row['created_at']}\n\n"
                     f"Source: session {row['session_id']}, message {row['message_id']}")
        for title, field in [('Prompt', 'prompt'), ('AI response (saved text)', 'response'),
                             ('Changes', 'changes'), ('Result', 'result'), ('Notes', 'notes')]:
            value = row[field]
            if not value:
                value = 'Not recorded.' if field != 'result' else 'Untested / not recorded.'
            parts.append(f'### {title}\n\n{fenced(value)}')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text('\n\n'.join(parts) + '\n', encoding='utf-8')
    print(f'Exported {len(rows)} entries to {output}. Review before publishing.')


def parser():
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument('--db', type=Path, default=HERE / 'ai_log.db', help='Assignment database path')
    commands = result.add_subparsers(dest='command', required=True)
    imp = commands.add_parser('import', help='Copy explicitly selected Hermes sessions')
    imp.add_argument('--source', type=Path,
                     default=Path(os.environ.get('HERMES_HOME', Path.home() / '.hermes')) / 'state.db')
    imp.add_argument('--session', action='append', required=True, help='Session ID; repeat for multiple sessions')
    imp.set_defaults(handler=import_prompts)
    categories = ('unclassified', 'discussion', 'implementation', 'debugging', 'unrelated')
    note = commands.add_parser('annotate', help='Document an entry without changing its prompt')
    note.add_argument('id', type=int)
    note.add_argument('--category', choices=categories)
    for field in ('changes', 'result', 'notes'):
        note.add_argument('--' + field)
    note.set_defaults(handler=annotate)
    view = commands.add_parser('show', help='Show one complete entry')
    view.add_argument('id', type=int)
    view.set_defaults(handler=show)
    listing = commands.add_parser('list', help='Find entries by keyword or category')
    listing.add_argument('--search')
    listing.add_argument('--category', choices=categories)
    listing.set_defaults(handler=list_entries)
    export = commands.add_parser('export', help='Export Markdown; excludes only unrelated entries')
    export.add_argument('--output', type=Path, default=HERE / 'AI_LOG.md')
    export.add_argument('--include-unrelated', action='store_true')
    export.set_defaults(handler=export_md)
    return result


def main(argv=None):
    cli = parser()
    args = cli.parse_args(argv)
    try:
        args.handler(args)
    except (OSError, sqlite3.Error, ValueError, TypeError, OverflowError) as error:
        cli.exit(1, f'Error: {error}\n')


if __name__ == '__main__':
    main()
