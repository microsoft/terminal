//! Cursor-aware Microsoft text-buffer reflow.
//!
//! `TextBuffer::Reflow` does more than repack glyphs: the cursor participates in
//! the logical extent of its row, may force otherwise trailing whitespace to be
//! retained, and determines which circular-buffer rows survive when shrinking
//! produces more physical rows than the destination can hold. This module keeps
//! those semantics explicit while leaving the lower-level storage owner safe and
//! platform-neutral.

use crate::row::DbcsAttribute;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferError, TextBufferPoint};

#[derive(Debug, Clone)]
struct SourceGlyph {
    text: Vec<u16>,
    width: u16,
    attribute: TextAttribute,
}

#[derive(Debug, Default)]
struct LogicalLine {
    glyphs: Vec<SourceGlyph>,
    columns: u32,
    cursor_column: Option<u32>,
}

#[derive(Debug, Default)]
struct OutputRow {
    glyphs: Vec<(u16, SourceGlyph)>,
    wrap_forced: bool,
    double_byte_padded: bool,
}

/// Reflows `buffer` while preserving the Microsoft cursor contract.
///
/// The cursor cell is treated as committed whitespace even when it lies to the
/// right of printable text (`REFLOW_JANK_CURSOR_WRAP` in the native owner).
/// When reflow produces more rows than the destination height, rows are kept in
/// circular-buffer order without allowing later text to overwrite the mapped
/// cursor row.
///
/// # Errors
///
/// Returns the same dimension/storage errors as [`TextBuffer`].
pub fn resize_with_reflow_and_cursor(
    buffer: &mut TextBuffer,
    cursor: &mut TextBufferPoint,
    new_width: u16,
    new_height: u16,
    fill_attribute: TextAttribute,
) -> Result<(), TextBufferError> {
    if new_width == 0 {
        return Err(TextBufferError::EmptyWidth);
    }
    if new_height == 0 {
        return Err(TextBufferError::EmptyHeight);
    }

    let old_cursor = TextBufferPoint::new(
        cursor.x.min(buffer.width().saturating_sub(1)),
        cursor.y.min(buffer.height().saturating_sub(1)),
    );
    let logical_lines = collect_logical_lines(buffer, old_cursor);
    let (mut rows, absolute_cursor) = wrap_logical_lines(&logical_lines, new_width)?;

    let cursor_limit = usize::from(absolute_cursor.y).saturating_add(usize::from(new_height));
    if rows.len() > cursor_limit {
        rows.truncate(cursor_limit);
    }

    let dropped = rows.len().saturating_sub(usize::from(new_height));
    if dropped != 0 {
        rows.drain(..dropped);
    }

    while rows.len() < usize::from(new_height) {
        rows.push(OutputRow::default());
    }

    let mut resized = TextBuffer::new(new_width, new_height, fill_attribute)?;
    for (y, output) in rows.iter().enumerate() {
        let row = resized.row_mut(i32::try_from(y).unwrap_or(i32::MAX));
        for (x, glyph) in &output.glyphs {
            row.replace_glyph(i32::from(*x), glyph.width, &glyph.text)?;
            row.replace_attributes(
                i32::from(*x),
                i32::from(x.saturating_add(glyph.width)),
                glyph.attribute,
            );
        }
        row.set_wrap_forced(output.wrap_forced);
        row.set_double_byte_padded(output.double_byte_padded);
    }

    *cursor = TextBufferPoint::new(
        absolute_cursor.x.min(new_width - 1),
        absolute_cursor
            .y
            .saturating_sub(u16::try_from(dropped).unwrap_or(u16::MAX))
            .min(new_height - 1),
    );
    *buffer = resized;
    Ok(())
}

