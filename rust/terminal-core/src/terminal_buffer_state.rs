//! Aggregate Terminal buffer/viewport state above the portable `TextBuffer` owner.

use std::collections::BTreeSet;

use terminal_buffer::row::RowError;
use terminal_buffer::row_writer::replace_text;
use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::{TextBuffer, TextBufferError};

#[derive(Debug, Clone)]
pub struct TerminalBufferState {
    buffer: TextBuffer,
    viewport_height: u16,
    history_rows: u16,
    mutable_viewport_top: u16,
    scroll_offset: u16,
    line_feeds: u32,
    tab_stops: BTreeSet<u16>,
}

impl TerminalBufferState {
    /// Creates a terminal buffer with a visible viewport and scrollback history.
    ///
    /// # Errors
    ///
    /// Returns [`TextBufferError`] when the requested backing buffer dimensions
    /// cannot be represented by the portable `TextBuffer` owner.
    pub fn new(
        width: u16,
        viewport_height: u16,
        history_rows: u16,
    ) -> Result<Self, TextBufferError> {
        let total_height = viewport_height.saturating_add(history_rows).max(1);
        Ok(Self {
            buffer: TextBuffer::new(width, total_height, TextAttribute::default())?,
            viewport_height: viewport_height.max(1),
            history_rows,
            mutable_viewport_top: 0,
            scroll_offset: 0,
            line_feeds: 0,
            tab_stops: Self::default_tab_stops(width),
        })
    }

    #[must_use]
    pub const fn buffer(&self) -> &TextBuffer {
        &self.buffer
    }

    #[must_use]
    pub const fn viewport_top(&self) -> u16 {
        self.mutable_viewport_top.saturating_sub(self.scroll_offset)
    }

    #[must_use]
    pub const fn viewport_bottom_exclusive(&self) -> u16 {
        self.viewport_top().saturating_add(self.viewport_height)
    }

    #[must_use]
    pub const fn scroll_offset(&self) -> u16 {
        self.scroll_offset
    }

    pub fn write_text_at(&mut self, x: i32, y: i32, text: &[u16]) -> Result<u16, RowError> {
        replace_text(self.buffer.row_mut(y), x, text)
    }

    pub fn line_feed(&mut self) {
        self.line_feeds = self.line_feeds.saturating_add(1);
        if self.line_feeds < u32::from(self.viewport_height) {
            return;
        }

        if self.mutable_viewport_top < self.history_rows {
            self.mutable_viewport_top += 1;
            if self.scroll_offset > 0 {
                self.scroll_offset = self.scroll_offset.saturating_add(1).min(self.history_rows);
            }
        } else if self.scroll_offset > 0 {
            self.scroll_offset = self.scroll_offset.saturating_add(1).min(self.history_rows);
        }
    }

    pub fn set_scroll_offset(&mut self, offset: u16) {
        self.scroll_offset = offset.min(self.history_rows).min(self.mutable_viewport_top);
    }

    #[must_use]
    pub fn tab_stops(&self) -> Vec<u16> {
        self.tab_stops.iter().copied().collect()
    }

    pub fn reset_tab_stops(&mut self) {
        self.tab_stops = Self::default_tab_stops(self.buffer.width());
    }

    pub fn clear_all_tab_stops(&mut self) {
        self.tab_stops.clear();
    }

    pub fn add_tab_stop(&mut self, column: u16) {
        if column < self.buffer.width() {
            self.tab_stops.insert(column);
        }
    }

    pub fn clear_tab_stop(&mut self, column: u16) {
        self.tab_stops.remove(&column);
    }

    #[must_use]
    pub fn forward_tab(&self, column: u16) -> u16 {
        self.tab_stops
            .range(column.saturating_add(1)..)
            .next()
            .copied()
            .unwrap_or_else(|| self.buffer.width().saturating_sub(1))
    }

    #[must_use]
    pub fn reverse_tab(&self, column: u16) -> u16 {
        self.tab_stops
            .range(..column)
            .next_back()
            .copied()
            .unwrap_or(0)
    }

