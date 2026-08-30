//! Deterministic keyboard-driven selection movement from `TerminalCore`.
//!
//! This module ports `Terminal::UpdateSelection` and its movement helpers while
//! keeping viewport geometry explicit and platform-neutral.

use crate::control_key_states::ControlKeyStates;
use crate::keyboard_selection::{KeyboardSelectionExpansion, SelectionDirection};
use crate::selection::{BufferPoint, EndpointState, SelectionEndpoint, SelectionInfo};
use terminal_buffer::row::DelimiterClass;
use terminal_buffer::text_buffer::TextBuffer;

/// Geometry needed by keyboard selection movement.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct SelectionViewport {
    pub height: i32,
    pub mutable_bottom: i32,
}

impl SelectionViewport {
    #[must_use]
    pub const fn new(height: i32, mutable_bottom: i32) -> Self {
        Self {
            height,
            mutable_bottom,
        }
    }
}

/// Result metadata from one `UpdateSelection` transition.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct UpdateSelectionResult {
    pub target: BufferPoint,
    pub endpoint: SelectionEndpoint,
    pub moved_both_endpoints: bool,
}

/// Ports `Terminal::UpdateSelection` as a deterministic state transition.
///
/// In Mark Mode without Shift, both endpoints and the pivot move together until
/// `SwitchSelectionEndpoint` anchors one side. Otherwise, only the endpoint on
/// the non-pivot side moves, and crossing the pivot retargets the opposite side.
pub fn update_selection(
    buffer: &TextBuffer,
    selection: &mut SelectionInfo,
    endpoint_state: &mut EndpointState,
    mark_mode: bool,
    direction: SelectionDirection,
    expansion: KeyboardSelectionExpansion,
    mods: ControlKeyStates,
    viewport: SelectionViewport,
    word_delimiters: &[u16],
) -> UpdateSelectionResult {
    let move_both =
        mark_mode && !endpoint_state.anchor_inactive_endpoint && !mods.is_shift_pressed();

    endpoint_state.target = if move_both {
        SelectionEndpoint::Both
    } else if selection.start == selection.pivot {
        SelectionEndpoint::End
    } else if selection.end == selection.pivot {
        SelectionEndpoint::Start
    } else {
        SelectionEndpoint::End
    };

    let mut target = if matches!(endpoint_state.target, SelectionEndpoint::Start) {
        selection.start
    } else {
        selection.end
    };

    target = match expansion {
        KeyboardSelectionExpansion::Char => move_by_char(buffer, target, direction, viewport),
        KeyboardSelectionExpansion::Word => {
            move_by_word(buffer, target, direction, viewport, word_delimiters)
        }
        KeyboardSelectionExpansion::Viewport => {
            move_by_viewport(buffer, target, direction, viewport)
        }
        KeyboardSelectionExpansion::Buffer => move_by_buffer(buffer, direction, viewport),
    };

    if move_both {
        selection.start = target;
        selection.end = target;
        selection.pivot = target;
        endpoint_state.target = SelectionEndpoint::Both;
    } else {
        let pivoted = selection.pivot_selection(target);
        selection.start = pivoted.start;
        selection.end = pivoted.end;
        endpoint_state.target = if pivoted.target_start {
            SelectionEndpoint::Start
        } else {
            SelectionEndpoint::End
        };
    }

    UpdateSelectionResult {
        target,
        endpoint: endpoint_state.target,
        moved_both_endpoints: move_both,
    }
}

#[must_use]
fn move_by_char(
    buffer: &TextBuffer,
    pos: BufferPoint,
    direction: SelectionDirection,
    viewport: SelectionViewport,
) -> BufferPoint {
    let width = i32::from(buffer.width());
    let bottom = viewport
        .mutable_bottom
        .clamp(0, i32::from(buffer.height()).saturating_sub(1));

    match direction {
        SelectionDirection::Left => previous_glyph(buffer, pos),
        SelectionDirection::Right => next_glyph(buffer, pos, bottom),
        SelectionDirection::Up => {
            let new_y = pos.y.saturating_sub(1);
            if pos.y <= 0 {
                BufferPoint::new(0, 0)
            } else {
                BufferPoint::new(pos.x.clamp(0, width), new_y)
            }
        }
        SelectionDirection::Down => {
            let new_y = pos.y.saturating_add(1);
            if new_y > bottom {
                BufferPoint::new(width, bottom)
            } else {
                BufferPoint::new(pos.x.clamp(0, width), new_y)
            }
        }
    }
}

