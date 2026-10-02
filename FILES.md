# File registry

## Existing
- README.md — program status, build/run commands, and test commands.
- AGENTS.md — collaboration, development, and logging rules.
- Introduction.odt — assignment requirements.
- FILES.md — this registry; update when files change.
- docs/SESSION_LOOKUP.md — current session lookup and registration procedure.
- docs/sessions.txt — selected assignment session IDs.
- LSP0_ Introduction.pptx.pdf — course slides.
- data.sqlite — local assignment text and FTS5 index; generated, Git-ignored.
- scripts/assignment_data.py — local ingestion, optional OCR, and bounded search.
- scripts/test_assignment_data.py — ingestion and search tests.
- scripts/requirements.txt — Python utility dependencies.
- docs/DATA_LOOKUP.md — setup and assignment lookup procedure.
- docs/DESIGN.md — planning and design rules.
- docs/CODING.md — C line limits, include comments, and function documentation.
- docs/TDD.md — test-driven implementation rules.
- docs/VERIFY.md — testing and delivery verification rules.
- docs/AI_LOGGING.md — registry, logging, and publication rules.
- .gitignore — excludes local environment, caches, and generated database.
- Makefile — builds CLI/tests; test, lint, format-check, format, and clean.
- .clang-format — C formatting with an 80-column limit.
- src/main.c — assignment CLI source.
- tests/test_help.c — failing CUnit CLI test for -h usage and exit status.
- docs/TESTING.md — CUnit setup and test commands.
- wiki/SCHEMA.md — wiki format and approval-first chat workflow.
- wiki/index.md — approved entries and drafts.
- wiki/log.md — wiki change log.
- wiki/_drafts/recursive-call.md — names-only recursive directory traversal design; draft.
- .tmp/HANDOFF.md — temporary restart context; Git-ignored.


## Planned
- docs/export_md.py — export selected Hermes history to Markdown.
- docs/AI_LOG.md — generated prompt history for publication.
- docs/CHANGES.md — brief decisions, changes, and test results.