fn collect_logical_lines(buffer: &TextBuffer, cursor: TextBufferPoint) -> Vec<LogicalLine> {
    let mut last_relevant_y = cursor.y;
    for y in 0..buffer.height() {
        let row = buffer.row(i32::from(y));
        if row.measure_right() != 0 || row.was_wrap_forced() {
            last_relevant_y = last_relevant_y.max(y);
        }
    }

    let mut lines = Vec::new();
    let mut line = LogicalLine::default();

    for y in 0..=last_relevant_y {
        let row = buffer.row(i32::from(y));
        let mut row_limit = if row.was_wrap_forced() {
            row.readable_column_count()
        } else {
            row.measure_right()
        };
        if y == cursor.y {
            row_limit = row_limit.max(cursor.x.saturating_add(1));
            line.cursor_column = Some(line.columns.saturating_add(u32::from(cursor.x)));
        }

        let mut column = 0_u16;
        while column < row_limit {
            match row.dbcs_attribute_at(i32::from(column)) {
                DbcsAttribute::Trailing => {
                    column = column.saturating_add(1);
                }
                DbcsAttribute::Single | DbcsAttribute::Leading => {
                    let width =
                        if row.dbcs_attribute_at(i32::from(column)) == DbcsAttribute::Leading {
                            2
                        } else {
                            1
                        };
                    line.glyphs.push(SourceGlyph {
                        text: row.glyph_at(i32::from(column)).to_vec(),
                        width,
                        attribute: row.attribute_at(i32::from(column)),
                    });
                    line.columns = line.columns.saturating_add(u32::from(width));
                    column = column.saturating_add(width);
                }
            }
        }

        if !row.was_wrap_forced() {
            lines.push(core::mem::take(&mut line));
        }
    }

    if !line.glyphs.is_empty() || line.cursor_column.is_some() {
        lines.push(line);
    }
    lines
}

