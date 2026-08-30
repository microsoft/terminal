//! Legacy conhost selection geometry mapped onto the migrated `TerminalCore` owner.
//!
//! `src/host/selection.cpp` stores an inclusive selection rectangle plus the
//! corner where the drag began. `TerminalCore` already owns half-open selection
//! spans; this module preserves the small compatibility transformation between
//! those representations and the historical Ctrl+Shift word-stepping behavior.

use crate::selection::{BufferPoint, SelectionInfo};
use crate::selection_rendering::{SelectionSpan, selection_spans};
use terminal_buffer::geometry::InclusiveRect;
use terminal_buffer::row::DelimiterClass;
use terminal_buffer::text_buffer::TextBuffer;

/// Reproduces `Selection::_RegenerateSelectionSpans` for the legacy host state.
///
/// Effective line selection is XOR between the configured line mode and the
/// temporary alternate-selection modifier. Conhost stores inclusive corners,
/// so the physically-later endpoint is advanced once before `TerminalCore`'s
/// half-open span owner is invoked.
#[must_use]
pub fn legacy_selection_spans(
    buffer: &TextBuffer,
    rect: InclusiveRect,
    anchor: BufferPoint,
    line_selection: bool,
    alternate_selection: bool,
) -> Vec<SelectionSpan> {
    let opposite = BufferPoint::new(
        if anchor.x == rect.left {
            rect.right
        } else {
            rect.left
        },
        if anchor.y == rect.top {
            rect.bottom
        } else {
            rect.top
        },
    );
    let block_selection = !(line_selection ^ alternate_selection);

    let mut start = anchor;
    let mut end = opposite;
    if block_selection {
        if start.x <= end.x {
            increment_in_bounds(buffer, &mut end);
        } else {
            increment_in_bounds(buffer, &mut start);
        }
    } else if start <= end {
        increment_in_bounds(buffer, &mut end);
    } else {
        increment_in_bounds(buffer, &mut start);
    }

    selection_spans(
        buffer,
        &SelectionInfo {
            start,
            end,
            pivot: anchor,
            block_selection,
            active: true,
        },
    )
}

/// Reproduces `Selection::WordByWordSelection` when cooked-read boundaries are
/// unavailable, which is the state exercised by Microsoft's `SelectionInputTests`.
/// Whitespace/control cells are delimiters; ordinary punctuation remains part
/// of the word exactly as in the source vector (`text.`).
#[must_use]
pub fn legacy_word_by_word_selection(
    buffer: &TextBuffer,
    anchor: BufferPoint,
    point: BufferPoint,
    reverse: bool,
) -> BufferPoint {
    let mut out = clamp_in_bounds(buffer, point);
    if reverse {
        decrement_in_bounds(buffer, &mut out);
    } else {
        increment_in_bounds(buffer, &mut out);
    }

    let mut current_is_delimiter = is_word_delimiter(buffer, out);
    let unhighlighting = if reverse { out > anchor } else { out < anchor };
    let max_left = BufferPoint::new(0, 0);
    let max_right = BufferPoint::new(
        i32::from(buffer.width()).saturating_sub(1),
        i32::from(buffer.height()).saturating_sub(1),
    );
    let mut move_succeeded = false;

    loop {
        let previous_is_delimiter = current_is_delimiter;
        if out == max_left || out >= max_right {
            move_succeeded = false;
            break;
        }

        move_succeeded = if reverse {
            decrement_in_bounds(buffer, &mut out)
        } else {
            increment_in_bounds(buffer, &mut out)
        };
        if !move_succeeded {
            break;
        }

        current_is_delimiter = is_word_delimiter(buffer, out);
        let reached_transition = if reverse {
            !previous_is_delimiter && current_is_delimiter
        } else {
            previous_is_delimiter && !current_is_delimiter
        };
        if reached_transition {
            break;
        }
    }

    if move_succeeded && !unhighlighting {
        if reverse {
            increment_in_bounds(buffer, &mut out);
        } else {
            decrement_in_bounds(buffer, &mut out);
        }
    }
    out
}

fn is_word_delimiter(buffer: &TextBuffer, point: BufferPoint) -> bool {
    buffer.row(point.y).delimiter_class_at(point.x, &[]) != DelimiterClass::RegularChar
}

fn clamp_in_bounds(buffer: &TextBuffer, point: BufferPoint) -> BufferPoint {
    BufferPoint::new(
        point
            .x
            .clamp(0, i32::from(buffer.width()).saturating_sub(1)),
        point
            .y
            .clamp(0, i32::from(buffer.height()).saturating_sub(1)),
    )
}

fn increment_in_bounds(buffer: &TextBuffer, point: &mut BufferPoint) -> bool {
    let width = i32::from(buffer.width());
    let height = i32::from(buffer.height());
    *point = clamp_in_bounds(buffer, *point);
    if point.x + 1 < width {
        point.x += 1;
        true
    } else if point.y + 1 < height {
        point.x = 0;
        point.y += 1;
        true
    } else {
        false
    }
}

