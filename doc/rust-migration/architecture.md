# Rust migration architecture

This document defines the migration track in `ChicoDotNet/terminal`. The goal is not a big-bang rewrite. The goal is a verifiable, incremental Rust implementation that preserves Windows Terminal behavior while making each migrated component independently testable.

The repeatable development lifecycle — Draft CI preflight, spelling policy, commit discipline, queued-CI writing windows, and the modified-sandwich strategy that alternates Missing and Partial work — is part of the migration contract and is defined in [`development-strategy.md`](development-strategy.md).

## Principles

1. **Microsoft C++ remains the oracle until a Rust component proves equivalence.**
2. **Fast Rust feedback is the normal development loop.** Microsoft/TAEF tests are the contractual compatibility gate.
3. **Migrate vertical slices.** Do not port an entire foundational C++ library merely because the current build groups unrelated responsibilities together.
4. **Keep unsafe code at explicit FFI boundaries.** Safe implementation crates should forbid unsafe code.
5. **No product C++ is removed in R00.** Infrastructure and evidence come first.
6. **Known upstream/baseline failures are recorded rather than silently normalized.** New failures are regressions.
7. **Performance and memory are measured, not assumed.**
8. **Managed UI is not migration debt.** The migration target is C++ to Rust, not C# or XAML to Rust. Existing C# that naturally owns XAML code-behind, bindings, view models, or managed UI orchestration remains C# unless a concrete product reason requires otherwise.
9. **Compatibility gates are selected by evidence, not convenience.** Rust tests are the fast inner loop. Microsoft tests remain blocking wherever the equivalence matrix says Rust coverage is partial, platform-only, boundary-sensitive, or missing. The full Microsoft suite remains a stage/final certification gate rather than a default per-commit tax once narrower equivalence has been demonstrated.

## Migration order

| Increment | Scope | Exit condition |
|---|---|---|
| R00 | Workspace, CI, TAEF contract harness, baseline, scorecard | Fast Rust CI works and TAEF output is evaluated independently of the legacy wrapper exit status |
| R01 | VT parser: Base64, state machine, output/input engines | Differential corpus agrees and Microsoft `terminal` suite does not regress |
| R02 | Terminal input plus required pure types | Input contracts and differential tests agree |
| R03 | Adapter/dispatch/Sixel | Adapter contract agrees |
| R04 | TextBuffer/TIL/pure foundational types | Foundational suites agree |
| R05 | TerminalCore | Core suite agrees |
| R06 | Host/server/interactivity/ConPTY | Host and ConPTY contracts agree |
| R07 | Renderer | Rendering acceptance/performance evidence agrees |
| R08 | WinRT/COM/XAML/settings/control/UI and product integration | Product-level acceptance agrees; existing managed XAML ownership remains managed where appropriate |
| R09 | Compatibility façade removal and C++ cleanup | Remaining C++ is intentional platform boundary or removed |

## FFI shape

```text
Existing C++ code/tests
        |
        v
C++ compatibility façade
        |
        | C ABI
        v
terminal-*-ffi
        |
        | safe Rust API
        v
terminal-*
```

Rust ABI is not exposed directly to C++. The C ABI should use narrow, explicit ownership rules, opaque handles, and byte/slice-oriented buffers where practical.

For R08 managed UI surfaces, the intended ownership can instead be:

```text
XAML
  |
  v
existing C# code-behind / bindings / view models
  |
  | managed interop where required
  v
narrow Rust product boundary
  |
  v
safe Rust semantic crates
```

This is not a mandate to introduce C# where Windows Terminal does not already use it. It is a rule against rewriting healthy managed UI code merely to increase the Rust percentage. Residual C++ is migration debt only when it still owns behavior that belongs in the migrated Rust semantic layer or when it is a removable compatibility façade.

## R08 language ownership

R08 moves behavior out of C++ while preserving the most appropriate owner for each layer:

- **Rust:** deterministic domain logic, settings semantics, control state, input interpretation, lifecycle state machines, renderer/control policy, and product behavior that does not require managed UI ownership.
- **C#:** existing XAML code-behind, bindings, view models, and managed UI orchestration.
- **XAML:** presentation and declarative UI.
- **C++/WinRT/COM/Win32:** only platform/ABI ownership that is still required during R08. Unavoidable unsafe Rust remains confined to narrow FFI code.
- **R09:** removes compatibility façades and residual C++ that no longer has an intentional platform role.

## Test equivalence and CI tiers

`doc/rust-migration/test-equivalence-matrix.md` is the evidence ledger for deciding which Microsoft tests remain necessary at each boundary.

Coverage classifications are:

- **Exact:** the Rust contract covers the same relevant behavior.
- **Stronger:** the Rust contract covers the Microsoft case plus additional vectors or invariants.
- **Partial:** Rust covers only part of the behavior; the Microsoft test remains blocking.
- **Platform-only:** the behavior requires Windows/COM/WinRT/GDI/DWrite/DX or another platform surface; the Microsoft test remains blocking.
- **UI-managed:** the responsibility correctly belongs to C#/XAML and is not a Rust migration target.
- **Missing:** no adequate migrated equivalent exists yet; the Microsoft test remains blocking.

CI tiers are:

1. **Fast:** every change — Rust fmt, Clippy with `-D warnings`, workspace check/test on Linux and Windows, repository quality/spelling, and TAEF harness self-test.
2. **Boundary:** when C++/FFI/platform boundaries change — the Microsoft tests mapped to that boundary and every matrix row still classified Partial, Platform-only, or Missing for the affected area.
3. **Stage:** before an R07/R08 merge — the accumulated Microsoft contracts for the stage that have not been proven Exact/Stronger by the matrix.
4. **Full certification:** the complete Microsoft Terminal suite at the R08 exit and again during R09 final differential/cleanup validation.

No tier may be skipped merely to reduce runtime. A test can leave the per-commit boundary set only after its equivalence evidence is recorded in the matrix.

## R01 target

The first production slice is `src/terminal/parser`. The existing project already isolates parser behavior into a static library and has a strong TAEF contract. R01 begins with Base64 because it is small and deterministic, then moves through the state machine and engines before connecting the C++ façade to Rust.

The intended proof is stronger than "the Rust tests pass":

```text
same VT corpus
   +--> C++ parser --> observation A
   |
   +--> Rust parser -> observation B

A == B
```

Only after differential equality is established does the Microsoft `terminal` suite become the final compatibility gate for the slice.
