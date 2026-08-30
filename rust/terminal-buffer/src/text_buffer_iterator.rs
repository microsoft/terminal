//! Safe row-major iteration over an owned terminal text buffer.
//!
//! Windows Terminal exposes cell and text iterators over the same two-dimensional
//! buffer. This module keeps that shared cursor/bounds behavior explicit while
//! borrowing the existing safe `TextBuffer`, `Row`, and `OutputCellView` owners.

use crate::output_cell::{OutputCellView, TextAttributeBehavior};
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferPoint};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TextBufferIteratorError {
    InvalidPoint,
    InvalidBounds,
    DifferentBuffers,
}

/// Inclusive iterator bounds in logical text-buffer coordinates.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TextBufferIteratorBounds {
    left: u16,
    top: u16,
    right: u16,
    bottom: u16,
}

impl TextBufferIteratorBounds {
    /// Creates inclusive bounds.
    ///
    /// # Errors
    ///
    /// Returns [`TextBufferIteratorError::InvalidBounds`] when either axis is
    /// reversed.
    pub fn inclusive(
        left: u16,
        top: u16,
        right: u16,
        bottom: u16,
    ) -> Result<Self, TextBufferIteratorError> {
        if left > right || top > bottom {
            return Err(TextBufferIteratorError::InvalidBounds);
        }
        Ok(Self {
            left,
            top,
            right,
            bottom,
        })
    }

    #[must_use]
    pub fn full(buffer: &TextBuffer) -> Self {
        Self {
            left: 0,
            top: 0,
            right: buffer.width() - 1,
            bottom: buffer.height() - 1,
        }
    }

    #[must_use]
    pub const fn left(self) -> u16 {
        self.left
    }

    #[must_use]
    pub const fn top(self) -> u16 {
        self.top
    }

    #[must_use]
    pub const fn right(self) -> u16 {
        self.right
    }

    #[must_use]
    pub const fn bottom(self) -> u16 {
        self.bottom
    }

    #[must_use]
    pub const fn width(self) -> u16 {
        self.right - self.left + 1
    }

    #[must_use]
    pub const fn height(self) -> u16 {
        self.bottom - self.top + 1
    }

    #[must_use]
    pub const fn contains(self, point: TextBufferPoint) -> bool {
        point.x >= self.left
            && point.x <= self.right
            && point.y >= self.top
            && point.y <= self.bottom
    }

    #[must_use]
    fn fits_buffer(self, buffer: &TextBuffer) -> bool {
        self.right < buffer.width() && self.bottom < buffer.height()
    }

    #[must_use]
    fn cell_count(self) -> usize {
        usize::from(self.width()) * usize::from(self.height())
    }

    #[must_use]
    fn index_of(self, point: TextBufferPoint) -> usize {
        let row = usize::from(point.y - self.top);
        let column = usize::from(point.x - self.left);
        row * usize::from(self.width()) + column
    }

    #[must_use]
    fn point_at(self, index: usize) -> TextBufferPoint {
        let width = usize::from(self.width());
        let row = index / width;
        let column = index % width;
        TextBufferPoint::new(
            self.left + u16::try_from(column).expect("column remains inside iterator bounds"),
            self.top + u16::try_from(row).expect("row remains inside iterator bounds"),
        )
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CharInfo {
    pub unicode_char: u16,
    pub text_attribute: TextAttribute,
}

/// Read-only full-fidelity iterator over stored cells.
#[derive(Debug, Clone, Copy)]
pub struct TextBufferCellIterator<'a> {
    buffer: &'a TextBuffer,
    position: TextBufferPoint,
    bounds: TextBufferIteratorBounds,
    exceeded: bool,
}

impl<'a> TextBufferCellIterator<'a> {
    /// Creates an iterator bounded by the complete text buffer.
    ///
    /// # Errors
    ///
    /// Returns [`TextBufferIteratorError::InvalidPoint`] when the starting
    /// coordinate is outside the buffer.
    pub fn new(buffer: &'a TextBuffer, x: i32, y: i32) -> Result<Self, TextBufferIteratorError> {
        Self::with_bounds(buffer, x, y, TextBufferIteratorBounds::full(buffer))
    }

