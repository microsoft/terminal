//! Safe ownership for VT terminal modes with screen-buffer observables.
//!
//! This module keeps the mode state that is independent of the VT parser:
//! LNM couples output newline-auto-return with input line-feed mode, DECSCNM
//! reverses rendered foreground/background colors, DECAWM controls cell-aware
//! output wrapping, and DECECM selects whether erase producers use active or
//! default colors. Cursor-origin addressing remains in `cursor_movement`, where
//! DEC margins are already owned.

use crate::output_cell::OutputCellIterator;
use crate::row::{DbcsAttribute, RowError};
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferPoint};
use crate::text_color::TextColor;
use crate::width_detector::CodepointWidthDetector;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TerminalModeState {
    newline_auto_return: bool,
    input_line_feed: bool,
    screen_reversed: bool,
    auto_wrap: bool,
    erase_color_mode: bool,
    cursor: TextBufferPoint,
    pending_wrap: bool,
}

impl Default for TerminalModeState {
    fn default() -> Self {
        Self {
            newline_auto_return: true,
            input_line_feed: false,
            screen_reversed: false,
            auto_wrap: true,
            erase_color_mode: false,
            cursor: TextBufferPoint::new(0, 0),
            pending_wrap: false,
        }
    }
}

impl TerminalModeState {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    pub fn set_newline_auto_return_enabled(&mut self, enabled: bool) {
        self.newline_auto_return = enabled;
    }

    /// Applies ANSI LNM as one coupled product state change.
    pub fn set_line_feed_mode(&mut self, enabled: bool) {
        self.newline_auto_return = enabled;
        self.input_line_feed = enabled;
    }

    #[must_use]
    pub const fn newline_auto_return_enabled(&self) -> bool {
        self.newline_auto_return
    }

    #[must_use]
    pub const fn input_line_feed_enabled(&self) -> bool {
        self.input_line_feed
    }

    pub fn set_screen_reversed(&mut self, enabled: bool) {
        self.screen_reversed = enabled;
    }

    #[must_use]
    pub const fn screen_reversed(&self) -> bool {
        self.screen_reversed
    }

    /// Resolves the foreground/background pair after DECSCNM.
    #[must_use]
    pub fn attribute_colors(&self, attribute: TextAttribute) -> (TextColor, TextColor) {
        if self.screen_reversed {
            (attribute.background(), attribute.foreground())
        } else {
            (attribute.foreground(), attribute.background())
        }
    }

    pub fn set_auto_wrap(&mut self, enabled: bool) {
        self.auto_wrap = enabled;
        self.pending_wrap = false;
    }

    #[must_use]
    pub const fn auto_wrap(&self) -> bool {
        self.auto_wrap
    }

    /// Applies DEC Erase Color Mode (DECECM / private mode 117).
    ///
    /// When enabled, erase-producing controls receive default attributes as
    /// their source. When disabled, they receive the active attributes and keep
    /// their existing standard-erase transformation. This keeps color policy at
    /// the terminal-mode boundary while each cell/scroll owner retains its own
    /// erase mechanics.
    pub fn set_erase_color_mode(&mut self, enabled: bool) {
        self.erase_color_mode = enabled;
    }

    #[must_use]
    pub const fn erase_color_mode(&self) -> bool {
        self.erase_color_mode
    }

    /// Resolves the source attributes passed to an erase-producing owner.
    #[must_use]
    pub fn erase_source_attribute(&self, active_attribute: TextAttribute) -> TextAttribute {
        if self.erase_color_mode {
            TextAttribute::default()
        } else {
            active_attribute
        }
    }

    pub fn set_cursor(&mut self, x: u16, y: u16, buffer: &TextBuffer) {
        self.cursor.x = x.min(buffer.width() - 1);
        self.cursor.y = y.min(buffer.height() - 1);
        self.pending_wrap = false;
    }

    #[must_use]
    pub const fn cursor(&self) -> TextBufferPoint {
        self.cursor
    }

    /// Writes UTF-16 output with Windows Terminal's DECAWM boundary behavior.
    ///
    /// With autowrap enabled, a glyph after the final cell begins on the next
    /// row. With autowrap disabled, the cursor remains on the final cell and
    /// subsequent narrow glyphs overwrite it. A wide glyph that cannot fit in
    /// the final cell is ignored; overwriting the trailing half of a wide glyph
    /// is repaired by `Row::replace_glyph`.
    pub fn write_text(&mut self, buffer: &mut TextBuffer, text: &[u16]) -> Result<(), RowError> {
        let detector = CodepointWidthDetector;
        for cell in OutputCellIterator::text_only(text, &detector) {
            if matches!(cell.dbcs_attribute(), DbcsAttribute::Trailing) {
                continue;
            }
            self.write_glyph(buffer, cell.chars(), cell.columns())?;
        }
        Ok(())
    }