#[must_use]
fn move_by_word(
    buffer: &TextBuffer,
    pos: BufferPoint,
    direction: SelectionDirection,
    viewport: SelectionViewport,
    delimiters: &[u16],
) -> BufferPoint {
    match direction {
        SelectionDirection::Left => {
            let mut next = word_start(buffer, pos, delimiters);
            if next == pos {
                next = previous_cell(buffer, next);
                next = word_start(buffer, next, delimiters);
            }
            next
        }
        SelectionDirection::Right => {
            let mut next = word_end(buffer, pos, delimiters, viewport.mutable_bottom);
            if next == pos {
                next = next_cell(buffer, next, viewport.mutable_bottom);
                next = word_end(buffer, next, delimiters, viewport.mutable_bottom);
            }
            next
        }
        SelectionDirection::Up => {
            let moved = move_by_char(buffer, pos, direction, viewport);
            word_start(buffer, moved, delimiters)
        }
        SelectionDirection::Down => {
            let moved = move_by_char(buffer, pos, direction, viewport);
            word_end(buffer, moved, delimiters, viewport.mutable_bottom)
        }
    }
}

#[must_use]
fn move_by_viewport(
    buffer: &TextBuffer,
    pos: BufferPoint,
    direction: SelectionDirection,
    viewport: SelectionViewport,
) -> BufferPoint {
    let width = i32::from(buffer.width());
    let bottom = viewport
        .mutable_bottom
        .clamp(0, i32::from(buffer.height()).saturating_sub(1));
    let height = viewport.height.max(1);

    match direction {
        SelectionDirection::Left => BufferPoint::new(0, pos.y.clamp(0, bottom)),
        SelectionDirection::Right => BufferPoint::new(width, pos.y.clamp(0, bottom)),
        SelectionDirection::Up => {
            let new_y = pos.y.saturating_sub(height);
            if pos.y < height {
                BufferPoint::new(0, 0)
            } else {
                BufferPoint::new(pos.x.clamp(0, width), new_y)
            }
        }
        SelectionDirection::Down => {
            let new_y = pos.y.saturating_add(height);
            if new_y > bottom {
                BufferPoint::new(width, bottom)
            } else {
                BufferPoint::new(pos.x.clamp(0, width), new_y)
            }
        }
    }
}

#[must_use]
fn move_by_buffer(
    buffer: &TextBuffer,
    direction: SelectionDirection,
    viewport: SelectionViewport,
) -> BufferPoint {
    match direction {
        SelectionDirection::Left | SelectionDirection::Up => BufferPoint::new(0, 0),
        SelectionDirection::Right | SelectionDirection::Down => BufferPoint::new(
            i32::from(buffer.width()),
            viewport
                .mutable_bottom
                .clamp(0, i32::from(buffer.height()).saturating_sub(1)),
        ),
    }
}

#[must_use]
fn previous_glyph(buffer: &TextBuffer, pos: BufferPoint) -> BufferPoint {
    let width = i32::from(buffer.width());
    let y = pos.y.clamp(0, i32::from(buffer.height()).saturating_sub(1));
    if pos.x > 0 {
        let row = buffer.row(y);
        return BufferPoint::new(i32::from(row.navigate_to_previous(pos.x.min(width))), y);
    }
    if y == 0 {
        return BufferPoint::new(0, 0);
    }
    let previous_y = y - 1;
    let row = buffer.row(previous_y);
    let last = i32::from(row.readable_column_count()).saturating_sub(1);
    BufferPoint::new(i32::from(row.adjust_to_glyph_start(last)), previous_y)
}

