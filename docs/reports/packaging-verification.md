# Packaging delivery verification

## Scope

The selected baseline is the current Week 1–2 shell, not the older committed
command set. It includes the existing `rmdir` work and retires `env`, `echo`,
`date`, `printf`, and `sleep`. These baseline decisions are not new package
features. Unrelated working changes, private wiki data, and course materials
are excluded from publication.

## Verified runtime commit

Commit: `6f579356abd67ceb1f7123db53f55896c247b967`.
Tested tree: `9b457a89b41217ad1dd8572dad968bf6734ca555`.
Branch: `feat/aishell-packaging`. Parent read back the matching remote hash.
No merge was made.

The parent exported the exact staged blobs into a new scratch checkout.
Only the existing external Argtable3 and CUnit dependency directories were
linked into that checkout. No project binaries or generated packages were
copied. New project builds and package outputs were created there.

Results on that tree:

| Check | Exit status |
| --- | --- |
| `make test all` with the project warning flags plus `-Werror` | 0 |
| `make test-shell-release release-shell` | 0 |
| `make lint` | 0 |
| `clang-format --dry-run --Werror` on changed C sources and headers | 0 |
| `make format-check` on the whole repository | 2 |

The global formatting failure is retained in unchanged legacy files. No broad
formatting change was made to hide it. Full logs and the exact-index manifest
are in `$TMPDIR/packaging-code-index-checks` and
`$TMPDIR/packaging-code-index-index-manifest.json` in this session.

## Independent review and safety evidence

Separate reviewers exercised hello, metadata generation, package building,
installed-state validation, physical source/root separation, partial removal,
shell discovery and execution, and relocated release behavior. Review defects
were reproduced and corrected. Reports preserve failures and test-harness
corrections instead of rewriting them as successful TDD cycles.

- [Hello and shared metadata](hello-packaging.md)
- [Package builder and immutable publication](package-build.md)
- [Package backend and fault checks](pkg-implementation.md)
- [Shell integration and hermetic tests](package-shell.md)
- [Full-shell release and exact-binary documentation](shell-release.md)

Normal and ASan/UBSan checks passed in the reviewed slices. The prebuilt
Argtable dependency was not sanitizer-instrumented. Negative controls detected
selected unsafe changes. This is not an exhaustive allocation/syscall matrix
or a hostile same-user race proof.

## Demo and documentation

The [actual demo](../../demo/PACKAGING.md) records 17 commands and 21 checks.
Its script validates statuses, catalog state, removal, cleanup, and execution
from a relocated shell release. The [brief guide](../commands/pkg.md) describes
package operations and maintenance.

The private wiki database contains `_drafts/aishell-packaging.md`, revision 1.
The parent compared its body and metadata with the reviewed inputs. Index and
log readback passed. Wiki validation returned no errors or warnings. It is a
draft for human review, not a certified ASD-STE100 document. It is not included
in Git or the release artifact.

## Clean-checkout dependency setup

[Build and test instructions](../TESTING.md) describe pinned Argtable3 and
CUnit sources and the new bootstrap helper. Its owner built both dependencies
from downloaded, checksum-checked sources in isolated prefixes. Strict full
project and release tests passed there. Independent review repeated both real
dependency builds with checksum-matching cached source archives, checked 22
safety cases, and passed the strict full-project and release checks. The demo
rerun and current documentation link checks also passed. Evidence is in
`$TMPDIR/bootstrap-independent-review`. A fresh network download was not part
of that independent rerun. Final documentation-index verification and remote
readback follow the scoped documentation commit.

## Operating limits

Use trusted local packages. There is no sandbox, signature verification,
remote package service, dependency solver, archive install, automatic upgrade,
or shell self-update. Removal and remove-then-install updates can be partial.
Install publication is a namespace boundary, not a power-loss durability claim.

The release is Linux x86-64 ELF with the documented loader/libc requirements.
A GLIBC symbol requirement is not proof of compatibility with every older OS.
Existing builtins have not all adopted structured argument metadata. Their
release documentation comes from actual help callbacks. The command catalog
is discovery metadata, not a complete LLM input schema.
