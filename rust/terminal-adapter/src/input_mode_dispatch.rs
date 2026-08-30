//! Product-level coupling between adapter output actions and `TerminalInput` modes.
//!
//! Microsoft `AdaptDispatch` owns a small but important bridge from output VT
//! control sequences into the input encoder: cursor-key mode, keypad mode,
//! auto-repeat/backarrow modes, mouse tracking/encoding modes, focus events,
//! Win32 input mode, line-feed mode, and 7/8-bit C1 transmission. The actual
//! keyboard/mouse semantics already live in `terminal-input`; this owner keeps
//! the bridge explicit instead of duplicating that state inside Adapter.

use terminal_input::{Mode, TerminalInput};
use terminal_parser::output_engine::{OutputAction, TermDispatch};

#[derive(Default)]
pub struct TerminalInputDispatchState {
    input: TerminalInput,
}

impl TerminalInputDispatchState {
    #[must_use]
    pub const fn input(&self) -> &TerminalInput {
        &self.input
    }

    pub const fn input_mut(&mut self) -> &mut TerminalInput {
        &mut self.input
    }

    #[must_use]
    pub fn handles(action: &OutputAction) -> bool {
        match action {
            OutputAction::SetKeypadMode(_) | OutputAction::SendC1Controls(_) => true,
            OutputAction::SetMode { private, mode, .. } => {
                Self::input_mode_for(*private, *mode).is_some()
            }
            _ => false,
        }
    }

    #[must_use]
    pub const fn input_mode_for(private: bool, mode: i32) -> Option<Mode> {
        if !private {
            return match mode {
                20 => Some(Mode::LineFeed),
                _ => None,
            };
        }

        match mode {
            1 => Some(Mode::CursorKey),
            8 => Some(Mode::AutoRepeat),
            66 => Some(Mode::Keypad),
            67 => Some(Mode::BackarrowKey),
            1000 => Some(Mode::DefaultMouseTracking),
            1002 => Some(Mode::ButtonEventMouseTracking),
            1003 => Some(Mode::AnyEventMouseTracking),
            1004 => Some(Mode::FocusEvent),
            1005 => Some(Mode::Utf8MouseEncoding),
            1006 => Some(Mode::SgrMouseEncoding),
            1007 => Some(Mode::AlternateScroll),
            9001 => Some(Mode::Win32),
            _ => None,
        }
    }
}

impl TermDispatch for TerminalInputDispatchState {
    fn dispatch(&mut self, action: OutputAction) {
        match action {
            OutputAction::SetKeypadMode(enabled) => {
                self.input.set_input_mode(Mode::Keypad, enabled);
            }
            OutputAction::SendC1Controls(enabled) => {
                self.input.set_input_mode(Mode::SendC1, enabled);
            }
            OutputAction::SetMode {
                private,
                mode,
                enabled,
            } => {
                if let Some(input_mode) = Self::input_mode_for(private, mode) {
                    self.input.set_input_mode(input_mode, enabled);
                }
            }
            _ => {}
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_cursor_key_and_keypad_modes_update_terminal_input() {
        let mut state = TerminalInputDispatchState::default();

        state.dispatch(OutputAction::SetMode {
            private: true,
            mode: 1,
            enabled: true,
        });
        assert!(state.input().get_input_mode(Mode::CursorKey));
        state.dispatch(OutputAction::SetMode {
            private: true,
            mode: 1,
            enabled: false,
        });
        assert!(!state.input().get_input_mode(Mode::CursorKey));

        state.dispatch(OutputAction::SetKeypadMode(true));
        assert!(state.input().get_input_mode(Mode::Keypad));
        state.dispatch(OutputAction::SetKeypadMode(false));
        assert!(!state.input().get_input_mode(Mode::Keypad));
    }

    #[test]
    fn microsoft_mouse_mode_matrix_updates_terminal_input() {
        let cases = [
            (1000, Mode::DefaultMouseTracking),
            (1005, Mode::Utf8MouseEncoding),
            (1006, Mode::SgrMouseEncoding),
            (1002, Mode::ButtonEventMouseTracking),
            (1003, Mode::AnyEventMouseTracking),
            (1007, Mode::AlternateScroll),
        ];

        for (mode, input_mode) in cases {
            let mut state = TerminalInputDispatchState::default();
            state.dispatch(OutputAction::SetMode {
                private: true,
                mode,
                enabled: false,
            });
            assert!(!state.input().get_input_mode(input_mode));
            state.dispatch(OutputAction::SetMode {
                private: true,
                mode,
                enabled: true,
            });
            assert!(state.input().get_input_mode(input_mode));
        }
    }

    #[test]
    fn c1_linefeed_focus_and_win32_modes_have_real_terminal_input_owners() {
        let mut state = TerminalInputDispatchState::default();

        state.dispatch(OutputAction::SendC1Controls(true));
        assert!(state.input().get_input_mode(Mode::SendC1));
        state.dispatch(OutputAction::SetMode {
            private: false,
            mode: 20,
            enabled: true,
        });
        assert!(state.input().get_input_mode(Mode::LineFeed));
        state.dispatch(OutputAction::SetMode {
            private: true,
            mode: 1004,
            enabled: true,
        });
        assert!(state.input().get_input_mode(Mode::FocusEvent));
        state.dispatch(OutputAction::SetMode {
            private: true,
            mode: 9001,
            enabled: true,
        });
        assert!(state.input().get_input_mode(Mode::Win32));
    }
}
