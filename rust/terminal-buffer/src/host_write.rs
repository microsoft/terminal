//! Safe host output coordination for printable text, inactive controls and backspace.
//!
//! The VT parser and Win32 adapters decide which path supplies the text. Once
//! text reaches the buffer, both the VT stream and `WriteCharsLegacy` must store
//! the complete modern `TextAttribute` without converting it back to a legacy
//! WORD. Backspace is cursor movement only: it never rewrites the cell being
//! left or the cell the cursor moves onto. C0 controls that Windows Terminal
//! classifies as inactive are discarded without writing or moving the cursor.

use crate::row::RowError;
use crate::row_writer::replace_text_with_attribute;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferPoint};

const BACKSPACE: u16 = 0x0008;

#[must_use]
const fn is_inactive_control(unit: u16) -> bool {
    matches!(unit, 0x0000..=0x0007 | 0x000e..=0x0019 | 0x001c..=0x001f)
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct HostWriteState {
    cursor: TextBufferPoint,
    current_attribute: TextAttribute,
}

impl HostWriteState {
    #[must_use]
    pub const fn new(cursor: TextBufferPoint, current_attribute: TextAttribute) -> Self {
        Self {
            cursor,
            current_attribute,
        }
    }

    #[must_use]
    pub const fn cursor(&self) -> TextBufferPoint {
        self.cursor
    }

    #[must_use]
    pub const fn current_attribute(&self) -> TextAttribute {
        self.current_attribute
    }

    pub fn set_current_attribute(&mut self, attribute: TextAttribute) {
        self.current_attribute = attribute;
    }

    /// Applies the screen-buffer side of ordinary VT text, inactive C0 controls
    /// and BS cursor movement.
    pub fn write_vt(&mut self, buffer: &mut TextBuffer, text: &[u16]) -> Result<(), RowError> {
        self.write_stream(buffer, text)
    }

    /// Applies the cell-writing side of `WriteCharsLegacy`.
    ///
    /// The legacy entry point intentionally shares the modern attribute-preserving
    /// writer. Platform conversion and API adaptation stay outside this owner.
    pub fn write_chars_legacy(
        &mut self,
        buffer: &mut TextBuffer,
        text: &[u16],
    ) -> Result<(), RowError> {
        self.write_stream(buffer, text)
    }

    /// Moves the cursor left without mutating row contents or attributes.
    pub fn cursor_left(&mut self, count: u16) {
        self.cursor.x = self.cursor.x.saturating_sub(count);
    }

    fn write_stream(&mut self, buffer: &mut TextBuffer, text: &[u16]) -> Result<(), RowError> {
        let mut run_start = 0;

        for (index, unit) in text.iter().copied().enumerate() {
            if unit != BACKSPACE && !is_inactive_control(unit) {
                continue;
            }

            self.write_run(buffer, &text[run_start..index])?;
            if unit == BACKSPACE {
                self.cursor_left(1);
            }
            run_start = index + 1;
        }

        self.write_run(buffer, &text[run_start..])
    }

    fn write_run(&mut self, buffer: &mut TextBuffer, text: &[u16]) -> Result<(), RowError> {
        if text.is_empty() {
            return Ok(());
        }

        let width = buffer.width();
        let row = self.cursor.y.min(buffer.height().saturating_sub(1));
        let end = replace_text_with_attribute(
            buffer.row_mut(i32::from(row)),
            i32::from(self.cursor.x),
            text,
            self.current_attribute,
        )?;

        // Autowrap and delayed-wrap state belong to `TerminalModeState`. This
        // host-row writer keeps its cursor inside the physical row it owns.
        self.cursor.x = end.min(width.saturating_sub(1));
        self.cursor.y = row;
        Ok(())
    }
}
