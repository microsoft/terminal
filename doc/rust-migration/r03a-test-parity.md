# R03-A Microsoft-to-Rust adapter test parity

This increment reconciles the first half of Microsoft's `adapterTest.cpp` source-method surface against the Rust `terminal-adapter` implementation and its executable boundary witnesses.

The purpose is evidence, not optimistic relabeling. A typed `OutputAction` that reaches `AdaptDispatchCore::deferred_actions` proves that parsing and semantic identity survive the Rust path, but it does **not** prove the downstream `TextBuffer`, renderer, host API, or response-generation behavior. Those rows remain `Partial` until that responsibility is migrated or a narrower platform boundary is established.

## Result

`adapterTest.cpp` contains 53 Microsoft `TEST_METHOD` contracts. R03-A reconciles the first 27:

- **Exact: 3**
- **Partial: 24**
- **Missing: 0 in the R03-A block**
- **Remaining R03-B methods: 26**

Combined with R02, the complete Microsoft `adapter` suite now reports:

```text
adapter=72; runtime=411; Exact=17, Missing=26, Partial=29
```

## Evidence

| Microsoft method | Coverage | Rust witness | Remaining boundary |
|---|---|---|---|
| `CursorMovementTest` | Exact | `microsoft_adapter_cursor_movement_matches_six_directions_and_bounds` | none |
| `CursorPositionTest` | Exact | `microsoft_adapter_cursor_position_matches_viewport_relative_rows_and_buffer_columns` | none |
| `CursorSingleDimensionMoveTest` | Exact | `microsoft_adapter_single_dimension_absolute_positioning_matches_reference_bounds` | none |
| `CursorSaveRestoreTest` | Partial | `microsoft_adapter_cursor_save_restore_ported_subset_preserves_cursor_state` | Microsoft also persists `TextAttribute` state |
| `CursorHideShowTest` | Partial | `microsoft_adapter_cursor_hide_show_preserves_dectcem_boundary_action` | concrete cursor visibility remains on `TextBuffer` |
| `GraphicsBaseTests` | Partial | `microsoft_adapter_graphics_base_preserves_sgr_reset_boundary_action` | `TextAttribute` mutation deferred |
| `GraphicsSingleTests` | Partial | `microsoft_adapter_graphics_single_preserves_single_sgr_parameter_boundary_action` | `TextAttribute` mutation deferred |
| `GraphicsSingleWithSubParamTests` | Partial | `microsoft_adapter_graphics_single_with_subparams_preserves_parser_shape` | subparameter shape exact; attribute application deferred |
| `GraphicsPushPopTests` | Partial | `microsoft_adapter_graphics_push_pop_preserves_stack_boundary_actions_in_order` | concrete rendition stack deferred |
| `GraphicsPersistBrightnessTests` | Partial | `microsoft_adapter_graphics_persist_brightness_preserves_sgr_ordering_boundary` | persistent intensity state deferred |
| `DeviceStatus_OperatingStatusTests` | Partial | `microsoft_adapter_device_status_operating_status_preserves_dsr_boundary` | response generation deferred |
| `DeviceStatus_CursorPositionReportTests` | Partial | `microsoft_adapter_device_status_cursor_position_preserves_cpr_boundary` | live cursor response generation deferred |
| `DeviceStatus_ExtendedCursorPositionReportTests` | Partial | `microsoft_adapter_device_status_extended_cursor_position_preserves_decxcpr_boundary` | page/cursor response generation deferred |
| `DeviceStatus_MacroSpaceReportTest` | Partial | `microsoft_adapter_device_status_macro_space_preserves_private_62_boundary` | macro-space response generation deferred |
| `DeviceStatus_MemoryChecksumReportTest` | Partial | `microsoft_adapter_device_status_memory_checksum_preserves_private_63_and_id_boundary` | checksum response generation deferred |
| `DeviceStatus_PrivateStatusTests` | Partial | `microsoft_adapter_device_status_private_status_preserves_all_microsoft_status_codes` | concrete private-status responses deferred |
| `DeviceAttributesTests` | Partial | `microsoft_adapter_primary_device_attributes_preserves_primary_da_boundary` | DA response generation deferred |
| `SecondaryDeviceAttributesTests` | Partial | `microsoft_adapter_secondary_device_attributes_preserves_secondary_da_boundary` | DA response generation deferred |
| `TertiaryDeviceAttributesTests` | Partial | `microsoft_adapter_tertiary_device_attributes_preserves_tertiary_da_boundary` | DA response generation deferred |
| `RequestDisplayedExtentTests` | Partial | `microsoft_adapter_request_displayed_extent_preserves_decrqde_boundary` | displayed-extent response deferred |
| `RequestTerminalParametersTests` | Partial | `microsoft_adapter_request_terminal_parameters_preserves_permission_parameter` | DECREPTPARM response deferred |
| `RequestSettingsTests` | Partial | `microsoft_adapter_request_settings_preserves_decrqss_dcs_boundary` | streamed setting ID and response deferred |
| `RequestStandardModeTests` | Partial | `microsoft_adapter_request_standard_mode_preserves_decrqm_boundary` | DECRPM response deferred |
| `RequestPrivateModeTests` | Partial | `microsoft_adapter_request_private_mode_preserves_dec_private_decrqm_boundary` | per-mode DECRPM responses deferred |
| `RequestPermanentModeTests` | Partial | `microsoft_adapter_request_permanent_mode_preserves_2027_boundary` | permanent-mode response semantics deferred |
| `RequestChecksumReportTests` | Partial | `microsoft_adapter_request_checksum_report_preserves_decrqcra_advanced_csi_boundary` | `TextBuffer` checksum computation/response deferred |
| `ColorTableReportTests` | Partial | `microsoft_adapter_color_table_report_preserves_terminal_state_report_boundary` | renderer color-table conversion/response deferred |

