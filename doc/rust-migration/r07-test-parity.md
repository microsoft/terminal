# R07 renderer test parity

R07 is a different kind of parity increment from R02-R06.

The frozen Microsoft TAEF baseline contains 12 suites and **does not contain a standalone renderer suite**. Creating a thirteenth synthetic suite would make the global contract ledger look more complete while weakening its meaning. Instead, R07 reconciles the renderer migration at the **source-ownership seam**: each deterministic policy extracted from C++ is tied to its Rust owner and every one of the 50 `terminal-renderer` tests is machine-checked as a witness.

## Result

The R07 renderer stage is represented by:

```text
split C++ source families = 6
Rust policy owners        = 12
Rust tests/witnesses      = 50
retained native engines   = 3
unmapped Rust tests       = 0
```

No product implementation was changed by this parity increment. The existing 50 Rust tests were already sufficiently granular; the missing artifact was durable traceability back to the C++ responsibilities they replace.

## Split ownership

| C++ source | Rust policy owner(s) | Witnesses | Native responsibility intentionally retained |
|---|---|---:|---|
| `src/renderer/base/CSSLengthPercentage.cpp` | `css_length_percentage.rs` | 6 | C++ caller and `wchar_t`/`wcstof` integration |
| `src/renderer/base/FontInfoDesired.cpp` | `font_info_desired_policy.rs` | 5 | Full `FontInfoDesired` object, glyph options and engine handoff |
| `src/renderer/base/fontinfo.cpp` | `font_info_policy.rs` | 4 | Object construction, fallback state and engine updates |
| `src/renderer/base/FontInfoBase.cpp` | `font_info_base_policy.rs` | 4 | code-page/default-font-list lookup and engine object state |
| `src/renderer/base/RenderSettings.cpp` | `render_settings_policy.rs`, `attribute_color_policy.rs` | 10 | color-table lookup, `ColorFix`, underline-color and renderer integration |
| `src/renderer/base/renderer.cpp` | `title_state.rs`, `timer_policy.rs`, `retry_policy.rs`, `redraw_region.rs`, `viewport_update.rs`, `rendition_blink.rs` | 21 | render thread, events/locks, Win32 timer wait, engine calls and `Present` orchestration |

Total: **50 Rust witnesses**.

## What the 50 witnesses prove

### CSS lengths — 6

The Rust policy covers invalid/unset input, unitless and percentage values, CSS pixels and points, `ch`, `wcstof`-compatible leading whitespace behavior, signs and exponent forms. Resolution retains the C++ reference-frame rules.

### Font policy — 13

`FontInfoDesired`, `FontInfo`, and `FontInfoBase` evidence covers:

- TrueType engine width suppression;
- raster size preservation;
- default-raster identity and the legacy 8x12 convention;
- zero-width/zero-height validation;
- preservation of scaled/unscaled valid sizes;
- fixed 32-cell legacy face-name truncation/termination;
- TrueType family-bit classification.

The default-font-list/code-page substitution logic remains C++ because it is wider engine/platform integration rather than the deterministic policy extracted by R07.

### RenderSettings and attribute colors — 10

Rust owns the deterministic mode/reset/blink state and the post-lookup color effects that can be expressed without the C++ renderer aggregate:

- independent render modes;
- programmable-mode reset;
- blink rendition toggle;
- faint/dim foreground;
- reverse-video XOR screen-reverse behavior;
- invisible foreground-to-background projection;
- default-background transparency versus opaque custom/reversed/invisible backgrounds.

The C++ side still owns palette lookup, brightening through the live color table, `ColorFix` indistinguishability adjustment, underline colors and application to renderer engines. Therefore this is deliberately **split ownership**, not a claim that all of `RenderSettings.cpp` moved to Rust.

### Renderer controller policy — 21

The Rust witnesses isolate deterministic decisions from `renderer.cpp`:

- title invalidation/commit decisions;
- saturating timer arithmetic and DWORD millisecond conversion;
- repeating timer rescheduling without drift;
- initial render plus five exponential-backoff retries;
- redraw clipping and double-width-row expansion;
- viewport force-resync and scroll-delta planning;
- one-second rendition-blink timer start/stop/no-op decisions.

The render thread, synchronization primitives, console lock, Win32 wait/timer clock, engine invocation and presentation lifecycle remain native.

## Retained native renderer boundaries

R07 explicitly does **not** migrate the graphics or accessibility backends merely to obtain Rust coverage:

- `src/renderer/atlas/AtlasEngine.cpp` — Direct2D/DirectWrite/COM/GPU-backed Atlas engine;
- `src/renderer/gdi/paint.cpp` — GDI/HDC paint path;
- `src/renderer/uia/UiaRenderer.cpp` — native UI Automation renderer/provider notification path.

This is consistent with R06-B, where the Win32 UIA text-range contract also remains native/platform-owned.

## Machine enforcement

`tools/rust/r07-renderer-source-map.json` is the source-of-truth ownership map. `tools/rust/Test-RustRendererSourceMap.ps1` verifies:

1. all six split C++ source files still exist;
2. source-level semantic anchors still exist in those files;
3. all twelve Rust policy files still exist;
4. every mapped Rust witness still exists as a test function;
5. each witness is mapped exactly once;
6. every real `#[test]` under `rust/terminal-renderer/src` is present in the map;
7. the renderer crate retains `#![forbid(unsafe_code)]`;
8. the expected totals remain exactly `split=6`, `native=3`, `Rust owners=12`, `Rust witnesses=50`.

This closes a gap left by a simple aggregate test count: future R07 policy tests cannot appear without an explicit C++ ownership decision.

## Relationship to the Microsoft TAEF oracle

The global Microsoft census remains unchanged at **1,098 source methods across 12 suites** and **6,998 runtime invocations**. R07 does not add fake TAEF identities to that ledger.

Where renderer behavior is exercised transitively by existing Microsoft suites, the Microsoft runtime oracle remains authoritative. The R07 source map supplements that oracle by proving the exact ownership of policy code that was extracted even though Microsoft does not expose it as a standalone `ut_renderer` TAEF suite.

## Safety

- Product Rust changed by parity increment: **0**
- Product C++ changed: **0**
- FFI changed: **0**
- New test-only product abstractions: **0**
- Existing Microsoft tests weakened: **0**
- Graphics/COM boundaries artificially moved to Rust: **0**

The next planned parity increment can proceed to the R08 managed/product surfaces: Settings, Control and TerminalApp, distinguishing C#/XAML ownership from native C++ semantics that actually transferred to Rust.
