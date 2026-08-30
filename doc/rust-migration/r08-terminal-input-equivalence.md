# R08 TerminalInput source-method equivalence

This note audits the nine Microsoft `InputTest` source methods in `src/terminal/adapter/ut_adapter/inputTest.cpp` against the current Rust `terminal-input` contract surface. It supplements the central equivalence matrix while R02 source-method mapping is completed.

The classification is intentionally conservative. A method is promoted to Exact only when the observations made by the Microsoft test are reproduced without relying on Windows keyboard-layout translation. Partial and Platform-only methods remain in the Microsoft boundary/stage set and all nine remain in full R08 certification.

## Exact portable contracts

| Microsoft contract | Rust evidence | Classification | Evidence |
|---|---|---|---|
| `InputTest.TestFocusEvents` | `microsoft_input_contract::microsoft_terminal_input_focus_events_match_disabled_and_enabled_contract` | Exact | Same disabled focus behavior, then `ESC[O` for focus loss and `ESC[I` for focus gain after enabling `FocusEvent`. |
| `InputTest.CtrlNumTest` | `microsoft_input_contract::microsoft_terminal_input_ctrl_num_contract_matches_one_through_nine` | Exact | Same Ctrl+digit control translations for the Microsoft digit table; no keyboard-layout API contributes to the expected output. |
| `InputTest.BackarrowKeyModeTest` | `microsoft_input_contract::microsoft_terminal_input_backarrow_mode_matches_all_sixteen_combinations` | Exact | Same BackarrowKey enabled/disabled behavior across the Microsoft Shift/Ctrl/Alt modifier combinations, including ESC-prefixed Alt output. |
| `InputTest.AutoRepeatModeTest` | `microsoft_input_contract::microsoft_terminal_input_auto_repeat_mode_matches_three_downs_then_release` | Exact | Same repeated `A` key-down sequence: repeats suppressed while AutoRepeat is disabled, preserved while enabled, and key-up emits nothing. |
| `InputTest.SendC1ControlTest` | `microsoft_input_contract::microsoft_terminal_input_send_c1_control_switches_home_and_f1_prefixes` | Exact | Same four observations: Home and F1 use 8-bit CSI/SS3 when SendC1 is enabled and 7-bit ESC-prefixed forms when disabled. |

These five methods may leave the per-change semantic boundary set for Rust-only implementation changes because their portable observations are now carried by the fast Rust loop. They become boundary-relevant again if their C++/FFI representation changes, and they never leave full Microsoft certification.

## Retained Microsoft/platform contracts

| Microsoft contract | Rust evidence | Classification | Why it remains blocking outside the fast Rust loop |
|---|---|---|---|
| `InputTest.TerminalInputTests` | Existing fixed-key/key-up Rust contracts cover deterministic subsets | Partial / platform boundary | The Microsoft method sweeps every byte-sized virtual key and derives Unicode with `OneCoreSafeMapVirtualKeyW(..., MAPVK_VK_TO_CHAR)`. It also observes non-key `INPUT_RECORD` handling that the portable Rust key abstraction does not represent directly. |
| `InputTest.TerminalInputModifierKeyTests` | Existing modifier tables cover deterministic key-output semantics | Platform-only | The Microsoft data-driven method builds keyboard state and uses Windows virtual-key/Unicode translation. Layout-dependent synthesis is part of the exercised path, so a hard-coded portable key table is not equivalent evidence for the complete method. |
| `InputTest.TerminalInputNullKeyTests` | `microsoft_terminal_input_null_key_portable_subset_matches_ctrl_space_contract` | Partial / platform boundary | Rust reproduces the portable Ctrl+Space/null-output subset, including Alt-prefixed cases, but Microsoft also derives the null virtual key through `OneCoreSafeVkKeyScanW(0)` and exercises layout/platform modifier combinations. |
| `InputTest.DifferentModifiersTest` | `microsoft_terminal_input_different_modifiers_backspace_delete_and_tab`; `microsoft_terminal_input_different_modifiers_slash_and_question` | Partial / platform boundary | Rust directly covers the deterministic Backspace/Delete/Tab and slash/question output cases. Microsoft obtains at least part of the key identity through Windows keyboard-layout translation, so the source method as a whole is not promoted to Exact. |

## Gate consequence

The R02 source-method census is now explicit: five of nine `InputTest` methods have Exact portable Rust evidence and four deliberately retain Microsoft/platform responsibility. This does not weaken any gate. Partial/Platform-only rows remain blocking for affected boundaries and stage validation, and the complete Microsoft Terminal Suite remains mandatory before R08 integration.

Repository spelling remains intentionally excluded only for the active draft `rust/r08-product-integration` development branch. This note does not alter that exclusion. Immediately before R08 integration, the temporary exclusion must be removed and a fresh successful spelling run must be required on the integration-ready head before the remaining exit gates and complete Microsoft certification are accepted.
