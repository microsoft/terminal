//! Portable host coordination for VT screen resizing.
//!
//! The native Host routes both CSI 8 t and DECCOLM through screen-buffer
//! resize machinery. This owner keeps the deterministic observables in safe
//! Rust: physical buffer height, viewport dimensions, cursor/margin resets and
//! the current text attribute survive the same resize transitions.

use crate::alternate_buffer::ViewportSize;
use crate::reflow::resize_with_reflow;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferError, TextBufferPoint};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct VerticalMargins {
    pub top: u16,
    pub bottom: u16,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VtResizeState {
    buffer: TextBuffer,
    viewport: ViewportSize,
    cursor: TextBufferPoint,
    margins: Option<VerticalMargins>,
    current_attribute: TextAttribute,
    allow_deccolm: bool,
}

impl VtResizeState {
    #[must_use]
    pub fn new(
        buffer: TextBuffer,
        viewport: ViewportSize,
        current_attribute: TextAttribute,
    ) -> Self {
        assert!(viewport.width > 0 && viewport.height > 0);
        assert!(viewport.width <= buffer.width());
        assert!(viewport.height <= buffer.height());

        Self {
            buffer,
            viewport,
            cursor: TextBufferPoint::new(0, 0),
            margins: None,
            current_attribute,
            allow_deccolm: false,
        }
    }

    #[must_use]
    pub const fn buffer(&self) -> &TextBuffer {
        &self.buffer
    }

    #[must_use]
    pub const fn viewport(&self) -> ViewportSize {
        self.viewport
    }

    #[must_use]
    pub const fn cursor(&self) -> TextBufferPoint {
        self.cursor
    }

    #[must_use]
    pub const fn margins(&self) -> Option<VerticalMargins> {
        self.margins
    }

    #[must_use]
    pub const fn current_attribute(&self) -> TextAttribute {
        self.current_attribute
    }

    pub fn set_current_attribute(&mut self, attribute: TextAttribute) {
        self.current_attribute = attribute;
    }

    pub fn set_cursor_relative(&mut self, x: u16, y: u16) {
        self.cursor = TextBufferPoint::new(
            x.min(self.viewport.width - 1),
            y.min(self.viewport.height - 1),
        );
    }

    pub fn set_vertical_margins(&mut self, top: u16, bottom: u16) {
        self.margins = (top < bottom && bottom < self.viewport.height)
            .then_some(VerticalMargins { top, bottom });
    }

    pub fn set_allow_deccolm(&mut self, enabled: bool) {
        self.allow_deccolm = enabled;
    }

    #[must_use]
    pub const fn allow_deccolm(&self) -> bool {
        self.allow_deccolm
    }

    /// Applies the Host observable for CSI 8 ; rows ; columns t.
    ///
    /// Zero dimensions leave the state untouched. Otherwise, width is applied
    /// to the physical buffer while its height is retained; the viewport adopts
    /// the requested dimensions. Cursor and margins are kept when they still fit.
    ///
    /// # Errors
    ///
    /// Returns any error produced by the safe reflow owner while resizing the
    /// physical buffer width.
    pub fn resize_window(&mut self, rows: u16, columns: u16) -> Result<bool, TextBufferError> {
        if rows == 0 || columns == 0 {
            return Ok(false);
        }

        let buffer_height = self.buffer.height();
        resize_with_reflow(
            &mut self.buffer,
            columns,
            buffer_height,
            self.current_attribute,
        )?;

        self.viewport = ViewportSize::new(columns, rows.min(buffer_height));
        self.cursor.x = self.cursor.x.min(columns - 1);
        self.cursor.y = self.cursor.y.min(self.viewport.height - 1);

        if self
            .margins
            .is_some_and(|margins| margins.bottom >= self.viewport.height)
        {
            self.margins = None;
        }

        Ok(true)
    }

    /// Applies DEC private column mode after DECSET 40 enables it.
    ///
    /// DECCOLM selects 132 or 80 columns, preserves physical/view height and
    /// current attributes, and resets both scroll margins and relative cursor.
    ///
    /// # Errors
    ///
    /// Returns any error produced by the safe reflow owner while resizing the
    /// physical buffer width.
    pub fn set_deccolm(&mut self, columns_132: bool) -> Result<bool, TextBufferError> {
        if !self.allow_deccolm {
            return Ok(false);
        }

        let columns = if columns_132 { 132 } else { 80 };
        let buffer_height = self.buffer.height();
        resize_with_reflow(
            &mut self.buffer,
            columns,
            buffer_height,
            self.current_attribute,
        )?;

        self.viewport.width = columns;
        self.cursor = TextBufferPoint::new(0, 0);
        self.margins = None;
        Ok(true)
    }
}