    /// Creates an iterator restricted to inclusive logical bounds.
    ///
    /// # Errors
    ///
    /// Returns an error when the bounds extend beyond the underlying buffer or
    /// when the starting coordinate is outside those bounds.
    pub fn with_bounds(
        buffer: &'a TextBuffer,
        x: i32,
        y: i32,
        bounds: TextBufferIteratorBounds,
    ) -> Result<Self, TextBufferIteratorError> {
        if !bounds.fits_buffer(buffer) {
            return Err(TextBufferIteratorError::InvalidBounds);
        }

        let x = u16::try_from(x).map_err(|_| TextBufferIteratorError::InvalidPoint)?;
        let y = u16::try_from(y).map_err(|_| TextBufferIteratorError::InvalidPoint)?;
        let position = TextBufferPoint::new(x, y);
        if !bounds.contains(position) {
            return Err(TextBufferIteratorError::InvalidPoint);
        }

        Ok(Self {
            buffer,
            position,
            bounds,
            exceeded: false,
        })
    }

    #[must_use]
    pub const fn position(self) -> TextBufferPoint {
        self.position
    }

    #[must_use]
    pub const fn bounds(self) -> TextBufferIteratorBounds {
        self.bounds
    }

    #[must_use]
    pub fn is_valid(self) -> bool {
        !self.exceeded && self.bounds.contains(self.position)
    }

    /// Moves by a signed row-major cell distance.
    pub fn advance_by(&mut self, movement: isize) {
        if movement >= 0 {
            self.advance_forward(movement.unsigned_abs());
        } else {
            self.advance_backward(movement.unsigned_abs());
        }
    }

    /// Moves backward by a signed row-major cell distance.
    pub fn retreat_by(&mut self, movement: isize) {
        if movement >= 0 {
            self.advance_backward(movement.unsigned_abs());
        } else {
            self.advance_forward(movement.unsigned_abs());
        }
    }

    pub fn increment(&mut self) {
        self.advance_forward(1);
    }

    pub fn decrement(&mut self) {
        self.advance_backward(1);
    }

    /// Advances this iterator and returns its previous value, matching postfix
    /// iterator semantics.
    #[must_use]
    pub fn post_increment(&mut self) -> Self {
        let previous = *self;
        self.increment();
        previous
    }

    /// Retreats this iterator and returns its previous value, matching postfix
    /// iterator semantics.
    #[must_use]
    pub fn post_decrement(&mut self) -> Self {
        let previous = *self;
        self.decrement();
        previous
    }

    /// Returns an independently moved copy without changing this iterator.
    #[must_use]
    pub fn offset(mut self, movement: isize) -> Self {
        self.advance_by(movement);
        self
    }

    /// Returns the row-major distance from `other` to `self`.
    ///
    /// # Errors
    ///
    /// Returns [`TextBufferIteratorError::DifferentBuffers`] for iterators that
    /// do not address the same underlying text buffer.
    pub fn distance_from(self, other: Self) -> Result<isize, TextBufferIteratorError> {
        if !core::ptr::eq(self.buffer, other.buffer) {
            return Err(TextBufferIteratorError::DifferentBuffers);
        }

        let this = isize::try_from(self.bounds.index_of(self.position))
            .expect("iterator distance fits in isize");
        let that = isize::try_from(self.bounds.index_of(other.position))
            .expect("iterator distance fits in isize");
        Ok(this - that)
    }

    /// Returns the stored cell view at the current iterator position.
    #[must_use]
    pub fn cell(self) -> Option<OutputCellView<'a>> {
        if !self.is_valid() {
            return None;
        }

        let row = self.buffer.row(i32::from(self.position.y));
        Some(OutputCellView::new(
            row.glyph_at(i32::from(self.position.x)),
            row.dbcs_attribute_at(i32::from(self.position.x)),
            row.attribute_at(i32::from(self.position.x)),
            TextAttributeBehavior::Stored,
        ))
    }

