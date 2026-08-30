# R06 — Host, server, interactivity, and ConPTY

R06 migrates host-side deterministic contracts before introducing operating-system handle ownership or thread boundaries.

## R06a — ConPTY signal wire contract

The first slice introduces the safe `terminal-host` crate and ports the private signal protocol consumed by `PtySignalInputThread`.

The Rust representation preserves the C++ signal discriminators exactly:

- `ShowHideWindow = 1`
- `ClearBuffer = 2`
- `SetParent = 3`
- `ResizeWindow = 8`

Payload decoding is explicit little-endian byte parsing rather than native-structure reinterpretation. Resize dimensions remain two 16-bit values, show/hide and keep-cursor-row fields retain their raw 16-bit wire values with nonzero boolean interpretation, and parent handles retain all 64 bits. Payload sizes are exact and unknown signal values are rejected.

## R06b — client command-line escaping

The second slice ports the deterministic `EscapeArgument` behavior from `ConsoleArguments.cpp` without taking ownership of `CommandLineToArgvW` or Win32 handles. Empty arguments are explicitly quoted, space/tab-containing arguments are wrapped in quotes, quote-adjacent backslashes are doubled, and trailing backslashes inside quoted arguments are escaped so Windows tokenization reconstructs the original value.

A small `join_client_arguments` helper mirrors the host path that rebuilds the client command line after host-only switches have been consumed. Tests cover empty/simple/Unicode arguments, spaces and tabs, embedded quotes, consecutive backslashes before quotes, trailing backslashes, and multi-argument reconstruction.

## R06c — tokenized ConsoleArguments parsing

The third slice ports the deterministic portion of `ConsoleArguments::ParseCommandline` while deliberately leaving `CommandLineToArgvW` on the Windows side. It consumes already-tokenized arguments and preserves server/signal handle forms, ForceV1/ForceNoHandoff/Embedding flags, width/height, `--feature pty`, headless/inherit-cursor, text measurement, ambiguous-width state, the historical `\\??\\` path token, explicit `--`, and the fallback where the first unrecognized argument begins the client command line.

Handle parsing mirrors the existing `wcstoul` behavior used by conhost, including nonzero enforcement, duplicate-handle rejection, prefix consumption and 32-bit saturation. Dimension parsing preserves the current C++ upper-bound behavior and full-token numeric validation.

## R06d — deterministic VtIo lifecycle

The fourth slice extracts the platform-neutral lifecycle decisions from `VtIo`. It preserves the `Uninitialized`, `Initialized`, `Starting`, `StartupFailed`, and `Running` states, the no-op path outside ConPTY mode, single initialization, non-reentrant start, startup failure when a close arrives while starting, close-event deduplication after startup, and the rule that shutdown reset sequences are emitted only while running.

Handle ownership, I/O threads, renderer construction, console locking, and actual close-event delivery remain on the C++/Win32 side. The Rust type models only the deterministic transition contract so later compatibility plumbing can delegate these decisions without duplicating state logic.

## R06e — deterministic VtIo protocol/configuration decisions

The fifth slice ports the remaining pure choices around `VtIo::Initialize`, `StartIfNeeded`, and `Shutdown`: text-measurement mapping, ambiguous-width override, cursor-inheritance negotiation ordering, the DA1/focus/win32-input startup byte sequence, shutdown resets, and C0/C1 control-character classification.

Actual writes, DA1 waiting, global settings mutation, handle ownership, threads, and console locking remain platform boundaries. The Rust code returns explicit data/bytes for those operations rather than performing them.

## R06f — legacy UCS-2 sanitization

The sixth slice ports `VtIo::SanitizeUCS2` as a safe UTF-16-code-unit transformation. It preserves the historical code page 437 display glyphs for C0 controls and DEL, maps C1 controls to `?`, maps isolated UTF-16 surrogate code units to U+FFFD, and leaves ordinary code units unchanged.

## R06g — legacy host attribute formatting

The seventh slice ports the deterministic `VtIo::FormatAttributes` formatting contract. It reuses the R04 `terminal-buffer::TextAttribute` representation rather than creating host-local color state, always emits SGR 0 to clear unknown VT-exclusive rendition state, then emits reverse video plus legacy ANSI foreground/background colors in the same order as the C++ host. Nonlegacy colors remain outside this legacy formatting path exactly as in the source contract.

## R06h — VtIo writer text transforms

The eighth slice ports three deterministic writer transforms without taking ownership of the output pipe: newline translation inserts CR only before an LF that does not already have one, raw control stripping maps C0/C1 code units through the same legacy printable substitutions while preserving cell count, and single-unit UCS-2 output encodes one to three UTF-8 bytes after replacing isolated surrogates with U+FFFD.

## R06i — deterministic VtIo writer sequences

The ninth slice ports the byte formatting for CUP, DECTCEM, SGR 1006 mouse mode, DECAWM, alternate-screen-buffer mode, DSR CPR, XTWINOPS visibility, OSC 0 title framing, and DECSC/DECRC cursor save/restore. These helpers only construct bytes; whether the bytes are submitted, corked, overlapped, or discarded remains the responsibility of the platform-owned writer boundary.

## R06j — deterministic CHAR_INFO serialization

The tenth slice ports `VtIo::Writer::WriteInfos` over a platform-neutral view of the consumed `CHAR_INFO` fields. It preserves CUP positioning, attribute transition emission, incomplete wide-glyph edge replacement, suppression of interior trailing halves, and the double replacement needed when a nominally wide cell contains a surrogate or control code unit. It reuses the migrated R04 attribute representation plus the R06 sanitization and UTF-8 helpers instead of duplicating those rules.

## R06k–R06y — remaining deterministic host/server/interactivity contracts

The remainder of R06 completes the pure policy surface needed before a future platform boundary is introduced: screen-snapshot serialization; pre-connect ConPTY signal deferral; clear-buffer planning; exact signal-stream framing and shutdown semantics; `_CONSOLE_API_MSG` buffer planning; console shim process classification; `ApiSorter` dispatch planning; host-signal framing; legacy full-width and keyboard-modifier planning; outbound remote-control packet serialization; OneCore API redirection policy; `ApiDetector` fallback selection; and `InteractivityFactory` implementation selection.

These slices preserve the existing C++ contracts while keeping Windows handles, `ReadFile`/`WriteFile`, process lookup, keyboard-layout APIs, library loading, object construction, locking, device communication, and actual console mutation on the existing platform side.

## Stage exit

R06 is complete when its final head passes workspace formatting, Clippy with warnings denied, Linux and Windows check/test, repository spelling/quality checks, and the TAEF harness self-test. Because R06 does not modify product C++ or an FFI boundary, Microsoft C++ contract tests are not additionally required for this stage.

## Safety boundary

`terminal-host` uses `#![forbid(unsafe_code)]`. R06 introduces no product unsafe Rust, no C++ changes, and no FFI changes. Platform ownership remains outside the migrated safe implementation crate.
