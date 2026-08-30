//! Deterministic framing for the private `ConPTY` signal stream.
//!
//! The C++ reader performs one exact-size read for the two-byte signal ID and
//! then one exact-size read for that signal's payload. A short read terminates
//! the input loop. This module models that contract over an in-memory byte
//! stream without owning a pipe or any Win32 resources.

use crate::pty_signal::{PtySignal, PtySignalData, PtySignalError, decode_payload};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PtySignalStreamError {
    TruncatedSignal {
        remaining: usize,
    },
    TruncatedPayload {
        signal: PtySignal,
        expected: usize,
        actual: usize,
    },
    Signal(PtySignalError),
}

impl From<PtySignalError> for PtySignalStreamError {
    fn from(value: PtySignalError) -> Self {
        Self::Signal(value)
    }
}

/// Decodes all complete messages from a `ConPTY` signal-stream transcript.
///
/// This intentionally rejects short reads instead of buffering them across
/// calls, matching `PtySignalInputThread::_GetData`, which requires every
/// `ReadFile` operation to return exactly the requested number of bytes.
///
/// # Errors
/// Returns an error for an unknown signal, a truncated two-byte signal ID, or
/// a payload shorter than the exact size required by that signal.
pub fn decode_stream(mut bytes: &[u8]) -> Result<Vec<PtySignalData>, PtySignalStreamError> {
    let mut messages = Vec::new();

    while !bytes.is_empty() {
        if bytes.len() < 2 {
            return Err(PtySignalStreamError::TruncatedSignal {
                remaining: bytes.len(),
            });
        }

        let signal = PtySignal::decode([bytes[0], bytes[1]])?;
        bytes = &bytes[2..];

        let expected = signal.payload_len();
        if bytes.len() < expected {
            return Err(PtySignalStreamError::TruncatedPayload {
                signal,
                expected,
                actual: bytes.len(),
            });
        }

        let (payload, rest) = bytes.split_at(expected);
        messages.push(decode_payload(signal, payload)?);
        bytes = rest;
    }

    Ok(messages)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::pty_signal::{ClearBufferData, ResizeWindowData, SetParentData, ShowHideData};

    #[test]
    fn decodes_multiple_messages_in_wire_order() {
        let bytes = [
            8, 0, 80, 0, 24, 0, // resize
            1, 0, 1, 0, // show
            2, 0, 1, 0, // clear and keep cursor row
            3, 0, 8, 7, 6, 5, 4, 3, 2, 1, // set parent
        ];

        assert_eq!(
            decode_stream(&bytes),
            Ok(vec![
                PtySignalData::ResizeWindow(ResizeWindowData {
                    columns: 80,
                    rows: 24,
                }),
                PtySignalData::ShowHideWindow(ShowHideData { show: 1 }),
                PtySignalData::ClearBuffer(ClearBufferData { keep_cursor_row: 1 }),
                PtySignalData::SetParent(SetParentData {
                    handle: 0x0102_0304_0506_0708,
                }),
            ])
        );
    }

    #[test]
    fn empty_stream_is_a_clean_end_of_file() {
        assert_eq!(decode_stream(&[]), Ok(Vec::new()));
    }

    #[test]
    fn short_signal_read_terminates_the_contract() {
        assert_eq!(
            decode_stream(&[8]),
            Err(PtySignalStreamError::TruncatedSignal { remaining: 1 })
        );
    }

    #[test]
    fn short_payload_read_reports_the_exact_expected_size() {
        assert_eq!(
            decode_stream(&[8, 0, 80, 0]),
            Err(PtySignalStreamError::TruncatedPayload {
                signal: PtySignal::ResizeWindow,
                expected: 4,
                actual: 2,
            })
        );
    }

    #[test]
    fn unknown_signal_stops_before_consuming_a_payload() {
        assert_eq!(
            decode_stream(&[4, 0, 1, 2, 3, 4]),
            Err(PtySignalStreamError::Signal(PtySignalError::UnknownSignal(
                4
            )))
        );
    }

    #[test]
    fn trailing_partial_message_is_not_silently_ignored() {
        let bytes = [1, 0, 1, 0, 2];
        assert_eq!(
            decode_stream(&bytes),
            Err(PtySignalStreamError::TruncatedSignal { remaining: 1 })
        );
    }
}
