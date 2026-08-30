# R03-B Microsoft-to-Rust adapter test parity

This increment reconciles the remaining 26 Microsoft `TEST_METHOD` contracts in `src/terminal/adapter/ut_adapter/adapterTest.cpp` against concrete Rust evidence.

R03-B closes the **inventory gap**, not every downstream implementation gap. A contract is `Exact` only where Rust materially owns the behavior that Microsoft asserts. When Rust preserves the protocol identity but the final `TextBuffer`, renderer, input, parser-coupling, host, or UI side effect remains outside the migrated core, the contract is deliberately classified `Partial`.

## Result

R03-B reconciles all 26 remaining `adapterTest.cpp` methods:

- **Exact: 3**
- **Partial: 23**
- **Missing: 0**

Together with R03-A, all 53 `adapterTest.cpp` methods are now classified:

- **Exact: 6**
- **Partial: 47**
- **Missing: 0**

Together with R02 (`inputTest.cpp`, `MouseInputTest.cpp`, and `kittyKeyboardProtocol.cpp`), the full Microsoft `adapter` source-method suite now has:

```text
adapter=72; runtime=411; Exact=20, Partial=52, Missing=0
```

This is the first stage boundary in the parity lane where the complete Microsoft adapter source-method inventory has no `Missing` contracts.

## Exact contracts

### `ScrollMarginsTest`

Rust owns the top/bottom margin state and Microsoft validation semantics in `AdaptDispatchCore`.

Witnesses:

- `adapt_dispatch::tests::top_bottom_margin_validation_matches_microsoft_cases`
- `adapt_dispatch::tests::full_height_variants_clear_stored_vertical_margins`

The Rust tests cover valid explicit margins, omitted/default ends, full-height clearing, reversed/equal bounds, and out-of-range rejection.

### `MacroDefinitions`

Rust owns DECDMAC definition parsing and storage in `MacroBuffer`, including text and hexadecimal encodings, defaults, replacement rules, repeat syntax, control filtering, and invalid-definition behavior.

Witnesses:

- `microsoft_adapter_macro_definitions_match_encodings_defaults_and_replacement`
- `microsoft_adapter_macro_definitions_match_repeat_and_control_vectors`

### `PageMovementTests`

Rust owns PPA, PPR, PPB, NP, PP, six-page clamping, cursor behavior, and DECPCCM active/visible-page coupling in `PageManager` + `AdapterDispatch`.

Witness:

- `microsoft_adapter_page_movement_matches_ppa_ppr_ppb_np_pp_and_decpccm`

## Partial boundary contracts

