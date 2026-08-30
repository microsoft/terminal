# R05 Microsoft-to-Rust test parity — TerminalCore

R05 reconciles the Microsoft `UnitTests_TerminalCore` source surface against the current safe Rust `terminal-core` implementation without inventing product abstractions merely to make the ledger green.

## Source census

The frozen Microsoft `terminalCore` suite contains **53 source `TEST_METHOD` identities** and expands to **54 TAEF runtime invocations**.

| Source | Methods | R05 interpretation |
| --- | ---: | --- |
| `InputTest.cpp` | 2 | Portable input translation exists, but the aggregate `Terminal::Send*Event` boundary is not fully Rust-owned. |
| `ScreenSizeLimitsTest.cpp` | 3 | No Rust `Terminal` aggregate owns settings/history/resize clamping yet. |
| `ScrollTest.cpp` | 1 | Scroll callbacks, buffer-circling notification policy, and renderer invalidation remain native integration behavior. |
| `SelectionTest.cpp` | 21 | Selection geometry is the strongest R05 Rust-owned surface; nine source contracts are reproduced directly and the remainder are deliberately Partial. |
| `TerminalApiTest.cpp` | 8 | Several parser/adapter actions have Rust witnesses, but hyperlink/taskbar/CWD/aggregate terminal state is not all owned by `terminal-core`. |
| `TerminalBufferTests.cpp` | 10 | Buffer primitives are migrated, while terminal-level write, scrolling, tab-stop mutation, and URL detection remain wider than the current Rust owner. |
| `TilWinRtHelpersTests.cpp` | 8 | Seven are retained native WinRT helper mechanics; `PropertyChanged` is explicitly XAML/UI-thread managed behavior. |
| **Total** | **53** | **54 runtime invocations** |

## Final R05 classification

```text
terminalCore=53; runtime=54; Exact=9, Missing=4, Partial=32, Platform-only=7, UI-managed=1
```

| Coverage | Count | Meaning in R05 |
| --- | ---: | --- |
| Exact | 9 | The Microsoft source-method behavior is materially reproduced by safe Rust. |
| Partial | 32 | A real Rust owner/witness exists, but Microsoft asserts additional aggregate, viewport, renderer, host, or native behavior. |
| Missing | 4 | No honest current Rust owner exists. |
| Platform-only | 7 | C++/WinRT compatibility-helper mechanics intentionally remain at the native boundary. |
| UI-managed | 1 | XAML `PropertyChanged` is intentionally retained as managed UI-thread behavior. |
| **Total** | **53** | Every source method is deliberately classified. |

## Exact selection contracts

`rust/terminal-core/tests/microsoft_terminal_core_r05_contract.rs` adds nine direct Microsoft-derived witnesses:

1. `SelectUnit` → `microsoft_terminal_core_select_unit_matches_single_cell_anchor_contract`
2. `SelectArea` → `microsoft_terminal_core_select_area_matches_linear_selection_contract`
3. `SelectBoxArea` → `microsoft_terminal_core_select_box_area_matches_one_span_per_row_contract`
4. `SelectWideGlyph_Leading` → `microsoft_terminal_core_wide_glyph_leading_anchor_stays_degenerate`
5. `DoubleClick_GeneralCase` → `microsoft_terminal_core_double_click_general_case_selects_complete_word`
6. `DoubleClick_Delimiter` → `microsoft_terminal_core_double_click_delimiter_selects_empty_row_class`
7. `DoubleClick_DelimiterClass` → `microsoft_terminal_core_double_click_delimiter_class_isolated_cell_contract`
8. `TripleClick_WrappedLine` → `microsoft_terminal_core_triple_click_wrapped_line_expands_full_logical_line`
9. `Pivot` → `microsoft_terminal_core_pivot_contract_preserves_anchor_across_drag_and_shift_click`

These witnesses exercise the same observable selection geometry as the Microsoft source: row-major spans, block rows, delimiter classes, forced-wrap logical lines, wide-glyph leading anchors, and pivot-preserving drag/Shift+Click transitions.

## Why the other SelectionTest methods remain Partial

The current Rust selection modules materially own selection state, word/line expansion, rendering spans, keyboard selection movement, and pivot behavior. They do **not** yet reproduce every terminal aggregate assumption in `SelectionTest.cpp`.

A concrete example is viewport/right-boundary handling. Microsoft permits several selection endpoints to reside at `RightExclusive()`. The current `SelectionState::set_end` data-point path clamps ordinary incoming positions to the last cell before expansion. That distinction affects source methods such as out-of-bounds selection and is enough to keep those contracts Partial rather than promoting them based on superficial similarity.

