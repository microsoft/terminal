# R03e: PageManager control plane

R03e ports the deterministic VT page-management rules from `PageManager` into safe Rust while leaving concrete `TextBuffer` storage and renderer side effects for R04.

## Compatibility surface

The Rust `PageManager` preserves these Windows Terminal rules:

- page numbers are clamped to the range 1 through 6;
- page 1 is the effective page when the active buffer is not the main buffer;
- active and visible page numbers are tracked independently;
- background page buffers are created lazily with the visible page dimensions;
- an existing background page is resized when the visible dimensions change while that page is inactive;
- making another page visible saves the current visible rows and loads the destination rows;
- current buffer properties and cursor coordinates transfer when the active page moves between distinct buffers;
- cursor row coordinates are adjusted between the visible viewport top and the zero-based background page;
- the visible cursor is hidden when the active cursor moves to a background page;
- a full redraw is requested after the page swap and property transfer, matching the C++ ordering;
- reset returns active and visible page numbers to 1 and releases the tracked background-page state.

`AdapterDispatch` routes PPA, PPR, PPB, NP, and PP through the Rust page manager. NP and PP additionally home the cursor. Cursor save and restore include the page number. DECPCCM is enabled by default at the composite Adapter boundary, and enabling it makes the active page visible immediately.

## TextBuffer boundary

R03e does not invent a second text-buffer implementation. Operations that require concrete storage or rendering are emitted as typed `PageEvent` values:

- create or resize a background buffer;
- save or load visible rows;
- copy active-buffer properties with viewport-top translation;
- hide the visible cursor;
- redraw the complete visible surface.

This completes the deterministic page-management control plane. R04 can consume those events with the Rust `TextBuffer` implementation without changing the page-number or transition semantics established here.

## Deferred to later surfaces

The following behavior depends on state intentionally outside the R03e control plane:

- concrete row copies and backing-buffer ownership;
- renderer invalidation side effects;
- `ImageSlice` and Sixel image placement in the text buffer;
- DECRQDE response generation, which needs the complete viewport and pan model;
- alternate-screen-buffer and hard-reset materialization around the eventual terminal surface;
- text insertion, erase, edit, scroll, tab, rectangular operations, grapheme width, and line rendition.

All R03e product code remains safe Rust and does not modify the C++ or FFI product surfaces.
