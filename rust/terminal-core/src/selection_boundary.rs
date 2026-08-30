//! Viewport/buffer boundary normalization for `TerminalCore` selection input.

use terminal_buffer::row::DbcsAttribute;
use terminal_buffer::text_buffer::TextBuffer;

use crate::selection::{BufferPoint, SelectionInfo, SelectionState};

#[must_use]
pub fn viewport_to_buffer(point: BufferPoint, viewport_top: i32) -> BufferPoint {
    BufferPoint::new(point.x, point.y.saturating_add(viewport_top))
}

#[must_use]
pub fn clamp_selection_point(buffer: &TextBuffer, point: BufferPoint) -> BufferPoint {
    BufferPoint::new(
        point.x.clamp(0, i32::from(buffer.width())),
        point
            .y
            .clamp(0, i32::from(buffer.height()).saturating_sub(1)),
    )
}

#[must_use]
pub fn repair_trailing_wide_anchor(
    buffer: &TextBuffer,
    point: BufferPoint,
) -> (BufferPoint, BufferPoint) {
    let point = clamp_selection_point(buffer, point);
    if point.x >= i32::from(buffer.width()) {
        return (point, point);
    }
    let row = buffer.row(point.y);
    if row.dbcs_attribute_at(point.x) == DbcsAttribute::Trailing {
        let start_x = i32::from(row.adjust_to_glyph_start(point.x));
        let end_x = i32::from(row.adjust_to_glyph_end(point.x.saturating_add(1)));
        return (
            BufferPoint::new(start_x, point.y),
            BufferPoint::new(end_x, point.y),
        );
    }
    (point, point)
}

impl SelectionState {
    pub fn set_anchor_from_viewport(
        &mut self,
        buffer: &TextBuffer,
        viewport_top: i32,
        point: BufferPoint,
    ) {
        let point = viewport_to_buffer(point, viewport_top);
        let (start, end) = repair_trailing_wide_anchor(buffer, point);
        self.selection = SelectionInfo {
            start,
            end,
            pivot: start,
            block_selection: false,
            active: true,
        };
    }

