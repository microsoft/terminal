//! Safe DECSTR coordination for screen-buffer cursor observables.
//!
//! This owner captures the product state exercised by Microsoft's Host
//! `VtSoftResetCursorPosition` and `VtSoftResetAltBufferCursorState` contracts.
//! VT byte parsing and Win32 buffer switching remain outside this module.

use crate::cursor_movement::CursorPosition;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SoftResetState {
    width: u16,
    height: u16,
    main_cursor: CursorPosition,
    alternate_cursor: Option<CursorPosition>,
    active_alternate: bool,
    vertical_margins: Option<(u16, u16)>,
    origin_mode: bool,
}

impl SoftResetState {
    #[must_use]
    pub fn new(width: u16, height: u16) -> Self {
        assert!(width > 0);
        assert!(height > 0);
        Self {
            width,
            height,
            main_cursor: CursorPosition { x: 0, y: 0 },
            alternate_cursor: None,
            active_alternate: false,
            vertical_margins: None,
            origin_mode: false,
        }
    }

    #[must_use]
    pub fn cursor(&self) -> CursorPosition {
        if self.active_alternate {
            self.alternate_cursor.unwrap_or(self.main_cursor)
        } else {
            self.main_cursor
        }
    }

    #[must_use]
    pub const fn origin_mode(&self) -> bool {
        self.origin_mode
    }

    fn active_cursor_mut(&mut self) -> &mut CursorPosition {
        if self.active_alternate {
            self.alternate_cursor
                .as_mut()
                .expect("active alternate cursor exists")
        } else {
            &mut self.main_cursor
        }
    }

    fn home(&mut self) {
        let y = if self.origin_mode {
            self.vertical_margins.map_or(0, |(top, _)| top)
        } else {
            0
        };
        *self.active_cursor_mut() = CursorPosition { x: 0, y };
    }

    /// Applies 1-based DECSTBM coordinates and its cursor-home side effect.
    pub fn set_vertical_margins(&mut self, top: u16, bottom: u16) {
        let top = top.max(1).saturating_sub(1).min(self.height - 1);
        let bottom = bottom.max(1).saturating_sub(1).min(self.height - 1);
        assert!(top < bottom);
        self.vertical_margins = Some((top, bottom));
        self.home();
    }

    /// Applies DECOM and homes the cursor to the newly selected addressing origin.
    pub fn set_origin_mode(&mut self, enabled: bool) {
        self.origin_mode = enabled;
        self.home();
    }

    /// Resolves a 1-based CUP position against the current DECOM state.
    pub fn cursor_position(&mut self, row: u16, column: u16) {
        let relative_y = row.max(1).saturating_sub(1);
        let relative_x = column.max(1).saturating_sub(1);
        let y = if self.origin_mode {
            let (top, bottom) = self.vertical_margins.unwrap_or((0, self.height - 1));
            top.saturating_add(relative_y).min(bottom)
        } else {
            relative_y.min(self.height - 1)
        };
        let x = relative_x.min(self.width - 1);
        *self.active_cursor_mut() = CursorPosition { x, y };
    }

    /// DECSTR resets origin addressing without moving the active cursor.
    ///
    /// The cursor-preservation detail is important: DECSTR differs from a hard
    /// reset and must not home either the main or alternate screen buffer.
    pub fn soft_reset(&mut self) {
        self.origin_mode = false;
    }

    /// Enters the alternate screen with an inherited cursor position while
    /// retaining the main cursor as separately owned state.
    pub fn use_alternate(&mut self) {
        self.alternate_cursor = Some(self.main_cursor);
        self.active_alternate = true;
    }

    /// Returns to the main screen and restores its cursor position verbatim.
    pub fn use_main(&mut self) {
        self.active_alternate = false;
        self.alternate_cursor = None;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_vt_soft_reset_cursor_position_contract() {
        let mut state = SoftResetState::new(80, 25);

        state.cursor_position(2, 2);
        assert_eq!(state.cursor(), CursorPosition { x: 1, y: 1 });
        state.soft_reset();
        assert_eq!(state.cursor(), CursorPosition { x: 1, y: 1 });

        state.set_vertical_margins(2, 10);
        assert_eq!(state.cursor(), CursorPosition { x: 0, y: 0 });
        state.cursor_position(2, 2);
        assert_eq!(state.cursor(), CursorPosition { x: 1, y: 1 });
        state.soft_reset();
        assert_eq!(state.cursor(), CursorPosition { x: 1, y: 1 });

        state.set_origin_mode(true);
        state.set_vertical_margins(5, 10);
        state.cursor_position(2, 2);
        assert_eq!(state.cursor(), CursorPosition { x: 1, y: 5 });

        state.soft_reset();
        assert!(!state.origin_mode());
        assert_eq!(state.cursor(), CursorPosition { x: 1, y: 5 });
        state.set_vertical_margins(5, 10);
        state.cursor_position(2, 2);
        assert_eq!(state.cursor(), CursorPosition { x: 1, y: 1 });
    }

    #[test]
    fn microsoft_vt_soft_reset_alt_buffer_cursor_state_contract() {
        let mut state = SoftResetState::new(80, 25);

        state.cursor_position(4, 7);
        assert_eq!(state.cursor(), CursorPosition { x: 6, y: 3 });
        state.use_alternate();
        state.soft_reset();
        state.use_main();

        assert_eq!(state.cursor(), CursorPosition { x: 6, y: 3 });
    }
}
