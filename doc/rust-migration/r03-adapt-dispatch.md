# R03c — Adapter dispatch core

R03c ports the deterministic geometry and mode-routing portion of `AdaptDispatch`
into the safe, platform-neutral `terminal-adapter` crate.

## In scope

- Page-relative and buffer-relative cursor geometry.
- CUU/CUD/CUF/CUB/CNL/CPL cursor movement.
- CUP/HVP, HPA/CHA, VPA, HPR, and VPR positioning semantics.
- DEC top/bottom scrolling margins.
- DECLRMM-gated left/right scrolling margins.
- DECOM origin mode and home-position behavior.
- Adapter-local insert/replace, autowrap, sixel-display, erase-color, and page-cursor-coupling mode state.
- Cursor save/restore for position, origin mode, and delayed-EOL-wrap state.
- Direct implementation of the Rust `TermDispatch` boundary produced by R01c.
- Explicit deferral of actions that still need `TextBuffer`, renderer, terminal input, host APIs, or later Adapter slices.

## Safety boundary

`AdaptDispatchCore` contains no Win32 types, raw pointers, FFI, renderer references, or C++ ownership. Product Rust remains `#![forbid(unsafe_code)]`.

The C++ `_CursorMovePosition` algorithm is preserved deliberately: absolute positions are one-based VT coordinates, vertical coordinates are page-relative when origin mode is off, origin mode rebases both dimensions to the active margins, CUU/CUD/CUF/CUB-style movement is margin constrained, while HPR/VPR are not margin constrained unless DECOM is active.

## Deferred

- Actual text insertion/replacement into `TextBuffer`.
- Grapheme-width / line-rendition clamping.
- Erase, insert/delete, rectangular editing, scrolling, and tabs.
- Full SGR/TextAttribute save/restore.
- Host/system modes delegated to `TerminalInput`, renderer, or `ITerminalApi`.
- DCS wiring for Sixel/MacroBuffer.
- C++ façade / FFI integration.

Deferred actions are retained in `deferred_actions`; they are never silently dropped.
