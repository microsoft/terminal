//! Screen-buffer-facing reflow semantics built on the safe Rust `TextBuffer` owner.
//!
//! Microsoft `TextBuffer::Reflow` copies the attribute tail of each physical row
//! independently from its printable text. That distinction matters for erased
//! rows and trailing spaces whose colors must survive a resize. The lower-level
//! `TextBuffer::resize_width_reflow` owns glyph/cell reconstruction; this module
//! adds the screen-buffer contract that preserves those trailing attributes and
//! coordinates width plus height changes atomically.

use crate::row::DbcsAttribute;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferError};

#[derive(Debug, Clone, PartialEq, Eq)]
struct ReflowTailPlan {
    glyph_widths: Vec<u16>,
    trailing_attributes: Vec<TextAttribute>,
}

/// Resizes a text buffer using Microsoft's screen-buffer reflow semantics.
///
/// Printable glyphs and forced-wrap chains are delegated to the safe
/// [`TextBuffer::resize_width_reflow`] owner. In addition, the attribute tail of
/// the final physical row in every logical line is projected onto the remainder
/// of the destination row. This preserves colored trailing spaces and erased
/// rows exactly as the native `TextBuffer::Reflow` contract requires.
///
/// The operation is built on a clone and committed only after both dimensions
/// succeed, so invalid dimensions cannot leave the caller partially resized.
///
/// # Errors
///
/// Returns [`TextBufferError::EmptyWidth`] or [`TextBufferError::EmptyHeight`]
/// for zero dimensions, plus any row-storage error reported by the underlying
/// safe buffer owner.
pub fn resize_with_reflow(
    buffer: &mut TextBuffer,
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

    let mut resized = buffer.clone();

    if new_width != resized.width() {
        let tail_plans = collect_tail_plans(&resized);
        resized.resize_width_reflow(new_width, fill_attribute)?;
        apply_trailing_attributes(&mut resized, &tail_plans, new_width);
    }

    resized.resize_height(new_height, fill_attribute)?;
    *buffer = resized;
    Ok(())
}

fn collect_tail_plans(buffer: &TextBuffer) -> Vec<ReflowTailPlan> {
    let mut plans = Vec::new();
    let mut glyph_widths = Vec::new();

    for row in buffer.logical_rows() {
        let column_limit = if row.was_wrap_forced() {
            row.readable_column_count()
        } else {
            row.measure_right()
        };
        let mut column = 0_u16;

        while column < column_limit {
            match row.dbcs_attribute_at(i32::from(column)) {
                DbcsAttribute::Trailing => {
                    column = column.saturating_add(1);
                }
                DbcsAttribute::Single => {
                    glyph_widths.push(1);
                    column = column.saturating_add(1);
                }
                DbcsAttribute::Leading => {
                    glyph_widths.push(2);
                    column = column.saturating_add(2);
                }
            }
        }

        if !row.was_wrap_forced() {
            let tail_begin = usize::from(column_limit.min(row.size()));
            plans.push(ReflowTailPlan {
                glyph_widths: core::mem::take(&mut glyph_widths),
                trailing_attributes: row.attributes()[tail_begin..].to_vec(),
            });
        }
    }

    if !glyph_widths.is_empty() {
        plans.push(ReflowTailPlan {
            glyph_widths,
            trailing_attributes: Vec::new(),
        });
    }

    plans
}

