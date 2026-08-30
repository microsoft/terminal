//! Narrow compatibility boundary between the existing C++ code and Rust.
//!
//! R08 turns the previously empty ABI placeholder into the common FFI
//! foundation used by product-integration facades. Product semantics stay in
//! safe Rust crates; raw-pointer handling belongs only in explicit FFI modules.

#![deny(unsafe_op_in_unsafe_fn)]

use std::panic::{AssertUnwindSafe, catch_unwind};

use terminal_parser::base64::DecodeError;

/// Stable status values returned across the C ABI.
#[repr(i32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FfiStatus {
    /// Operation completed successfully.
    Ok = 0,
    /// The caller supplied an invalid ABI argument.
    InvalidArgument = 1,
    /// The parser rejected the supplied Base64 input.
    InvalidBase64 = 2,
    /// Base64 decoded successfully but the payload was not UTF-8.
    InvalidUtf8 = 3,
    /// Rust panicked while servicing the call.
    Panic = 255,
}

impl From<DecodeError> for FfiStatus {
    fn from(error: DecodeError) -> Self {
        match error {
            DecodeError::InvalidBase64 => Self::InvalidBase64,
            DecodeError::InvalidUtf8 => Self::InvalidUtf8,
        }
    }
}

/// Current ABI contract version.
///
/// Increment this only for an intentional breaking change to the exported C
/// surface. Additive functions do not require a bump.
pub const ABI_VERSION: u32 = 1;

/// Returns the ABI contract version without allocating or crossing ownership
/// boundaries.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_abi_version() -> u32 {
    ABI_VERSION
}

/// Executes an FFI operation while preventing Rust panics from unwinding into
/// C++.
fn ffi_guard(operation: impl FnOnce() -> FfiStatus) -> FfiStatus {
    catch_unwind(AssertUnwindSafe(operation)).unwrap_or(FfiStatus::Panic)
}

/// Exercises the status-returning ABI path without pointer or ownership
/// semantics. This gives C/C++ consumers a stable handshake that also proves
/// the panic-containment path is part of the production boundary.
#[unsafe(no_mangle)]
pub extern "C" fn terminal_parser_ffi_status_probe() -> FfiStatus {
    ffi_guard(|| FfiStatus::Ok)
}

#[cfg(test)]
mod tests {
    use super::{
        ABI_VERSION, FfiStatus, ffi_guard, terminal_parser_ffi_abi_version,
        terminal_parser_ffi_status_probe,
    };
    use terminal_parser::base64::DecodeError;

    #[test]
    fn abi_version_and_status_probe_are_stable_and_exported() {
        assert_eq!(terminal_parser_ffi_abi_version(), ABI_VERSION);
        assert_eq!(terminal_parser_ffi_status_probe(), FfiStatus::Ok);
        assert_eq!(ABI_VERSION, 1);
    }

    #[test]
    fn decode_errors_have_stable_status_mapping() {
        assert_eq!(
            FfiStatus::from(DecodeError::InvalidBase64),
            FfiStatus::InvalidBase64
        );
        assert_eq!(
            FfiStatus::from(DecodeError::InvalidUtf8),
            FfiStatus::InvalidUtf8
        );
    }

    #[test]
    fn ffi_guard_returns_status_without_translation() {
        assert_eq!(ffi_guard(|| FfiStatus::Ok), FfiStatus::Ok);
        assert_eq!(
            ffi_guard(|| FfiStatus::InvalidArgument),
            FfiStatus::InvalidArgument
        );
    }

    #[test]
    fn ffi_guard_contains_panics() {
        assert_eq!(
            ffi_guard(|| panic!("panic must not cross the C ABI")),
            FfiStatus::Panic
        );
    }
}
