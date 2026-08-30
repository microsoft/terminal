# R02 Microsoft-to-Rust test parity

This document records the R02 reconciliation of the Microsoft `adapter` source-method inventory against Rust `terminal-input` evidence.

The authoritative machine-readable classifications live in `tools/rust/microsoft-rust-equivalence.json`. The global source census remains frozen by `tools/rust/microsoft-test-source-census.json` and is enforced by `Test-MicrosoftGlobalTestInventory.ps1`.

## Result

R02 owns 19 of the 72 Microsoft source methods currently counted in the `adapter` suite:

- `inputTest.cpp`: 9 methods
- `MouseInputTest.cpp`: 5 methods
- `kittyKeyboardProtocol.cpp`: 5 methods

After reconciliation:

- **Exact: 14**
- **Partial: 5**
- **Missing: 0**

The remaining **53 Missing** methods in the `adapter` suite therefore belong to the R03 reconciliation surface rather than R02.

The distinction between Exact and Partial is deliberately conservative. A Rust test that covers the portable semantic core does not become Exact when Microsoft also observes behavior supplied by a Windows keyboard-layout API that the Rust product has not yet integrated.

## `inputTest.cpp`

| Microsoft method | Coverage | Rust witness | Remaining boundary |
|---|---|---|---|
| `TerminalInputTests` | Partial | `microsoft_terminal_input_tests_fixed_special_key_table`; `microsoft_terminal_input_tests_all_key_up_events_are_silent` | Default-character lookup uses `MapVirtualKeyW`; non-key `INPUT_RECORD` dispatch remains native |
| `TestFocusEvents` | Exact | `microsoft_terminal_input_focus_events_match_disabled_and_enabled_contract` | None |
| `TerminalInputModifierKeyTests` | Partial | `microsoft_terminal_input_modifier_fixed_vt_table_matches_all_fifteen_states` | Generic/OEM character branches use `ToUnicodeEx` and the active layout |
| `TerminalInputNullKeyTests` | Partial | `microsoft_terminal_input_null_key_portable_subset_matches_ctrl_space_contract` | Additional NUL virtual-key lookup uses `VkKeyScanExW` under a selected layout |
| `DifferentModifiersTest` | Exact | `microsoft_terminal_input_different_modifiers_backspace_delete_and_tab`; `microsoft_terminal_input_different_modifiers_slash_and_question` | None |
| `CtrlNumTest` | Exact | `microsoft_terminal_input_ctrl_num_contract_matches_one_through_nine` | None |
| `BackarrowKeyModeTest` | Exact | `microsoft_terminal_input_backarrow_mode_matches_all_sixteen_combinations` | None |
| `AutoRepeatModeTest` | Exact | `microsoft_terminal_input_auto_repeat_mode_matches_three_downs_then_release` | None |
| `SendC1ControlTest` | Exact | `microsoft_terminal_input_send_c1_control_switches_home_and_f1_prefixes` | None |

## `MouseInputTest.cpp`

All five source methods now have dedicated Rust witnesses. These were added instead of treating existing representative unit tests as sufficient evidence.

| Microsoft method | Coverage | Rust witness | Evidence |
|---|---|---|---|
| `DefaultModeTests` | Exact | `microsoft_mouse_default_mode_tests_match_full_button_modifier_coordinate_matrix` | Six button transitions, five modifier states, three tracking modes, and the Microsoft coordinate boundary matrix |
| `Utf8ModeTests` | Exact | `microsoft_mouse_utf8_mode_tests_match_full_button_modifier_coordinate_matrix` | Same button/modifier/tracking matrix through the UTF-8 coordinate limit |
| `SgrModeTests` | Exact | `microsoft_mouse_sgr_mode_tests_match_tracking_button_modifier_coordinate_matrix` | Press/release/move payloads, modifiers, coordinates, and tracking behavior |
| `ScrollWheelTests` | Exact | `microsoft_mouse_scroll_wheel_tests_match_all_recorded_deltas_modifiers_and_encodings` | Microsoft wheel deltas, modifiers, and Default/UTF-8/SGR encodings |
| `AlternateScrollModeTests` | Exact | `microsoft_mouse_alternate_scroll_mode_tests_match_buffer_mode_and_direction_contract` | Alternate buffer requirement, vertical/horizontal directions, CSI/SS3 cursor mode, and disable/main-buffer cases |

The direct witnesses are in `rust/terminal-input/tests/microsoft_mouse_contract.rs`.

## `kittyKeyboardProtocol.cpp`

| Microsoft method | Coverage | Rust witness | Remaining boundary |
|---|---|---|---|
| `KeyPressTests` | Partial | Existing `keyboard::tests::*` Microsoft-derived families | Major semantic families are covered, but the exported Microsoft data source has not yet been reproduced row-for-row |
| `KeyRepeatEvents` | Exact | `microsoft_kitty_key_repeat_events_match_press_repeat_release_reset_contract` | None |
| `KeyRepeatWithModifiers` | Exact | `microsoft_kitty_key_repeat_with_modifiers_preserves_modifier_contract` | None |
| `KeyRepeatResetOnDifferentKey` | Exact | `microsoft_kitty_key_repeat_resets_on_different_key_contract` | None |
| `IgnoreDeadKey` | Partial | `microsoft_kitty_ignore_dead_key_release_contract` | Microsoft relies on `ToUnicodeEx` dead-key behavior; the Rust core has the `KeyboardMapper` seam but no Windows mapper yet |

The four new direct Kitty witnesses are in `rust/terminal-input/tests/microsoft_kitty_contract.rs`.

### Dead-key finding

The Microsoft test selects French (Standard, AZERTY) and releases `VK_OEM_6` with U+00A8 and Shift. The Microsoft production implementation does not hard-code that key as a dead key. Its `KeyboardHelper` asks the active Windows layout to translate it, and the translation cannot be represented as exactly one codepoint. That result is rejected and the event emits no output.

The portable Rust mapper cannot infer that layout fact from the event alone. Rust already exposes `handle_key_with_mapper` and `KeyboardMapper` specifically for this platform translation seam. The R02 test therefore injects a deterministic mapper that represents the Windows dead-key result and proves the portable core emits no output. Full Exact classification waits for the Windows `ToUnicodeEx` mapper rather than introducing a French-layout heuristic into safe portable Rust.

## Validation

On the completed R02 head, the global census reports:

```text
adapter=72; runtime=411; Exact=14, Missing=53, Partial=5
```

The same head passes:

- `cargo fmt --all --check`
- Clippy with `-D warnings`
- Ubuntu workspace check and tests
- Windows workspace check and tests
- TAEF result-parser self-test
- existing Microsoft parser/adapter source-inventory self-test
- global 1,098-method Microsoft source-census gate
- repository spelling

No C++, Rust product implementation, or FFI behavior changed in this increment. Only test evidence, equivalence metadata, and documentation changed.

## Next increment

Reconcile the remaining 53 `adapterTest.cpp` source methods for R03 against the existing `terminal-adapter` tests. Existing Rust witnesses should be reused where they prove the same vectors; new tests should be added only for genuine gaps. No Microsoft method is promoted from Missing merely by name similarity.