    fn write_glyph(
        &mut self,
        buffer: &mut TextBuffer,
        glyph: &[u16],
        columns: u16,
    ) -> Result<(), RowError> {
        let width = buffer.width();
        let last = width - 1;

        if self.auto_wrap {
            if self.pending_wrap || self.cursor.x.saturating_add(columns) > width {
                self.cursor.x = 0;
                self.cursor.y = self.cursor.y.saturating_add(1).min(buffer.height() - 1);
                self.pending_wrap = false;
            }

            buffer.row_mut(i32::from(self.cursor.y)).replace_glyph(
                i32::from(self.cursor.x),
                columns,
                glyph,
            )?;

            let next = self.cursor.x.saturating_add(columns);
            if next >= width {
                self.cursor.x = last;
                self.pending_wrap = true;
            } else {
                self.cursor.x = next;
            }
        } else {
            self.pending_wrap = false;
            if columns > 1 && self.cursor.x.saturating_add(columns) > width {
                return Ok(());
            }

            buffer.row_mut(i32::from(self.cursor.y)).replace_glyph(
                i32::from(self.cursor.x),
                columns,
                glyph,
            )?;
            self.cursor.x = self.cursor.x.saturating_add(columns).min(last);
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::output_cell::GlyphWidthDetector;
    use crate::text_color::Rgb;

    fn text(buffer: &TextBuffer, y: u16, begin: i32, end: i32) -> String {
        String::from_utf16_lossy(buffer.row(i32::from(y)).text_range(begin, end))
    }

    #[test]
    fn microsoft_screen_buffer_set_line_feed_mode_contract() {
        let mut modes = TerminalModeState::new();
        modes.set_newline_auto_return_enabled(false);

        modes.set_line_feed_mode(true);
        assert!(modes.newline_auto_return_enabled());
        assert!(modes.input_line_feed_enabled());

        modes.set_line_feed_mode(false);
        assert!(!modes.newline_auto_return_enabled());
        assert!(!modes.input_line_feed_enabled());
    }

    #[test]
    fn microsoft_screen_buffer_set_screen_mode_contract() {
        let mut modes = TerminalModeState::new();
        let foreground = Rgb::new(12, 34, 56);
        let background = Rgb::new(78, 90, 12);
        let attribute = TextAttribute::from_rgb(foreground, background);

        assert!(!modes.screen_reversed());
        assert_eq!(
            modes.attribute_colors(attribute),
            (attribute.foreground(), attribute.background())
        );

        modes.set_screen_reversed(true);
        assert!(modes.screen_reversed());
        assert_eq!(
            modes.attribute_colors(attribute),
            (attribute.background(), attribute.foreground())
        );

        modes.set_screen_reversed(false);
        assert_eq!(
            modes.attribute_colors(attribute),
            (attribute.foreground(), attribute.background())
        );
    }

    #[test]
    fn microsoft_screen_buffer_set_auto_wrap_mode_contract() {
        let attribute = TextAttribute::default();
        let mut buffer = TextBuffer::new(80, 6, attribute).unwrap();
        let mut modes = TerminalModeState::new();

        modes.set_cursor(77, 0, &buffer);
        modes
            .write_text(&mut buffer, &"abcdef".encode_utf16().collect::<Vec<_>>())
            .unwrap();
        assert_eq!(text(&buffer, 0, 77, 80), "abc");
        assert_eq!(text(&buffer, 1, 0, 3), "def");
        assert_eq!(modes.cursor(), TextBufferPoint::new(3, 1));

        modes.set_auto_wrap(false);
        modes.set_cursor(77, 2, &buffer);
        modes
            .write_text(&mut buffer, &"abcdef".encode_utf16().collect::<Vec<_>>())
            .unwrap();
        assert_eq!(text(&buffer, 2, 77, 80), "abf");
        assert_eq!(modes.cursor(), TextBufferPoint::new(79, 2));

        let smile = [u16::from(b'a'), 0xd83d, 0xde04, u16::from(b'b')];
        modes.set_cursor(77, 2, &buffer);
        modes.write_text(&mut buffer, &smile).unwrap();
        assert_eq!(text(&buffer, 2, 77, 80), "a b");
        assert_eq!(modes.cursor(), TextBufferPoint::new(79, 2));

        let final_cell_smile = [
            u16::from(b'a'),
            u16::from(b'b'),
            0xd83d,
            0xde04,
            u16::from(b'c'),
        ];
        modes.set_cursor(77, 2, &buffer);
        modes.write_text(&mut buffer, &final_cell_smile).unwrap();
        assert_eq!(text(&buffer, 2, 77, 80), "abc");
        assert_eq!(modes.cursor(), TextBufferPoint::new(79, 2));

        modes.set_auto_wrap(true);
        modes.set_cursor(77, 4, &buffer);
        modes
            .write_text(&mut buffer, &"abcdef".encode_utf16().collect::<Vec<_>>())
            .unwrap();
        assert_eq!(text(&buffer, 4, 77, 80), "abc");
        assert_eq!(text(&buffer, 5, 0, 3), "def");
        assert_eq!(modes.cursor(), TextBufferPoint::new(3, 5));
    }

    #[test]
    fn dececm_selects_the_erase_source_attribute() {
        let active = TextAttribute::from_rgb(Rgb::new(12, 34, 56), Rgb::new(78, 90, 12));
        let mut modes = TerminalModeState::new();

        assert!(!modes.erase_color_mode());
        assert_eq!(modes.erase_source_attribute(active), active);

        modes.set_erase_color_mode(true);
        assert!(modes.erase_color_mode());
        assert_eq!(
            modes.erase_source_attribute(active),
            TextAttribute::default()
        );
    }

    #[test]
    fn detector_remains_the_product_unicode_width_policy() {
        let detector = CodepointWidthDetector;
        assert!(detector.is_full_width(&[0xd83d, 0xde04]));
    }
}
