# Current session lookup and registration

1. Read `HERMES_SESSION_ID` and `HERMES_HOME` from the runtime environment; do not dump unrelated environment variables.
2. Call `session_search` with that exact session ID. Verify the returned ID and conversation match this session, and retrieve its title. Never substitute the latest session or search unrelated history.
3. If the ID is missing, lookup fails, or the title cannot be verified, ask the human. Do not guess.
4. Read `docs/sessions.txt`. Create it if missing; otherwise preserve existing entries. If the verified ID is absent, append a title comment followed by the ID, one ID per line. Never add duplicates.
5. Report the verified ID, title, and whether registration changed the file.

AGENTS.md authorizes this lookup and registration on assignment-session startup. Register only the current assignment session; explicit human exclusions take precedence. This does not authorize other file edits, exports, publication, or changes to Hermes history.
