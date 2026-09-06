---
author: leecher1337 @leecher1337
created on: 2026-09-06
last updated: 2026-09-06
issue id: 246
---

# Console Graphics Buffer

## Abstract

This resurrects `CONSOLE_GRAPHICS_BUFFER` - a pixel-addressable console
screen buffer that a client can draw directly into, with the console host
rendering it as part of the client's console window. It's a second kind of
screen buffer alongside the ordinary `CONSOLE_TEXTMODE_BUFFER`, selected via
`CreateConsoleScreenBuffer`'s `dwFlags` argument. This existed in older
Windows console hosts and was dropped from the modern implementation; this
document describes the resurrected mechanism.

## Inspiration

microsoft/terminal#246, "Bringing back the console graphics
screen-buffers?" Historically used by NTVDM to display DOS graphics-mode
output, but not specific to it - #246 itself notes other potential
consumers, such as Far Manager plugins wanting to show image thumbnails
in-console. This implementation intentionally does not gate buffer creation
on caller identity.

## Solution Design

### Buffer creation and pixel access

A client requests a graphics buffer the same way it would request a second
text buffer, via the public `CreateConsoleScreenBuffer` API, just with
`dwFlags = CONSOLE_GRAPHICS_BUFFER` (2) instead of `CONSOLE_TEXTMODE_BUFFER`
(1), and `lpScreenBufferData` pointing at a `CONSOLE_GRAPHICS_BUFFER_INFO`
describing the desired bitmap (a `BITMAPINFO` header/color-table plus a
`dwUsage` of `DIB_RGB_COLORS` or `DIB_PAL_COLORS`). Neither the flag value
nor the structure are declared in the public Windows SDK headers, but
`CreateConsoleScreenBuffer` itself still accepts the flag - see
`src/tools/graphicsbuffertest/main.cpp` for a from-scratch client that
declares the pieces it needs itself.

Server-side (`CreateGraphicsBuffer`/`ConsoleCreateScreenBuffer` in
`src/host/directio.cpp`), this builds a pagefile-backed shared section sized
from the validated bitmap dimensions, mapped both into the console host
(for rendering) and directly into the client's process, plus a mutex for
synchronizing access to it. The returned `CONSOLE_GRAPHICS_BUFFER_INFO`
carries back a pointer (`lpBitMap`) directly into that shared memory in the
client's own address space - once created, the client writes pixels with
ordinary memory stores, no further IPC needed for the pixels themselves.

### Telling the host to repaint: `InvalidateConsoleDIBits`

Writing to the shared bitmap memory doesn't by itself cause a repaint - the
client calls `InvalidateConsoleDIBits(hConsoleOutput, &rect)` (still
exported by kernel32.dll by name, declared in `dep/Console/winconp.h`, not
present in the public import library) to mark a rectangular region dirty.
This can be the whole buffer or a small sub-rectangle for a partial update;
`src/tools/graphicsbuffertest` exercises both.

### `SetConsolePalette` and why palette handles need publishing first

For a `DIB_PAL_COLORS` buffer, the client separately calls
`SetConsolePalette(hConsoleOutput, hPalette, dwUsage)` (also still exported
by name, also not in the public import library) with a GDI palette object
it created locally, telling the renderer how to resolve the buffer's 8-bit
indices to actual colors.

This has a sharp edge: GDI object handles (`HGDIOBJ`, `HPALETTE` included)
live in a per-process handle table maintained by win32k.sys, not in the NT
object manager's handle table - unlike the `HANDLE`s this driver's protocol
duplicates across processes elsewhere (e.g. `RegisterConsoleVdm`'s
`NtDuplicateObject` calls in `directio.cpp`), a raw `HPALETTE` value sent to
another process is not usable there. `ServerSetConsolePalette` in
`src/server/ApiDispatchers.cpp` stores the client's handle value as-is and
later hands it to ordinary GDI calls (`GetPaletteEntries` and similar) from
the renderer - which will silently fail on a handle that isn't valid in the
console host's own process. "Silently" is the sharp part: GDI doesn't
surface an error for this, so the visible symptom is just that nothing gets
drawn, not an obvious failure.

Historically, when the console subsystem still lived inside `CSRSS.EXE`
rather than a separate, unprivileged `conhost.exe`/`OpenConsole.exe`
process, this didn't bite the same way: the client side of
`SetConsolePalette` went through `NtUserConsoleControl(ConsolePublicPalette,
...)`, an internal win32k.sys console-control operation that marked the
client's palette object "public" within the window station's shared GDI
handle table - a privilege tied to CSRSS's unique role as the window
station's owning process, not something available to today's unprivileged,
out-of-process console host.

A client on this implementation instead needs to use the ordinary,
unprivileged mechanism for handing a GDI object to another process: the
clipboard. Publishing the palette with `OpenClipboard`/`EmptyClipboard`/
`SetClipboardData(CF_PALETTE, hPalette)`/`CloseClipboard` releases the
creating process's exclusive ownership, after which the same handle value
is valid in any process - including the console host's, once the client
sends that value over via `SetConsolePalette`. The object must not be
`DeleteObject`'d by the client afterward; it belongs to the clipboard/system
once published. `src/tools/graphicsbuffertest` does this end to end and is
the reference example for the sequencing.

