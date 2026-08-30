# R05 — TerminalCore

R05 begins from the fully validated R04 checkpoint on `rust/main` and migrates the deterministic state and behavior currently owned by `src/cascadia/TerminalCore`.

## R05a — core key-state foundation

The first slice introduces a safe `terminal-core` workspace crate and ports `ControlKeyStates` without depending on Win32 headers, WinRT, COM, XAML, C++, or FFI.

The Rust representation deliberately preserves the numeric `KEY_EVENT_RECORD` flag values used by the NT console subsystem, plus Windows Terminal's two Windows-key extension bits. It also preserves the C++ queries for Shift, Alt, Ctrl, Windows, AltGr, and generic modifier state, including unknown-bit round-tripping.

## R05b — deterministic selection state

The second slice ports the platform-neutral selection state machine from `TerminalSelection.cpp`.

It adds row-major `BufferPoint` ordering compatible with the `til::point` comparisons used by TerminalCore, `SelectionInfo`, selection expansion and interaction enums, the exact `_PivotSelection` behavior (including equality targeting the start side), block-selection state, clearing, and Mark Mode endpoint switching/pivot updates. Unit tests cover forward/backward pivot crossing, pivot equality, inactive selection behavior, and each endpoint transition.

## R05c — buffer-backed selection expansion

The third slice connects `terminal-core` directly to the safe R04 `terminal-buffer` crate and ports the deterministic expansion path from `TerminalSelection.cpp`:

- `Line` expansion climbs to the complete forced-wrap chain and uses left/right-exclusive line anchors.
- `Word` expansion uses the R04 row delimiter classes and can cross row boundaries only when the preceding row is force-wrapped.
- Forward word expansion preserves the GH#5099 one-cell backoff before calculating the word end.
- Shift+Click overrides the active expansion mode for the operation, expands only the moving endpoint, and restores the opposite endpoint to the immutable pivot.
- Forward character Shift+Click advances one cell before pivoting so the clicked cell is included.
- Multi-click selection restores its pivot to the expanded selection start for future Shift+Click operations.

Tests cover wrapped-line expansion, delimiter-aware word expansion, cross-wrap words, forward character Shift+Click, and target-side-only word expansion.

## R05d — keyboard selection command mapping

The fourth slice ports `Terminal::ConvertKeyEventToUpdateSelectionParams` as a safe, platform-neutral mapping layer.

- Mark Mode permits selection movement without requiring Shift; outside Mark Mode, Shift is required.
- Alt suppresses selection movement exactly as in TerminalCore.
- Ctrl+Left/Right maps to word movement.
- Ctrl+Home/End maps to whole-buffer movement.
- Home/End and PageUp/PageDown map to viewport movement without Ctrl.
- Arrow keys map to character movement without Ctrl.
- The Windows virtual-key values used by this contract are represented locally, avoiding a Win32 dependency.

Tests cover every mapped direction/expansion family, Alt suppression, Mark Mode behavior, unsupported keys, and the important rule that Ctrl does not fall through to non-Ctrl commands.

## R05e — deterministic UpdateSelection movement

The fifth slice ports `Terminal::UpdateSelection` and the deterministic movement helpers used by keyboard selection.

- Mark Mode without Shift moves start, end, and pivot together while no endpoint has been explicitly anchored.
- Shift/Quick Edit movement targets the non-pivot endpoint and re-pivots safely when the moving endpoint crosses the immutable pivot.
- Character movement preserves right-exclusive row endpoints, saturates at the buffer origin/mutable bottom, and respects migrated glyph boundaries.
- Word movement uses the R04 delimiter classification and mirrors the "already at boundary, move to previous/next word" behavior.
- Viewport movement implements Home/End and PageUp/PageDown semantics with explicit viewport height and mutable-bottom geometry.
- Buffer movement implements Ctrl+Home/Ctrl+End semantics against the mutable viewport bottom.

The movement layer remains platform-neutral and requires no C++, Win32 headers, or FFI.

## Safety

`terminal-core` uses `#![forbid(unsafe_code)]`.

R05a–R05e add no product C++, no FFI, and no platform-specific dependency. The ordinary blocking gate is therefore workspace fmt, Clippy with `-D warnings`, Linux and Windows check/test, repository quality gates, and the TAEF harness self-test.

## Next slices

Continue with the remaining deterministic TerminalCore selection operations and copy/selection geometry that can reuse R04 directly. A C++ compatibility facade is deferred until a concrete boundary is required and then becomes subject to the relevant Microsoft C++ contract tests.
