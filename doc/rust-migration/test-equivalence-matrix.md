# Microsoft-to-Rust test equivalence matrix

This document is the evidence ledger for deciding when a Microsoft C++/TAEF test must remain in the per-change compatibility gate and when a proven Rust equivalent can carry the fast inner loop.

The matrix is deliberately conservative: until an individual Microsoft contract is mapped to concrete Rust evidence, its area remains **Partial** and the relevant Microsoft test stays blocking for a changed C++/FFI boundary.

## Baseline

The fully integrated R07 checkpoint (`33190ef8d43626adabc7286e2ffe09ea383300fe`) contains **418 distinct Rust tests**. The same 418 contracts run on Linux and Windows, so the CI matrix performs 836 executions but represents 418 distinct test definitions. R07 reported zero ignored tests.

R08 has expanded the portable contract surface to **525 distinct Rust tests** on audited head `a199ab95bb26db11a11127a567422f5fb2ae6d1b`. Rust CI #781 executed that complete inventory successfully on both Linux and Windows.

`tools/rust/contract-baseline.json` records the Microsoft `terminal` suite at **760 total**, with zero failed/blocked/not-run allowed and at most one skipped. The full suite remains the certification oracle; its approximately 24-minute runtime is not used as a reason to weaken a gate.

R08c adds two complementary inventories. `tools/rust/Get-MicrosoftTestInventory.ps1` derives stable source-level `TEST_METHOD` identities without a build. The TAEF harness also uses `/listProperties` to enumerate expanded runtime invocation identities, including data-driven `#metadataSet` cases, before executing the expensive suite. A baseline-count mismatch therefore fails early before spending the full contract runtime.

Source methods are contract groups, not a replacement for TAEF's expanded 760-case inventory. The runtime inventory is authoritative when finer-grained boundary selection is needed.

## Coverage classifications

| Classification | Meaning | Microsoft test in per-boundary gate? |
|---|---|---|
| Exact | Rust covers the same relevant behavior and vectors | Can leave the per-change boundary set after evidence is recorded |
| Stronger | Rust covers the Microsoft case plus additional vectors/invariants | Can leave the per-change boundary set after evidence is recorded |
| Partial | Rust covers only part of the behavior | Yes |
| Platform-only | Requires Windows/COM/WinRT/GDI/DWrite/DX or another platform surface | Yes |
| UI-managed | Responsibility correctly belongs to C#/XAML rather than Rust | Validate in the managed/UI contract appropriate to that surface |
| Missing | No adequate migrated equivalent exists | Yes |

Leaving the per-change set does **not** remove a Microsoft test from full certification. The complete Microsoft suite remains an R08 exit gate and an R09 final-validation gate.

## Current Rust inventory

| Area | Rust crate | Stage | R07 stable tests | Current R08 tests | Initial equivalence status | C# retained? | Default CI tier |
|---|---|---:|---:|---:|---|---|---|
| VT parser | `terminal-parser` | R01 | 39 | 124 | Partial overall; Base64 + StateMachine complete, all 25 InputEngine methods classified, and OutputEngine XParse color gaps closed | No | Fast + affected boundary |
| Terminal input | `terminal-input` | R02 | 28 | 39 | Partial pending remaining source-method mapping; fixed-key, key-up, and R01/R02 round-trip evidence present | No | Fast + affected boundary |
| Adapter / dispatch / Sixel | `terminal-adapter` | R03 | 77 | 84 | Partial pending method mapping | No | Fast + affected boundary |
| TextBuffer / foundational types | `terminal-buffer` | R04 | 68 | 68 | Partial pending method mapping | No | Fast + affected boundary |
| TerminalCore | `terminal-core` | R05 | 38 | 38 | Partial pending method mapping | No | Fast + affected boundary |
| Host / server / interactivity / ConPTY | `terminal-host` | R06 | 118 | 118 | Partial pending method mapping | No | Fast + affected boundary |
| Renderer | `terminal-renderer` | R07 | 50 | 50 | Partial pending method mapping | No | Fast + affected boundary |
| Product FFI foundation | `terminal-parser-ffi` | R08 | 0 | 4 | Platform/boundary evidence not yet consumed by C++ | No | Fast; Microsoft contract becomes blocking when a consumer is added |
| XAML code-behind / bindings / view models | existing managed projects | R08 | n/a | n/a | UI-managed where already owned by C# | Yes | Managed/UI contract |
| WinRT/COM/XAML native boundary | existing platform layer | R08 | n/a | n/a | Platform-only until narrowed | Where applicable | Boundary + Stage |

