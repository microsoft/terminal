//! Product composition for DEC presentation-state reports.
//!
//! R08 F05 keeps the established [`AdaptDispatchProductState`] intact while
//! adding DECRQPSR as a narrow response-producing decorator. The wrapper owns
//! DCS presentation-state restore sessions and report-only terminal-output
//! metadata, while every unrelated parser action continues through the existing
//! product aggregate. This gives DECCIR/DECTABSR real parser-to-product state
//! without duplicating the underlying cursor or `TextAttribute` owners.

use terminal_parser::output_engine::{DcsAction, OutputAction, TermDispatch};

use crate::{
    adapt_dispatch::{PageGeometry, Point},
    presentation_reports::{CursorRestore, PresentationReportEngine},
    product_dispatch::AdaptDispatchProductState,
};

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
enum DcsOwner {
    #[default]
    None,
    Product,
    PresentationReport,
}

pub struct AdaptDispatchReportingState {
    product: AdaptDispatchProductState,
    presentation_reports: PresentationReportEngine,
    outbound: String,
    writable: bool,
    dcs_owner: DcsOwner,
}

impl AdaptDispatchReportingState {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        Self {
            product: AdaptDispatchProductState::new(geometry),
            presentation_reports: PresentationReportEngine::new(geometry.width),
            outbound: String::new(),
            writable: true,
            dcs_owner: DcsOwner::None,
        }
    }

    #[must_use]
    pub const fn product(&self) -> &AdaptDispatchProductState {
        &self.product
    }

    pub const fn product_mut(&mut self) -> &mut AdaptDispatchProductState {
        &mut self.product
    }

    #[must_use]
    pub const fn presentation_reports(&self) -> &PresentationReportEngine {
        &self.presentation_reports
    }

    pub fn set_text_width(&mut self, width: i32) {
        self.presentation_reports.set_width(width);
    }

    #[must_use]
    pub fn response(&self) -> &str {
        &self.outbound
    }

    pub fn clear_response(&mut self) {
        self.outbound.clear();
        self.product.clear_response();
        self.presentation_reports.clear_response();
    }

    pub const fn set_response_writable(&mut self, writable: bool) {
        self.writable = writable;
        self.product.set_response_writable(writable);
        self.presentation_reports.set_response_writable(writable);
    }

    fn collect_responses(&mut self) {
        if !self.product.response().is_empty() {
            self.outbound.push_str(self.product.response());
            self.product.clear_response();
        }
        if !self.presentation_reports.response().is_empty() {
            self.outbound.push_str(self.presentation_reports.response());
            self.presentation_reports.clear_response();
        }
    }

    fn dispatch_cursor_information_report(&mut self, action: OutputAction) {
        if !self.writable {
            self.product.dispatch(action);
            self.collect_responses();
            return;
        }

        let (cursor, viewport_top, attributes, origin_mode, delayed_eol_wrap) = {
            let presentation = self.product.response_state().presentation();
            let core = presentation.core();
            (
                core.cursor(),
                core.geometry().top,
                presentation.current_attributes(),
                core.origin_mode(),
                core.delayed_eol_wrap(),
            )
        };
        let _ = self.presentation_reports.request_cursor_information_report(
            cursor,
            viewport_top,
            attributes,
            origin_mode,
            delayed_eol_wrap,
        );
        self.collect_responses();
    }

    fn dispatch_tabulation_report(&mut self, action: OutputAction) {
        if self.writable {
            self.presentation_reports.dispatch(action);
            self.collect_responses();
        } else {
            self.product.dispatch(action);
            self.collect_responses();
        }
    }

    fn print_will_mark_delayed_wrap(&self, action: &OutputAction) -> bool {
        let has_text = match action {
            OutputAction::Print(_) => true,
            OutputAction::PrintString(text) => !text.is_empty(),
            _ => false,
        };
        if !has_text {
            return false;
        }
        let core = self.product.response_state().presentation().core();
        core.cursor().x >= core.geometry().right()
    }

    fn apply_cursor_restore(&mut self, restored: &CursorRestore) {
        self.product
            .dispatch(OutputAction::PagePositionAbsolute(restored.page));
        let presentation = self.product.response_state_mut().presentation_mut();
        let _ = presentation.set_mode(true, 6, restored.origin_mode);
        let geometry = presentation.core().geometry();
        presentation.core_mut().set_cursor(Point {
            x: restored.column.saturating_sub(1),
            y: geometry.top.saturating_add(restored.row.saturating_sub(1)),
        });
        presentation
            .core_mut()
            .set_delayed_eol_wrap(restored.delayed_eol_wrap);
        presentation.set_current_attributes(restored.attributes);
    }
}

