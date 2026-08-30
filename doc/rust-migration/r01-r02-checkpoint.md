# R01-R02 migration checkpoint

This checkpoint closes the first two product migration stages on the Rust track before starting R03.

## Included

- R01a: Base64 compatibility behavior in safe Rust.
- R01b: VT state-machine core in safe Rust.
- R01c: output state-machine engine behind a typed Rust dispatch boundary.
- R01d: input state-machine engine behind a platform-neutral Rust dispatch boundary.
- R02a: deterministic TerminalInput keyboard state and classic VT encoding.
- R02b: layout-aware Unicode, AltGr and Kitty keyboard protocol semantics.
- R02c: mouse tracking and encoding, including default, UTF-8 and SGR modes.

## Safety and integration state

- Product Rust remains safe; migrated crates forbid unsafe code.
- No product C++ implementation has been replaced yet.
- No Rust ABI is exposed to C++ yet; the FFI compatibility boundary remains deferred.
- Routine development validation is the Rust CI lane. The Microsoft C++ contract runs only for upstream synchronization checkpoints or explicit manual dispatch.

## Checkpoint meaning

This is a semantic-core checkpoint, not the final C++ replacement milestone. R03 starts from this stable `rust/main` state and moves into Adapter / dispatch / Sixel integration.
