# R06-B test parity — Server + Interactivity Win32

R06-B closes the planned server/interactivity increment without inventing a synthetic Microsoft test suite or moving Windows UI Automation/COM mechanics into portable Rust.

## Microsoft `interactivityWin32` census

The frozen global census contains **19 source `TEST_METHOD` identities** in `src/interactivity/win32/ut_interactivity_win32/UiaTextRangeTests.cpp`, expanding to **52 TAEF runtime invocations**.

The complete R06-B classification is:

```text
interactivityWin32=19; runtime=52; Platform-only=19
```

All 19 are deliberately `Platform-only`:

1. `DegenerateRangesDetected`
2. `CompareRange`
3. `CompareEndpoints`
4. `ExpandToEnclosingUnit`
5. `MoveEndpointByRange`
6. `CanMoveByCharacter`
7. `CanMoveByLine`
8. `CanMoveEndpointByUnitCharacter`
9. `CanMoveEndpointByUnitLine`
10. `CanMoveEndpointByUnitDocument`
11. `ExpansionAtExclusiveEnd`
12. `MovementAtExclusiveEnd`
13. `MoveToPreviousWord`
14. `ScrollIntoView`
15. `GetAttributeValue`
16. `FindAttribute`
17. `BlockRange`
18. `Movement`
19. `GeneratedMovementTests`

### Why these are Platform-only rather than Missing

`UiaTextRangeTests.cpp` exercises `Microsoft::Console::Interactivity::Win32::UiaTextRange` through the Windows UI Automation provider contract. The tests use COM/WRL provider objects, `ITextRangeProvider`, `TextPatternRangeEndpoint`, `TextUnit`, UIA attribute `VARIANT` values, native scrolling through `IRenderData`, and the C++ `TextBuffer`/provider bridge.

The migration policy keeps native WinRT/COM/Win32 boundaries explicit. Therefore these methods are not transferred Rust responsibilities and do not require Rust shadow implementations. Treating them as `Missing` would imply migration debt where none is intended; reimplementing them in Rust only to obtain parity witnesses would weaken the architecture.

`tools/rust/microsoft-rust-equivalence-r06b.json` freezes the complete source rule and the expected `Platform-only=19` distribution. The normal global source fingerprint and expected-coverage gates still fail if Microsoft adds, removes, or changes the source contract without deliberate reconciliation.

## Server is a source seam, not a separate TAEF suite

The 12-suite Microsoft runtime baseline contains no independent `server` test suite. Server behavior is exercised indirectly through other Microsoft suites, especially `host`, while R06 product work extracted deterministic portions of `src/server` into the safe `terminal-host` crate.

R06-B therefore does **not** fabricate a thirteenth suite or pretend that source files are TAEF test identities. Instead, `tools/rust/r06b-server-source-map.json` records the audited ownership seam and `Test-RustServerSourceMap.ps1` validates it in CI.

### Split server ownership — deterministic Rust + native boundary

| Microsoft source | Rust owner | Rust witnesses | Native remainder |
| --- | --- | ---: | --- |
| `src/server/ApiSorter.cpp` | `terminal-host::api_sorter` | 6 | routine dispatch, exception translation, NTSTATUS conversion, pending replies, device I/O |
| `src/server/ApiMessage.cpp` | `terminal-host::api_message_buffers` | 5 | buffer allocation and `DeviceComm` reads/writes |
| `src/server/ConsoleShimPolicy.cpp` | `terminal-host::console_shim_policy` | 5 | retrieving the module filename from a Windows process handle |

The 16 existing Rust witnesses are not renamed or copied into test-only abstractions. The new server seam gate verifies that each audited C++ source still exists, each Rust owner still exists, and every named Rust witness remains present.

### Intentionally native server boundaries

Three audited sources remain fully native in this increment:

- `src/server/ApiDispatchers.cpp` — console-global mutation, object handles, code pages, aliases/history, screen buffers and asynchronous I/O;
- `src/server/ApiDispatchersInternal.cpp` — internal native dispatch coupled to console objects and the Windows server message path;
- `src/server/ConDrvDeviceComm.cpp` — Windows ConDrv device/handle I/O.

The source map rejects a claimed Rust owner or Rust witness for these native entries. That makes the native boundary explicit instead of allowing an accidental documentation drift toward “everything must become Rust.”

## Machine enforcement

R06-B adds two machine-readable artifacts:

- `tools/rust/microsoft-rust-equivalence-r06b.json` — freezes the 19-method Interactivity Win32 classification;
- `tools/rust/r06b-server-source-map.json` — freezes the audited server ownership split.

`tools/rust/Test-RustServerSourceMap.ps1` enforces:

- exactly 3 split server entries;
- exactly 3 retained-native server entries;
- exactly 16 named Rust witnesses across the split entries;
- existence of every audited C++ source and Rust owner;
- presence of every witness function in its Rust owner;
- no Rust owner/witness claims on retained-native entries.

The Rust CI contract-harness job runs this gate after the existing TAEF/global census gates.

## Safety

R06-B changes parity metadata, documentation, and CI validation only.

- Product Rust changed: **0**
- Product C++ changed: **0**
- FFI changed: **0**
- Microsoft tests removed or weakened: **0**
- Native Win32/UIA ownership moved artificially into Rust: **0**
- Existing certification gate relaxed: **0**

## Result

The R06 phase now has an explicit ownership answer on both sides of the boundary:

- R06-A: complete 331-method `host` source reconciliation;
- R06-B: complete 19-method `interactivityWin32` reconciliation plus machine-checked server seams.

The next planned parity lane is R07 Renderer.
