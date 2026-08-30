# H05 — R05 TerminalCore hardening

H05 re-audits the frozen Microsoft `UnitTests_TerminalCore` surface after the later Rust migration stages landed.

The rule remains strict: a parser action, helper resemblance, or test-only abstraction is not enough for `Exact`. The Rust witness must reproduce the Microsoft source method's material observable through a real migrated owner.

## Frozen source surface

`terminalCore` remains:

- 53 distinct Microsoft `TEST_METHOD` identities
- 54 runtime TAEF invocations

H05 changes no Microsoft source census.

## H05 distribution

```text
Before H05
Exact           9
Partial        32
Missing         4
Platform-only   7
UI-managed      1
Total          53

After H05
Exact          15
Partial        26
Missing         4
Platform-only   7
UI-managed      1
Total          53
```

Six source methods move from `Partial` to `Exact`. No `Missing` row is hidden or converted merely to improve the number.

## InputTest remains Partial — CI found a real discrepancy

Microsoft `InputTest.cpp` contains two methods:

- `AltShiftKey`
- `InvalidKeyEvent`

Neither is promoted in H05.

`AltShiftKey` exercises `Terminal::SendCharEvent` and expects Alt+`a` / Alt+Shift+`A` to produce ESC-prefixed character output. Rust `TerminalInput` does not yet expose that aggregate char-event path with the same observable.

H05 also directly attempted the Microsoft `InvalidKeyEvent` vectors: virtual keys `0` and `255`, both with scan code `123`, should be unhandled. The first CI pass found that Rust emits a NUL (`"\0"`) for at least one of those vectors instead of emitting no output. The candidate Exact witness was therefore removed and the source family remains `Partial`.

This is intentional hardening behavior: CI evidence overrides the desired coverage number.

## Six Exact selection promotions

H05 adds direct Microsoft-derived witnesses for:

- `DoubleClickDrag_Right`
- `DoubleClickDrag_Left`
- `TripleClick_GeneralCase`
- `TripleClickDrag_Horizontal`
- `TripleClickDrag_Vertical`
- `ShiftClick`

These use the existing product-owned `SelectionState` and `TextBuffer` implementations. No new selection product abstraction is introduced.

The `ShiftClick` witness reproduces the complete eight-step Microsoft sequence: initial word selection, Shift+Click Char, Shift+DoubleClick Word, Shift+TripleClick Line, return to Word, drag past the next word, drag back, and drag within the same word. The important observable is that the expansion mode persists correctly between explicit Shift+multi-click changes and later ordinary drags.

## Remaining selection debt

The following source methods remain `Partial` for concrete reasons:

- `OverflowTests`: aggregate viewport/scrollback ownership is not represented by `terminal-core` alone.
- `SelectFromOutofBounds`: Microsoft clamps a right-overflow anchor to `RightExclusive()`; Rust's current cell clamp still resolves ordinary anchor coordinates to `width - 1`.
- `SelectToOutOfBounds`: same right-exclusive endpoint discrepancy for a moving selection end.
- `SelectAreaAfterScroll`: requires viewport-relative input coordinates translated through content/user scroll state.
- `SelectWideGlyph_Trailing`: Microsoft repairs a trailing-cell anchor to the leading cell and extends the endpoint through the complete wide glyph; current Rust rendering helpers repair marker positions but the selection anchor/span itself does not yet reproduce that source observable.
- `SelectWideGlyphsInBoxSelection`: Microsoft repairs wide-glyph boundaries independently on each block-selection row; current `selection_spans` emits uniform horizontal bounds.

H05 therefore records the known `RightExclusive()` discrepancy instead of introducing a buffer-aware anchor API solely to make the parity ledger greener. That discrepancy is product migration backlog and should be fixed where viewport/selection ownership is completed.

## Four honest Missing contracts remain

No H05 change is made to:

- `ScreenSizeLimitsTest::ScreenWidthAndHeightAreClampedToBounds`
- `ScreenSizeLimitsTest::ScrollbackHistorySizeIsClampedToBounds`
- `ScreenSizeLimitsTest::ResizeIsClampedToBounds`
- `ScrollTest::TestNotifyScrolling`

They still require a Rust Terminal aggregate that owns settings-driven viewport/history clamping, user resize, scroll callbacks, buffer-circling notification policy, and renderer invalidation. Recreating those behaviors in tests would violate the ownership policy.

## TerminalApi / TerminalBuffer audit

The source-family classifications remain `Partial`.

`TerminalApiTest` still observes aggregate terminal state: hyperlink ID/URI tables, taskbar progress, working directory, cursor state and the complete write path. Existing parser/adapter actions prove command identity but not those downstream observables.

`TerminalBufferTests` still mixes real Rust buffer ownership with Terminal-level write/snap-to-output/tab-stop/URL-pattern behavior that is not yet represented by one Rust owner.

## Safety

H05 changes only Rust contract tests, parity metadata/global snapshot/gate, and migration documentation.

- Microsoft C++ product changed: 0
- Rust product implementation changed: 0
- Microsoft tests removed or weakened: 0
- FFI changed: 0
- managed/XAML changed: 0
- certification gates relaxed: 0
- new test-only Terminal aggregate abstractions: 0

The known invalid-key, right-exclusive and wide-glyph selection gaps remain visible rather than patched through parity-only APIs.
