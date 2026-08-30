//! Rust implementation track for Windows Terminal's VT parser.
//!
//! R01 ports Base64, the VT state machine, and the input/output dispatch layers
//! behind compatibility and differential tests before introducing the C ABI
//! boundary.

#![forbid(unsafe_code)]

#[allow(clippy::chunks_exact_to_as_chunks)]
pub mod base64;
pub mod input_c0;
pub mod input_engine;
pub mod input_layout;
#[path = "output_engine_compat.rs"]
pub mod output_engine;
#[path = "output_engine.rs"]
mod output_engine_core;
pub mod state_machine;
