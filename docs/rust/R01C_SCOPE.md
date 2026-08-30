# R01c — Output state machine engine

R01c ports the semantic dispatch layer behind the VT parser into safe Rust.

The Rust output engine consumes parser actions and emits typed terminal-dispatch actions. Product C++, conhost integration, WinRT/COM boundaries, and FFI remain untouched.

## Internal green rule

Pure-Rust increments are green when the Rust lane passes (`fmt`, `clippy`, `check`, `test`, and repository spelling). The Microsoft C++ contract is not part of the routine Rust development loop.

## Microsoft contract cadence

The Microsoft C++/TAEF contract is intentionally run only when explicitly dispatched or after synchronizing the fork with upstream Microsoft Terminal. This preserves contractual evidence without paying the C++ build cost for every Rust-only change.
