//! Product-level adapter response dispatch.
//!
//! This owner wires parser response-producing actions into the portable VT
//! response serializer while retaining the existing presentation-state owner
//! for cursor, modes, and rendition semantics.

use terminal_parser::{
    output_engine::{DcsAction, DeviceAttributesKind, OutputAction, TermDispatch},
    state_machine::Parameters,
};

use crate::{
    adapt_dispatch::PageGeometry,
    decrqss::{DecrqssState, serialize_request_setting},
    decrqss_color_alias::{ColorAliasIndices, serialize_decac},
    decrqss_cursor::{
        CursorShape, CursorStyleState, serialize_character_protection, serialize_cursor_style,
    },
    presentation_state::AdaptDispatchPresentationState,
    vt_response::VtResponseEngine,
};

const ESC: u16 = 0x1b;
const PERMANENT_GRAPHEME_CLUSTER_MODE: i32 = 2027;
const STANDARD_REPORT_ONLY_MODES: &[i32] = &[20];
const PRIVATE_REPORT_ONLY_MODES: &[i32] = &[
    1, 3, 5, 8, 12, 40, 66, 67, 1000, 1002, 1003, 1004, 1005, 1006, 1007, 1049, 2004, 9001,
];

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AdaptDispatchResponseState {
    presentation: AdaptDispatchPresentationState,
    responses: VtResponseEngine,
    clipboard_supported: bool,
    viewport_left: i32,
    active_page: i32,
    visible_page: i32,
    cursor_style: CursorStyleState,
    color_aliases: ColorAliasIndices,
    request_setting_buffer: Option<String>,
    report_only_modes: Vec<(bool, i32, bool)>,
}