## New executable evidence

R03-A adds 23 direct Rust witnesses in:

`rust/terminal-adapter/tests/microsoft_adapter_r03a_surface_contract.rs`

The four cursor contracts already had dedicated Microsoft-derived tests, so they were reused instead of duplicated.

The new tests deliberately exercise the boundary contract:

- SGR parameter and subparameter identity.
- SGR push/pop ordering.
- ANSI and DEC-private DSR identities, including the complete private-status vector used by Microsoft.
- Primary, secondary, and tertiary DA identities.
- DECRQDE, DECREPTPARM, DECRQSS, DECRQM, and permanent-mode request identities.
- DECRQCRA advanced-CSI identity.
- Terminal-state color-report parameter identity.

No test promotes a deferred action to `Exact` merely because it survives routing.

## R03-B remaining surface

The next increment owns the remaining 26 `adapterTest.cpp` methods:

1. `Osc4ColorPaletteReportTests`
2. `XtermColorResourceReportTests`
3. `TabulationStopReportTests`
4. `CursorInformationReportTests`
5. `CursorKeysModeTest`
6. `KeypadModeTest`
7. `AnsiModeTest`
8. `AllowBlinkingTest`
9. `ScrollMarginsTest`
10. `LineFeedTest`
11. `SetConsoleTitleTest`
12. `TestMouseModes`
13. `Xterm256ColorTest`
14. `XtermExtendedColorDefaultParameterTest`
15. `XtermExtendedSubParameterColorTest`
16. `SetColorTableValue`
17. `SoftFontSizeDetection`
18. `TogglingC1ParserMode`
19. `AssignUserPreferenceCharsets`
20. `RequestUserPreferenceCharsets`
21. `MacroDefinitions`
22. `MacroInvokes`
23. `WindowManipulationTypeTests`
24. `MenuCompletionsTests`
25. `PageMovementTests`
26. `SendC1ControlTest`

Several of these already have substantial Rust implementations (`MacroBuffer`, `PageManager`, mode routing, mouse input), so R03-B should again reconcile existing evidence before adding new tests.

## Safety

- Product Rust changed: **0**
- Product C++ changed: **0**
- FFI changed: **0**
- Microsoft tests removed or relaxed: **0**
- Full Microsoft certification remains authoritative.
