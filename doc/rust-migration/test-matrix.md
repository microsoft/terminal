# Test matrix

R00 separates fast development feedback from compatibility proof.

## Gate 1: Rust Fast

Workflow: `.github/workflows/rust-ci.yml`

Runs on Rust changes. It validates:

- `cargo fmt --check`
- `cargo clippy --workspace --all-targets --locked -- -D warnings`
- `cargo check --workspace --all-targets --locked`
- `cargo test --workspace --all-targets --locked`
- the TAEF result-parser self-test

Rust tests run on Linux and Windows. The safe implementation crate forbids unsafe code; FFI code is isolated in a separate crate.

## Gate 2: Differential

Introduced in R01.

For the same generated or recorded VT inputs, the C++ and Rust implementations must produce the same normalized observations. Differential failures are product regressions even when independent unit tests pass.

## Gate 3: Microsoft contract

Workflow: `.github/workflows/rust-contract.yml`

R00 initially runs the Microsoft `terminal` suite because R01 targets the VT parser. The suite is judged by parsed TAEF counts against `tools/rust/contract-baseline.json`, not by the legacy wrapper's final PowerShell status.

The contract workflow uses GitHub-hosted Windows Server 2025 with Visual Studio 2026 and keeps TAEF evidence as an artifact.

## Gate 4: Full compatibility

Not enabled as a GitHub Actions matrix in R00.

First run:

```powershell
./tools/rust/Measure-ContractSuites.ps1
```

against an already built Debug/x64 tree. The script records each of the 12 unit suites separately. Those timings will determine the eventual matrix/sharding strategy rather than guessing how the 8h38m baseline is distributed.

The source suite taxonomy is:

- host
- textBuffer
- terminalCore
- terminalApp
- localTerminalApp
- unitSettingsModel
- unitControl
- interactivityWin32
- terminal
- adapter
- types
- til

Functional suites (`feature`, `uia`, `winconpty`) remain outside the R00 unit-contract matrix and will be added when the migration reaches their boundaries.