    /// Returns the portable `CHAR_INFO` observables owned by the stored cell.
    #[must_use]
    pub fn char_info(self) -> Option<CharInfo> {
        let cell = self.cell()?;
        Some(CharInfo {
            unicode_char: cell.chars().first().copied().unwrap_or_default(),
            text_attribute: cell.text_attribute(),
        })
    }

    fn advance_forward(&mut self, movement: usize) {
        if self.exceeded || movement == 0 {
            return;
        }

        let current = self.bounds.index_of(self.position);
        let Some(target) = current.checked_add(movement) else {
            self.exceeded = true;
            return;
        };
        if target >= self.bounds.cell_count() {
            self.exceeded = true;
            return;
        }

        self.position = self.bounds.point_at(target);
    }

    fn advance_backward(&mut self, movement: usize) {
        if self.exceeded || movement == 0 {
            return;
        }

        let current = self.bounds.index_of(self.position);
        let Some(target) = current.checked_sub(movement) else {
            self.exceeded = true;
            return;
        };
        self.position = self.bounds.point_at(target);
    }
}

impl PartialEq for TextBufferCellIterator<'_> {
    fn eq(&self, other: &Self) -> bool {
        core::ptr::eq(self.buffer, other.buffer)
            && self.position == other.position
            && self.bounds == other.bounds
            && self.exceeded == other.exceeded
    }
}

impl Eq for TextBufferCellIterator<'_> {}

/// Text-only view over the same cell iterator owner.
#[derive(Debug, Clone, Copy)]
pub struct TextBufferTextIterator<'a> {
    cell: TextBufferCellIterator<'a>,
}

impl<'a> TextBufferTextIterator<'a> {
    pub fn new(buffer: &'a TextBuffer, x: i32, y: i32) -> Result<Self, TextBufferIteratorError> {
        Ok(Self {
            cell: TextBufferCellIterator::new(buffer, x, y)?,
        })
    }

    pub fn with_bounds(
        buffer: &'a TextBuffer,
        x: i32,
        y: i32,
        bounds: TextBufferIteratorBounds,
    ) -> Result<Self, TextBufferIteratorError> {
        Ok(Self {
            cell: TextBufferCellIterator::with_bounds(buffer, x, y, bounds)?,
        })
    }

    #[must_use]
    pub const fn position(self) -> TextBufferPoint {
        self.cell.position()
    }

    #[must_use]
    pub const fn bounds(self) -> TextBufferIteratorBounds {
        self.cell.bounds()
    }

    #[must_use]
    pub fn is_valid(self) -> bool {
        self.cell.is_valid()
    }

    pub fn advance_by(&mut self, movement: isize) {
        self.cell.advance_by(movement);
    }

    pub fn retreat_by(&mut self, movement: isize) {
        self.cell.retreat_by(movement);
    }

    pub fn increment(&mut self) {
        self.cell.increment();
    }

    pub fn decrement(&mut self) {
        self.cell.decrement();
    }

    #[must_use]
    pub fn post_increment(&mut self) -> Self {
        let previous = *self;
        self.increment();
        previous
    }

    #[must_use]
    pub fn post_decrement(&mut self) -> Self {
        let previous = *self;
        self.decrement();
        previous
    }

    #[must_use]
    pub fn offset(mut self, movement: isize) -> Self {
        self.advance_by(movement);
        self
    }

    pub fn distance_from(self, other: Self) -> Result<isize, TextBufferIteratorError> {
        self.cell.distance_from(other.cell)
    }

    #[must_use]
    pub fn text(self) -> Option<&'a [u16]> {
        self.cell.cell().map(OutputCellView::chars)
    }
}

impl PartialEq for TextBufferTextIterator<'_> {
    fn eq(&self, other: &Self) -> bool {
        self.cell == other.cell
    }
}

impl Eq for TextBufferTextIterator<'_> {}
