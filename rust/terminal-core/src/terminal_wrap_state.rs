//! Aggregate wrapping state for `TerminalCore` write paths.
//!
//! The buffer layer already owns Microsoft's delayed end-of-line wrap semantics.
//! This wrapper adds the `TerminalCore` observable that records which logical row
//! consumed a pending wrap, matching `Row::WasWrapForced` without duplicating
//! the underlying Last Column Flag state machine.

use std::collections::BTreeSet;

use terminal_buffer::delayed_wrap::AutowrapState;
use terminal_buffer::geometry::Point;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TerminalWrapState {
    autowrap: AutowrapState,
    forced_wrap_rows: BTreeSet<i32>,
}

impl TerminalWrapState {
    #[must_use]
    pub fn new(width: i32) -> Self {
        Self {
            autowrap: AutowrapState::new(width, Point::new(0, 0)),
            forced_wrap_rows: BTreeSet::new(),
        }
    }

    #[must_use]
    pub const fn cursor(&self) -> Point {
        self.autowrap.cursor()
    }

    #[must_use]
    pub fn row_was_wrap_forced(&self, row: i32) -> bool {
        self.forced_wrap_rows.contains(&row)
    }

    pub fn write_char(&mut self, ch: char) {
        if self.autowrap.delayed_wrap() {
            self.forced_wrap_rows.insert(self.autowrap.cursor().y);
        }
        self.autowrap.write_char(ch);
    }

    pub fn write_text(&mut self, text: &str) {
        for ch in text.chars() {
            self.write_char(ch);
        }
    }

    #[must_use]
    pub fn char_at(&self, point: Point) -> Option<char> {
        self.autowrap.char_at(point)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const MICROSOFT_100_CHARS: &str = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~!\"#$%&";

    fn verify_microsoft_100_char_result(state: &TerminalWrapState) {
        assert_eq!(MICROSOFT_100_CHARS.chars().count(), 100);
        assert_eq!(state.cursor(), Point::new(20, 1));
        assert!(state.row_was_wrap_forced(0));
        assert!(!state.row_was_wrap_forced(1));

        for (index, expected) in MICROSOFT_100_CHARS.chars().enumerate() {
            let x = i32::try_from(index % 80).expect("column fits i32");
            let y = i32::try_from(index / 80).expect("row fits i32");
            assert_eq!(
                state.char_at(Point::new(x, y)),
                Some(expected),
                "index={index}"
            );
        }
    }

    #[test]
    fn microsoft_terminal_buffer_wrapping_char_by_char_contract() {
        let mut state = TerminalWrapState::new(80);
        for index in 0..100_u32 {
            let codepoint = 33 + (index % 94);
            state.write_char(char::from_u32(codepoint).expect("Microsoft printable ASCII vector"));
        }
        verify_microsoft_100_char_result(&state);
    }

    #[test]
    fn microsoft_terminal_buffer_wrapping_long_string_contract() {
        let mut state = TerminalWrapState::new(80);
        state.write_text(MICROSOFT_100_CHARS);
        verify_microsoft_100_char_result(&state);
    }
}