    fn default_tab_stops(width: u16) -> BTreeSet<u16> {
        (8..width).step_by(8).collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn utf16(text: &str) -> Vec<u16> {
        text.encode_utf16().collect()
    }

    #[test]
    fn microsoft_terminal_buffer_simple_writing_contract() {
        let mut terminal = TerminalBufferState::new(80, 32, 100).expect("valid terminal");
        assert_eq!(terminal.viewport_top(), 0);
        assert_eq!(terminal.viewport_bottom_exclusive(), 32);

        let expected = utf16("Hello World");
        terminal
            .write_text_at(0, 0, &expected)
            .expect("source text fits");

        assert_eq!(terminal.viewport_top(), 0);
        assert_eq!(terminal.viewport_bottom_exclusive(), 32);
        for (column, code_unit) in expected.into_iter().enumerate() {
            let column = i32::try_from(column).expect("Hello World column fits in i32");
            assert_eq!(
                terminal.buffer().row(0).glyph_at(column),
                &[code_unit],
                "column {column} must preserve Microsoft's source string"
            );
        }
    }

    #[test]
    fn microsoft_terminal_buffer_dont_snap_to_output_contract() {
        const HEIGHT: u16 = 32;
        const HISTORY: u16 = 100;
        let mut terminal = TerminalBufferState::new(80, HEIGHT, HISTORY).expect("valid terminal");

        assert_eq!(terminal.viewport_top(), 0);
        assert_eq!(terminal.viewport_bottom_exclusive(), HEIGHT);
        assert_eq!(terminal.scroll_offset(), 0);

        for _ in 0..(u32::from(HEIGHT) + 8 - 1) {
            terminal.line_feed();
        }
        assert_eq!(terminal.viewport_top(), 8);
        assert_eq!(terminal.viewport_bottom_exclusive(), HEIGHT + 8);
        assert_eq!(terminal.scroll_offset(), 0);

        terminal.set_scroll_offset(1);
        assert_eq!(terminal.viewport_top(), 7);
        assert_eq!(terminal.viewport_bottom_exclusive(), HEIGHT + 7);
        assert_eq!(terminal.scroll_offset(), 1);

        for _ in 0..8 {
            terminal.line_feed();
        }
        assert_eq!(terminal.viewport_top(), 7);
        assert_eq!(terminal.viewport_bottom_exclusive(), HEIGHT + 7);
        assert_eq!(terminal.scroll_offset(), 9);

        while terminal.mutable_viewport_top < HISTORY {
            terminal.line_feed();
        }
        assert_eq!(terminal.viewport_top(), 7);
        assert_eq!(terminal.viewport_bottom_exclusive(), HEIGHT + 7);
        assert_eq!(terminal.scroll_offset(), HISTORY - 7);

        for _ in 0..3 {
            terminal.line_feed();
        }
        assert_eq!(terminal.viewport_top(), 4);
        assert_eq!(terminal.viewport_bottom_exclusive(), HEIGHT + 4);
        assert_eq!(terminal.scroll_offset(), HISTORY - 4);

        for _ in 0..8 {
            terminal.line_feed();
        }
        assert_eq!(terminal.viewport_top(), 0);
        assert_eq!(terminal.viewport_bottom_exclusive(), HEIGHT);
        assert_eq!(terminal.scroll_offset(), HISTORY);
    }

    #[test]
    fn microsoft_terminal_buffer_reset_clear_tab_stops_contract() {
        let mut terminal = TerminalBufferState::new(80, 32, 100).expect("valid terminal");
        let defaults = vec![8, 16, 24, 32, 40, 48, 56, 64, 72];
        assert_eq!(terminal.tab_stops(), defaults);

        terminal.clear_all_tab_stops();
        assert!(terminal.tab_stops().is_empty());

        terminal.reset_tab_stops();
        assert_eq!(terminal.tab_stops(), defaults);
    }

    #[test]
    fn microsoft_terminal_buffer_add_tab_stop_contract() {
        let mut terminal = TerminalBufferState::new(80, 32, 100).expect("valid terminal");
        terminal.clear_all_tab_stops();
        assert!(terminal.tab_stops().is_empty());

        terminal.add_tab_stop(12);
        assert_eq!(terminal.tab_stops(), [12]);
        terminal.add_tab_stop(4);
        assert_eq!(terminal.tab_stops(), [4, 12]);
        terminal.add_tab_stop(30);
        assert_eq!(terminal.tab_stops(), [4, 12, 30]);
        terminal.add_tab_stop(24);
        assert_eq!(terminal.tab_stops(), [4, 12, 24, 30]);
        terminal.add_tab_stop(24);
        assert_eq!(terminal.tab_stops(), [4, 12, 24, 30]);
    }

    #[test]
    fn microsoft_terminal_buffer_clear_tab_stop_contract() {
        let mut terminal = TerminalBufferState::new(80, 32, 100).expect("valid terminal");
        terminal.clear_all_tab_stops();
        terminal.clear_tab_stop(0);
        assert!(terminal.tab_stops().is_empty());

        terminal.add_tab_stop(0);
        terminal.clear_tab_stop(0);
        assert!(terminal.tab_stops().is_empty());

        terminal.add_tab_stop(1);
        terminal.clear_tab_stop(2);
        terminal.clear_tab_stop(0);
        assert_eq!(terminal.tab_stops(), [1]);
        terminal.clear_all_tab_stops();

        for cleared in [3, 5, 17, 0] {
            let mut candidate = TerminalBufferState::new(80, 32, 100).expect("valid terminal");
            candidate.clear_all_tab_stops();
            for stop in [3, 5, 6, 10, 15, 17] {
                candidate.add_tab_stop(stop);
            }
            candidate.clear_tab_stop(cleared);
            let expected: Vec<u16> = [3, 5, 6, 10, 15, 17]
                .into_iter()
                .filter(|stop| *stop != cleared)
                .collect();
            assert_eq!(candidate.tab_stops(), expected);
        }
    }

    #[test]
    fn microsoft_terminal_buffer_forward_tab_contract() {
        let mut terminal = TerminalBufferState::new(80, 32, 100).expect("valid terminal");
        terminal.clear_all_tab_stops();
        for stop in [3, 5, 6, 10, 15, 17] {
            terminal.add_tab_stop(stop);
        }

        assert_eq!(terminal.forward_tab(0), 3);
        assert_eq!(terminal.forward_tab(6), 10);
        assert_eq!(terminal.forward_tab(30), 79);
        assert_eq!(terminal.forward_tab(79), 79);
    }

    #[test]
    fn microsoft_terminal_buffer_reverse_tab_contract() {
        let mut terminal = TerminalBufferState::new(80, 32, 100).expect("valid terminal");
        terminal.clear_all_tab_stops();
        for stop in [3, 5, 6, 10, 15, 17] {
            terminal.add_tab_stop(stop);
        }

        assert_eq!(terminal.reverse_tab(1), 0);
        assert_eq!(terminal.reverse_tab(6), 5);
        assert_eq!(terminal.reverse_tab(30), 17);
    }
}
