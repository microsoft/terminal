# R03d — DCS integration

R03d connects the Rust VT parser and output engine to the Adapter protocol cores
implemented in R03a through R03c. The resulting path is entirely Rust:

```text
StateMachine
    -> OutputStateMachineEngine
    -> AdapterDispatch
       -> AdaptDispatchCore
       -> Sixel Parser
       -> MacroBuffer
```

## DCS routing

`AdapterDispatch` composes `AdaptDispatchCore`, the persistent Sixel parser, and
`MacroBuffer` behind the parser's `TermDispatch` boundary. `begin_dcs` only
accepts a data string when the corresponding Rust handler has been initialized
successfully; `dcs_put` then routes every UTF-16 code unit to that active
handler, including the terminating ESC delivered by `StateMachine`.

The following DCS operations are active in R03d:

- SIXEL / `DefineSixelImage` (`DCS ... q`)
- DECDMAC / `DefineMacro` (`DCS ... ! z`)

Recognized DCS operations that still depend on later Adapter surfaces are not
silently consumed. Their semantic `DcsAction` is retained in
`AdaptDispatchCore::deferred_actions`, and their data string is rejected by the
Rust DCS boundary until its handler is implemented.

## Parameter compatibility

The integration preserves the C++ `VTParameter` conversion rules used by
`OutputStateMachineEngine::ActionDcsDispatch`:

- numeric parameters treat omitted and zero values as `1`;
- selective/enum parameters treat omitted values as `0`;
- DECDMAC macro id uses an explicit default of `0`;
- Sixel background selection maps `0` to default, `1` to transparent, and `2`
  to opaque;
- DECDMAC delete control maps `0` to delete-id and `1` to delete-all;
- DECDMAC encoding maps `0` to text and `1` to hexadecimal pairs.

Invalid DECDMAC enum values reject the DCS handler before any existing macro is
mutated.

## Persistent Sixel state

The Microsoft C++ Adapter creates one `SixelParser` lazily and reuses it for
later images. R03d mirrors that lifetime. `Parser::restart_image` resets
per-image raster state, color-number mappings, dimensions, and image storage
while preserving terminal-scoped palette state and the current display mode.

A dedicated end-to-end test changes a Sixel palette entry in one DCS image,
starts a second image, and verifies that the changed palette entry and its color
index remain observable.

## Validation boundary

R03d exercises real parser-to-Adapter flows for:

- Sixel image data and DCS termination;
- Sixel macro/background parameters and DECSDM display mode;
- palette persistence between Sixel DCS sessions;
- text-encoded DECDMAC definitions;
- hexadecimal DECDMAC definitions with omitted selective parameters;
- invalid macro-parameter rejection without mutation;
- explicit deferral of recognized but unimplemented DCS actions;
- ordinary CSI cursor dispatch before and after DCS traffic.

Product Rust remains covered by `#![forbid(unsafe_code)]`. No product C++ or FFI
surface is changed by this increment.

## Deferred after R03d

- DECDLD dynamic character-set download and renderer soft-font update.
- DECAUPSS user-preference supplemental character-set assignment.
- DECRSTS terminal-state restoration.
- DECRQSS setting requests and DCS responses.
- DECRSPS presentation-state restoration.
- DECINVM macro invocation back into `StateMachine` after CSI completion.
- `TextBuffer` / `ImageSlice` placement and renderer redraw.
- Sixel margin scrolling, viewport panning, partial flush timing, and
  palette-animation flush policy.
