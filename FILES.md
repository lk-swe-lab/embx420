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
- demo.sh — recorded demo: clean, tests, clean, build, CLI options, lint.
- demo/2026-10-01-demo.mp4 — silent MP4 demonstration.
- demo/Screencast from 2026-10-01 22-27-29.webm — original recording.
- .clang-format — C formatting with an 80-column limit.
- src/main.c — assignment CLI source.
- tests/test_help.c — failing CUnit CLI test for -h usage and exit status.
- docs/TESTING.md — CUnit setup and test commands.
- wiki/SCHEMA.md — wiki format and approval-first chat workflow.
- wiki/index.md — approved entries and drafts.
- wiki/log.md — wiki change log.
- wiki/_drafts/recursive-call.md — names-only recursive directory traversal design; draft.
- .tmp/HANDOFF.md — temporary restart context; Git-ignored.


## ls command
- docs/commands/ls.md — ls inventory, implemented contract, TDD evidence, and verification limits.
- src/cmd_ls.c, src/cmd_ls.h — sorted directory listing, long metadata, and JSON module.
- src/ls_main.c — thin standalone ls entry point.
- tests/test_ls.py — independent ls CLI expectations, GNU adapter, and boundaries/failures.
- tests/test_ls_module.c — repeated ls calls, borrowed resources, unchanged argv, and injected failures.

## Planned
- docs/export_md.py — export selected Hermes history to Markdown.
- docs/AI_LOG.md — generated prompt history for publication.
- docs/CHANGES.md — brief decisions, changes, and test results.

## mkdir command
- docs/commands/mkdir.md — mkdir inventory, implemented contract, interruption/TDD evidence, and verification limits.
- docs/commands/mkdir-review.md — independent mkdir review, executed checks, readiness, and isolated-commit integration scope.
- logs/commands/mkdir-20261008-execution.txt — exact mkdir resume commands, observed outputs/statuses, and tested-source hashes.
- logs/commands/mkdir-20261008-interrupted.txt — preserved prior worker transcript, reference/negative/RED logs, and manual research.
- src/cmd_mkdir.c, src/cmd_mkdir.h — invocation-local directory creation, parents, and JSON specification.
- src/mkdir_main.c — thin standalone mkdir entry point.
- tests/test_mkdir.py — independent reference/project CLI, filesystem/mode, and forbidden-effect fixtures.
- tests/test_mkdir_module.c — mkdir reuse, borrowed-state, allocation cleanup, and injected fault tests.
