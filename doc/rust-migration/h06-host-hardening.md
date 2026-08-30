# H06 — R06-A Host hardening

H06 re-audits the frozen Microsoft `host/ut_host` source surface against the Rust owners available after the later migration stages.

The hardening rule remains strict: `Exact` requires the Microsoft source method's material observable to be reproduced by a real migrated owner. Lower-level resemblance, parser survival, or a test-only facade is not enough.

## Frozen Host surface

The Microsoft Host suite remains:

- 331 distinct `TEST_METHOD` identities
- 5,101 runtime TAEF invocations

H06 changes no Microsoft source census.

## H06 distribution

```text
Before H06
Exact          11
Partial       105
Missing       210
Platform-only   5
Total         331

After H06
Exact          34
Partial        82
Missing       210
Platform-only   5
Total         331
```

H06 moves 23 source methods from `Partial` to `Exact`. No `Missing` contract is reclassified merely to improve the number.

## Why Viewport is a real Rust owner

All 23 methods in `ViewportTests.cpp` exercise deterministic geometry. Microsoft's product implementation lives in `src/types/viewport.cpp`; it does not depend on Win32 handles, ConPTY pipes, COM, the renderer, console globals, or a live screen buffer.

Before H06, Rust exposed only `Point`, exclusive `Rect`, and `InclusiveRect`, so the Host ledger could only classify the Viewport family as `Partial`.

H06 adds `rust/terminal-buffer/src/viewport.rs` as the platform-neutral product owner for the same semantics:

- empty/inclusive/exclusive/dimensions construction
- inclusive and exclusive edges
- dimensions and origin
- point and viewport containment
- point and viewport clamping
- row-major increment, decrement, and walking
- row-major comparison distance
- offset
- union and intersection
- subtraction into zero through four non-overlapping regions

This is product migration, not a parity-only testing abstraction. Other Rust buffer, selection, or host code can reuse the same owner.

## 23 Exact promotions

H06 adds one direct Microsoft-derived witness per frozen source method:

1. `CreateEmpty`
2. `CreateFromInclusive`
3. `CreateFromExclusive`
4. `CreateFromDimensionsWidthHeight`
5. `CreateFromDimensions`
6. `CreateFromDimensionsNoOrigin`
7. `IsInBoundsCoord`
8. `IsInBoundsViewport`
9. `ClampCoord`
10. `ClampViewport`
11. `IncrementInBounds`
12. `DecrementInBounds`
13. `MoveInBounds`
14. `CompareInBounds`
15. `Offset`
16. `Union`
17. `Intersect`
18. `SubtractFour`
19. `SubtractThree`
20. `SubtractTwo`
21. `SubtractOne`
22. `SubtractZero`
23. `SubtractSame`

The H06 `MoveInBounds` witness is deliberately stronger than the randomized Microsoft loop: it exhausts every starting point in the 20×20 viewport against a fixed family of representative deltas, including row boundaries and the maximum in-bounds distance. This removes random-test variance while preserving Microsoft's observable row-major contract.

The six subtraction witnesses preserve Microsoft's exact result ordering: top, bottom, left, right, filtering only invalid regions.

## Deliberate non-promotions

### ConsoleArgumentsTests remains Partial

Rust owns `ConsoleArguments::ParseCommandline` behavior after argument tokenization through `rust/terminal-host/src/console_argument_parser.rs`. The Microsoft source methods begin with the raw Windows command line and therefore include `CommandLineToArgvW` tokenization behavior.

Promoting these ten source methods to `Exact` would conflate a retained Windows boundary with the deterministic parsing core, so H06 keeps them `Partial`.

### Remaining OutputCellIterator methods remain Partial

Six text-backed iterator contracts were already Exact before H06. The remaining source methods exercise constructor surfaces Rust does not yet own end-to-end:

- character fill, including unlimited fill
- attribute-only fill
- text + attribute fill constructors
- `CHAR_INFO` runs
- legacy color runs
- `OutputCell` runs

The existing Rust iterator owns UTF-16 text-backed iteration, width expansion, stored attributes, and destination-cell limits. H06 does not fabricate the remaining constructors solely for parity.

### Remaining VtIo methods remain Partial

Four pure VT byte-format contracts were already Exact. The other Microsoft methods drive `ApiRoutines`, live screen-buffer state, pipes, writes/fills, scrolling, and downstream mutation. Rust has several deterministic helpers for those paths, but a helper action is not the same as the complete observable. They remain `Partial`.

### Missing remains honest

The 210 Host `Missing` methods are unchanged. Major families still include:

- DOSKEY alias/history ownership
- console API/global-state behavior
- input queue/coalescing/wait signaling
- live ScreenBuffer aggregate behavior
- ICU-backed search
- C++ TextBuffer iterator API contracts without a direct Rust product equivalent
- title environment/filesystem translation
- clipboard extraction paths without a migrated CopyRequest/GetPlainText owner

Win32-only clipboard, language, and console-object handle contracts remain `Platform-only`.

## Global snapshot

H06 advances the versioned global hardening snapshot to priority 6:

```text
Exact         159
Stronger       11
Partial       351
Platform-only  68
UI-managed     22
Missing       487
Total        1098
```

The global gate still calculates coverage independently and requires exact equality with the highest-priority snapshot.

## Safety

H06 deliberately changes one platform-neutral Rust product owner plus tests, parity metadata, and documentation.

- Microsoft C++ product changed: 0
- Microsoft tests removed or weakened: 0
- Win32/ConPTY ownership moved artificially: 0
- FFI changed: 0
- managed/XAML changed: 0
- certification gates relaxed: 0
- parity-only fake Host/ScreenBuffer aggregates added: 0

CI on the final H06 head is authoritative for formatting, Clippy, Linux/Windows tests, source census, and witness integrity.

## Next lane

The next planned hardening delivery is H07 / R06-B, re-auditing Server and Interactivity Win32 seams while preserving the native boundary where Microsoft behavior is intrinsically platform-owned.
