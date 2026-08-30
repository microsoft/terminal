//! Selection spans and viewport-relative rendering endpoints from `TerminalCore`.
//!
//! This module ports the deterministic geometry from
//! `Terminal::_GetSelectionSpans`, `SelectionStartForRendering`, and
//! `SelectionEndForRendering` without introducing renderer, Win32, C++, or FFI
//! dependencies.

use crate::selection::{BufferPoint, SelectionInfo};
use terminal_buffer::row::DbcsAttribute;
use terminal_buffer::text_buffer::TextBuffer;

/// A half-open row-major span in buffer coordinates.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct SelectionSpan {
    pub start: BufferPoint,
    pub end: BufferPoint,
}

impl SelectionSpan {
    #[must_use]
    pub const fn new(start: BufferPoint, end: BufferPoint) -> Self {
        Self { start, end }
    }
}

/// Returns absolute buffer spans for the active selection.
///
/// Linear selections remain one row-major span. Block selections are expanded
/// into one horizontal half-open span per selected row, matching the shape
/// consumed by the renderer while keeping the terminal buffer representation
/// platform-neutral. Per-row block boundaries are repaired when they cut
/// through the trailing half of a wide glyph, matching `TerminalCore`.
#[must_use]
pub fn selection_spans(buffer: &TextBuffer, selection: &SelectionInfo) -> Vec<SelectionSpan> {
    if !selection.active {
        return Vec::new();
    }

    let start = clamp_exclusive(buffer, selection.start);
    let end = clamp_exclusive(buffer, selection.end);
    if !selection.block_selection {
        let (start, end) = if start <= end {
            (start, end)
        } else {
            (end, start)
        };
        return vec![SelectionSpan::new(start, end)];
    }

    let top = start.y.min(end.y);
    let bottom = start.y.max(end.y);
    let left = start.x.min(end.x).clamp(0, i32::from(buffer.width()));
    let right = start.x.max(end.x).clamp(0, i32::from(buffer.width()));
    let width = i32::from(buffer.width());

    (top..=bottom)
        .map(|y| {
            let row = buffer.row(y);
            let mut row_left = left;
            let mut row_right = right;

            if row_left < width && row.dbcs_attribute_at(row_left) == DbcsAttribute::Trailing {
                row_left = i32::from(row.adjust_to_glyph_start(row_left));
            }
            if row_right < width && row.dbcs_attribute_at(row_right) == DbcsAttribute::Trailing {
                row_right = i32::from(row.adjust_to_glyph_end(row_right.saturating_add(1)));
            }

            SelectionSpan::new(
                BufferPoint::new(row_left, y),
                BufferPoint::new(row_right, y),
            )
        })
        .collect()
}

/// Gets the viewport-relative marker position for the selection start.
///
/// A start endpoint on the trailing half of a wide glyph first moves to the
/// leading half. The marker is then drawn one cell before the selection unless
/// the selection begins at the left edge, where the marker is flipped instead.
#[must_use]
pub fn selection_start_for_rendering(
    buffer: &TextBuffer,
    selection: &SelectionInfo,
    visible_start_y: i32,
) -> BufferPoint {
    let mut pos = clamp_exclusive(buffer, selection.start);
    if is_in_bounds(buffer, pos)
        && matches!(
            buffer.row(pos.y).dbcs_attribute_at(pos.x),
            DbcsAttribute::Trailing
        )
    {
        pos = decrement_in_exclusive_bounds(buffer, pos);
    }

    if pos.x != 0 {
        pos = decrement_in_exclusive_bounds(buffer, pos);
    }
    pos.y = pos.y.saturating_sub(visible_start_y).max(0);
    pos
}

/// Gets the viewport-relative marker position for the selection end.
///
/// A trailing-half endpoint advances so the entire wide glyph remains selected.
/// Because the end anchor is right-exclusive, an endpoint at the right boundary
/// moves one cell left so the marker is drawn inside the row.
#[must_use]
pub fn selection_end_for_rendering(
    buffer: &TextBuffer,
    selection: &SelectionInfo,
    visible_start_y: i32,
) -> BufferPoint {
    let mut pos = clamp_exclusive(buffer, selection.end);
    if is_in_bounds(buffer, pos)
        && matches!(
            buffer.row(pos.y).dbcs_attribute_at(pos.x),
            DbcsAttribute::Trailing
        )
    {
        pos = increment_in_exclusive_bounds(buffer, pos);
    }

    if pos.x == i32::from(buffer.width()) {
        pos = decrement_in_exclusive_bounds(buffer, pos);
    }
    pos.y = pos.y.saturating_sub(visible_start_y).max(0);
    pos
}

#[must_use]
fn is_in_bounds(buffer: &TextBuffer, point: BufferPoint) -> bool {
    point.x >= 0
        && point.x < i32::from(buffer.width())
        && point.y >= 0
        && point.y < i32::from(buffer.height())
}

#[must_use]
fn clamp_exclusive(buffer: &TextBuffer, point: BufferPoint) -> BufferPoint {
    BufferPoint::new(
        point.x.clamp(0, i32::from(buffer.width())),
        point
            .y
            .clamp(0, i32::from(buffer.height()).saturating_sub(1)),
    )
}

#[must_use]
fn decrement_in_exclusive_bounds(buffer: &TextBuffer, point: BufferPoint) -> BufferPoint {
    let width = i32::from(buffer.width());
    let mut point = clamp_exclusive(buffer, point);
    if point.x > 0 {
        point.x -= 1;
    } else if point.y > 0 {
        point.y -= 1;
        point.x = width.saturating_sub(1);
    }
    point
}

