//! Pure `PtySignalInputThread` session semantics over an in-memory transcript.
//!
//! The C++ input thread dispatches each complete signal in wire order and uses
//! a scope-exit guard to send the VT close event whenever the loop terminates,
//! including clean EOF, truncated reads, and unexpected signal IDs. This module
//! models that sequencing without owning a pipe, a thread, or any Win32 state.

use crate::pty_signal::{PtySignal, PtySignalError, decode_payload};
use crate::pty_signal_state::{PtySignalAction, PtySignalState};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PtySignalSessionEnd {
    EndOfFile,
    TruncatedSignal {
        remaining: usize,
    },
    TruncatedPayload {
        signal: PtySignal,
        expected: usize,
        actual: usize,
    },
    UnexpectedSignal(u16),
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct PtySignalSessionResult {
    pub actions: Vec<PtySignalAction>,
    pub end: PtySignalSessionEnd,
    pub send_close_event: bool,
}

/// Processes a finite signal-pipe transcript using the C++ dispatch contract.
///
/// Deferred pre-connect actions are governed by the supplied state. The
/// resulting `send_close_event` flag is always true because `_Shutdown()` is
/// registered before the C++ read loop and therefore runs on every exit path.
#[must_use]
pub fn process_transcript(state: &mut PtySignalState, mut bytes: &[u8]) -> PtySignalSessionResult {
    let mut actions = Vec::new();

    loop {
        if bytes.is_empty() {
            return result(actions, PtySignalSessionEnd::EndOfFile);
        }
        if bytes.len() < 2 {
            return result(
                actions,
                PtySignalSessionEnd::TruncatedSignal {
                    remaining: bytes.len(),
                },
            );
        }

        let raw = u16::from_le_bytes([bytes[0], bytes[1]]);
        let signal = match PtySignal::decode([bytes[0], bytes[1]]) {
            Ok(signal) => signal,
            Err(PtySignalError::UnknownSignal(value)) => {
                return result(actions, PtySignalSessionEnd::UnexpectedSignal(value));
            }
            Err(PtySignalError::InvalidPayloadLength { .. }) => {
                return result(actions, PtySignalSessionEnd::UnexpectedSignal(raw));
            }
        };
        debug_assert_eq!(raw, signal as u16);
        bytes = &bytes[2..];

        let expected = signal.payload_len();
        if bytes.len() < expected {
            return result(
                actions,
                PtySignalSessionEnd::TruncatedPayload {
                    signal,
                    expected,
                    actual: bytes.len(),
                },
            );
        }

        let (payload, rest) = bytes.split_at(expected);
        let decoded = match decode_payload(signal, payload) {
            Ok(decoded) => decoded,
            Err(PtySignalError::InvalidPayloadLength { expected, actual }) => {
                return result(
                    actions,
                    PtySignalSessionEnd::TruncatedPayload {
                        signal,
                        expected,
                        actual,
                    },
                );
            }
            Err(PtySignalError::UnknownSignal(value)) => {
                return result(actions, PtySignalSessionEnd::UnexpectedSignal(value));
            }
        };
        if let Some(action) = state.apply(decoded) {
            actions.push(action);
        }
        bytes = rest;
    }
}

fn result(actions: Vec<PtySignalAction>, end: PtySignalSessionEnd) -> PtySignalSessionResult {
    PtySignalSessionResult {
        actions,
        end,
        send_close_event: true,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::pty_signal::{ResizeWindowData, SetParentData};

    #[test]
    fn connected_session_dispatches_in_wire_order_then_closes() {
        let mut state = PtySignalState::default();
        assert!(state.connect().is_empty());
        let bytes = [
            8, 0, 80, 0, 24, 0, // resize
            3, 0, 8, 7, 6, 5, 4, 3, 2, 1, // parent
        ];

        assert_eq!(
            process_transcript(&mut state, &bytes),
            PtySignalSessionResult {
                actions: vec![
                    PtySignalAction::ResizeWindow(ResizeWindowData {
                        columns: 80,
                        rows: 24,
                    }),
                    PtySignalAction::SetParent(SetParentData {
                        handle: 0x0102_0304_0506_0708,
                    }),
                ],
                end: PtySignalSessionEnd::EndOfFile,
                send_close_event: true,
            }
        );
    }

    #[test]
    fn preconnect_resize_is_deferred_but_parent_is_immediate() {
        let mut state = PtySignalState::default();
        let bytes = [
            8, 0, 100, 0, 30, 0, // resize
            3, 0, 1, 0, 0, 0, 0, 0, 0, 0, // parent
        ];

        let outcome = process_transcript(&mut state, &bytes);
        assert_eq!(
            outcome.actions,
            vec![PtySignalAction::SetParent(SetParentData { handle: 1 })]
        );
        assert_eq!(outcome.end, PtySignalSessionEnd::EndOfFile);
        assert!(outcome.send_close_event);
        assert_eq!(
            state.connect(),
            vec![PtySignalAction::ResizeWindow(ResizeWindowData {
                columns: 100,
                rows: 30,
            })]
        );
    }

    #[test]
    fn partial_read_keeps_prior_actions_and_still_closes() {
        let mut state = PtySignalState::default();
        assert!(state.connect().is_empty());
        let bytes = [8, 0, 80, 0, 24, 0, 1, 0, 1];

        let outcome = process_transcript(&mut state, &bytes);
        assert_eq!(
            outcome.actions,
            vec![PtySignalAction::ResizeWindow(ResizeWindowData {
                columns: 80,
                rows: 24,
            })]
        );
        assert_eq!(
            outcome.end,
            PtySignalSessionEnd::TruncatedPayload {
                signal: PtySignal::ShowHideWindow,
                expected: 2,
                actual: 1,
            }
        );
        assert!(outcome.send_close_event);
    }

    #[test]
    fn unexpected_signal_terminates_and_still_closes() {
        let mut state = PtySignalState::default();
        let outcome = process_transcript(&mut state, &[4, 0]);

        assert!(outcome.actions.is_empty());
        assert_eq!(outcome.end, PtySignalSessionEnd::UnexpectedSignal(4));
        assert!(outcome.send_close_event);
    }

    #[test]
    fn short_signal_id_terminates_and_still_closes() {
        let mut state = PtySignalState::default();
        let outcome = process_transcript(&mut state, &[8]);

        assert_eq!(
            outcome.end,
            PtySignalSessionEnd::TruncatedSignal { remaining: 1 }
        );
        assert!(outcome.send_close_event);
    }
}
