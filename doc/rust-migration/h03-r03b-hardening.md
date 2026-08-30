# H03 — R03-B adapter hardening

H03 audits the remaining 26 `adapterTest.cpp` source contracts after H02, using the same downstream-observation rule: preserving an `OutputAction` at the Rust boundary is useful evidence, but it is not full equivalence when Microsoft asserts a concrete `TextBuffer`, renderer, parser, `TerminalInput`, host, or response-formatting effect.

## Scope and classification

The R03-B source slice contains 26 methods:

- Exact: 3
- Partial: 23
- Missing: 0

The three Exact contracts remain justified after source-level re-audit:

- `ScrollMarginsTest` — Rust owns the complete top/bottom margin validation, defaulting, invalid-range behavior, and full-height clearing semantics asserted by Microsoft.
- `MacroDefinitions` — Microsoft DECDMAC strings are parsed through the Rust state machine into `MacroBuffer`, covering text/hex encoding, defaults, replacement, repeats, control filtering, cancellation, and encoded controls.
- `PageMovementTests` — `AdapterDispatch` and `PageManager` materially own PPA/PPR/PPB/NP/PP movement, clamping, cursor behavior, and DECPCCM active/visible page coupling.

H03 deliberately makes no new `Partial -> Exact` promotion. The remaining 23 contracts still contain at least one Microsoft-observed effect that is not wired through the Rust adapter today.

## Exhaustive boundary hardening

Several R03-B witnesses were previously representative rather than source-complete. H03 closes those input-vector gaps while retaining `Partial` where the downstream effect is not migrated.

### OSC 4 palette reports

`Osc4ColorPaletteReportTests` queries all sixteen VT525 palette entries in Microsoft. The Rust boundary witness now covers indices `0..=15`, rather than four representative indices.

Renderer lookup and formatted OSC response generation are still external, so the contract remains Partial.

### Xterm 256-color rendition

`Xterm256ColorTest` contains five source operations. The Rust witness now retains all five ordered indexed-color vectors, including the final foreground change back to bright red.

`TextAttribute` mutation remains deferred.

### Extended-color default parameters

`XtermExtendedColorDefaultParameterTest` has seven source cases. H03 covers all seven parameter shapes, including the previously absent out-of-range RGB vector `38;2;283;182;123`.

The contract remains Partial because Microsoft also asserts application/defaulting/rejection behavior on the live attributes.

### Extended-color subparameters

`XtermExtendedSubParameterColorTest` has eight colon-delimited source shapes. H03 parses and checks all eight at the Rust parser/adapter boundary, including:

- omitted indexed value;
- explicit default indexed value;
- omitted RGB components;
- non-empty color-space ID;
- out-of-range RGB component;
- out-of-range indexed color.

The parser shape is now source-complete; color validation and `TextAttribute` mutation are still downstream debt.

### Color-table mutation domain

`SetColorTableValue` loops over every palette index `0..255`. The Rust boundary witness now does the same instead of checking only domain edges.

Concrete renderer color-table mutation remains outside `AdaptDispatchCore`, so this is still Partial.

### Soft-font parameter families

The DECDLD witness now covers matrix-size, explicit-size, font-set, and usage families. The Microsoft source contract is fundamentally about `FontBuffer` cell-size inference and bitmap-derived sizing, which is not yet ported; H03 therefore does not manufacture dozens of boundary-only cases as a substitute for that missing algorithm.

## TerminalInput ownership versus adapter wiring

Four Microsoft R03-B contracts mutate `TerminalInput` through `AdaptDispatch`:

- `CursorKeysModeTest`
- `KeypadModeTest`
- `TestMouseModes`
- `SendC1ControlTest`

Rust already owns all corresponding input states. H03 adds direct owner witnesses proving set/reset behavior for cursor-key mode, keypad mode, all six mouse-related modes used by the Microsoft source, and SendC1 state.

However, `terminal-adapter` does not currently depend on or own a `TerminalInput` instance. The adapter actions are still deferred rather than wired into that owner. These four source contracts therefore correctly remain Partial: the remaining debt is the adapter-to-input connection, not the input-state implementation.

## Other deliberate Partial contracts

The remaining Partial rows continue to have concrete downstream gaps:

- tabulation and cursor-information reports require state storage and exact DCS serialization;
- ANSI mode and C1 toggling require dispatch-driven mutation of the live parser plus code-page behavior;
- cursor blinking and line feed require live `TextBuffer`/host effects;
- console title and menu completions require external UI/host calls;
- user-preference charset requests require terminal-output state and response generation;
- macro invocation requires recursive reinjection through the live parser and resulting buffer output;
- window manipulation requires live dimensions and exact response formatting;
- SendC1 report behavior requires adapter-to-input/output integration;
- renderer-backed color/resource reports require actual renderer state and formatted replies.

## Safety

H03 changes tests and documentation only.

- Product Rust changed: 0
- Product C++ changed: 0
- FFI changed: 0
- managed/XAML changed: 0
- Microsoft tests removed or weakened: 0
- certification gates relaxed: 0

## Expected coverage

H03 improves evidence quality without changing the global coverage-class totals. The expected adapter classification remains:

```text
adapter=72; runtime=411; Exact=19, Partial=52, Platform-only=1
```

The expected global classification remains:

```text
Exact         127
Stronger       11
Partial       383
Platform-only  63
UI-managed     22
Missing       492
Total        1098
```

Final CI is authoritative for these counts and for witness integrity.
