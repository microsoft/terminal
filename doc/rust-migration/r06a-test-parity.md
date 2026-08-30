# R06-A Microsoft host test parity

## Scope

R06-A reconciles the complete Microsoft `host` source suite under `src/host/ut_host` against the current Rust ownership in `terminal-host`, `terminal-buffer`, and the small amount of shared deterministic `terminal-core` geometry/selection behavior.

The frozen global census contains **331 source `TEST_METHOD` identities** in this suite. TAEF expands those methods to the existing **5,101 runtime invocations** recorded in `tools/rust/contract-baseline.json`.

Source methods and runtime invocations are intentionally different levels of evidence. The 331 source contracts are the traceable reconciliation surface; all 5,101 runtime invocations remain part of Microsoft certification.

## Frozen source families

| Microsoft source | Source methods |
|---|---:|
| `AliasTests.cpp` | 5 |
| `ApiRoutinesTests.cpp` | 16 |
| `ClipboardTests.cpp` | 5 |
| `ConsoleArgumentsTests.cpp` | 10 |
| `HistoryTests.cpp` | 11 |
| `InitTests.cpp` | 1 |
| `InputBufferTests.cpp` | 20 |
| `ObjectTests.cpp` | 1 |
| `OutputCellIteratorTests.cpp` | 18 |
| `ScreenBufferTests.cpp` | 113 |
| `SearchTests.cpp` | 13 |
| `SelectionTests.cpp` | 4 |
| `TextBufferIteratorTests.cpp` | 29 |
| `TextBufferTests.cpp` | 45 |
| `TitleTests.cpp` | 1 |
| `UtilsTests.cpp` | 1 |
| `ViewportTests.cpp` | 23 |
| `VtIoTests.cpp` | 15 |
| **Total** | **331** |

The validator emits these family counts directly from the frozen source inventory. Upstream method additions, removals, or renames still change the suite fingerprint and fail the global census gate.

## R06-A result

The machine ledger freezes this distribution:

```text
host=331; runtime=5101; Exact=11, Partial=105, Missing=210, Platform-only=5
```

Every host source method is now deliberately classified by an exact entry or source-family rule. The validator no longer permits the `host` suite to silently fall back to its default classification.

## Eleven Exact contracts

### OutputCellIterator — six

`rust/terminal-buffer/tests/microsoft_host_r06a_contract.rs` reproduces Microsoft vectors against the already-migrated safe Rust `OutputCellIterator`:

- `StringData`
- `FullWidthStringData`
- `StringDataWithColor`
- `FullWidthStringDataWithColor`
- `DistanceStandard`
- `DistanceFullWidth`

The witnesses use Microsoft's pangram, five Katakana full-width codepoints, and the mixed `QWER + Katakana + TYUI` distance shape. They check narrow vs. leading/trailing cells, input position, destination-cell distance, and stored/current attribute behavior.

The remaining twelve `OutputCellIteratorTests.cpp` methods stay Partial because the Rust product type does not reproduce every C++ constructor surface, especially attribute-only, `CHAR_INFO`, `OutputCell` run, and unlimited fill-generator forms.

### VtIo — four

`rust/terminal-host/tests/microsoft_host_r06a_contract.rs` reproduces four pure deterministic VtIo byte-format contracts:

- `SetConsoleCursorPosition` — same four CUP coordinates and concatenated bytes.
- `SetConsoleTitleW` — same plain-title, C0, DEL and C1 sanitization vectors followed by OSC-0 framing.
- `SetConsoleCursorInfo` — same DECTCEM hidden/visible sequence.
- `SetConsoleTextAttribute` — all sixteen foregrounds, all sixteen backgrounds, and both reverse-video vectors.

Other VtIo source methods remain Partial because Microsoft executes them through live `ApiRoutines`, output pipes and screen-buffer state. Rust already owns deterministic sequence, sanitization, attribute and CHAR_INFO formatting, but not that complete Windows aggregate.

### Host coordinate comparison — one

`rust/terminal-core/tests/microsoft_host_r06a_contract.rs` reproduces every `UtilsTests::TestCompareCoords` ordering case against the product `BufferPoint` ordering: equal, left/right on a row, above/below on a column, and all four diagonal relationships. This contract is Exact.

## Why 105 are Partial

Partial is used only where real migrated product behavior exists but Microsoft asserts a wider aggregate:

