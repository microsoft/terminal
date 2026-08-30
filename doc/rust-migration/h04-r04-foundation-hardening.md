# H04 — R04 foundation hardening

H04 re-audits the R04 `textBuffer`, `types`, and `til` Microsoft source contracts after the later Rust migration stages have landed. The hardening rule is stricter than simple API resemblance:

- `Exact` requires the complete Microsoft source-method observable to be materially reproduced by an existing Rust owner.
- `Platform-only` is used when the Microsoft source method is explicitly testing a retained native ABI/platform boundary.
- `Missing` remains correct when no migrated Rust product owner exists.
- no test-only product abstraction is introduced merely to improve the coverage histogram.

## Scope

The frozen R04 census remains unchanged:

| Suite | Source methods | TAEF runtime |
| --- | ---: | ---: |
| `textBuffer` | 14 | 13 |
| `types` | 16 | 16 |
| `til` | 199 | 278 |
| **Total** | **229** | **307** |

## H04 result

| Suite | Exact | Partial | Platform-only | Missing | Total |
| --- | ---: | ---: | ---: | ---: | ---: |
| `textBuffer` | 5 | 8 | 0 | 1 | 14 |
| `types` | 0 | 14 | 0 | 2 | 16 |
| `til` | 3 | 168 | 5 | 23 | 199 |
| **R04 total** | **8** | **190** | **5** | **26** | **229** |

Compared with the original R04 delivery, H04 makes eight source-method classifications more precise:

- three `Partial -> Exact` promotions backed by direct Rust `Rle<T>` witnesses;
- five `Missing -> Platform-only` corrections for explicit Win32/Direct2D interoperability.

No product Rust, C++, FFI, or managed/XAML code changes are required.

## Exact promotions: `RunLengthEncodingTests.cpp`

### `ConstructWithLengthAndValue`

Microsoft constructs an RLE sequence of five copies of value `1` and verifies the decoded observable. `Rle::new(5, 1)` owns the same complete behavior. H04 adds:

- `microsoft_r04_rle_construct_with_length_and_value_matches_source_contract`

### `CopyAndMove`

The Microsoft method observes:

1. swapping full/empty containers;
2. copying the full value;
3. moving the full value into the destination.

It does **not** assert C++-specific moved-from state. Rust `Clone`, `std::mem::swap`, and ownership move therefore reproduce all asserted observables without fabricating C++ lifetime mechanics. H04 adds:

- `microsoft_r04_rle_copy_swap_and_move_observables_match_source_contract`

### `Comparison`

Microsoft compares two equal RLE values, mutates one, then verifies equality/inequality changes. Rust `Rle<T>: PartialEq + Eq` plus the migrated replacement operation reproduces the complete contract. H04 adds:

- `microsoft_r04_rle_comparison_matches_source_contract`

The remaining RLE methods stay `Partial`. In particular, Rust still does not expose the complete Microsoft initializer-list, throwing `at`, slicing, arbitrary-RLE insert/remove replacement, `replace_values`, trailing-extent resize, or random-access iterator contracts.

## Platform-only corrections

### `EnvTests.cpp::Generate`

This Microsoft test dynamically loads `shell32.dll`, resolves `RegenerateUserEnvironment`, and compares the generated Windows user environment block. That is explicitly a native Windows boundary and should not have been counted as missing portable Rust foundation behavior.

The other three `EnvTests.cpp` methods remain `Missing`: there is still no migrated Rust `til::env`-equivalent product owner for construction/serialization/percent-expansion semantics.

### `SizeTests.cpp`

Four of the 22 source methods are explicit platform interoperability contracts:

- `CoordConstruct` — Win32 `COORD` -> `til::size`;
- `SizeConstruct` — Win32 `SIZE` -> `til::size`;
- `CastToSize` — `til::size` -> Win32 `SIZE` with narrowing policy;
- `CastToD2D1SizeF` — `til::size` -> Direct2D `D2D1_SIZE_F`.

These are now `Platform-only`. The remaining 18 methods stay `Missing`, because the Rust migration still has dimensions on concrete types but no standalone product-owned `til::size` replacement with the full construction, boolean, arithmetic, scaling, division, area, narrowing, and floating-rounding contract.

## Remaining 26 Missing contracts

| Source | Missing methods after H04 | Why they remain Missing |
| --- | ---: | --- |
| `textBuffer/UTextAdapterTests.cpp` | 1 | Rust `TextBuffer` has selection/reflow but no Unicode substring-search owner equivalent to `SearchText`. |
| `types/UuidTests.cpp` | 2 | No UUID-v5 implementation is consumed by the Rust workspace; adding SHA-1/UUID code only for parity would be test-only product fiction. |
| `til/EnvTests.cpp` | 3 | Portable env table construction/serialization/expansion has no migrated Rust owner. |
| `til/GenerationalTests.cpp` | 1 | No migrated generational/value-version wrapper owns this contract. |
| `til/SizeTests.cpp` | 18 | No standalone Rust size value owns the portable arithmetic/cast surface. |
| `til/throttled_func.cpp` | 1 | No migrated throttled scheduling primitive is consumed by the Rust product surface. |
| **Total** | **26** | |

## Why H04 does not implement the remaining Missing rows

The hardening lane is evidence work, not a shadow product migration. A UUID-v5 helper, `Size` clone, environment class, generational wrapper, Unicode search adapter, or throttled scheduler created only under `tests/` would make the ledger look greener while leaving the real product unchanged.

Those contracts should move out of `Missing` only when a functional migration stage introduces or identifies the real Rust owner.

## Machine-readable enforcement

`tools/rust/microsoft-rust-equivalence-r04.json` now contains source-specific overrides above the existing source-family rules. The global gate therefore verifies both the original source census and the H04 distribution:

```text
textBuffer=14; Exact=5, Partial=8, Missing=1
types=16; Partial=14, Missing=2
til=199; Exact=3, Partial=168, Platform-only=5, Missing=23
```

Expected global coverage after H04 is:

```text
Exact         130
Stronger       11
Partial       380
Platform-only  68
UI-managed     22
Missing       487
Total        1098
```

CI output is authoritative.

## Safety

- Product Rust changed: **0**
- Product C++ changed: **0**
- FFI changed: **0**
- managed/XAML changed: **0**
- Microsoft tests removed/weakened: **0**
- certification gates relaxed: **0**

H04 changes only Rust contract tests, parity metadata, and migration documentation.
