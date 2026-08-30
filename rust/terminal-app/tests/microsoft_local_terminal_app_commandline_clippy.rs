#![allow(clippy::float_cmp, clippy::unreadable_literal)]

// Keep the Microsoft command-line corpus byte-for-byte readable while scoping
// the two pedantic lint exceptions to this compatibility test target only.
include!("microsoft_local_terminal_app_commandline_contract.rs");
