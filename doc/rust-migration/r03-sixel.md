# R03a — Sixel core

R03 begins the Adapter migration after the R01-R02 semantic-core checkpoint.
The first slice ports the deterministic core of `SixelParser` into a new safe,
platform-neutral `terminal-adapter` crate.

## In scope

- DEC conformance-level cell sizes and color-table limits.
- Sixel command grammar (`#`, `!`, `$`, `-`, `+`, `"`, and sixel data bytes).
- Parameter accumulation using the parser's `MAX_PARAMETER_VALUE`.
- Macro and raster aspect-ratio behavior.
- Transparent/opaque background behavior and raster dimensions.
- VT340 base palette plus XTerm extended colors.
- DEC HLS and RGB-percentage definitions and color-number mapping.
- Repeat, carriage return, graphics next line, VT240 home, and safe indexed-pixel rasterization.

## Deferred to later R03 slices

- `AdaptDispatch` / `PageManager` integration.
- `TextBuffer` / `ImageSlice` output.
- Renderer redraw notifications.
- Margin scrolling and viewport panning.
- Partial-flush timing, packet-boundary, and palette-animation flush policy.
- C++ facade / FFI integration.

R03a intentionally replaces the raw-pointer rasterization algorithm with checked Rust indexing while preserving the observable Sixel raster semantics.
