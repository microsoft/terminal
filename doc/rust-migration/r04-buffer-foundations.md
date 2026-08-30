# R04a — TextBuffer foundations

R04 begins the `TextBuffer` migration by porting the deterministic value types that `ROW` and `TextBuffer` depend on. The goal is to avoid a temporary parallel type system when row storage is moved in R04b.

## In scope

- Minimal TIL-compatible `Point`, `Rect`, and `InclusiveRect` geometry.
- VT `LineRendition` and the exact screen/buffer coordinate transforms used by `LineRendition.hpp`.
- `TextColor` default, 16-color index, 256-color index, and RGB states.
- Color-table resolution and intense/bright behavior.
- Exact Windows/ANSI legacy-index transposition.
- Exact 256-color and compressed-RGB fallback tables used when producing legacy 16-color attributes.
- `TextAttribute` rendition flags, underline styles, grid lines, reverse video, protection, hyperlinks, marks, and standard-erase behavior.
- Legacy attribute round-tripping, including configurable foreground/background defaults.

## Intentional Rust boundary

The C++ `TextAttribute` stores the current legacy default-color conversion in mutable process-global static tables. R04a represents that state as an explicit `LegacyColorDefaults` value passed to legacy conversion operations. This keeps product Rust free of shared mutable globals while preserving the observable mapping.

## Safety

`terminal-buffer` is a product crate and uses `#![forbid(unsafe_code)]`.

No C++, FFI, Win32, renderer, UIA, or heap-pointer row layout is introduced in R04a.

## Next slice

R04b will port `ROW` storage and glyph/column navigation on top of these types. That slice will replace the C++ row's raw `wchar_t*`/`uint16_t*` offset storage with owned safe Rust containers while retaining the observable UTF-16, wide-glyph, dirty-range, wrapping, and line-rendition semantics.