- `ConsoleArgumentsTests.cpp` — Rust owns parsing after tokenization and client command-line escaping; raw `CommandLineToArgvW` tokenization remains Windows-owned.
- remaining `OutputCellIteratorTests.cpp` — core iteration semantics are migrated, while several C++-specific constructor forms are not.
- `SelectionTests.cpp` — Rust owns selection spans and deterministic keyboard movement; the legacy Selection singleton, console globals, cooked-read/history integration remain outside that owner.
- `TextBufferTests.cpp` — rows, attributes, wide glyph handling and resize/reflow are migrated; circular host buffers, renderer/VT orchestration, API write paths, clipboard extraction, hyperlinks and prompt regions are broader.
- `ViewportTests.cpp` — Rust preserves inclusive/exclusive rectangle geometry but does not expose the complete C++ Viewport API surface.
- remaining `VtIoTests.cpp` — deterministic formatting exists in Rust, while live pipe and screen-buffer mutation remains native.

A source method is not promoted merely because a lower-level Rust helper has a similar name.

## Why 210 remain Missing

The largest Missing blocks are intentional product migration backlog:

| Family | Missing methods | Reason |
|---|---:|---|
| `ScreenBufferTests.cpp` | 113 | No single safe Rust screen-buffer aggregate owns cursor, viewport, renderer, selection, VT state, API mutation and reflow coordination end-to-end. |
| `TextBufferIteratorTests.cpp` | 29 | The tests target concrete C++ iterator operator APIs; Rust currently uses different iteration mechanics without a direct migrated product abstraction. |
| `InputBufferTests.cpp` | 20 | Queue insertion/coalescing, flush/peek/read, wait events, DBCS padding, suspension and stream de-coalescing are unported. |
| `ApiRoutinesTests.cpp` | 16 | Full console API/global-state behavior, input modes, cooked reads, titles/codepages, waits, writes and scrolling remain native. |
| `SearchTests.cpp` | 13 | The ICU-backed console search adapter is not migrated; this is the same search debt exposed by R04. |
| `HistoryTests.cpp` | 11 | Command history allocation, reuse, resize, duplicate policy and session persistence have no Rust owner. |
| `AliasTests.cpp` | 5 | DOSKEY alias storage and macro expansion are unported. |
| clipboard plain-text extraction | 2 | Rust TextBuffer lacks the C++ `CopyRequest`/`GetPlainText` block/line extraction contract. |
| `TitleTests.cpp` | 1 | `TranslateConsoleTitle` environment unexpansion/path substitution is distinct from migrated OSC title framing. |
| **Total** | **210** | |

No test-only production abstraction was added to erase these Missing rows.

## Five Platform-only contracts

- `InitTests::TestGetConsoleLangId` preserves Windows loader/code-page-to-LANGID behavior.
- `ObjectTests::TestFailedHandleAllocationWhenNotShared` exercises Windows console handle/share-count ownership and sharing violations.
- the three keyboard-conversion methods in `ClipboardTests.cpp` rely on Win32 keyboard-layout mapping, AltGr/numpad behavior and platform input integration.

The two clipboard buffer-extraction methods are explicitly Missing rather than hidden under the platform rule.

## Machine enforcement

`tools/rust/microsoft-rust-equivalence-r06a.json` records the exact entries and source-family rules. `tools/rust/Test-MicrosoftGlobalTestInventory.ps1` now treats `host` as a reconciled-stage suite.

CI therefore fails if:

- the frozen Microsoft host source identity set changes;
- a host method has neither an explicit entry nor a source-family rule;
- an overlay references a removed source/method;
- a non-missing family rule has no Rust witness;
- or the exact `11 / 105 / 210 / 5` coverage distribution drifts without deliberate reconciliation.

## Safety

This increment changes only tests, parity metadata, validation diagnostics and documentation.

- Product Rust semantic implementation changed: **0**
- Product C++ changed: **0**
- FFI changed: **0**
- Microsoft tests removed or weakened: **0**
- Existing certification gates relaxed: **0**

The complete Microsoft host runtime suite remains part of full R08/R09 certification.

## Next lane

R06-B can now focus on the separate **server/interactivity Win32** surface instead of mixing it with the 331-method host suite. The host ledger also gives the functional migration lane an explicit backlog: screen-buffer aggregate, input buffer, API routines, ICU search, history, aliases, iterator replacement and clipboard text extraction.