#[must_use]
fn increment_in_exclusive_bounds(buffer: &TextBuffer, point: BufferPoint) -> BufferPoint {
    let width = i32::from(buffer.width());
    let bottom = i32::from(buffer.height()).saturating_sub(1);
    let mut point = clamp_exclusive(buffer, point);
    if point.x < width {
        point.x += 1;
    }
    if point.x >= width && point.y < bottom {
        point.x = 0;
        point.y += 1;
    }
    point
}

#[cfg(test)]
mod tests {
    use super::*;
    use terminal_buffer::text_attribute::TextAttribute;

    fn buffer(width: u16, height: u16) -> TextBuffer {
        TextBuffer::new(width, height, TextAttribute::default()).expect("valid test buffer")
    }

    #[test]
    fn inactive_selection_has_no_spans() {
        let buffer = buffer(8, 3);
        assert!(selection_spans(&buffer, &SelectionInfo::default()).is_empty());
    }

    #[test]
    fn linear_selection_is_one_row_major_span() {
        let buffer = buffer(8, 3);
        let selection = SelectionInfo {
            start: BufferPoint::new(6, 0),
            end: BufferPoint::new(2, 2),
            pivot: BufferPoint::new(6, 0),
            block_selection: false,
            active: true,
        };

        assert_eq!(
            selection_spans(&buffer, &selection),
            [SelectionSpan::new(
                BufferPoint::new(6, 0),
                BufferPoint::new(2, 2)
            )]
        );
    }

    #[test]
    fn block_selection_emits_one_horizontal_span_per_row() {
        let buffer = buffer(8, 4);
        let selection = SelectionInfo {
            start: BufferPoint::new(6, 1),
            end: BufferPoint::new(2, 3),
            pivot: BufferPoint::new(6, 1),
            block_selection: true,
            active: true,
        };

        assert_eq!(
            selection_spans(&buffer, &selection),
            [
                SelectionSpan::new(BufferPoint::new(2, 1), BufferPoint::new(6, 1)),
                SelectionSpan::new(BufferPoint::new(2, 2), BufferPoint::new(6, 2)),
                SelectionSpan::new(BufferPoint::new(2, 3), BufferPoint::new(6, 3)),
            ]
        );
    }

    #[test]
    fn microsoft_selection_wide_glyph_block_spans_expand_per_row() {
        let mut buffer = buffer(100, 100);
        buffer
            .row_mut(10)
            .replace_glyph(4, 2, &[0xd83c, 0xdf2f])
            .expect("first wide glyph fits");
        buffer
            .row_mut(11)
            .replace_glyph(7, 2, &[0xd83c, 0xdf2f])
            .expect("second wide glyph fits");

        let selection = SelectionInfo {
            start: BufferPoint::new(5, 8),
            end: BufferPoint::new(8, 12),
            pivot: BufferPoint::new(5, 8),
            block_selection: true,
            active: true,
        };

        assert_eq!(
            selection_spans(&buffer, &selection),
            [
                SelectionSpan::new(BufferPoint::new(5, 8), BufferPoint::new(8, 8)),
                SelectionSpan::new(BufferPoint::new(5, 9), BufferPoint::new(8, 9)),
                SelectionSpan::new(BufferPoint::new(4, 10), BufferPoint::new(8, 10)),
                SelectionSpan::new(BufferPoint::new(5, 11), BufferPoint::new(9, 11)),
                SelectionSpan::new(BufferPoint::new(5, 12), BufferPoint::new(8, 12)),
            ]
        );
    }

    #[test]
    fn start_rendering_moves_left_and_becomes_viewport_relative() {
        let buffer = buffer(8, 5);
        let selection = SelectionInfo {
            start: BufferPoint::new(4, 3),
            end: BufferPoint::new(6, 3),
            pivot: BufferPoint::new(4, 3),
            block_selection: false,
            active: true,
        };

        assert_eq!(
            selection_start_for_rendering(&buffer, &selection, 2),
            BufferPoint::new(3, 1)
        );
    }

    #[test]
    fn start_rendering_repairs_trailing_wide_cell_before_marker_offset() {
        let mut buffer = buffer(8, 2);
        buffer
            .row_mut(0)
            .replace_glyph(2, 2, &[0x4e00])
            .expect("wide glyph fits");
        let selection = SelectionInfo {
            start: BufferPoint::new(3, 0),
            end: BufferPoint::new(5, 0),
            pivot: BufferPoint::new(3, 0),
            block_selection: false,
            active: true,
        };

        assert_eq!(
            selection_start_for_rendering(&buffer, &selection, 0),
            BufferPoint::new(1, 0)
        );
    }

    #[test]
    fn end_rendering_moves_right_boundary_inside_row() {
        let buffer = buffer(8, 2);
        let selection = SelectionInfo {
            start: BufferPoint::new(2, 0),
            end: BufferPoint::new(8, 0),
            pivot: BufferPoint::new(2, 0),
            block_selection: false,
            active: true,
        };

        assert_eq!(
            selection_end_for_rendering(&buffer, &selection, 0),
            BufferPoint::new(7, 0)
        );
    }

    #[test]
    fn end_rendering_advances_off_trailing_wide_cell() {
        let mut buffer = buffer(8, 2);
        buffer
            .row_mut(0)
            .replace_glyph(2, 2, &[0x4e00])
            .expect("wide glyph fits");
        let selection = SelectionInfo {
            start: BufferPoint::new(1, 0),
            end: BufferPoint::new(3, 0),
            pivot: BufferPoint::new(1, 0),
            block_selection: false,
            active: true,
        };

        assert_eq!(
            selection_end_for_rendering(&buffer, &selection, 0),
            BufferPoint::new(4, 0)
        );
    }
}