#[must_use]
fn next_glyph(buffer: &TextBuffer, pos: BufferPoint, mutable_bottom: i32) -> BufferPoint {
    let width = i32::from(buffer.width());
    let bottom = mutable_bottom.clamp(0, i32::from(buffer.height()).saturating_sub(1));
    let y = pos.y.clamp(0, bottom);
    if pos.x >= width {
        return if y < bottom {
            BufferPoint::new(0, y + 1)
        } else {
            BufferPoint::new(width, bottom)
        };
    }

    let row = buffer.row(y);
    let next_x = i32::from(row.navigate_to_next(pos.x));
    if next_x < width {
        BufferPoint::new(next_x, y)
    } else if y < bottom {
        BufferPoint::new(0, y + 1)
    } else {
        BufferPoint::new(width, bottom)
    }
}

#[must_use]
fn previous_cell(buffer: &TextBuffer, pos: BufferPoint) -> BufferPoint {
    let width = i32::from(buffer.width());
    if pos.x > 0 {
        BufferPoint::new(pos.x - 1, pos.y)
    } else if pos.y > 0 {
        BufferPoint::new(width.saturating_sub(1), pos.y - 1)
    } else {
        BufferPoint::new(0, 0)
    }
}

#[must_use]
fn next_cell(buffer: &TextBuffer, pos: BufferPoint, mutable_bottom: i32) -> BufferPoint {
    let width = i32::from(buffer.width());
    let bottom = mutable_bottom.clamp(0, i32::from(buffer.height()).saturating_sub(1));
    if pos.x < width {
        BufferPoint::new(pos.x + 1, pos.y.clamp(0, bottom))
    } else if pos.y < bottom {
        BufferPoint::new(0, pos.y + 1)
    } else {
        BufferPoint::new(width, bottom)
    }
}

#[must_use]
fn word_start(buffer: &TextBuffer, pos: BufferPoint, delimiters: &[u16]) -> BufferPoint {
    let mut current = glyph_start(buffer, data_point(buffer, pos));
    let class = delimiter_class(buffer, current, delimiters);

    loop {
        let previous = previous_glyph(buffer, current);
        if previous == current || delimiter_class(buffer, previous, delimiters) != class {
            break;
        }
        current = previous;
    }
    current
}

#[must_use]
fn word_end(
    buffer: &TextBuffer,
    pos: BufferPoint,
    delimiters: &[u16],
    mutable_bottom: i32,
) -> BufferPoint {
    let mut current = glyph_start(buffer, data_point(buffer, pos));
    let class = delimiter_class(buffer, current, delimiters);

    loop {
        let next = next_glyph(buffer, current, mutable_bottom);
        if next == current
            || next.x == i32::from(buffer.width())
            || delimiter_class(buffer, next, delimiters) != class
        {
            break;
        }
        current = next;
    }

    let row = buffer.row(current.y);
    BufferPoint::new(
        i32::from(row.adjust_to_glyph_end(current.x.saturating_add(1))),
        current.y,
    )
}

#[must_use]
fn data_point(buffer: &TextBuffer, pos: BufferPoint) -> BufferPoint {
    BufferPoint::new(
        pos.x.clamp(0, i32::from(buffer.width()).saturating_sub(1)),
        pos.y.clamp(0, i32::from(buffer.height()).saturating_sub(1)),
    )
}

#[must_use]
fn glyph_start(buffer: &TextBuffer, pos: BufferPoint) -> BufferPoint {
    BufferPoint::new(
        i32::from(buffer.row(pos.y).adjust_to_glyph_start(pos.x)),
        pos.y,
    )
}

#[must_use]
fn delimiter_class(buffer: &TextBuffer, pos: BufferPoint, delimiters: &[u16]) -> DelimiterClass {
    buffer.row(pos.y).delimiter_class_at(pos.x, delimiters)
}

#[cfg(test)]
mod tests {
    use super::*;
    use terminal_buffer::text_attribute::TextAttribute;

