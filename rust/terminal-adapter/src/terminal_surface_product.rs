//! Final portable owner for adapter terminal-surface behavior.
//!
//! `ColorProductDispatch` already owns the live color/report path. This outer
//! product owner adds the remaining deterministic surface state that Microsoft
//! observes without importing a native window or renderer: 7/8-bit C1 response
//! framing, window title state, ATT610 cursor blinking, and VS Code completion
//! requests.

use terminal_parser::output_engine::{DcsAction, OutputAction, TermDispatch};

use crate::{adapt_dispatch::PageGeometry, color_product_dispatch::ColorProductDispatch};

const ESC: char = '\u{1b}';
const ATT610_START_CURSOR_BLINK: i32 = 12;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CompletionRequest {
    pub menu_json: String,
    pub replacement_length: u32,
}

pub struct TerminalSurfaceProductDispatch {
    inner: ColorProductDispatch,
    send_c1_controls: bool,
    window_title: String,
    cursor_blinking: bool,
    completion_request: Option<CompletionRequest>,
    outbound: String,
}

impl TerminalSurfaceProductDispatch {
    #[must_use]
    pub fn new(geometry: PageGeometry) -> Self {
        Self {
            inner: ColorProductDispatch::new(geometry),
            send_c1_controls: false,
            window_title: String::new(),
            cursor_blinking: true,
            completion_request: None,
            outbound: String::new(),
        }
    }

    #[must_use]
    pub const fn inner(&self) -> &ColorProductDispatch {
        &self.inner
    }

    pub const fn inner_mut(&mut self) -> &mut ColorProductDispatch {
        &mut self.inner
    }

    #[must_use]
    pub const fn send_c1_controls(&self) -> bool {
        self.send_c1_controls
    }

    #[must_use]
    pub fn window_title(&self) -> &str {
        &self.window_title
    }

    #[must_use]
    pub const fn cursor_blinking(&self) -> bool {
        self.cursor_blinking
    }

    #[must_use]
    pub const fn completion_request(&self) -> Option<&CompletionRequest> {
        self.completion_request.as_ref()
    }

    #[must_use]
    pub fn response(&self) -> &str {
        &self.outbound
    }

    pub fn clear_response(&mut self) {
        self.outbound.clear();
        self.inner.clear_response();
    }

    fn collect_inner_response(&mut self) {
        if self.inner.response().is_empty() {
            return;
        }
        let response = self.inner.response().to_owned();
        self.inner.clear_response();
        self.push_response(&response);
    }

    fn push_response(&mut self, response: &str) {
        if self.send_c1_controls {
            self.outbound.push_str(&to_eight_bit_c1(response));
        } else {
            self.outbound.push_str(response);
        }
    }

    fn request_blink_mode(&mut self) {
        let state = if self.cursor_blinking { 1 } else { 2 };
        self.push_response(&format!("\u{1b}[?{ATT610_START_CURSOR_BLINK};{state}$y"));
    }

    fn record_vscode_action(&mut self, payload: &str) {
        let Some(request) = parse_completion_request(payload) else {
            return;
        };
        self.completion_request = Some(request);
    }
}

impl TermDispatch for TerminalSurfaceProductDispatch {
    fn dispatch(&mut self, action: OutputAction) {
        match action {
            OutputAction::SendC1Controls(enabled) => {
                self.send_c1_controls = enabled;
                self.inner.dispatch(OutputAction::SendC1Controls(enabled));
                self.collect_inner_response();
            }
            OutputAction::SetWindowTitle(title) => {
                self.window_title = title;
            }
            OutputAction::SetMode {
                private: true,
                enabled,
                mode: ATT610_START_CURSOR_BLINK,
            } => {
                self.cursor_blinking = enabled;
            }
            OutputAction::RequestMode {
                private: true,
                mode: ATT610_START_CURSOR_BLINK,
            } => self.request_blink_mode(),
            OutputAction::VsCodeAction(payload) => self.record_vscode_action(&payload),
            other => {
                self.inner.dispatch(other);
                self.collect_inner_response();
            }
        }
    }

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        let handled = self.inner.begin_dcs(action);
        self.collect_inner_response();
        handled
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        let handled = self.inner.dcs_put(code_unit);
        self.collect_inner_response();
        handled
    }
}

fn parse_completion_request(payload: &str) -> Option<CompletionRequest> {
    let mut parts = payload.splitn(5, ';');
    if parts.next()? != "Completions" {
        return None;
    }

    let _replacement_index = parts.next()?.parse::<u32>().ok()?;
    let replacement_length = parts.next()?.parse::<u32>().ok()?;
    let _cursor_index = parts.next()?.parse::<u32>().ok()?;
    let menu_json = parts.next()?.to_owned();

    Some(CompletionRequest {
        menu_json,
        replacement_length,
    })
}