impl TermDispatch for AdaptDispatchReportingState {
    fn dispatch(&mut self, action: OutputAction) {
        if PresentationReportEngine::is_cursor_information_report(&action) {
            self.dispatch_cursor_information_report(action);
        } else if PresentationReportEngine::is_tabulation_report(&action) {
            self.dispatch_tabulation_report(action);
        } else if PresentationReportEngine::is_clear_all_tabs(&action) {
            self.presentation_reports.dispatch(action);
        } else {
            let mark_delayed_wrap = self.print_will_mark_delayed_wrap(&action);
            self.presentation_reports.observe(&action);
            self.product.dispatch(action);
            if mark_delayed_wrap {
                self.product
                    .response_state_mut()
                    .presentation_mut()
                    .core_mut()
                    .set_delayed_eol_wrap(true);
            }
            self.collect_responses();
        }
    }

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        self.dcs_owner = DcsOwner::None;
        if PresentationReportEngine::handles_restore(&action) {
            if self.presentation_reports.begin_dcs(action) {
                self.dcs_owner = DcsOwner::PresentationReport;
                return true;
            }
            return false;
        }

        if self.product.begin_dcs(action) {
            self.dcs_owner = DcsOwner::Product;
            true
        } else {
            false
        }
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        let result = match self.dcs_owner {
            DcsOwner::Product => self.product.dcs_put(code_unit),
            DcsOwner::PresentationReport => self.presentation_reports.dcs_put(code_unit),
            DcsOwner::None => false,
        };
        if let Some(restored) = self.presentation_reports.take_cursor_restore() {
            self.apply_cursor_restore(&restored);
        }
        self.collect_responses();
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use terminal_buffer::text_attribute::{TextAttribute, UnderlineStyle};
    use terminal_parser::{
        output_engine::OutputStateMachineEngine,
        state_machine::{Parameters, StateMachine, VtId},
    };

    #[test]
    fn microsoft_tabulation_stop_report_flows_parser_through_reporting_product_state() {
        let dispatch = AdaptDispatchReportingState::new(PageGeometry::new(0, 80, 24));
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        machine.process_str("\u{1b}[2$w");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P2$u9/17/25/33/41/49/57/65/73\u{1b}\\"
        );

