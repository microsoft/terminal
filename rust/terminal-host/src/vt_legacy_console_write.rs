//! Portable projection of the legacy processed console-write path onto VT bytes.
//!
//! `WriteCharsLegacy` mutates the text buffer first and mirrors the observable
//! result through `VtIo::Writer`. For single-cell ASCII input, the portable
//! decision is entirely determined by the buffer width, cursor column, control
//! characters, and whether the final write reaches the right edge. Native text
//! buffer mutation and Unicode glyph measurement remain separate owners.

use crate::vt_io_protocol::{encode_ucs2_utf8, sanitize_ucs2};

const CRLF: &[u8] = b"\r\n";

/// Cursor-column state required to reproduce the processed, wrapping legacy
/// console-write projection used by `WriteConsoleW`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct LegacyConsoleWriteState {
    width: usize,
    column: usize,
}

impl LegacyConsoleWriteState {
    /// Creates a writer at the first column of a non-empty console row.
    #[must_use]
    pub fn new(width: usize) -> Self {
        Self::with_column(width, 0)
    }

    /// Creates a writer at an explicit in-row cursor column.
    #[must_use]
    pub fn with_column(width: usize, column: usize) -> Self {
        assert!(width > 0, "legacy console rows must be non-empty");
        assert!(column < width, "cursor column must be inside the row");
        Self { width, column }
    }

    /// Current zero-based cursor column after all legacy write processing.
    #[must_use]
    pub const fn column(&self) -> usize {
        self.column
    }

    /// Projects processed single-cell ASCII input into the bytes mirrored by
    /// `VtIo::Writer` when wrapping and newline auto-return are enabled.
    ///
    /// Printable runs are emitted as runs. This is significant: the terminal
    /// itself performs intermediate autowraps, while conhost appends a CRLF only
    /// when the *last* cell in a run lands exactly on the right edge. Tabs are
    /// expanded only for cursor accounting; the mirrored payload retains `\t`.
    #[must_use]
    pub fn write_processed_ascii(&mut self, input: &[u8]) -> Vec<u8> {
        assert!(
            input.iter().all(u8::is_ascii),
            "legacy ASCII projection accepts ASCII input only"
        );

        let mut output = Vec::with_capacity(input.len() + 8);
        let mut index = 0usize;

        while index < input.len() {
            let value = input[index];

            if !is_legacy_control(value) {
                let start = index;
                index += 1;
                while index < input.len() && !is_legacy_control(input[index]) {
                    index += 1;
                }

                let chunk = &input[start..index];
                output.extend_from_slice(chunk);
                if self.advance_cells(chunk.len()) {
                    output.extend_from_slice(CRLF);
                }
                continue;
            }

            let mut wrapped = false;
            match value {
                0x00 => {
                    // NUL occupies one legacy cell and is mirrored as a space.
                    wrapped = self.advance_cells(1);
                    output.push(b' ');
                }
                0x07 => {
                    // BEL notifies the host and is also mirrored to the VT peer.
                    output.push(value);
                }
                0x08 => {
                    self.column = self.column.saturating_sub(1);
                    output.push(value);
                }
                0x09 => {
                    let remaining = self.width - self.column;
                    let to_next_stop = 8 - (self.column & 7);
                    let tab_cells = remaining.min(to_next_stop);
                    wrapped = self.advance_cells(tab_cells);
                    output.push(value);
                }
                0x0a => {
                    // With newline auto-return enabled, a bare LF is mirrored as
                    // CRLF. An LF already preceded by CR keeps the original LF.
                    self.column = 0;
                    if index == 0 || input[index - 1] != 0x0d {
                        output.extend_from_slice(CRLF);
                    } else {
                        output.push(value);
                    }
                }
                0x0d => {
                    self.column = 0;
                    output.push(value);
                }
                _ => {
                    // The remaining C0/DEL controls follow the legacy printable
                    // glyph projection already owned by vt_io_protocol.
                    let glyph = sanitize_ucs2(u16::from(value));
                    output.extend_from_slice(&encode_ucs2_utf8(glyph));
                    wrapped = self.advance_cells(1);
                }
            }

            if wrapped {
                output.extend_from_slice(CRLF);
            }
            index += 1;
        }

        output
    }

    /// Advances single-cell legacy text and reports whether the final cell of
    /// this run forced a wrap. Intermediate wraps are intentionally not
    /// reported because the VT peer performs those through DECAWM itself.
    fn advance_cells(&mut self, cells: usize) -> bool {
        if cells == 0 {
            return false;
        }

        let total = self
            .column
            .checked_add(cells)
            .expect("legacy console column accounting overflowed");
        self.column = total % self.width;
        self.column == 0 && total >= self.width
    }
}

const fn is_legacy_control(value: u8) -> bool {
    value < 0x20 || value == 0x7f
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_vt_io_write_console_w_matches_exact_vectors() {
        let mut state = LegacyConsoleWriteState::new(8);

        assert_eq!(state.write_processed_ascii(b""), b"");
        assert_eq!(state.write_processed_ascii(b"aaaaaaaa"), b"aaaaaaaa\r\n");
        assert_eq!(state.column(), 0);

        assert_eq!(state.write_processed_ascii(b"a\t\r\nb"), b"a\t\r\n\r\nb");
        assert_eq!(state.column(), 1);
    }

    #[test]
    fn printable_runs_only_reconcile_the_final_delayed_wrap() {
        let mut state = LegacyConsoleWriteState::new(8);
        assert_eq!(
            state.write_processed_ascii(b"aaaaaaaaaaaaaaaa"),
            b"aaaaaaaaaaaaaaaa\r\n"
        );
        assert_eq!(state.column(), 0);
    }

    #[test]
    fn separate_writes_preserve_cursor_column_between_calls() {
        let mut state = LegacyConsoleWriteState::new(8);
        assert_eq!(state.write_processed_ascii(b"abc"), b"abc");
        assert_eq!(state.column(), 3);
        assert_eq!(state.write_processed_ascii(b"defgh"), b"defgh\r\n");
        assert_eq!(state.column(), 0);
    }

    #[test]
    fn tab_uses_eight_column_stops_but_preserves_the_tab_byte() {
        let mut edge = LegacyConsoleWriteState::with_column(8, 1);
        assert_eq!(edge.write_processed_ascii(b"\t"), b"\t\r\n");
        assert_eq!(edge.column(), 0);

        let mut interior = LegacyConsoleWriteState::with_column(10, 1);
        assert_eq!(interior.write_processed_ascii(b"\t"), b"\t");
        assert_eq!(interior.column(), 8);
    }

    #[test]
    fn newline_auto_return_avoids_duplicate_carriage_return() {
        let mut state = LegacyConsoleWriteState::new(8);
        assert_eq!(state.write_processed_ascii(b"a\nb"), b"a\r\nb");
        assert_eq!(state.column(), 1);

        let mut state = LegacyConsoleWriteState::new(8);
        assert_eq!(state.write_processed_ascii(b"a\r\nb"), b"a\r\nb");
        assert_eq!(state.column(), 1);
    }
}