### Rendering

- `GdiEngine::PaintConsoleBitmap` (`src/renderer/gdi/paint.cpp`): stretches
  the buffer to the client area, resolving `DIB_PAL_COLORS` indices to RGB
  in software rather than relying on GDI's own `SelectPalette`/
  `RealizePalette` + `DIB_PAL_COLORS` path, which proved unreliable for
  this.
- `AtlasEngine::PaintConsoleBitmap` (`src/renderer/atlas/`): reuses the
  existing per-row `ShapedRow::bitmap` mechanism already used for inline
  (Sixel-style) images. That mechanism assumes a given bitmap revision's
  content is immutable and cacheable forever, which doesn't hold for a live,
  constantly-changing video buffer; a new `Bitmap::alwaysRefresh` flag makes
  the backend reuse one fixed atlas slot per row and re-upload into it
  instead of requesting new atlas space every frame.
- `SCREEN_INFORMATION::GetScreenFontSize()` returns `{1,1}` for a graphics
  buffer, since its "window size in chars" is really its pixel dimensions
  in disguise - without this, the window/rendering math elsewhere in the
  codebase would multiply those dimensions by the real font's cell size,
  producing a window many times too large.

### NTVDM's own startup handshake: `ConsolepRegisterVDM`

Separately from graphics buffers themselves, `RegisterConsoleVdm` in
`directio.cpp` resurrects NTVDM's own startup handshake (`RegisterConsoleVDM`
client-side), including its shared VDM text-buffer fast path -
`InvalidateConsoleDIBits` against that specific buffer is fed through the
existing `WriteConsoleOutputW` machinery rather than a new render path. The
original caller-identity check (`NtVdmControl`) is not restorable - the
kernel-mode VDM subsystem was never ported to x64 Windows, so that API is
unconditionally `STATUS_NOT_IMPLEMENTED` there; one-VDM-per-console
exclusivity is still enforced independently via the existing registration
state.

## UI/UX Design

None directly - no new user-facing UI. The buffer is rendered as part of
the client's own console window, the same as ordinary text content.

## Capabilities

### Accessibility

No new impact - a graphics buffer participates in the same
`RegionChanged`/`Layout` accessibility notifications as any other buffer
content change.

### Security

Bounded, validated inputs: `CreateGraphicsBuffer` enforces ceilings on the
client-supplied `BITMAPINFO` length, computed image size, and dimensions
before committing any shared memory, so a hostile or buggy client can't
force an unreasonable allocation. No caller-identity gating is applied
(intentional - see Inspiration), so this is available to any client that
can open a handle to the console the same way any other screen-buffer
creation is.

### Reliability

No impact on ordinary text-mode consoles - this is purely additive, gated
behind the `CONSOLE_GRAPHICS_BUFFER` flag a client has to explicitly
request.

### Compatibility

Purely additive; behavior for `CONSOLE_TEXTMODE_BUFFER` clients is
unchanged.

### Performance, Power, and Efficiency

`AtlasEngine`'s fixed-atlas-slot reuse (see Rendering above) keeps a
constantly-updating graphics buffer from growing the shared glyph atlas
without bound, which would otherwise be a real concern for something
updated as often as DOS video output.

## Potential Issues

- **ConPTY can't carry pixel data.** A graphics buffer only works when this
  build is the *direct* host of the console window - e.g. replacing
  `conhost.exe`, or invoked the way `conhost.exe` normally is for a plain,
  non-PTY console session - not when reached through Windows Terminal or any
  other ConPTY-based consumer. Testing `src/tools/graphicsbuffertest` (or
  NTVDM) through a ConPTY-backed terminal will show nothing, regardless of
  whether this feature is working correctly. A separate, complementary
  change (see `doc/specs/Standalone Console Delegation.md`) adds one way to
  reach that "real window" mode through the normal handoff/delegation path;
  it isn't required to use or test this feature, just one route among
  others (directly replacing `conhost.exe` being the simplest) to get a
  modern build acting as a direct host.
- **Palette publication is an easy mistake.** See the `SetConsolePalette`
  section above - a client that skips publishing its palette via the
  clipboard first will see nothing drawn, with no error to point at why.

## Future considerations

- Any client needing genuine pixel-buffer console output - not just the
  NTVDM use case that historically drove this - can build on this
  mechanism, per #246's own examples.

## Resources

- microsoft/terminal#246 - "Bringing back the console graphics
  screen-buffers?"
- `src/host/directio.cpp` (`CreateGraphicsBuffer`, `ConsoleCreateScreenBuffer`,
  `RegisterConsoleVdm`)
- `src/server/ApiDispatchers.cpp` (`ServerSetConsolePalette` and related
  dispatchers)
- `src/renderer/gdi/paint.cpp`, `src/renderer/atlas/` (`PaintConsoleBitmap`)
- `src/tools/graphicsbuffertest` - standalone validation client and
  reference example for the palette-publishing sequence
- `doc/specs/Standalone Console Delegation.md` - companion change for
  reaching a real, directly-hosted window through the handoff path
