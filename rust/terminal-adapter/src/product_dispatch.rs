//! Final portable product aggregate for adapter response-producing behavior.
//!
//! `AdaptDispatchResponseState` owns the ordinary VT response path,
//! `ChecksumReportEngine` owns DECRQCRA state and serialization,
//! `MacroReportEngine` owns DECDMAC storage plus DSR 62/63,
//! `UserPreferenceCharsetEngine` owns DECAUPSS/DECRQUPSS,
//! `WindowReportEngine` owns deterministic window-size reports, and
//! `TerminalInputDispatchState` owns Adapter-to-TerminalInput mode coupling.
//! This aggregate is the single `TermDispatch` surface that composes those
//! owners, preventing parser and product state from becoming disconnected
//! reporting copies.

use terminal_parser::{
    output_engine::{DcsAction, OutputAction, TermDispatch},
    state_machine::VtId,
};

use crate::{
    adapt_dispatch::PageGeometry, checksum_reports::ChecksumReportEngine,
    decrqss_color_alias::ColorAliasIndices, input_mode_dispatch::TerminalInputDispatchState,
    macro_reports::MacroReportEngine, response_dispatch::AdaptDispatchResponseState,
    user_preference_charset::UserPreferenceCharsetEngine, window_reports::WindowReportEngine,
};

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
enum DcsOwner {
    #[default]
    None,
    Response,
    Macro,
    UserPreferenceCharset,
}

pub struct AdaptDispatchProductState {
    responses: AdaptDispatchResponseState,
    checksum_reports: ChecksumReportEngine,
    input_modes: TerminalInputDispatchState,
    macros: MacroReportEngine,
    user_preference_charset: UserPreferenceCharsetEngine,
    window_reports: WindowReportEngine,
    outbound: String,
    writable: bool,
    dcs_owner: DcsOwner,
}

