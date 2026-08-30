//! Platform-neutral selection behavior from `TerminalCore`.
//!
//! This module keeps selection geometry and endpoint transitions in safe Rust,
//! while reusing the migrated R04 `TextBuffer` for word/line expansion.

use terminal_buffer::row::DelimiterClass;
use terminal_buffer::text_buffer::TextBuffer;

/// A cell position in the terminal text buffer.
///
/// Ordering is row-major, matching `til::point` comparisons used by `TerminalCore`:
/// rows compare first, then columns within a row.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct BufferPoint {
    pub x: i32,
    pub y: i32,
}

impl BufferPoint {
    #[must_use]
    pub const fn new(x: i32, y: i32) -> Self {
        Self { x, y }
    }
}

impl Ord for BufferPoint {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.y.cmp(&other.y).then_with(|| self.x.cmp(&other.x))
    }
}

impl PartialOrd for BufferPoint {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

/// The selection expansion policy selected by mouse/keyboard interaction.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum SelectionExpansion {
    #[default]
    Char,
    Word,
    Line,
}

/// Which endpoint Mark Mode currently moves.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum SelectionEndpoint {
    Start,
    End,
    #[default]
    Both,
}

/// Interaction mode associated with the active selection.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum SelectionInteractionMode {
    #[default]
    None,
    Mouse,
    Mark,
}

/// Mutable `TerminalCore` selection state.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct SelectionInfo {
    pub start: BufferPoint,
    pub end: BufferPoint,
    pub pivot: BufferPoint,
    pub block_selection: bool,
    pub active: bool,
}

impl SelectionInfo {
    /// Starts a character selection at `point`, preserving the pivot until a new
    /// selection is created.
    #[must_use]
    pub const fn anchored(point: BufferPoint) -> Self {
        Self {
            start: point,
            end: point,
            pivot: point,
            block_selection: false,
            active: true,
        }
    }

    /// Returns ordered anchors around the immutable pivot and tells the caller
    /// whether the moving target is the start endpoint.
    ///
    /// This is the safe Rust equivalent of `Terminal::_PivotSelection`.
    #[must_use]
    pub fn pivot_selection(&self, target: BufferPoint) -> PivotedSelection {
        if target <= self.pivot {
            PivotedSelection {
                start: target,
                end: self.pivot,
                target_start: true,
            }
        } else {
            PivotedSelection {
                start: self.pivot,
                end: target,
                target_start: false,
            }
        }
    }

    pub fn set_block_selection(&mut self, enabled: bool) {
        self.block_selection = enabled;
    }

    pub fn clear(&mut self) {
        self.active = false;
    }
}

/// Result of pivoting a drag target around the selection pivot.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PivotedSelection {
    pub start: BufferPoint,
    pub end: BufferPoint,
    pub target_start: bool,
}

/// Stateful selection expansion and interaction mode from `TerminalCore`.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct SelectionState {
    pub selection: SelectionInfo,
    pub expansion: SelectionExpansion,
    pub interaction: SelectionInteractionMode,
}

impl SelectionState {
    /// Starts a new character selection at a buffer position.
    pub fn set_anchor(&mut self, point: BufferPoint) {
        self.selection = SelectionInfo::anchored(point);
        self.expansion = SelectionExpansion::Char;
        self.set_end(point, None, None, &[]);
        self.selection.start = self.selection.pivot;
    }

    /// Starts a multi-click selection and expands both anchors according to the
    /// requested mode. The pivot is restored to the expanded start afterwards,
    /// matching `Terminal::MultiClickSelection` for future Shift+Click actions.
    pub fn multi_click(
        &mut self,
        buffer: &TextBuffer,
        point: BufferPoint,
        expansion: SelectionExpansion,
        word_delimiters: &[u16],
    ) {
        self.selection.pivot = clamp_point(buffer, point);
        self.selection.active = true;
        self.expansion = expansion;
        self.set_end(point, Some(buffer), None, word_delimiters);
        self.selection.pivot = self.selection.start;
    }

