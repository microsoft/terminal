//! Pure state transitions around `PtySignalInputThread` connection timing.
//!
//! The C++ implementation defers resize and initial visibility until the
//! console client has connected, ignores clear-buffer before that point, and
//! applies parent changes immediately. Locking and Win32 side effects remain
//! platform-owned boundaries.

use crate::pty_signal::{PtySignalData, ResizeWindowData, SetParentData, ShowHideData};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PtySignalAction {
    ResizeWindow(ResizeWindowData),
    ShowHideWindow(ShowHideData),
    ClearBuffer { keep_cursor_row: bool },
    SetParent(SetParentData),
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct PtySignalState {
    connected: bool,
    early_resize: Option<ResizeWindowData>,
    initial_show_hide: Option<ShowHideData>,
}

impl PtySignalState {
    #[must_use]
    pub const fn is_connected(self) -> bool {
        self.connected
    }

    /// Applies one decoded signal and returns any side effect that is allowed
    /// immediately by the C++ connection-timing contract.
    #[must_use]
    pub fn apply(&mut self, signal: PtySignalData) -> Option<PtySignalAction> {
        match signal {
            PtySignalData::ResizeWindow(data) if !self.connected => {
                self.early_resize = Some(data);
                None
            }
            PtySignalData::ShowHideWindow(data) if !self.connected => {
                self.initial_show_hide = Some(data);
                None
            }
            PtySignalData::ClearBuffer(_) if !self.connected => None,
            PtySignalData::ResizeWindow(data) => Some(PtySignalAction::ResizeWindow(data)),
            PtySignalData::ShowHideWindow(data) => Some(PtySignalAction::ShowHideWindow(data)),
            PtySignalData::ClearBuffer(data) => Some(PtySignalAction::ClearBuffer {
                keep_cursor_row: data.keep_cursor_row(),
            }),
            PtySignalData::SetParent(data) => Some(PtySignalAction::SetParent(data)),
        }
    }

    /// Marks the console connected and returns deferred work in the same order
    /// as `PtySignalInputThread::ConnectConsole`: resize first, visibility next.
    #[must_use]
    pub fn connect(&mut self) -> Vec<PtySignalAction> {
        self.connected = true;
        let mut actions = Vec::with_capacity(2);

        if let Some(resize) = self.early_resize.take() {
            actions.push(PtySignalAction::ResizeWindow(resize));
        }
        if let Some(show_hide) = self.initial_show_hide.take() {
            actions.push(PtySignalAction::ShowHideWindow(show_hide));
        }

        actions
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::pty_signal::ClearBufferData;

    #[test]
    fn resize_and_visibility_are_deferred_until_connect() {
        let mut state = PtySignalState::default();
        let resize = ResizeWindowData {
            columns: 120,
            rows: 40,
        };
        let show = ShowHideData { show: 1 };

        assert_eq!(state.apply(PtySignalData::ResizeWindow(resize)), None);
        assert_eq!(state.apply(PtySignalData::ShowHideWindow(show)), None);
        assert!(!state.is_connected());

        assert_eq!(
            state.connect(),
            vec![
                PtySignalAction::ResizeWindow(resize),
                PtySignalAction::ShowHideWindow(show),
            ]
        );
        assert!(state.is_connected());
    }

    #[test]
    fn latest_preconnect_resize_and_visibility_win() {
        let mut state = PtySignalState::default();
        let first_resize = ResizeWindowData {
            columns: 80,
            rows: 24,
        };
        let last_resize = ResizeWindowData {
            columns: 100,
            rows: 30,
        };
        let first_show = ShowHideData { show: 0 };
        let last_show = ShowHideData { show: 7 };

        assert_eq!(state.apply(PtySignalData::ResizeWindow(first_resize)), None);
        assert_eq!(state.apply(PtySignalData::ResizeWindow(last_resize)), None);
        assert_eq!(state.apply(PtySignalData::ShowHideWindow(first_show)), None);
        assert_eq!(state.apply(PtySignalData::ShowHideWindow(last_show)), None);

        assert_eq!(
            state.connect(),
            vec![
                PtySignalAction::ResizeWindow(last_resize),
                PtySignalAction::ShowHideWindow(last_show),
            ]
        );
    }

    #[test]
    fn clear_is_ignored_before_connect_and_applied_afterward() {
        let mut state = PtySignalState::default();
        let clear = ClearBufferData { keep_cursor_row: 9 };

        assert_eq!(state.apply(PtySignalData::ClearBuffer(clear)), None);
        assert!(state.connect().is_empty());
        assert_eq!(
            state.apply(PtySignalData::ClearBuffer(clear)),
            Some(PtySignalAction::ClearBuffer {
                keep_cursor_row: true,
            })
        );
    }

    #[test]
    fn parent_change_is_immediate_even_before_connect() {
        let mut state = PtySignalState::default();
        let parent = SetParentData {
            handle: 0x1234_5678_9abc_def0,
        };

        assert_eq!(
            state.apply(PtySignalData::SetParent(parent)),
            Some(PtySignalAction::SetParent(parent))
        );
        assert!(!state.is_connected());
    }

    #[test]
    fn connected_signals_are_not_deferred() {
        let mut state = PtySignalState::default();
        assert!(state.connect().is_empty());

        let resize = ResizeWindowData {
            columns: 132,
            rows: 50,
        };
        let show = ShowHideData { show: 0 };

        assert_eq!(
            state.apply(PtySignalData::ResizeWindow(resize)),
            Some(PtySignalAction::ResizeWindow(resize))
        );
        assert_eq!(
            state.apply(PtySignalData::ShowHideWindow(show)),
            Some(PtySignalAction::ShowHideWindow(show))
        );
    }
}