impl AdaptDispatchProductState {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        Self {
            responses: AdaptDispatchResponseState::new(geometry),
            checksum_reports: ChecksumReportEngine::new(geometry),
            input_modes: TerminalInputDispatchState::default(),
            macros: MacroReportEngine::default(),
            user_preference_charset: UserPreferenceCharsetEngine::default(),
            window_reports: WindowReportEngine::new(geometry),
            outbound: String::new(),
            writable: true,
            dcs_owner: DcsOwner::None,
        }
    }

    #[must_use]
    pub const fn response_state(&self) -> &AdaptDispatchResponseState {
        &self.responses
    }

    pub const fn response_state_mut(&mut self) -> &mut AdaptDispatchResponseState {
        &mut self.responses
    }

    #[must_use]
    pub const fn checksum_reports(&self) -> &ChecksumReportEngine {
        &self.checksum_reports
    }

    #[must_use]
    pub const fn input_modes(&self) -> &TerminalInputDispatchState {
        &self.input_modes
    }

    pub const fn input_modes_mut(&mut self) -> &mut TerminalInputDispatchState {
        &mut self.input_modes
    }

    #[must_use]
    pub const fn macro_reports(&self) -> &MacroReportEngine {
        &self.macros
    }

    #[must_use]
    pub const fn user_preference_charset(&self) -> &UserPreferenceCharsetEngine {
        &self.user_preference_charset
    }

    #[must_use]
    pub const fn window_reports(&self) -> &WindowReportEngine {
        &self.window_reports
    }

    #[must_use]
    pub fn response(&self) -> &str {
        &self.outbound
    }

    pub fn clear_response(&mut self) {
        self.outbound.clear();
        self.responses.clear_response();
        self.checksum_reports.clear_response();
        self.macros.clear_response();
        self.user_preference_charset.clear_response();
        self.window_reports.clear_response();
    }

    pub const fn set_response_writable(&mut self, writable: bool) {
        self.writable = writable;
        self.responses.set_response_writable(writable);
        self.checksum_reports.set_response_writable(writable);
        self.macros.set_response_writable(writable);
        self.user_preference_charset.set_response_writable(writable);
        self.window_reports.set_response_writable(writable);
    }

    pub const fn set_checksum_report_enabled(&mut self, enabled: bool) {
        self.checksum_reports.set_enabled(enabled);
    }

    pub const fn set_color_alias_indices(&mut self, aliases: ColorAliasIndices) {
        self.responses.set_color_alias_indices(aliases);
        self.checksum_reports.set_color_alias_indices(aliases);
    }

    fn collect_responses(&mut self) {
        if !self.responses.response().is_empty() {
            self.outbound.push_str(self.responses.response());
            self.responses.clear_response();
        }
        if !self.checksum_reports.response().is_empty() {
            self.outbound.push_str(self.checksum_reports.response());
            self.checksum_reports.clear_response();
        }
        if !self.macros.response().is_empty() {
            self.outbound.push_str(self.macros.response());
            self.macros.clear_response();
        }
        if !self.user_preference_charset.response().is_empty() {
            self.outbound
                .push_str(self.user_preference_charset.response());
            self.user_preference_charset.clear_response();
        }
        if !self.window_reports.response().is_empty() {
            self.outbound.push_str(self.window_reports.response());
            self.window_reports.clear_response();
        }
    }

    fn dispatch_macro_report(&mut self, status: i32, id: Option<i32>) {
        let action = OutputAction::DeviceStatusReport {
            private: true,
            status,
            id,
        };

        if self.writable {
            self.macros.dispatch(action);
            self.collect_responses();
        } else {
            // Preserve the same fail-closed behavior as every other response:
            // an unwritable sink leaves the request visible as deferred work.
            self.responses.dispatch(action);
        }
    }

    fn dispatch_user_preference_report(
        &mut self,
        id: VtId,
        parameters: terminal_parser::state_machine::Parameters,
    ) {
        let action = OutputAction::AdvancedCsi { id, parameters };
        if self.writable {
            self.user_preference_charset.dispatch(action);
            self.collect_responses();
        } else {
            self.responses.dispatch(action);
        }
    }

    fn dispatch_checksum_report(
        &mut self,
        id: VtId,
        parameters: terminal_parser::state_machine::Parameters,
    ) {
        if self.writable && self.checksum_reports.request(&parameters) {
            self.collect_responses();
        } else {
            self.responses
                .dispatch(OutputAction::AdvancedCsi { id, parameters });
        }
    }

    fn dispatch_window_report(&mut self, function: i32, parameter1: i32, parameter2: i32) {
        let action = OutputAction::WindowManipulation {
            function,
            parameter1,
            parameter2,
        };
        if self.writable {
            self.window_reports.dispatch(action);
            self.collect_responses();
        } else {
            self.responses.dispatch(action);
        }
    }

    fn dispatch_input_mode(&mut self, action: OutputAction) {
        match action {
            OutputAction::SetMode {
                private,
                mode,
                enabled,
            } => {
                self.input_modes.dispatch(OutputAction::SetMode {
                    private,
                    mode,
                    enabled,
                });
                if !self
                    .responses
                    .record_report_only_mode(private, mode, enabled)
                {
                    self.responses.dispatch(OutputAction::SetMode {
                        private,
                        mode,
                        enabled,
                    });
                }
            }
            other => self.input_modes.dispatch(other),
        }
    }

    fn record_print(&mut self, text: &[u16]) {
        let attributes = self.responses.presentation().current_attributes();
        self.checksum_reports.write_text(text, attributes);
    }
}

