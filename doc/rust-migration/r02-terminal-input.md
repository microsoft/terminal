# R02 — TerminalInput

R02 ports `src/terminal/input` into safe Rust without changing product C++ or introducing an FFI boundary.

## R02a — deterministic state and keyboard protocol

R02a creates the `terminal-input` crate and ports the parts of `TerminalInput` that can be evaluated identically on Linux and Windows without consulting the active Windows keyboard layout:

- input-mode state and reset behavior;
- mutual exclusion of mouse tracking and mouse encoding modes;
- main/alternate screen Kitty protocol stack state;
- focus reports;
- Win32 input mode serialization;
- classic Backspace, Tab, Return, Pause and Ctrl mappings;
- cursor/navigation keys;
- F1–F20;
- Insert/Delete/PageUp/PageDown;
- ANSI versus VT52 prefixes;
- 7-bit versus C1 CSI/SS3 prefixes;
- application cursor/keypad modes;
- classic auto-repeat suppression.

The Rust tests are derived from the observable contracts in Microsoft's `src/terminal/adapter/ut_adapter/inputTest.cpp`, especially `TerminalInputTests`, `TestFocusEvents`, `CtrlNumTest`, `BackarrowKeyModeTest`, `AutoRepeatModeTest`, and `SendC1ControlTest`.

## R02b — layout-sensitive keyboard translation

R02b will introduce a narrow keyboard-layout abstraction for behavior currently implemented with `GetKeyboardLayout`, `MapVirtualKeyExW`, `ToUnicodeEx`, AltGr timing/state tracking, surrogate handling, and fallback Unicode translation. The deterministic core must remain platform-neutral; Windows-specific translation belongs behind the abstraction.

R02b also completes Kitty keyboard protocol encoding on top of that abstraction.

## R02c — mouse input

R02c ports `mouseInput.cpp` and the mouse input state machine, including default, UTF-8 and SGR encodings, drag/hover tracking, wheel accumulation, alternate scroll and button state.

## Exit condition

R02 exits when the Rust implementation covers the full observable `TerminalInput` contract with Rust-native tests and differential vectors, while `#![forbid(unsafe_code)]` remains true for implementation crates.
