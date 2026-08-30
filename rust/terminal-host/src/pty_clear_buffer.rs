//! Pure planning for `ConPTY` clear-buffer requests.
//!
//! `PtySignalInputThread::_DoClearBuffer` owns locking and buffer mutation in
//! C++. This module preserves only the deterministic arguments supplied to
//! `TextBuffer::ClearScrollback` and the cursor position chosen afterward.

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CursorPosition {
    pub x: i16,
    pub y: i16,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ClearBufferPlan {
    /// Cursor row passed to `TextBuffer::ClearScrollback`.
    pub scrollback_cursor_row: i16,
    /// Number of cursor rows preserved at the top of the resulting buffer.
    pub rows_to_keep: i16,
    /// Cursor position applied after clearing scrollback.
    pub final_cursor: CursorPosition,
}

/// Reproduces the deterministic portion of
/// `PtySignalInputThread::_DoClearBuffer`.
#[must_use]
pub const fn plan_clear_buffer(cursor: CursorPosition, keep_cursor_row: bool) -> ClearBufferPlan {
    ClearBufferPlan {
        scrollback_cursor_row: cursor.y,
        rows_to_keep: if keep_cursor_row { 1 } else { 0 },
        final_cursor: CursorPosition {
            x: if keep_cursor_row { cursor.x } else { 0 },
            y: 0,
        },
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn clearing_without_cursor_row_moves_cursor_to_origin() {
        assert_eq!(
            plan_clear_buffer(CursorPosition { x: 27, y: 14 }, false),
            ClearBufferPlan {
                scrollback_cursor_row: 14,
                rows_to_keep: 0,
                final_cursor: CursorPosition { x: 0, y: 0 },
            }
        );
    }

    #[test]
    fn clearing_with_cursor_row_preserves_cursor_column() {
        assert_eq!(
            plan_clear_buffer(CursorPosition { x: 27, y: 14 }, true),
            ClearBufferPlan {
                scrollback_cursor_row: 14,
                rows_to_keep: 1,
                final_cursor: CursorPosition { x: 27, y: 0 },
            }
        );
    }

    #[test]
    fn scrollback_always_uses_original_cursor_row() {
        assert_eq!(
            plan_clear_buffer(CursorPosition { x: 0, y: 0 }, true).scrollback_cursor_row,
            0
        );
        assert_eq!(
            plan_clear_buffer(CursorPosition { x: 3, y: 32767 }, false).scrollback_cursor_row,
            32767
        );
    }
}
