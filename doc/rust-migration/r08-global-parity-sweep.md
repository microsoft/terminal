# R08 global parity sweep and merge readiness

Delivery 11 converts the remaining parity work from a collection of stage-local ledgers into a single machine-checked global picture, and synchronizes the parity lane with the current functional R08 branch before final certification.

## Merge synchronization

Before the sweep, `rust/test-parity-census` had diverged from `rust/r08-product-integration`: the parity lane was 66 commits ahead and two functional commits behind. The only functional-side file delta was `rust/terminal-input/tests/microsoft_terminal_input_surface_contract.rs`.

The functional and parity blobs for that file were byte-identical, so the branch histories were joined with merge commit `45ebef38ae34140d8c650f338c5760f2302f7a3d` without any content resolution or product change.

Immediately after the synchronization:

```text
base:  rust/r08-product-integration @ c587a6a3b2f0f16b581852c30d8cf0dfbcc06a49
head:  rust/test-parity-census
state: ahead
behind_by: 0
merge_base: c587a6a3b2f0f16b581852c30d8cf0dfbcc06a49
```

This makes the functional R08 head an ancestor of the parity lane rather than merely relying on GitHub's synthetic PR merge ref.

## R01 machine-ledger reconciliation

The largest false-negative in the global ledger was the `terminal` suite. The historical equivalence matrix already contained audited R01 evidence, but only the two Base64 rows had been imported into the machine ledger. As a result, the global census incorrectly displayed 96 `Missing` parser contracts.

Delivery 11 imports that evidence through `tools/rust/microsoft-rust-equivalence-r01.json`:

- Base64: existing 1 Exact + 1 Stronger.
- StateMachine: 7 Exact source methods.
- InputEngine: 25 source methods = 11 Exact, 10 Stronger, 3 Platform-only, 1 Partial.
- OutputEngine: all 64 methods receive a conservative source-family `Partial` rule backed by the dedicated Rust Microsoft contract suites. This deliberately avoids mass-promoting every OutputEngine method to Exact merely because older prose records stronger row-level evidence.

Final `terminal` result:

```text
terminal=98; runtime=760
Exact=19
Stronger=11
Partial=65
Platform-only=3
Missing=0
```

The global gate now treats both `terminal` and `adapter` as reconciled stages. A newly introduced Microsoft source method in either suite cannot silently fall through to the default `Missing` classification after a census update.

## Global source coverage

The complete frozen Microsoft source surface remains 1,098 source methods across 12 suites, expanding to 6,998 runtime TAEF invocations.

Delivery 11 freezes the following global distribution:

```text
Exact=64
Stronger=11
Partial=447
Platform-only=62
UI-managed=22
Missing=492
Total=1098
```

The remaining 492 `Missing` contracts are no longer parser/adapter evidence debt. They are concentrated in later or deliberately deferred product responsibilities:

| Suite | Missing | Interpretation |
|---|---:|---|
| `host` | 210 | Functional/native host migration backlog |
| `unitSettingsModel` | 157 | Settings model/product semantics not yet owned by Rust |
| `terminalApp` | 52 | Native C++ FZF/JSON/product utility behavior |
| `localTerminalApp` | 38 | Native command-line/settings product behavior outside XAML-owned `TabTests` |
| `til` | 28 | Foundation semantics not yet represented by a complete Rust owner |
| `terminalCore` | 4 | Residual core functionality gaps |
| `types` | 2 | Residual foundational type gaps |
| `textBuffer` | 1 | Residual TextBuffer functionality gap |
| **Total** | **492** | **Functional/deferred ownership backlog, not unclassified test inventory** |

The 447 `Partial` rows are explicit incomplete-equivalence contracts. They remain in the appropriate Microsoft boundary/stage gates and full certification; Delivery 11 does not relabel them merely to improve the score.

## Global Rust witness gate

`tools/rust/Test-RustGlobalWitnesses.ps1` closes a different class of parity debt: stale or fictional witness references.

For every `Exact`, `Stronger`, or `Partial` entry/source rule across the main ledger and all overlays, the gate now requires:

1. at least one `rustWitnesses` reference;
2. every `file:` witness to resolve to an existing repository file;
3. every named witness leaf to exist somewhere in the Rust source tree.

The first complete run passed with:

```text
semantic contracts/rules=152
witness references=234
unique witnesses=198
Rust files scanned=97
missing witness references=0
```

This does not claim that a name match alone proves semantic equivalence; semantic classification still comes from the audited stage ledgers. It does guarantee that machine-ledger evidence cannot silently point to a deleted or misspelled Rust witness.

## Merge-readiness implications

Delivery 11 establishes the conditions needed for the final certification increment:

- functional R08 is an ancestor of the parity lane at the synchronization checkpoint;
- R01-R03 have zero `Missing` source contracts;
- all 1,098 Microsoft source methods are classified by the global census;
- global coverage totals are frozen and CI-checked;
- semantic Rust witness references are machine-checked and currently all resolve;
- no Microsoft runtime baseline, source fingerprint, boundary gate, or certification oracle was weakened;
- no product C++, product Rust, FFI, XAML, or managed implementation was changed by this sweep.

## Delivery 12 target

The final increment should be certification rather than another bookkeeping pass:

1. re-check the latest functional R08 head and merge/synchronize it if it moved;
2. require zero branch lag before integration;
3. run the complete fast Rust matrix and all parity/source-map gates;
4. run the full Microsoft certification oracle where the available CI environment supports it;
5. distinguish any remaining `Missing` that blocks the declared R08 migration scope from backlog intentionally deferred to later functional stages;
6. only then move PR #24 out of draft and merge it into the functional branch.
