# Microsoft test census for Rust parity

This is the source-level census used by the parallel Microsoft-to-Rust test-parity lane.

## Two complementary inventories

The Microsoft contract has two useful granularities and neither replaces the other:

- **1,098 source methods**: stable `TEST_METHOD` identities that can be discovered without building Microsoft C++.
- **6,998 runtime invocations**: the existing TAEF contract baseline after data-driven expansion, ignored/skipped behavior, and suite execution semantics.

The runtime suite remains the certification oracle. The source-method census is the fast drift detector and the unit used by the Rust equivalence ledger.

| Microsoft suite | Source methods | Runtime baseline |
|---|---:|---:|
| `host` | 331 | 5,101 |
| `til` | 199 | 278 |
| `unitSettingsModel` | 157 | 157 |
| `terminal` | 98 | 760 |
| `adapter` | 72 | 411 |
| `localTerminalApp` | 59 | 76 |
| `terminalCore` | 53 | 54 |
| `terminalApp` | 52 | 51 |
| `unitControl` | 28 | 29 |
| `interactivityWin32` | 19 | 52 |
| `types` | 16 | 16 |
| `textBuffer` | 14 | 13 |
| **Total** | **1,098** | **6,998** |

Source-method counts and runtime counts are intentionally not expected to match. A source method may expand into many data-driven runtime identities, while ignored/platform configuration and the TAEF metadata model can also make the relationship non-1:1.

## Files

- `tools/rust/microsoft-test-suites.json` defines the twelve source roots.
- `tools/rust/microsoft-test-source-census.json` freezes each suite's source-method count and SHA-256 identity fingerprint alongside its runtime baseline.
- `tools/rust/microsoft-rust-equivalence.json` is the machine-readable Rust evidence ledger.
- `tools/rust/Get-MicrosoftGlobalTestInventory.ps1` derives the full source inventory.
- `tools/rust/Test-MicrosoftGlobalTestInventory.ps1` validates the census and ledger in Rust CI.

## Drift rule

A Microsoft source contract may not be silently added, removed, or renamed. Any identity change alters its suite fingerprint and fails Rust CI. The change must first be classified in the equivalence ledger and then the frozen census may be deliberately updated.

The ledger starts conservatively: an unmapped source method is `Missing`. Existing detailed R01 evidence in `test-equivalence-matrix.md` is not discarded; it will be migrated into the machine-readable ledger as the parity increments reconcile R01/R02/R03 and then R04-R07. A conservative bootstrap avoids falsely claiming equivalence merely because a Rust test has a similar name.

## Next parity increment

Reconcile the complete R02 input surface (`inputTest.cpp`, `MouseInputTest.cpp`, and `kittyKeyboardProtocol.cpp`) against the existing Rust witnesses. Reuse existing Rust tests where they prove the Microsoft vector; add tests only for genuine gaps.
