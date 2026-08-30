//! Safe rectangular screen-buffer operations.
//!
//! This owner covers the deterministic cell-grid behavior beneath DEC
//! rectangular fill/erase/attribute/copy commands plus overlap-safe scrolling.
//! VT parsing, viewport-relative coordinate resolution and Win32 adaptation stay
//! outside this module; callers provide already-resolved screen coordinates.

use core::ops::Range;

use crate::row::{DbcsAttribute, Row, RowError};
use crate::text_attribute::{TextAttribute, UnderlineStyle};
use crate::text_buffer::{TextBuffer, TextBufferPoint};
use crate::text_color::TextColor;

/// Exclusive screen-coordinate rectangle.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ScreenRect {
    pub left: u16,
    pub top: u16,
    pub right: u16,
    pub bottom: u16,
}

impl ScreenRect {
    #[must_use]
    pub const fn new(left: u16, top: u16, right: u16, bottom: u16) -> Self {
        Self {
            left,
            top,
            right,
            bottom,
        }
    }

    #[must_use]
    pub const fn width(self) -> u16 {
        self.right.saturating_sub(self.left)
    }

    #[must_use]
    pub const fn height(self) -> u16 {
        self.bottom.saturating_sub(self.top)
    }
}

/// Attribute changes used by DEC Change Attributes in Rectangular Area.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct AttributePatch {
    pub reverse_video: Option<bool>,
    pub underline_style: Option<UnderlineStyle>,
    pub underline_color: Option<TextColor>,
}

/// Fills every cell in a resolved rectangle with one narrow UTF-16 character
/// and the supplied attributes.
pub fn fill_rect(
    buffer: &mut TextBuffer,
    rect: ScreenRect,
    character: u16,
    attribute: TextAttribute,
) -> Result<(), RowError> {
    let Some(rect) = normalize_rect(buffer, rect) else {
        return Ok(());
    };

    for y in rect.top..rect.bottom {
        let row = buffer.row_mut(i32::from(y));
        let columns = screen_columns(row, rect.left, rect.right);
        for column in columns {
            row.replace_glyph(i32::from(column), 1, &[character])?;
            row.replace_attributes(
                i32::from(column),
                i32::from(column.saturating_add(1)),
                attribute,
            );
        }
    }
    Ok(())
}

/// Erases a rectangle with spaces carrying standard-erase active attributes.
pub fn erase_rect(
    buffer: &mut TextBuffer,
    rect: ScreenRect,
    active_attribute: TextAttribute,
) -> Result<(), RowError> {
    let mut erase = active_attribute;
    erase.set_standard_erase();
    fill_rect(buffer, rect, u16::from(b' '), erase)
}

/// Selectively erases text while preserving each cell's existing attributes.
/// Protected cells are skipped, matching DECSERA's selective behavior.
pub fn selective_erase_rect(buffer: &mut TextBuffer, rect: ScreenRect) -> Result<(), RowError> {
    let Some(rect) = normalize_rect(buffer, rect) else {
        return Ok(());
    };

    for y in rect.top..rect.bottom {
        let row = buffer.row_mut(i32::from(y));
        let columns = screen_columns(row, rect.left, rect.right);
        for column in columns {
            let attribute = row.attribute_at(i32::from(column));
            if attribute.is_protected() {
                continue;
            }
            let dirty = row.replace_glyph(i32::from(column), 1, &[u16::from(b' ')])?;
            row.replace_attributes(i32::from(dirty.begin), i32::from(dirty.end), attribute);
        }
    }
    Ok(())
}

/// Applies a rectangular rendition patch while leaving text and unspecified
/// attributes untouched.
pub fn patch_rect_attributes(buffer: &mut TextBuffer, rect: ScreenRect, patch: AttributePatch) {
    let Some(rect) = normalize_rect(buffer, rect) else {
        return;
    };

    for y in rect.top..rect.bottom {
        let row = buffer.row_mut(i32::from(y));
        let columns = screen_columns(row, rect.left, rect.right);
        for column in columns {
            let mut attribute = row.attribute_at(i32::from(column));
            if let Some(enabled) = patch.reverse_video {
                attribute.set_reverse_video(enabled);
            }
            if let Some(style) = patch.underline_style {
                attribute.set_underline_style(style);
            }
            if let Some(color) = patch.underline_color {
                attribute.set_underline_color(color);
            }
            row.replace_attributes(
                i32::from(column),
                i32::from(column.saturating_add(1)),
                attribute,
            );
        }
    }
}