| Microsoft method | Rust witness | Boundary still outside complete Rust ownership |
|---|---|---|
| `Osc4ColorPaletteReportTests` | `microsoft_adapter_osc4_palette_report_preserves_query_indices` | renderer color lookup + OSC response formatting |
| `XtermColorResourceReportTests` | `microsoft_adapter_xterm_color_resource_report_preserves_resource_ids` | renderer aliases + dynamic-color response formatting |
| `TabulationStopReportTests` | `microsoft_adapter_tabulation_stop_report_preserves_decrqpsr_2_boundary` | tab-stop storage, resize normalization, report generation |
| `CursorInformationReportTests` | `microsoft_adapter_cursor_information_report_preserves_decrqpsr_1_boundary` | full cursor/rendition/charset/page report serialization |
| `CursorKeysModeTest` | `microsoft_adapter_cursor_keys_mode_preserves_decckm_set_and_reset` | AdaptDispatch → TerminalInput CursorKey coupling |
| `KeypadModeTest` | `microsoft_adapter_keypad_mode_preserves_application_and_numeric_actions` | AdaptDispatch → TerminalInput keypad coupling |
| `AnsiModeTest` | `microsoft_adapter_ansi_mode_preserves_decanm_set_and_reset_boundary` | dispatch-driven live parser-mode mutation |
| `AllowBlinkingTest` | `microsoft_adapter_allow_blinking_preserves_att610_mode_boundary` | concrete TextBuffer cursor blinking state |
| `LineFeedTest` | `microsoft_adapter_line_feed_preserves_all_three_dispatch_types` | TextBuffer movement + host LineFeed mode |
| `SetConsoleTitleTest` | `microsoft_adapter_console_title_preserves_nonempty_and_empty_titles` | external window-title side effect |
| `TestMouseModes` | `microsoft_adapter_mouse_modes_preserve_all_six_input_mode_boundaries` | AdaptDispatch → TerminalInput mode coupling |
| `Xterm256ColorTest` | `microsoft_adapter_xterm_256_color_preserves_indexed_sgr_vectors` | TextAttribute indexed-color mutation |
| `XtermExtendedColorDefaultParameterTest` | `microsoft_adapter_extended_color_default_parameters_preserve_omissions` | TextAttribute application and range rejection |
| `XtermExtendedSubParameterColorTest` | `microsoft_adapter_extended_subparameter_color_preserves_subparameter_shape` | color-space validation + TextAttribute application |
| `SetColorTableValue` | `microsoft_adapter_set_color_table_value_preserves_full_index_domain_edges` | concrete renderer color-table mutation |
| `SoftFontSizeDetection` | `microsoft_adapter_soft_font_size_detection_preserves_decdld_parameters_boundary` | C++ `FontBuffer` DRCS cell-size inference |
| `TogglingC1ParserMode` | `microsoft_adapter_c1_parser_mode_preserves_accept_and_coding_system_boundaries` | AdaptDispatch-driven parser/code-page coupling |
| `AssignUserPreferenceCharsets` | `microsoft_adapter_assign_user_preference_charset_preserves_decaupss_boundary` | streamed charset identifier + terminal-output state |
| `RequestUserPreferenceCharsets` | `microsoft_adapter_request_user_preference_charset_preserves_decrqupss_boundary` | charset state lookup + response formatting |
| `MacroInvokes` | `microsoft_adapter_macro_invokes_preserve_ids_bounds_and_depth_core` | CSI-triggered recursive execution through live parser/TextBuffer |
| `WindowManipulationTypeTests` | `microsoft_adapter_window_manipulation_reports_preserve_function_codes` | live text/cell/pixel dimensions + response formatting |
| `MenuCompletionsTests` | `microsoft_adapter_menu_completions_preserve_vscode_action_payloads` | completion parsing + UI/menu dispatch |
| `SendC1ControlTest` | `microsoft_adapter_send_c1_control_preserves_7bit_and_8bit_boundaries` | AdaptDispatch → TerminalInput send-C1 state coupling |

Several rows also reuse Microsoft-derived parser or input witnesses. The machine-readable ledger records those supporting witnesses where applicable.

## New executable evidence

R03-B adds 23 direct witnesses in:

`rust/terminal-adapter/tests/microsoft_adapter_r03b_surface_contract.rs`

They verify that the remaining Microsoft contract identities survive the Rust path without being dropped or conflated, including:

- OSC color-table and dynamic-color queries.
- DECRQPSR cursor/tab report selectors.
- DECCKM, DECANM, ATT610 and six mouse-mode identities.
- Keypad application/numeric mode actions.
- All three line-feed dispatch types.
- Empty and nonempty window titles.
- Indexed, omitted/default, and colon-subparameter SGR color vectors.
- Color-table index domain edges.
- DECDLD/DRCS parameter transport.
- C1/coding-system controls.
- DECAUPSS and DECRQUPSS boundaries.
- Macro IDs, out-of-range rejection, and the 16-level invocation-depth contract.
- Window report function codes 18, 14, and 16.
- VS Code completion payload integrity, including JSON containing semicolons.
- S7C1T/S8C1T identity.

## Distinguishing two `SendC1ControlTest` contracts

There are two source methods with the same method name in different Microsoft files:

- `inputTest.cpp::SendC1ControlTest` is already `Exact` because R02 tests the `TerminalInput` sequence behavior itself.
- `adapterTest.cpp::SendC1ControlTest` remains `Partial` because it tests the separate AdaptDispatch-to-TerminalInput state mutation, which is not yet materially owned by `AdapterDispatch`.

The ledger keeps source-file identity so these contracts cannot be accidentally conflated.

## Safety

This increment changes only parity evidence and documentation:

- Product Rust changed: **0**
- Product C++ changed: **0**
- FFI changed: **0**
- Microsoft tests removed or weakened: **0**
- Existing certification gates relaxed: **0**

The original Microsoft/TAEF suites remain the oracle. `Partial` rows are explicit migration debt and can later be promoted only when their downstream responsibility is materially implemented or isolated behind a justified platform boundary.

## Next increment

With R02 and R03 source inventory fully reconciled, the parity lane can move to **R04: TextBuffer + Types + TIL**, again reconciling existing Rust evidence before adding new tests.