fn to_eight_bit_c1(response: &str) -> String {
    let mut result = String::with_capacity(response.len());
    let mut chars = response.chars().peekable();

    while let Some(character) = chars.next() {
        if character == ESC
            && let Some(&next) = chars.peek()
            && ('@'..='_').contains(&next)
        {
            let next = chars.next().expect("peeked character exists");
            if let Some(c1) = char::from_u32(u32::from(next) + 0x40) {
                result.push(c1);
                continue;
            }
        }
        result.push(character);
    }

    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use terminal_parser::{
        output_engine::{DeviceAttributesKind, OutputStateMachineEngine},
        state_machine::StateMachine,
    };

    fn product() -> TerminalSurfaceProductDispatch {
        TerminalSurfaceProductDispatch::new(PageGeometry::new(20, 100, 29))
    }

    #[test]
    fn microsoft_send_c1_control_test_matches_8bit_and_7bit_report_framing() {
        let mut state = product();

        state.dispatch(OutputAction::SendC1Controls(true));
        assert!(state.send_c1_controls());

        state.dispatch(OutputAction::DeviceAttributes(
            DeviceAttributesKind::Secondary,
        ));
        assert_eq!(state.response(), "\u{009b}>0;10;1c");
        state.clear_response();

        state.dispatch(OutputAction::DeviceAttributes(
            DeviceAttributesKind::Tertiary,
        ));
        assert_eq!(state.response(), "\u{0090}!|00000000\u{009c}");
        state.clear_response();

        state.dispatch(OutputAction::RequestColorTableEntry(0));
        assert_eq!(state.response(), "\u{009d}4;0;rgb:0c0c/0c0c/0c0c\u{009c}");
        state.clear_response();

        state.dispatch(OutputAction::SendC1Controls(false));
        assert!(!state.send_c1_controls());

        state.dispatch(OutputAction::DeviceAttributes(
            DeviceAttributesKind::Secondary,
        ));
        assert_eq!(state.response(), "\u{1b}[>0;10;1c");
        state.clear_response();

        state.dispatch(OutputAction::DeviceAttributes(
            DeviceAttributesKind::Tertiary,
        ));
        assert_eq!(state.response(), "\u{1b}P!|00000000\u{1b}\\");
        state.clear_response();

        state.dispatch(OutputAction::RequestColorTableEntry(0));
        assert_eq!(state.response(), "\u{1b}]4;0;rgb:0c0c/0c0c/0c0c\u{1b}\\");
    }

    #[test]
    fn microsoft_allow_blinking_test_owns_att610_set_reset_and_report() {
        let mut state = product();

        state.cursor_blinking = false;
        state.dispatch(OutputAction::SetMode {
            private: true,
            enabled: true,
            mode: ATT610_START_CURSOR_BLINK,
        });
        assert!(state.cursor_blinking());

        state.cursor_blinking = true;
        state.dispatch(OutputAction::SetMode {
            private: true,
            enabled: false,
            mode: ATT610_START_CURSOR_BLINK,
        });
        assert!(!state.cursor_blinking());

        state.dispatch(OutputAction::RequestMode {
            private: true,
            mode: ATT610_START_CURSOR_BLINK,
        });
        assert_eq!(state.response(), "\u{1b}[?12;2$y");
    }

    #[test]
    fn microsoft_set_console_title_test_owns_nonempty_and_empty_titles() {
        let mut state = product();
        state.dispatch(OutputAction::SetWindowTitle("Foo bar".to_owned()));
        assert_eq!(state.window_title(), "Foo bar");

        state.dispatch(OutputAction::SetWindowTitle(String::new()));
        assert_eq!(state.window_title(), "");
    }

    #[test]
    fn microsoft_menu_completions_tests_match_validation_and_payload_preservation() {
        let mut state = product();
        for invalid in [
            "garbage",
            "Completions",
            "Completions;",
            "Completions;10;",
            "Completions;10;20",
            "Completions;10;20;",
            "Completions;10;20;3",
        ] {
            state.dispatch(OutputAction::VsCodeAction(invalid.to_owned()));
            assert!(state.completion_request().is_none(), "payload={invalid:?}");
        }

        state.dispatch(OutputAction::VsCodeAction(
            "Completions;1;2;3;{ \"foo\": 1, \"bar\": 2 }".to_owned(),
        ));
        assert_eq!(
            state.completion_request(),
            Some(&CompletionRequest {
                menu_json: "{ \"foo\": 1, \"bar\": 2 }".to_owned(),
                replacement_length: 2,
            })
        );

        state.dispatch(OutputAction::VsCodeAction(
            "Completions;10;20;30;{ \"foo\": \"what;ever\", \"bar\": 2 }".to_owned(),
        ));
        assert_eq!(
            state.completion_request(),
            Some(&CompletionRequest {
                menu_json: "{ \"foo\": \"what;ever\", \"bar\": 2 }".to_owned(),
                replacement_length: 20,
            })
        );
    }

    #[test]
    fn parser_routes_surface_actions_into_the_same_final_product_owner() {
        let dispatch = product();
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        machine.process_str("\u{1b} G\u{1b}]2;Parser title\u{7}\u{1b}[?12l");
        let state = machine.engine().dispatch();
        assert!(state.send_c1_controls());
        assert_eq!(state.window_title(), "Parser title");
        assert!(!state.cursor_blinking());
    }
}
