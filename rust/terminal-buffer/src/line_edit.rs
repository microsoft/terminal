//! Safe horizontal line-editing semantics for screen-buffer operations.
//!
//! This owner covers the deterministic cell movement beneath VT DCH/ICH,
//! insert/replace mode and the DEC horizontal-margin column operations. Parser,
//! cursor rendering and Win32 API adaptation stay outside this module; the cell
//! grid, erase attributes and margin-limited shifts live here.

use core::ops::Range;

use crate::output_cell::OutputCellIterator;
use crate::row::{DbcsAttribute, Row, RowError};
use crate::row_writer::write_cells;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::TextBuffer;
use crate::width_detector::CodepointWidthDetector;

/// Inserts blank cells at `cursor`, shifting the remainder of `region` right.
///
/// Cells shifted beyond the region are discarded. The inserted cells receive
/// the active colors with non-color metadata cleared through the same standard
/// erase transformation used by the native terminal.
pub fn insert_cells(
    row: &mut Row,
    cursor: u16,
    count: u16,
    region: Range<u16>,
    active_attribute: TextAttribute,
) -> Result<(), RowError> {
    let Some(region) = normalize_region(row, region) else {
        return Ok(());
    };
    if cursor < region.start || cursor >= region.end || count == 0 {
        return Ok(());
    }

    let count = count.min(region.end - cursor);
    let source = row.clone();
    let erase = erase_attribute(active_attribute);

    fill_range(row, cursor..region.end, erase)?;
    let source_end = region.end - count;
    copy_shifted(&source, row, cursor..source_end, cursor + count, region.end)?;
    Ok(())
}

/// Deletes cells at `cursor`, shifting the remainder of `region` left.
///
/// The newly exposed tail is always overwritten with spaces carrying standard
/// erase attributes. Explicitly overwriting the complete tail prevents the
/// near-end DCH artifact that Microsoft's regression tests guard against.
pub fn delete_cells(
    row: &mut Row,
    cursor: u16,
    count: u16,
    region: Range<u16>,
    active_attribute: TextAttribute,
) -> Result<(), RowError> {
    let Some(region) = normalize_region(row, region) else {
        return Ok(());
    };
    if cursor < region.start || cursor >= region.end || count == 0 {
        return Ok(());
    }

    let count = count.min(region.end - cursor);
    let source = row.clone();
    let erase = erase_attribute(active_attribute);

    fill_range(row, cursor..region.end, erase)?;
    copy_shifted(
        &source,
        row,
        cursor + count..region.end,
        cursor,
        region.end - count,
    )?;
    Ok(())
}

/// Writes UTF-16 text in insert or replace mode within a horizontal region.
///
/// Unicode display width is measured with the same concrete width detector used
/// by the row writer. Insert mode first opens exactly that many cells; replace
/// mode writes in place. Text never crosses `region.end`.
pub fn write_text(
    row: &mut Row,
    cursor: u16,
    text: &[u16],
    attribute: TextAttribute,
    insert_mode: bool,
    region: Range<u16>,
) -> Result<u16, RowError> {
    let Some(region) = normalize_region(row, region) else {
        return Ok(cursor.min(row.size()));
    };
    if cursor < region.start || cursor >= region.end || text.is_empty() {
        return Ok(cursor.min(region.end));
    }

    let detector = CodepointWidthDetector;
    let remaining = usize::from(region.end - cursor);
    let requested_cells = OutputCellIterator::text_only(text, &detector).count();
    let written_cells = requested_cells.min(remaining);

    if insert_mode {
        insert_cells(
            row,
            cursor,
            u16::try_from(written_cells).expect("row cell counts are representable as u16"),
            region.clone(),
            attribute,
        )?;
    }

    write_cells(
        row,
        i32::from(cursor),
        OutputCellIterator::text_with_attribute(text, attribute, &detector)
            .with_fill_limit(written_cells),
    )
}

/// Applies ICH-like insertion to every row in a vertical span.
pub fn insert_columns(
    buffer: &mut TextBuffer,
    rows: Range<u16>,
    cursor_x: u16,
    count: u16,
    columns: Range<u16>,
    active_attribute: TextAttribute,
) -> Result<(), RowError> {
    for y in clamp_rows(buffer, rows) {
        insert_cells(
            buffer.row_mut(i32::from(y)),
            cursor_x,
            count,
            columns.clone(),
            active_attribute,
        )?;
    }
    Ok(())
}