    /// Updates the moving selection endpoint.
    ///
    /// Supplying `shift_click_expansion` mirrors the C++ `newExpansionMode`
    /// argument: only the moving side is expanded and the opposite side is reset
    /// to the immutable pivot. A forward character Shift+Click first advances one
    /// cell so that the clicked cell is included.
    pub fn set_end(
        &mut self,
        mut target: BufferPoint,
        buffer: Option<&TextBuffer>,
        shift_click_expansion: Option<SelectionExpansion>,
        word_delimiters: &[u16],
    ) {
        if !self.selection.active {
            return;
        }

        if let Some(buffer) = buffer {
            target = clamp_point(buffer, target);
            if matches!(shift_click_expansion, Some(SelectionExpansion::Char))
                && target >= self.selection.pivot
            {
                target = increment_in_exclusive_bounds(buffer, target);
            }
        }

        if let Some(expansion) = shift_click_expansion {
            self.expansion = expansion;
        }

        let pivoted = self.selection.pivot_selection(target);
        let expanded = if let Some(buffer) = buffer {
            expand_selection_anchors(
                buffer,
                pivoted.start,
                pivoted.end,
                self.selection.pivot,
                self.expansion,
                word_delimiters,
            )
        } else {
            (pivoted.start, pivoted.end)
        };

        if shift_click_expansion.is_some() {
            if pivoted.target_start {
                self.selection.start = expanded.0;
                self.selection.end = self.selection.pivot;
            } else {
                self.selection.end = expanded.1;
                self.selection.start = self.selection.pivot;
            }
        } else {
            self.selection.start = expanded.0;
            self.selection.end = expanded.1;
        }

        self.interaction = SelectionInteractionMode::Mouse;
    }
}

/// Expands ordered anchors according to the active multi-click mode.
#[must_use]
pub fn expand_selection_anchors(
    buffer: &TextBuffer,
    mut start: BufferPoint,
    mut end: BufferPoint,
    pivot: BufferPoint,
    expansion: SelectionExpansion,
    word_delimiters: &[u16],
) -> (BufferPoint, BufferPoint) {
    start = clamp_point(buffer, start);
    end = clamp_point(buffer, end);

    match expansion {
        SelectionExpansion::Line => {
            while start.y > 0 && buffer.row(start.y.saturating_sub(1)).was_wrap_forced() {
                start.y -= 1;
            }
            while end.y + 1 < i32::from(buffer.height()) && buffer.row(end.y).was_wrap_forced() {
                end.y += 1;
            }
            start.x = 0;
            end.x = i32::from(buffer.width());
        }
        SelectionExpansion::Word => {
            start = word_start(buffer, start, word_delimiters);

            // GH#5099 compatibility: while expanding to the right, back up by
            // one cell before asking for the word end so approaching the next
            // word does not prematurely select it.
            if end > pivot {
                end = decrement_in_exclusive_bounds(buffer, end);
            }
            end = word_end(buffer, end, word_delimiters);
        }
        SelectionExpansion::Char => {}
    }

    (start, end)
}

#[must_use]
fn word_start(buffer: &TextBuffer, point: BufferPoint, delimiters: &[u16]) -> BufferPoint {
    let mut point = normalize_to_glyph_start(buffer, point);
    let class = delimiter_class(buffer, point, delimiters);

    while let Some(previous) = previous_glyph(buffer, point) {
        if delimiter_class(buffer, previous, delimiters) != class {
            break;
        }
        point = previous;
    }

    point
}

#[must_use]
fn word_end(buffer: &TextBuffer, point: BufferPoint, delimiters: &[u16]) -> BufferPoint {
    let mut point = normalize_to_glyph_start(buffer, point);
    let class = delimiter_class(buffer, point, delimiters);

    loop {
        let next = next_glyph(buffer, point);
        match next {
            Some(next) if delimiter_class(buffer, next, delimiters) == class => point = next,
            _ => break,
        }
    }

    glyph_end(buffer, point)
}

#[must_use]
fn delimiter_class(buffer: &TextBuffer, point: BufferPoint, delimiters: &[u16]) -> DelimiterClass {
    buffer.row(point.y).delimiter_class_at(point.x, delimiters)
}

#[must_use]
fn normalize_to_glyph_start(buffer: &TextBuffer, point: BufferPoint) -> BufferPoint {
    let point = clamp_point(buffer, point);
    BufferPoint::new(
        i32::from(buffer.row(point.y).adjust_to_glyph_start(point.x)),
        point.y,
    )
}

#[must_use]
fn glyph_end(buffer: &TextBuffer, point: BufferPoint) -> BufferPoint {
    let row = buffer.row(point.y);
    BufferPoint::new(
        i32::from(row.adjust_to_glyph_end(point.x.saturating_add(1))),
        point.y,
    )
}

#[must_use]
fn previous_glyph(buffer: &TextBuffer, point: BufferPoint) -> Option<BufferPoint> {
    if point.x > 0 {
        return Some(BufferPoint::new(
            i32::from(buffer.row(point.y).navigate_to_previous(point.x)),
            point.y,
        ));
    }

    if point.y == 0 || !buffer.row(point.y - 1).was_wrap_forced() {
        return None;
    }

    let previous_y = point.y - 1;
    let previous_row = buffer.row(previous_y);
    let last = i32::from(previous_row.readable_column_count()).saturating_sub(1);
    Some(BufferPoint::new(
        i32::from(previous_row.adjust_to_glyph_start(last)),
        previous_y,
    ))
}