impl AdaptDispatchResponseState {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        let mut presentation = AdaptDispatchPresentationState::new(geometry);
        presentation.dispatch(OutputAction::SetMode {
            private: true,
            mode: 64,
            enabled: true,
        });
        Self {
            presentation,
            responses: VtResponseEngine::default(),
            clipboard_supported: true,
            viewport_left: 0,
            active_page: 1,
            visible_page: 1,
            cursor_style: CursorStyleState::default(),
            color_aliases: ColorAliasIndices::default(),
            request_setting_buffer: None,
            report_only_modes: Vec::new(),
        }
    }

    #[must_use]
    pub const fn presentation(&self) -> &AdaptDispatchPresentationState {
        &self.presentation
    }
    pub const fn presentation_mut(&mut self) -> &mut AdaptDispatchPresentationState {
        &mut self.presentation
    }
    #[must_use]
    pub fn response(&self) -> &str {
        self.responses.response()
    }
    pub fn clear_response(&mut self) {
        self.responses.clear();
    }
    pub const fn set_clipboard_supported(&mut self, supported: bool) {
        self.clipboard_supported = supported;
    }
    pub const fn set_response_writable(&mut self, writable: bool) {
        self.responses.set_writable(writable);
    }
    pub fn set_viewport_left(&mut self, left: i32) {
        self.viewport_left = left.max(0);
    }
    pub const fn set_color_alias_indices(&mut self, aliases: ColorAliasIndices) {
        self.color_aliases = aliases;
    }

    /// Records a DECRQM-reportable mode whose real side effect is owned by a
    /// sibling product component (for example `terminal-input`). Returning
    /// `false` means this response owner does not recognize the mode and the
    /// caller should preserve the action for another owner instead.
    pub fn record_report_only_mode(&mut self, private: bool, mode: i32, enabled: bool) -> bool {
        if !Self::is_report_only_mode(private, mode) {
            return false;
        }
        self.set_report_only_mode(private, mode, enabled);
        true
    }

    fn is_report_only_mode(private: bool, mode: i32) -> bool {
        if private {
            PRIVATE_REPORT_ONLY_MODES.contains(&mode)
        } else {
            STANDARD_REPORT_ONLY_MODES.contains(&mode)
        }
    }

    fn report_only_mode_status(&self, private: bool, mode: i32) -> Option<bool> {
        if !Self::is_report_only_mode(private, mode) {
            return None;
        }

        Some(
            self.report_only_modes
                .iter()
                .find(|entry| entry.0 == private && entry.1 == mode)
                .is_some_and(|entry| entry.2),
        )
    }

    fn set_report_only_mode(&mut self, private: bool, mode: i32, enabled: bool) {
        if let Some(entry) = self
            .report_only_modes
            .iter_mut()
            .find(|entry| entry.0 == private && entry.1 == mode)
        {
            entry.2 = enabled;
        } else {
            self.report_only_modes.push((private, mode, enabled));
        }
    }

    fn device_status_report(&mut self, private: bool, status: i32, id: Option<i32>) -> bool {
        match (private, status) {
            (false, 5) => self.responses.operating_status(),
            (false, 6) => {
                let cursor = self.presentation.core().cursor();
                let viewport_top = self.presentation.core().geometry().top;
                self.responses
                    .cursor_position_report(cursor.x, cursor.y, viewport_top)
            }
            (true, 5) => self.responses.return_response("\u{1b}[?0n"),
            (true, 6) => {
                let cursor = self.presentation.core().cursor();
                let viewport_top = self.presentation.core().geometry().top;
                self.responses.extended_cursor_position_report(
                    cursor.x,
                    cursor.y,
                    viewport_top,
                    id.unwrap_or(1),
                )
            }
            (true, 15) => self.responses.return_response("\u{1b}[?13n"),
            (true, 25) => self.responses.return_response("\u{1b}[?20n"),
            (true, 26) => self.responses.return_response("\u{1b}[?27;1;0;0n"),
            (true, 53) => self.responses.return_response("\u{1b}[?53n"),
            (true, 55) => self.responses.return_response("\u{1b}[?57;0n"),
            (true, 75) => self.responses.return_response("\u{1b}[?70n"),
            (true, 85) => self.responses.return_response("\u{1b}[?83n"),
            _ => false,
        }
    }

    fn device_attributes(&mut self, kind: DeviceAttributesKind) -> bool {
        match kind {
            DeviceAttributesKind::Primary => self
                .responses
                .primary_device_attributes(self.clipboard_supported),
            DeviceAttributesKind::Secondary => self.responses.secondary_device_attributes(),
            DeviceAttributesKind::Tertiary => self.responses.tertiary_device_attributes(),
            DeviceAttributesKind::Vt52 => false,
        }
    }

    fn request_displayed_extent(&mut self) -> bool {
        let geometry = self.presentation.core().geometry();
        self.responses.displayed_extent(
            geometry.height,
            geometry.width,
            self.viewport_left,
            self.visible_page,
        )
    }

    fn request_mode(&mut self, private: bool, mode: i32) -> bool {
        if private && mode == PERMANENT_GRAPHEME_CLUSTER_MODE {
            return self.responses.mode_report_state(true, mode, 3);
        }

        let status = if private && mode == 25 {
            Some(self.presentation.cursor_visible())
        } else {
            self.presentation
                .core()
                .mode_status(private, mode)
                .or_else(|| self.report_only_mode_status(private, mode))
        };
        status.is_some_and(|enabled| self.responses.mode_report(private, mode, enabled))
    }

    fn page_position_absolute(&mut self, page: i32) {
        self.active_page = page.max(1);
        if self.presentation.core().page_cursor_coupling_mode() {
            self.visible_page = self.active_page;
        }
    }

    fn set_cursor_style(&mut self, parameter: i32) {
        self.cursor_style = match parameter {
            1 => CursorStyleState {
                shape: CursorShape::Block,
                blinking: true,
            },
            2 => CursorStyleState {
                shape: CursorShape::Block,
                blinking: false,
            },
            3 => CursorStyleState {
                shape: CursorShape::Underline,
                blinking: true,
            },
            4 => CursorStyleState {
                shape: CursorShape::Underline,
                blinking: false,
            },
            5 => CursorStyleState {
                shape: CursorShape::Bar,
                blinking: true,
            },
            6 => CursorStyleState {
                shape: CursorShape::Bar,
                blinking: false,
            },
            _ => CursorStyleState::default(),
        };
    }

    fn set_character_protection(&mut self, parameters: &Parameters) {
        let protected = parameters.at(0).unwrap_or(0) == 1;
        let mut attributes = self.presentation.current_attributes();
        attributes.set_protected(protected);
        self.presentation.set_current_attributes(attributes);
    }

    fn request_setting_response(&self, setting_id: &str) -> String {
        let core = self.presentation.core();
        let state = DecrqssState {
            geometry: core.geometry(),
            margins: core.margins(),
            attributes: self.presentation.current_attributes(),
        };

        match setting_id {
            " q" => serialize_cursor_style(self.cursor_style),
            "\"q" => serialize_character_protection(state.attributes.is_protected()),
            _ => {
                if let Some(item) = setting_id.strip_suffix(",|") {
                    let item = if item.is_empty() {
                        None
                    } else if let Ok(item) = item.parse::<u16>() {
                        Some(item)
                    } else {
                        return serialize_request_setting(setting_id, state);
                    };
                    serialize_decac(item, self.color_aliases)
                } else {
                    serialize_request_setting(setting_id, state)
                }
            }
        }
    }

    fn finish_request_setting(&mut self) -> bool {
        let Some(setting_id) = self.request_setting_buffer.take() else {
            return false;
        };
        let response = self.request_setting_response(&setting_id);
        self.responses.return_response(&response)
    }
}

