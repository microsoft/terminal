# R03b — MacroBuffer core

R03b ports the deterministic DECDMAC macro storage and definition parser into the
safe, platform-neutral `terminal-adapter` crate.

## In scope

- Fixed 64-macro namespace.
- `MAX_SPACE = 0x40000` total storage ceiling.
- UTF-16 code-unit storage matching Windows `std::wstring` behavior.
- Text and hexadecimal-pair macro definitions.
- Delete-one and delete-all replacement semantics.
- Repeat syntax (`!count;...;`) including default/zero repeat behavior.
- Ignoring literal C0 controls while retaining controls encoded as hex pairs.
- Pending repeat application when ESC terminates a definition.
- VT parameter saturation using the shared parser `MAX_PARAMETER_VALUE`.
- VT420-compatible wrapping checksum.
- Invocation depth ceiling of 16 and cumulative invocation length ceiling.
- Hard-reset behavior during active invocation: overwrite definitions with NUL
  code units without releasing the active backing sequences.

## Invocation boundary

The C++ class keeps `_invokedDepth` and `_invokedSequenceLength` as mutable
members because `InvokeMacro` calls directly back into `StateMachine`. R03b does
not couple `terminal-adapter` back to the parser runtime. Instead,
`prepare_invoke` returns the exact UTF-16 sequence plus an immutable
`InvocationContext`. Nested invocations consume that returned context.

This preserves the observable depth and sequence-length limits while avoiding a
mutable reentrant borrow or hidden RAII state in the Rust core. Actual dispatch
back into the VT state machine remains an Adapter integration concern for a
later R03 slice.

## Safety

The module is covered by the crate-level `#![forbid(unsafe_code)]`. Storage,
repeat expansion, checksum arithmetic, and invocation accounting use checked or
wrapping operations deliberately chosen to match the C++ contract without raw
pointer access.
