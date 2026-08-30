//! Host `ScreenBufferTests` cursor-presentation state.
//!
//! Microsoft exercises DEC private mode 12 (cursor blinking) together with
//! DECTCEM mode 25 (cursor visibility). Visibility already belongs to
//! `AdaptDispatchPresentationState`; this owner composes that state and adds the
//! one missing portable cursor-blink bit instead of duplicating presentation
//! ownership.

use terminal_parser::output_engine::{OutputAction, TermDispatch};

use crate::{adapt_dispatch::PageGeometry, presentation_state::AdaptDispatchPresentationState};

const ATT610_START_BLINKING_CURSOR_MODE: i32 = 12;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ScreenBufferCursorState {
    presentation: AdaptDispatchPresentationState,
    cursor_blinking: bool,
}

impl ScreenBufferCursorState {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        Self {
            presentation: AdaptDispatchPresentationState::new(geometry),
            cursor_blinking: true,
        }
    }

    #[must_use]
    pub const fn cursor_blinking(&self) -> bool {
        self.cursor_blinking
    }

    #[must_use]
    pub const fn cursor_visible(&self) -> bool {
        self.presentation.cursor_visible()
    }

    pub fn set_mode(&mut self, private: bool, mode: i32, enabled: bool) -> bool {
        if private && mode == ATT610_START_BLINKING_CURSOR_MODE {
            self.cursor_blinking = enabled;
            true
        } else {
            self.presentation.set_mode(private, mode, enabled)
        }
    }
}

impl TermDispatch for ScreenBufferCursorState {
    fn dispatch(&mut self, action: OutputAction) {
        match action {
            OutputAction::SetMode {
                private,
                enabled,
                mode,
            } => {
                if !self.set_mode(private, mode, enabled) {
                    self.presentation.dispatch(OutputAction::SetMode {
                        private,
                        enabled,
                        mode,
                    });
                }
            }
            other => self.presentation.dispatch(other),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn set_private_mode(state: &mut ScreenBufferCursorState, mode: i32, enabled: bool) {
        state.dispatch(OutputAction::SetMode {
            private: true,
            enabled,
            mode,
        });
    }

    #[test]
    fn microsoft_screen_buffer_cursor_is_on_contract() {
        let mut state = ScreenBufferCursorState::new(PageGeometry::new(0, 80, 25));

        assert!(state.cursor_blinking());
        assert!(state.cursor_visible());

        set_private_mode(&mut state, 12, false);
        assert!(!state.cursor_blinking());
        assert!(state.cursor_visible());
        set_private_mode(&mut state, 12, true);
        assert!(state.cursor_blinking());
        assert!(state.cursor_visible());
        set_private_mode(&mut state, 12, false);
        assert!(!state.cursor_blinking());
        assert!(state.cursor_visible());
        set_private_mode(&mut state, 12, true);
        assert!(state.cursor_blinking());
        assert!(state.cursor_visible());

        set_private_mode(&mut state, 25, false);
        assert!(state.cursor_blinking());
        assert!(!state.cursor_visible());
        set_private_mode(&mut state, 25, true);
        assert!(state.cursor_blinking());
        assert!(state.cursor_visible());

        // Microsoft's final ESC[?12;25l is normalized by the parser to one
        // SetMode action for each DEC private parameter.
        set_private_mode(&mut state, 12, false);
        set_private_mode(&mut state, 25, false);
        assert!(!state.cursor_blinking());
        assert!(!state.cursor_visible());
    }
}