**Current total:** 418 stable R07 tests; **525** on audited R08 head `a199ab95bb26db11a11127a567422f5fb2ae6d1b`. Rust CI #781 passed fmt, Clippy with `-D warnings`, Ubuntu, Windows, TAEF harness self-test, and Microsoft source-inventory self-test on that exact head.

## Evidence rows

### R01 Base64

`src/terminal/parser/ut_parser/Base64Test.cpp` contains exactly two Microsoft `TEST_METHOD` contracts, and the source-inventory self-test locks that identity set.

| Microsoft suite/test | Area | Behavior | Rust equivalent | Vector evidence | Coverage | Windows dependency | FFI dependency | CI tier | Stage | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| `terminal / Base64Test.DecodeUTF8` | Parser/Base64 | Decode multilingual UTF-8 and emoji/skin-tone payloads | `base64::tests::matches_windows_terminal_unicode_vectors` | Rust uses the same two Base64 inputs and the same expected Unicode strings as Microsoft | Exact | No | No | Fast + Full certification | R01 | Direct vector-for-vector equivalence |
| `terminal / Base64Test.DecodeFuzz` | Parser/Base64 | ASCII round-trip across varying lengths, padded/unpadded input, including empty input | `base64::tests::deterministic_ascii_round_trips_match_reference_encoding`; `decodes_rfc_4648_vectors_with_and_without_padding` | Microsoft samples 8 random lengths/content choices; Rust deterministically covers every length 0..128 and both padded and unpadded forms, plus canonical RFC vectors | Stronger | No | No | Fast + Full certification | R01 | Rust trades nondeterministic sampling for broader reproducible length/padding coverage |

### R01 StateMachine

`StateMachineTest.cpp` defines seven source methods. The data-driven DCS method expands to four Microsoft runtime invocations and Rust covers the same four terminators. R08c added a dedicated integration test for the one ordering case that had previously been only partially represented.

| Microsoft suite/test | Area | Behavior | Rust equivalent | Vector evidence | Coverage | Windows dependency | FFI dependency | CI tier | Stage | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| `terminal / StateMachineTest.TwoStateMachinesDoNotInterfereWithEachOther` | Parser/state machine | Parser instance isolation across interleaved partial/full CSI sequences | `state_machine::tests::two_state_machines_do_not_interfere` | Same partial `ESC[12`, independent `ESC[3C`, then completion `;34m`; same parameter observations | Exact | No | No | Fast + Full certification | R01 | Direct scenario equivalence |
| `terminal / StateMachineTest.PassThroughUnhandled` | Parser/state machine | Unknown CSI is flushed intact while following printable text remains printable | `state_machine_microsoft_contract::microsoft_passthrough_unhandled_sequence_before_printable_text` | Rust uses the same `ESC[?999h 12345 Hello World` ordering and separately asserts the intact passthrough sequence and following printable text | Exact | No | No | Fast + Full certification | R01 | Dedicated R08c test closes the former ordering gap |
| `terminal / StateMachineTest.RunStorageBeforeEscape` | Parser/state machine | Buffered printable run is emitted before transition into an escape sequence | `state_machine::tests::unhandled_csi_is_passed_through_without_losing_prior_text` | Both send `12345 Hello World` followed by `ESC[?999h` and observe complete text plus passthrough sequence | Exact | No | No | Fast + Full certification | R01 | Direct ordering/vector match |
| `terminal / StateMachineTest.BulkTextPrint` | Parser/state machine | Plain text is emitted as a single bulk print run | `state_machine::tests::bulk_text_is_printed_as_one_run` | Same `12345 Hello World` payload and expected single run | Exact | No | No | Fast + Full certification | R01 | Direct scenario equivalence |
| `terminal / StateMachineTest.PassThroughUnhandledSplitAcrossWrites` | Parser/state machine | Unknown CSI/OSC survives two- and three-part writes | `state_machine::tests::unhandled_sequences_survive_split_writes` | Rust covers the same split CSI cases and OSC split at ESC/ST | Exact | No | No | Fast + Full certification | R01 | Direct split-write equivalence |
| `terminal / StateMachineTest.DcsDataStringsReceivedByHandler` | Parser/state machine | DCS id/params/data and ST, CSI, CAN, SUB termination | `state_machine::tests::dcs_data_is_delivered_and_st_can_terminate_it`; `dcs_can_be_terminated_by_csi_can_or_sub` | Same four terminator families with id/params/data and post-termination observations | Exact | No | No | Fast + Full certification | R01 | Four expanded TAEF cases map to two Rust tests |
| `terminal / StateMachineTest.VtParameterSubspanTest` | Parser/parameters | Parameter subspan at 0, 2, end, past-end | `state_machine::tests::parameter_subspan_matches_terminal_semantics` | Same `[12,34,56,78]`, offsets `0,2,4,6` and omitted-value semantics | Exact | No | No | Fast + Full certification | R01 | Direct vector-for-vector equivalence |

