//! Deterministic escape-sequence formatting from `VtIo::Writer`.
//!
//! This module owns no output handles. It only reproduces the byte sequences
//! selected by the host writer before those bytes cross the Windows I/O boundary.

/// Formats CUP (Cursor Position) using the one-based coordinates emitted by
/// `VtIo::Writer::WriteCUP`.
#[must_use]
pub fn cup(x: i32, y: i32) -> Vec<u8> {
    format!("\x1b[{};{}H", y + 1, x + 1).into_bytes()
}

/// Formats a DEC private mode sequence ending in `h` when enabled and `l`
/// when disabled.
fn private_mode(parameter: &str, enabled: bool) -> Vec<u8> {
    let suffix = if enabled { 'h' } else { 'l' };
    format!("\x1b[?{parameter}{suffix}").into_bytes()
}

/// DECTCEM: text cursor visibility.
#[must_use]
pub fn dectcem(enabled: bool) -> Vec<u8> {
    private_mode("25", enabled)
}

/// SGR 1006 extended mouse mode, paired with any-event tracking mode 1003.
#[must_use]
pub fn sgr1006(enabled: bool) -> Vec<u8> {
    private_mode("1003;1006", enabled)
}

/// DECAWM: autowrap mode.
#[must_use]
pub fn decawm(enabled: bool) -> Vec<u8> {
    private_mode("7", enabled)
}

/// Alternate screen buffer mode 1049.
#[must_use]
pub fn alternate_screen_buffer(enabled: bool) -> Vec<u8> {
    private_mode("1049", enabled)
}

/// DSR CPR request emitted when cursor inheritance can be captured.
#[must_use]
pub const fn cursor_position_report_request() -> &'static [u8] {
    b"\x1b[6n"
}

/// XTWINOPS window visibility sequence. Visible maps to operation 1, hidden to 2.
#[must_use]
pub fn window_visibility(visible: bool) -> Vec<u8> {
    if visible {
        b"\x1b[1t".to_vec()
    } else {
        b"\x1b[2t".to_vec()
    }
}

/// Wraps an already-sanitized UTF-8 title using the OSC 0 framing used by
/// `VtIo::Writer::WriteWindowTitle`.
#[must_use]
pub fn window_title(sanitized_title: &[u8]) -> Vec<u8> {
    let mut output = Vec::with_capacity(sanitized_title.len() + 6);
    output.extend_from_slice(b"\x1b]0;");
    output.extend_from_slice(sanitized_title);
    output.extend_from_slice(b"\x1b\\");
    output
}

/// DECSC/DECRC cursor save/restore bytes used by the writer's corked flush path.
#[must_use]
pub const fn save_cursor() -> &'static [u8] {
    b"\x1b\x37"
}

#[must_use]
pub const fn restore_cursor() -> &'static [u8] {
    b"\x1b\x38"
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::vt_io_protocol::{encode_ucs2_utf8, sanitize_ucs2};

    fn sanitized_title_utf8(input: &[u16]) -> Vec<u8> {
        let mut output = Vec::new();
        for unit in input.iter().copied() {
            output.extend_from_slice(&encode_ucs2_utf8(sanitize_ucs2(unit)));
        }
        output
    }

    #[test]
    fn cup_is_one_based() {
        assert_eq!(cup(0, 0), b"\x1b[1;1H");
        assert_eq!(cup(79, 23), b"\x1b[24;80H");
    }

    #[test]
    fn boolean_private_modes_match_writer_bytes() {
        assert_eq!(dectcem(true), b"\x1b[?25h");
        assert_eq!(dectcem(false), b"\x1b[?25l");
        assert_eq!(sgr1006(true), b"\x1b[?1003;1006h");
        assert_eq!(sgr1006(false), b"\x1b[?1003;1006l");
        assert_eq!(decawm(true), b"\x1b[?7h");
        assert_eq!(decawm(false), b"\x1b[?7l");
        assert_eq!(alternate_screen_buffer(true), b"\x1b[?1049h");
        assert_eq!(alternate_screen_buffer(false), b"\x1b[?1049l");
    }

    #[test]
    fn report_visibility_and_cursor_sequences_match_vt_io() {
        assert_eq!(cursor_position_report_request(), b"\x1b[6n");
        assert_eq!(window_visibility(true), b"\x1b[1t");
        assert_eq!(window_visibility(false), b"\x1b[2t");
        assert_eq!(save_cursor(), b"\x1b\x37");
        assert_eq!(restore_cursor(), b"\x1b\x38");
    }

    #[test]
    fn title_framing_preserves_sanitized_payload() {
        assert_eq!(
            window_title(b"Windows Terminal"),
            b"\x1b]0;Windows Terminal\x1b\\"
        );
        assert_eq!(window_title(b""), b"\x1b]0;\x1b\\");
    }

    #[test]
    fn microsoft_vt_io_set_console_cursor_position_matches_exact_vectors() {
        let mut actual = Vec::new();
        for (x, y) in [(2, 3), (0, 0), (7, 3), (3, 2)] {
            actual.extend_from_slice(&cup(x, y));
        }
        assert_eq!(actual, b"\x1b[4;3H\x1b[1;1H\x1b[4;8H\x1b[3;4H");
    }

    #[test]
    fn microsoft_vt_io_set_console_cursor_info_matches_exact_vectors() {
        let mut actual = Vec::new();
        actual.extend_from_slice(&dectcem(false));
        actual.extend_from_slice(&dectcem(true));
        assert_eq!(actual, b"\x1b[?25l\x1b[?25h");
    }

    #[test]
    fn microsoft_vt_io_set_console_title_matches_exact_vectors() {
        let cases: [(&[u16], &[u8]); 3] = [
            (
                &[0x0066, 0x006f, 0x006f, 0x0062, 0x0061, 0x0072],
                b"\x1b]0;foobar\x1b\\",
            ),
            (
                &[
                    0x0066, 0x006f, 0x006f, 0x0001, 0x001f, 0x0062, 0x0061, 0x0072,
                ],
                b"\x1b]0;foo\xe2\x98\xba\xe2\x96\xbcbar\x1b\\",
            ),
            (
                &[
                    0x0066, 0x006f, 0x006f, 0x0001, 0x001f, 0x0062, 0x0061, 0x0072, 0x007f, 0x009f,
                ],
                b"\x1b]0;foo\xe2\x98\xba\xe2\x96\xbcbar\xe2\x8c\x82?\x1b\\",
            ),
        ];

        for (input, expected) in cases {
            assert_eq!(window_title(&sanitized_title_utf8(input)), expected);
        }
    }
}