Other Partial selection remainder includes:

- viewport-relative input coordinates after user/content scrolling;
- trailing-half wide-glyph anchor expansion;
- block-selection span repair when a boundary slices through a wide glyph;
- some double/triple-click drag transitions whose complete behavior includes terminal viewport state.

Existing Rust unit tests remain useful evidence for those subsets, but they are not mislabeled Exact.

## The four Missing contracts are functional backlog

The four Missing source methods are:

- `ScreenSizeLimitsTest::ScreenWidthAndHeightAreClampedToBounds`
- `ScreenSizeLimitsTest::ScrollbackHistorySizeIsClampedToBounds`
- `ScreenSizeLimitsTest::ResizeIsClampedToBounds`
- `ScrollTest::TestNotifyScrolling`

They are not missing because nobody wrote equivalent test syntax. They are missing because there is currently no safe Rust `Terminal` aggregate that owns the corresponding product behavior:

- settings-driven viewport dimension clamping;
- history + visible-row clamping to the terminal's `SHRT_MAX` policy;
- `UserResize` interactions with history allocation;
- scroll-position callbacks;
- buffer-circling notification policy;
- renderer `TriggerScroll` invalidation.

A test-only Rust imitation would create false parity and is therefore intentionally not added.

## Platform and UI ownership

Seven `TilWinRtHelpersTests.cpp` methods exercise `til::property`, `til::event`, and `til::typed_event` C++/WinRT compatibility helpers. These are classified `Platform-only`: the migration target is C++ product semantics, not duplication of retained compatibility-helper mechanics where no Rust product owner consumes them.

`TestPropertyChanged` is `UI-managed`. The Microsoft source explicitly documents that raising the event requires the XAML UI thread and does not run in the ordinary unit-test host. Rewriting that managed/UI ownership into Rust merely to remove a ledger row would violate the R08 ownership policy.

## Terminal API and buffer families

`TerminalApiTest.cpp` and `TerminalBufferTests.cpp` are Partial because the migration already owns meaningful pieces across Rust crates:

- parser/adapter typed actions and color-table boundaries;
- cursor-mode routing boundaries;
- terminal-buffer row and reflow primitives;
- wide-glyph storage semantics;
- line-feed and tab-report protocol boundaries.

Microsoft's source methods also assert state that belongs to the full `Terminal` aggregate, including hyperlink tables, taskbar progress, working directory, snap-to-output behavior, mutable tab stops, URL pattern detection, viewport motion, and complete parser→terminal→buffer side effects. Those wider contracts stay Partial until a Rust product owner exists.

## Machine-readable enforcement

`tools/rust/microsoft-rust-equivalence-r05.json` records the R05 classification using exact entries for the nine direct selection contracts plus source-family rules for the broader families.

`tools/rust/Test-MicrosoftGlobalTestInventory.ps1` now treats `terminalCore` as a deliberately reconciled stage alongside the R04 suites. CI therefore fails when:

- a TerminalCore source method is not covered by an exact entry or deliberate source-family rule;
- the frozen Microsoft source fingerprint changes;
- an exact ledger entry references a removed source method;
- a source rule references a removed source file;
- a non-Missing source rule lacks Rust witnesses;
- the R05 expected coverage distribution changes unexpectedly.

This preserves source-method traceability without adding dozens of repetitive ledger rows for families that legitimately share one Rust ownership boundary.

## Validation

The implementation head before documentation, `c3adc99248b4239164167970e35596c5d79c2659`, passed Rust CI #869:

- `cargo fmt --all --check` ✅
- Clippy with `-D warnings` ✅
- Ubuntu workspace check + all tests ✅
- Windows workspace check + all tests ✅
- TAEF result parser self-test ✅
- Microsoft parser/adapter source inventory self-test ✅
- global Microsoft source census gate ✅
- exact TerminalCore ledger distribution `Exact=9, Partial=32, Missing=4, Platform-only=7, UI-managed=1` ✅

A final CI run is required on the documentation head before Delivery 6 is considered closed.

## Safety

- Product Rust changed: **0**
- Product C++ changed: **0**
- FFI changed: **0**
- Microsoft tests removed or weakened: **0**
- Existing certification gates relaxed: **0**
- The parity PR remains draft and continues to target `rust/r08-product-integration`.

## Next parity lane

The planned next delivery is R06-A: Microsoft Host/ConPTY-facing contracts, reconciling the 331-method `host` source surface against the current `terminal-host` Rust ownership before deciding which families need direct witnesses or remain explicit platform/native backlog.
