//! Portable `SetConsoleOutputMode` state transitions.
//!
//! The native host still owns parser reset and renderer invalidation side effects.
//! This module owns the mode validation, state mutation and the decisions that
//! tell those platform services what work is required. VT byte generation is
//! delegated to the existing writer-sequence owner.

use crate::vt_writer_sequences::decawm;

pub const ENABLE_PROCESSED_OUTPUT: u32 = 0x0001;
pub const ENABLE_WRAP_AT_EOL_OUTPUT: u32 = 0x0002;
pub const ENABLE_VIRTUAL_TERMINAL_PROCESSING: u32 = 0x0004;
pub const DISABLE_NEWLINE_AUTO_RETURN: u32 = 0x0008;
pub const ENABLE_LVB_GRID_WORLDWIDE: u32 = 0x0010;

const VALID_OUTPUT_MODE_MASK: u32 = ENABLE_PROCESSED_OUTPUT
    | ENABLE_WRAP_AT_EOL_OUTPUT
    | ENABLE_VIRTUAL_TERMINAL_PROCESSING
    | DISABLE_NEWLINE_AUTO_RETURN
    | ENABLE_LVB_GRID_WORLDWIDE;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct OutputModeEffects {
    pub reset_parser: bool,
    pub redraw: bool,
    pub vt_bytes: Vec<u8>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum OutputModeError {
    InvalidArgument,
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct ConsoleOutputModeState {
    mode: u32,
}

impl ConsoleOutputModeState {
    #[must_use]
    pub const fn from_mode(mode: u32) -> Self {
        Self { mode }
    }

    #[must_use]
    pub const fn mode(self) -> u32 {
        self.mode
    }

    /// Mirrors the portable portion of `ApiRoutines::SetConsoleOutputModeImpl`.
    ///
    /// Unknown flags are rejected before mutation. A VT-processing `on -> off`
    /// transition requests a parser reset, VT/LVB changes request a redraw, and
    /// changes to `ENABLE_WRAP_AT_EOL_OUTPUT` emit the corresponding DECAWM.
    pub fn set_mode(&mut self, requested: u32) -> Result<OutputModeEffects, OutputModeError> {
        if requested & !VALID_OUTPUT_MODE_MASK != 0 {
            return Err(OutputModeError::InvalidArgument);
        }

        let old = self.mode;
        let diff = old ^ requested;
        self.mode = requested;

        let reset_parser = old & ENABLE_VIRTUAL_TERMINAL_PROCESSING != 0
            && requested & ENABLE_VIRTUAL_TERMINAL_PROCESSING == 0;
        let redraw = diff & (ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_LVB_GRID_WORLDWIDE) != 0;
        let vt_bytes = if diff & ENABLE_WRAP_AT_EOL_OUTPUT != 0 {
            decawm(requested & ENABLE_WRAP_AT_EOL_OUTPUT != 0)
        } else {
            Vec::new()
        };

        Ok(OutputModeEffects {
            reset_parser,
            redraw,
            vt_bytes,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_vt_io_set_console_output_mode_matches_exact_vectors() {
        let mut state = ConsoleOutputModeState::default();
        let mut output = Vec::new();

        let modes = [
            ENABLE_PROCESSED_OUTPUT
                | ENABLE_WRAP_AT_EOL_OUTPUT
                | ENABLE_VIRTUAL_TERMINAL_PROCESSING
                | DISABLE_NEWLINE_AUTO_RETURN
                | ENABLE_LVB_GRID_WORLDWIDE,
            ENABLE_PROCESSED_OUTPUT | DISABLE_NEWLINE_AUTO_RETURN | ENABLE_LVB_GRID_WORLDWIDE,
            0,
            ENABLE_PROCESSED_OUTPUT
                | ENABLE_WRAP_AT_EOL_OUTPUT
                | DISABLE_NEWLINE_AUTO_RETURN
                | ENABLE_LVB_GRID_WORLDWIDE,
        ];

        for mode in modes {
            output.extend_from_slice(&state.set_mode(mode).unwrap().vt_bytes);
        }

        assert_eq!(output, b"\x1b[?7h\x1b[?7l\x1b[?7h");
        assert_eq!(state.mode(), modes[3]);
    }

    #[test]
    fn vt_disable_requests_parser_reset_and_rendering_changes_request_redraw() {
        let mut state = ConsoleOutputModeState::from_mode(
            ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_WRAP_AT_EOL_OUTPUT,
        );
        let effects = state.set_mode(ENABLE_WRAP_AT_EOL_OUTPUT).unwrap();
        assert!(effects.reset_parser);
        assert!(effects.redraw);
        assert!(effects.vt_bytes.is_empty());

        let effects = state
            .set_mode(ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_LVB_GRID_WORLDWIDE)
            .unwrap();
        assert!(!effects.reset_parser);
        assert!(effects.redraw);
        assert!(effects.vt_bytes.is_empty());
    }

    #[test]
    fn invalid_flags_fail_before_mutating_state() {
        let initial = ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;
        let mut state = ConsoleOutputModeState::from_mode(initial);

        assert_eq!(
            state.set_mode(initial | 0x8000_0000),
            Err(OutputModeError::InvalidArgument)
        );
        assert_eq!(state.mode(), initial);
    }
}
