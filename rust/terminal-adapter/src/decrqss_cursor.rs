//! Portable DECRQSS serialization for cursor style and character protection.
//!
//! These are deterministic adapter settings: DECSCUSR reports the current
//! cursor shape/blink pair and DECSCA reports whether new text is protected
//! from selective erase. Native cursor drawing remains outside this owner.

const DCS_VALID_PREFIX: &str = "\u{1b}P1$r";
const ST: &str = "\u{1b}\\";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CursorShape {
    Legacy,
    Block,
    Underline,
    Bar,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CursorStyleState {
    pub shape: CursorShape,
    pub blinking: bool,
}

impl Default for CursorStyleState {
    fn default() -> Self {
        Self {
            shape: CursorShape::Legacy,
            blinking: true,
        }
    }
}

#[must_use]
pub fn serialize_cursor_style(state: CursorStyleState) -> String {
    let parameter = match (state.shape, state.blinking) {
        (CursorShape::Block, true) => 1,
        (CursorShape::Block, false) => 2,
        (CursorShape::Underline, true) => 3,
        (CursorShape::Underline, false) => 4,
        (CursorShape::Bar, true) => 5,
        (CursorShape::Bar, false) => 6,
        (CursorShape::Legacy, _) => 0,
    };
    format!("{DCS_VALID_PREFIX}{parameter} q{ST}")
}

#[must_use]
pub fn serialize_character_protection(protected: bool) -> String {
    let parameter = u8::from(protected);
    format!("{DCS_VALID_PREFIX}{parameter}\"q{ST}")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_request_settings_reports_all_decscusr_styles() {
        let cases = [
            (CursorShape::Block, true, "\u{1b}P1$r1 q\u{1b}\\"),
            (CursorShape::Block, false, "\u{1b}P1$r2 q\u{1b}\\"),
            (CursorShape::Underline, true, "\u{1b}P1$r3 q\u{1b}\\"),
            (CursorShape::Underline, false, "\u{1b}P1$r4 q\u{1b}\\"),
            (CursorShape::Bar, true, "\u{1b}P1$r5 q\u{1b}\\"),
            (CursorShape::Bar, false, "\u{1b}P1$r6 q\u{1b}\\"),
            (CursorShape::Legacy, true, "\u{1b}P1$r0 q\u{1b}\\"),
        ];

        for (shape, blinking, expected) in cases {
            assert_eq!(
                serialize_cursor_style(CursorStyleState { shape, blinking }),
                expected
            );
        }
    }

    #[test]
    fn microsoft_request_settings_reports_decsca_protection() {
        assert_eq!(
            serialize_character_protection(false),
            "\u{1b}P1$r0\"q\u{1b}\\"
        );
        assert_eq!(
            serialize_character_protection(true),
            "\u{1b}P1$r1\"q\u{1b}\\"
        );
    }
}