        machine.engine_mut().dispatch_mut().clear_response();
        machine.engine_mut().dispatch_mut().set_text_width(132);
        machine.process_str("\u{1b}[2$w");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P2$u9/17/25/33/41/49/57/65/73/81/89/97/105/113/121/129\u{1b}\\"
        );

        machine.engine_mut().dispatch_mut().clear_response();
        machine.engine_mut().dispatch_mut().set_text_width(80);
        machine.process_str("\u{1b}P2$t30/60/120/240\u{1b}\\\u{1b}[2$w");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P2$u30/60\u{1b}\\"
        );

        machine.engine_mut().dispatch_mut().clear_response();
        machine.engine_mut().dispatch_mut().set_text_width(132);
        machine.process_str("\u{1b}[2$w");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P2$u30/60/120\u{1b}\\"
        );

        machine.engine_mut().dispatch_mut().clear_response();
        machine.engine_mut().dispatch_mut().set_text_width(80);
        for (restore, expected) in [
            ("44/22/66", "\u{1b}P2$u22/44/66\u{1b}\\"),
            ("3//7", "\u{1b}P2$u3/7\u{1b}\\"),
            ("0/5/10", "\u{1b}P2$u5/10\u{1b}\\"),
            ("1/8/18", "\u{1b}P2$u8/18\u{1b}\\"),
        ] {
            machine.process_str(&format!("\u{1b}P2$t{restore}\u{1b}\\\u{1b}[2$w"));
            assert_eq!(machine.engine().dispatch().response(), expected);
            machine.engine_mut().dispatch_mut().clear_response();
        }

        machine.process_str("\u{1b}[3g\u{1b}[2$w");
        assert_eq!(machine.engine().dispatch().response(), "\u{1b}P2$u\u{1b}\\");
    }

    #[test]
    fn microsoft_cursor_information_report_uses_live_product_and_terminal_output_state() {
        let dispatch = AdaptDispatchReportingState::new(PageGeometry::new(20, 100, 29));
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        machine.process_str("\u{1b}[1$w");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P1$u1;1;1;@;@;@;0;2;@;BBBB\u{1b}\\"
        );
        machine.engine_mut().dispatch_mut().clear_response();

        let mut attributes = TextAttribute::default();
        attributes.set_intense(true);
        attributes.set_underline_style(UnderlineStyle::Single);
        attributes.set_blinking(true);
        attributes.set_reverse_video(true);
        attributes.set_invisible(true);
        attributes.set_protected(true);
        machine
            .engine_mut()
            .dispatch_mut()
            .product_mut()
            .response_state_mut()
            .presentation_mut()
            .set_current_attributes(attributes);
        machine.process_str("\u{1b}[?6h");
        machine
            .engine_mut()
            .dispatch_mut()
            .dispatch(OutputAction::SingleShift(3));
        machine
            .engine_mut()
            .dispatch_mut()
            .dispatch(OutputAction::CursorForward(999));
        machine
            .engine_mut()
            .dispatch_mut()
            .dispatch(OutputAction::Print(u16::from(b'*')));
        machine
            .engine_mut()
            .dispatch_mut()
            .dispatch(OutputAction::LockingShift(1));
        machine
            .engine_mut()
            .dispatch_mut()
            .dispatch(OutputAction::LockingShiftRight(3));
        for action in [
            OutputAction::Designate94Charset {
                slot: 0,
                charset: VtId::from_ascii("%5").value(),
            },
            OutputAction::Designate96Charset {
                slot: 1,
                charset: VtId::from_ascii("H").value(),
            },
            OutputAction::Designate96Charset {
                slot: 2,
                charset: VtId::from_ascii("M").value(),
            },
            OutputAction::Designate96Charset {
                slot: 3,
                charset: VtId::from_ascii("B").value(),
            },
        ] {
            machine.engine_mut().dispatch_mut().dispatch(action);
        }
        machine.process_str("\u{1b}[1$w");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P1$u1;100;1;_;A;I;1;3;N;%5HMB\u{1b}\\"
        );
    }

    #[test]
    fn microsoft_cursor_information_restore_mutates_live_cursor_attributes_and_flags() {
        let dispatch = AdaptDispatchReportingState::new(PageGeometry::new(20, 100, 29));
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        machine.process_str("\u{1b}P1$t3;4;1;J;A;J;1;3;N;%5HMB\u{1b}\\");
        let dispatch = machine.engine().dispatch();
        let presentation = dispatch.product().response_state().presentation();
        assert_eq!(presentation.core().cursor(), Point { x: 3, y: 22 });
        assert!(!presentation.core().origin_mode());
        assert!(presentation.core().delayed_eol_wrap());
        assert!(presentation.current_attributes().is_underlined());
        assert!(presentation.current_attributes().is_reverse_video());
        assert!(presentation.current_attributes().is_protected());
        assert_eq!(
            dispatch
                .presentation_reports()
                .cursor_information()
                .single_shift(),
            Some(2)
        );
        assert_eq!(
            dispatch
                .presentation_reports()
                .cursor_information()
                .locking_shifts(),
            (1, 3)
        );
        assert_eq!(
            dispatch
                .presentation_reports()
                .cursor_information()
                .charsets(),
            &[
                "%5".to_owned(),
                "H".to_owned(),
                "M".to_owned(),
                "B".to_owned()
            ]
        );

        machine.engine_mut().dispatch_mut().clear_response();
        machine.process_str("\u{1b}[1$w");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P1$u3;4;1;J;A;J;1;3;N;%5HMB\u{1b}\\"
        );
    }

    #[test]
    fn tabulation_stop_report_sink_failure_remains_deferred_at_product_boundary() {
        let mut dispatch = AdaptDispatchReportingState::new(PageGeometry::new(0, 80, 24));
        dispatch.set_response_writable(false);
        for selector in [1, 2] {
            dispatch.dispatch(OutputAction::AdvancedCsi {
                id: VtId::from_ascii("$w"),
                parameters: Parameters::from_values(vec![Some(selector)]),
            });
        }

        assert!(dispatch.response().is_empty());
        assert_eq!(
            dispatch
                .product()
                .response_state()
                .presentation()
                .core()
                .deferred_actions()
                .len(),
            2
        );
    }

    #[test]
    fn existing_product_responses_keep_order_with_presentation_reports() {
        let mut dispatch = AdaptDispatchReportingState::new(PageGeometry::new(0, 80, 24));
        dispatch.dispatch(OutputAction::DeviceStatusReport {
            private: false,
            status: 5,
            id: None,
        });
        dispatch.dispatch(OutputAction::AdvancedCsi {
            id: VtId::from_ascii("$w"),
            parameters: Parameters::from_values(vec![Some(2)]),
        });

        assert_eq!(
            dispatch.response(),
            "\u{1b}[0n\u{1b}P2$u9/17/25/33/41/49/57/65/73\u{1b}\\"
        );
    }
}
