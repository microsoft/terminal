//! Safe row ownership and circular-buffer semantics for Windows Terminal text storage.
//!
//! The C++ `TextBuffer` keeps a fixed set of rows and rotates the logical top
//! through that storage as the viewport advances. This module keeps the same
//! ownership model without pointer arithmetic or shared mutable aliases.

use crate::row::{DbcsAttribute, Row, RowError};
use crate::text_attribute::TextAttribute;

type ReflowGlyph = (Vec<u16>, u16, TextAttribute);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TextBufferError {
    EmptyWidth,
    EmptyHeight,
    HeightTooLarge,
    Row(RowError),
}

impl From<RowError> for TextBufferError {
    fn from(value: RowError) -> Self {
        Self::Row(value)
    }
}

/// A cell coordinate in logical buffer space.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct TextBufferPoint {
    pub y: u16,
    pub x: u16,
}

impl TextBufferPoint {
    #[must_use]
    pub const fn new(x: u16, y: u16) -> Self {
        Self { y, x }
    }
}

/// An inclusive selection in logical buffer space.
///
/// The endpoints are normalized by [`TextBuffer::selection_text`] so callers
/// may provide them in either order. Selection boundaries that land on the
/// trailing half of a wide glyph are expanded to include the complete glyph.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TextSelection {
    pub anchor: TextBufferPoint,
    pub end: TextBufferPoint,
}

impl TextSelection {
    #[must_use]
    pub const fn new(anchor: TextBufferPoint, end: TextBufferPoint) -> Self {
        Self { anchor, end }
    }

