# Rejected native-contract refactor

**Decision: do not pursue or ship this approach.** The user rejected the
amount of refactoring and the resulting review burden. This commit is an
archival snapshot of an experiment, and is intended to be immediately reverted.

The attempt moved profile, appearance, font and media state behind native
reference-counted APIs, added identity-preserving projected adapters, and
started moving loaders and UI consumers. In parallel, host summon policy,
command-line state and window-request transport moved to native types.
It introduced explicit DLL export roots, import-library dependencies, private
compatibility adapters and substantial test infrastructure.

Those changes spread across too many production surfaces. The native/projected
transition required enough ownership, collection, identity, serialization and
UI-binding machinery that the review and maintenance cost was unacceptable
for this build-performance investigation.

## What this snapshot does and does not establish

- It combines the stopped settings worktree with the latest parent transport
  implementation, including the window-request changes that had not yet been
  applied to the settings worktree.
- Intermediate native ABI tests, Model tests and executable/UI smoke runs
  passed. The settings worktree's last reported green gates were 11 native
  tests, 164 Model tests, a complete executable build and six-path UI smoke.
- Later loader/generator ownership edits and notification tests were not fully
  validated when work stopped. The final combined archival snapshot has not
  been built or validated and must not be treated as production-ready.
- A fresh pre-refactor executable rebuild took 784.956 seconds with the
  earlier accepted optimizations. There is **no measured full-refactor
  performance improvement**: the all-layer conversion was never completed.
- Earlier slice/PCH experiments and raw measurement scripts are included for
  historical context, not as evidence that this broad refactor was successful.

The accepted baseline is `a5263b044`, including the small wrapper, metadata
search-order and Model-PCH optimization commits. Reverting this archival commit
must restore that baseline, not revert those accepted optimizations.

No code, PR, issue or comment from this experiment was published. The stopped
child session and local build logs remain evidence, not ongoing implementation.
