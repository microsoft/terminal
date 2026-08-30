# R04 Microsoft-to-Rust test parity

> **Historical baseline:** this document records the original R04 delivery. H04 later hardens the same frozen source surface and supersedes the current classification counts; see `h04-r04-foundation-hardening.md` for the post-audit distribution.

R04 reconciles the Microsoft source-test contracts owned by the TextBuffer and shared foundation layer against the current Rust migration.

## Scope

The frozen Microsoft census contains 229 source `TEST_METHOD` identities in the R04 surface:

| Suite | Source methods | TAEF runtime baseline |
| --- | ---: | ---: |
| `textBuffer` | 14 | 13 |
| `types` | 16 | 16 |
| `til` | 199 | 278 |
| **R04 total** | **229** | **307** |

This delivery does not equate a C++ helper type with a Rust type merely because their APIs look similar. Rust standard-library replacements are credited only for the observable behavior they materially witness, and therefore remain `Partial` unless the complete Microsoft contract is represented.

## Result

| Suite | Exact | Partial | Missing | Total |
| --- | ---: | ---: | ---: | ---: |
| `textBuffer` | 5 | 8 | 1 | 14 |
| `types` | 0 | 14 | 2 | 16 |
| `til` | 0 | 171 | 28 | 199 |
| **R04 total** | **5** | **193** | **31** | **229** |

The global inventory gate expands the source-family rules in `tools/rust/microsoft-rust-equivalence-r04.json` against every frozen `TEST_METHOD`, so source-family classification does not weaken source-method traceability. A Microsoft source-method addition/removal changes the fingerprint, and a classification-count change breaks `expectedCoverage`.

## Exact contracts

All five `TextColorTests.cpp` methods are `Exact`:

- `TestBrightIndexColor`
- `TestChangeColor`
- `TestDarkIndexColor`
- `TestDefaultColor`
- `TestRgbColor`

Rust owns the same default, indexed-16, indexed-256 and RGB states, mutation transitions, color-table resolution and brighten behavior in `terminal-buffer::text_color`.

## Partial contracts

### TextBuffer

`ReflowTests.cpp` remains `Partial`. Rust materially owns deterministic width reflow, forced-wrap chains and wide-glyph boundaries. The new Microsoft-derived witness also records an important invariant: a forced-wrap row preserves its readable trailing space as part of the logical chain, while a normal row is measured to its rightmost content. The complete upstream data-driven `ReflowTestCases` matrix has not yet been reproduced vector-for-vector.

`TextAttributeTests.cpp` remains `Partial`. Rust owns legacy Windows color ordering, metadata bits and configurable default-color round trips. Renderer-resolved attribute colors, reverse-default projection and the intense-as-bright rendering policy still cross the renderer boundary.

### Types

`CodepointWidthDetectorTests.cpp` remains `Partial`. Rust owns deterministic full-width classification for ASCII/ambiguous, CJK/fullwidth, supplementary-plane emoji and malformed UTF-16. Microsoft's full Unicode grapheme-break and chunked-text traversal contract is larger than the current Rust width detector.

`UtilsTests.cpp` remains `Partial`. Portable splitting, parsing, string and color behavior already has Rust evidence, while GUID formatting, Windows/WSL path transformation, paste filtering and starting-directory policy belong to later platform/product ownership.

### TIL

TIL is not one buffer component; the 199 source methods span containers, geometry, run-length encoding, strings, Unicode conversion, synchronization, hashing, math and platform helpers.

Where C++ helper types disappear naturally in Rust, this delivery tests the portable observable replacement rather than recreating test-only C++ abstractions. Examples include:

- `std::option::Option` for coalescing semantics;
- `Vec`, `BTreeSet` and `BTreeMap` for portable collection observables;
- Rust `String` operations and numeric parsing for portable string helpers;
- `char::decode_utf16` / `String::from_utf16` for scalar UTF-16 behavior;
- `Mutex` and `sync_channel` for basic synchronization/order witnesses;
- `terminal-buffer::geometry` for currently migrated point/rectangle storage and dimensions;
- `terminal-buffer::rle::Rle` for canonical RLE construction, lookup and replacement.

These are deliberately `Partial` because TIL also tests implementation-specific APIs, casts, iterators, small-buffer details, Windows types, incremental conversion state, exact hashing algorithms or a wider arithmetic surface not owned by the current Rust migration.

## Remaining Missing contracts

Thirty-one source methods intentionally remain `Missing` because no honest current Rust owner exists:

| Source | Methods | Reason |
| --- | ---: | --- |
| `textBuffer/UTextAdapterTests.cpp` | 1 | ICU/search-equivalent TextBuffer substring adapter is not migrated. |
| `types/UuidTests.cpp` | 2 | UUID-v5 generation is not currently owned/consumed by the Rust product surface. |
| `til/EnvTests.cpp` | 4 | Environment-block generation/expansion is a host/platform responsibility. |
| `til/GenerationalTests.cpp` | 1 | No migrated generational-wrapper owner exists. |
| `til/SizeTests.cpp` | 22 | R04 has dimensions on concrete types but no standalone `til::size` equivalent with the full arithmetic/cast surface. |
| `til/throttled_func.cpp` | 1 | No migrated Rust throttled-function scheduler exists. |
| **Total** | **31** | |

Adding test-only implementations solely to turn these rows green would create false parity. They should become non-Missing only when a migrated product or platform boundary genuinely owns the behavior.

## New Rust witnesses

`rust/terminal-buffer/tests/microsoft_r04_foundation_contract.rs` adds focused witnesses for:

- `TextColor` state and mutation;
- legacy `TextAttribute` round trips;
- reflow + wide-glyph/wrap behavior;
- Unicode width classification;
- point/rectangle foundation semantics;
- RLE construction, lookup, replacement and canonicalization;
- Option/vector/set/map replacement observables;
- string split/replace/case/numeric parsing;
- UTF-16 scalar decoding and round trip;
- floor/ceiling/rounding/division basics;
- mutex/SPSC ordering basics;
- same-process Rust hashing behavior.

These tests complement existing `terminal-buffer` unit tests rather than duplicate them.

## CI enforcement

`Test-MicrosoftGlobalTestInventory.ps1` now supports machine-readable source-family overlays while preserving exact Microsoft source identities. For R04 it enforces all of the following:

1. source fingerprints still match the frozen Microsoft census;
2. every `textBuffer`, `types` and `til` source method is deliberately reconciled by an exact entry or source-family rule;
3. non-Missing source rules carry at least one named Rust witness;
4. source rules may not reference removed Microsoft source files;
5. the exact R04 coverage distribution must remain:
   - `textBuffer`: `Exact=5, Partial=8, Missing=1`;
   - `types`: `Partial=14, Missing=2`;
   - `til`: `Partial=171, Missing=28`.

## Safety

This increment changes no product Rust behavior, no C++ implementation and no FFI surface. It adds tests, machine-readable evidence and CI reconciliation logic only.
