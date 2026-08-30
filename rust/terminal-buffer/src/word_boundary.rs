//! Screen-buffer word-boundary policy shared by selection-style consumers.
//!
//! Windows Terminal's host treats the returned end coordinate as exclusive. If
//! the query lands on a separator, the boundary belongs to the word immediately
//! to its left. The optional leading-zero policy preserves the recognized `0x`,
//! `0X`, and `0n` numeric prefixes while trimming other redundant leading zeroes.

use crate::row::DelimiterClass;
use crate::text_buffer::{TextBuffer, TextBufferPoint};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct WordBoundary {
    pub start: TextBufferPoint,
    pub end: TextBufferPoint,
}

/// Returns the host-style word boundary containing `point`.
///
/// `end` is exclusive and can therefore equal the row width. The operation is
/// entirely platform-neutral and reads only validated `TextBuffer` row storage.
#[must_use]
pub fn screen_word_boundary(
    buffer: &TextBuffer,
    point: TextBufferPoint,
    trim_leading_zeros: bool,
    word_delimiters: &[u16],
) -> WordBoundary {
    let y = point.y.min(buffer.height().saturating_sub(1));
    let row = buffer.row(i32::from(y));
    let readable = row.readable_column_count();

    if readable == 0 {
        let origin = TextBufferPoint::new(0, y);
        return WordBoundary {
            start: origin,
            end: origin,
        };
    }

    let mut x = point.x.min(readable - 1);
    x = row.adjust_to_glyph_start(i32::from(x));

    // The host's GetWordBoundary treats a separator coordinate as the exclusive
    // end of the preceding word. This is observable in Microsoft's test where
    // querying the space after "some" returns the boundary for "some".
    let mut class = row.delimiter_class_at(i32::from(x), word_delimiters);
    if class != DelimiterClass::RegularChar && x > 0 {
        let previous = row.navigate_to_previous(i32::from(x));
        if previous < x {
            x = previous;
            class = row.delimiter_class_at(i32::from(x), word_delimiters);
        }
    }

    let mut start = x;
    while start > 0 {
        let previous = row.navigate_to_previous(i32::from(start));
        if previous >= start
            || row.delimiter_class_at(i32::from(previous), word_delimiters) != class
        {
            break;
        }
        start = previous;
    }

    let mut end = row.navigate_to_next(i32::from(x));
    while end < readable && row.delimiter_class_at(i32::from(end), word_delimiters) == class {
        let next = row.navigate_to_next(i32::from(end));
        if next <= end {
            break;
        }
        end = next;
    }

    if trim_leading_zeros && class == DelimiterClass::RegularChar {
        start = trimmed_numeric_start(row, start, end);
    }

    WordBoundary {
        start: TextBufferPoint::new(start, y),
        end: TextBufferPoint::new(end, y),
    }
}

fn trimmed_numeric_start(row: &crate::row::Row, start: u16, end: u16) -> u16 {
    if start >= end || row.glyph_at(i32::from(start)).first() != Some(&u16::from(b'0')) {
        return start;
    }

    let after_first = row.navigate_to_next(i32::from(start));
    if after_first < end {
        let prefix = row
            .glyph_at(i32::from(after_first))
            .first()
            .copied()
            .unwrap_or_default();
        if matches!(prefix, unit if unit == u16::from(b'x') || unit == u16::from(b'X') || unit == u16::from(b'n'))
        {
            return start;
        }
    }

    let mut cursor = start;
    while cursor < end && row.glyph_at(i32::from(cursor)).first() == Some(&u16::from(b'0')) {
        let next = row.navigate_to_next(i32::from(cursor));
        if next <= cursor {
            break;
        }
        cursor = next;
    }

    // Keep an all-zero word non-empty. Otherwise, the first non-zero glyph is
    // the boundary start, matching the host's trim-leading-zero behavior.
    if cursor < end { cursor } else { start }
}