#[must_use]
fn next_glyph(buffer: &TextBuffer, point: BufferPoint) -> Option<BufferPoint> {
    let row = buffer.row(point.y);
    let next_x = i32::from(row.navigate_to_next(point.x));
    if next_x < i32::from(row.readable_column_count()) {
        return Some(BufferPoint::new(next_x, point.y));
    }

    if !row.was_wrap_forced() || point.y + 1 >= i32::from(buffer.height()) {
        return None;
    }

    Some(BufferPoint::new(0, point.y + 1))
}

#[must_use]
fn clamp_point(buffer: &TextBuffer, point: BufferPoint) -> BufferPoint {
    BufferPoint::new(
        point
            .x
            .clamp(0, i32::from(buffer.width()).saturating_sub(1)),
        point
            .y
            .clamp(0, i32::from(buffer.height()).saturating_sub(1)),
    )
}

#[must_use]
fn increment_in_exclusive_bounds(buffer: &TextBuffer, point: BufferPoint) -> BufferPoint {
    let width = i32::from(buffer.width());
    let height = i32::from(buffer.height());
    let mut point = clamp_point(buffer, point);

    point.x += 1;
    if point.x >= width && point.y + 1 < height {
        point.x = 0;
        point.y += 1;
    }
    point
}

#[must_use]
fn decrement_in_exclusive_bounds(buffer: &TextBuffer, point: BufferPoint) -> BufferPoint {
    let width = i32::from(buffer.width());
    let mut point = point;

    if point.x > 0 {
        point.x -= 1;
    } else if point.y > 0 {
        point.y -= 1;
        point.x = width.saturating_sub(1);
    }
    clamp_point(buffer, point)
}

/// Deterministic endpoint switching used by Mark Mode.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct EndpointState {
    pub target: SelectionEndpoint,
    pub anchor_inactive_endpoint: bool,
}