fn decrement_in_bounds(buffer: &TextBuffer, point: &mut BufferPoint) -> bool {
    let width = i32::from(buffer.width());
    *point = clamp_in_bounds(buffer, *point);
    if point.x > 0 {
        point.x -= 1;
        true
    } else if point.y > 0 {
        point.y -= 1;
        point.x = width.saturating_sub(1);
        true
    } else {
        false
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use terminal_buffer::text_attribute::TextAttribute;

    fn buffer(width: u16, height: u16) -> TextBuffer {
        TextBuffer::new(width, height, TextAttribute::default()).expect("valid fixture")
    }

    fn write_ascii(buffer: &mut TextBuffer, text: &str) {
        for (x, byte) in text.bytes().enumerate() {
            buffer
                .row_mut(0)
                .replace_glyph(i32::try_from(x).unwrap(), 1, &[u16::from(byte)])
                .expect("source text fits fixture");
        }
    }

    #[test]
    fn microsoft_host_selection_get_spans_box_mode_matches_all_source_configurations() {
        let buffer = buffer(80, 25);
        let rect = InclusiveRect::new(1, 0, 10, 3);
        let expected = (0..=3)
            .map(|y| SelectionSpan::new(BufferPoint::new(1, y), BufferPoint::new(11, y)))
            .collect::<Vec<_>>();

        for (anchor, line, alternate) in [
            (BufferPoint::new(1, 0), false, false),
            (BufferPoint::new(1, 0), true, true),
            (BufferPoint::new(10, 0), true, true),
            (BufferPoint::new(1, 3), true, true),
            (BufferPoint::new(10, 3), true, true),
        ] {
            assert_eq!(
                legacy_selection_spans(&buffer, rect, anchor, line, alternate),
                expected
            );
        }
    }

    #[test]
    fn microsoft_host_selection_get_spans_line_mode_matches_all_source_configurations() {
        let buffer = buffer(80, 25);
        let rect = InclusiveRect::new(1, 0, 10, 3);

        for (anchor, line, alternate, expected) in [
            (
                BufferPoint::new(1, 0),
                true,
                false,
                SelectionSpan::new(BufferPoint::new(1, 0), BufferPoint::new(11, 3)),
            ),
            (
                BufferPoint::new(1, 0),
                false,
                true,
                SelectionSpan::new(BufferPoint::new(1, 0), BufferPoint::new(11, 3)),
            ),
            (
                BufferPoint::new(10, 0),
                false,
                true,
                SelectionSpan::new(BufferPoint::new(10, 0), BufferPoint::new(2, 3)),
            ),
            (
                BufferPoint::new(1, 3),
                false,
                true,
                SelectionSpan::new(BufferPoint::new(10, 0), BufferPoint::new(2, 3)),
            ),
            (
                BufferPoint::new(10, 3),
                false,
                true,
                SelectionSpan::new(BufferPoint::new(1, 0), BufferPoint::new(11, 3)),
            ),
        ] {
            assert_eq!(
                legacy_selection_spans(&buffer, rect, anchor, line, alternate),
                vec![expected]
            );
        }

        let single = InclusiveRect::new(1, 2, 10, 2);
        for anchor in [BufferPoint::new(1, 2), BufferPoint::new(10, 2)] {
            assert_eq!(
                legacy_selection_spans(&buffer, single, anchor, true, false),
                vec![SelectionSpan::new(
                    BufferPoint::new(1, 2),
                    BufferPoint::new(11, 2)
                )]
            );
        }
    }

    #[test]
    fn microsoft_host_selection_word_by_word_previous_matches_complete_walk() {
        let text = "this is some test text.";
        let mut buffer = buffer(80, 25);
        write_ascii(&mut buffer, text);
        let anchor = BufferPoint::new(i32::try_from(text.len()).unwrap(), 0);
        let mut point = anchor;
        let mut actual = Vec::new();
        while point.x > 0 {
            point = legacy_word_by_word_selection(&buffer, anchor, point, true);
            actual.push(point);
        }
        assert_eq!(
            actual,
            [
                BufferPoint::new(18, 0),
                BufferPoint::new(13, 0),
                BufferPoint::new(8, 0),
                BufferPoint::new(5, 0),
                BufferPoint::new(0, 0),
            ]
        );
    }

    #[test]
    fn microsoft_host_selection_word_by_word_next_matches_complete_walk_to_buffer_end() {
        let text = "this is some test text.";
        let mut buffer = buffer(80, 25);
        write_ascii(&mut buffer, text);
        let anchor = BufferPoint::new(0, 0);
        let mut point = anchor;
        let mut actual = Vec::new();
        while point.y < i32::from(buffer.height()).saturating_sub(1) {
            point = legacy_word_by_word_selection(&buffer, anchor, point, false);
            actual.push(point);
        }
        assert_eq!(
            &actual[..5],
            &[
                BufferPoint::new(4, 0),
                BufferPoint::new(7, 0),
                BufferPoint::new(12, 0),
                BufferPoint::new(17, 0),
                BufferPoint::new(79, 24),
            ]
        );
        assert_eq!(actual.len(), 5);
    }
}