/// Applies DCH-like deletion to every row in a vertical span.
pub fn delete_columns(
    buffer: &mut TextBuffer,
    rows: Range<u16>,
    cursor_x: u16,
    count: u16,
    columns: Range<u16>,
    active_attribute: TextAttribute,
) -> Result<(), RowError> {
    for y in clamp_rows(buffer, rows) {
        delete_cells(
            buffer.row_mut(i32::from(y)),
            cursor_x,
            count,
            columns.clone(),
            active_attribute,
        )?;
    }
    Ok(())
}

/// Performs DEC Forward Index repeatedly within horizontal margins.
///
/// The cursor advances until it reaches the right margin. Additional operations
/// scroll the whole rectangular margin area left one column while the cursor
/// remains pinned to the edge.
pub fn forward_index(
    buffer: &mut TextBuffer,
    rows: Range<u16>,
    columns: Range<u16>,
    cursor_x: u16,
    repetitions: u16,
    active_attribute: TextAttribute,
) -> Result<u16, RowError> {
    let Some(columns) = normalize_columns(buffer.width(), columns) else {
        return Ok(cursor_x.min(buffer.width().saturating_sub(1)));
    };
    let mut cursor = cursor_x.clamp(columns.start, columns.end - 1);

    for _ in 0..repetitions {
        if cursor < columns.end - 1 {
            cursor += 1;
        } else {
            delete_columns(
                buffer,
                rows.clone(),
                columns.start,
                1,
                columns.clone(),
                active_attribute,
            )?;
        }
    }
    Ok(cursor)
}

/// Performs DEC Back Index repeatedly within horizontal margins.
///
/// The cursor retreats until the left margin. Additional operations scroll the
/// rectangular margin area right one column while keeping the cursor at the
/// edge.
pub fn back_index(
    buffer: &mut TextBuffer,
    rows: Range<u16>,
    columns: Range<u16>,
    cursor_x: u16,
    repetitions: u16,
    active_attribute: TextAttribute,
) -> Result<u16, RowError> {
    let Some(columns) = normalize_columns(buffer.width(), columns) else {
        return Ok(cursor_x.min(buffer.width().saturating_sub(1)));
    };
    let mut cursor = cursor_x.clamp(columns.start, columns.end - 1);

    for _ in 0..repetitions {
        if cursor > columns.start {
            cursor -= 1;
        } else {
            insert_columns(
                buffer,
                rows.clone(),
                columns.start,
                1,
                columns.clone(),
                active_attribute,
            )?;
        }
    }
    Ok(cursor)
}

fn erase_attribute(mut attribute: TextAttribute) -> TextAttribute {
    attribute.set_standard_erase();
    attribute
}

fn normalize_region(row: &Row, region: Range<u16>) -> Option<Range<u16>> {
    normalize_columns(row.size(), region)
}

fn normalize_columns(width: u16, columns: Range<u16>) -> Option<Range<u16>> {
    let start = columns.start.min(width);
    let end = columns.end.min(width);
    (start < end).then_some(start..end)
}

fn clamp_rows(buffer: &TextBuffer, rows: Range<u16>) -> Range<u16> {
    rows.start.min(buffer.height())..rows.end.min(buffer.height())
}

fn fill_range(row: &mut Row, range: Range<u16>, attribute: TextAttribute) -> Result<(), RowError> {
    for column in range {
        row.replace_glyph(i32::from(column), 1, &[u16::from(b' ')])?;
        row.replace_attributes(
            i32::from(column),
            i32::from(column.saturating_add(1)),
            attribute,
        );
    }
    Ok(())
}

