//! Portable aggregate policy for `TerminalCore` sizing, history and scrolling.
//!
//! Microsoft Terminal clamps visible dimensions and total backing rows to the
//! signed 16-bit coordinate domain used by the native product. Keeping that
//! policy here gives Rust a real aggregate owner without pulling `WinRT` or
//! renderer concerns into the semantic core.

const MAX_COORD: i32 = i16::MAX as i32;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalDimensions {
    pub width: u16,
    pub height: u16,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ScrollBarNotification {
    pub viewport_top: u16,
    pub viewport_height: u16,
    pub buffer_height: u16,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalScrollUpdate {
    pub scroll_bar: Option<ScrollBarNotification>,
    pub renderer_delta: Option<(i16, i16)>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TerminalLayout {
    viewport: TerminalDimensions,
    configured_history_rows: u16,
    total_rows: u16,
    emitted_rows: u32,
    always_notify_on_buffer_rotation: bool,
}

impl TerminalLayout {
    #[must_use]
    pub fn from_settings(history_size: i32, rows: i32, columns: i32) -> Self {
        let height = clamp_dimension(rows);
        let width = clamp_dimension(columns);
        let configured_history_rows = clamp_history(history_size, height);
        Self {
            viewport: TerminalDimensions { width, height },
            configured_history_rows,
            total_rows: height + configured_history_rows,
            emitted_rows: 0,
            always_notify_on_buffer_rotation: false,
        }
    }

    #[must_use]
    pub const fn viewport(&self) -> TerminalDimensions {
        self.viewport
    }

    #[must_use]
    pub const fn total_rows(&self) -> u16 {
        self.total_rows
    }

    #[must_use]
    pub const fn configured_history_rows(&self) -> u16 {
        self.configured_history_rows
    }

    pub const fn set_always_notify_on_buffer_rotation(&mut self, value: bool) {
        self.always_notify_on_buffer_rotation = value;
    }

    /// Records one emitted line and returns the portable side effects that the
    /// native shell must forward to the scrollbar callback and renderer.
    ///
    /// Once the viewport starts scrolling, its top advances until history is
    /// saturated. After the backing buffer starts circling, every new line
    /// requests renderer scroll `(0, -1)`; scrollbar callbacks continue only
    /// when the host explicitly requested notifications on buffer rotation.
    #[must_use]
    pub fn line_feed(&mut self) -> TerminalScrollUpdate {
        let current_row = self.emitted_rows;
        self.emitted_rows = self.emitted_rows.saturating_add(1);

        let viewport_height = u32::from(self.viewport.height);
        let total_rows = u32::from(self.total_rows);
        let scrolled = current_row >= viewport_height.saturating_sub(1);
        let circled_buffer = current_row >= total_rows.saturating_sub(1);
        let notify_scroll_bar = (scrolled && !circled_buffer)
            || (circled_buffer && self.always_notify_on_buffer_rotation);

        let scroll_bar = notify_scroll_bar.then(|| {
            let unclamped_top = current_row
                .saturating_add(2)
                .saturating_sub(viewport_height);
            let top = unclamped_top.min(u32::from(self.configured_history_rows));
            let top = u16::try_from(top).unwrap_or(self.configured_history_rows);
            ScrollBarNotification {
                viewport_top: top,
                viewport_height: self.viewport.height,
                buffer_height: top + self.viewport.height,
            }
        });

        let renderer_delta = (scrolled && circled_buffer).then_some((0, -1));

        TerminalScrollUpdate {
            scroll_bar,
            renderer_delta,
        }
    }

    /// Applies the same user-resize capacity rule as `TerminalCore`: viewport
    /// dimensions remain in range and the backing row count is clamped to
    /// `SHRT_MAX` without mutating the configured history allowance. Shrinking
    /// the viewport can therefore restore rows that a larger viewport had
    /// temporarily clipped.
    pub fn user_resize(&mut self, columns: i32, rows: i32) {
        let height = clamp_dimension(rows);
        let width = clamp_dimension(columns);
        let requested_total = i32::from(height) + i32::from(self.configured_history_rows);
        self.total_rows = coord_to_u16(requested_total.min(MAX_COORD));
        self.viewport = TerminalDimensions { width, height };
    }
}

fn coord_to_u16(value: i32) -> u16 {
    u16::try_from(value).expect("terminal coordinate policy keeps values in u16 range")
}

fn clamp_dimension(value: i32) -> u16 {
    coord_to_u16(value.clamp(1, MAX_COORD))
}

fn clamp_history(history_size: i32, visible_rows: u16) -> u16 {
    let available = MAX_COORD - i32::from(visible_rows);
    coord_to_u16(history_size.clamp(0, available))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn max_coord_u16() -> u16 {
        u16::try_from(i16::MAX).expect("i16::MAX is positive and representable as u16")
    }

    #[test]
    fn microsoft_screen_size_limits_width_and_height_are_clamped_to_bounds() {
        let negative_columns = TerminalLayout::from_settings(10_000, 9_999_999, -1_234);
        assert_eq!(negative_columns.viewport().height, max_coord_u16());
        assert_eq!(negative_columns.viewport().width, 1);

        let zero_rows = TerminalLayout::from_settings(10_000, 0, 9_999_999);
        assert_eq!(zero_rows.viewport().height, 1);
        assert_eq!(zero_rows.viewport().width, max_coord_u16());
    }

    #[test]
    fn microsoft_screen_size_limits_scrollback_history_is_clamped_to_bounds() {
        const VISIBLE: i32 = 100;
        assert_eq!(
            TerminalLayout::from_settings(0, VISIBLE, 100).total_rows(),
            100
        );
        assert_eq!(
            TerminalLayout::from_settings(-100, VISIBLE, 100).total_rows(),
            100
        );
        assert_eq!(
            TerminalLayout::from_settings(i32::from(i16::MAX) - VISIBLE, VISIBLE, 100).total_rows(),
            max_coord_u16()
        );
        assert_eq!(
            TerminalLayout::from_settings(i32::from(i16::MAX) - VISIBLE + 1, VISIBLE, 100)
                .total_rows(),
            max_coord_u16()
        );
        assert_eq!(
            TerminalLayout::from_settings(99_999_999, VISIBLE, 100).total_rows(),
            max_coord_u16()
        );
    }

    #[test]
    fn microsoft_screen_size_limits_resize_is_clamped_to_bounds() {
        const COLS: i32 = 50;
        const ROWS: i32 = 50;
        let history = i32::from(i16::MAX) - ROWS * 2;
        let expected_total =
            u16::try_from(history + ROWS).expect("test total remains in the coordinate domain");
        let mut terminal = TerminalLayout::from_settings(history, ROWS, COLS);
        assert_eq!(terminal.total_rows(), expected_total);

        terminal.user_resize(COLS, ROWS * 2);
        assert_eq!(terminal.total_rows(), max_coord_u16());

        terminal.user_resize(COLS, ROWS * 3);
        assert_eq!(terminal.total_rows(), max_coord_u16());

        terminal.user_resize(COLS, ROWS);
        assert_eq!(terminal.total_rows(), expected_total);
    }

    #[test]
    fn microsoft_scroll_test_notify_scrolling_matches_source_contract() {
        const VIEW_HEIGHT: i32 = 32;
        const HISTORY: i32 = 9_001;

        for notify_on_circling in [false, true] {
            let mut terminal = TerminalLayout::from_settings(HISTORY, VIEW_HEIGHT, 80);
            terminal.set_always_notify_on_buffer_rotation(notify_on_circling);
            let total_rows = u32::from(terminal.total_rows());

            for current_row in 0..total_rows * 2 {
                let update = terminal.line_feed();
                let scrolled = current_row >= 31;
                let circled_buffer = current_row >= total_rows - 1;
                let expect_scroll_bar =
                    (scrolled && !circled_buffer) || (circled_buffer && notify_on_circling);

                assert_eq!(update.scroll_bar.is_some(), expect_scroll_bar);
                assert_eq!(
                    update.renderer_delta,
                    (scrolled && circled_buffer).then_some((0, -1))
                );

                if let Some(notification) = update.scroll_bar {
                    let expected_top = current_row.saturating_add(2).saturating_sub(32).min(9_001);
                    assert_eq!(u32::from(notification.viewport_top), expected_top);
                    assert_eq!(notification.viewport_height, 32);
                    assert_eq!(
                        notification.buffer_height,
                        notification.viewport_top + notification.viewport_height
                    );
                }
            }
        }
    }
}
