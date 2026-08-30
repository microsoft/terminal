//! Portable DECAUPSS / DECRQUPSS state and response owner.
//!
//! Windows Terminal stores the user-preference supplemental character set in
//! terminal output state. The protocol is deterministic and platform-neutral,
//! so Rust owns both the streamed DCS assignment and the exact DCS query
//! response while the product aggregate keeps sink failure behavior explicit.

use terminal_parser::{
    output_engine::{DcsAction, OutputAction, TermDispatch},
    state_machine::{Parameters, VtId},
};

use crate::vt_response::VtResponseEngine;

const ESC: u16 = 0x1b;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum CharsetSize {
    #[default]
    Size94,
    Size96,
}

impl CharsetSize {
    #[must_use]
    const fn from_parameter(value: i32) -> Option<Self> {
        match value {
            0 => Some(Self::Size94),
            1 => Some(Self::Size96),
            _ => None,
        }
    }

    #[must_use]
    const fn report_parameter(self) -> u8 {
        match self {
            Self::Size94 => 0,
            Self::Size96 => 1,
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UserPreferenceCharsetState {
    size: CharsetSize,
    id: String,
    pending_size: CharsetSize,
    pending_id: String,
    assigning: bool,
}

impl Default for UserPreferenceCharsetState {
    fn default() -> Self {
        Self {
            size: CharsetSize::Size94,
            id: String::new(),
            pending_size: CharsetSize::Size94,
            pending_id: String::new(),
            assigning: false,
        }
    }
}

impl UserPreferenceCharsetState {
    #[must_use]
    pub const fn size(&self) -> CharsetSize {
        self.size
    }

    #[must_use]
    pub fn id(&self) -> &str {
        &self.id
    }

    fn begin_assignment(&mut self, parameters: &Parameters) -> bool {
        let Some(size) = CharsetSize::from_parameter(parameters.at(0).unwrap_or(0)) else {
            return false;
        };

        self.pending_size = size;
        self.pending_id.clear();
        self.assigning = true;
        true
    }

    fn put(&mut self, code_unit: u16) -> bool {
        if !self.assigning {
            return false;
        }

        if code_unit == ESC {
            self.assigning = false;
            if valid_vt_charset_id(&self.pending_id) {
                self.size = self.pending_size;
                self.id.clone_from(&self.pending_id);
            }
            return false;
        }

        let Ok(byte) = u8::try_from(code_unit) else {
            self.assigning = false;
            self.pending_id.clear();
            return false;
        };
        if !(0x20..=0x7e).contains(&byte) || self.pending_id.len() >= 7 {
            self.assigning = false;
            self.pending_id.clear();
            return false;
        }

        self.pending_id.push(char::from(byte));
        true
    }

    #[must_use]
    fn response(&self) -> String {
        format!(
            "\u{1b}P{}!u{}\u{1b}\\",
            self.size.report_parameter(),
            self.id
        )
    }
}

fn valid_vt_charset_id(id: &str) -> bool {
    let bytes = id.as_bytes();
    let Some((&final_byte, intermediates)) = bytes.split_last() else {
        return false;
    };
    (0x30..=0x7e).contains(&final_byte)
        && intermediates
            .iter()
            .all(|byte| (0x20..=0x2f).contains(byte))
}

#[derive(Debug, Clone, Default)]
pub struct UserPreferenceCharsetEngine {
    state: UserPreferenceCharsetState,
    responses: VtResponseEngine,
}

impl UserPreferenceCharsetEngine {
    #[must_use]
    pub const fn state(&self) -> &UserPreferenceCharsetState {
        &self.state
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

    fn request(&mut self) -> bool {
        self.responses.return_response(&self.state.response())
    }
}

impl TermDispatch for UserPreferenceCharsetEngine {
    fn dispatch(&mut self, action: OutputAction) {
        if let OutputAction::AdvancedCsi { id, .. } = action
            && id == VtId::from_ascii("&u")
        {
            let _ = self.request();
        }
    }

    fn begin_dcs(&mut self, action: DcsAction) -> bool {
        match action {
            DcsAction::AssignUserPreferenceCharset(parameters) => {
                self.state.begin_assignment(&parameters)
            }
            _ => false,
        }
    }

    fn dcs_put(&mut self, code_unit: u16) -> bool {
        self.state.put(code_unit)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn assign(engine: &mut UserPreferenceCharsetEngine, size: i32, id: &str) {
        assert!(engine.begin_dcs(DcsAction::AssignUserPreferenceCharset(
            Parameters::from_values(vec![Some(size)])
        )));
        for code_unit in id.encode_utf16() {
            assert!(engine.dcs_put(code_unit));
        }
        assert!(!engine.dcs_put(ESC));
    }

    #[test]
    fn microsoft_decaupss_assigns_all_user_preference_charset_vectors() {
        let mut engine = UserPreferenceCharsetEngine::default();
        let cases = [
            (0, "%5"),
            (0, "\"?"),
            (0, "\"4"),
            (0, "%0"),
            (0, "&4"),
            (1, "A"),
            (1, "B"),
            (1, "F"),
            (1, "H"),
            (1, "L"),
            (1, "M"),
        ];

        for (size, id) in cases {
            assign(&mut engine, size, id);
            assert_eq!(engine.state().id(), id);
            assert_eq!(
                engine.state().size(),
                if size == 0 {
                    CharsetSize::Size94
                } else {
                    CharsetSize::Size96
                }
            );
        }
    }

    #[test]
    fn microsoft_decrqupss_reports_all_assigned_charset_vectors_exactly() {
        let mut engine = UserPreferenceCharsetEngine::default();
        let cases = [
            (0, "%5"),
            (0, "\"?"),
            (0, "\"4"),
            (0, "%0"),
            (0, "&4"),
            (1, "A"),
            (1, "B"),
            (1, "F"),
            (1, "H"),
            (1, "L"),
            (1, "M"),
        ];

        for (size, id) in cases {
            assign(&mut engine, size, id);
            engine.dispatch(OutputAction::AdvancedCsi {
                id: VtId::from_ascii("&u"),
                parameters: Parameters::default(),
            });
            assert_eq!(engine.response(), format!("\u{1b}P{size}!u{id}\u{1b}\\"));
            engine.clear_response();
        }
    }

    #[test]
    fn user_preference_charset_sink_failure_writes_nothing() {
        let mut engine = UserPreferenceCharsetEngine::default();
        assign(&mut engine, 1, "A");
        engine.set_response_writable(false);
        engine.dispatch(OutputAction::AdvancedCsi {
            id: VtId::from_ascii("&u"),
            parameters: Parameters::default(),
        });
        assert!(engine.response().is_empty());
    }

    #[test]
    fn invalid_assignment_does_not_replace_the_last_valid_charset() {
        let mut engine = UserPreferenceCharsetEngine::default();
        assign(&mut engine, 1, "A");
        assert!(!engine.begin_dcs(DcsAction::AssignUserPreferenceCharset(
            Parameters::from_values(vec![Some(2)])
        )));
        assert_eq!(engine.state().size(), CharsetSize::Size96);
        assert_eq!(engine.state().id(), "A");
    }
}
