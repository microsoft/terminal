//! Safe screen-buffer resize integrity for traditional and reflow paths.
//!
//! The native Host has two resize strategies, but cursor presentation must not
//! depend on which strategy is selected. Traditional resize also used to expose
//! attribute-row lifetime bugs in C++; this owner rebuilds row storage through
//! safe `TextBuffer` ownership so shrinking cannot retain aliased row storage.

use crate::alternate_buffer::CursorState;
use crate::reflow::resize_with_reflow;
use crate::row::{DbcsAttribute, Row};
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferError};

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ResizeIntegrityState {
    buffer: TextBuffer,
    cursor: CursorState,
    fill_attribute: TextAttribute,
}

impl ResizeIntegrityState {
    #[must_use]
    pub const fn new(
        buffer: TextBuffer,
        cursor: CursorState,
        fill_attribute: TextAttribute,
    ) -> Self {
        Self {
            buffer,
            cursor,
            fill_attribute,
        }
    }

    #[must_use]
    pub const fn buffer(&self) -> &TextBuffer {
        &self.buffer
    }

    #[must_use]
    pub const fn cursor(&self) -> CursorState {
        self.cursor
    }

    /// Resizes the screen buffer without changing cursor presentation.
    ///
    /// `use_reflow=false` follows traditional row-by-row resize semantics;
    /// `use_reflow=true` delegates to the existing screen-buffer reflow owner.
    /// In both cases cursor shape, size, visibility and blinking state are
    /// presentation state and therefore survive the geometry change unchanged.
    ///
    /// # Errors
    ///
    /// Returns any validated storage error reported by the selected resize path.
    pub fn resize_screen_buffer(
        &mut self,
        new_width: u16,
        new_height: u16,
        use_reflow: bool,
    ) -> Result<(), TextBufferError> {
        let preserved_cursor = self.cursor;

        if use_reflow {
            resize_with_reflow(&mut self.buffer, new_width, new_height, self.fill_attribute)?;
        } else {
            resize_traditional(&mut self.buffer, new_width, new_height, self.fill_attribute)?;
        }

        self.cursor = preserved_cursor;
        Ok(())
    }
}

/// Resizes rows independently, without joining forced-wrap chains.
///
/// The operation is transactional: a new owned `TextBuffer` is built first and
/// replaces the original only after all retained glyphs and attributes have
/// been copied successfully. When width grows, newly exposed cells are spaces
/// carrying the source row's final attribute, matching the native traditional
/// resize contract. Newly created rows use `fill_attribute`.
///
/// # Errors
///
/// Returns a validated text-buffer or row-storage error for invalid dimensions
/// or an unrepresentable retained glyph.
pub fn resize_traditional(
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

    if new_width == buffer.width() {
        return buffer.resize_height(new_height, fill_attribute);
    }

    let source = buffer.clone();
    let mut resized = TextBuffer::new(new_width, new_height, fill_attribute)?;
    let rows_to_copy = source.height().min(new_height);

    for y in 0..rows_to_copy {
        copy_row_traditionally(source.row(i32::from(y)), resized.row_mut(i32::from(y)))?;
    }

    *buffer = resized;
    Ok(())
}

fn copy_row_traditionally(source: &Row, target: &mut Row) -> Result<(), TextBufferError> {
    target.set_line_rendition(source.line_rendition());
    target.set_wrap_forced(source.was_wrap_forced());
    target.set_double_byte_padded(source.was_double_byte_padded());

    let copy_width = source.size().min(target.size());
    let mut column = 0_u16;

    while column < copy_width {
        match source.dbcs_attribute_at(i32::from(column)) {
            DbcsAttribute::Trailing => {
                column = column.saturating_add(1);
            }
            DbcsAttribute::Single => {
                target.replace_glyph(i32::from(column), 1, source.glyph_at(i32::from(column)))?;
                target.replace_attributes(
                    i32::from(column),
                    i32::from(column + 1),
                    source.attribute_at(i32::from(column)),
                );
                column = column.saturating_add(1);
            }
            DbcsAttribute::Leading => {
                if column.saturating_add(1) >= copy_width {
                    break;
                }
                target.replace_glyph(i32::from(column), 2, source.glyph_at(i32::from(column)))?;
                target.replace_attributes(
                    i32::from(column),
                    i32::from(column + 1),
                    source.attribute_at(i32::from(column)),
                );
                target.replace_attributes(
                    i32::from(column + 1),
                    i32::from(column + 2),
                    source.attribute_at(i32::from(column + 1)),
                );
                column = column.saturating_add(2);
            }
        }
    }

    if target.size() > source.size() {
        let extension = source.attribute_at(i32::from(source.size().saturating_sub(1)));
        target.replace_attributes(
            i32::from(source.size()),
            i32::from(target.size()),
            extension,
        );
    }

    Ok(())
}
