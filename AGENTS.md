## Session startup
- Follow [session lookup and registration](docs/SESSION_LOOKUP.md) on assignment-session startup; this authorizes its lookup and registration.
- Keep AGENTS.md small; put additional procedures in separate files and reference them here.
- For assignment questions, follow [local data lookup](docs/DATA_LOOKUP.md); query data.sqlite for relevant excerpts before rereading whole sources.

## Collaboration
- By default, the human writes code and tests; the AI discusses, reviews, and suggests. Upon a user request to edit any file, the AI must first confirm the scope and obtain approval before editing.
- Provide code examples only when requested.
- Run commands only when the human requests execution.
- After requested edits, state what changed and where to inspect it; for tests, provide the exact command to run them. Keep this report brief.
- Keep replies, prompts, explanations, and documentation very brief unless the human requests more detail.
- MUST NOT make things up; MUST specify if inferred or speculating.

## Procedures — read when relevant
- Before editing C source or headers: [coding rules](docs/CODING.md).
- Before planning or design: [design](docs/DESIGN.md).
- Before implementation or test development: [TDD](docs/TDD.md).
- Before testing, verification, or delivery: [verification](docs/VERIFY.md).
- Before documentation registry changes, logging, export, or publication: [AI logging](docs/AI_LOGGING.md).

