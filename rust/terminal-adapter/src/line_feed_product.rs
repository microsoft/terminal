//! Final product integration for Adapter line-feed semantics.
//!
//! The VT parser already distinguishes LF/IND, NEL, and mode-dependent LF;
//! `terminal-buffer::viewport_index::line_feed` already owns the real buffer and
//! viewport mutation. This decorator connects that buffer owner to the same
//! `AdaptDispatchCore` cursor and live `TerminalInput` `LineFeed` mode used by the
//! rest of the final adapter product, avoiding a second cursor model.

use terminal_buffer::{
    rect_ops::ScreenRect,
    text_attribute::TextAttribute,
    text_buffer::{TextBuffer, TextBufferPoint},
    viewport_index::{LineFeedMargins, line_feed},
};
use terminal_input::Mode;
use terminal_parser::output_engine::{DcsAction, LineFeedType, OutputAction, TermDispatch};

use crate::{
    adapt_dispatch::PageGeometry, terminal_surface_product::TerminalSurfaceProductDispatch,
};

pub struct LineFeedProductDispatch {
    inner: TerminalSurfaceProductDispatch,
    buffer: TextBuffer,
    viewport: ScreenRect,
}

impl LineFeedProductDispatch {
    /// Creates a live screen-buffer owner aligned to the supplied page geometry.
    ///
    /// # Panics
    /// Panics only when geometry cannot fit the migration's `u16` buffer model;
    /// `PageGeometry` itself has already normalized zero/negative dimensions.
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        let width = u16::try_from(geometry.width).expect("adapter width fits terminal buffer");
        let top = u16::try_from(geometry.top.max(0)).expect("adapter top fits terminal buffer");
        let height = u16::try_from(geometry.height).expect("adapter height fits terminal buffer");
        let buffer_height = top
            .checked_add(height)
            .expect("adapter buffer height fits u16");
        let attribute = TextAttribute::default();
        let buffer = TextBuffer::new(width, buffer_height, attribute)
            .expect("normalized adapter geometry creates a valid buffer");
        let viewport = ScreenRect::new(0, top, width, buffer_height);
        Self {
            inner: TerminalSurfaceProductDispatch::new(geometry),
            buffer,
            viewport,
        }
    }

    #[must_use]
    pub const fn inner(&self) -> &TerminalSurfaceProductDispatch {
        &self.inner
    }

    pub const fn inner_mut(&mut self) -> &mut TerminalSurfaceProductDispatch {
        &mut self.inner
    }

    #[must_use]
    pub const fn buffer(&self) -> &TextBuffer {
        &self.buffer
    }

    #[must_use]
    pub const fn viewport(&self) -> ScreenRect {
        self.viewport
    }

    fn core(&self) -> &crate::adapt_dispatch::AdaptDispatchCore {
        self.inner
            .inner()
            .inner()
            .response_state()
            .presentation()
            .core()
    }

    fn core_mut(&mut self) -> &mut crate::adapt_dispatch::AdaptDispatchCore {
        self.inner
            .inner_mut()
            .inner_mut()
            .response_state_mut()
            .presentation_mut()
            .core_mut()
    }

    fn line_feed_mode(&self) -> bool {
        self.inner
            .inner()
            .inner()
            .input_modes()
            .input()
            .get_input_mode(Mode::LineFeed)
    }

    fn live_margins(&self) -> LineFeedMargins {
        let geometry = self.core().geometry();
        let margins = self.core().margins();
        let mut result = LineFeedMargins::none();
        if let Some(vertical) = margins.vertical()
            && let (Ok(top), Ok(bottom)) = (
                u16::try_from(geometry.top.saturating_add(vertical.start)),
                u16::try_from(geometry.top.saturating_add(vertical.end).saturating_add(1)),
            )
        {
            result.vertical = Some((top, bottom));
        }
        if let Some(horizontal) = margins.horizontal()
            && let (Ok(left), Ok(right)) = (
                u16::try_from(horizontal.start),
                u16::try_from(horizontal.end.saturating_add(1)),
            )
        {
            result.horizontal = Some((left, right));
        }
        result
    }

    fn apply_line_feed(&mut self, kind: LineFeedType) {
        let with_return = match kind {
            LineFeedType::WithoutReturn => false,
            LineFeedType::WithReturn => true,
            LineFeedType::DependsOnMode => self.line_feed_mode(),
        };
        let current = self.core().cursor();
        let Ok(x) = u16::try_from(current.x.max(0)) else {
            return;
        };
        let Ok(y) = u16::try_from(current.y.max(0)) else {
            return;
        };
        let mut cursor = TextBufferPoint::new(x, y);
        let margins = self.live_margins();
        let erase_attribute = self
            .inner
            .inner()
            .inner()
            .response_state()
            .presentation()
            .current_attributes();

        if line_feed(
            &mut self.buffer,
            &mut self.viewport,
            &mut cursor,
            margins,
            with_return,
            erase_attribute,
        )
        .is_ok()
        {
            self.core_mut().set_cursor(crate::adapt_dispatch::Point {
                x: i32::from(cursor.x),
                y: i32::from(cursor.y),
            });
        }
    }
}

impl TermDispatch for LineFeedProductDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        match action {
            OutputAction::LineFeed(kind) => self.apply_line_feed(kind),
            other => self.inner.dispatch(other),
        }
    }

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        self.inner.begin_dcs(action)
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        self.inner.dcs_put(code_unit)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::adapt_dispatch::Point;

    fn product() -> LineFeedProductDispatch {
        LineFeedProductDispatch::new(PageGeometry::new(0, 100, 29))
    }

    fn set_cursor(state: &mut LineFeedProductDispatch) {
        state.core_mut().set_cursor(Point { x: 10, y: 0 });
    }

    #[test]
    fn microsoft_line_feed_test_matches_all_four_source_cases_on_live_buffer_owner() {
        let mut state = product();

        set_cursor(&mut state);
        state.dispatch(OutputAction::LineFeed(LineFeedType::WithoutReturn));
        assert_eq!(state.core().cursor(), Point { x: 10, y: 1 });

        set_cursor(&mut state);
        state.dispatch(OutputAction::LineFeed(LineFeedType::WithReturn));
        assert_eq!(state.core().cursor(), Point { x: 0, y: 1 });

        state.dispatch(OutputAction::SetMode {
            private: false,
            mode: 20,
            enabled: false,
        });
        set_cursor(&mut state);
        state.dispatch(OutputAction::LineFeed(LineFeedType::DependsOnMode));
        assert_eq!(state.core().cursor(), Point { x: 10, y: 1 });

        state.dispatch(OutputAction::SetMode {
            private: false,
            mode: 20,
            enabled: true,
        });
        set_cursor(&mut state);
        state.dispatch(OutputAction::LineFeed(LineFeedType::DependsOnMode));
        assert_eq!(state.core().cursor(), Point { x: 0, y: 1 });
    }
}