fn copy_shifted(
    source: &Row,
    target: &mut Row,
    source_range: Range<u16>,
    destination_start: u16,
    destination_end: u16,
) -> Result<(), RowError> {
    let mut source_column = source_range.start;

    while source_column < source_range.end {
        let destination_column =
            destination_start.saturating_add(source_column - source_range.start);
        if destination_column >= destination_end {
            break;
        }

        match source.dbcs_attribute_at(i32::from(source_column)) {
            DbcsAttribute::Trailing => {
                source_column = source_column.saturating_add(1);
            }
            DbcsAttribute::Single => {
                target.replace_glyph(
                    i32::from(destination_column),
                    1,
                    source.glyph_at(i32::from(source_column)),
                )?;
                target.replace_attributes(
                    i32::from(destination_column),
                    i32::from(destination_column + 1),
                    source.attribute_at(i32::from(source_column)),
                );
                source_column = source_column.saturating_add(1);
            }
            DbcsAttribute::Leading => {
                let complete_source = source_column.saturating_add(1) < source_range.end;
                let complete_destination = destination_column.saturating_add(1) < destination_end;
                if complete_source && complete_destination {
                    target.replace_glyph(
                        i32::from(destination_column),
                        2,
                        source.glyph_at(i32::from(source_column)),
                    )?;
                    target.replace_attributes(
                        i32::from(destination_column),
                        i32::from(destination_column + 1),
                        source.attribute_at(i32::from(source_column)),
                    );
                    target.replace_attributes(
                        i32::from(destination_column + 1),
                        i32::from(destination_column + 2),
                        source.attribute_at(i32::from(source_column + 1)),
                    );
                }
                source_column = source_column.saturating_add(2);
            }
        }
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::text_color::TextColor;

    fn tagged(background: u8) -> TextAttribute {
        let mut attribute = TextAttribute::default();
        attribute.set_background(TextColor::index16(background));
        attribute
    }

    fn active_erase_source() -> TextAttribute {
        let mut attribute = tagged(TextColor::BRIGHT_MAGENTA);
        attribute.set_crossed_out(true);
        attribute.set_reverse_video(true);
        attribute.set_underline_style(crate::text_attribute::UnderlineStyle::Curly);
        attribute
    }

    fn expected_erase() -> TextAttribute {
        let mut attribute = active_erase_source();
        attribute.set_standard_erase();
        attribute
    }

    fn fill(row: &mut Row, glyph: u8, attribute: TextAttribute) {
        for column in 0..row.size() {
            row.replace_glyph(i32::from(column), 1, &[u16::from(glyph)])
                .expect("fixture glyph fits");
            row.replace_attributes(i32::from(column), i32::from(column + 1), attribute);
        }
    }

    fn write_ascii(row: &mut Row, start: u16, text: &[u8], attribute: TextAttribute) {
        for (offset, &glyph) in text.iter().enumerate() {
            let column = start + u16::try_from(offset).expect("fixture fits");
            row.replace_glyph(i32::from(column), 1, &[u16::from(glyph)])
                .expect("fixture glyph fits");
            row.replace_attributes(i32::from(column), i32::from(column + 1), attribute);
        }
    }

    fn assert_ascii(row: &Row, start: u16, text: &[u8]) {
        for (offset, &glyph) in text.iter().enumerate() {
            let column = start + u16::try_from(offset).expect("assertion fits");
            assert_eq!(row.glyph_at(i32::from(column)), &[u16::from(glyph)]);
        }
    }

    fn assert_attributes(row: &Row, start: u16, end: u16, attribute: TextAttribute) {
        for column in start..end {
            assert_eq!(row.attribute_at(i32::from(column)), attribute);
        }
    }

    fn assert_repeated(row: &Row, start: u16, end: u16, glyph: u8, attribute: TextAttribute) {
        for column in start..end {
            assert_eq!(row.glyph_at(i32::from(column)), &[u16::from(glyph)]);
            assert_eq!(row.attribute_at(i32::from(column)), attribute);
        }
    }

    fn ich_dch_fixture() -> Row {
        let buffer_attr = tagged(TextColor::DARK_GREEN);
        let text_attr = tagged(TextColor::DARK_BLUE);
        let mut row = Row::new(40, buffer_attr).expect("fixture dimensions are valid");
        fill(&mut row, b'Q', buffer_attr);
        write_ascii(&mut row, 10, b"ABCDEFGHIJKLMNOPQRST", text_attr);
        row
    }

    #[test]
    fn microsoft_screen_buffer_delete_chars_near_end_of_line_contract() {
        for dx in [1_u16, 2, 3, 5, 8, 13, 21, 34] {
            for count in [1_u16, 2, 3, 5, 8, 13, 21, 34] {
                let mut row = Row::new(80, TextAttribute::default()).unwrap();
                fill(&mut row, b'X', TextAttribute::default());
                let cursor = 80 - dx;

                delete_cells(&mut row, cursor, count, 0..80, TextAttribute::default())
                    .expect("DCH matrix vector succeeds");

                let spaces = dx.min(count);
                assert_repeated(&row, 0, 80 - spaces, b'X', TextAttribute::default());
                assert_repeated(&row, 80 - spaces, 80, b' ', TextAttribute::default());
            }
        }
    }

    #[test]
    fn microsoft_screen_buffer_delete_chars_near_end_simple_first_contract() {
        let mut row = Row::new(8, TextAttribute::default()).unwrap();
        write_ascii(&mut row, 0, b"ABCDEFG", TextAttribute::default());

        delete_cells(&mut row, 3, 3, 0..8, TextAttribute::default()).unwrap();

        assert_ascii(&row, 0, b"ABCG");
        assert_repeated(&row, 4, 8, b' ', TextAttribute::default());
    }

    #[test]
    fn microsoft_screen_buffer_delete_chars_near_end_simple_second_contract() {
        let mut row = Row::new(8, TextAttribute::default()).unwrap();
        write_ascii(&mut row, 0, b"ABCDEFG", TextAttribute::default());

        delete_cells(&mut row, 2, 4, 0..8, TextAttribute::default()).unwrap();

        assert_ascii(&row, 0, b"ABG");
        assert_repeated(&row, 3, 8, b' ', TextAttribute::default());
    }

    #[test]
    fn microsoft_screen_buffer_insert_chars_contract() {
        let buffer_attr = tagged(TextColor::DARK_GREEN);
        let erase = expected_erase();

        for horizontal_margins_active in [true, false] {
            let region = if horizontal_margins_active {
                10..30
            } else {
                0..40
            };

            let mut row = ich_dch_fixture();
            insert_cells(&mut row, 20, 5, region.clone(), active_erase_source()).unwrap();
            assert_repeated(&row, 0, 10, b'Q', buffer_attr);
            assert_ascii(&row, 10, b"ABCDEFGHIJ");
            assert_repeated(&row, 20, 25, b' ', erase);
            if horizontal_margins_active {
                assert_ascii(&row, 25, b"KLMNO");
                assert_repeated(&row, 30, 40, b'Q', buffer_attr);
            } else {
                assert_ascii(&row, 25, b"KLMNOPQRST");
                assert_repeated(&row, 35, 40, b'Q', buffer_attr);
            }

            let mut row = ich_dch_fixture();
            let edge = if horizontal_margins_active { 29 } else { 39 };
            insert_cells(&mut row, edge, 5, region.clone(), active_erase_source()).unwrap();
            assert_eq!(row.glyph_at(i32::from(edge)), &[u16::from(b' ')]);
            assert_eq!(row.attribute_at(i32::from(edge)), erase);
            if horizontal_margins_active {
                assert_ascii(&row, 10, b"ABCDEFGHIJKLMNOPQRS");
                assert_repeated(&row, 30, 40, b'Q', buffer_attr);
            } else {
                assert_ascii(&row, 10, b"ABCDEFGHIJKLMNOPQRST");
                assert_repeated(&row, 30, 39, b'Q', buffer_attr);
            }

            let mut row = ich_dch_fixture();
            let start = region.start;
            insert_cells(&mut row, start, 100, region.clone(), active_erase_source()).unwrap();
            if horizontal_margins_active {
                assert_repeated(&row, 0, 10, b'Q', buffer_attr);
                assert_repeated(&row, 10, 30, b' ', erase);
                assert_repeated(&row, 30, 40, b'Q', buffer_attr);
            } else {
                assert_repeated(&row, 0, 40, b' ', erase);
            }
        }
    }

    #[test]
    fn microsoft_screen_buffer_delete_chars_contract() {
        let buffer_attr = tagged(TextColor::DARK_GREEN);
        let erase = expected_erase();

        for horizontal_margins_active in [true, false] {
            let region = if horizontal_margins_active {
                10..30
            } else {
                0..40
            };

            let mut row = ich_dch_fixture();
            delete_cells(&mut row, 20, 5, region.clone(), active_erase_source()).unwrap();
            assert_repeated(&row, 0, 10, b'Q', buffer_attr);
            assert_ascii(&row, 10, b"ABCDEFGHIJ");
            assert_ascii(&row, 20, b"PQRST");
            if horizontal_margins_active {
                assert_repeated(&row, 25, 30, b' ', erase);
                assert_repeated(&row, 30, 40, b'Q', buffer_attr);
            } else {
                assert_repeated(&row, 25, 35, b'Q', buffer_attr);
                assert_repeated(&row, 35, 40, b' ', erase);
            }

            let mut row = ich_dch_fixture();
            let edge = if horizontal_margins_active { 29 } else { 39 };
            delete_cells(&mut row, edge, 5, region.clone(), active_erase_source()).unwrap();
            assert_eq!(row.glyph_at(i32::from(edge)), &[u16::from(b' ')]);
            assert_eq!(row.attribute_at(i32::from(edge)), erase);
            if horizontal_margins_active {
                assert_ascii(&row, 10, b"ABCDEFGHIJKLMNOPQRS");
                assert_repeated(&row, 30, 40, b'Q', buffer_attr);
            } else {
                assert_ascii(&row, 10, b"ABCDEFGHIJKLMNOPQRST");
                assert_repeated(&row, 30, 39, b'Q', buffer_attr);
            }

            let mut row = ich_dch_fixture();
            let start = region.start;
            delete_cells(&mut row, start, 100, region.clone(), active_erase_source()).unwrap();
            if horizontal_margins_active {
                assert_repeated(&row, 0, 10, b'Q', buffer_attr);
                assert_repeated(&row, 10, 30, b' ', erase);
                assert_repeated(&row, 30, 40, b'Q', buffer_attr);
            } else {
                assert_repeated(&row, 0, 40, b' ', erase);
            }
        }
    }

    #[test]
    fn microsoft_screen_buffer_insert_replace_mode_contract() {
        let initial_attr = tagged(TextColor::DARK_BLUE);
        let new_attr = tagged(TextColor::BRIGHT_GREEN);
        let digits: Vec<u16> = b"12345".iter().map(|&value| u16::from(value)).collect();

        let mut row = Row::new(60, initial_attr).unwrap();
        fill(&mut row, b'*', initial_attr);
        write_ascii(&mut row, 0, b"ABCDEFGHIJKLMNOPQRST", initial_attr);
        write_text(&mut row, 10, &digits, new_attr, true, 0..60).unwrap();
        assert_ascii(&row, 0, b"ABCDEFGHIJ");
        assert_ascii(&row, 10, b"12345");
        assert_attributes(&row, 10, 15, new_attr);
        assert_ascii(&row, 15, b"KLMNOPQRST");
        assert_eq!(row.glyph_at(35), &[u16::from(b'*')]);
        assert_eq!(row.attribute_at(35), initial_attr);

        let mut row = Row::new(60, initial_attr).unwrap();
        fill(&mut row, b'*', initial_attr);
        write_ascii(&mut row, 0, b"ABCDEFGHIJKLMNOPQRST", initial_attr);
        write_text(&mut row, 10, &digits, new_attr, false, 0..60).unwrap();
        assert_ascii(&row, 0, b"ABCDEFGHIJ");
        assert_ascii(&row, 10, b"12345");
        assert_attributes(&row, 10, 15, new_attr);
        assert_ascii(&row, 15, b"PQRST");
        assert_eq!(row.glyph_at(35), &[u16::from(b'*')]);
        assert_eq!(row.attribute_at(35), initial_attr);
    }

    fn horizontal_fixture() -> TextBuffer {
        let attr = tagged(TextColor::DARK_BLUE);
        let mut buffer = TextBuffer::new(40, 25, attr).unwrap();
        for y in 0..25 {
            write_ascii(
                buffer.row_mut(y),
                0,
                b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn",
                attr,
            );
        }
        buffer
    }

    #[test]
    fn microsoft_screen_buffer_horizontal_scroll_operations_contract() {
        let attr = tagged(TextColor::DARK_BLUE);
        let erase = expected_erase();

        let mut insert = horizontal_fixture();
        insert_columns(&mut insert, 14..20, 20, 4, 10..30, active_erase_source()).unwrap();
        for y in 14..20 {
            assert_ascii(insert.row(y), 10, b"KLMNOPQRST");
            assert_repeated(insert.row(y), 20, 24, b' ', erase);
            assert_ascii(insert.row(y), 24, b"UVWXYZ");
        }
        assert_ascii(
            insert.row(13),
            0,
            b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn",
        );
        assert_ascii(
            insert.row(20),
            0,
            b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmn",
        );

        let mut delete = horizontal_fixture();
        delete_columns(&mut delete, 14..20, 20, 4, 10..30, active_erase_source()).unwrap();
        for y in 14..20 {
            assert_ascii(delete.row(y), 10, b"KLMNOPQRSTYZabcd");
            assert_repeated(delete.row(y), 26, 30, b' ', erase);
        }

        let mut forward = horizontal_fixture();
        let cursor =
            forward_index(&mut forward, 14..20, 10..30, 27, 4, active_erase_source()).unwrap();
        assert_eq!(cursor, 29);
        for y in 14..20 {
            assert_ascii(forward.row(y), 10, b"MNOPQRSTUVWXYZabcd");
            assert_repeated(forward.row(y), 28, 30, b' ', erase);
        }

        let mut back = horizontal_fixture();
        let cursor = back_index(&mut back, 14..20, 10..30, 12, 4, active_erase_source()).unwrap();
        assert_eq!(cursor, 10);
        for y in 14..20 {
            assert_repeated(back.row(y), 10, 12, b' ', erase);
            assert_ascii(back.row(y), 12, b"KLMNOPQRSTUVWXYZab");
        }

        assert_eq!(insert.row(14).attribute_at(10), attr);
    }
}