impl TermDispatch for AdaptDispatchProductState {
    fn dispatch(&mut self, action: OutputAction) {
        if TerminalInputDispatchState::handles(&action) {
            self.dispatch_input_mode(action);
            return;
        }

        match action {
            OutputAction::Print(unit) => {
                self.record_print(&[unit]);
                self.responses.dispatch(OutputAction::Print(unit));
            }
            OutputAction::PrintString(text) => {
                self.record_print(&text);
                self.responses.dispatch(OutputAction::PrintString(text));
            }
            OutputAction::DeviceStatusReport {
                private: true,
                status: status @ (62 | 63),
                id,
            } => self.dispatch_macro_report(status, id),
            OutputAction::AdvancedCsi { id, parameters } if id == VtId::from_ascii("&u") => {
                self.dispatch_user_preference_report(id, parameters);
            }
            OutputAction::AdvancedCsi { id, parameters } if ChecksumReportEngine::handles(id) => {
                self.dispatch_checksum_report(id, parameters);
            }
            OutputAction::WindowManipulation {
                function,
                parameter1,
                parameter2,
            } if WindowReportEngine::handles(function) => {
                self.dispatch_window_report(function, parameter1, parameter2);
            }
            other => {
                self.responses.dispatch(other);
                self.collect_responses();
            }
        }
    }

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        self.dcs_owner = DcsOwner::None;
        match action {
            action @ DcsAction::DefineMacro(_) => {
                if self.macros.begin_dcs(action) {
                    self.dcs_owner = DcsOwner::Macro;
                    true
                } else {
                    false
                }
            }
            action @ DcsAction::AssignUserPreferenceCharset(_) => {
                if self.user_preference_charset.begin_dcs(action) {
                    self.dcs_owner = DcsOwner::UserPreferenceCharset;
                    true
                } else {
                    false
                }
            }
            other => {
                if self.responses.begin_dcs(other) {
                    self.dcs_owner = DcsOwner::Response;
                    true
                } else {
                    false
                }
            }
        }
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        let result = match self.dcs_owner {
            DcsOwner::Response => self.responses.dcs_put(code_unit),
            DcsOwner::Macro => self.macros.dcs_put(code_unit),
            DcsOwner::UserPreferenceCharset => self.user_preference_charset.dcs_put(code_unit),
            DcsOwner::None => false,
        };
        self.collect_responses();
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{macro_buffer::MAX_SPACE, user_preference_charset::CharsetSize};
    use terminal_buffer::text_attribute::TextAttribute;
    use terminal_input::Mode;
    use terminal_parser::{
        output_engine::OutputStateMachineEngine,
        state_machine::{Parameters, StateMachine},
    };