/// Reverses the intensity bit in a resolved rectangle while preserving all
/// other text attributes.
pub fn reverse_rect_intensity(buffer: &mut TextBuffer, rect: ScreenRect) {
    let Some(rect) = normalize_rect(buffer, rect) else {
        return;
    };

    for y in rect.top..rect.bottom {
        let row = buffer.row_mut(i32::from(y));
        let columns = screen_columns(row, rect.left, rect.right);
        for column in columns {
            let mut attribute = row.attribute_at(i32::from(column));
            attribute.set_intense(!attribute.is_intense());
            row.replace_attributes(
                i32::from(column),
                i32::from(column.saturating_add(1)),
                attribute,
            );
        }
    }
}

/// Copies a rectangle from an immutable snapshot so overlap cannot make a wide
/// glyph erase itself while it is being moved.
///
/// Source screen columns are projected through each row's line rendition. A
/// double-width source row therefore contributes half as many underlying cells,
/// matching DECCRA's source-coordinate semantics.
pub fn copy_rect(
    buffer: &mut TextBuffer,
    source: ScreenRect,
    destination: TextBufferPoint,
) -> Result<(), RowError> {
    let snapshot = buffer.clone();
    copy_rect_from(&snapshot, buffer, source, destination)
}

/// Scrolls/copies a rectangle from a stable snapshot after blanking its source
/// area. This is the safe overlap primitive used by horizontal ScrollRegion-like
/// movement.
pub fn scroll_rect(
    buffer: &mut TextBuffer,
    source: ScreenRect,
    destination: TextBufferPoint,
    fill_attribute: TextAttribute,
) -> Result<(), RowError> {
    let snapshot = buffer.clone();
    fill_rect(buffer, source, u16::from(b' '), fill_attribute)?;
    copy_rect_from(&snapshot, buffer, source, destination)
}

fn copy_rect_from(
    source_buffer: &TextBuffer,
    target_buffer: &mut TextBuffer,
    source_rect: ScreenRect,
    destination: TextBufferPoint,
) -> Result<(), RowError> {
    let Some(source_rect) = normalize_rect(source_buffer, source_rect) else {
        return Ok(());
    };

    for source_y in source_rect.top..source_rect.bottom {
        let row_offset = source_y - source_rect.top;
        let Some(target_y) = destination.y.checked_add(row_offset) else {
            break;
        };
        if target_y >= target_buffer.height() {
            break;
        }

        let source_row = source_buffer.row(i32::from(source_y));
        let source_columns = screen_columns(source_row, source_rect.left, source_rect.right);
        if source_columns.is_empty() {
            continue;
        }

        let target_row = target_buffer.row_mut(i32::from(target_y));
        let destination_start = screen_column(target_row, destination.x);
        copy_row_segment(source_row, target_row, source_columns, destination_start)?;
    }

    Ok(())
}

fn copy_row_segment(
    source: &Row,
    target: &mut Row,
    source_columns: Range<u16>,
    destination_start: u16,
) -> Result<(), RowError> {
    let target_limit = target.readable_column_count();
    let mut source_column = source_columns.start;

    while source_column < source_columns.end {
        let relative = source_column - source_columns.start;
        let destination_column = destination_start.saturating_add(relative);
        if destination_column >= target_limit {
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
                    i32::from(destination_column.saturating_add(1)),
                    source.attribute_at(i32::from(source_column)),
                );
                source_column = source_column.saturating_add(1);
            }
            DbcsAttribute::Leading => {
                let source_complete = source_column.saturating_add(1) < source_columns.end;
                let target_complete = destination_column.saturating_add(1) < target_limit;
                if source_complete && target_complete {
                    target.replace_glyph(
                        i32::from(destination_column),
                        2,
                        source.glyph_at(i32::from(source_column)),
                    )?;
                    target.replace_attributes(
                        i32::from(destination_column),
                        i32::from(destination_column.saturating_add(1)),
                        source.attribute_at(i32::from(source_column)),
                    );
                    target.replace_attributes(
                        i32::from(destination_column.saturating_add(1)),
                        i32::from(destination_column.saturating_add(2)),
                        source.attribute_at(i32::from(source_column.saturating_add(1))),
                    );
                }
                source_column = source_column.saturating_add(2);
            }
        }
    }

    Ok(())
}

fn normalize_rect(buffer: &TextBuffer, rect: ScreenRect) -> Option<ScreenRect> {
    let left = rect.left.min(buffer.width());
    let right = rect.right.min(buffer.width());
    let top = rect.top.min(buffer.height());
    let bottom = rect.bottom.min(buffer.height());
    (left < right && top < bottom).then_some(ScreenRect::new(left, top, right, bottom))
}

fn screen_columns(row: &Row, left: u16, right: u16) -> Range<u16> {
    let readable = row.readable_column_count();
    let start = screen_column(row, left).min(readable);
    let end = if row.line_rendition().is_double_width() {
        right.saturating_add(1) >> 1
    } else {
        right
    }
    .min(readable);
    start.min(end)..end
}