    #[must_use]
    pub fn normalized(self) -> Self {
        if self.anchor <= self.end {
            self
        } else {
            Self {
                anchor: self.end,
                end: self.anchor,
            }
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TextBuffer {
    rows: Vec<Row>,
    width: u16,
    height: u16,
    first_row: u16,
}

impl TextBuffer {
    /// Creates a fixed-size circular row store.
    ///
    /// # Errors
    ///
    /// Returns an error for zero dimensions, a height that cannot be represented
    /// by the buffer's `u16` row coordinates, or an invalid row width.
    pub fn new(
        width: u16,
        height: u16,
        fill_attribute: TextAttribute,
    ) -> Result<Self, TextBufferError> {
        if width == 0 {
            return Err(TextBufferError::EmptyWidth);
        }
        if height == 0 {
            return Err(TextBufferError::EmptyHeight);
        }

        let mut rows = Vec::with_capacity(usize::from(height));
        for _ in 0..height {
            rows.push(Row::new(width, fill_attribute)?);
        }

        Ok(Self {
            rows,
            width,
            height,
            first_row: 0,
        })
    }

    #[must_use]
    pub const fn width(&self) -> u16 {
        self.width
    }

    #[must_use]
    pub const fn height(&self) -> u16 {
        self.height
    }

    #[must_use]
    pub const fn first_row_index(&self) -> u16 {
        self.first_row
    }

    #[must_use]
    pub fn row(&self, logical_y: i32) -> &Row {
        &self.rows[self.physical_index(logical_y)]
    }

    #[must_use]
    pub fn row_mut(&mut self, logical_y: i32) -> &mut Row {
        let index = self.physical_index(logical_y);
        &mut self.rows[index]
    }

    /// Returns rows in logical top-to-bottom order regardless of circular
    /// storage rotation.
    pub fn logical_rows(&self) -> impl Iterator<Item = &Row> {
        (0..self.height).map(|logical_y| self.row(i32::from(logical_y)))
    }

    /// Extracts UTF-16 text for an inclusive rectangularly linear selection.
    ///
    /// Rows between the endpoints are joined with `line_separator`. The first
    /// and last row are clipped to the selected cells, while intermediate rows
    /// contribute their complete readable text. Wide-glyph boundaries are
    /// repaired so a selection can never return half of a glyph.
    #[must_use]
    pub fn selection_text(&self, selection: TextSelection, line_separator: &[u16]) -> Vec<u16> {
        let selection = selection.normalized();
        let start = self.clamp_point(selection.anchor);
        let end = self.clamp_point(selection.end);
        let mut output = Vec::new();

        for y in start.y..=end.y {
            if y != start.y {
                output.extend_from_slice(line_separator);
            }

            let row = self.row(i32::from(y));
            let readable = row.readable_column_count();
            if readable == 0 {
                continue;
            }

            let begin = if y == start.y {
                row.adjust_to_glyph_start(i32::from(start.x.min(readable - 1)))
            } else {
                0
            };
            let end_exclusive = if y == end.y {
                row.adjust_to_glyph_end(i32::from(end.x.min(readable - 1).saturating_add(1)))
            } else {
                readable
            };

            output.extend_from_slice(row.text_range(i32::from(begin), i32::from(end_exclusive)));
        }

        output
    }

    /// Rotates the logical top upward by `count` rows and resets the rows that
    /// become newly visible at the logical bottom.
    pub fn rotate_up(&mut self, count: u16, fill_attribute: TextAttribute) {
        let count = count.min(self.height);
        for _ in 0..count {
            self.first_row = (self.first_row + 1) % self.height;
            let bottom = self.physical_index(i32::from(self.height) - 1);
            self.rows[bottom].reset(fill_attribute);
        }
    }

    /// Rotates the logical top downward by `count` rows and resets the rows that
    /// become newly visible at the logical top.
    pub fn rotate_down(&mut self, count: u16, fill_attribute: TextAttribute) {
        let count = count.min(self.height);
        for _ in 0..count {
            self.first_row = if self.first_row == 0 {
                self.height - 1
            } else {
                self.first_row - 1
            };
            let top = self.physical_index(0);
            self.rows[top].reset(fill_attribute);
        }
    }

    pub fn reset(&mut self, fill_attribute: TextAttribute) {
        self.first_row = 0;
        for row in &mut self.rows {
            row.reset(fill_attribute);
        }
    }

    /// Changes only the row count while preserving the oldest logical rows.
    ///
    /// # Errors
    ///
    /// Returns an error for zero height or row allocation failure.
    pub fn resize_height(
        &mut self,
        new_height: u16,
        fill_attribute: TextAttribute,
    ) -> Result<(), TextBufferError> {
        if new_height == 0 {
            return Err(TextBufferError::EmptyHeight);
        }
        if new_height == self.height {
            return Ok(());
        }

        let preserve = self.height.min(new_height);
        let mut rows = Vec::with_capacity(usize::from(new_height));
        for logical_y in 0..preserve {
            rows.push(self.row(i32::from(logical_y)).clone());
        }
        for _ in preserve..new_height {
            rows.push(Row::new(self.width, fill_attribute)?);
        }

        self.rows = rows;
        self.height = new_height;
        self.first_row = 0;
        Ok(())
    }

    /// Changes the width and reflows forced-wrap row chains into the new width.
    ///
    /// Unwrapped rows keep their logical line boundary. Forced-wrap rows are
    /// concatenated with the following row before being wrapped again. Glyph
    /// storage, wide-cell boundaries, and per-cell attributes are preserved.
    /// The fixed buffer height is retained; excess reflow rows are clipped at
    /// the logical bottom and newly unused rows are initialized with
    /// `fill_attribute`.
    ///
    /// # Errors
    ///
    /// Returns an error for zero width or if a reconstructed row cannot satisfy
    /// the validated `Row` storage invariants.
    pub fn resize_width_reflow(
        &mut self,
        new_width: u16,
        fill_attribute: TextAttribute,
    ) -> Result<(), TextBufferError> {
        if new_width == 0 {
            return Err(TextBufferError::EmptyWidth);
        }
        if new_width == self.width {
            return Ok(());
        }

        let mut logical_lines: Vec<Vec<ReflowGlyph>> = Vec::new();
        let mut line = Vec::new();

        for row in self.logical_rows() {
            let column_limit = if row.was_wrap_forced() {
                row.readable_column_count()
            } else {
                row.measure_right()
            };
            let mut column = 0;

            while column < column_limit {
                match row.dbcs_attribute_at(i32::from(column)) {
                    DbcsAttribute::Trailing => {
                        column = column.saturating_add(1);
                    }
                    DbcsAttribute::Single | DbcsAttribute::Leading => {
                        let glyph_width = if matches!(
                            row.dbcs_attribute_at(i32::from(column)),
                            DbcsAttribute::Leading
                        ) {
                            2
                        } else {
                            1
                        };
                        line.push((
                            row.glyph_at(i32::from(column)).to_vec(),
                            glyph_width,
                            row.attribute_at(i32::from(column)),
                        ));
                        column = column.saturating_add(glyph_width);
                    }
                }
            }

            if !row.was_wrap_forced() {
                logical_lines.push(core::mem::take(&mut line));
            }
        }

        if !line.is_empty() {
            logical_lines.push(line);
        }

        let mut rows = Vec::with_capacity(usize::from(self.height));
        for logical_line in logical_lines {
            if rows.len() >= usize::from(self.height) {
                break;
            }

            if logical_line.is_empty() {
                rows.push(Row::new(new_width, fill_attribute)?);
                continue;
            }

            let mut row = Row::new(new_width, fill_attribute)?;
            let mut column = 0_u16;

            for (glyph, original_width, attribute) in logical_line {
                let glyph_width = original_width.min(new_width);
                if column != 0 && column.saturating_add(glyph_width) > new_width {
                    row.set_wrap_forced(true);
                    rows.push(row.clone());
                    if rows.len() >= usize::from(self.height) {
                        break;
                    }
                    row = Row::new(new_width, fill_attribute)?;
                    column = 0;
                }

                row.replace_glyph(i32::from(column), glyph_width, &glyph)?;
                row.replace_attributes(
                    i32::from(column),
                    i32::from(column.saturating_add(glyph_width)),
                    attribute,
                );
                column = column.saturating_add(glyph_width);
            }

            if rows.len() < usize::from(self.height) {
                row.set_wrap_forced(false);
                rows.push(row);
            }
        }

        while rows.len() < usize::from(self.height) {
            rows.push(Row::new(new_width, fill_attribute)?);
        }

        self.rows = rows;
        self.width = new_width;
        self.first_row = 0;
        Ok(())
    }

    #[must_use]
    fn clamp_point(&self, point: TextBufferPoint) -> TextBufferPoint {
        TextBufferPoint {
            x: point.x.min(self.width - 1),
            y: point.y.min(self.height - 1),
        }
    }

    #[must_use]
    fn physical_index(&self, logical_y: i32) -> usize {
        let logical_y = logical_y.clamp(0, i32::from(self.height) - 1);
        let logical_y = u16::try_from(logical_y).unwrap_or_default();
        usize::from((self.first_row + logical_y) % self.height)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn attribute() -> TextAttribute {
        TextAttribute::default()
    }

    #[test]
    fn creates_fixed_owned_rows() {
        let buffer = TextBuffer::new(8, 3, attribute()).unwrap();
        assert_eq!(buffer.width(), 8);
        assert_eq!(buffer.height(), 3);
        assert_eq!(buffer.first_row_index(), 0);
        assert_eq!(buffer.row(0).size(), 8);
        assert_eq!(buffer.row(99).size(), 8);
    }

    #[test]
    fn logical_rows_follow_rotation() {
        let mut buffer = TextBuffer::new(4, 3, attribute()).unwrap();
        for (row, glyph) in [(0, b'A'), (1, b'B'), (2, b'C')] {
            buffer
                .row_mut(row)
                .replace_glyph(0, 1, &[u16::from(glyph)])
                .unwrap();
        }
        buffer.rotate_up(1, attribute());

        let first_glyphs: Vec<u16> = buffer
            .logical_rows()
            .map(|row| row.glyph_at(0)[0])
            .collect();
        assert_eq!(
            first_glyphs,
            [u16::from(b'B'), u16::from(b'C'), u16::from(b' ')]
        );
    }

    #[test]
    fn selection_normalizes_reverse_endpoints_and_joins_rows() {
        let mut buffer = TextBuffer::new(4, 2, attribute()).unwrap();
        for (x, glyph) in [(0, b'A'), (1, b'B'), (2, b'C'), (3, b'D')] {
            buffer
                .row_mut(0)
                .replace_glyph(x, 1, &[u16::from(glyph)])
                .unwrap();
        }
        for (x, glyph) in [(0, b'E'), (1, b'F'), (2, b'G'), (3, b'H')] {
            buffer
                .row_mut(1)
                .replace_glyph(x, 1, &[u16::from(glyph)])
                .unwrap();
        }

        let text = buffer.selection_text(
            TextSelection::new(TextBufferPoint::new(2, 1), TextBufferPoint::new(1, 0)),
            &[u16::from(b'\r'), u16::from(b'\n')],
        );
        assert_eq!(
            text,
            [
                u16::from(b'B'),
                u16::from(b'C'),
                u16::from(b'D'),
                u16::from(b'\r'),
                u16::from(b'\n'),
                u16::from(b'E'),
                u16::from(b'F'),
                u16::from(b'G')
            ]
        );
    }

    #[test]
    fn selection_expands_trailing_half_of_wide_glyph() {
        let mut buffer = TextBuffer::new(5, 1, attribute()).unwrap();
        buffer
            .row_mut(0)
            .replace_glyph(1, 2, &[0x4e00])
            .expect("wide glyph fits");

        let text = buffer.selection_text(
            TextSelection::new(TextBufferPoint::new(2, 0), TextBufferPoint::new(2, 0)),
            &[],
        );
        assert_eq!(text, [0x4e00]);
    }

    #[test]
    fn rotate_up_reuses_storage_and_clears_new_bottom() {
        let mut buffer = TextBuffer::new(4, 3, attribute()).unwrap();
        buffer
            .row_mut(0)
            .replace_glyph(0, 1, &[u16::from(b'A')])
            .unwrap();
        buffer
            .row_mut(1)
            .replace_glyph(0, 1, &[u16::from(b'B')])
            .unwrap();
        buffer
            .row_mut(2)
            .replace_glyph(0, 1, &[u16::from(b'C')])
            .unwrap();

        buffer.rotate_up(1, attribute());

        assert_eq!(buffer.first_row_index(), 1);
        assert_eq!(buffer.row(0).glyph_at(0), &[u16::from(b'B')]);
        assert_eq!(buffer.row(1).glyph_at(0), &[u16::from(b'C')]);
        assert_eq!(buffer.row(2).glyph_at(0), &[u16::from(b' ')]);
    }

    #[test]
    fn rotate_down_reuses_storage_and_clears_new_top() {
        let mut buffer = TextBuffer::new(4, 3, attribute()).unwrap();
        buffer
            .row_mut(0)
            .replace_glyph(0, 1, &[u16::from(b'A')])
            .unwrap();
        buffer
            .row_mut(1)
            .replace_glyph(0, 1, &[u16::from(b'B')])
            .unwrap();

        buffer.rotate_down(1, attribute());

        assert_eq!(buffer.first_row_index(), 2);
        assert_eq!(buffer.row(0).glyph_at(0), &[u16::from(b' ')]);
        assert_eq!(buffer.row(1).glyph_at(0), &[u16::from(b'A')]);
        assert_eq!(buffer.row(2).glyph_at(0), &[u16::from(b'B')]);
    }

    #[test]
    fn resize_height_preserves_logical_order_across_rotation() {
        let mut buffer = TextBuffer::new(4, 3, attribute()).unwrap();
        buffer
            .row_mut(0)
            .replace_glyph(0, 1, &[u16::from(b'A')])
            .unwrap();
        buffer
            .row_mut(1)
            .replace_glyph(0, 1, &[u16::from(b'B')])
            .unwrap();
        buffer
            .row_mut(2)
            .replace_glyph(0, 1, &[u16::from(b'C')])
            .unwrap();
        buffer.rotate_up(1, attribute());

        buffer.resize_height(4, attribute()).unwrap();

        assert_eq!(buffer.first_row_index(), 0);
        assert_eq!(buffer.row(0).glyph_at(0), &[u16::from(b'B')]);
        assert_eq!(buffer.row(1).glyph_at(0), &[u16::from(b'C')]);
        assert_eq!(buffer.row(2).glyph_at(0), &[u16::from(b' ')]);
        assert_eq!(buffer.row(3).glyph_at(0), &[u16::from(b' ')]);
    }

    #[test]
    fn resize_width_reflows_forced_wrap_chain() {
        let mut buffer = TextBuffer::new(4, 4, attribute()).unwrap();
        for (x, glyph) in [(0, b'A'), (1, b'B'), (2, b'C'), (3, b'D')] {
            buffer
                .row_mut(0)
                .replace_glyph(x, 1, &[u16::from(glyph)])
                .unwrap();
        }
        buffer.row_mut(0).set_wrap_forced(true);
        for (x, glyph) in [(0, b'E'), (1, b'F')] {
            buffer
                .row_mut(1)
                .replace_glyph(x, 1, &[u16::from(glyph)])
                .unwrap();
        }

        buffer.resize_width_reflow(3, attribute()).unwrap();

        assert_eq!(buffer.width(), 3);
        assert_eq!(
            buffer.row(0).text_range(0, 3),
            &[u16::from(b'A'), u16::from(b'B'), u16::from(b'C')]
        );
        assert!(buffer.row(0).was_wrap_forced());
        assert_eq!(
            buffer.row(1).text_range(0, 3),
            &[u16::from(b'D'), u16::from(b'E'), u16::from(b'F')]
        );
        assert!(!buffer.row(1).was_wrap_forced());
    }

    #[test]
    fn resize_width_preserves_wide_glyph_boundary() {
        let mut buffer = TextBuffer::new(5, 3, attribute()).unwrap();
        buffer
            .row_mut(0)
            .replace_glyph(0, 1, &[u16::from(b'A')])
            .unwrap();
        buffer.row_mut(0).replace_glyph(1, 2, &[0x4e00]).unwrap();
        buffer
            .row_mut(0)
            .replace_glyph(3, 1, &[u16::from(b'B')])
            .unwrap();

        buffer.resize_width_reflow(3, attribute()).unwrap();

        assert_eq!(buffer.row(0).glyph_at(0), &[u16::from(b'A')]);
        assert_eq!(buffer.row(0).glyph_at(1), &[0x4e00]);
        assert_eq!(buffer.row(0).dbcs_attribute_at(1), DbcsAttribute::Leading);
        assert_eq!(buffer.row(0).dbcs_attribute_at(2), DbcsAttribute::Trailing);
        assert!(buffer.row(0).was_wrap_forced());
        assert_eq!(buffer.row(1).glyph_at(0), &[u16::from(b'B')]);
    }
}
