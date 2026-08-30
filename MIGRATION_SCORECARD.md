# Rust migration scorecard

This scorecard is the evidence ledger for the Windows Terminal Rust migration track.

## Control

- Baseline: `CPP-BASELINE-001`
- Source commit: `20588130d8ef2ba40eb56bdae88e04cce7fc5b5d`
- C++ build: `00:09:27`
- C++ unit-test wall clock: `08:38:55`
- Baseline unit tests: `6998 total / 6905 passed / 15 failed / 76 blocked / 2 skipped`
- R01 parser contract: `760 total / 759 passed / 0 failed / 0 blocked / 1 skipped`

## Increment scorecard

| Increment | Component | Rust contract | Microsoft contract | Differential | Fast feedback | Unsafe Rust | Runtime | Memory |
|---|---|---|---|---|---|---|---|---|
| R00 | Migration infrastructure | workspace wired | baseline captured | n/a | pending CI measurement | 0 LOC | n/a | n/a |
| R01 | VT parser | pending | 759/760 executable baseline | pending | pending | pending | pending | pending |

## Rules

1. Microsoft contract counts must never regress from the recorded ceiling.
2. Differential mismatches block migration even if each implementation's own unit tests pass.
3. Unsafe Rust is measured and should remain concentrated in FFI/platform boundary crates.
4. Runtime and memory claims require measurements against the C++ control on comparable hardware/workloads.
5. Build/test latency is measured from source change to actionable test result; full compatibility and fast developer feedback are tracked separately.