All nine Base64/StateMachine source methods are Exact or Stronger. They may leave the per-change semantic boundary set for Rust-only implementation changes. They remain in complete Microsoft certification and become boundary-relevant whenever their C ABI representation or C++ consumer changes.

### R01 InputEngine — deterministic semantic contracts

`InputEngineTest.cpp` contains 25 source methods; the source-inventory self-test locks the complete set before individual mappings are accepted. Microsoft `TestInputCallback` deliberately ignores virtual-key and scan-code identity and compares key-down, repeat count, Unicode character, and Shift/Alt/Ctrl/Enhanced semantics; the classifications below follow those actual observations rather than treating every `MapVirtualKeyW` call as a semantic dependency.

| Microsoft suite/test | Area | Behavior | Rust equivalent | Vector evidence | Coverage | Windows dependency | FFI dependency | CI tier | Stage | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| `terminal / InputEngineTest.TestWin32InputOptionals` | Parser/input engine | Six optional Win32 key fields across parameter counts 0..6 | `input_engine_microsoft_contract::microsoft_win32_input_optionals_matrix` | Complete `64 * 7 = 448` Cartesian product; every output field checked | Exact | No | No | Fast + Full certification | R01 | Runtime inventory provides expanded TAEF identities |
| `terminal / InputEngineTest.TestWin32InputParsing` | Parser/input engine | Prefixes of six Win32 key fields preserve defaults/supplied values | `input_engine_microsoft_contract::microsoft_win32_input_optionals_matrix` | All-six-bits case with parameter counts 1..6 reproduces Microsoft prefixes; exhaustive matrix adds all optional combinations | Stronger | No | No | Fast + Full certification | R01 | Fixed vectors are a subset of the Rust matrix |
| `terminal / InputEngineTest.SGRMouseTest_ButtonClick` | Parser/input mouse | Left/middle/right press and release state | `input_engine_microsoft_contract::microsoft_sgr_mouse_button_click_table` | Same 6 SGR sequences, same zero-based coordinates/button states/event flags | Exact | No | No | Fast + Full certification | R01 | Direct vector table |
| `terminal / InputEngineTest.SGRMouseTest_Modifiers` | Parser/input mouse | Shift/Alt/Ctrl projection onto SGR mouse events | `input_engine_microsoft_contract::microsoft_sgr_mouse_modifier_table` | Same 6 sequences and modifier/button combinations | Exact | No | No | Fast + Full certification | R01 | Direct vector table |
| `terminal / InputEngineTest.SGRMouseTest_Movement` | Parser/input mouse | Drag/move state across held/released buttons and coordinates | `input_engine_microsoft_contract::microsoft_sgr_mouse_movement_table` | Same 10-sequence stateful movement trace | Exact | No | No | Fast + Full certification | R01 | State is preserved across the full Microsoft trace |
| `terminal / InputEngineTest.SGRMouseTest_Scroll` | Parser/input mouse | Vertical/horizontal forward/backward wheel encoding | `input_engine_microsoft_contract::microsoft_sgr_mouse_scroll_table` | Same 4 sequences, deltas, wheel flags and coordinates | Exact | No | No | Fast + Full certification | R01 | Direct vector table |
| `terminal / InputEngineTest.SGRMouseTest_DoubleClick` | Parser/input mouse | Same-button/same-position click pairs produce double-click and reset | `input_engine_microsoft_contract::microsoft_sgr_mouse_double_click_table` | Same 18-event left/middle/right trace; one-second deterministic test interval | Exact | No | No | Fast + Full certification | R01 | Direct stateful trace |
| `terminal / InputEngineTest.SGRMouseTest_Hover` | Parser/input mouse | Hover motion with no pressed button | `input_engine_microsoft_contract::microsoft_sgr_mouse_hover_table` | Same 2 motion sequences and coordinates | Exact | No | No | Fast + Full certification | R01 | Direct vector table |
| `terminal / InputEngineTest.ChunkedSequence` | Parser state | Partial CSI `ESC[1` remains in CSI-parameter state | `state_machine_microsoft_contract::microsoft_chunked_csi_remains_in_parameter_state` | Same bytes and same intermediate state | Exact | No | No | Fast + Full certification | R01 | Pure parser state contract |
| `terminal / InputEngineTest.TestSs3Entry` | Parser state | `ESC O` enters SS3 and final `m` returns to ground | `state_machine_microsoft_contract::microsoft_ss3_entry_transitions_to_ground_after_dispatch` | Same characters and state after each character | Exact | No | No | Fast + Full certification | R01 | Pure parser state contract |
| `terminal / InputEngineTest.TestSs3Immediate` | Parser state | `$`, `#`, `%`, `?` dispatch immediately from SS3 entry | `state_machine_microsoft_contract::microsoft_ss3_immediates_dispatch_directly_from_entry` | Same four final bytes and state transitions | Exact | No | No | Fast + Full certification | R01 | Pure parser state contract |
| `terminal / InputEngineTest.TestSs3Param` | Parser state | `;324;;8` remains SS3-param until final `J` | `state_machine_microsoft_contract::microsoft_ss3_parameters_remain_parameter_state_until_final_byte` | Same exact byte trace and intermediate/final states | Exact | No | No | Fast + Full certification | R01 | Pure parser state contract |
| `terminal / InputEngineTest.CursorPositioningTest` | Parser/input engine | First `ESC[1;4R` is captured as cursor position; second returns to normal F3 interpretation | `input_engine_microsoft_key_contract::microsoft_cursor_positioning_consumes_once_then_reverts_to_f3` | Same sequence twice, same `{column:4,row:1}` capture and same Alt+Shift/Unicode-0 F3 observation; Rust additionally asserts the F3 virtual key | Stronger | No | No | Fast + Full certification | R01 | Microsoft callback does not compare the platform scan code |
| `terminal / InputEngineTest.CSICursorBackTabTest` | Parser/input engine | `ESC[Z` becomes Shift+Tab with Unicode tab | `input_engine_microsoft_key_contract::microsoft_csi_cursor_backtab_matches_shift_tab` | Same sequence, repeat, Unicode and Shift state; Rust additionally asserts `VK_TAB` | Stronger | No | No | Fast + Full certification | R01 | Microsoft callback ignores scan code |
| `terminal / InputEngineTest.EnhancedKeysTest` | Parser/input engine | Ten CSI navigation/editing sequences carry Enhanced semantics | `input_engine_microsoft_key_contract::microsoft_enhanced_keys_table_matches_all_ten_sequences` | Same Prior/Next/End/Home/Left/Up/Right/Down/Insert/Delete sequences; same Unicode-zero and Enhanced observations; Rust also asserts each virtual key | Stronger | No | No | Fast + Full certification | R01 | Platform scan code is not an observed assertion in Microsoft callback |
| `terminal / InputEngineTest.SS3CursorKeyTest` | Parser/input engine | Six SS3 cursor/home/end sequences decode without modifiers | `input_engine_microsoft_key_contract::microsoft_ss3_cursor_key_table_matches_all_six_sequences` | Same six sequences, Unicode-zero and modifier state; Rust also asserts virtual-key identity | Stronger | No | No | Fast + Full certification | R01 | Platform scan code is not an observed assertion in Microsoft callback |
| `terminal / InputEngineTest.AltBackspaceTest` | Parser/input engine | `ESC DEL` becomes Alt+Backspace with Unicode BS | `input_engine_microsoft_key_contract::microsoft_alt_backspace_matches_escape_delete` | Same bytes, repeat, Unicode `0x08` and Alt state; Rust also asserts `VK_BACK` | Stronger | No | No | Fast + Full certification | R01 | Scan code ignored by Microsoft comparator |
| `terminal / InputEngineTest.AltCtrlDTest` | Parser/input engine | `ESC EOT` becomes Alt+Ctrl+D | `input_engine_microsoft_key_contract::microsoft_alt_ctrl_d_matches_escape_eot` | Same bytes, Unicode `0x04`, Alt+Ctrl and repeat; Rust also asserts `D` virtual key | Stronger | No | No | Fast + Full certification | R01 | Direct semantic vector |
| `terminal / InputEngineTest.CtrlAltZCtrlAltXTest` | Parser/input engine | CAN/SUB following ESC execute as Ctrl+Alt+X/Z in input mode | `input_engine_microsoft_key_contract::microsoft_ctrl_alt_z_and_x_execute_from_escape_in_input_mode` | Same `ESC SUB` and `ESC CAN`, Unicode `0x1a/0x18`, Alt+Ctrl and repeat | Stronger | No | No | Fast + Full certification | R01 | Rust additionally asserts X/Z virtual-key identity |
| `terminal / InputEngineTest.AltBackspaceEnterTest` | Parser/input engine | Alt+Backspace must not leak Alt state into following Enter | `input_engine_microsoft_key_contract::microsoft_alt_backspace_then_enter_returns_to_ground_between_keys` | Same `ESC DEL` then CR; same Alt+BS then plain CR and explicit Ground state after each | Stronger | No | No | Fast + Full certification | R01 | Direct GH#2746 regression scenario |
| `terminal / InputEngineTest.AltIntermediateTest` | Parser/input + TerminalInput | Alt+`/` round-trips as `ESC/`; subsequent Ctrl+E remains independent | `input_engine_microsoft_key_contract::microsoft_alt_intermediate_parser_half_preserves_alt_slash_then_ctrl_e`; `terminal-input/tests/input_engine_microsoft_contract::microsoft_alt_intermediate_roundtrip_preserves_alt_slash_then_ctrl_e` | Same two sequences and same round-trip outputs, split across the R01 parser and R02 TerminalInput owners to avoid a crate cycle | Stronger | No | No | Fast + Full certification | R01/R02 | Composition preserves the Microsoft end-to-end contract while each crate remains independently testable |