impl TermDispatch for AdaptDispatchResponseState {
    fn dispatch(&mut self, action: OutputAction) {
        match action {
            OutputAction::DeviceStatusReport {
                private,
                status,
                id,
            } => {
                if !self.device_status_report(private, status, id) {
                    self.presentation
                        .dispatch(OutputAction::DeviceStatusReport {
                            private,
                            status,
                            id,
                        });
                }
            }
            OutputAction::DeviceAttributes(kind) => {
                if !self.device_attributes(kind) {
                    self.presentation
                        .dispatch(OutputAction::DeviceAttributes(kind));
                }
            }
            OutputAction::RequestTerminalParameters(permission) => {
                if !self.responses.terminal_parameters(permission) {
                    self.presentation
                        .dispatch(OutputAction::RequestTerminalParameters(permission));
                }
            }
            OutputAction::RequestDisplayedExtent => {
                if !self.request_displayed_extent() {
                    self.presentation
                        .dispatch(OutputAction::RequestDisplayedExtent);
                }
            }
            OutputAction::RequestMode { private, mode } => {
                if !self.request_mode(private, mode) {
                    self.presentation
                        .dispatch(OutputAction::RequestMode { private, mode });
                }
            }
            OutputAction::PagePositionAbsolute(page) => {
                self.page_position_absolute(page);
                self.presentation
                    .dispatch(OutputAction::PagePositionAbsolute(page));
            }
            OutputAction::SetCursorStyle(parameter) => {
                self.set_cursor_style(parameter);
                self.presentation
                    .dispatch(OutputAction::SetCursorStyle(parameter));
            }
            OutputAction::SetCharacterProtectionAttribute(parameters) => {
                self.set_character_protection(&parameters);
            }
            OutputAction::SetMode {
                private: true,
                mode: PERMANENT_GRAPHEME_CLUSTER_MODE,
                ..
            } => {}
            OutputAction::SetMode {
                private: true,
                mode: 64,
                enabled,
            } => {
                let was_coupled = self.presentation.core().page_cursor_coupling_mode();
                self.presentation.dispatch(OutputAction::SetMode {
                    private: true,
                    mode: 64,
                    enabled,
                });
                if enabled && !was_coupled {
                    self.visible_page = self.active_page;
                }
            }
            OutputAction::SetMode {
                private,
                mode,
                enabled,
            } if Self::is_report_only_mode(private, mode) => {
                self.record_report_only_mode(private, mode, enabled);
                self.presentation.dispatch(OutputAction::SetMode {
                    private,
                    mode,
                    enabled,
                });
            }
            other => self.presentation.dispatch(other),
        }
    }

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        if action == DcsAction::RequestSetting {
            self.request_setting_buffer = Some(String::new());
            true
        } else {
            self.presentation.begin_dcs(action)
        }
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        let Some(buffer) = self.request_setting_buffer.as_mut() else {
            return self.presentation.dcs_put(code_unit);
        };

        if code_unit == ESC {
            return self.finish_request_setting();
        }

        if code_unit > 0x7f || buffer.len() >= 64 {
            self.request_setting_buffer = None;
            return false;
        }

        buffer.push(char::from(u8::try_from(code_unit).unwrap_or_default()));
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{adapt_dispatch::Point, decrqss_color_alias::ColorAliasIndices};
    use terminal_parser::{output_engine::OutputStateMachineEngine, state_machine::StateMachine};

    fn state() -> AdaptDispatchResponseState {
        let mut state = AdaptDispatchResponseState::new(PageGeometry::new(20, 100, 29));
        state
            .presentation_mut()
            .core_mut()
            .set_cursor(Point { x: 50, y: 34 });
        state
    }

