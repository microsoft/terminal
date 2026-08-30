# Rust migration development strategy

This document defines the repeatable development loop for the Rust migration. It is intentionally independent of any one pull request so that changing branches or opening a continuation PR does not silently change the way the migration is developed or certified.

## Pull-request lifecycle

Active migration PRs stay **Draft while functionality is being written**.

Draft is a development mode, not a relaxation of product correctness:

- `rust-ci.yml` runs `cargo fmt --all` before the fast checks.
- the quality job runs `cargo clippy --fix --workspace --all-targets --locked --allow-dirty --allow-staged -- -D warnings` in incremental/Draft mode;
- Linux and Windows test jobs run rustfmt before `cargo check` and `cargo test`;
- the R08 `-RequireZero` functional-debt exit gate is reserved for an integration-ready PR;
- repository spelling is not paid on every synchronize event and is restored by `ready_for_review`.

These corrective CI steps operate in the runner checkout. They prevent mechanical fmt/Clippy drift from obscuring functional feedback during rapid development, but they do not rewrite the source branch. Before a PR becomes integration-ready, any remaining mechanical diff must be folded into the current functional increment or the final certification change rather than creating avoidable one-off repair history.

Syntax errors, type errors, failing tests and semantic regressions are not mechanical drift and cannot be waived by the preflight. They are corrected as soon as authoritative CI identifies them, preferably inside the same coherent functional batch when that preserves reviewability.

A migration PR is marked **Ready for review only when it is an exit candidate**. That transition intentionally restores the strict integration gates: rustfmt verification, Clippy with `-D warnings`, spelling, zero functional debt where required, boundary/stage contracts and final certification appropriate to the phase.

## Spelling policy

Spelling is a certification gate, not a per-synchronize development tax.

- cancellation, superseded runs and runner/infrastructure failures are not lexical failures;
- a genuine typo is fixed in source;
- a genuine new domain/API/product term is added to the appropriate `.github/actions/spelling/allow/*.txt` dictionary (or the equivalent check-spelling metadata) rather than repeatedly failing the build;
- `ready_for_review` must obtain a fresh successful spelling result before integration.

The goal is to keep spelling authoritative without making accepted technical vocabulary a recurring source of noise.

## Modified-sandwich strategy

Development alternates between **Missing** and **Partial** Microsoft contracts while staying on the same functional neighborhood whenever possible.

The purpose is end-to-end coverage, not score manipulation. A typical cycle is:

```text
Missing contract
    -> establish or extend the Rust product owner
    -> adjacent Partial contract
    -> harden the same owner/boundary
    -> adjacent Missing contract
    -> continue the vertical slice
```

Selection rules:

1. Prefer contracts that extend an owner or boundary touched by the previous work.
2. Alternate Missing and functional Partial work when an adjacent candidate exists; do not exhaust an easy Missing list while leaving the same end-to-end path Partial.
3. Favor vertical behavior that crosses parsing/model/fixup/projection boundaries over isolated test-count wins.
4. Promote a contract only with a real Rust owner or a direct witness proving that the existing Rust owner already satisfies it.
5. Never reclassify metadata merely to reduce Missing or Partial totals.
6. A surviving Partial must have explicit remaining behavior. Platform/language/API-shape/upstream-ignored boundaries are recorded as such rather than disguised as functional completion.

This is the migration "sandwich": new Rust ownership grows from one side while nearby partial ownership is hardened from the other, repeatedly closing complete functional slices instead of creating a wide front of disconnected ports.

## CI-amortized implementation policy

The primary unit of planning is **the largest coherent Microsoft functionality slice for which the marginal cost of adding the next adjacent behavior is lower than the expected cost of rediscovering that context and paying another full integration validation**.

The former 5–9 range is retained only as a decision checkpoint:

1. At roughly five cases, ask whether a complete functional unit already exists.
2. Between five and nine, actively search for the natural end of the current seam.
3. Nine is not a stop condition.
4. Continue to 10, 12, 17 or more cases when the remaining contracts substantially reuse the same Rust owner, data representation, parser/state machine, fixtures, Microsoft vectors, APIs, test infrastructure and already-loaded technical context.

Stop because a **real boundary** appears, not because a count was reached. Real boundaries include a new owner or architecture, a material platform dependency, a new state machine, a substantially different regression class, independent Microsoft research, a diff that is no longer reasonably diagnosable, or the natural completion of the seam/family.

For each adjacent group after the first checkpoint, ask:

> Is implementing these cases now substantially cheaper than rediscovering this context and paying another full CI validation later?

If yes, include them. In particular, when one common implementation unlocks several Microsoft tests at negligible marginal cost, all tests belonging to that behavior should be included even when the batch exceeds nine.

`Exact > Partial > Missing` remains the governing quality rule. Larger batches do not authorize weaker evidence, cosmetic metadata promotions, hidden debt or unrelated owners.

## Family-first planning

When entering a Microsoft source family:

1. census the complete family;
2. cluster contracts by owner/seam;
3. start with the highest-ROI coherent cluster;
4. after implementing that cluster, reevaluate the marginal cost of the remainder;
5. if the remaining family now shares the owner/abstractions already built, attempt to close the complete family in the same batch.

A source family must not be split merely to preserve a preferred test count. A family may still span multiple batches when the census demonstrates real ownership, architecture, platform or diagnostic boundaries.

## CI is a checkpoint, not the development metronome

Optimize for **Microsoft-certified functionality per full GitHub Actions cycle**, not for the probability that each tiny push is green.

Before publishing a batch, execute all cheap validation that is actually available in the working environment: targeted crate compilation/tests, new contract witnesses, neighboring owner tests, rustfmt/Clippy/check, and census/equivalence consistency. If a particular local capability is unavailable, do not pretend it ran; use the remaining cheap checks and let Actions be the authoritative integration checkpoint.

GitHub Actions should primarily answer:

> Does this coherent slice integrate correctly with the whole system?

It should not be used as a substitute for reasoning or as an interactive compiler for each small edit.

While CI is running, continue useful work that does not move the published ref: inspect the next family, read Microsoft sources, identify owners, prepare vectors or design the next slice. Avoid continuously publishing newer HEADs that turn useful compute into superseded/cancelled runs.

## Expected-compute and failure policy

Do not optimize for "probability this push is green." Optimize for **the expected number of complete CI rounds required to retire the functional debt**. One coherent, moderately more ambitious batch can be cheaper than several individually safer micro-batches when it amortizes the same discovery and integration fan-out.

When CI fails, classify the failure before changing scope:

1. infrastructure/runner;
2. mechanical formatting/lint;
3. localized new-test/implementation defect;
4. neighboring regression;
5. transversal architectural error.

Cases 1–3 normally remain inside the same batch. Split retroactively only when the failure demonstrates that independent responsibilities were actually mixed and separation materially improves diagnosis or ownership clarity. Do not microfragment preventively out of fear of a possible failure.

## Commit, publication and PR discipline

- `rust/main` is the compact migration baseline (`Initial rust migration effort`).
- Preserve logical/reviewable commits when they improve traceability, but do not publish every microcommit merely to obtain CI feedback.
- The CI-relevant atomic event is the **visible branch update**. When tooling permits, prepare one or more coherent commits without moving the remote PR head, then publish the validated batch once at the CI checkpoint.
- Mechanical construction commits may be compacted before publication when doing so improves reviewability without losing useful history.
- Never sacrifice traceability merely to reduce CI, and never use commit granularity as an excuse to trigger avoidable full fan-out.
- Feature branches are validated by their pull-request checkpoint; direct `push` validation is reserved for `rust/main` so the same feature SHA does not pay duplicate `push` and `pull_request` fan-out.
- The active PR description is a living Microsoft-style review artifact: Summary, References, Detailed Description, Validation Steps and Checklist are updated as certified slices accumulate.
- CI queued/pending is a research/design window; a real failure with authoritative logs takes priority over publishing a wider head.
- Do not merge, start the next migration phase or mark Ready for review merely because individual batches are green.

## Iteration accounting

Every completed iteration records at least:

- Microsoft source family and cases examined;
- cases implemented;
- `Missing -> Exact`, `Missing -> Partial`, and `Partial -> Exact`;
- total functional debt retired;
- new owners and reused owners;
- tests/witnesses added;
- whether the complete family closed;
- the real boundary that ended the batch;
- the next excluded case and the marginal cost/risk that justified excluding it;
- visible remote branch updates / full CI checkpoints used to obtain the delivery.

The last metric is deliberate: `12 Exact / 1 CI checkpoint` communicates operational efficiency that `12 Exact` alone does not.

## R08 exit discipline

R08 is not exit-ready until known functional debt has been eliminated or explicitly proven to be a genuine non-Rust/product boundary. The final integration-ready head must pass the strict gates after development mode is turned off. Draft-mode speed and CI amortization shorten the path to that proof; they do not substitute for it.
