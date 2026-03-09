# AGENTS.md — Windows Terminal Asciicast Feature

## What This Is
A feature branch on a fork of [microsoft/terminal](https://github.com/microsoft/terminal) adding **asciicast v2 recording and playback** (issue #469).

## Build
```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Invoke-OpenConsoleBuild
```
Requires: VS 2022 (C++ desktop + UWP workloads), Windows 11 SDK (10.0.22621.0+).

## Architecture

### Connection Model
All terminal backends implement `ITerminalConnection` (defined in `src/cascadia/TerminalConnection/ITerminalConnection.idl`):
- `Start()`, `WriteInput()`, `Resize()`, `Close()`
- `TerminalOutput` event — fires raw VT data as `char16_t[]`
- `StateChanged` event — fires on connection state transitions

Existing connections: `ConptyConnection` (local shell), `AzureConnection` (cloud shell), `EchoConnection` (test).

### Our Additions (all in `src/cascadia/TerminalConnection/`)
- **`AsciicastRecorder`** — Wraps any `ITerminalConnection`. Subscribes to `TerminalOutput`, timestamps chunks, writes `.cast` file. Passes all calls through to inner connection unchanged.
- **`AsciicastConnection`** — Implements `ITerminalConnection` for playback. Reads `.cast` file, fires `TerminalOutput` events with original timing via coroutine.

### Asciicast v2 Format
```json
{"version": 2, "width": 120, "height": 30, "timestamp": 1709999999}
[0.000000, "o", "output data here"]
```
NDJSON: first line is header, subsequent lines are `[elapsed_seconds, event_type, data]`.

### Code Conventions
- C++/WinRT (not raw COM) — follow existing patterns in the codebase
- Use `BaseTerminalConnection<T>` CRTP base for state management
- IDL files define WinRT interfaces, `*.g.h` headers are auto-generated
- Run `Invoke-CodeFormat` before committing (clang-format enforced)
- TAEF for tests

### Key Files
- `ITerminalConnection.idl` — The interface definition
- `BaseTerminalConnection.h` — CRTP base with state machine helpers
- `EchoConnection.{h,cpp,idl}` — Simplest example connection (~45 lines)
- `ConptyConnection.{h,cpp}` — The real connection; `_OutputThread()` fires `TerminalOutput.raise()`
- `TerminalConnection.vcxproj` / `.filters` — Must add new files here

### Recording Tap Point
`ConptyConnection::_OutputThread()` reads from ConPTY pipe, converts UTF-8→UTF-16, and fires:
```cpp
TerminalOutput.raise(winrt_wstring_to_array_view(wstr));
```
Our recorder subscribes to this event — no VT parsing needed, just raw passthrough with timestamps.