impl EndpointState {
    /// Mirrors `Terminal::SwitchSelectionEndpoint` while leaving pivot mutation
    /// explicit and testable.
    pub fn switch(&mut self, selection: &mut SelectionInfo) {
        if !selection.active {
            return;
        }

        match self.target {
            SelectionEndpoint::Both => {
                self.target = SelectionEndpoint::End;
                self.anchor_inactive_endpoint = true;
            }
            SelectionEndpoint::End => {
                self.target = SelectionEndpoint::Start;
                selection.pivot = selection.end;
            }
            SelectionEndpoint::Start => {
                self.target = SelectionEndpoint::End;
                selection.pivot = selection.start;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use terminal_buffer::text_attribute::TextAttribute;

    fn buffer(width: u16, height: u16) -> TextBuffer {
        TextBuffer::new(width, height, TextAttribute::default()).expect("valid test buffer")
    }

    fn write_ascii(buffer: &mut TextBuffer, y: i32, text: &[u8]) {
        for (x, byte) in text.iter().copied().enumerate() {
            buffer
                .row_mut(y)
                .replace_glyph(
                    i32::try_from(x).expect("small test column"),
                    1,
                    &[u16::from(byte)],
                )
                .expect("test glyph fits");
        }
    }

    #[test]
    fn buffer_points_are_row_major() {
        assert!(BufferPoint::new(99, 3) < BufferPoint::new(0, 4));
        assert!(BufferPoint::new(2, 4) < BufferPoint::new(3, 4));
    }

    #[test]
    fn pivot_selection_keeps_pivot_selected_when_drag_crosses_it() {
        let selection = SelectionInfo::anchored(BufferPoint::new(5, 2));

        let forward = selection.pivot_selection(BufferPoint::new(9, 2));
        assert_eq!(forward.start, BufferPoint::new(5, 2));
        assert_eq!(forward.end, BufferPoint::new(9, 2));
        assert!(!forward.target_start);

        let backward = selection.pivot_selection(BufferPoint::new(1, 1));
        assert_eq!(backward.start, BufferPoint::new(1, 1));
        assert_eq!(backward.end, BufferPoint::new(5, 2));
        assert!(backward.target_start);
    }

    #[test]
    fn pivot_equality_targets_start_like_terminal_core() {
        let selection = SelectionInfo::anchored(BufferPoint::new(5, 2));
        let pivoted = selection.pivot_selection(selection.pivot);

        assert!(pivoted.target_start);
        assert_eq!(pivoted.start, selection.pivot);
        assert_eq!(pivoted.end, selection.pivot);
    }

    #[test]
    fn line_expansion_includes_entire_forced_wrap_chain() {
        let mut buffer = buffer(5, 4);
        buffer.row_mut(0).set_wrap_forced(true);
        buffer.row_mut(1).set_wrap_forced(true);
        buffer.row_mut(2).set_wrap_forced(false);

        let expanded = expand_selection_anchors(
            &buffer,
            BufferPoint::new(3, 1),
            BufferPoint::new(2, 1),
            BufferPoint::new(3, 1),
            SelectionExpansion::Line,
            &[],
        );

        assert_eq!(expanded.0, BufferPoint::new(0, 0));
        assert_eq!(expanded.1, BufferPoint::new(5, 2));
    }

    #[test]
    fn word_expansion_uses_r04_delimiter_classes() {
        let mut buffer = buffer(12, 1);
        write_ascii(&mut buffer, 0, b"one two.three");

        let expanded = expand_selection_anchors(
            &buffer,
            BufferPoint::new(5, 0),
            BufferPoint::new(5, 0),
            BufferPoint::new(5, 0),
            SelectionExpansion::Word,
            &[u16::from(b'.')],
        );

        assert_eq!(expanded.0, BufferPoint::new(4, 0));
        assert_eq!(expanded.1, BufferPoint::new(7, 0));
    }

    #[test]
    fn word_expansion_crosses_only_forced_wrap_boundaries() {
        let mut buffer = buffer(4, 2);
        write_ascii(&mut buffer, 0, b"abcd");
        write_ascii(&mut buffer, 1, b"efgh");
        buffer.row_mut(0).set_wrap_forced(true);

        let expanded = expand_selection_anchors(
            &buffer,
            BufferPoint::new(1, 1),
            BufferPoint::new(1, 1),
            BufferPoint::new(1, 1),
            SelectionExpansion::Word,
            &[],
        );

        assert_eq!(expanded.0, BufferPoint::new(0, 0));
        assert_eq!(expanded.1, BufferPoint::new(4, 1));
    }

    #[test]
    fn forward_character_shift_click_includes_clicked_cell() {
        let buffer = buffer(8, 1);
        let mut state = SelectionState {
            selection: SelectionInfo::anchored(BufferPoint::new(2, 0)),
            ..SelectionState::default()
        };

        state.set_end(
            BufferPoint::new(5, 0),
            Some(&buffer),
            Some(SelectionExpansion::Char),
            &[],
        );

        assert_eq!(state.selection.start, BufferPoint::new(2, 0));
        assert_eq!(state.selection.end, BufferPoint::new(6, 0));
        assert_eq!(state.interaction, SelectionInteractionMode::Mouse);
    }

    #[test]
    fn shift_click_only_expands_the_moving_word_side() {
        let mut buffer = buffer(12, 1);
        write_ascii(&mut buffer, 0, b"one two six ");
        let mut state = SelectionState {
            selection: SelectionInfo::anchored(BufferPoint::new(4, 0)),
            ..SelectionState::default()
        };

        state.set_end(
            BufferPoint::new(9, 0),
            Some(&buffer),
            Some(SelectionExpansion::Word),
            &[],
        );

        assert_eq!(state.selection.start, BufferPoint::new(4, 0));
        assert_eq!(state.selection.end, BufferPoint::new(11, 0));
    }

    #[test]
    fn switching_both_endpoints_targets_end_and_anchors_inactive_side() {
        let mut selection = SelectionInfo::anchored(BufferPoint::new(2, 3));
        let mut endpoints = EndpointState::default();

        endpoints.switch(&mut selection);

        assert_eq!(endpoints.target, SelectionEndpoint::End);
        assert!(endpoints.anchor_inactive_endpoint);
        assert_eq!(selection.pivot, BufferPoint::new(2, 3));
    }

    #[test]
    fn switching_end_to_start_pivots_on_end() {
        let mut selection = SelectionInfo {
            start: BufferPoint::new(1, 3),
            end: BufferPoint::new(7, 3),
            pivot: BufferPoint::new(1, 3),
            block_selection: false,
            active: true,
        };
        let mut endpoints = EndpointState {
            target: SelectionEndpoint::End,
            anchor_inactive_endpoint: false,
        };

        endpoints.switch(&mut selection);

        assert_eq!(endpoints.target, SelectionEndpoint::Start);
        assert_eq!(selection.pivot, selection.end);
    }

    #[test]
    fn switching_start_to_end_pivots_on_start() {
        let mut selection = SelectionInfo {
            start: BufferPoint::new(1, 3),
            end: BufferPoint::new(7, 3),
            pivot: BufferPoint::new(7, 3),
            block_selection: false,
            active: true,
        };
        let mut endpoints = EndpointState {
            target: SelectionEndpoint::Start,
            anchor_inactive_endpoint: false,
        };

        endpoints.switch(&mut selection);

        assert_eq!(endpoints.target, SelectionEndpoint::End);
        assert_eq!(selection.pivot, selection.start);
    }

    #[test]
    fn inactive_selection_does_not_switch_endpoints() {
        let mut selection = SelectionInfo::default();
        let mut endpoints = EndpointState::default();

        endpoints.switch(&mut selection);

        assert_eq!(endpoints, EndpointState::default());
    }
}
