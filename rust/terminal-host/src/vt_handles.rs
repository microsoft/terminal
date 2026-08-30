//! Portable VT pipe-handle semantics for conhost startup arguments.
//!
//! Microsoft treats both null and `INVALID_HANDLE_VALUE` as invalid VT handles.
//! A VT session is considered active only when both the input and output handles
//! are valid. The handles are supplied by the host boundary and must survive
//! command-line parsing unchanged.

use crate::console_argument_parser::{ConsoleArgumentError, ConsoleArgumentState};
use crate::raw_console_arguments::parse_raw_console_arguments;

/// Opaque process-local handle value used by the portable owner.
pub type RawHandle = usize;

/// Portable representation of Win32 `INVALID_HANDLE_VALUE` (`(HANDLE)-1`).
pub const INVALID_HANDLE_VALUE: RawHandle = usize::MAX;

/// Returns whether a host handle is usable by the VT input/output path.
#[must_use]
pub const fn is_valid_handle(handle: RawHandle) -> bool {
    handle != 0 && handle != INVALID_HANDLE_VALUE
}

/// Input/output VT handles supplied by the host startup boundary.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct VtHandlePair {
    input: RawHandle,
    output: RawHandle,
}

impl VtHandlePair {
    #[must_use]
    pub const fn new(input: RawHandle, output: RawHandle) -> Self {
        Self { input, output }
    }

    #[must_use]
    pub const fn input(&self) -> RawHandle {
        self.input
    }

    #[must_use]
    pub const fn output(&self) -> RawHandle {
        self.output
    }

    pub fn set_input(&mut self, input: RawHandle) {
        self.input = input;
    }

    pub fn set_output(&mut self, output: RawHandle) {
        self.output = output;
    }

    /// Mirrors `ConsoleArguments::HasVtHandles`: both handles must be valid.
    #[must_use]
    pub const fn has_vt_handles(&self) -> bool {
        is_valid_handle(self.input) && is_valid_handle(self.output)
    }
}

/// Parsed conhost state together with externally supplied VT handles.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ParsedConsoleArgumentsWithVtHandles {
    pub arguments: ConsoleArgumentState,
    pub vt_handles: VtHandlePair,
}

/// Parses a raw conhost command line without allowing parsing to mutate the VT
/// handles supplied by the host boundary.
///
/// # Errors
/// Returns the same deterministic parse error as `parse_raw_console_arguments`.
pub fn parse_raw_console_arguments_with_vt_handles(
    command_line: &str,
    vt_input: RawHandle,
    vt_output: RawHandle,
) -> Result<ParsedConsoleArgumentsWithVtHandles, ConsoleArgumentError> {
    Ok(ParsedConsoleArgumentsWithVtHandles {
        arguments: parse_raw_console_arguments(command_line)?,
        vt_handles: VtHandlePair::new(vt_input, vt_output),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_console_arguments_is_using_vt_handle_contract() {
        let mut handles = VtHandlePair::new(INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE);
        assert!(!handles.has_vt_handles());

        handles.set_input(0x12);
        assert!(!handles.has_vt_handles());

        handles.set_output(0x16);
        assert!(handles.has_vt_handles());

        handles.set_input(0);
        assert!(!handles.has_vt_handles());

        handles.set_input(0x20);
        handles.set_output(0);
        assert!(!handles.has_vt_handles());
    }

    #[test]
    fn microsoft_console_arguments_combine_vt_pipe_handle_contract() {
        let parsed = parse_raw_console_arguments_with_vt_handles("conhost.exe", 0x10, 0x24)
            .expect("Microsoft VT handle vector parses");

        assert_eq!(parsed.arguments, ConsoleArgumentState::default());
        assert_eq!(parsed.vt_handles.input(), 0x10);
        assert_eq!(parsed.vt_handles.output(), 0x24);
        assert!(parsed.vt_handles.has_vt_handles());
    }

    #[test]
    fn microsoft_console_arguments_is_vt_handle_valid_contract() {
        assert!(!is_valid_handle(0));
        assert!(!is_valid_handle(INVALID_HANDLE_VALUE));
        assert!(is_valid_handle(0x4));
    }
}