fn screen_column(row: &Row, column: u16) -> u16 {
    if row.line_rendition().is_double_width() {
        column >> 1
    } else {
        column
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::line_edit::{delete_cells, insert_cells, write_text};
    use crate::line_rendition::LineRendition;
    use crate::text_color::Rgb;

    fn rgb_attr(foreground: Rgb, background: Rgb, underline: Rgb) -> TextAttribute {
        let mut attribute = TextAttribute::from_rgb(foreground, background);
        attribute.set_underline_color(TextColor::rgb(underline.r, underline.g, underline.b));
        attribute
    }

    fn fill_buffer(buffer: &mut TextBuffer, character: u8, attribute: TextAttribute) {
        let width = buffer.width();
        let height = buffer.height();
        fill_rect(
            buffer,
            ScreenRect::new(0, 0, width, height),
            u16::from(character),
            attribute,
        )
        .unwrap();
    }

    fn assert_cell(row: &Row, column: u16, character: u8, attribute: TextAttribute) {
        assert_eq!(row.glyph_at(i32::from(column)), &[u16::from(character)]);
        assert_eq!(row.attribute_at(i32::from(column)), attribute);
    }

    fn assert_rect(buffer: &TextBuffer, rect: ScreenRect, character: u8, attribute: TextAttribute) {
        for y in rect.top..rect.bottom {
            let row = buffer.row(i32::from(y));
            for x in rect.left..rect.right {
                assert_cell(row, x, character, attribute);
            }
        }
    }

    #[test]
    fn microsoft_screen_buffer_rectangular_area_operations_contract() {
        let mut buffer_attr = rgb_attr(
            Rgb::new(0, 0, 255),
            Rgb::new(0, 255, 0),
            Rgb::new(255, 0, 0),
        );
        buffer_attr.set_underline_style(UnderlineStyle::Curly);
        buffer_attr.set_intense(true);

        let mut active_attr = rgb_attr(
            Rgb::new(255, 0, 0),
            Rgb::new(0, 0, 255),
            Rgb::new(255, 0, 0),
        );
        active_attr.set_intense(true);

        let target = ScreenRect::new(26, 12, 54, 16);

        // DECFRA
        let mut fill = TextBuffer::new(80, 40, buffer_attr).unwrap();
        fill_buffer(&mut fill, b'Z', buffer_attr);
        fill_rect(&mut fill, target, u16::from(b'*'), active_attr).unwrap();
        assert_rect(&fill, target, b'*', active_attr);
        assert_cell(fill.row(11), 26, b'Z', buffer_attr);
        assert_cell(fill.row(12), 25, b'Z', buffer_attr);
        assert_cell(fill.row(12), 54, b'Z', buffer_attr);
        assert_cell(fill.row(16), 26, b'Z', buffer_attr);

        // DECERA
        let mut erase = TextBuffer::new(80, 40, buffer_attr).unwrap();
        fill_buffer(&mut erase, b'Z', buffer_attr);
        erase_rect(&mut erase, target, active_attr).unwrap();
        let mut expected_erase = active_attr;
        expected_erase.set_standard_erase();
        assert_rect(&erase, target, b' ', expected_erase);

        // DECSERA
        let mut selective = TextBuffer::new(80, 40, buffer_attr).unwrap();
        fill_buffer(&mut selective, b'Z', buffer_attr);
        selective_erase_rect(&mut selective, target).unwrap();
        assert_rect(&selective, target, b' ', buffer_attr);

        // DECCARA
        let mut change = TextBuffer::new(80, 40, buffer_attr).unwrap();
        fill_buffer(&mut change, b'Z', buffer_attr);
        patch_rect_attributes(
            &mut change,
            target,
            AttributePatch {
                reverse_video: Some(true),
                underline_style: Some(UnderlineStyle::Dotted),
                underline_color: Some(TextColor::rgb(55, 23, 28)),
            },
        );
        let mut changed_attr = buffer_attr;
        changed_attr.set_reverse_video(true);
        changed_attr.set_underline_style(UnderlineStyle::Dotted);
        changed_attr.set_underline_color(TextColor::rgb(55, 23, 28));
        assert_rect(&change, target, b'Z', changed_attr);

        // DECRARA
        let mut reverse = TextBuffer::new(80, 40, buffer_attr).unwrap();
        fill_buffer(&mut reverse, b'Z', buffer_attr);
        reverse_rect_intensity(&mut reverse, target);
        let mut reversed_attr = buffer_attr;
        reversed_attr.set_intense(false);
        assert_rect(&reverse, target, b'Z', reversed_attr);

        // DECCRA
        let mut copy = TextBuffer::new(80, 40, buffer_attr).unwrap();
        fill_buffer(&mut copy, b'Z', buffer_attr);
        let copy_attr = TextAttribute::from_rgb(Rgb::new(0, 255, 0), Rgb::new(255, 0, 0));
        let source = ScreenRect::new(26, 20, 54, 24);
        fill_rect(&mut copy, source, u16::from(b'*'), copy_attr).unwrap();
        copy_rect(&mut copy, source, TextBufferPoint::new(26, 12)).unwrap();
        fill_rect(&mut copy, source, u16::from(b'Z'), buffer_attr).unwrap();
        assert_rect(&copy, target, b'*', copy_attr);
        assert_cell(copy.row(12), 25, b'Z', buffer_attr);
        assert_cell(copy.row(12), 54, b'Z', buffer_attr);
    }

    #[test]
    fn microsoft_screen_buffer_copy_double_width_rectangular_area_contract() {
        let mut buffer_attr = TextAttribute::default();
        buffer_attr.set_foreground(TextColor::index16(TextColor::DARK_BLUE));
        buffer_attr.set_background(TextColor::index16(TextColor::DARK_GREEN));
        buffer_attr.set_underline_style(UnderlineStyle::Single);

        let mut copy_attr = TextAttribute::default();
        copy_attr.set_foreground(TextColor::index16(TextColor::DARK_GREEN));
        copy_attr.set_background(TextColor::index16(TextColor::DARK_RED));
        copy_attr.set_intense(true);

        let mut buffer = TextBuffer::new(80, 10, buffer_attr).unwrap();
        fill_buffer(&mut buffer, b'Z', buffer_attr);
        fill_rect(
            &mut buffer,
            ScreenRect::new(0, 0, 80, 3),
            u16::from(b'C'),
            copy_attr,
        )
        .unwrap();
        buffer
            .row_mut(1)
            .set_line_rendition(LineRendition::DoubleWidth);

        copy_rect(
            &mut buffer,
            ScreenRect::new(30, 0, 50, 3),
            TextBufferPoint::new(30, 3),
        )
        .unwrap();

        for x in 0..30 {
            assert_cell(buffer.row(3), x, b'Z', buffer_attr);
            assert_cell(buffer.row(4), x, b'Z', buffer_attr);
            assert_cell(buffer.row(5), x, b'Z', buffer_attr);
        }
        for x in 30..50 {
            assert_cell(buffer.row(3), x, b'C', copy_attr);
            assert_cell(buffer.row(5), x, b'C', copy_attr);
        }
        for x in 30..40 {
            assert_cell(buffer.row(4), x, b'C', copy_attr);
        }
        assert_cell(buffer.row(3), 50, b'Z', buffer_attr);
        assert_cell(buffer.row(4), 40, b'Z', buffer_attr);
        assert_cell(buffer.row(5), 50, b'Z', buffer_attr);
    }

    #[test]
    fn microsoft_screen_buffer_scrolling_wide_chars_horizontally_contract() {
        let attr = TextAttribute::default();
        let mut buffer = TextBuffer::new(40, 1, attr).unwrap();
        let text: Vec<u16> = "こんにちは World".encode_utf16().collect();
        let cell_width = 16_u16;

        write_text(buffer.row_mut(0), 0, &text, attr, false, 0..40).unwrap();
        assert_eq!(
            buffer.row(0).text_range(0, i32::from(cell_width)),
            text.as_slice()
        );

        insert_cells(buffer.row_mut(0), 0, 1, 0..40, attr).unwrap();
        assert_eq!(
            buffer.row(0).text_range(1, i32::from(1 + cell_width)),
            text.as_slice()
        );

        delete_cells(buffer.row_mut(0), 0, 1, 0..40, attr).unwrap();
        assert_eq!(
            buffer.row(0).text_range(0, i32::from(cell_width)),
            text.as_slice()
        );

        copy_rect(
            &mut buffer,
            ScreenRect::new(0, 0, cell_width, 1),
            TextBufferPoint::new(1, 0),
        )
        .unwrap();
        assert_eq!(
            buffer.row(0).text_range(1, i32::from(1 + cell_width)),
            text.as_slice()
        );

        copy_rect(
            &mut buffer,
            ScreenRect::new(1, 0, 1 + cell_width, 1),
            TextBufferPoint::new(0, 0),
        )
        .unwrap();
        assert_eq!(
            buffer.row(0).text_range(0, i32::from(cell_width)),
            text.as_slice()
        );

        scroll_rect(
            &mut buffer,
            ScreenRect::new(0, 0, 39, 1),
            TextBufferPoint::new(1, 0),
            attr,
        )
        .unwrap();
        assert_eq!(
            buffer.row(0).text_range(1, i32::from(1 + cell_width)),
            text.as_slice()
        );

        scroll_rect(
            &mut buffer,
            ScreenRect::new(1, 0, 40, 1),
            TextBufferPoint::new(0, 0),
            attr,
        )
        .unwrap();
        assert_eq!(
            buffer.row(0).text_range(0, i32::from(cell_width)),
            text.as_slice()
        );
    }
}