    #[test]
    fn microsoft_macro_space_report_uses_macros_defined_through_product_dispatch() {
        let dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 80, 24));
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        for id in 1..=4 {
            machine.process_str(&format!("\u{1b}P{id};0;0!z12345678\u{1b}\\"));
        }

        let dispatch = machine.engine_mut().dispatch_mut();
        assert_eq!(
            dispatch.macro_reports().buffer().space_available(),
            MAX_SPACE - 32
        );
        dispatch.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 62,
            id: None,
        });
        assert_eq!(
            dispatch.response(),
            format!("\u{1b}[{}*{{", (MAX_SPACE / 16) - 2)
        );
        assert!(
            dispatch
                .response_state()
                .presentation()
                .core()
                .deferred_actions()
                .is_empty()
        );
    }

    #[test]
    fn microsoft_macro_checksum_report_uses_live_product_macro_memory_and_request_id() {
        let dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 80, 24));
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));
        machine.process_str("\u{1b}P1;0;0!zABC\u{1b}\\");

        let dispatch = machine.engine_mut().dispatch_mut();
        let checksum = dispatch.macro_reports().buffer().calculate_checksum();
        dispatch.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 63,
            id: Some(12),
        });
        assert_eq!(
            dispatch.response(),
            format!("\u{1b}P12!~{checksum:04X}\u{1b}\\")
        );
        assert!(
            dispatch
                .response_state()
                .presentation()
                .core()
                .deferred_actions()
                .is_empty()
        );
    }

    #[test]
    fn microsoft_user_preference_charset_round_trips_through_product_dispatch() {
        let dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 80, 24));
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        machine.process_str("\u{1b}P0!u%5\u{1b}\\");
        machine.process_str("\u{1b}[&u");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P0!u%5\u{1b}\\"
        );
        assert_eq!(
            machine
                .engine()
                .dispatch()
                .user_preference_charset()
                .state()
                .size(),
            CharsetSize::Size94
        );
        assert_eq!(
            machine
                .engine()
                .dispatch()
                .user_preference_charset()
                .state()
                .id(),
            "%5"
        );

        machine.engine_mut().dispatch_mut().clear_response();
        machine.process_str("\u{1b}P1!uA\u{1b}\\\u{1b}[&u");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P1!uA\u{1b}\\"
        );
        assert_eq!(
            machine
                .engine()
                .dispatch()
                .user_preference_charset()
                .state()
                .size(),
            CharsetSize::Size96
        );
    }

    #[test]
    fn microsoft_window_manipulation_reports_flow_parser_to_live_product_geometry() {
        let dispatch = AdaptDispatchProductState::new(PageGeometry::new(20, 100, 29));
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        machine.process_str("\u{1b}[18t\u{1b}[14t\u{1b}[16t");

        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}[8;29;100t\u{1b}[4;580;1000t\u{1b}[6;20;10t"
        );
        assert!(
            machine
                .engine()
                .dispatch()
                .response_state()
                .presentation()
                .core()
                .deferred_actions()
                .is_empty()
        );
    }

    #[test]
    fn microsoft_decrqcra_flows_parser_to_live_product_checksum_state() {
        let mut dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 100, 29));
        dispatch.set_checksum_report_enabled(true);
        dispatch
            .response_state_mut()
            .presentation_mut()
            .set_current_attributes(TextAttribute::default());
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        machine.process_str("ABC");
        machine.process_str("\u{1b}[99;1;1;1;1;3*y");

        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P99!~FDEA\u{1b}\\"
        );
    }

    #[test]
    fn decrqcra_sink_failure_remains_deferred_at_product_boundary() {
        let mut dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 80, 24));
        dispatch.set_checksum_report_enabled(true);
        dispatch.set_response_writable(false);
        dispatch.dispatch(OutputAction::AdvancedCsi {
            id: VtId::from_ascii("*y"),
            parameters: Parameters::from_values(vec![
                Some(99),
                Some(1),
                Some(1),
                Some(1),
                Some(1),
                Some(1),
            ]),
        });

        assert!(dispatch.response().is_empty());
        assert_eq!(
            dispatch
                .response_state()
                .presentation()
                .core()
                .deferred_actions()
                .len(),
            1
        );
    }

    #[test]
    fn microsoft_cursor_key_mode_updates_live_terminal_input_without_deferred_work() {
        let dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 80, 24));
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        machine.process_str("\u{1b}[?1h");
        assert!(
            machine
                .engine()
                .dispatch()
                .input_modes()
                .input()
                .get_input_mode(Mode::CursorKey)
        );
        machine.process_str("\u{1b}[?1l");
        assert!(
            !machine
                .engine()
                .dispatch()
                .input_modes()
                .input()
                .get_input_mode(Mode::CursorKey)
        );
        machine.process_str("\u{1b}[?1h");
        machine
            .engine_mut()
            .dispatch_mut()
            .dispatch(OutputAction::RequestMode {
                private: true,
                mode: 1,
            });
        assert_eq!(machine.engine().dispatch().response(), "\u{1b}[?1;1$y");
        assert!(
            machine
                .engine()
                .dispatch()
                .response_state()
                .presentation()
                .core()
                .deferred_actions()
                .is_empty()
        );
    }

    #[test]
    fn microsoft_keypad_mode_flows_parser_to_live_terminal_input() {
        let dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 80, 24));
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        machine.process_str("\u{1b}=");
        assert!(
            machine
                .engine()
                .dispatch()
                .input_modes()
                .input()
                .get_input_mode(Mode::Keypad)
        );
        machine.process_str("\u{1b}>");
        assert!(
            !machine
                .engine()
                .dispatch()
                .input_modes()
                .input()
                .get_input_mode(Mode::Keypad)
        );
        machine.process_str("\u{1b}=");
        assert!(
            machine
                .engine()
                .dispatch()
                .input_modes()
                .input()
                .get_input_mode(Mode::Keypad)
        );
        assert!(
            machine
                .engine()
                .dispatch()
                .response_state()
                .presentation()
                .core()
                .deferred_actions()
                .is_empty()
        );
    }

    #[test]
    fn microsoft_mouse_mode_matrix_flows_parser_to_live_terminal_input() {
        let cases = [
            (1000, Mode::DefaultMouseTracking),
            (1005, Mode::Utf8MouseEncoding),
            (1006, Mode::SgrMouseEncoding),
            (1002, Mode::ButtonEventMouseTracking),
            (1003, Mode::AnyEventMouseTracking),
            (1007, Mode::AlternateScroll),
        ];

        for (mode, input_mode) in cases {
            let dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 80, 24));
            let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

            machine.process_str(&format!("\u{1b}[?{mode}l"));
            assert!(
                !machine
                    .engine()
                    .dispatch()
                    .input_modes()
                    .input()
                    .get_input_mode(input_mode),
                "mouse mode {mode} should reset"
            );
            machine.process_str(&format!("\u{1b}[?{mode}h"));
            assert!(
                machine
                    .engine()
                    .dispatch()
                    .input_modes()
                    .input()
                    .get_input_mode(input_mode),
                "mouse mode {mode} should set"
            );
            assert!(
                machine
                    .engine()
                    .dispatch()
                    .response_state()
                    .presentation()
                    .core()
                    .deferred_actions()
                    .is_empty(),
                "mouse mode {mode} should be consumed by terminal-input"
            );
        }
    }

    #[test]
    fn send_c1_sequences_update_live_terminal_input_without_deferred_work() {
        let dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 80, 24));
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        machine.process_str("\u{1b} G");
        assert!(
            machine
                .engine()
                .dispatch()
                .input_modes()
                .input()
                .get_input_mode(Mode::SendC1)
        );
        machine.process_str("\u{1b} F");
        assert!(
            !machine
                .engine()
                .dispatch()
                .input_modes()
                .input()
                .get_input_mode(Mode::SendC1)
        );
        assert!(
            machine
                .engine()
                .dispatch()
                .response_state()
                .presentation()
                .core()
                .deferred_actions()
                .is_empty()
        );
    }

    #[test]
    fn window_report_sink_failure_remains_deferred_at_product_boundary() {
        let mut dispatch = AdaptDispatchProductState::new(PageGeometry::new(20, 100, 29));
        dispatch.set_response_writable(false);
        dispatch.dispatch(OutputAction::WindowManipulation {
            function: 18,
            parameter1: 0,
            parameter2: 0,
        });

        assert!(dispatch.response().is_empty());
        assert_eq!(
            dispatch
                .response_state()
                .presentation()
                .core()
                .deferred_actions()
                .len(),
            1
        );
    }

    #[test]
    fn user_preference_report_sink_failure_remains_deferred_at_product_boundary() {
        let mut dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 80, 24));
        assert!(dispatch.begin_dcs(DcsAction::AssignUserPreferenceCharset(
            Parameters::from_values(vec![Some(1)])
        )));
        assert!(dispatch.dcs_put(u16::from(b'A')));
        assert!(!dispatch.dcs_put(0x1b));

        dispatch.set_response_writable(false);
        dispatch.dispatch(OutputAction::AdvancedCsi {
            id: VtId::from_ascii("&u"),
            parameters: Parameters::default(),
        });

        assert!(dispatch.response().is_empty());
        assert_eq!(
            dispatch
                .response_state()
                .presentation()
                .core()
                .deferred_actions()
                .len(),
            1
        );
    }

    #[test]
    fn macro_report_sink_failure_remains_deferred_at_the_product_boundary() {
        let mut dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 80, 24));
        dispatch.set_response_writable(false);
        dispatch.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 62,
            id: None,
        });
        dispatch.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 63,
            id: Some(7),
        });

        assert!(dispatch.response().is_empty());
        assert_eq!(
            dispatch
                .response_state()
                .presentation()
                .core()
                .deferred_actions()
                .len(),
            2
        );
    }

    #[test]
    fn ordinary_response_order_is_preserved_across_the_composed_product_sink() {
        let mut dispatch = AdaptDispatchProductState::new(PageGeometry::new(0, 80, 24));
        dispatch.dispatch(OutputAction::DeviceStatusReport {
            private: false,
            status: 5,
            id: None,
        });
        dispatch.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 62,
            id: None,
        });
        dispatch.dispatch(OutputAction::RequestTerminalParameters(0));

        assert_eq!(
            dispatch.response(),
            format!(
                "\u{1b}[0n\u{1b}[{}*{{\u{1b}[2;1;1;128;128;1;0x",
                MAX_SPACE / 16
            )
        );
    }
}