The six SGR methods represent **46 concrete Microsoft mouse-event observations**. Together with the Win32-optional matrix, the key tables, and the parser-state regressions, the fast Rust loop now carries the deterministic portion of all 25 InputEngine source methods without invoking the full Microsoft suite.

### R01 InputEngine — retained Windows/platform contracts

Four methods intentionally remain in the Microsoft/platform gate because Windows mapping or synthesis contributes to observations the source test actually checks.

| Microsoft suite/test | Area | Rust evidence | Coverage | Why it remains Microsoft/platform evidence |
|---|---|---|---|---|
| `terminal / InputEngineTest.RoundTripTest` | Parser/input + TerminalInput | none adequate | Platform-only | Upstream-skipped GH#4405; intended body depends on Windows virtual-key/scan-code translation |
| `terminal / InputEngineTest.AlphanumericTest` | Parser/input | `printable_and_non_ascii_runs_use_the_string_dispatch_boundary` | Platform-only | Microsoft derives modifiers/Unicode through `VkKeyScanW`/`CharToKeyEvents`; those observations are layout/platform behavior |
| `terminal / InputEngineTest.NonAsciiTest` | Parser/input | `printable_and_non_ascii_runs_use_the_string_dispatch_boundary` | Platform-only | Microsoft exercises Windows `CharToKeyEvents` synthesis for non-ASCII UTF-16 |
| `terminal / InputEngineTest.C0Test` | Parser/input | `c0_and_alt_controls_match_input_contract` | Partial / platform boundary | Rust covers core C0/Alt semantics, but Microsoft uses `VkKeyScanW` to derive Shift for the full `0x00..0x1f` table and observes modifier equivalence |

