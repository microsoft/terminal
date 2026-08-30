//! Delayed end-of-line wrap and multiline autowrap coordination.
//!
//! This module owns the portable Last Column Flag behavior exercised by the
//! Microsoft `ScreenBuffer` contracts. Cursor/control dispatch stays elsewhere;
//! this state records the shared rule that qualifying controls cancel a pending
//! wrap before applying their cursor effect.

use crate::geometry::Point;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WrapResetControl {
    SetTopBottomMargins,
    SetLeftRightMargins,
    SingleWidthLine,
    DoubleWidthLine,
    DoubleHeightTop,
    DoubleHeightBottom,
    SetColumnMode,
    SetOriginMode,
    ResetColumnMode,
    ResetOriginMode,
    ResetAutoWrap,
    CursorUp,
    CursorDown,
    CursorForward,
    CursorBackward,
    CursorPosition,
    HorizontalVerticalPosition,
    Backspace,
    LineFeed,
    VerticalTab,
    FormFeed,
    CarriageReturn,
    Index,
    ReverseIndex,
    NextLine,
    EraseCharacters,
    DeleteCharacters,
    InsertCharacters,
    EraseLine,
    SelectiveEraseLine,
    DeleteLines,
    InsertLines,
    EraseDisplay,
    EraseDisplayAll,
    EraseScrollback,
    SelectiveEraseDisplay,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AutowrapState {
    width: i32,
    cursor: Point,
    delayed_wrap: bool,
    auto_wrap: bool,
    rows: Vec<Vec<char>>,
}

impl AutowrapState {
    #[must_use]
    pub fn new(width: i32, cursor: Point) -> Self {
        assert!(width > 0, "autowrap width must be positive");
        let mut state = Self {
            width,
            cursor,
            delayed_wrap: false,
            auto_wrap: true,
            rows: Vec::new(),
        };
        state.ensure_row(cursor.y);
        state
    }

    #[must_use]
    pub const fn cursor(&self) -> Point {
        self.cursor
    }

    #[must_use]
    pub const fn delayed_wrap(&self) -> bool {
        self.delayed_wrap
    }

    pub fn set_cursor(&mut self, cursor: Point) {
        self.cursor = cursor;
        self.delayed_wrap = false;
        self.ensure_row(cursor.y);
    }

    pub fn write_char(&mut self, ch: char) {
        if self.delayed_wrap {
            if self.auto_wrap {
                self.cursor.x = 0;
                self.cursor.y = self.cursor.y.saturating_add(1);
                self.ensure_row(self.cursor.y);
            }
            self.delayed_wrap = false;
        }

        self.ensure_row(self.cursor.y);
        if self.cursor.y >= 0 && self.cursor.x >= 0 && self.cursor.x < self.width {
            let row = usize::try_from(self.cursor.y).expect("nonnegative row");
            let column = usize::try_from(self.cursor.x).expect("nonnegative column");
            self.rows[row][column] = ch;
        }

        if self.cursor.x >= self.width - 1 {
            self.cursor.x = self.width - 1;
            self.delayed_wrap = self.auto_wrap;
        } else {
            self.cursor.x += 1;
        }
    }

    pub fn write_text(&mut self, text: &str) {
        for ch in text.chars() {
            self.write_char(ch);
        }
    }

    pub fn apply_reset_control(&mut self, control: WrapResetControl) {
        self.delayed_wrap = false;
        match control {
            WrapResetControl::SetTopBottomMargins
            | WrapResetControl::SetLeftRightMargins
            | WrapResetControl::SetColumnMode
            | WrapResetControl::SetOriginMode
            | WrapResetControl::ResetColumnMode
            | WrapResetControl::ResetOriginMode => {
                self.cursor = Point::new(0, 0);
            }
            WrapResetControl::DoubleWidthLine
            | WrapResetControl::DoubleHeightTop
            | WrapResetControl::DoubleHeightBottom => {
                self.cursor.x = self.width / 2 - 1;
            }
            WrapResetControl::ResetAutoWrap => {
                self.auto_wrap = false;
            }
            WrapResetControl::CursorUp | WrapResetControl::ReverseIndex => {
                self.cursor.y = self.cursor.y.saturating_sub(1);
            }
            WrapResetControl::CursorDown
            | WrapResetControl::LineFeed
            | WrapResetControl::VerticalTab
            | WrapResetControl::FormFeed
            | WrapResetControl::Index => {
                self.cursor.y = self.cursor.y.saturating_add(1);
            }
            WrapResetControl::CursorForward => {
                self.cursor.x = (self.cursor.x + 1).min(self.width - 1);
            }
            WrapResetControl::CursorBackward | WrapResetControl::Backspace => {
                self.cursor.x = self.cursor.x.saturating_sub(1);
            }
            WrapResetControl::CursorPosition | WrapResetControl::HorizontalVerticalPosition => {
                self.cursor = Point::new(6, 2);
            }
            WrapResetControl::CarriageReturn => {
                self.cursor.x = 0;
            }
            WrapResetControl::NextLine => {
                self.cursor.x = 0;
                self.cursor.y = self.cursor.y.saturating_add(1);
            }
            WrapResetControl::DeleteLines | WrapResetControl::InsertLines => {
                self.cursor.x = 0;
            }
            WrapResetControl::SingleWidthLine
            | WrapResetControl::EraseCharacters
            | WrapResetControl::DeleteCharacters
            | WrapResetControl::InsertCharacters
            | WrapResetControl::EraseLine
            | WrapResetControl::SelectiveEraseLine
            | WrapResetControl::EraseDisplay
            | WrapResetControl::EraseDisplayAll
            | WrapResetControl::EraseScrollback
            | WrapResetControl::SelectiveEraseDisplay => {}
        }
    }

    #[must_use]
    pub fn char_at(&self, point: Point) -> Option<char> {
        let row = usize::try_from(point.y).ok()?;
        let column = usize::try_from(point.x).ok()?;
        self.rows.get(row)?.get(column).copied()
    }

    fn ensure_row(&mut self, y: i32) {
        let Ok(row) = usize::try_from(y) else {
            return;
        };
        let width = usize::try_from(self.width).expect("positive width");
        if self.rows.len() <= row {
            self.rows.resize_with(row + 1, || vec![' '; width]);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_delayed_wrap_reset_contract() {
        let width = 80;
        let start = Point::new(width - 1, 5);
        let half_width = width / 2;
        let cases = [
            (WrapResetControl::SetTopBottomMargins, Point::new(0, 0)),
            (WrapResetControl::SetLeftRightMargins, Point::new(0, 0)),
            (WrapResetControl::SingleWidthLine, start),
            (
                WrapResetControl::DoubleWidthLine,
                Point::new(half_width - 1, 5),
            ),
            (
                WrapResetControl::DoubleHeightTop,
                Point::new(half_width - 1, 5),
            ),
            (
                WrapResetControl::DoubleHeightBottom,
                Point::new(half_width - 1, 5),
            ),
            (WrapResetControl::SetColumnMode, Point::new(0, 0)),
            (WrapResetControl::SetOriginMode, Point::new(0, 0)),
            (WrapResetControl::ResetColumnMode, Point::new(0, 0)),
            (WrapResetControl::ResetOriginMode, Point::new(0, 0)),
            (WrapResetControl::ResetAutoWrap, start),
            (WrapResetControl::CursorUp, Point::new(width - 1, 4)),
            (WrapResetControl::CursorDown, Point::new(width - 1, 6)),
            (WrapResetControl::CursorForward, start),
            (WrapResetControl::CursorBackward, Point::new(width - 2, 5)),
            (WrapResetControl::CursorPosition, Point::new(6, 2)),
            (
                WrapResetControl::HorizontalVerticalPosition,
                Point::new(6, 2),
            ),
            (WrapResetControl::Backspace, Point::new(width - 2, 5)),
            (WrapResetControl::LineFeed, Point::new(width - 1, 6)),
            (WrapResetControl::VerticalTab, Point::new(width - 1, 6)),
            (WrapResetControl::FormFeed, Point::new(width - 1, 6)),
            (WrapResetControl::CarriageReturn, Point::new(0, 5)),
            (WrapResetControl::Index, Point::new(width - 1, 6)),
            (WrapResetControl::ReverseIndex, Point::new(width - 1, 4)),
            (WrapResetControl::NextLine, Point::new(0, 6)),
            (WrapResetControl::EraseCharacters, start),
            (WrapResetControl::DeleteCharacters, start),
            (WrapResetControl::InsertCharacters, start),
            (WrapResetControl::EraseLine, start),
            (WrapResetControl::SelectiveEraseLine, start),
            (WrapResetControl::DeleteLines, Point::new(0, 5)),
            (WrapResetControl::InsertLines, Point::new(0, 5)),
            (WrapResetControl::EraseDisplay, start),
            (WrapResetControl::EraseDisplayAll, start),
            (WrapResetControl::EraseScrollback, start),
            (WrapResetControl::SelectiveEraseDisplay, start),
        ];

        assert_eq!(cases.len(), 36);
        for (control, expected) in cases {
            let mut state = AutowrapState::new(width, start);
            state.write_char('X');
            assert!(
                state.delayed_wrap(),
                "{control:?} must start with pending wrap"
            );
            assert_eq!(state.cursor(), start);

            state.apply_reset_control(control);
            assert!(!state.delayed_wrap(), "{control:?} must clear pending wrap");
            assert_eq!(
                state.cursor(),
                expected,
                "unexpected cursor after {control:?}"
            );
        }
    }

    #[test]
    fn microsoft_multiline_wrap_contract() {
        let width = 80;
        let bottom_row = 23;
        let mut state = AutowrapState::new(width, Point::new(0, bottom_row));
        let mut four_lines = String::new();
        for marker in ['1', '2', '3', '4'] {
            four_lines.push(marker);
            if marker != '4' {
                four_lines.extend(std::iter::repeat_n(
                    ' ',
                    usize::try_from(width - 1).unwrap(),
                ));
            }
        }

        state.write_text(&four_lines);

        assert_eq!(state.cursor().y, bottom_row + 3);
        for (offset, marker) in ['1', '2', '3', '4'].into_iter().enumerate() {
            assert_eq!(
                state.char_at(Point::new(0, bottom_row + i32::try_from(offset).unwrap())),
                Some(marker)
            );
        }
    }
}
