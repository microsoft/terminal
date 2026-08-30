# H02 — R03-A adapter hardening

H02 starts from the H01 merge on `rust/r08-product-integration@11ff55b8347974b6f2b3a8831e102d2f3c592d71` and audits the first 27 methods of `src/terminal/adapter/ut_adapter/adapterTest.cpp` against the actual downstream observations made by Microsoft.

The hardening rule is stricter than source-action preservation: an `OutputAction` surviving into `AdaptDispatchCore::deferred_actions` proves parser/boundary identity, but it does **not** prove the `TextBuffer`, renderer, response-generation, checksum, or color-conversion behavior that the Microsoft method asserts.

## Result

The R03-A classification remains deliberately:

```text
27 Microsoft source methods
Exact    3
Partial 24
Missing  0
```

No method was promoted merely to improve the headline number. The audit instead separates three already complete Rust-owned cursor contracts from 24 genuine downstream ownership gaps.

### Exact

The three methods whose complete Microsoft observation is already materially owned by `AdaptDispatchCore` remain Exact:

- `CursorMovementTest`
- `CursorPositionTest`
- `CursorSingleDimensionMoveTest`

They operate on deterministic page/cursor geometry and bounds that Rust owns directly.

## Why the remaining 24 stay Partial

### Cursor state beyond geometry

`CursorSaveRestoreTest` observes both cursor state and `TextAttribute` restoration. Rust owns position, delayed-wrap, and origin-relative cursor state, but `SavedCursor` does not yet own the saved attributes.

`CursorHideShowTest` verifies the concrete `TextBuffer` cursor visibility flag across all four starting/ending permutations. Rust preserves DECTCEM mode 25 at the adapter boundary, but does not yet mutate that cursor object.

### SGR and rendition stack

`GraphicsBaseTests`, `GraphicsSingleTests`, `GraphicsSingleWithSubParamTests`, `GraphicsPushPopTests`, and `GraphicsPersistBrightnessTests` all assert actual `TextAttribute` mutation or rendition-stack restoration.

H02 strengthens their portable boundary evidence substantially:

- `GraphicsSingleTests`: all **49** Microsoft data-source SGR values now reach the Rust adapter boundary unchanged, replacing the former representative subset.
- `GraphicsSingleWithSubParamTests`: all **4** Microsoft vectors are parser-checked (`4:3`, `38:5:1`, `48:5:15`, `58:5:1`).
- `GraphicsPushPopTests`: empty, nested, partial-attribute, and underline-only push/pop action shapes are retained in source order.
- `GraphicsPersistBrightnessTests`: the complete three Microsoft SGR command traces are retained in source order.

Those stronger tests remove vector uncertainty, but not the real functional gap: `AdaptDispatchCore` still defers rendition actions rather than applying them to a Rust-owned current attribute and stack.

### Device status and attributes

The Microsoft methods do not merely request reports; they validate exact response strings and, in several cases, failure propagation from `ReturnResponse`:

- `DeviceStatus_OperatingStatusTests` expects `ESC[0n`.
- `DeviceStatus_CursorPositionReportTests` formats the live cursor relative to the viewport, including two retained CPRs around a cursor move.
- `DeviceStatus_ExtendedCursorPositionReportTests` adds the active page number.
- `DeviceStatus_MacroSpaceReportTest` reports live `MacroBuffer` free space.
- `DeviceStatus_MemoryChecksumReportTest` generates a checksum response with request ID.
- `DeviceStatus_PrivateStatusTests` validates concrete private DSR responses.
- primary, secondary, and tertiary device-attribute tests validate exact DA payloads and `ReturnResponse` failure behavior.

Rust currently preserves the typed requests but does not yet own the complete response-generating surface, so these remain Partial.

### Settings, modes, checksum, and color-table reports

`RequestDisplayedExtentTests`, `RequestTerminalParametersTests`, `RequestSettingsTests`, `RequestStandardModeTests`, `RequestPrivateModeTests`, `RequestPermanentModeTests`, `RequestChecksumReportTests`, and `ColorTableReportTests` similarly validate generated responses or state derived from live terminal/renderer data.

The existing Rust witnesses already retain the Microsoft request identities, including the full private-mode list, permanent mode 2027, DECRQCRA parameters, and both HLS/RGB color-table report selectors. H02 does not promote them because their response semantics are not yet materially owned in Rust.

`RequestSettingsTests` is especially broad: Microsoft exercises margins, a large SGR attribute surface, cursor styles, protected attributes, and other DECRQSS settings. A DCS request surviving the parser boundary is not equivalent to those formatted responses.

## Evidence-hardening outcome

H02 intentionally leaves the global coverage distribution unchanged from H01:

```text
Exact         127
Stronger       11
Partial       383
Platform-only  63
UI-managed     22
Missing       492
Total        1098
```

The value of this delivery is that several R03-A `Partial` rows are now **exhaustive at the parser/adapter boundary**. Their remaining gap is narrowly identified as downstream functional ownership rather than incomplete vector coverage.

This gives the functional migration lane a clearer target: implementing `TextAttribute`/rendition ownership or VT response generation can later promote specific source methods without first reconstructing their Microsoft input matrices.

## Safety

- Product Rust changed: **0**
- Product C++ changed: **0**
- FFI changed: **0**
- managed/XAML changed: **0**
- Microsoft tests removed/weakened: **0**
- certification gates relaxed: **0**

H02 changes only Rust contract tests and migration documentation.

## Next hardening increment

H03 should audit R03-B, applying the same downstream-observation rule. It should distinguish contracts whose mode/page/macro semantics are now materially owned by Rust from those that still depend on renderer, terminal API, or response formatting.