All 25 `InputEngineTest` source methods are now classified. Twenty-one have Exact/Stronger portable evidence; four remain explicitly platform-bound. None leave full R08/R09 certification.

### R01 OutputEngine — XParse color gap closure

`OutputEngineTest.cpp` locks 64 source methods. The three methods below were the final external-dispatch rows still classified Partial. The R08 compatibility facade now normalizes Microsoft XParse color forms before dispatch, including high-bit `#hhh/#hhhhhh/#hhhhhhhhh/#hhhhhhhhhhhh` semantics and case-insensitive X11 names used by the Microsoft vectors. Dedicated Rust contracts reproduce the valid, partial, invalid, multi-resource, and color-table cases.

| Microsoft suite/test | Area | Behavior | Rust equivalent | Vector evidence | Coverage | Windows dependency | FFI dependency | CI tier | Stage | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| `terminal / OutputEngineTest.TestOscSetDefaultForeground` | Parser/output OSC | Set default foreground and following dynamic resources from XParse color specifications | `output_engine_microsoft_color_contract::microsoft_output_osc_set_default_foreground_matches_all_reference_vectors`; `output_engine::tests::xparse_hash_uses_high_bits_instead_of_rgb_component_scaling`; `microsoft_output_engine_xorg_names_are_ascii_case_insensitive` | Same `rgb:`/`#111`/`#123456`/`DarkOrange` vectors, multi-resource progression, empty/invalid fields, and Microsoft high-bit component semantics | Exact | No | No | Fast + Full certification | R01/R08 | Product parser correction closes the former `#hhh` and X11-name gap |
| `terminal / OutputEngineTest.TestOscSetDefaultBackground` | Parser/output OSC | Set default background and following dynamic resources from XParse color specifications | `output_engine_microsoft_color_contract::microsoft_output_osc_set_default_background_matches_all_reference_vectors`; XParse facade unit tests | Same foreground-equivalent vector family rooted at OSC 11, including multi-resource progression and invalid fields | Exact | No | No | Fast + Full certification | R01/R08 | Uses the same normalized XParse path as OSC 10 |
| `terminal / OutputEngineTest.TestOscSetColorTableEntry` | Parser/output OSC | Parse indexed color-table assignments, multiple entries, partial and invalid payloads | `output_engine_microsoft_color_contract::microsoft_output_osc_set_color_table_entry_matches_valid_partial_and_invalid_vectors` | Same indexed `rgb:`/`#111`/`orange` vectors, multiple assignments, truncation at malformed data, and invalid payload rejection | Exact | No | No | Fast + Full certification | R01/R08 | X11 `orange` and high-bit hash semantics now match Microsoft |