    pub fn set_end_from_viewport(
        &mut self,
        buffer: &TextBuffer,
        viewport_top: i32,
        point: BufferPoint,
    ) {
        if !self.selection.active {
            return;
        }
        let target = clamp_selection_point(buffer, viewport_to_buffer(point, viewport_top));
        if target < self.selection.pivot {
            self.selection.start = target;
            self.selection.end = self.selection.pivot;
        } else {
            self.selection.start = self.selection.pivot;
            self.selection.end = target;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::selection::SelectionExpansion;
    use terminal_buffer::text_attribute::TextAttribute;

    fn buffer(width: u16, height: u16) -> TextBuffer {
        TextBuffer::new(width, height, TextAttribute::default()).expect("valid fixture")
    }

    #[test]
    fn microsoft_selection_overflow_contract() {
        let small = buffer(10, 10);
        let max = BufferPoint::new(i32::MAX, i32::MAX);
        assert_eq!(clamp_selection_point(&small, max), BufferPoint::new(10, 9));

        let mut single = SelectionState::default();
        single.set_anchor_from_viewport(&small, 0, max);
        assert_eq!(single.selection.start, BufferPoint::new(10, 9));
        assert_eq!(single.selection.end, BufferPoint::new(10, 9));

        let mut word = SelectionState::default();
        word.multi_click(&small, max, SelectionExpansion::Word, &[]);
        assert_eq!(word.selection.start, BufferPoint::new(0, 9));
        assert_eq!(word.selection.end, BufferPoint::new(10, 9));

        let mut line = SelectionState::default();
        line.multi_click(&small, max, SelectionExpansion::Line, &[]);
        assert_eq!(line.selection.start, BufferPoint::new(0, 9));
        assert_eq!(line.selection.end, BufferPoint::new(10, 9));

        let maximum_scrollback = buffer(10, i16::MAX as u16);
        let expected_row = i32::from(i16::MAX) - 1;
        assert_eq!(
            clamp_selection_point(&maximum_scrollback, max),
            BufferPoint::new(10, expected_row)
        );

        let mut maximum_single = SelectionState::default();
        maximum_single.set_anchor_from_viewport(&maximum_scrollback, 0, max);
        assert_eq!(
            maximum_single.selection.start,
            BufferPoint::new(10, expected_row)
        );
        assert_eq!(
            maximum_single.selection.end,
            BufferPoint::new(10, expected_row)
        );

        let mut maximum_word = SelectionState::default();
        maximum_word.multi_click(&maximum_scrollback, max, SelectionExpansion::Word, &[]);
        assert_eq!(
            maximum_word.selection.start,
            BufferPoint::new(0, expected_row)
        );
        assert_eq!(
            maximum_word.selection.end,
            BufferPoint::new(10, expected_row)
        );

        let mut maximum_line = SelectionState::default();
        maximum_line.multi_click(&maximum_scrollback, max, SelectionExpansion::Line, &[]);
        assert_eq!(
            maximum_line.selection.start,
            BufferPoint::new(0, expected_row)
        );
        assert_eq!(
            maximum_line.selection.end,
            BufferPoint::new(10, expected_row)
        );
    }

    #[test]
    fn microsoft_selection_from_out_of_bounds_contract() {
        let buffer = buffer(10, 10);
        for (input, expected) in [
            (BufferPoint::new(20, 5), BufferPoint::new(10, 5)),
            (BufferPoint::new(-20, 5), BufferPoint::new(0, 5)),
            (BufferPoint::new(5, -20), BufferPoint::new(5, 0)),
            (BufferPoint::new(5, 20), BufferPoint::new(5, 9)),
        ] {
            let mut state = SelectionState::default();
            state.set_anchor_from_viewport(&buffer, 0, input);
            assert_eq!(state.selection.start, expected);
            assert_eq!(state.selection.end, expected);
        }
    }

    #[test]
    fn microsoft_selection_to_out_of_bounds_contract() {
        let buffer = buffer(10, 10);
        let mut state = SelectionState::default();
        state.set_anchor_from_viewport(&buffer, 0, BufferPoint::new(5, 5));

        state.set_end_from_viewport(&buffer, 0, BufferPoint::new(20, 5));
        assert_eq!(state.selection.start, BufferPoint::new(5, 5));
        assert_eq!(state.selection.end, BufferPoint::new(10, 5));

        state.set_end_from_viewport(&buffer, 0, BufferPoint::new(-20, 5));
        assert_eq!(state.selection.start, BufferPoint::new(0, 5));
        assert_eq!(state.selection.end, BufferPoint::new(5, 5));

        state.set_end_from_viewport(&buffer, 0, BufferPoint::new(5, -20));
        assert_eq!(state.selection.start, BufferPoint::new(5, 0));
        assert_eq!(state.selection.end, BufferPoint::new(5, 5));

        state.set_end_from_viewport(&buffer, 0, BufferPoint::new(5, 20));
        assert_eq!(state.selection.start, BufferPoint::new(5, 5));
        assert_eq!(state.selection.end, BufferPoint::new(5, 9));
    }

    #[test]
    fn microsoft_selection_after_scroll_uses_buffer_coordinates() {
        let buffer = buffer(120, 130);
        let mut state = SelectionState::default();
        state.set_anchor_from_viewport(&buffer, 15, BufferPoint::new(5, 10));
        state.set_end_from_viewport(&buffer, 15, BufferPoint::new(15, 20));
        assert_eq!(state.selection.start, BufferPoint::new(5, 25));
        assert_eq!(state.selection.end, BufferPoint::new(15, 35));

        state.set_anchor_from_viewport(&buffer, 10, BufferPoint::new(5, 10));
        state.set_end_from_viewport(&buffer, 10, BufferPoint::new(15, 20));
        assert_eq!(state.selection.start, BufferPoint::new(5, 20));
        assert_eq!(state.selection.end, BufferPoint::new(15, 30));
    }

    #[test]
    fn microsoft_selection_trailing_wide_glyph_repairs_both_halves() {
        let mut buffer = buffer(100, 100);
        buffer
            .row_mut(10)
            .replace_glyph(4, 2, &[0xd83c, 0xdf2f])
            .expect("wide glyph fixture fits");
        let mut state = SelectionState::default();
        state.set_anchor_from_viewport(&buffer, 0, BufferPoint::new(5, 10));
        assert_eq!(state.selection.start, BufferPoint::new(4, 10));
        assert_eq!(state.selection.end, BufferPoint::new(6, 10));
    }
}