    fn buffer(width: u16, height: u16) -> TextBuffer {
        TextBuffer::new(width, height, TextAttribute::default()).expect("valid test buffer")
    }

    #[test]
    fn mark_mode_without_shift_moves_all_three_points() {
        let buffer = buffer(10, 5);
        let mut selection = SelectionInfo::anchored(BufferPoint::new(2, 2));
        let mut endpoint = EndpointState::default();

        let result = update_selection(
            &buffer,
            &mut selection,
            &mut endpoint,
            true,
            SelectionDirection::Right,
            KeyboardSelectionExpansion::Char,
            ControlKeyStates::default(),
            SelectionViewport::new(3, 4),
            &[],
        );

        assert!(result.moved_both_endpoints);
        assert_eq!(selection.start, BufferPoint::new(3, 2));
        assert_eq!(selection.end, selection.start);
        assert_eq!(selection.pivot, selection.start);
        assert_eq!(endpoint.target, SelectionEndpoint::Both);
    }

    #[test]
    fn shift_moves_only_non_pivot_endpoint_and_retargets_when_crossing() {
        let buffer = buffer(10, 5);
        let mut selection = SelectionInfo {
            start: BufferPoint::new(2, 2),
            end: BufferPoint::new(5, 2),
            pivot: BufferPoint::new(2, 2),
            block_selection: false,
            active: true,
        };
        let mut endpoint = EndpointState::default();

        update_selection(
            &buffer,
            &mut selection,
            &mut endpoint,
            true,
            SelectionDirection::Left,
            KeyboardSelectionExpansion::Buffer,
            ControlKeyStates::SHIFT_PRESSED,
            SelectionViewport::new(3, 4),
            &[],
        );

        assert_eq!(selection.start, BufferPoint::new(0, 0));
        assert_eq!(selection.end, BufferPoint::new(2, 2));
        assert_eq!(endpoint.target, SelectionEndpoint::Start);
    }

    #[test]
    fn viewport_edges_match_home_end_page_semantics() {
        let buffer = buffer(10, 8);
        let viewport = SelectionViewport::new(3, 6);

        assert_eq!(
            move_by_viewport(
                &buffer,
                BufferPoint::new(4, 4),
                SelectionDirection::Left,
                viewport
            ),
            BufferPoint::new(0, 4)
        );
        assert_eq!(
            move_by_viewport(
                &buffer,
                BufferPoint::new(4, 4),
                SelectionDirection::Right,
                viewport
            ),
            BufferPoint::new(10, 4)
        );
        assert_eq!(
            move_by_viewport(
                &buffer,
                BufferPoint::new(4, 1),
                SelectionDirection::Up,
                viewport
            ),
            BufferPoint::new(0, 0)
        );
        assert_eq!(
            move_by_viewport(
                &buffer,
                BufferPoint::new(4, 5),
                SelectionDirection::Down,
                viewport
            ),
            BufferPoint::new(10, 6)
        );
    }

    #[test]
    fn buffer_movement_uses_origin_and_mutable_bottom_right_exclusive() {
        let buffer = buffer(10, 8);
        let viewport = SelectionViewport::new(3, 6);

        assert_eq!(
            move_by_buffer(&buffer, SelectionDirection::Left, viewport),
            BufferPoint::new(0, 0)
        );
        assert_eq!(
            move_by_buffer(&buffer, SelectionDirection::Right, viewport),
            BufferPoint::new(10, 6)
        );
    }

    #[test]
    fn vertical_char_movement_saturates_to_terminal_core_edges() {
        let buffer = buffer(10, 5);
        let viewport = SelectionViewport::new(3, 3);

        assert_eq!(
            move_by_char(
                &buffer,
                BufferPoint::new(4, 0),
                SelectionDirection::Up,
                viewport
            ),
            BufferPoint::new(0, 0)
        );
        assert_eq!(
            move_by_char(
                &buffer,
                BufferPoint::new(4, 3),
                SelectionDirection::Down,
                viewport
            ),
            BufferPoint::new(10, 3)
        );
    }
}
