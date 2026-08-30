# CPP-BASELINE-001

Baseline source commit:

`20588130d8ef2ba40eb56bdae88e04cce7fc5b5d`

Captured on 2026-08-19 before Rust product implementation.

## Wall-clock baseline

| Phase | Time |
|---|---:|
| C++ build | 00:09:27 |
| Unit-test run | 08:38:55 |
| Build + tests | 08:48:23 |

The legacy PowerShell wrapper completed with a successful PowerShell status even though TAEF reported failing and blocked tests. R00 therefore treats parsed TAEF summaries, not the wrapper's final `$?`, as the contract result.

## Unit-test baseline

| Suite | Total | Passed | Failed | Blocked | Skipped |
|---|---:|---:|---:|---:|---:|
| host | 5101 | 5095 | 5 | 0 | 1 |
| textBuffer | 13 | 13 | 0 | 0 | 0 |
| terminalCore | 54 | 54 | 0 | 0 | 0 |
| terminalApp | 51 | 51 | 0 | 0 | 0 |
| localTerminalApp | 76 | 0 | 0 | 76 | 0 |
| unitSettingsModel | 157 | 153 | 4 | 0 | 0 |
| unitControl | 29 | 29 | 0 | 0 | 0 |
| interactivityWin32 | 52 | 51 | 1 | 0 | 0 |
| terminal | 760 | 759 | 0 | 0 | 1 |
| adapter | 411 | 406 | 5 | 0 | 0 |
| types | 16 | 16 | 0 | 0 | 0 |
| til | 278 | 278 | 0 | 0 | 0 |
| **Total** | **6998** | **6905** | **15** | **76** | **2** |

`Not Run` was zero in every suite.

## Known baseline conditions

- `localTerminalApp` was blocked because TAEF could not locate the `Microsoft.VCLibs.140.00.Debug` AppX dependency in the local run.
- `terminal` is the R01 compatibility gate: 760 total, 759 passed, zero failed, zero blocked, one known skip.
- Existing failures are ceilings, not allowances to add new failures. A reduction is accepted; an increase is a regression.

The machine-readable contract is `tools/rust/contract-baseline.json`.
