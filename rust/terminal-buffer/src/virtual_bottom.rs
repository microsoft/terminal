//! Safe viewport/cursor/virtual-bottom semantics derived from Host `ScreenBuffer` tests.
//!
//! The core owner models the portable state transitions shared by cursor movement,
//! viewport sizing, offscreen line feeds, cursor visibility and returning to the
//! virtual viewport. Reflow coordination composes that state with the existing
//! safe `TextBuffer` reflow owner so the screen-buffer virtual-bottom invariants
//! remain explicit rather than being hidden in test metadata.

use crate::reflow::resize_with_reflow as resize_text_buffer_with_reflow;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferError};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ViewportState {
    pub left: u16,
    pub top: u16,
    pub width: u16,
    pub height: u16,
}

impl ViewportState {
    #[must_use]
    pub const fn bottom(self) -> u16 {
        self.top.saturating_add(self.height.saturating_sub(1))
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CursorPosition {
    pub x: u16,
    pub y: u16,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VirtualBottomState {
    viewport: ViewportState,
    virtual_bottom: u16,
    cursor: CursorPosition,
}

impl VirtualBottomState {
    #[must_use]
    pub fn new(width: u16, height: u16) -> Self {
        let viewport = ViewportState {
            left: 0,
            top: 0,
            width,
            height,
        };
        Self {
            viewport,
            virtual_bottom: viewport.bottom(),
            cursor: CursorPosition { x: 0, y: 0 },
        }
    }

    #[must_use]
    pub const fn viewport(&self) -> ViewportState {
        self.viewport
    }

    #[must_use]
    pub const fn virtual_bottom(&self) -> u16 {
        self.virtual_bottom
    }

    #[must_use]
    pub const fn cursor(&self) -> CursorPosition {
        self.cursor
    }

    #[must_use]
    pub fn virtual_viewport(&self) -> ViewportState {
        ViewportState {
            left: self.viewport.left,
            top: self
                .virtual_bottom
                .saturating_sub(self.viewport.height.saturating_sub(1)),
            width: self.viewport.width,
            height: self.viewport.height,
        }
    }

    pub fn set_viewport_origin(&mut self, left: u16, top: u16, update_virtual_bottom: bool) {
        self.viewport.left = left;
        self.viewport.top = top;
        if update_virtual_bottom {
            self.virtual_bottom = self.viewport.bottom();
        }
    }

    pub fn set_cursor_direct(&mut self, x: u16, y: u16) {
        self.cursor = CursorPosition { x, y };
    }

    /// Output-driven cursor movement grows virtual bottom when content advances
    /// below it, but never forces a manually scrolled viewport to follow.
    pub fn advance_output_lines(&mut self, lines: u16) {
        self.cursor.y = self.cursor.y.saturating_add(lines);
        if self.cursor.y > self.virtual_bottom {
            self.virtual_bottom = self.cursor.y;
        }
    }

    /// Console API cursor positioning makes the cursor visible inside the virtual
    /// viewport. Moving above that range may move virtual bottom upward; moving
    /// within it preserves virtual bottom.
    pub fn set_console_cursor_position(&mut self, x: u16, y: u16) {
        self.cursor = CursorPosition { x, y };
        let virtual_viewport = self.virtual_viewport();
        if y < virtual_viewport.top {
            self.viewport.top = y;
            self.virtual_bottom = self.viewport.bottom();
        } else if y > self.virtual_bottom {
            self.viewport.top = y.saturating_sub(self.viewport.height.saturating_sub(1));
            self.virtual_bottom = y;
        } else {
            self.viewport.top = virtual_viewport.top;
        }
    }

    /// Internal viewport resizing preserves virtual bottom unless the resized
    /// viewport crosses through it, in which case the bottom realigns.
    pub fn internal_set_viewport_height(&mut self, height: u16) {
        let old_bottom = self.viewport.bottom();
        self.viewport.height = height;
        let new_bottom = self.viewport.bottom();
        let crossed = (old_bottom < self.virtual_bottom && new_bottom >= self.virtual_bottom)
            || (old_bottom > self.virtual_bottom && new_bottom <= self.virtual_bottom);
        if crossed {
            self.virtual_bottom = new_bottom;
        }
    }

    /// Window-resize VT changes viewport dimensions without rebasing virtual bottom.
    pub fn resize_window(&mut self, width: u16, height: u16) {
        self.viewport.width = width;
        self.viewport.height = height;
    }

    /// Screen-buffer reflow composes the existing safe text reflow with the
    /// virtual-bottom contract from `ScreenBufferTests.cpp`.
    ///
    /// Reflow may create additional physical rows. The virtual viewport must
    /// include the final non-space row, but a resize performed while the viewport
    /// is already at the top must never shrink virtual bottom above the viewport
    /// bottom. Cursor distance from virtual bottom is preserved when possible so
    /// a shrink/grow round trip keeps the cleared-screen cursor anchored to the
    /// same virtual viewport row.
    ///
    /// # Errors
    ///
    /// Propagates invalid-dimension or row-storage errors from the safe reflow owner.
    pub fn resize_with_reflow(
        &mut self,
        buffer: &mut TextBuffer,
        new_width: u16,
        fill_attribute: TextAttribute,
    ) -> Result<(), TextBufferError> {
        let cursor_distance_from_bottom = self.virtual_bottom.saturating_sub(self.cursor.y);

        resize_text_buffer_with_reflow(buffer, new_width, buffer.height(), fill_attribute)?;
        self.viewport.width = new_width;

        let last_non_space_row = buffer
            .logical_rows()
            .enumerate()
            .filter(|&(_row, content)| content.measure_right() != 0)
            .map(|(row, _content)| {
                u16::try_from(row).expect("TextBuffer row index always fits u16")
            })
            .last();

        let minimum_bottom = last_non_space_row.map_or(self.viewport.bottom(), |row| {
            row.max(self.viewport.bottom())
        });
        self.virtual_bottom = self.virtual_bottom.max(minimum_bottom);

        if self.cursor.y <= self.virtual_bottom {
            self.cursor.y = self
                .virtual_bottom
                .saturating_sub(cursor_distance_from_bottom);
        }

        Ok(())
    }

    /// A line feed issued while the cursor is outside the visible viewport must
    /// not perturb the virtual bottom unless the cursor actually crosses it.
    pub fn offscreen_linefeed(&mut self) {
        self.cursor.y = self.cursor.y.saturating_add(1);
        if self.cursor.y > self.virtual_bottom {
            self.virtual_bottom = self.cursor.y;
        }
    }

    /// Scrolls only enough to make the cursor visible while preserving virtual bottom.
    pub fn make_cursor_visible(&mut self) {
        if self.cursor.y < self.viewport.top {
            self.viewport.top = self.cursor.y;
        } else if self.cursor.y > self.viewport.bottom() {
            self.viewport.top = self
                .cursor
                .y
                .saturating_sub(self.viewport.height.saturating_sub(1));
        }
    }

    /// Returns to the virtual viewport while retaining horizontal scroll offset.
    pub fn move_to_virtual_bottom(&mut self) {
        self.viewport.top = self.virtual_viewport().top;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn write_ascii_row(buffer: &mut TextBuffer, row: u16, width: u16) {
        let row = buffer.row_mut(i32::from(row));
        for column in 0..width {
            row.replace_glyph(i32::from(column), 1, &[u16::from(b'X')])
                .expect("fixture glyph fits");
        }
    }

    #[test]
    fn microsoft_virtual_bottom_reflow_contract() {
        let fill = TextAttribute::default();
        let mut buffer = TextBuffer::new(10, 20, fill).expect("fixture dimensions are valid");
        let mut state = VirtualBottomState::new(10, 5);

        // Microsoft emits two viewport-pages of almost-full logical lines.
        for row in 0..10_u16 {
            write_ascii_row(&mut buffer, row, 9);
        }
        state.set_cursor_direct(0, 4);
        state.advance_output_lines(5);
        state.set_viewport_origin(0, 0, false);

        state
            .resize_with_reflow(&mut buffer, 5, fill)
            .expect("Microsoft shrink-with-reflow succeeds");

        let last_non_space_row = buffer
            .logical_rows()
            .enumerate()
            .filter_map(|(row, content)| {
                (content.measure_right() != 0)
                    .then(|| u16::try_from(row).expect("fixture row index fits u16"))
            })
            .last()
            .expect("fixture retains printable content");
        assert!(state.virtual_bottom() >= last_non_space_row);

        // Microsoft then clears the virtual viewport, homes the cursor there,
        // and requires a grow reflow to retain cursor distance from its bottom.
        buffer.reset(fill);
        let virtual_top = state.virtual_viewport().top;
        state.set_cursor_direct(0, virtual_top);
        let distance = state.virtual_bottom() - state.cursor().y;
        assert_eq!(distance, state.viewport().height - 1);

        state
            .resize_with_reflow(&mut buffer, 10, fill)
            .expect("Microsoft grow-with-reflow succeeds");
        assert_eq!(distance, state.virtual_bottom() - state.cursor().y);
    }

    #[test]
    fn microsoft_virtual_bottom_reflow_at_top_does_not_shrink_contract() {
        let fill = TextAttribute::default();
        let mut buffer = TextBuffer::new(10, 20, fill).expect("fixture dimensions are valid");
        let mut state = VirtualBottomState::new(10, 5);

        state.set_viewport_origin(0, 0, true);
        state.set_cursor_direct(0, 0);
        let initial_bottom = state.virtual_bottom();
        assert_eq!(initial_bottom, state.viewport().bottom());

        state
            .resize_with_reflow(&mut buffer, 5, fill)
            .expect("Microsoft top-of-buffer shrink succeeds");

        assert_eq!(state.virtual_bottom(), initial_bottom);
        assert_eq!(state.virtual_bottom(), state.viewport().bottom());
    }
}
