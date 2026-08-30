//! Portable clipboard text extraction over the safe Rust `TextBuffer` owner.
//!
//! Microsoft's host clipboard tests distinguish rectangular (block) selection
//! from line selection. Block selection preserves selected trailing spaces and
//! inserts CRLF between every physical row. Line selection follows forced-wrap
//! chains, trims only non-wrapped physical rows, and emits CRLF only where a
//! logical line actually ends.

use crate::text_buffer::{TextBuffer, TextBufferPoint};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ClipboardSelectionMode {
    Block,
    Line,
}

/// A half-open clipboard span. `end.x` is excluded while `end.y` is included,
/// matching the point-span configuration used by Microsoft's `CopyRequest`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ClipboardCopyRequest {
    pub start: TextBufferPoint,
    pub end: TextBufferPoint,
    pub mode: ClipboardSelectionMode,
}

impl ClipboardCopyRequest {
    #[must_use]
    pub const fn new(
        start: TextBufferPoint,
        end: TextBufferPoint,
        mode: ClipboardSelectionMode,
    ) -> Self {
        Self { start, end, mode }
    }
}

/// Extracts UTF-16 clipboard text with Microsoft's block/line selection rules.
#[must_use]
pub fn get_plain_text(buffer: &TextBuffer, request: ClipboardCopyRequest) -> Vec<u16> {
    let start_y = request.start.y.min(buffer.height() - 1);
    let end_y = request.end.y.min(buffer.height() - 1);
    let (top, bottom) = if start_y <= end_y {
        (start_y, end_y)
    } else {
        (end_y, start_y)
    };

    match request.mode {
        ClipboardSelectionMode::Block => block_text(buffer, request, top, bottom),
        ClipboardSelectionMode::Line => line_text(buffer, request, top, bottom),
    }
}

fn block_text(
    buffer: &TextBuffer,
    request: ClipboardCopyRequest,
    top: u16,
    bottom: u16,
) -> Vec<u16> {
    let left = request.start.x.min(request.end.x).min(buffer.width());
    let right = request.start.x.max(request.end.x).min(buffer.width());
    let mut output = Vec::new();

    for y in top..=bottom {
        if y != top {
            append_crlf(&mut output);
        }
        let row = buffer.row(i32::from(y));
        output.extend_from_slice(row.text_range(i32::from(left), i32::from(right)));
    }

    output
}

fn line_text(
    buffer: &TextBuffer,
    request: ClipboardCopyRequest,
    top: u16,
    bottom: u16,
) -> Vec<u16> {
    let mut output = Vec::new();

    for y in top..=bottom {
        let row = buffer.row(i32::from(y));
        let begin = if y == top {
            request.start.x.min(buffer.width())
        } else {
            0
        };
        let end = if y == bottom {
            request.end.x.min(buffer.width())
        } else {
            buffer.width()
        };

        let mut row_text = row.text_range(i32::from(begin), i32::from(end)).to_vec();
        if !row.was_wrap_forced() {
            trim_trailing_spaces(&mut row_text);
        }
        output.extend_from_slice(&row_text);

        if y != bottom && !row.was_wrap_forced() {
            append_crlf(&mut output);
        }
    }

    output
}

fn trim_trailing_spaces(text: &mut Vec<u16>) {
    while text.last() == Some(&u16::from(b' ')) {
        text.pop();
    }
}

fn append_crlf(output: &mut Vec<u16>) {
    output.extend_from_slice(&[u16::from(b'\r'), u16::from(b'\n')]);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::text_attribute::TextAttribute;

    fn write_common_row(buffer: &mut TextBuffer, y: i32) {
        let row = buffer.row_mut(y);
        for (x, width, glyph) in [
            (0, 1, vec![u16::from(b'A')]),
            (1, 1, vec![u16::from(b'B')]),
            (2, 2, vec![0x304b]),
            (4, 1, vec![u16::from(b'C')]),
            (5, 2, vec![0x304d]),
            (7, 1, vec![u16::from(b'D')]),
            (8, 1, vec![u16::from(b'E')]),
        ] {
            row.replace_glyph(x, width, &glyph)
                .expect("fixture glyph fits");
        }
    }

    fn fixture() -> TextBuffer {
        let mut buffer = TextBuffer::new(80, 4, TextAttribute::default()).unwrap();
        for y in 0..4 {
            write_common_row(&mut buffer, y);
        }
        buffer.row_mut(1).set_wrap_forced(true);
        buffer.row_mut(3).set_wrap_forced(true);
        buffer
    }

    #[test]
    fn block_selection_preserves_spaces_and_physical_line_breaks() {
        let buffer = fixture();
        let text = get_plain_text(
            &buffer,
            ClipboardCopyRequest::new(
                TextBufferPoint::new(0, 0),
                TextBufferPoint::new(15, 3),
                ClipboardSelectionMode::Block,
            ),
        );
        let row: Vec<u16> = "ABかCきDE      ".encode_utf16().collect();
        let mut expected = Vec::new();
        for index in 0..4 {
            if index != 0 {
                append_crlf(&mut expected);
            }
            expected.extend_from_slice(&row);
        }
        assert_eq!(text, expected);
    }

    #[test]
    fn line_selection_follows_wraps_and_trims_only_logical_line_ends() {
        let buffer = fixture();
        let text = get_plain_text(
            &buffer,
            ClipboardCopyRequest::new(
                TextBufferPoint::new(0, 0),
                TextBufferPoint::new(15, 3),
                ClipboardSelectionMode::Line,
            ),
        );
        let mut expected: Vec<u16> = "ABかCきDE\r\n".encode_utf16().collect();
        expected.extend("ABかCきDE".encode_utf16());
        expected.extend(std::iter::repeat(u16::from(b' ')).take(71));
        expected.extend("ABかCきDE\r\nABかCきDE      ".encode_utf16());
        assert_eq!(text, expected);
    }
}
