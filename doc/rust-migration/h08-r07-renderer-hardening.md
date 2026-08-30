# H08 — R07 renderer hardening

H08 turns the R07 renderer ownership inventory into a fail-closed audit. It does not invent a Microsoft TAEF suite for Renderer: the frozen 1,098-source-method census has no standalone renderer suite, so the global Microsoft coverage distribution remains unchanged from H07.

## Starting point

R07 already had a useful safe-Rust renderer policy crate and a source map:

- 6 split C++ source files
- 3 explicitly native graphics/UIA source files
- 12 Rust owner modules
- 50 Rust witnesses

The existing gate verified that sources, source-pattern strings, Rust owners and all 50 Rust tests still existed. It did not pin the audited C++ source content, and it did not require every extracted C++ method to carry an explicit completeness classification.

The re-audit found two concrete traceability gaps:

1. `rust/terminal-renderer/src/title_state.rs` actually owns deterministic state from `RenderEngineBase::InvalidateTitle` and `RenderEngineBase::UpdateTitle`, but `src/renderer/base/RenderEngineBase.cpp` was not represented in the source map. The witnesses were loosely attached to `renderer.cpp`.
2. `renderer.cpp` witnesses exercise `Renderer::_timerSaturatingSub` and the repeating reschedule semantics in `Renderer::_tickTimers`, but those methods were absent from the old source-pattern anchors.

H08 corrects both gaps.

## Hardened ownership map

The R07 map is now schema v2 and records:

```text
split C++ sources       7
native C++ sources      3
Rust owners            12
Rust witnesses         50
pinned C++ source blobs 10
method contracts       25
```

Every audited C++ source is pinned to its exact Git blob. CI resolves `HEAD:<sourcePath>` and fails if the current blob differs. A Microsoft/upstream edit can therefore no longer inherit an old split/native or completeness decision silently; the source must be re-audited and explicitly repinned.

Every extracted source pattern in a split source also requires exactly one method-level classification and rationale.

## Method-level classifications

The 25 extracted contracts reconcile as:

```text
Exact       14
Stronger     1
Partial     10
-------------
Total       25
```

### CSS length policy

`src/renderer/base/CSSLengthPercentage.cpp`

- `CSSLengthPercentage::FromString` — **Partial**. Rust covers the supported suffixes, signs/exponents and common whitespace behavior, but C++ uses `std::wcstof`, whose full non-finite/range/underflow edge surface is wider than the hand parser. H08 does not overclaim equivalence.
- `CSSLengthPercentage::Resolve` — **Exact**. Reference-frame selection, multiplication and fallback are fully represented.

### FontInfoDesired

`src/renderer/base/FontInfoDesired.cpp`

- `GetEngineSize` — **Exact**.
- `IsDefaultRasterFont` — **Exact**.

The Rust policy completely represents TrueType width suppression and both engine-marked and legacy blank-face default-raster identity rules.

### FontInfo size validation

`src/renderer/base/fontinfo.cpp`

- `_ValidateCoordSize` — **Exact**.
- `ValidateFont` — **Exact**.

The default-raster exception, zero-width promotion, 8x12 fallback and unscaled-size update are all represented by the Rust owner.

### FontInfoBase identity helpers

`src/renderer/base/FontInfoBase.cpp`

- `FillLegacyNameBuffer` — **Stronger**. Rust preserves the truncation and null-termination observable while also zero-filling the unused fixed array. The C++ method documentation promises zeroed unused positions, but its implementation only writes the copied prefix and terminator. Calling the Rust witness merely Exact would hide that distinction.
- `IsDefaultRasterFontNoSize` — **Exact**.
- `IsTrueTypeFont` — **Exact**.

### RenderSettings

`src/renderer/base/RenderSettings.cpp`