fn apply_trailing_attributes(buffer: &mut TextBuffer, plans: &[ReflowTailPlan], new_width: u16) {
    let height = i32::from(buffer.height());
    let mut destination_y = 0_i32;

    for plan in plans {
        if destination_y >= height {
            break;
        }

        let mut column = 0_u16;
        for &original_width in &plan.glyph_widths {
            let glyph_width = original_width.min(new_width);
            if column != 0 && column.saturating_add(glyph_width) > new_width {
                destination_y += 1;
                if destination_y >= height {
                    return;
                }
                column = 0;
            }
            column = column.saturating_add(glyph_width);
        }

        if let Some(&last_attribute) = plan.trailing_attributes.last() {
            for destination_x in column..new_width {
                let offset = usize::from(destination_x - column);
                let attribute = plan
                    .trailing_attributes
                    .get(offset)
                    .copied()
                    .unwrap_or(last_attribute);
                buffer.row_mut(destination_y).replace_attributes(
                    i32::from(destination_x),
                    i32::from(destination_x + 1),
                    attribute,
                );
            }
        }

        destination_y += 1;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::row::Row;
    use crate::text_color::TextColor;

    fn background(index: u8) -> TextAttribute {
        let mut attribute = TextAttribute::default();
        attribute.set_background(TextColor::index16(index));
        attribute
    }

    fn write_ascii(
        buffer: &mut TextBuffer,
        row: i32,
        start: u16,
        text: &[u8],
        attribute: TextAttribute,
    ) {
        let row = buffer.row_mut(row);
        for (offset, &byte) in text.iter().enumerate() {
            let column = start + u16::try_from(offset).expect("test fixture fits in one row");
            row.replace_glyph(i32::from(column), 1, &[u16::from(byte)])
                .expect("test fixture glyph fits");
            row.replace_attributes(i32::from(column), i32::from(column + 1), attribute);
        }
    }

    fn assert_ascii(row: &Row, start: u16, text: &[u8]) {
        for (offset, &byte) in text.iter().enumerate() {
            let column = start + u16::try_from(offset).expect("test assertion fits in one row");
            assert_eq!(row.glyph_at(i32::from(column)), &[u16::from(byte)]);
        }
    }

    fn assert_attributes(row: &Row, begin: u16, end: u16, expected: TextAttribute) {
        for column in begin..end {
            assert_eq!(row.attribute_at(i32::from(column)), expected);
        }
    }

    fn microsoft_end_of_line_color_fixture() -> TextBuffer {
        let default = TextAttribute::default();
        let red = background(TextColor::DARK_RED);
        let green = background(TextColor::DARK_GREEN);
        let blue = background(TextColor::DARK_BLUE);
        let yellow = background(TextColor::DARK_YELLOW);
        let mut buffer = TextBuffer::new(80, 6, default).expect("fixture dimensions are valid");

        write_ascii(&mut buffer, 0, 0, b"AAAAA", red);
        write_ascii(&mut buffer, 1, 0, b"BBBBB", green);

        buffer.row_mut(2).replace_attributes(0, 5, blue);
        write_ascii(&mut buffer, 2, 1, b"CCC", blue);

        buffer
            .row_mut(3)
            .replace_glyph(0, 2, &[0xd83d, 0xde43])
            .expect("surrogate-pair glyph fits");
        buffer.row_mut(3).replace_attributes(0, 2, yellow);

        buffer.row_mut(4).set_attr_to_end(0, yellow);
        buffer
    }

    #[test]
    fn microsoft_screen_buffer_reflow_end_of_line_color_contract() {
        let default = TextAttribute::default();
        let red = background(TextColor::DARK_RED);
        let green = background(TextColor::DARK_GREEN);
        let blue = background(TextColor::DARK_BLUE);
        let yellow = background(TextColor::DARK_YELLOW);

        for new_width in [79_u16, 80, 81] {
            for new_height in [5_u16, 6, 7] {
                let mut buffer = microsoft_end_of_line_color_fixture();
                resize_with_reflow(&mut buffer, new_width, new_height, default)
                    .expect("Microsoft dx/dy resize vector succeeds");

                assert_eq!(buffer.width(), new_width);
                assert_eq!(buffer.height(), new_height);

                assert_ascii(buffer.row(0), 0, b"AAAAA");
                assert_attributes(buffer.row(0), 0, 5, red);
                assert_attributes(buffer.row(0), 5, new_width, default);

                assert_ascii(buffer.row(1), 0, b"BBBBB");
                assert_attributes(buffer.row(1), 0, 5, green);
                assert_attributes(buffer.row(1), 5, new_width, default);

                assert_eq!(buffer.row(2).glyph_at(0), &[u16::from(b' ')]);
                assert_ascii(buffer.row(2), 1, b"CCC");
                assert_eq!(buffer.row(2).glyph_at(4), &[u16::from(b' ')]);
                assert_attributes(buffer.row(2), 0, 5, blue);
                assert_attributes(buffer.row(2), 5, new_width, default);

                assert_eq!(buffer.row(3).glyph_at(0), &[0xd83d, 0xde43]);
                assert_eq!(buffer.row(3).dbcs_attribute_at(0), DbcsAttribute::Leading);
                assert_eq!(buffer.row(3).dbcs_attribute_at(1), DbcsAttribute::Trailing);
                assert_attributes(buffer.row(3), 0, 2, yellow);
                assert_attributes(buffer.row(3), 2, new_width, default);

                assert_attributes(buffer.row(4), 0, new_width, yellow);
            }
        }
    }

    #[test]
    fn microsoft_screen_buffer_reflow_smaller_long_line_with_color_contract() {
        let default = TextAttribute::default();
        let red = background(TextColor::DARK_RED);
        let green = background(TextColor::DARK_GREEN);
        let mut buffer = TextBuffer::new(80, 4, default).expect("fixture dimensions are valid");

        write_ascii(&mut buffer, 0, 0, &[b'A'; 70], red);
        buffer.row_mut(0).replace_attributes(70, 75, green);
        write_ascii(&mut buffer, 0, 71, b"BBB", green);

        resize_with_reflow(&mut buffer, 65, 4, default)
            .expect("Microsoft smaller-width reflow succeeds");

        assert_ascii(buffer.row(0), 0, &[b'A'; 65]);
        assert_attributes(buffer.row(0), 0, 65, red);
        assert!(buffer.row(0).was_wrap_forced());

        assert_ascii(buffer.row(1), 0, &[b'A'; 5]);
        assert_eq!(buffer.row(1).glyph_at(5), &[u16::from(b' ')]);
        assert_ascii(buffer.row(1), 6, b"BBB");
        assert_eq!(buffer.row(1).glyph_at(9), &[u16::from(b' ')]);
        assert_attributes(buffer.row(1), 0, 5, red);
        assert_attributes(buffer.row(1), 5, 10, green);
        assert_attributes(buffer.row(1), 10, 65, default);
        assert!(!buffer.row(1).was_wrap_forced());
    }

    #[test]
    fn microsoft_screen_buffer_reflow_bigger_long_line_with_color_contract() {
        let default = TextAttribute::default();
        let red = background(TextColor::DARK_RED);
        let green = background(TextColor::DARK_GREEN);
        let mut buffer = TextBuffer::new(80, 4, default).expect("fixture dimensions are valid");

        write_ascii(&mut buffer, 0, 0, &[b'A'; 80], red);
        buffer.row_mut(0).set_wrap_forced(true);
        write_ascii(&mut buffer, 1, 0, &[b'A'; 5], red);
        buffer.row_mut(1).replace_attributes(5, 10, green);
        write_ascii(&mut buffer, 1, 6, b"BBB", green);

        resize_with_reflow(&mut buffer, 95, 4, default)
            .expect("Microsoft larger-width reflow succeeds");

        assert_ascii(buffer.row(0), 0, &[b'A'; 85]);
        assert_attributes(buffer.row(0), 0, 85, red);
        assert_eq!(buffer.row(0).glyph_at(85), &[u16::from(b' ')]);
        assert_ascii(buffer.row(0), 86, b"BBB");
        assert_eq!(buffer.row(0).glyph_at(89), &[u16::from(b' ')]);
        assert_attributes(buffer.row(0), 85, 90, green);
        assert_attributes(buffer.row(0), 90, 95, default);
        assert!(!buffer.row(0).was_wrap_forced());
    }
}