fn wrap_logical_lines(
    lines: &[LogicalLine],
    new_width: u16,
) -> Result<(Vec<OutputRow>, TextBufferPoint), TextBufferError> {
    let mut rows = Vec::new();
    let mut cursor = None;

    for line in lines {
        let line_start_y = rows.len();
        let mut row = OutputRow::default();
        let mut x = 0_u16;
        let mut source_column = 0_u32;

        if line.glyphs.is_empty() {
            if line.cursor_column == Some(0) {
                cursor = Some(TextBufferPoint::new(
                    0,
                    u16::try_from(rows.len()).unwrap_or(u16::MAX),
                ));
            }
            rows.push(row);
            continue;
        }

        for glyph in &line.glyphs {
            if x != 0 && x.saturating_add(glyph.width) > new_width {
                // A wide glyph with exactly one destination cell remaining is
                // represented by Microsoft's transient DBCS padding cell. It
                // renders as a normal space, but must not become logical text
                // during a later reflow that grows the row again.
                if glyph.width == 2 && new_width.saturating_sub(x) == 1 {
                    row.double_byte_padded = true;
                }
                row.wrap_forced = true;
                rows.push(row);
                row = OutputRow::default();
                x = 0;
            }
            if x >= new_width {
                row.wrap_forced = true;
                rows.push(row);
                row = OutputRow::default();
                x = 0;
            }

            if let Some(cursor_column) = line.cursor_column
                && cursor.is_none()
                && cursor_column >= source_column
                && cursor_column < source_column.saturating_add(u32::from(glyph.width))
            {
                cursor = Some(TextBufferPoint::new(
                    x,
                    u16::try_from(rows.len()).unwrap_or(u16::MAX),
                ));
            }

            row.glyphs.push((x, glyph.clone()));
            x = x.saturating_add(glyph.width);
            source_column = source_column.saturating_add(u32::from(glyph.width));
        }

        if let Some(cursor_column) = line.cursor_column
            && cursor.is_none()
            && cursor_column == source_column
        {
            let y = rows.len();
            let (cursor_x, cursor_y) = if x < new_width {
                (x, y)
            } else {
                (0, y.saturating_add(1))
            };
            cursor = Some(TextBufferPoint::new(
                cursor_x,
                u16::try_from(cursor_y).unwrap_or(u16::MAX),
            ));
        }

        rows.push(row);

        debug_assert!(rows.len() > line_start_y);
    }

    let cursor = cursor.unwrap_or_else(|| TextBufferPoint::new(0, 0));
    Ok((rows, cursor))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cursor_whitespace_participates_in_reflow() {
        let attribute = TextAttribute::default();
        let mut buffer = TextBuffer::new(6, 5, attribute).unwrap();
        buffer
            .row_mut(0)
            .replace_glyph(0, 1, &[u16::from(b'A')])
            .unwrap();
        buffer
            .row_mut(1)
            .replace_glyph(0, 1, &[u16::from(b'$')])
            .unwrap();
        let mut cursor = TextBufferPoint::new(5, 1);

        resize_with_reflow_and_cursor(&mut buffer, &mut cursor, 5, 5, attribute).unwrap();

        assert_eq!(cursor, TextBufferPoint::new(0, 2));
        assert!(buffer.row(1).was_wrap_forced());
    }

    #[test]
    fn dbcs_padding_is_ephemeral_across_shrink_then_grow() {
        let attribute = TextAttribute::default();
        let mut buffer = TextBuffer::new(6, 5, attribute).unwrap();
        for (x, glyph) in [(0, 0x30ab), (2, 0x30bf), (4, 0x30ab)] {
            buffer.row_mut(0).replace_glyph(x, 2, &[glyph]).unwrap();
        }
        buffer.row_mut(0).set_wrap_forced(true);
        buffer.row_mut(1).replace_glyph(0, 2, &[0x30ca]).unwrap();
        buffer
            .row_mut(1)
            .replace_glyph(2, 1, &[u16::from(b'$')])
            .unwrap();
        let mut cursor = TextBufferPoint::new(2, 1);

        resize_with_reflow_and_cursor(&mut buffer, &mut cursor, 5, 5, attribute).unwrap();
        assert!(buffer.row(0).was_double_byte_padded());
        assert_eq!(cursor, TextBufferPoint::new(4, 1));

        resize_with_reflow_and_cursor(&mut buffer, &mut cursor, 6, 5, attribute).unwrap();
        assert_eq!(cursor, TextBufferPoint::new(2, 1));
        assert!(!buffer.row(0).was_double_byte_padded());
        assert_eq!(buffer.row(0).glyph_at(4), &[0x30ab]);
    }

    #[test]
    fn far_right_cursor_whitespace_survives_shrink_then_grow() {
        let attribute = TextAttribute::default();
        let mut buffer = TextBuffer::new(6, 5, attribute).unwrap();
        for (x, byte) in b"ABCDEF".iter().copied().enumerate() {
            buffer
                .row_mut(0)
                .replace_glyph(i32::try_from(x).unwrap(), 1, &[u16::from(byte)])
                .unwrap();
        }
        buffer
            .row_mut(1)
            .replace_glyph(0, 1, &[u16::from(b'$')])
            .unwrap();
        let mut cursor = TextBufferPoint::new(5, 1);

        resize_with_reflow_and_cursor(&mut buffer, &mut cursor, 5, 5, attribute).unwrap();
        assert_eq!(cursor, TextBufferPoint::new(0, 3));
        assert!(buffer.row(2).was_wrap_forced());

        let lines = collect_logical_lines(&buffer, cursor);
        assert_eq!(lines.len(), 2);
        assert_eq!(lines[1].cursor_column, Some(5));
        assert_eq!(lines[1].columns, 6);
        assert_eq!(lines[1].glyphs.len(), 6);
        let (_, remapped) = wrap_logical_lines(&lines, 6).unwrap();
        assert_eq!(remapped, TextBufferPoint::new(5, 1));

        resize_with_reflow_and_cursor(&mut buffer, &mut cursor, 6, 5, attribute).unwrap();
        assert_eq!(cursor, TextBufferPoint::new(5, 1));
    }
}
