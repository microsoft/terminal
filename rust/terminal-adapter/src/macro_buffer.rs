//! Safe, platform-neutral core for DEC DECDMAC macro storage and parsing.
//!
//! Windows Terminal's C++ `MacroBuffer` stores 64 UTF-16 macro sequences,
//! supports text and hexadecimal definitions, bounds total macro storage, and
//! limits recursive invocation. This module preserves those semantics without
//! exposing mutable parser state across the adapter boundary.

use terminal_parser::state_machine::MAX_PARAMETER_VALUE;

pub const MAX_SPACE: usize = 0x4_0000;
pub const MACRO_COUNT: usize = 64;
pub const MAX_INVOCATION_DEPTH: usize = 16;
const ESC: u16 = 0x1b;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MacroDeleteControl {
    DeleteId,
    DeleteAll,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MacroEncoding {
    Text,
    HexPair,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct InvocationContext {
    depth: usize,
    sequence_length: usize,
}

impl InvocationContext {
    #[must_use]
    pub const fn depth(self) -> usize {
        self.depth
    }

    #[must_use]
    pub const fn sequence_length(self) -> usize {
        self.sequence_length
    }

    #[must_use]
    pub const fn is_active(self) -> bool {
        self.depth != 0
    }
}

#[derive(Debug, Clone, Copy)]
pub struct PreparedMacro<'a> {
    sequence: &'a [u16],
    context: InvocationContext,
}

impl<'a> PreparedMacro<'a> {
    #[must_use]
    pub const fn sequence(self) -> &'a [u16] {
        self.sequence
    }

    #[must_use]
    pub const fn context(self) -> InvocationContext {
        self.context
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
enum ParseState {
    #[default]
    Text,
    HexDigit,
    SecondHexDigit,
    RepeatCount,
}

#[derive(Debug, Clone)]
pub struct MacroBuffer {
    parse_state: ParseState,
    decoded_char: u16,
    repeat_pending: bool,
    repeat_count: usize,
    repeat_start: usize,
    macros: [Vec<u16>; MACRO_COUNT],
    active_macro_id: usize,
    space_used: usize,
}

impl Default for MacroBuffer {
    fn default() -> Self {
        Self {
            parse_state: ParseState::default(),
            decoded_char: 0,
            repeat_pending: false,
            repeat_count: 0,
            repeat_start: 0,
            macros: std::array::from_fn(|_| Vec::new()),
            active_macro_id: 0,
            space_used: 0,
        }
    }
}

impl MacroBuffer {
    #[must_use]
    pub const fn space_available(&self) -> usize {
        MAX_SPACE - self.space_used
    }

    #[must_use]
    pub fn macro_contents(&self, macro_id: usize) -> Option<&[u16]> {
        self.macros.get(macro_id).map(Vec::as_slice)
    }

    #[must_use]
    pub fn calculate_checksum(&self) -> u16 {
        self.macros
            .iter()
            .flatten()
            .fold(0u16, |checksum, &ch| checksum.wrapping_sub(ch))
    }

    /// Prepares one macro invocation without coupling this crate to the VT state
    /// machine. The returned context is passed to nested invocations.
    ///
    /// The strict length comparison intentionally matches the C++ implementation:
    /// the cumulative invoked sequence length must remain below `MAX_SPACE`.
    #[must_use]
    pub fn prepare_invoke(
        &self,
        macro_id: usize,
        context: InvocationContext,
    ) -> Option<PreparedMacro<'_>> {
        let sequence = self.macros.get(macro_id)?;
        let allowed_length = MAX_SPACE.saturating_sub(context.sequence_length);
        if context.depth >= MAX_INVOCATION_DEPTH || sequence.len() >= allowed_length {
            return None;
        }

        Some(PreparedMacro {
            sequence,
            context: InvocationContext {
                depth: context.depth + 1,
                sequence_length: context.sequence_length + sequence.len(),
            },
        })
    }

    /// Mirrors `ClearMacrosIfInUse`: during an active invocation the allocations
    /// and lengths must remain stable, so definitions are overwritten with NULs
    /// instead of being released.
    pub fn clear_macros_if_in_use(&mut self, context: InvocationContext) {
        if context.is_active() {
            for macro_sequence in &mut self.macros {
                macro_sequence.fill(0);
            }
        }
    }

    pub fn init_parser(
        &mut self,
        macro_id: usize,
        delete_control: MacroDeleteControl,
        encoding: MacroEncoding,
    ) -> bool {
        self.init_parser_with_context(
            macro_id,
            delete_control,
            encoding,
            InvocationContext::default(),
        )
    }

    pub fn init_parser_with_context(
        &mut self,
        macro_id: usize,
        delete_control: MacroDeleteControl,
        encoding: MacroEncoding,
        invocation: InvocationContext,
    ) -> bool {
        if macro_id >= MACRO_COUNT || invocation.is_active() {
            return false;
        }

        self.active_macro_id = macro_id;
        self.decoded_char = 0;
        self.repeat_pending = false;
        self.parse_state = match encoding {
            MacroEncoding::HexPair => ParseState::HexDigit,
            MacroEncoding::Text => ParseState::Text,
        };

        match delete_control {
            MacroDeleteControl::DeleteId => self.delete_macro(macro_id),
            MacroDeleteControl::DeleteAll => {
                for id in 0..MACRO_COUNT {
                    self.delete_macro(id);
                }
            }
        }
        true
    }

    /// Consumes one UTF-16 code unit from a DECDMAC definition.
    ///
    /// `false` means either the ESC terminator was reached or the definition is
    /// invalid. Invalid definitions are cleared exactly like the C++ parser.
    pub fn parse_definition(&mut self, ch: u16) -> bool {
        if ch == ESC {
            if self.repeat_pending && !self.apply_pending_repeat() {
                self.delete_macro(self.active_macro_id);
            }
            return false;
        }

        if ch < u16::from(b' ') {
            return true;
        }

        let success = match self.parse_state {
            ParseState::Text => self.append_to_active_macro(ch),
            ParseState::HexDigit => {
                if self.decode_hex_digit(ch) {
                    self.parse_state = ParseState::SecondHexDigit;
                    true
                } else if ch == u16::from(b'!') && !self.repeat_pending {
                    self.parse_state = ParseState::RepeatCount;
                    self.repeat_count = 0;
                    true
                } else if ch == u16::from(b';') && self.repeat_pending {
                    self.apply_pending_repeat()
                } else {
                    false
                }
            }
            ParseState::SecondHexDigit => {
                let success =
                    self.decode_hex_digit(ch) && self.append_to_active_macro(self.decoded_char);
                self.decoded_char = 0;
                self.parse_state = ParseState::HexDigit;
                success
            }
            ParseState::RepeatCount => {
                if let Some(digit) = decimal_digit(ch) {
                    let limit = usize::try_from(MAX_PARAMETER_VALUE).unwrap_or(usize::MAX);
                    self.repeat_count = self
                        .repeat_count
                        .saturating_mul(10)
                        .saturating_add(digit)
                        .min(limit);
                    true
                } else if ch == u16::from(b';') {
                    self.repeat_pending = true;
                    self.repeat_start = self.active_macro().len();
                    self.parse_state = ParseState::HexDigit;
                    true
                } else {
                    false
                }
            }
        };

        if !success {
            self.delete_macro(self.active_macro_id);
        }
        success
    }

    fn decode_hex_digit(&mut self, ch: u16) -> bool {
        self.decoded_char = self.decoded_char.wrapping_shl(4);
        let Some(value) = hex_digit(ch) else {
            return false;
        };
        self.decoded_char = self.decoded_char.wrapping_add(value);
        true
    }

    fn append_to_active_macro(&mut self, ch: u16) -> bool {
        if self.space_available() == 0 {
            return false;
        }
        self.macros[self.active_macro_id].push(ch);
        self.space_used += 1;
        true
    }

    fn active_macro(&self) -> &Vec<u16> {
        &self.macros[self.active_macro_id]
    }

    fn delete_macro(&mut self, macro_id: usize) {
        self.space_used -= self.macros[macro_id].len();
        self.macros[macro_id] = Vec::new();
    }

    fn apply_pending_repeat(&mut self) -> bool {
        if self.repeat_count > 1 {
            let sequence_length = self.active_macro().len() - self.repeat_start;
            let Some(space_required) = (self.repeat_count - 1).checked_mul(sequence_length) else {
                return false;
            };
            if space_required > self.space_available() {
                return false;
            }

            let repeated = self.active_macro()[self.repeat_start..].to_vec();
            for _ in 1..self.repeat_count {
                self.macros[self.active_macro_id].extend_from_slice(&repeated);
                self.space_used += sequence_length;
            }
        }
        self.repeat_pending = false;
        true
    }
}

fn decimal_digit(ch: u16) -> Option<usize> {
    if (u16::from(b'0')..=u16::from(b'9')).contains(&ch) {
        Some(usize::from(ch - u16::from(b'0')))
    } else {
        None
    }
}

fn hex_digit(ch: u16) -> Option<u16> {
    match ch {
        ch if (u16::from(b'0')..=u16::from(b'9')).contains(&ch) => Some(ch - u16::from(b'0')),
        ch if (u16::from(b'A')..=u16::from(b'F')).contains(&ch) => Some(ch - u16::from(b'A') + 10),
        ch if (u16::from(b'a')..=u16::from(b'f')).contains(&ch) => Some(ch - u16::from(b'a') + 10),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn units(text: &str) -> Vec<u16> {
        text.encode_utf16().collect()
    }

    fn feed(buffer: &mut MacroBuffer, text: &str) -> bool {
        text.encode_utf16().all(|ch| buffer.parse_definition(ch))
    }

    fn define_text(buffer: &mut MacroBuffer, id: usize, text: &str) {
        assert!(buffer.init_parser(id, MacroDeleteControl::DeleteId, MacroEncoding::Text));
        assert!(feed(buffer, text));
        assert!(!buffer.parse_definition(ESC));
    }

    fn define_hex(buffer: &mut MacroBuffer, id: usize, text: &str) {
        assert!(buffer.init_parser(id, MacroDeleteControl::DeleteId, MacroEncoding::HexPair));
        assert!(feed(buffer, text));
        assert!(!buffer.parse_definition(ESC));
    }

    #[test]
    fn text_encoding_matches_microsoft_definition_contract() {
        let mut buffer = MacroBuffer::default();
        define_text(&mut buffer, 1, "Text Encoding");
        assert_eq!(
            buffer.macro_contents(1),
            Some(units("Text Encoding").as_slice())
        );
    }

    #[test]
    fn uppercase_and_lowercase_hex_pairs_match_microsoft_examples() {
        let mut buffer = MacroBuffer::default();
        define_hex(&mut buffer, 2, "486578204A4B4C4D4E4F");
        define_hex(&mut buffer, 3, "486578206a6b6c6d6e6f");
        assert_eq!(
            buffer.macro_contents(2),
            Some(units("Hex JKLMNO").as_slice())
        );
        assert_eq!(
            buffer.macro_contents(3),
            Some(units("Hex jklmno").as_slice())
        );
    }

    #[test]
    fn delete_id_and_delete_all_preserve_cpp_replacement_rules() {
        let mut buffer = MacroBuffer::default();
        define_text(&mut buffer, 1, "Retained");
        define_text(&mut buffer, 2, "Replaced");
        define_text(&mut buffer, 2, "New");
        assert_eq!(buffer.macro_contents(1), Some(units("Retained").as_slice()));
        assert_eq!(buffer.macro_contents(2), Some(units("New").as_slice()));

        assert!(buffer.init_parser(2, MacroDeleteControl::DeleteAll, MacroEncoding::Text));
        assert!(feed(&mut buffer, "Only"));
        assert_eq!(buffer.macro_contents(1), Some([].as_slice()));
        assert_eq!(buffer.macro_contents(2), Some(units("Only").as_slice()));
    }

    #[test]
    fn repeat_sequences_match_microsoft_three_zero_and_default_cases() {
        let mut buffer = MacroBuffer::default();
        define_hex(&mut buffer, 5, "526570656174!3;206563686F;207468726565");
        define_hex(&mut buffer, 6, "526570656174!0;206563686F;207A65726F");
        define_hex(&mut buffer, 7, "526570656174!;206563686F;2064656661756C74");
        assert_eq!(
            buffer.macro_contents(5),
            Some(units("Repeat echo echo echo three").as_slice())
        );
        assert_eq!(
            buffer.macro_contents(6),
            Some(units("Repeat echo zero").as_slice())
        );
        assert_eq!(
            buffer.macro_contents(7),
            Some(units("Repeat echo default").as_slice())
        );
    }

    #[test]
    fn unterminated_repeat_is_applied_when_escape_terminates_definition() {
        let mut buffer = MacroBuffer::default();
        assert!(buffer.init_parser(8, MacroDeleteControl::DeleteId, MacroEncoding::HexPair));
        assert!(feed(&mut buffer, "556E7465726D696E61746564!3;206563686F"));
        assert!(!buffer.parse_definition(ESC));
        assert_eq!(
            buffer.macro_contents(8),
            Some(units("Unterminated echo echo echo").as_slice())
        );
    }

    #[test]
    fn unexpected_semicolon_cancels_the_definition() {
        let mut buffer = MacroBuffer::default();
        define_text(&mut buffer, 9, "Replaced");
        assert!(buffer.init_parser(9, MacroDeleteControl::DeleteId, MacroEncoding::HexPair));
        assert!(!feed(
            &mut buffer,
            "526570656174!3;206563;686F;207468726565"
        ));
        assert_eq!(buffer.macro_contents(9), Some([].as_slice()));
    }

    #[test]
    fn literal_controls_are_ignored_in_text_and_hex_definitions() {
        let mut buffer = MacroBuffer::default();
        assert!(buffer.init_parser(10, MacroDeleteControl::DeleteId, MacroEncoding::Text));
        for ch in units("A\u{7}B\u{8}C\tD\nE\u{b}F\u{c}G\rH") {
            assert!(buffer.parse_definition(ch));
        }
        assert_eq!(
            buffer.macro_contents(10),
            Some(units("ABCDEFGH").as_slice())
        );

        assert!(buffer.init_parser(11, MacroDeleteControl::DeleteId, MacroEncoding::HexPair));
        for ch in units("41\u{7}42\u{8}43\t44\n45\u{b}46\u{c}47\r48") {
            assert!(buffer.parse_definition(ch));
        }
        assert_eq!(
            buffer.macro_contents(11),
            Some(units("ABCDEFGH").as_slice())
        );
    }

    #[test]
    fn controls_encoded_as_hex_are_retained() {
        let mut buffer = MacroBuffer::default();
        define_hex(&mut buffer, 13, "410742084309440A450B460C470D481B49");
        assert_eq!(
            buffer.macro_contents(13),
            Some(units("A\u{7}B\u{8}C\tD\nE\u{b}F\u{c}G\rH\u{1b}I").as_slice())
        );
    }

    #[test]
    fn repeat_count_saturates_at_the_shared_vt_parameter_limit() {
        let mut buffer = MacroBuffer::default();
        assert!(buffer.init_parser(0, MacroDeleteControl::DeleteId, MacroEncoding::HexPair));
        assert!(feed(&mut buffer, "!999999999999999999999999999999999999;"));
        assert_eq!(
            buffer.repeat_count,
            usize::try_from(MAX_PARAMETER_VALUE).unwrap()
        );
    }

    #[test]
    fn invalid_macro_id_is_rejected_without_mutating_existing_data() {
        let mut buffer = MacroBuffer::default();
        define_text(&mut buffer, 1, "keep");
        assert!(!buffer.init_parser(
            MACRO_COUNT,
            MacroDeleteControl::DeleteAll,
            MacroEncoding::Text
        ));
        assert_eq!(buffer.macro_contents(1), Some(units("keep").as_slice()));
    }

    #[test]
    fn storage_limit_clears_definition_that_would_exceed_max_space() {
        let mut buffer = MacroBuffer::default();
        buffer.macros[0] = vec![u16::from(b'x'); MAX_SPACE - 1];
        buffer.space_used = MAX_SPACE - 1;
        assert!(buffer.init_parser(1, MacroDeleteControl::DeleteId, MacroEncoding::Text));
        assert!(buffer.parse_definition(u16::from(b'a')));
        assert!(!buffer.parse_definition(u16::from(b'b')));
        assert_eq!(buffer.macro_contents(1), Some([].as_slice()));
        assert_eq!(buffer.space_available(), 1);
    }

    #[test]
    fn checksum_uses_vt420_wrapping_subtraction() {
        let mut buffer = MacroBuffer::default();
        define_text(&mut buffer, 0, "A");
        define_text(&mut buffer, 1, "B");
        assert_eq!(
            buffer.calculate_checksum(),
            0u16.wrapping_sub(65).wrapping_sub(66)
        );
    }

    #[test]
    fn invocation_depth_is_strictly_limited_to_sixteen() {
        let mut buffer = MacroBuffer::default();
        define_text(&mut buffer, 0, "x");

        let mut context = InvocationContext::default();
        for expected_depth in 1..=MAX_INVOCATION_DEPTH {
            let prepared = buffer
                .prepare_invoke(0, context)
                .expect("invocation should fit");
            context = prepared.context();
            assert_eq!(context.depth(), expected_depth);
        }
        assert!(buffer.prepare_invoke(0, context).is_none());
    }

    #[test]
    fn cumulative_invocation_length_must_remain_strictly_below_max_space() {
        let mut buffer = MacroBuffer::default();
        define_text(&mut buffer, 0, "x");
        let context = InvocationContext {
            depth: 1,
            sequence_length: MAX_SPACE - 2,
        };
        let prepared = buffer
            .prepare_invoke(0, context)
            .expect("one unit still fits");
        assert_eq!(prepared.context().sequence_length(), MAX_SPACE - 1);
        assert!(buffer.prepare_invoke(0, prepared.context()).is_none());
    }

    #[test]
    fn macro_definition_is_rejected_from_inside_an_invocation() {
        let mut buffer = MacroBuffer::default();
        define_text(&mut buffer, 0, "outer");
        let context = buffer
            .prepare_invoke(0, InvocationContext::default())
            .expect("root invocation")
            .context();
        assert!(!buffer.init_parser_with_context(
            1,
            MacroDeleteControl::DeleteId,
            MacroEncoding::Text,
            context,
        ));
    }

    #[test]
    fn hard_reset_during_invocation_overwrites_without_releasing_storage() {
        let mut buffer = MacroBuffer::default();
        define_text(&mut buffer, 0, "Macro 0");
        define_text(&mut buffer, 1, "Macro 1");
        let used = buffer.space_used;
        let context = buffer
            .prepare_invoke(0, InvocationContext::default())
            .expect("root invocation")
            .context();

        buffer.clear_macros_if_in_use(context);
        assert_eq!(buffer.space_used, used);
        assert!(buffer.macro_contents(0).unwrap().iter().all(|&ch| ch == 0));
        assert!(buffer.macro_contents(1).unwrap().iter().all(|&ch| ch == 0));
    }

    #[test]
    fn prepare_invoke_returns_the_exact_utf16_sequence() {
        let mut buffer = MacroBuffer::default();
        define_text(&mut buffer, 63, "Macro 63");
        let prepared = buffer
            .prepare_invoke(63, InvocationContext::default())
            .expect("valid macro id");
        assert_eq!(prepared.sequence(), units("Macro 63"));
        assert!(
            buffer
                .prepare_invoke(MACRO_COUNT, InvocationContext::default())
                .is_none()
        );
    }
}