- `SetRenderMode` — **Exact**.
- `GetRenderMode` — **Exact**.
- `ToggleBlinkRendition` — **Exact**.
- `RestoreDefaultSettings` — **Partial**: the programmable mode reset is represented, while color-table and alias restoration remain C++.
- `GetAttributeColors` — **Partial**: Rust owns dim/reverse/invisible post-processing; TextColor lookup, intense brightening and ColorFix remain native.
- `GetAttributeColorsWithAlpha` — **Partial**: Rust owns deterministic alpha post-processing after native color resolution.

### RenderEngineBase title state

`src/renderer/base/RenderEngineBase.cpp` is now a first-class split seam instead of being hidden under `renderer.cpp`.

- `InvalidateTitle` — **Exact**. Changed-title detection and invalidation state are completely represented.
- `UpdateTitle` — **Partial**. Rust represents unchanged/update-required state and commit-only-on-success semantics; `_DoUpdateTitle` and HRESULT/backend behavior remain C++.

### Renderer controller policy

`src/renderer/base/renderer.cpp`

- `_timerSaturatingAdd` — **Exact**.
- `_timerSaturatingSub` — **Exact**.
- `_timerToMillis` — **Exact**.
- `_tickTimers` — **Partial**: drift-free repeating rescheduling is represented; iteration, callback execution and exception handling remain native.
- `PaintFrame` — **Partial**: the six-attempt exponential retry schedule is represented; actual rendering, HRESULT handling, sleeps, disable and error callbacks remain native.
- `TriggerRedraw` — **Partial**: double-width expansion, clipping and viewport-origin conversion are represented; engine invalidation and wakeup remain native.
- `_CheckViewportAndScroll` — **Partial**: force/no-op, viewport replacement and scroll delta are represented; engine calls remain native.
- `_scheduleRenditionBlink` — **Partial**: start/stop/unchanged planning and the one-second interval are represented; buffer inspection and timer side effects remain native.

## Native renderer boundaries

These remain explicitly native and have no Rust shadow implementation:

- `src/renderer/atlas/AtlasEngine.cpp` — Direct2D/DirectWrite/COM/GPU-backed Atlas engine.
- `src/renderer/gdi/paint.cpp` — GDI/HDC painting.
- `src/renderer/uia/UiaRenderer.cpp` — UI Automation renderer/provider notifications.

This is intentional. R07 migrates deterministic renderer policy, not Windows graphics or UI Automation infrastructure.

## Gate behavior

`tools/rust/Test-RustRendererSourceMap.ps1` now fails closed when any of the following occurs:

- an audited C++ file changes blob content;
- a split source loses one of its anchored methods;
- an extracted source method has no Exact/Stronger/Partial classification;
- a method receives duplicate classifications;
- classification rationale is omitted;
- an owner or witness disappears;
- a Rust renderer test is added without a deliberate C++ ownership mapping;
- a mapped witness stops being a real `#[test]`;
- `terminal-renderer` loses `#![forbid(unsafe_code)]`;
- the expected 7/3/12/50/10/25 audit summary changes without an explicit map update.

## Global Microsoft census

Renderer still has no standalone suite in the frozen Microsoft TAEF census. H08 therefore deliberately freezes H07 rather than modifying it:

```text
Exact         164
Stronger       11
Partial       351
Platform-only  63
UI-managed     22
Missing       487
-----------------
Total        1098
```

The renderer-local 14 Exact / 1 Stronger / 10 Partial figures are **not** added to those numbers; they describe source seams outside the 1,098 Microsoft TEST_METHOD identities.

## Safety

H08 changes audit metadata, gates and documentation only.

- Microsoft/C++ product code changed: 0
- Rust product behavior changed: 0
- FFI changed: 0
- graphics backend ownership changed: 0
- UIA/COM ownership changed: 0
- Microsoft tests removed or weakened: 0
- certification gates relaxed: 0

The new gate is strictly stronger: previously source-pattern existence could survive arbitrary edits to the surrounding C++; now any content drift forces a deliberate re-audit.

## Next hardening increment

H09 should move to the Settings / Control / TerminalApp area, separating C#/XAML-managed ownership from C++ semantics that genuinely transferred to Rust before attempting any coverage promotion.
