# R04b — ROW storage and navigation

R04b ports the deterministic storage contract of `ROW` onto the safe Rust foundations introduced in R04a.

## In scope

- Owned UTF-16 row text storage.
- Column-to-character offsets compatible with the C++ `CharOffsetsTrailer = 0x8000` representation.
- One- and two-column glyph replacement.
- Correct cleanup when a narrow write intersects either half of an existing wide glyph.
- Glyph-start/glyph-end adjustment and previous/next cursor navigation.
- DBCS-style leading/trailing classification for wide glyph cells.
- Character-offset to leading/trailing column mapping, including offsets inside surrogate pairs.
- Readable-column behavior for double-width/double-height lines and double-byte padding.
- Half-open attribute replacement and set-to-end operations.
- Text ranges, last-non-space measurement, wrapping measurement, and delimiter classification.

## Safety improvement

The C++ row deliberately disables several bounds/pointer-arithmetic warnings and can switch between a shared inline character buffer and a heap allocation. The Rust row owns `Vec<u16>` buffers for text, offsets, and attributes. Offset changes are validated before they are committed, and the crate remains covered by `#![forbid(unsafe_code)]`.

The high-bit trailer representation is retained because it is observable through glyph/column navigation semantics and provides a compact compatibility bridge. It no longer acts as an unchecked pointer offset.

## Deferred

- Unicode grapheme segmentation and `CodepointWidthDetector` integration for general `ReplaceText`.
- The optimized ASCII/Unicode bulk-write paths.
- TIL `small_rle`-equivalent attribute compression; R04b keeps the same per-column attribute semantics with flat safe storage first.
- `OutputCellIterator` integration.
- `ImageSlice` ownership.
- Prompt/scrollbar mark metadata.
- Full row-copy/reflow integration with `TextBuffer`.
