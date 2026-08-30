//! Portable coordinate queries over the safe text-buffer owner.
//!
//! These helpers cover host `TextBuffer` queries whose results are purely a
//! function of row storage: finding the last non-space cell and normalizing
//! glyph boundaries, including the exclusive end coordinate just beyond a row.

use crate::text_buffer::{TextBuffer, TextBufferPoint};

/// Finds the last non-space cell at or above `start.y`.
#[must_use]
pub fn last_non_space_character(
    buffer: &TextBuffer,
    start: TextBufferPoint,
) -> Option<TextBufferPoint> {
    let mut y = start.y.min(buffer.height().saturating_sub(1));
    loop {
        let right = buffer.row(i32::from(y)).measure_right();
        if right > 0 {
            return Some(TextBufferPoint::new(right - 1, y));
        }
        if y == 0 {
            return None;
        }
        y -= 1;
    }
}

/// Returns the leading cell of the glyph containing `point`.
#[must_use]
pub fn glyph_start(buffer: &TextBuffer, point: TextBufferPoint) -> TextBufferPoint {
    let y = point.y.min(buffer.height().saturating_sub(1));
    let row = buffer.row(i32::from(y));
    TextBufferPoint::new(row.adjust_to_glyph_start(i32::from(point.x)), y)
}

/// Returns the exclusive cell coordinate immediately after the glyph containing
/// `point`. Crossing the right edge advances to column zero of the next logical
/// row; the final row therefore returns `(0, height)` as an end sentinel.
#[must_use]
pub fn glyph_end(buffer: &TextBuffer, point: TextBufferPoint) -> TextBufferPoint {
    let y = point.y.min(buffer.height().saturating_sub(1));
    let row = buffer.row(i32::from(y));
    let start = row.adjust_to_glyph_start(i32::from(point.x));
    let end = row.adjust_to_glyph_end(i32::from(start.saturating_add(1)));
    if end >= buffer.width() {
        TextBufferPoint::new(0, y.saturating_add(1))
    } else {
        TextBufferPoint::new(end, y)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::row_writer::replace_text;
    use crate::text_attribute::TextAttribute;

    #[test]
    fn microsoft_text_buffer_get_last_non_space_character_contract() {
        let mut buffer = TextBuffer::new(80, 15, TextAttribute::default()).unwrap();
        for (y, text) in [(0_i32, "first"), (1, "second"), (2, "third"), (3, "fourth")] {
            replace_text(
                buffer.row_mut(y),
                0,
                &text.encode_utf16().collect::<Vec<_>>(),
            )
            .unwrap();
        }

        assert_eq!(
            last_non_space_character(&buffer, TextBufferPoint::new(0, 3)),
            Some(TextBufferPoint::new(5, 3))
        );
        assert_eq!(
            last_non_space_character(&buffer, TextBufferPoint::new(0, 4)),
            Some(TextBufferPoint::new(5, 3))
        );
        assert_eq!(
            last_non_space_character(&buffer, TextBufferPoint::new(0, 14)),
            Some(TextBufferPoint::new(5, 3))
        );
    }

    #[test]
    fn microsoft_text_buffer_get_glyph_boundaries_contract() {
        let vectors = [
            (
                TextBufferPoint::new(0, 0),
                TextBufferPoint::new(1, 0),
                TextBufferPoint::new(2, 0),
            ),
            (
                TextBufferPoint::new(0, 1),
                TextBufferPoint::new(1, 1),
                TextBufferPoint::new(2, 1),
            ),
            (
                TextBufferPoint::new(1, 1),
                TextBufferPoint::new(2, 1),
                TextBufferPoint::new(3, 1),
            ),
            (
                TextBufferPoint::new(8, 1),
                TextBufferPoint::new(9, 1),
                TextBufferPoint::new(0, 2),
            ),
            (
                TextBufferPoint::new(7, 1),
                TextBufferPoint::new(8, 1),
                TextBufferPoint::new(9, 1),
            ),
            (
                TextBufferPoint::new(9, 9),
                TextBufferPoint::new(0, 10),
                TextBufferPoint::new(0, 10),
            ),
        ];

        for (start, normal_end, wide_end) in vectors {
            let mut normal = TextBuffer::new(10, 10, TextAttribute::default()).unwrap();
            normal
                .row_mut(i32::from(start.y))
                .replace_glyph(i32::from(start.x), 1, &[u16::from(b'X')])
                .unwrap();
            assert_eq!(glyph_start(&normal, start), start);
            assert_eq!(glyph_end(&normal, start), normal_end);

            let mut wide = TextBuffer::new(10, 10, TextAttribute::default()).unwrap();
            if start.x < 9 {
                wide.row_mut(i32::from(start.y))
                    .replace_glyph(i32::from(start.x), 2, &[0xd83c, 0xdf2f])
                    .unwrap();
            } else {
                wide.row_mut(i32::from(start.y))
                    .replace_glyph(i32::from(start.x), 1, &[u16::from(b'X')])
                    .unwrap();
            }
            assert_eq!(glyph_start(&wide, start), start);
            assert_eq!(glyph_end(&wide, start), wide_end);
        }
    }
}
