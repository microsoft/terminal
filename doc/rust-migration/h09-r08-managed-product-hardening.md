# H09 — R08 Settings / Control / TerminalApp hardening

H09 re-audits the R08 managed/product surface after H08 and moves only a product responsibility that is both portable and materially separable from XAML/WinRT: TerminalApp fuzzy matching.

The ownership rule remains unchanged: this migration is C++ to Rust, not C#/XAML to Rust. Lower-level Rust evidence is not double-counted as application/control parity, and Windows control surfaces are not shadow-implemented merely to improve coverage numbers.

## Starting point

The four managed/product suites contribute 296 source contracts:

| Suite | Source methods | Before H09 |
|---|---:|---|
| `unitSettingsModel` | 157 | Missing=157 |
| `unitControl` | 28 | Platform-only=28 |
| `terminalApp` | 52 | Missing=52 |
| `localTerminalApp` | 59 | Missing=38, UI-managed=21 |
| **Total** | **296** | **Missing=247, Platform-only=28, UI-managed=21** |

## A real TerminalApp Rust owner

`src/cascadia/ut_app/FzfTests.cpp` is not a XAML or COM suite. Its product implementation lives separately in `src/cascadia/fzf/fzf.cpp` and consists of portable fuzzy-search behavior:

- score, gap and consecutive-match rules;
- word-boundary, non-word, camel-case and digit bonuses;
- multi-term matching;
- dynamic-programming traceback and tie-breaking;
- case-insensitive Unicode matching;
- conversion of matched code-point positions back to inclusive UTF-16 runs.

H09 therefore adds the reusable `rust/terminal-app` crate and ports that matcher into `rust/terminal-app/src/fzf.rs`. The crate forbids unsafe code and is part of the normal workspace; it is product code, not a parity-only helper.

## FZF source contracts

The frozen Microsoft family contains 39 `TEST_METHOD` identities. Thirty-eight are active. One method is explicitly disabled by Microsoft:

- `German_CaseMisMatch_FoldResultsInMultipleCodePoints`

That test describes full Unicode case folding where one code point (`ß`) expands into multiple code points (`ss`). Microsoft marks it `Ignore=true`, and the current C++ implementation comments that this behavior does not pass. H09 deliberately does not claim it as Rust parity.

The other 38 methods now have one direct Rust witness each in:

`rust/terminal-app/tests/microsoft_fzf_contract.rs`

Those witnesses reproduce the Microsoft score/run vectors, including Russian, French, Greek, surrogate-pair/UTF-16, traceback, multiple-term, boundary and gap cases.

Result for `terminalApp`:

```text
Before H09
Missing 52

After H09
Exact   38
Missing 14
Total   52
```

The remaining 14 Missing methods are:

- 13 `JsonUtilsTests.cpp` contracts, whose TerminalApp JSON utility owner has not migrated;
- the one explicitly ignored FZF full-fold expansion contract.

## SettingsModel, Control and LocalTerminalApp remain honest

H09 does not manufacture movement elsewhere:

- `unitSettingsModel`: **157 Missing**. No Rust SettingsModel owner exists yet.
- `unitControl`: **28 Platform-only**. C++/WinRT construction, COM apartments, projected interfaces and Windows control/interactivity remain at the native boundary.
- `localTerminalApp`: **38 Missing + 21 UI-managed**. Command-line, filtered-command and settings integration still lack Rust application owners; `TabTests.cpp` continues to exercise genuine XAML/UI-thread orchestration.

Combined R08 managed/product result after H09:

```text
Exact          38
Missing       209
Platform-only  28
UI-managed     21
Total         296
```

## Fail-closed FZF audit

Method identity alone is insufficient for FZF because Microsoft could change a score formula, Unicode vector or traceback expectation without renaming a `TEST_METHOD`.

H09 therefore pins both audited C++ blobs:

- `src/cascadia/ut_app/FzfTests.cpp` — Microsoft vectors;
- `src/cascadia/fzf/fzf.cpp` — C++ product reference implementation.

`tools/rust/Test-RustTerminalAppFzfSourceMap.ps1` verifies:

- both source blobs still match the audited content;
- exactly 39 Microsoft FZF source methods remain;
- exactly 38 active methods have individual Exact ledger rows;
- the ignored full-fold method is not promoted;
- every Exact row has exactly one direct Rust test witness;
- every FZF Rust contract test maps back to one Exact Microsoft method;
- `terminal-app` retains `#![forbid(unsafe_code)]`.

Rust CI is also triggered directly by changes under `src/cascadia/fzf/**` or `src/cascadia/ut_app/FzfTests.cpp`, so source drift is detected immediately rather than waiting for another Rust change.

## Global snapshot

H09 moves exactly 38 contracts from Missing to Exact:

```text
Exact         202
Stronger       11
Partial       351
Platform-only  63
UI-managed     22
Missing       449
Total        1098
```

No other suite changes classification.

## Safety

- Microsoft C++ product changes: **0**
- Microsoft tests removed or weakened: **0**
- FFI changes: **0**
- XAML/managed changes: **0**
- Control/COM/WinRT boundaries falsely migrated: **0**
- lower-level Rust evidence double-counted: **0**
- certification gates relaxed: **0**

H09 does add one real platform-neutral Rust product owner because the FZF responsibility genuinely transfers cleanly from C++.

TAEF remains the Microsoft runtime certification oracle. The Rust witnesses establish ownership/equivalence for transferred behavior; they do not replace full Microsoft certification.
