//! Safe, platform-neutral core for `AdaptDispatch` cursor and mode semantics.
//!
//! The C++ adapter mixes deterministic VT geometry with `TextBuffer`, renderer,
//! input, and host APIs. This module isolates the deterministic portion so the
//! parser can already dispatch into a real Rust state model. Operations that
//! still require those external surfaces are retained in `deferred_actions`
//! instead of being silently discarded.

use terminal_parser::output_engine::{OutputAction, TermDispatch};

const MODE_INSERT_REPLACE: u16 = 1 << 0;
const MODE_ORIGIN: u16 = 1 << 1;
const MODE_AUTO_WRAP: u16 = 1 << 2;
const MODE_ALLOW_LEFT_RIGHT_MARGINS: u16 = 1 << 3;
const MODE_SIXEL_DISPLAY: u16 = 1 << 4;
const MODE_ERASE_COLOR: u16 = 1 << 5;
const MODE_PAGE_CURSOR_COUPLING: u16 = 1 << 6;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Point {
    pub x: i32,
    pub y: i32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PageGeometry {
    pub top: i32,
    pub width: i32,
    pub height: i32,
}

impl PageGeometry {
    #[must_use]
    pub const fn new(top: i32, width: i32, height: i32) -> Self {
        Self {
            top,
            width: if width < 1 { 1 } else { width },
            height: if height < 1 { 1 } else { height },
        }
    }

    #[must_use]
    pub const fn bottom_exclusive(self) -> i32 {
        self.top.saturating_add(self.height)
    }

    #[must_use]
    pub const fn bottom(self) -> i32 {
        self.bottom_exclusive() - 1
    }

    #[must_use]
    pub const fn right(self) -> i32 {
        self.width - 1
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MarginRange {
    pub start: i32,
    pub end: i32,
}

impl MarginRange {
    #[must_use]
    pub const fn new(start: i32, end: i32) -> Self {
        Self { start, end }
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct ScrollMargins {
    vertical: Option<MarginRange>,
    horizontal: Option<MarginRange>,
}

impl ScrollMargins {
    #[must_use]
    pub const fn vertical(self) -> Option<MarginRange> {
        self.vertical
    }

    #[must_use]
    pub const fn horizontal(self) -> Option<MarginRange> {
        self.horizontal
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
struct ModeBits(u16);

impl ModeBits {
    const fn get(self, bit: u16) -> bool {
        self.0 & bit != 0
    }

    fn set(&mut self, bit: u16, enabled: bool) {
        if enabled {
            self.0 |= bit;
        } else {
            self.0 &= !bit;
        }
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
struct SavedCursor {
    row: i32,
    column: i32,
    delayed_eol_wrap: bool,
    origin_relative: bool,
    initialized: bool,
}

#[derive(Debug, Clone, Copy)]
struct Offset {
    value: i32,
    absolute: bool,
}

impl Offset {
    const fn absolute(value: i32) -> Self {
        Self {
            value: value.saturating_sub(1),
            absolute: true,
        }
    }

    const fn forward(value: i32) -> Self {
        Self {
            value,
            absolute: false,
        }
    }

    const fn backward(value: i32) -> Self {
        Self::forward(value.saturating_neg())
    }

    const fn unchanged() -> Self {
        Self::forward(0)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AdaptDispatchCore {
    geometry: PageGeometry,
    cursor: Point,
    delayed_eol_wrap: bool,
    modes: ModeBits,
    margins: ScrollMargins,
    saved_cursor: SavedCursor,
    deferred_actions: Vec<OutputAction>,
}

impl AdaptDispatchCore {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        Self {
            geometry,
            cursor: Point {
                x: 0,
                y: geometry.top,
            },
            delayed_eol_wrap: false,
            modes: ModeBits(MODE_AUTO_WRAP),
            margins: ScrollMargins::default(),
            saved_cursor: SavedCursor::default(),
            deferred_actions: Vec::new(),
        }
    }

    #[must_use]
    pub const fn geometry(&self) -> PageGeometry {
        self.geometry
    }

    pub fn set_geometry(&mut self, geometry: PageGeometry) {
        self.geometry = geometry;
        self.cursor.x = self.cursor.x.clamp(0, geometry.right());
        self.cursor.y = self.cursor.y.clamp(geometry.top, geometry.bottom());
        self.normalize_margins();
    }

    #[must_use]
    pub const fn cursor(&self) -> Point {
        self.cursor
    }

    pub fn set_cursor(&mut self, point: Point) {
        self.cursor = Point {
            x: point.x.clamp(0, self.geometry.right()),
            y: point.y.clamp(self.geometry.top, self.geometry.bottom()),
        };
        self.delayed_eol_wrap = false;
    }

    #[must_use]
    pub const fn delayed_eol_wrap(&self) -> bool {
        self.delayed_eol_wrap
    }

    pub const fn set_delayed_eol_wrap(&mut self, enabled: bool) {
        self.delayed_eol_wrap = enabled;
    }

    #[must_use]
    pub const fn margins(&self) -> ScrollMargins {
        self.margins
    }

    #[must_use]
    pub const fn origin_mode(&self) -> bool {
        self.modes.get(MODE_ORIGIN)
    }

    #[must_use]
    pub const fn insert_replace_mode(&self) -> bool {
        self.modes.get(MODE_INSERT_REPLACE)
    }

    #[must_use]
    pub const fn auto_wrap_mode(&self) -> bool {
        self.modes.get(MODE_AUTO_WRAP)
    }

    #[must_use]
    pub const fn left_right_margin_mode(&self) -> bool {
        self.modes.get(MODE_ALLOW_LEFT_RIGHT_MARGINS)
    }

    #[must_use]
    pub const fn sixel_display_mode(&self) -> bool {
        self.modes.get(MODE_SIXEL_DISPLAY)
    }

    #[must_use]
    pub const fn erase_color_mode(&self) -> bool {
        self.modes.get(MODE_ERASE_COLOR)
    }

    #[must_use]
    pub const fn page_cursor_coupling_mode(&self) -> bool {
        self.modes.get(MODE_PAGE_CURSOR_COUPLING)
    }

    #[must_use]
    pub fn deferred_actions(&self) -> &[OutputAction] {
        &self.deferred_actions
    }

    pub fn take_deferred_actions(&mut self) -> Vec<OutputAction> {
        std::mem::take(&mut self.deferred_actions)
    }

    pub fn cursor_up(&mut self, distance: i32) {
        self.move_cursor(Offset::backward(distance), Offset::unchanged(), true);
    }

    pub fn cursor_down(&mut self, distance: i32) {
        self.move_cursor(Offset::forward(distance), Offset::unchanged(), true);
    }

    pub fn cursor_forward(&mut self, distance: i32) {
        self.move_cursor(Offset::unchanged(), Offset::forward(distance), true);
    }

    pub fn cursor_backward(&mut self, distance: i32) {
        self.move_cursor(Offset::unchanged(), Offset::backward(distance), true);
    }

    pub fn cursor_next_line(&mut self, distance: i32) {
        self.move_cursor(Offset::forward(distance), Offset::absolute(1), true);
    }

    pub fn cursor_previous_line(&mut self, distance: i32) {
        self.move_cursor(Offset::backward(distance), Offset::absolute(1), true);
    }

    pub fn cursor_horizontal_absolute(&mut self, column: i32) {
        self.move_cursor(Offset::unchanged(), Offset::absolute(column), false);
    }

    pub fn cursor_vertical_absolute(&mut self, line: i32) {
        self.move_cursor(Offset::absolute(line), Offset::unchanged(), false);
    }

    pub fn horizontal_position_relative(&mut self, distance: i32) {
        self.move_cursor(Offset::unchanged(), Offset::forward(distance), false);
    }

    pub fn vertical_position_relative(&mut self, distance: i32) {
        self.move_cursor(Offset::forward(distance), Offset::unchanged(), false);
    }

    pub fn cursor_position(&mut self, line: i32, column: i32) {
        self.move_cursor(Offset::absolute(line), Offset::absolute(column), false);
    }

    pub fn set_top_bottom_margins(&mut self, top: i32, bottom: i32) -> bool {
        let height = self.geometry.height;
        let top = if top <= 0 { 1 } else { top };
        let bottom = if bottom <= 0 { height } else { bottom };

        if top < 1 || bottom > height || top >= bottom {
            return false;
        }

        self.margins.vertical = if top == 1 && bottom == height {
            None
        } else {
            Some(MarginRange::new(top - 1, bottom - 1))
        };
        self.cursor_position(1, 1);
        true
    }

    pub fn set_left_right_margins(&mut self, left: i32, right: i32) -> bool {
        if !self.left_right_margin_mode() {
            return false;
        }

        let width = self.geometry.width;
        let left = if left <= 0 { 1 } else { left };
        let right = if right <= 0 { width } else { right };

        if left < 1 || right > width || left >= right {
            return false;
        }

        self.margins.horizontal = if left == 1 && right == width {
            None
        } else {
            Some(MarginRange::new(left - 1, right - 1))
        };
        self.cursor_position(1, 1);
        true
    }

    pub fn set_mode(&mut self, private: bool, mode: i32, enabled: bool) -> bool {
        let bit = match (private, mode) {
            (false, 4) => MODE_INSERT_REPLACE,
            (true, 6) => MODE_ORIGIN,
            (true, 7) => MODE_AUTO_WRAP,
            (true, 64) => MODE_PAGE_CURSOR_COUPLING,
            (true, 69) => MODE_ALLOW_LEFT_RIGHT_MARGINS,
            (true, 80) => MODE_SIXEL_DISPLAY,
            (true, 117) => MODE_ERASE_COLOR,
            _ => return false,
        };

        self.modes.set(bit, enabled);
        if bit == MODE_ORIGIN {
            self.cursor_position(1, 1);
        } else if bit == MODE_ALLOW_LEFT_RIGHT_MARGINS {
            self.margins.horizontal = None;
        }
        true
    }

    #[must_use]
    pub fn mode_status(&self, private: bool, mode: i32) -> Option<bool> {
        let bit = match (private, mode) {
            (false, 4) => MODE_INSERT_REPLACE,
            (true, 6) => MODE_ORIGIN,
            (true, 7) => MODE_AUTO_WRAP,
            (true, 64) => MODE_PAGE_CURSOR_COUPLING,
            (true, 69) => MODE_ALLOW_LEFT_RIGHT_MARGINS,
            (true, 80) => MODE_SIXEL_DISPLAY,
            (true, 117) => MODE_ERASE_COLOR,
            _ => return None,
        };
        Some(self.modes.get(bit))
    }

    fn effective_vertical_margins(&self) -> MarginRange {
        self.margins.vertical.map_or_else(
            || MarginRange::new(self.geometry.top, self.geometry.bottom()),
            |range| {
                MarginRange::new(
                    self.geometry.top.saturating_add(range.start),
                    self.geometry.top.saturating_add(range.end),
                )
            },
        )
    }

    fn effective_horizontal_margins(&self) -> MarginRange {
        self.margins
            .horizontal
            .unwrap_or_else(|| MarginRange::new(0, self.geometry.right()))
    }

    fn normalize_margins(&mut self) {
        if let Some(range) = self.margins.vertical {
            if range.start >= self.geometry.height - 1 {
                self.margins.vertical = None;
            } else {
                self.margins.vertical = Some(MarginRange::new(
                    range.start,
                    range.end.min(self.geometry.height - 1),
                ));
            }
        }

        if let Some(range) = self.margins.horizontal {
            if range.start >= self.geometry.width - 1 {
                self.margins.horizontal = None;
            } else {
                self.margins.horizontal = Some(MarginRange::new(
                    range.start,
                    range.end.min(self.geometry.width - 1),
                ));
            }
        }
    }

    fn move_cursor(&mut self, row_offset: Offset, column_offset: Offset, clamp_in_margins: bool) {
        let original = self.cursor;
        let vertical = self.effective_vertical_margins();
        let horizontal = self.effective_horizontal_margins();

        let mut row = original.y;
        let mut column = original.x;

        if row_offset.absolute {
            row = if self.origin_mode() {
                vertical.start
            } else {
                self.geometry.top
            };
        }
        if column_offset.absolute {
            column = if self.origin_mode() {
                horizontal.start
            } else {
                0
            };
        }

        row = row
            .saturating_add(row_offset.value)
            .clamp(self.geometry.top, self.geometry.bottom());
        column = column
            .saturating_add(column_offset.value)
            .clamp(0, self.geometry.right());

        if clamp_in_margins || self.origin_mode() {
            if original.x >= horizontal.start && original.x <= horizontal.end {
                if original.y >= vertical.start {
                    row = row.max(vertical.start);
                }
                if original.y <= vertical.end {
                    row = row.min(vertical.end);
                }
            }

            if row >= vertical.start && row <= vertical.end {
                if original.x >= horizontal.start {
                    column = column.max(horizontal.start);
                }
                if original.x <= horizontal.end {
                    column = column.min(horizontal.end);
                }
            }
        }

        self.cursor = Point { x: column, y: row };
        self.delayed_eol_wrap = false;
    }

    fn save_cursor(&mut self) {
        let vertical = self.effective_vertical_margins();
        let horizontal = self.effective_horizontal_margins();
        let origin_relative = self.origin_mode();
        self.saved_cursor = SavedCursor {
            row: if origin_relative {
                self.cursor.y - vertical.start + 1
            } else {
                self.cursor.y - self.geometry.top + 1
            },
            column: if origin_relative {
                self.cursor.x - horizontal.start + 1
            } else {
                self.cursor.x + 1
            },
            delayed_eol_wrap: self.delayed_eol_wrap,
            origin_relative,
            initialized: true,
        };
    }

    fn restore_cursor(&mut self) {
        if !self.saved_cursor.initialized {
            self.modes.set(MODE_ORIGIN, false);
            self.cursor_position(1, 1);
            self.delayed_eol_wrap = false;
            return;
        }

        self.modes
            .set(MODE_ORIGIN, self.saved_cursor.origin_relative);
        self.cursor_position(self.saved_cursor.row, self.saved_cursor.column);
        self.delayed_eol_wrap = self.saved_cursor.delayed_eol_wrap;
    }
}

impl TermDispatch for AdaptDispatchCore {
    fn dispatch(&mut self, action: OutputAction) {
        match action {
            OutputAction::CursorUp(distance) => self.cursor_up(distance),
            OutputAction::CursorDown(distance) => self.cursor_down(distance),
            OutputAction::CursorForward(distance) => self.cursor_forward(distance),
            OutputAction::CursorBackward(distance) => self.cursor_backward(distance),
            OutputAction::CursorNextLine(distance) => self.cursor_next_line(distance),
            OutputAction::CursorPreviousLine(distance) => self.cursor_previous_line(distance),
            OutputAction::CursorHorizontalPositionAbsolute(column) => {
                self.cursor_horizontal_absolute(column);
            }
            OutputAction::VerticalLinePositionAbsolute(line) => self.cursor_vertical_absolute(line),
            OutputAction::HorizontalPositionRelative(distance) => {
                self.horizontal_position_relative(distance);
            }
            OutputAction::VerticalPositionRelative(distance) => {
                self.vertical_position_relative(distance);
            }
            OutputAction::CursorPosition { line, column } => self.cursor_position(line, column),
            OutputAction::SetTopBottomScrollingMargins { top, bottom } => {
                let _ = self.set_top_bottom_margins(top, bottom);
            }
            OutputAction::SetLeftRightScrollingMargins { left, right } => {
                let _ = self.set_left_right_margins(left, right);
            }
            OutputAction::SetMode {
                private,
                enabled,
                mode,
            } => {
                if !self.set_mode(private, mode, enabled) {
                    self.deferred_actions.push(OutputAction::SetMode {
                        private,
                        enabled,
                        mode,
                    });
                }
            }
            OutputAction::CursorSaveState => self.save_cursor(),
            OutputAction::CursorRestoreState => self.restore_cursor(),
            other => self.deferred_actions.push(other),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use terminal_parser::output_engine::OutputStateMachineEngine;
    use terminal_parser::state_machine::StateMachine;

    fn geometry() -> PageGeometry {
        PageGeometry::new(20, 100, 29)
    }

    fn parse(text: &str) -> AdaptDispatchCore {
        let dispatch = AdaptDispatchCore::new(geometry());
        let engine = OutputStateMachineEngine::new(dispatch);
        let mut machine = StateMachine::new(engine);
        machine.process_str(text);
        machine.engine().dispatch().clone()
    }

    #[test]
    fn defaults_match_adapter_page_home_and_autowrap() {
        let dispatch = AdaptDispatchCore::new(geometry());
        assert_eq!(dispatch.cursor(), Point { x: 0, y: 20 });
        assert!(dispatch.auto_wrap_mode());
        assert!(!dispatch.origin_mode());
        assert_eq!(dispatch.margins(), ScrollMargins::default());
    }

    #[test]
    fn microsoft_cursor_movement_contract_clamps_to_page_and_buffer_edges() {
        let mut dispatch = AdaptDispatchCore::new(geometry());
        dispatch.set_cursor(Point { x: 50, y: 34 });
        dispatch.cursor_up(100);
        assert_eq!(dispatch.cursor(), Point { x: 50, y: 20 });

        dispatch.set_cursor(Point { x: 50, y: 34 });
        dispatch.cursor_down(100);
        assert_eq!(dispatch.cursor(), Point { x: 50, y: 48 });

        dispatch.cursor_forward(100);
        assert_eq!(dispatch.cursor().x, 99);
        dispatch.cursor_backward(100);
        assert_eq!(dispatch.cursor().x, 0);
    }

    #[test]
    fn next_and_previous_line_always_target_column_one() {
        let mut dispatch = AdaptDispatchCore::new(geometry());
        dispatch.set_cursor(Point { x: 99, y: 48 });
        dispatch.cursor_next_line(1);
        assert_eq!(dispatch.cursor(), Point { x: 0, y: 48 });

        dispatch.set_cursor(Point { x: 99, y: 20 });
        dispatch.cursor_previous_line(1);
        assert_eq!(dispatch.cursor(), Point { x: 0, y: 20 });
    }

    #[test]
    fn absolute_positioning_is_one_based_and_vertical_position_is_page_relative() {
        let mut dispatch = AdaptDispatchCore::new(geometry());
        dispatch.cursor_position(15, 15);
        assert_eq!(dispatch.cursor(), Point { x: 14, y: 34 });

        dispatch.cursor_position(1, 1);
        assert_eq!(dispatch.cursor(), Point { x: 0, y: 20 });

        dispatch.cursor_position(100, 200);
        assert_eq!(dispatch.cursor(), Point { x: 99, y: 48 });
    }

    #[test]
    fn single_dimension_absolute_commands_preserve_the_other_coordinate() {
        let mut dispatch = AdaptDispatchCore::new(geometry());
        dispatch.set_cursor(Point { x: 50, y: 34 });
        dispatch.cursor_horizontal_absolute(20);
        assert_eq!(dispatch.cursor(), Point { x: 19, y: 34 });
        dispatch.cursor_vertical_absolute(5);
        assert_eq!(dispatch.cursor(), Point { x: 19, y: 24 });
    }

    #[test]
    fn top_bottom_margin_validation_matches_microsoft_cases() {
        let mut dispatch = AdaptDispatchCore::new(PageGeometry::new(0, 8, 8));
        assert!(dispatch.set_top_bottom_margins(2, 6));
        assert_eq!(dispatch.margins().vertical(), Some(MarginRange::new(1, 5)));

        assert!(dispatch.set_top_bottom_margins(7, 0));
        assert_eq!(dispatch.margins().vertical(), Some(MarginRange::new(6, 7)));

        assert!(dispatch.set_top_bottom_margins(0, 7));
        assert_eq!(dispatch.margins().vertical(), Some(MarginRange::new(0, 6)));

        assert!(dispatch.set_top_bottom_margins(0, 0));
        assert_eq!(dispatch.margins().vertical(), None);

        assert!(!dispatch.set_top_bottom_margins(7, 3));
        assert_eq!(dispatch.margins().vertical(), None);
        assert!(!dispatch.set_top_bottom_margins(4, 4));
        assert!(!dispatch.set_top_bottom_margins(9, 18));
        assert!(!dispatch.set_top_bottom_margins(1, 9));
    }

    #[test]
    fn full_height_variants_clear_stored_vertical_margins() {
        let mut dispatch = AdaptDispatchCore::new(PageGeometry::new(0, 8, 8));
        assert!(dispatch.set_top_bottom_margins(2, 6));
        assert!(dispatch.set_top_bottom_margins(0, 8));
        assert_eq!(dispatch.margins().vertical(), None);

        assert!(dispatch.set_top_bottom_margins(2, 6));
        assert!(dispatch.set_top_bottom_margins(1, 8));
        assert_eq!(dispatch.margins().vertical(), None);

        assert!(dispatch.set_top_bottom_margins(2, 6));
        assert!(dispatch.set_top_bottom_margins(1, 0));
        assert_eq!(dispatch.margins().vertical(), None);
    }

    #[test]
    fn origin_mode_homes_to_top_left_margin_and_clamps_absolute_positions() {
        let mut dispatch = AdaptDispatchCore::new(PageGeometry::new(20, 100, 29));
        assert!(dispatch.set_top_bottom_margins(5, 10));
        assert!(dispatch.set_mode(true, 69, true));
        assert!(dispatch.set_left_right_margins(10, 30));
        assert!(dispatch.set_mode(true, 6, true));
        assert_eq!(dispatch.cursor(), Point { x: 9, y: 24 });

        dispatch.cursor_position(2, 3);
        assert_eq!(dispatch.cursor(), Point { x: 11, y: 25 });
        dispatch.cursor_position(100, 100);
        assert_eq!(dispatch.cursor(), Point { x: 29, y: 29 });
    }

    #[test]
    fn disabling_left_right_margin_mode_clears_horizontal_margins() {
        let mut dispatch = AdaptDispatchCore::new(geometry());
        assert!(!dispatch.set_left_right_margins(5, 10));
        assert!(dispatch.set_mode(true, 69, true));
        assert!(dispatch.set_left_right_margins(5, 10));
        assert_eq!(
            dispatch.margins().horizontal(),
            Some(MarginRange::new(4, 9))
        );
        assert!(dispatch.set_mode(true, 69, false));
        assert_eq!(dispatch.margins().horizontal(), None);
    }

    #[test]
    fn relative_hpr_and_vpr_ignore_margins_when_origin_mode_is_off() {
        let mut dispatch = AdaptDispatchCore::new(PageGeometry::new(0, 100, 30));
        assert!(dispatch.set_top_bottom_margins(5, 10));
        assert!(dispatch.set_mode(true, 69, true));
        assert!(dispatch.set_left_right_margins(10, 20));
        dispatch.set_cursor(Point { x: 15, y: 7 });

        dispatch.horizontal_position_relative(50);
        assert_eq!(dispatch.cursor().x, 65);
        dispatch.vertical_position_relative(20);
        assert_eq!(dispatch.cursor().y, 27);
    }

    #[test]
    fn cursor_motion_commands_do_obey_margins_from_inside_the_region() {
        let mut dispatch = AdaptDispatchCore::new(PageGeometry::new(0, 100, 30));
        assert!(dispatch.set_top_bottom_margins(5, 10));
        assert!(dispatch.set_mode(true, 69, true));
        assert!(dispatch.set_left_right_margins(10, 20));
        dispatch.set_cursor(Point { x: 15, y: 7 });

        dispatch.cursor_down(100);
        assert_eq!(dispatch.cursor().y, 9);
        dispatch.cursor_forward(100);
        assert_eq!(dispatch.cursor().x, 19);
    }

    #[test]
    fn mode_routing_tracks_adapter_local_modes_and_reports_unknowns() {
        let mut dispatch = AdaptDispatchCore::new(geometry());
        assert!(dispatch.set_mode(false, 4, true));
        assert!(dispatch.insert_replace_mode());
        assert!(dispatch.set_mode(true, 80, true));
        assert!(dispatch.sixel_display_mode());
        assert_eq!(dispatch.mode_status(true, 117), Some(false));
        assert_eq!(dispatch.mode_status(true, 9_999), None);
    }

    #[test]
    fn save_restore_preserves_position_origin_and_delayed_wrap() {
        let mut dispatch = AdaptDispatchCore::new(PageGeometry::new(0, 80, 24));
        assert!(dispatch.set_top_bottom_margins(5, 20));
        assert!(dispatch.set_mode(true, 69, true));
        assert!(dispatch.set_left_right_margins(10, 70));
        assert!(dispatch.set_mode(true, 6, true));
        dispatch.cursor_position(3, 4);
        dispatch.set_delayed_eol_wrap(true);
        dispatch.dispatch(OutputAction::CursorSaveState);

        assert!(dispatch.set_mode(true, 6, false));
        dispatch.cursor_position(1, 1);
        dispatch.set_delayed_eol_wrap(false);
        dispatch.dispatch(OutputAction::CursorRestoreState);

        assert!(dispatch.origin_mode());
        assert_eq!(dispatch.cursor(), Point { x: 12, y: 6 });
        assert!(dispatch.delayed_eol_wrap());
    }

    #[test]
    fn restore_without_a_saved_state_returns_to_absolute_home() {
        let mut dispatch = AdaptDispatchCore::new(geometry());
        dispatch.set_cursor(Point { x: 50, y: 30 });
        assert!(dispatch.set_mode(true, 6, true));
        dispatch.dispatch(OutputAction::CursorRestoreState);
        assert!(!dispatch.origin_mode());
        assert_eq!(dispatch.cursor(), Point { x: 0, y: 20 });
    }

    #[test]
    fn unsupported_actions_are_deferred_instead_of_dropped() {
        let mut dispatch = AdaptDispatchCore::new(geometry());
        dispatch.dispatch(OutputAction::WarningBell);
        dispatch.dispatch(OutputAction::PrintString(vec![u16::from(b'A')]));
        assert_eq!(dispatch.deferred_actions().len(), 2);
        assert_eq!(dispatch.take_deferred_actions().len(), 2);
        assert!(dispatch.deferred_actions().is_empty());
    }

    #[test]
    fn parser_to_output_engine_to_adapter_core_moves_the_cursor_end_to_end() {
        let dispatch = parse("\u{1b}[10;20H");
        assert_eq!(dispatch.cursor(), Point { x: 19, y: 29 });
    }

    #[test]
    fn parser_private_modes_drive_origin_and_left_right_margin_semantics() {
        let dispatch = parse("\u{1b}[5;10r\u{1b}[?69h\u{1b}[10;30s\u{1b}[?6h\u{1b}[2;3H");
        assert!(dispatch.origin_mode());
        assert_eq!(dispatch.margins().vertical(), Some(MarginRange::new(4, 9)));
        assert_eq!(
            dispatch.margins().horizontal(),
            Some(MarginRange::new(9, 29))
        );
        assert_eq!(dispatch.cursor(), Point { x: 11, y: 25 });
    }

    #[test]
    fn unknown_mode_action_is_preserved_for_later_terminal_surfaces() {
        let mut dispatch = AdaptDispatchCore::new(geometry());
        dispatch.dispatch(OutputAction::SetMode {
            private: true,
            enabled: true,
            mode: 1_004,
        });
        assert_eq!(dispatch.deferred_actions().len(), 1);
    }
}
