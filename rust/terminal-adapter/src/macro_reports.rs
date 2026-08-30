//! DEC macro status-report state shared by the adapter response path.
//!
//! [`MacroBuffer`] remains the sole owner of DECDMAC storage/checksum semantics.
//! This module composes that buffer with the exact VT framing required by DSR
//! 62 (macro space) and DSR 63 (macro memory checksum). The same state accepts
//! DECDMAC DCS definitions, so reports observe the live macro memory instead of
//! a disconnected reporting copy.

use terminal_parser::{
    output_engine::{DcsAction, OutputAction, TermDispatch},
    state_machine::Parameters,
};

use crate::{
    macro_buffer::{MacroBuffer, MacroDeleteControl, MacroEncoding},
    vt_response::VtResponseEngine,
};

const ESC: u16 = 0x1b;

/// DEC reports macro space in blocks of 16 bytes via `CSI Ps * {`.
#[must_use]
pub fn macro_space_report(buffer: &MacroBuffer) -> String {
    let available_blocks = buffer.space_available() / 16;
    format!("\u{1b}[{available_blocks}*{{")
}

/// DEC reports the macro-memory checksum as `DCS id ! ~ hhhh ST`.
#[must_use]
pub fn macro_checksum_report(buffer: &MacroBuffer, request_id: i32) -> String {
    let request_id = request_id.max(0);
    format!(
        "\u{1b}P{request_id}!~{:04X}\u{1b}\\",
        buffer.calculate_checksum()
    )
}

/// Focused product state for DECDMAC plus its two DSR responses.
///
/// This is intentionally narrower than the full adapter aggregate. It exists so
/// the response dispatcher can embed one live macro owner instead of maintaining
/// a reporting-only copy of macro memory.
#[derive(Debug, Clone, Default)]
pub struct MacroReportEngine {
    buffer: MacroBuffer,
    responses: VtResponseEngine,
    active_macro: bool,
}

impl MacroReportEngine {
    #[must_use]
    pub const fn buffer(&self) -> &MacroBuffer {
        &self.buffer
    }

    pub const fn buffer_mut(&mut self) -> &mut MacroBuffer {
        &mut self.buffer
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
    pub const fn active_macro(&self) -> bool {
        self.active_macro
    }

    fn begin_macro(&mut self, parameters: &Parameters) -> bool {
        let Ok(macro_id) = usize::try_from(parameters.at(0).unwrap_or(0)) else {
            return false;
        };
        let delete_control = match parameters.at(1).unwrap_or(0) {
            0 => MacroDeleteControl::DeleteId,
            1 => MacroDeleteControl::DeleteAll,
            _ => return false,
        };
        let encoding = match parameters.at(2).unwrap_or(0) {
            0 => MacroEncoding::Text,
            1 => MacroEncoding::HexPair,
            _ => return false,
        };

        if !self.buffer.init_parser(macro_id, delete_control, encoding) {
            return false;
        }
        self.active_macro = true;
        true
    }

    fn report(&mut self, status: i32, id: Option<i32>) -> bool {
        let response = match status {
            62 => macro_space_report(&self.buffer),
            63 => macro_checksum_report(&self.buffer, id.unwrap_or(0)),
            _ => return false,
        };
        self.responses.return_response(&response)
    }
}

impl TermDispatch for MacroReportEngine {
    fn dispatch(&mut self, action: OutputAction) {
        if let OutputAction::DeviceStatusReport {
            private: true,
            status,
            id,
        } = action
        {
            let _ = self.report(status, id);
        }
    }

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        self.active_macro = false;
        match action {
            DcsAction::DefineMacro(parameters) => self.begin_macro(&parameters),
            _ => false,
        }
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        if !self.active_macro {
            return false;
        }

        let keep_parsing = self.buffer.parse_definition(code_unit);
        if code_unit == ESC || !keep_parsing {
            self.active_macro = false;
        }
        keep_parsing
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::macro_buffer::MAX_SPACE;
    use terminal_parser::{output_engine::OutputStateMachineEngine, state_machine::StateMachine};

    fn define_text_macro(buffer: &mut MacroBuffer, id: usize, text: &str) {
        assert!(buffer.init_parser(id, MacroDeleteControl::DeleteId, MacroEncoding::Text));
        for unit in text.encode_utf16() {
            assert!(buffer.parse_definition(unit));
        }
        assert!(!buffer.parse_definition(ESC));
    }

    #[test]
    fn microsoft_macro_space_report_uses_sixteen_byte_blocks() {
        let mut buffer = MacroBuffer::default();
        assert_eq!(
            macro_space_report(&buffer),
            format!("\u{1b}[{}*{{", MAX_SPACE / 16)
        );

        // Microsoft defines four eight-byte macros: 32 bytes total, therefore
        // two 16-byte report blocks are consumed.
        for id in 1..=4 {
            define_text_macro(&mut buffer, id, "12345678");
        }
        assert_eq!(
            macro_space_report(&buffer),
            format!("\u{1b}[{}*{{", (MAX_SPACE / 16) - 2)
        );
    }

    #[test]
    fn microsoft_macro_memory_checksum_report_matches_dsr_63_framing() {
        let mut buffer = MacroBuffer::default();
        assert_eq!(
            macro_checksum_report(&buffer, 12),
            "\u{1b}P12!~0000\u{1b}\\"
        );

        define_text_macro(&mut buffer, 1, "ABC");
        let expected = format!("\u{1b}P12!~{:04X}\u{1b}\\", buffer.calculate_checksum());
        assert_eq!(macro_checksum_report(&buffer, 12), expected);
    }

    #[test]
    fn microsoft_macro_reports_observe_macros_defined_through_the_live_dcs_parser() {
        let dispatch = MacroReportEngine::default();
        let mut machine = StateMachine::new(OutputStateMachineEngine::new(dispatch));

        for id in 1..=4 {
            machine.process_str(&format!("\u{1b}P{id};0;0!z12345678\u{1b}\\"));
        }

        let engine = machine.engine_mut().dispatch_mut();
        assert!(!engine.active_macro());
        assert_eq!(engine.buffer().space_available(), MAX_SPACE - 32);

        engine.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 62,
            id: None,
        });
        assert_eq!(
            engine.response(),
            format!("\u{1b}[{}*{{", (MAX_SPACE / 16) - 2)
        );

        engine.clear_response();
        engine.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 63,
            id: Some(12),
        });
        assert_eq!(
            engine.response(),
            format!(
                "\u{1b}P12!~{:04X}\u{1b}\\",
                engine.buffer().calculate_checksum()
            )
        );
    }

    #[test]
    fn macro_report_sink_failure_writes_nothing() {
        let mut engine = MacroReportEngine::default();
        engine.set_response_writable(false);
        engine.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 62,
            id: None,
        });
        engine.dispatch(OutputAction::DeviceStatusReport {
            private: true,
            status: 63,
            id: Some(7),
        });
        assert!(engine.response().is_empty());
    }

    #[test]
    fn macro_checksum_report_clamps_negative_request_ids() {
        let buffer = MacroBuffer::default();
        assert_eq!(macro_checksum_report(&buffer, -1), "\u{1b}P0!~0000\u{1b}\\");
    }
}