These three rows are now Exact. Together with the 30 external-dispatch methods already carrying direct portable evidence, all 33 OutputEngine external-dispatch source methods have portable Rust evidence. They remain part of full R08/R09 Microsoft certification.

## Per-test row schema

The area-level inventory above is only the bootstrap. The matrix becomes authoritative for CI reduction only when the Microsoft suite is expanded into one row per source method or independently meaningful runtime case using this schema:

| Field | Description |
|---|---|
| Microsoft suite/test | Canonical source/runtime identity |
| Area | Parser, input, adapter, buffer, core, host, renderer, control, settings, UI, platform |
| Behavior | Contract protected by the test |
| Current owner | C++, Rust, C#, XAML, or platform boundary |
| Rust equivalent | Concrete Rust test function(s), if any |
| Vector evidence | Important cases/parameters covered on each side |
| Coverage | Exact, Stronger, Partial, Platform-only, UI-managed, Missing |
| Windows dependency | Whether the contract requires Windows runtime behavior |
| FFI dependency | Whether the contract crosses the product ABI |
| C# retained | Whether healthy managed UI ownership intentionally remains C# |
| CI tier | Fast, Boundary, Stage, Full certification |
| Stage | R01 through R09 |
| Notes | Differences, known gaps, or evidence references |

## CI selection rule

1. **Fast** runs on every active R08 change: Rust fmt, Clippy with `-D warnings`, Linux/Windows workspace check+test, TAEF harness self-test, and the Microsoft source-inventory self-test. Repository spelling is intentionally deferred while a pull request is draft to avoid paying a repository-wide pass on every synchronization; `ready_for_review` restores the spelling gate before integration, and push/final certification still require it.
2. **Boundary** is added when C++/FFI/platform code changes. Run every affected Microsoft row still classified Partial, Platform-only, or Missing, plus any Exact/Stronger row whose boundary representation itself changed.
3. **Stage** runs before R08 merge for all R08 contracts that have not been proven sufficiently equivalent.
4. **Full certification** runs the complete Microsoft Terminal Suite at R08 exit and again in R09.

A contract run captures the authoritative TAEF runtime inventory before executing the suite. If that inventory differs from the recorded baseline total, the run stops before spending the cost of the full suite. A successful full run additionally requires inventory count and result total to agree.

No Microsoft test is removed from a blocking tier merely because it is slow. It leaves the per-change boundary tier only when the matrix contains concrete equivalence evidence.

## R08 managed-UI rule

The migration is **C++ to Rust**, not C# to Rust. Existing C# that naturally drives XAML remains managed code. Such rows are classified **UI-managed**, not Missing, provided the C# layer is genuinely UI orchestration rather than a wrapper around business logic that still resides in removable C++.

The desired direction is therefore:

```text
XAML -> existing C# managed UI -> narrow interop -> safe Rust semantics
```

where that ownership already exists, while native WinRT/COM/Win32 boundaries remain explicit and narrow.

## Next matrix increment

Map the remaining `TerminalInputTests`/R02 source-method surface next, then continue through R03-R07. Runtime-expanded TAEF identities are captured automatically by the contract harness when certification is required. Until a method has concrete evidence, it remains Partial and does not justify relaxing the Microsoft boundary gate.