    #[test]
    fn microsoft_operating_status_is_returned_through_adapter_dispatch() {
        let mut state = state();
        state.dispatch(OutputAction::DeviceStatusReport {
            private: false,
            status: 5,
            id: None,
        });
        assert_eq!(state.response(), "\u{1b}[0n");
    }

    #[test]
    fn microsoft_cpr_uses_live_cursor_and_viewport_state() {
        let mut state = state();
        state.dispatch(OutputAction::DeviceStatusReport {
            private: false,
            status: 6,
            id: None,
        });
        assert_eq!(state.response(), "\u{1b}[15;51R");
        state
            .presentation_mut()
            .core_mut()
            .set_cursor(Point { x: 51, y: 35 });
        state.dispatch(OutputAction::DeviceStatusReport {
            private: false,
            status: 6,
            id: None,
        });
        assert_eq!(state.response(), "\u{1b}[15;51R\u{1b}[16;52R");
    }

    #[test]
    fn microsoft_decxcpr_uses_reported_page_identifier() {
        let mut state = state();
        state.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 6,
            id: Some(1),
        });
        assert_eq!(state.response(), "\u{1b}[?15;51;1R");
        state.clear_response();
        state.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 6,
            id: Some(3),
        });
        assert_eq!(state.response(), "\u{1b}[?15;51;3R");
    }

    #[test]
    fn microsoft_private_status_reports_match_all_static_source_vectors() {
        let cases = [
            (5, "\u{1b}[?0n"),
            (15, "\u{1b}[?13n"),
            (25, "\u{1b}[?20n"),
            (26, "\u{1b}[?27;1;0;0n"),
            (53, "\u{1b}[?53n"),
            (55, "\u{1b}[?57;0n"),
            (75, "\u{1b}[?70n"),
            (85, "\u{1b}[?83n"),
        ];

        for (status, expected) in cases {
            let mut state = state();
            state.dispatch(OutputAction::DeviceStatusReport {
                private: true,
                status,
                id: None,
            });
            assert_eq!(state.response(), expected, "private DSR status {status}");
            assert!(state.presentation().core().deferred_actions().is_empty());
        }
    }

    #[test]
    fn microsoft_private_status_sink_failure_remains_deferred() {
        let mut state = state();
        state.set_response_writable(false);
        state.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 15,
            id: None,
        });
        assert!(state.response().is_empty());
        assert_eq!(state.presentation().core().deferred_actions().len(), 1);
    }

    #[test]
    fn microsoft_primary_device_attributes_uses_live_clipboard_capability() {
        let mut state = state();
        state.dispatch(OutputAction::DeviceAttributes(
            DeviceAttributesKind::Primary,
        ));
        assert_eq!(
            state.response(),
            "\u{1b}[?61;4;6;7;14;21;22;23;24;28;32;42;52c"
        );
        state.clear_response();
        state.set_clipboard_supported(false);
        state.dispatch(OutputAction::DeviceAttributes(
            DeviceAttributesKind::Primary,
        ));
        assert_eq!(
            state.response(),
            "\u{1b}[?61;4;6;7;14;21;22;23;24;28;32;42c"
        );
    }

    #[test]
    fn microsoft_secondary_and_tertiary_attributes_flow_through_adapter_dispatch() {
        let mut state = state();
        state.dispatch(OutputAction::DeviceAttributes(
            DeviceAttributesKind::Secondary,
        ));
        assert_eq!(state.response(), "\u{1b}[>0;10;1c");
        state.clear_response();
        state.dispatch(OutputAction::DeviceAttributes(
            DeviceAttributesKind::Tertiary,
        ));
        assert_eq!(state.response(), "\u{1b}P!|00000000\u{1b}\\");
    }

    #[test]
    fn microsoft_terminal_parameters_flow_through_adapter_dispatch() {
        let mut state = state();
        state.dispatch(OutputAction::RequestTerminalParameters(0));
        assert_eq!(state.response(), "\u{1b}[2;1;1;128;128;1;0x");
        state.clear_response();
        state.dispatch(OutputAction::RequestTerminalParameters(1));
        assert_eq!(state.response(), "\u{1b}[3;1;1;128;128;1;0x");
    }

    #[test]
    fn microsoft_displayed_extent_tracks_pan_visible_page_and_coupling() {
        let mut state = AdaptDispatchResponseState::new(PageGeometry::new(0, 80, 24));
        state.dispatch(OutputAction::RequestDisplayedExtent);
        assert_eq!(state.response(), "\u{1b}[24;80;1;1;1\"w");

        state.clear_response();
        state.set_viewport_left(5);
        state.dispatch(OutputAction::RequestDisplayedExtent);
        assert_eq!(state.response(), "\u{1b}[24;80;6;1;1\"w");

        state.clear_response();
        state.dispatch(OutputAction::PagePositionAbsolute(3));
        state.dispatch(OutputAction::RequestDisplayedExtent);
        assert_eq!(state.response(), "\u{1b}[24;80;6;1;3\"w");

        state.clear_response();
        state.dispatch(OutputAction::SetMode {
            private: true,
            mode: 64,
            enabled: false,
        });
        state.dispatch(OutputAction::PagePositionAbsolute(1));
        state.dispatch(OutputAction::RequestDisplayedExtent);
        assert_eq!(state.response(), "\u{1b}[24;80;6;1;3\"w");

        state.clear_response();
        state.dispatch(OutputAction::SetMode {
            private: true,
            mode: 64,
            enabled: true,
        });
        state.dispatch(OutputAction::RequestDisplayedExtent);
        assert_eq!(state.response(), "\u{1b}[24;80;6;1;1\"w");
    }

    #[test]
    fn microsoft_standard_decrqm_matrix_tracks_owned_and_report_only_state() {
        for mode in [4, 20] {
            let mut state = state();
            state.dispatch(OutputAction::SetMode {
                private: false,
                mode,
                enabled: true,
            });
            state.dispatch(OutputAction::RequestMode {
                private: false,
                mode,
            });
            assert_eq!(state.response(), format!("\u{1b}[{mode};1$y"));

            state.clear_response();
            state.dispatch(OutputAction::SetMode {
                private: false,
                mode,
                enabled: false,
            });
            state.dispatch(OutputAction::RequestMode {
                private: false,
                mode,
            });
            assert_eq!(state.response(), format!("\u{1b}[{mode};2$y"));

            if mode == 4 {
                assert!(state.presentation().core().deferred_actions().is_empty());
            } else {
                assert_eq!(state.presentation().core().deferred_actions().len(), 2);
            }
        }
    }

    #[test]
    fn microsoft_private_decrqm_matrix_reports_all_source_modes_without_swallowing_effects() {
        const MODES: [i32; 23] = [
            1, 3, 5, 6, 7, 8, 12, 25, 40, 66, 67, 69, 117, 1000, 1002, 1003, 1004, 1005, 1006,
            1007, 1049, 2004, 9001,
        ];

        for mode in MODES {
            let mut state = state();
            if mode == 3 {
                state.dispatch(OutputAction::SetMode {
                    private: true,
                    mode: 40,
                    enabled: true,
                });
            }

            state.dispatch(OutputAction::SetMode {
                private: true,
                mode,
                enabled: true,
            });
            state.dispatch(OutputAction::RequestMode {
                private: true,
                mode,
            });
            assert_eq!(state.response(), format!("\u{1b}[?{mode};1$y"));

            state.clear_response();
            state.dispatch(OutputAction::SetMode {
                private: true,
                mode,
                enabled: false,
            });
            state.dispatch(OutputAction::RequestMode {
                private: true,
                mode,
            });
            assert_eq!(state.response(), format!("\u{1b}[?{mode};2$y"));

            if matches!(mode, 6 | 7 | 25 | 69 | 117) {
                assert!(state.presentation().core().deferred_actions().is_empty());
            } else {
                assert!(!state.presentation().core().deferred_actions().is_empty());
            }
        }
    }

    #[test]
    fn unsupported_decrqm_modes_remain_deferred() {
        let mut state = state();
        state.dispatch(OutputAction::RequestMode {
            private: true,
            mode: 9999,
        });
        assert!(state.response().is_empty());
        assert_eq!(state.presentation().core().deferred_actions().len(), 1);
    }

    #[test]
    fn microsoft_permanent_mode_2027_stays_enabled_after_reset() {
        let mut state = state();
        state.dispatch(OutputAction::SetMode {
            private: true,
            mode: PERMANENT_GRAPHEME_CLUSTER_MODE,
            enabled: false,
        });
        assert!(state.presentation().core().deferred_actions().is_empty());

        state.dispatch(OutputAction::RequestMode {
            private: true,
            mode: PERMANENT_GRAPHEME_CLUSTER_MODE,
        });
        assert_eq!(state.response(), "\u{1b}[?2027;3$y");
    }

    #[test]
    fn microsoft_request_settings_dcs_flows_parser_to_live_adapter_state() {
        let mut dispatch = AdaptDispatchResponseState::new(PageGeometry::new(0, 100, 25));
        dispatch.set_color_alias_indices(ColorAliasIndices {
            default_foreground: 3,
            default_background: 5,
            frame_foreground: 4,
            frame_background: 6,
        });
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        machine.process_str("\u{1b}[5;10r\u{1b}P$q r");
        machine.process_str("\u{1b}\\");
        assert_eq!(machine.engine().dispatch().response(), "\u{1b}P0$r\u{1b}\\");
        machine.engine_mut().dispatch_mut().clear_response();

        machine.process_str("\u{1b}P$q r\u{1b}\\");
        assert_eq!(machine.engine().dispatch().response(), "\u{1b}P0$r\u{1b}\\");
        machine.engine_mut().dispatch_mut().clear_response();

        machine.process_str("\u{1b}P$qr\u{1b}\\");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P1$r5;10r\u{1b}\\"
        );
        machine.engine_mut().dispatch_mut().clear_response();

        machine.process_str("\u{1b}[1;4;7m\u{1b}P$qm\u{1b}\\");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P1$r0;1;4;7m\u{1b}\\"
        );
        machine.engine_mut().dispatch_mut().clear_response();

        machine.process_str("\u{1b}[4 q\u{1b}P$q q\u{1b}\\");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P1$r4 q\u{1b}\\"
        );
        machine.engine_mut().dispatch_mut().clear_response();

        machine.process_str("\u{1b}[1\"q\u{1b}P$q\"q\u{1b}\\");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P1$r1\"q\u{1b}\\"
        );
        machine.engine_mut().dispatch_mut().clear_response();

        machine.process_str("\u{1b}P$q,|\u{1b}\\");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P1$r1;3;5,|\u{1b}\\"
        );
        machine.engine_mut().dispatch_mut().clear_response();

        machine.process_str("\u{1b}P$q2,|\u{1b}\\");
        assert_eq!(
            machine.engine().dispatch().response(),
            "\u{1b}P1$r2;4;6,|\u{1b}\\"
        );
    }

    #[test]
    fn decrqss_sink_failure_terminates_the_dcs_without_output() {
        let mut dispatch = AdaptDispatchResponseState::new(PageGeometry::new(0, 100, 25));
        dispatch.set_response_writable(false);
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));
        machine.process_str("\u{1b}P$qm\u{1b}\\");
        assert!(machine.engine().dispatch().response().is_empty());
    }

    #[test]
    fn response_sink_failure_is_propagated_as_deferred_adapter_work() {
        let mut state = state();
        state.set_response_writable(false);
        state.dispatch(OutputAction::DeviceAttributes(
            DeviceAttributesKind::Primary,
        ));
        state.dispatch(OutputAction::DeviceAttributes(
            DeviceAttributesKind::Secondary,
        ));
        state.dispatch(OutputAction::DeviceAttributes(
            DeviceAttributesKind::Tertiary,
        ));
        state.dispatch(OutputAction::RequestTerminalParameters(0));
        state.dispatch(OutputAction::RequestDisplayedExtent);
        state.dispatch(OutputAction::RequestMode {
            private: false,
            mode: 4,
        });
        state.dispatch(OutputAction::RequestMode {
            private: true,
            mode: PERMANENT_GRAPHEME_CLUSTER_MODE,
        });
        assert!(state.response().is_empty());
        assert_eq!(state.presentation().core().deferred_actions().len(), 7);
    }

    #[test]
    fn unsupported_reports_vt52_attributes_and_parameters_remain_deferred() {
        let mut state = state();
        state.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 9999,
            id: None,
        });
        state.dispatch(OutputAction::DeviceAttributes(DeviceAttributesKind::Vt52));
        state.dispatch(OutputAction::RequestTerminalParameters(2));
        assert!(state.response().is_empty());
        assert_eq!(state.presentation().core().deferred_actions().len(), 3);
    }
}
