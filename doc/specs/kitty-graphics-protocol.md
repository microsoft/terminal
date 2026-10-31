---
author: undy-aeon
created on: 2026-10-30
last updated: 2026-10-30
issue id: 8389
---

# Kitty Graphics Protocol Support

## Abstract

Adds support for the Kitty Graphics Protocol to Windows Terminal: image
transmission over APC sequences, direct placement at the cursor, Unicode
Placeholder ("virtual") placement, deletion, and the query actions clients
use to detect support. Images are composed into the per-row ImageSlices
the renderer already paints for Sixel, so both the AtlasEngine and GDI
backends render them without renderer-backend changes.

## Background

The Kitty Graphics Protocol is a de facto standard for displaying images in
terminals, supported by kitty, Ghostty, WezTerm, Konsole, iTerm2 and
others. Windows Terminal supports Sixel only, so tools that emit Kitty
sequences (icat, timg, chafa in kitty mode, several terminal-based coding
assistants) show nothing. Upstream tracking issue: #8389.

## Solution Design

### Wire protocol

Sequences are APC-framed: `ESC _ G <key=value,...> ; <base64> ESC \`. The
VT state machine gains an `ApcString` state mirroring the DCS
pass-through pattern: when the engine reports an interest in an APC
sequence, its content streams to a StringHandler one character at a time
and a synthetic ESC terminates it. APC content the engine does not claim
falls back to the previous ignore-until-ST behavior, so other APC users
(tmux passthrough and friends) are unaffected.

`KittyGraphicsParser` (src/terminal/adapter) parses the control keys per
the reference grammar in kitty's `parse-graphics-command.h` and executes
the actions against the buffer's image store.

Supported:

- Transmission of RGB (`f=24`), RGBA (`f=32`) and PNG (`f=100`) over the
  direct medium (`t=d`), including chunked transmission (`m=1`/`m=0`).
- Client-chosen ids (`i=`), terminal-assigned ids, and image numbers
  (`I=`) with the response echoing the assignment.
- Direct placement (`a=p`, `a=T`) with source-rectangle cropping
  (`x,y,w,h`), cell-rectangle scaling (`c,r` with aspect-ratio
  computation when only one is given), pixel offsets (`X,Y`), z-index
  acceptance (`z`, used for stacking order among placements), and all
  three cursor movement policies (`C=0/1/2`).
- Unicode Placeholder placement (`U=1`) with the full diacritic encoding:
  row/column/image-id-high-byte diacritics, 8-bit ids via indexed
  foreground colors, 24-bit ids via RGB colors, and the diacritic
  inheritance rules for cells that continue a run to their left.
- Deletion (`a=d`) with the `a`, `i`, `I`, `n`, `N`, `c`, `p`, `x`, `y`,
  `z`, `r` specifiers, including placement-id addressed deletes and data
  freeing for the capital variants.
- Query actions (`a=q`) including feature queries (`e=`), which is how
  well-behaved clients detect support, and quiet levels (`q=1/2`).

Not supported, answered with `ENOSYS` errors: file and shared-memory
transmission (`t=f/t/s/t=t`), compression (`o=z`), indexed color formats
(`f=8/f=16`), animation (`a=f/a=c` and `d=f`), and relative placements
(`P/Q/H/V`). The file-path media are refused deliberately: honoring an
arbitrary client-supplied path is an arbitrary-file-read primitive for
any program that can write to the terminal, including remote ones.

### Storage and composition

`KittyImages` (src/buffer/out), one instance per TextBuffer, holds the
decoded images (bounded at 256 MiB total, oldest-first eviction) and the
placement records. Placements are records in buffer coordinates rather
than pixels baked into rows at placement time, which is what makes
deletion, replacement and repositioning possible; the hook points are
`TextBuffer::IncrementCircularBuffer`, `TextBuffer::ScrollRows`, and the
reflow path, which move the records along with the buffer content.

At paint time `KittyImages::ComposeRow` composes everything visible on a
row into that row's ImageSlice: direct placements first, then any
Unicode Placeholder cells found in the row's text. The scaled pixel
strips are cached per (image, placement, grid row, cell size) and rows
are only recomposed when their inputs change, so steady-state rendering
cost is a cache-key comparison per row. Scaling uses a box filter, so
downscaled placements average rather than alias.

Unicode Placeholder text is ordinary buffer content and rides through
scroll, reflow, and resize for free; only the pixel resolution happens
at paint time. Placeholder sequences are stripped from
`TextBuffer::GetPlainText`, keeping private-use code points and combining
marks out of the clipboard and UIA.

### Capability signaling

- Query actions answer, so the standard detection dance
  (`a=q` + primary DA) identifies the terminal as supporting the
  protocol.
- The setting `experimental.enableKittyGraphicsProtocol` (default on in
  this fork) gates the feature. When off, APC sequences are ignored
  exactly as before. The classic console host (conhost) reports the
  capability as disabled unconditionally.

## Deviations from kitty

- z-index below text: placements always render above text; `z` orders
  placements among themselves. Negative z-index layering under text is
  not advertised in feature queries.
- Direct placements taller than the buffer are clipped at the buffer
  bottom instead of scrolling the screen.
- On reflow, direct placement positions are clamped rather than
  reflowed; Unicode Placeholder placements reflow naturally because they
  are text.
- Mixing Sixel images and Kitty placements on the same row is
  last-writer-wins; the two protocols sharing a row is undefined here.
- The underline-color placement id of Unicode Placeholders is accepted
  but the first virtual placement of the image resolves placeholders
  regardless of it.

## Security considerations

- Parsing fails closed: malformed control keys, oversized values,
  non-base64 payload bytes and dimension mismatches abort the sequence
  without touching the buffer.
- Payload accumulation is capped (64 MiB base64), image dimensions are
  capped (10000), and total decoded image memory is capped (256 MiB,
  oldest-first eviction).
- PNG decoding goes through WIC with dimension caps; decompression bombs
  resolve to a failed decode and an error response.

## Validation

Unit tests in `src/terminal/adapter/ut_adapter/KittyGraphicsTests.cpp`
cover the end-to-end path (VT bytes through parser to storage to
composed rows): all three formats, chunking, id assignment and echo,
mutually exclusive ids, size mismatches, unsupported media/formats,
placement geometry and cursor policies, queries and feature queries,
quiet levels, deletion semantics, re-transmission replacement, Unicode
Placeholder encoding and stripping, foreign-APC tolerance, and a
malformed-sequence battery. Buffer-level tests in
`src/buffer/out/ut_textbuffer/KittyPlacementTests.cpp` cover placement
tracking through scrolling, buffer rotation and reflow, the storage
move on resize, and the memory-budget eviction rules.

## Build note

The Windows Store platform does not set a linker TargetMachine for
static libraries. The WinRT-emitting static libraries in this repo hit
LNK1112 (x64 objects, x86 default) unless x64 is specified explicitly,
so `common.build.pre.props` pins MachineX64 alongside the existing
Win32 pin.
