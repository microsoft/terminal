# H07 — R06-B Server and Interactivity Win32 hardening

H07 revisits the R06-B boundary after the later migration stages. It does not attempt to make Windows UI Automation or ConDrv portable. Instead, it tightens the ownership line in two ways:

1. deterministic UIA text-range state that is not intrinsically COM/Win32 now has a real Rust owner and direct Microsoft-derived witnesses;
2. the six audited `src/server` source seams are content-pinned, so any upstream C++ change forces a new ownership audit.

## Interactivity Win32 census

The frozen `interactivityWin32` suite contains 19 `TEST_METHOD` identities and 52 runtime TAEF invocations. All 19 live in `src/interactivity/win32/ut_interactivity_win32/UiaTextRangeTests.cpp`.

Before H07 the complete file was classified `Platform-only`. That was safe but too coarse: `UiaTextRange` is the Win32 wrapper, while several assertions are implemented entirely by deterministic `UiaTextRangeBase` endpoint/block-range state.

H07 extracts exactly five complete source contracts:

| Microsoft source method | H07 | Rust witness |
| --- | --- | --- |
| `DegenerateRangesDetected` | Exact | `microsoft_interactivity_uia_degenerate_ranges_detected` |
| `CompareRange` | Exact | `microsoft_interactivity_uia_compare_range` |
| `CompareEndpoints` | Exact | `microsoft_interactivity_uia_compare_endpoints` |
| `MoveEndpointByRange` | Exact | `microsoft_interactivity_uia_move_endpoint_by_range` |
| `BlockRange` | Exact | `microsoft_interactivity_uia_block_range_clone_preserves_state` |

The owner is `rust/terminal-buffer/src/uia_text_range.rs`. It implements normalized start/end state, degeneracy, endpoint access/collapse, row-major endpoint comparison, endpoint copying, and block-range copy state. It contains no COM, WRL, HWND, VARIANT, BSTR, or native provider imitation.

The remaining 14 source methods stay `Platform-only` in H07 because their Microsoft observable still materially includes one or more retained boundaries: UIA COM/WRL calls, richer `TextUnit` movement coupled to native word/glyph/document-end behavior, provider/window scrolling, VARIANT/BSTR attribute surfaces, or generated UIA movement data.

Expected H07 suite distribution:

```text
interactivityWin32=19; runtime=52;
Exact=5, Platform-only=14
```

## Server seam hardening

R06-B already records six audited `src/server` implementation seams:

### Split ownership

- `src/server/ApiSorter.cpp` → `rust/terminal-host/src/api_sorter.rs`
- `src/server/ApiMessage.cpp` → `rust/terminal-host/src/api_message_buffers.rs`
- `src/server/ConsoleShimPolicy.cpp` → `rust/terminal-host/src/console_shim_policy.rs`

These retain 16 Rust witnesses in total. Function-pointer dispatch, NTSTATUS/device I/O, allocation, process HANDLE lookup, and other native effects remain C++.

### Native ownership

- `src/server/ApiDispatchers.cpp`
- `src/server/ApiDispatchersInternal.cpp`
- `src/server/ConDrvDeviceComm.cpp`

No Rust owner is claimed for those native seams.

### New H07 drift invariant

The previous seam gate checked that source files existed and that split Rust witnesses still existed. It could not detect a semantic change to an already-audited C++ file.

Schema v2 adds the exact Git blob SHA for every audited source. `Test-RustServerSourceMap.ps1` resolves `HEAD:<sourcePath>` and fails if the current blob differs. Therefore an upstream edit cannot inherit the old split/native decision silently; the map must be deliberately reviewed and repinned.

The expected Server gate remains:

```text
split=3
native=3
Rust witnesses=16
pinned source blobs=6
```

## Global hardening target

H07 moves five source identities from `Platform-only` to `Exact` and changes no other suite classification:

```text
Exact         164
Stronger       11
Partial       351
Platform-only  63
UI-managed     22
Missing       487
Total        1098
```

## Safety

- Microsoft C++ tests removed or weakened: **0**
- COM/WRL/Win32 shadow implementations created: **0**
- native Server seams falsely promoted to Rust: **0**
- certification gates relaxed: **0**
- FFI changed: **0**
- managed/XAML changed: **0**

H07 adds a platform-neutral Rust product owner only for behavior already implemented in the shared `UiaTextRangeBase` state machine, and makes the existing Server audit stricter rather than broader.
