# Empty host — 2026-10-30

## Scope and decisions

- The user approved a host that starts without command packages. Show available
  local packages at interactive startup. Let humans and agents select tools.
- Keep catalog reading data-only. Load native code only after explicit selection.
- Use host controls `:available`, `:load`, `:loaded`, and `:quit`. These controls
  solve the first-package bootstrap; tool commands remain separate modules.
- Loading uses a trusted bundle in place for this process. It does not install
  files or save a selection. Explicit pkg operations can retain installed copies.
- Schema 3 stores real callback usage without an invented argument grammar.
  Schema 2 keeps its strict structured-metadata checks.
- Build the host and package collection separately. Ship no packages in the
  host release. Retain `build/main` as a separate legacy compatibility host.
- Keep the pipe sidequest unchanged. Its schema-2 test fixture now uses
  `build/compat-packages-abi-v1`; older immutable artifacts are preserved.
- Split package-format and host work between two workers. Parent owns build,
  release, demo, documentation, integration checks, and publication.

## Verification

- Workers observed failing tests before implementing selection and schema 3.
- Parent `make test` passed after adding the missing usage-writer dependency
  to the native transaction fault harness. The first attempt exposed that link
  failure; the failed log was retained in scratch.
- New suites passed: 14 empty-host tests, 9 catalog tests, 7 host-release tests,
  and the real-output catalog demo test. Empty-host ASan/UBSan also passed 14.
- Pipe checks passed 23 normal and 23 sanitizer tests, plus the full demo with
  real pauses. No pipe execution behavior changed.
- Real startup reported an empty loaded list. Explicit pwd and live hello worked.
  The demo verified that installed state is not loaded in a new process.
- The host symbol check found no command specs, Argtable parser, or legacy
  installer function. A forced default-build plan invoked no module builder.
- Review exposed a required-size check that rejected previous ABI-v1 hosts.
  The new optional callback must not increase the required prefix. Five ABI
  tests passed after the previous-host acceptance test first failed.
- The preliminary catalog was stale after review repairs. Preserve it; publish
  catalog 0.3.1 in a new directory. Expose catalog path/version in Make and demo
  options. The new collection and host release both passed identical rebuilds.
- Final parent `make test`, host sanitizers, strict scoped analysis, and pipe
  tests/demo passed. Final independent review passed with no blockers.

## Limits

Trusted native code is not a sandbox. Constructors can run during selection.
No downloads, PATH fallback, main-shell pipes, background jobs, or full job
control were added. Private wiki and course data are excluded from publication.
