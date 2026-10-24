## Session startup
- Follow [session lookup and registration](docs/SESSION_LOOKUP.md) on assignment-session startup; this authorizes its lookup and registration.
- Keep AGENTS.md small; put additional procedures in separate files and reference them here.
- Do not read AI_LOG unless the user explicitly requests it.
- For assignment questions, follow [local data lookup](docs/DATA_LOOKUP.md); query docs/data.sqlite before rereading sources. See [source catalog](data/CATALOG.md) for chapters 01/02; the curated wiki is separate.

## Project identity
- Use GitHub account lk-swe-lab for this project.
- Git author: lk-swe-lab <314523841+lk-swe-lab@users.noreply.github.com>.
- AI commit attribution: [Hermes, <model>, <reasoning>]; no AI author field.
- Use the actual model and reasoning level; write unknown if unavailable.

## Collaboration
- By default, the human writes code and tests; the AI discusses, reviews, and suggests. Upon a user request to edit any file, the AI must first confirm the scope and obtain approval before editing.
- Provide code examples only when requested.
- Run commands only when the human requests execution.
- After requested edits, state what changed and where to inspect it; for tests, provide the exact command to run them. Keep this report brief.
- Keep replies, prompts, explanations, and documentation very brief unless the human requests more detail.
- MUST NOT make things up; MUST specify if inferred or speculating.

## Procedures — read when relevant
- Before adding or refactoring a command: [add-command workflow](docs/ADDING_COMMANDS.md) — research/discuss/keep/plan, approval gates, TDD, and verification.
- Before designing commands: [command anatomy](docs/COMMAND_ANATOMY.md) — module specification and shared parsing/help.
- Before using command APIs: [command API](docs/COMMAND_API.md) — cmd_spec_t/registry, argtable3, and POSIX file I/O with official references.
- Before editing C source or headers: [coding rules](docs/CODING.md).
- Before planning or design: [design](docs/DESIGN.md).
- Before implementation or test development: [TDD](docs/TDD.md).
- Before testing, verification, or delivery: [verification](docs/VERIFY.md).
- Before documentation registry changes, logging, export, or publication: [AI logging](docs/AI_LOGGING.md).

