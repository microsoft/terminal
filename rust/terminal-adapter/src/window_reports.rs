//! Portable window-manipulation report owner for the adapter product path.
//!
//! Microsoft Terminal reports text dimensions with the live buffer width and
//! viewport height, while pixel reports intentionally use the DEC-compatible
//! virtual cell size of 10x20 pixels. This owner keeps that deterministic VT
//! framing in Rust without importing renderer or platform dependencies.

use terminal_parser::output_engine::{OutputAction, TermDispatch};

use crate::{adapt_dispatch::PageGeometry, vt_response::VtResponseEngine};

const VIRTUAL_CELL_WIDTH: i32 = 10;
const VIRTUAL_CELL_HEIGHT: i32 = 20;
const REPORT_TEXT_SIZE_IN_PIXELS: i32 = 14;
const REPORT_CHARACTER_CELL_SIZE: i32 = 16;
const REPORT_TEXT_SIZE_IN_CHARACTERS: i32 = 18;

#[derive(Debug, Clone)]
pub struct WindowReportEngine {
    geometry: PageGeometry,
    responses: VtResponseEngine,
}

impl WindowReportEngine {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        Self {
            geometry,
            responses: VtResponseEngine::default(),
        }
    }

    #[must_use]
    pub fn response(&self) -> &str {
        self.responses.response()
    }

    pub fn clear_response(&mut self) {
        self.responses.clear();
    }

    pub const fn set_response_writable(&mut self, writable: bool) {
        self.responses.set_writable(writable);
    }

    #[must_use]
    pub const fn handles(function: i32) -> bool {
        matches!(
            function,
            REPORT_TEXT_SIZE_IN_PIXELS
                | REPORT_CHARACTER_CELL_SIZE
                | REPORT_TEXT_SIZE_IN_CHARACTERS
        )
    }

    fn report(&mut self, function: i32) -> bool {
        let height = self.geometry.height.max(1);
        let width = self.geometry.width.max(1);
        let response = match function {
            REPORT_TEXT_SIZE_IN_CHARACTERS => format!("\u{1b}[8;{height};{width}t"),
            REPORT_TEXT_SIZE_IN_PIXELS => format!(
                "\u{1b}[4;{};{}t",
                height.saturating_mul(VIRTUAL_CELL_HEIGHT),
                width.saturating_mul(VIRTUAL_CELL_WIDTH)
            ),
            REPORT_CHARACTER_CELL_SIZE => {
                format!("\u{1b}[6;{VIRTUAL_CELL_HEIGHT};{VIRTUAL_CELL_WIDTH}t")
            }
            _ => return false,
        };
        self.responses.return_response(&response)
    }
}

impl TermDispatch for WindowReportEngine {
    fn dispatch(&mut self, action: OutputAction) {
        if let OutputAction::WindowManipulation { function, .. } = action {
            let _ = self.report(function);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_window_reports_match_character_pixel_and_cell_vectors() {
        let mut engine = WindowReportEngine::new(PageGeometry::new(20, 100, 29));

        engine.dispatch(OutputAction::WindowManipulation {
            function: REPORT_TEXT_SIZE_IN_CHARACTERS,
            parameter1: 0,
            parameter2: 0,
        });
        assert_eq!(engine.response(), "\u{1b}[8;29;100t");

        engine.clear_response();
        engine.dispatch(OutputAction::WindowManipulation {
            function: REPORT_TEXT_SIZE_IN_PIXELS,
            parameter1: 0,
            parameter2: 0,
        });
        assert_eq!(engine.response(), "\u{1b}[4;580;1000t");

        engine.clear_response();
        engine.dispatch(OutputAction::WindowManipulation {
            function: REPORT_CHARACTER_CELL_SIZE,
            parameter1: 0,
            parameter2: 0,
        });
        assert_eq!(engine.response(), "\u{1b}[6;20;10t");
    }

    #[test]
    fn unsupported_window_operation_is_not_reported() {
        let mut engine = WindowReportEngine::new(PageGeometry::new(0, 80, 24));
        engine.dispatch(OutputAction::WindowManipulation {
            function: 8,
            parameter1: 30,
            parameter2: 120,
        });
        assert!(engine.response().is_empty());
    }

    #[test]
    fn window_report_sink_failure_writes_nothing() {
        let mut engine = WindowReportEngine::new(PageGeometry::new(20, 100, 29));
        engine.set_response_writable(false);
        engine.dispatch(OutputAction::WindowManipulation {
            function: REPORT_TEXT_SIZE_IN_CHARACTERS,
            parameter1: 0,
            parameter2: 0,
        });
        assert!(engine.response().is_empty());
    }
}
