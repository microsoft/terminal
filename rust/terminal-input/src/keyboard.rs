//! Layout-aware keyboard sanitization and Kitty keyboard protocol encoding.
//!
//! The core remains platform neutral. A Windows adapter can implement
//! [`KeyboardMapper`] with `GetKeyboardLayout`/`ToUnicodeEx` later without
//! leaking Win32 or unsafe code into this crate.

use super::{
    ESC, KeyEvent, KittyKeyboardProtocolFlags, Mode, TerminalInput, codepoint_string,
    control_state, make_ctrl_char, virtual_key,
};

const INVALID_CODEPOINT: u32 = 0x11_0000;
const KITTY_TEXT_SENTINEL: u32 = 0;
const KITTY_LEGACY_SENTINEL: u32 = 1;
const KITTY_KP_0: u32 = 57_399;
const KITTY_KP_BEGIN: u32 = 57_427;

/// Narrow abstraction over keyboard-layout translation.
///
/// The methods correspond to the four translations the C++ `KeyboardHelper`
/// performs. Implementations may call an operating-system keyboard API, use a
/// deterministic fixture, or provide a portable approximation.
pub trait KeyboardMapper {
    /// Character produced by the active layout with Ctrl/Alt removed.
    fn unmodified_key(&self, event: &KeyEvent) -> Option<u32>;

    /// Kitty base key in the active layout (Ctrl/Alt/Shift/Caps removed, `AltGr` retained).
    fn kitty_base_key(&self, event: &KeyEvent, alt_gr: bool) -> Option<u32>;

    /// Kitty shifted key in the active layout.
    fn kitty_shifted_key(&self, event: &KeyEvent, alt_gr: bool) -> Option<u32>;

    /// Kitty base-layout key for the physical scan code (US PC-101 semantics).
    fn kitty_us_base_key(&self, event: &KeyEvent) -> Option<u32>;
}

/// Platform-neutral fallback used by [`TerminalInput::handle_key`].
///
/// It is exact for ASCII virtual keys and for events that already contain their
/// translated Unicode codepoint. A Windows integration layer can inject a richer
/// mapper through [`TerminalInput::handle_key_with_mapper`].
#[derive(Debug, Default, Clone, Copy)]
pub struct PortableKeyboardMapper;

impl KeyboardMapper for PortableKeyboardMapper {
    fn unmodified_key(&self, event: &KeyEvent) -> Option<u32> {
        ascii_base_key(event.virtual_key).or_else(|| valid_codepoint(event.codepoint))
    }

    fn kitty_base_key(&self, event: &KeyEvent, alt_gr: bool) -> Option<u32> {
        if alt_gr {
            valid_codepoint(event.codepoint).map(lowercase_ascii)
        } else {
            ascii_base_key(event.virtual_key)
                .or_else(|| valid_codepoint(event.codepoint).map(lowercase_ascii))
        }
    }

    fn kitty_shifted_key(&self, event: &KeyEvent, alt_gr: bool) -> Option<u32> {
        if alt_gr {
            valid_codepoint(event.codepoint)
        } else {
            ascii_shifted_key(event.virtual_key).or_else(|| valid_codepoint(event.codepoint))
        }
    }

    fn kitty_us_base_key(&self, event: &KeyEvent) -> Option<u32> {
        ascii_base_key(event.virtual_key)
    }
}

#[derive(Debug, Clone, Copy, Default)]
struct Modifiers(u8);

impl Modifiers {
    const ALT_GR: u8 = 1 << 0;
    const CTRL: u8 = 1 << 1;
    const ALT: u8 = 1 << 2;
    const SHIFT: u8 = 1 << 3;

    const fn alt_gr(self) -> bool {
        self.0 & Self::ALT_GR != 0
    }

    const fn ctrl(self) -> bool {
        self.0 & Self::CTRL != 0
    }

    const fn alt(self) -> bool {
        self.0 & Self::ALT != 0
    }

    const fn shift(self) -> bool {
        self.0 & Self::SHIFT != 0
    }
}

#[derive(Debug, Clone, Copy)]
struct SanitizedKeyEvent {
    raw: KeyEvent,
    codepoint: u32,
    key_repeat: bool,
    modifiers: Modifiers,
}

#[derive(Debug, Default)]
struct KittyEncoding {
    unicode_key: u32,
    shifted_key: u32,
    base_layout_key: u32,
    modifier: u32,
    event_type: u32,
    associated_text: u32,
}

pub(crate) fn handle_key<M: KeyboardMapper>(
    input: &mut TerminalInput,
    mut event: KeyEvent,
    mapper: &M,
) -> String {
    if input.get_input_mode(Mode::Win32)
        && !input.force_disable_win32_input_mode
        && input.kitty_flags == 0
    {
        return input.make_win32_output(event);
    }

    if let Some(output) = combine_surrogate(input, &mut event) {
        return output;
    }

    if event.key_down && event.virtual_key == 0 && event.codepoint == 0 {
        return String::new();
    }

    if event.key_down && (event.virtual_key == virtual_key::PACKET || event.virtual_key == 0) {
        return codepoint_string(event.codepoint);
    }

    let key_repeat = event.key_down && input.last_virtual_key == Some(event.virtual_key);
    if event.key_down {
        input.last_virtual_key = Some(event.virtual_key);
    } else {
        input.last_virtual_key = None;
    }

    if key_repeat && (is_modifier_key(event.virtual_key) || !input.get_input_mode(Mode::AutoRepeat))
    {
        return String::new();
    }

    let all_keys = flag(
        input.kitty_flags,
        KittyKeyboardProtocolFlags::REPORT_ALL_KEYS_AS_ESCAPE_CODES,
    );
    if is_modifier_key(event.virtual_key) && !all_keys {
        return String::new();
    }

    if !event.key_down {
        if event.control_key_state & control_state::NUMLOCK_ON != 0
            && event.virtual_key == virtual_key::MENU
            && event.codepoint != 0
        {
            return codepoint_string(event.codepoint);
        }
        if !flag(
            input.kitty_flags,
            KittyKeyboardProtocolFlags::REPORT_EVENT_TYPES,
        ) {
            return String::new();
        }
        let regular_enter = event.virtual_key == virtual_key::RETURN
            && event.control_key_state & control_state::ENHANCED_KEY == 0;
        if !all_keys
            && (regular_enter
                || event.virtual_key == virtual_key::TAB
                || event.virtual_key == virtual_key::BACK)
        {
            return String::new();
        }
    }

    let any_ctrl = event.control_key_state & control_state::CTRL_PRESSED != 0;
    let both_ctrl =
        event.control_key_state & control_state::CTRL_PRESSED == control_state::CTRL_PRESSED;
    let any_alt = event.control_key_state & control_state::ALT_PRESSED != 0;
    let both_alt =
        event.control_key_state & control_state::ALT_PRESSED == control_state::ALT_PRESSED;
    let alt_gr = any_alt && any_ctrl && event.codepoint > 0x20 && event.codepoint != 0x7f;
    let ctrl = both_ctrl || (any_ctrl && !alt_gr);
    let alt = both_alt || (any_alt && !alt_gr);
    let shift = event.control_key_state & control_state::SHIFT_PRESSED != 0;

    let modifier_state = Modifiers(
        u8::from(alt_gr) | (u8::from(ctrl) << 1) | (u8::from(alt) << 2) | (u8::from(shift) << 3),
    );
    let key = SanitizedKeyEvent {
        raw: event,
        codepoint: event.codepoint,
        key_repeat,
        modifiers: modifier_state,
    };

    if input.kitty_flags != 0
        && let Some(sequence) = encode_kitty(input, &key, mapper)
    {
        return sequence;
    }

    if !key.raw.key_down {
        return encode_regular_special(input, &key).unwrap_or_default();
    }

    if let Some(sequence) = encode_regular_special(input, &key) {
        return sequence;
    }

    encode_fallback(input, &key, mapper)
}

fn combine_surrogate(input: &mut TerminalInput, event: &mut KeyEvent) -> Option<String> {
    if (0xd800..=0xdbff).contains(&event.codepoint) {
        let Ok(code_unit) = u16::try_from(event.codepoint) else {
            return None;
        };
        input.leading_surrogate = Some(code_unit);
        return Some(String::new());
    }

    if let Some(leading) = input.leading_surrogate.take()
        && (0xdc00..=0xdfff).contains(&event.codepoint)
        && let Ok(trailing) = u16::try_from(event.codepoint)
    {
        let high = u32::from(leading) - 0xd800;
        let low = u32::from(trailing) - 0xdc00;
        event.codepoint = 0x1_0000 + ((high << 10) | low);
    }
    None
}

fn encode_kitty<M: KeyboardMapper>(
    input: &TerminalInput,
    key: &SanitizedKeyEvent,
    mapper: &M,
) -> Option<String> {
    let functional = kitty_functional_key(
        key.raw.virtual_key,
        key.raw.scan_code,
        key.raw.control_key_state & control_state::ENHANCED_KEY != 0,
    );
    let mut enc = KittyEncoding {
        modifier: modifier_bits(key),
        ..KittyEncoding::default()
    };

    let disambiguate = flag(
        input.kitty_flags,
        KittyKeyboardProtocolFlags::DISAMBIGUATE_ESCAPE_CODES,
    );
    let report_events = flag(
        input.kitty_flags,
        KittyKeyboardProtocolFlags::REPORT_EVENT_TYPES,
    );
    let all_keys = flag(
        input.kitty_flags,
        KittyKeyboardProtocolFlags::REPORT_ALL_KEYS_AS_ESCAPE_CODES,
    );

    if disambiguate
        && functional > KITTY_LEGACY_SENTINEL
        && (functional == 27
            || (functional <= 127 && enc.modifier != 0)
            || (KITTY_KP_0..=KITTY_KP_BEGIN).contains(&functional))
    {
        enc.unicode_key = functional;
    }

    if report_events {
        if !key.raw.key_down {
            enc.event_type = 3;
        } else if key.key_repeat {
            enc.event_type = 2;
        }
    }

    let release = enc.event_type == 3;
    let disambiguated_text = disambiguate && functional == KITTY_TEXT_SENTINEL && enc.modifier > 1;
    if all_keys || disambiguated_text || release {
        if is_kitty_functional(functional) {
            enc.unicode_key = functional;
        } else if functional == KITTY_TEXT_SENTINEL
            && let Some(codepoint) = mapper.kitty_base_key(&key.raw, key.modifiers.alt_gr())
            && codepoint < INVALID_CODEPOINT
        {
            enc.unicode_key = codepoint;
        }

        if flag(
            input.kitty_flags,
            KittyKeyboardProtocolFlags::REPORT_ASSOCIATED_TEXT,
        ) && !release
            && is_kitty_valid_text(key.codepoint)
        {
            enc.associated_text = key.codepoint;
        }
    }

    if flag(
        input.kitty_flags,
        KittyKeyboardProtocolFlags::REPORT_ALTERNATE_KEYS,
    ) && enc.unicode_key != 0
    {
        if functional == KITTY_TEXT_SENTINEL
            && key.modifiers.shift()
            && let Some(codepoint) = mapper.kitty_shifted_key(&key.raw, key.modifiers.alt_gr())
            && codepoint < INVALID_CODEPOINT
        {
            enc.shifted_key = codepoint;
        }
        if key.raw.scan_code != 0
            && let Some(codepoint) = mapper.kitty_us_base_key(&key.raw)
            && codepoint < INVALID_CODEPOINT
            && codepoint != enc.unicode_key
        {
            enc.base_layout_key = codepoint;
        }
    }

    if is_kitty_functional(enc.unicode_key) || all_keys {
        if key.raw.control_key_state & control_state::CAPSLOCK_ON != 0 {
            enc.modifier |= 64;
        }
        if key.raw.control_key_state & control_state::NUMLOCK_ON != 0 {
            enc.modifier |= 128;
        }
    }

    (enc.unicode_key != 0).then(|| format_kitty(input, &enc))
}

fn format_kitty(input: &TerminalInput, enc: &KittyEncoding) -> String {
    let mut output = input.csi_prefix();
    output.push_str(&enc.unicode_key.to_string());
    if enc.shifted_key != 0 || enc.base_layout_key != 0 {
        output.push(':');
        if enc.shifted_key != 0 {
            output.push_str(&enc.shifted_key.to_string());
        }
        if enc.base_layout_key != 0 {
            output.push(':');
            output.push_str(&enc.base_layout_key.to_string());
        }
    }

    if enc.modifier != 0 || enc.event_type != 0 || enc.associated_text != 0 {
        output.push(';');
        if enc.modifier != 0 || enc.event_type != 0 {
            output.push_str(&(enc.modifier + 1).to_string());
            if enc.event_type != 0 {
                output.push(':');
                output.push_str(&enc.event_type.to_string());
            }
        }
        if enc.associated_text != 0 {
            output.push(';');
            output.push_str(&enc.associated_text.to_string());
        }
    }
    output.push('u');
    output
}

#[expect(
    clippy::too_many_lines,
    reason = "VT key dispatch stays contiguous for direct Microsoft parity review"
)]
fn encode_regular_special(input: &TerminalInput, key: &SanitizedKeyEvent) -> Option<String> {
    let kitty_regular = input.kitty_flags
        & (KittyKeyboardProtocolFlags::DISAMBIGUATE_ESCAPE_CODES
            | KittyKeyboardProtocolFlags::REPORT_EVENT_TYPES
            | KittyKeyboardProtocolFlags::REPORT_ALL_KEYS_AS_ESCAPE_CODES)
        != 0;
    let modifier = modifier_bits(key);
    let event_type = if flag(
        input.kitty_flags,
        KittyKeyboardProtocolFlags::REPORT_EVENT_TYPES,
    ) {
        if !key.raw.key_down {
            3
        } else if key.key_repeat {
            2
        } else {
            0
        }
    } else {
        0
    };
    let ansi = input.get_input_mode(Mode::Ansi);

    match key.raw.virtual_key {
        virtual_key::BACK => {
            let backarrow = input.get_input_mode(Mode::BackarrowKey);
            let character = if key.modifiers.ctrl() == backarrow {
                '\u{7f}'
            } else {
                '\u{8}'
            };
            Some(alt_prefix(
                character.to_string(),
                key.modifiers.alt() && ansi,
            ))
        }
        virtual_key::TAB => {
            let sequence = if key.modifiers.shift() {
                format!("{}Z", input.csi_prefix())
            } else {
                "\t".to_string()
            };
            Some(alt_prefix(sequence, key.modifiers.alt() && ansi))
        }
        virtual_key::RETURN => {
            let enhanced = key.raw.control_key_state & control_state::ENHANCED_KEY != 0;
            let sequence = if input.get_input_mode(Mode::Keypad) && enhanced {
                if ansi {
                    format!("{}M", input.ss3_prefix())
                } else {
                    format!("{ESC}?M")
                }
            } else if key.modifiers.ctrl() {
                "\n".to_string()
            } else if input.get_input_mode(Mode::LineFeed) {
                "\r\n".to_string()
            } else {
                "\r".to_string()
            };
            Some(alt_prefix(sequence, key.modifiers.alt() && ansi))
        }
        virtual_key::PAUSE => Some("\u{1a}".to_string()),
        virtual_key::CANCEL => Some("\u{3}".to_string()),
        virtual_key::F1..=virtual_key::F4 => {
            let final_character = match key.raw.virtual_key - virtual_key::F1 {
                0 => 'P',
                1 => 'Q',
                2 => 'R',
                _ => 'S',
            };
            if !ansi {
                return Some(format!("{ESC}{final_character}"));
            }
            if kitty_regular && key.raw.virtual_key == virtual_key::F1 + 2 {
                return Some(format_csi(input, Some(13), modifier, event_type, '~'));
            }
            if kitty_regular || modifier != 0 || event_type != 0 {
                Some(format_csi(
                    input,
                    None,
                    modifier,
                    event_type,
                    final_character,
                ))
            } else {
                Some(format!("{}{final_character}", input.ss3_prefix()))
            }
        }
        virtual_key::F5..=virtual_key::F20 => {
            if !ansi {
                return Some(match key.raw.virtual_key {
                    virtual_key::F11 => ESC.to_string(),
                    virtual_key::F12 => "\u{8}".to_string(),
                    virtual_key::F13 => "\n".to_string(),
                    _ => String::new(),
                });
            }
            let number =
                super::FUNCTION_KEY_NUMBERS[usize::from(key.raw.virtual_key - virtual_key::F5)];
            Some(format_csi(input, Some(number), modifier, event_type, '~'))
        }
        virtual_key::LEFT | virtual_key::UP | virtual_key::RIGHT | virtual_key::DOWN => {
            let final_character = match key.raw.virtual_key {
                virtual_key::LEFT => 'D',
                virtual_key::UP => 'A',
                virtual_key::RIGHT => 'C',
                _ => 'B',
            };
            if !ansi {
                return Some(format!("{ESC}{final_character}"));
            }
            if !kitty_regular
                && modifier == 0
                && event_type == 0
                && input.get_input_mode(Mode::CursorKey)
            {
                Some(format!("{}{final_character}", input.ss3_prefix()))
            } else if modifier == 0 && event_type == 0 {
                Some(format!("{}{final_character}", input.csi_prefix()))
            } else {
                Some(format_csi(
                    input,
                    None,
                    modifier,
                    event_type,
                    final_character,
                ))
            }
        }
        virtual_key::CLEAR | virtual_key::HOME | virtual_key::END => {
            let final_character = match key.raw.virtual_key {
                virtual_key::CLEAR => 'E',
                virtual_key::HOME => 'H',
                _ => 'F',
            };
            if !ansi {
                return Some(format!("{ESC}{final_character}"));
            }
            if !kitty_regular
                && modifier == 0
                && event_type == 0
                && input.get_input_mode(Mode::CursorKey)
            {
                Some(format!("{}{final_character}", input.ss3_prefix()))
            } else if modifier == 0 && event_type == 0 {
                Some(format!("{}{final_character}", input.csi_prefix()))
            } else {
                Some(format_csi(
                    input,
                    None,
                    modifier,
                    event_type,
                    final_character,
                ))
            }
        }
        virtual_key::INSERT | virtual_key::DELETE if ansi => {
            let number = 2 + (key.raw.virtual_key - virtual_key::INSERT);
            Some(format_csi(input, Some(number), modifier, event_type, '~'))
        }
        virtual_key::PRIOR | virtual_key::NEXT if ansi => {
            let number = 5 + (key.raw.virtual_key - virtual_key::PRIOR);
            Some(format_csi(input, Some(number), modifier, event_type, '~'))
        }
        virtual_key::NUMPAD0..=virtual_key::NUMPAD9 if input.get_input_mode(Mode::Keypad) => {
            let final_character = char::from_u32(
                u32::from(b'p') + u32::from(key.raw.virtual_key - virtual_key::NUMPAD0),
            )
            .unwrap_or('p');
            Some(if ansi {
                format!("{}{final_character}", input.ss3_prefix())
            } else {
                format!("{ESC}?{final_character}")
            })
        }
        virtual_key::MULTIPLY..=virtual_key::DIVIDE if input.get_input_mode(Mode::Keypad) => {
            let final_character = char::from_u32(
                u32::from(b'j') + u32::from(key.raw.virtual_key - virtual_key::MULTIPLY),
            )
            .unwrap_or('j');
            Some(if ansi {
                format!("{}{final_character}", input.ss3_prefix())
            } else {
                format!("{ESC}?{final_character}")
            })
        }
        _ => None,
    }
}

fn format_csi(
    input: &TerminalInput,
    number: Option<u16>,
    modifier: u32,
    event_type: u32,
    final_character: char,
) -> String {
    let mut output = input.csi_prefix();
    if modifier == 0 && event_type == 0 {
        if let Some(number) = number {
            output.push_str(&number.to_string());
        }
    } else {
        output.push_str(&number.unwrap_or(1).to_string());
        output.push(';');
        output.push_str(&(modifier + 1).to_string());
        if event_type != 0 {
            output.push(':');
            output.push_str(&event_type.to_string());
        }
    }
    output.push(final_character);
    output
}

fn encode_fallback<M: KeyboardMapper>(
    input: &TerminalInput,
    key: &SanitizedKeyEvent,
    mapper: &M,
) -> String {
    if !key.raw.key_down {
        return String::new();
    }

    let ctrl_space = key.modifiers.ctrl() && key.raw.virtual_key == virtual_key::SPACE;
    let mut codepoint = if key.codepoint != 0 && !ctrl_space {
        key.codepoint
    } else if key.modifiers.alt() || key.modifiers.ctrl() {
        match mapper.unmodified_key(&key.raw) {
            Some(value) if value < INVALID_CODEPOINT => value,
            _ => return String::new(),
        }
    } else {
        return String::new();
    };

    if key.modifiers.ctrl() {
        codepoint = make_ctrl_char(codepoint);
        if codepoint >= u32::from(b' ')
            && (u16::from(b'2')..=u16::from(b'Z')).contains(&key.raw.virtual_key)
        {
            codepoint = make_ctrl_char(u32::from(key.raw.virtual_key));
        }
    }

    let mut output = String::new();
    if key.modifiers.alt() && input.get_input_mode(Mode::Ansi) {
        output.push(ESC);
    }
    output.push_str(&codepoint_string(codepoint));
    output
}

fn kitty_functional_key(mut virtual_key: u16, scan_code: u16, enhanced: bool) -> u32 {
    match virtual_key {
        virtual_key::ESCAPE => return 27,
        virtual_key::RETURN => return if enhanced { 57_414 } else { 13 },
        virtual_key::TAB => return 9,
        virtual_key::BACK => return 127,
        virtual_key::LEFT
        | virtual_key::RIGHT
        | virtual_key::UP
        | virtual_key::DOWN
        | virtual_key::PRIOR
        | virtual_key::NEXT
        | virtual_key::HOME
        | virtual_key::END
        | virtual_key::INSERT
        | virtual_key::DELETE
            if enhanced =>
        {
            return KITTY_LEGACY_SENTINEL;
        }
        virtual_key::SHIFT => {
            virtual_key = if scan_code == 0x36 {
                virtual_key::RSHIFT
            } else {
                virtual_key::LSHIFT
            };
        }
        virtual_key::CONTROL => {
            virtual_key = if enhanced {
                virtual_key::RCONTROL
            } else {
                virtual_key::LCONTROL
            };
        }
        virtual_key::MENU => {
            virtual_key = if enhanced {
                virtual_key::RMENU
            } else {
                virtual_key::LMENU
            };
        }
        _ => {}
    }

    match virtual_key {
        virtual_key::CAPITAL => 57_358,
        virtual_key::SCROLL => 57_359,
        virtual_key::NUMLOCK => 57_360,
        virtual_key::SNAPSHOT => 57_361,
        virtual_key::PAUSE => 57_362,
        virtual_key::APPS => 57_363,
        virtual_key::F1..=virtual_key::F12 => KITTY_LEGACY_SENTINEL,
        virtual_key::F13..=virtual_key::F24 => 57_376 + u32::from(virtual_key - virtual_key::F13),
        virtual_key::NUMPAD0..=virtual_key::NUMPAD9 => {
            57_399 + u32::from(virtual_key - virtual_key::NUMPAD0)
        }
        virtual_key::DECIMAL => 57_409,
        virtual_key::DIVIDE => 57_410,
        virtual_key::MULTIPLY => 57_411,
        virtual_key::SUBTRACT => 57_412,
        virtual_key::ADD => 57_413,
        virtual_key::SEPARATOR => 57_416,
        virtual_key::LEFT => 57_417,
        virtual_key::RIGHT => 57_418,
        virtual_key::UP => 57_419,
        virtual_key::DOWN => 57_420,
        virtual_key::PRIOR => 57_421,
        virtual_key::NEXT => 57_422,
        virtual_key::HOME => 57_423,
        virtual_key::END => 57_424,
        virtual_key::INSERT => 57_425,
        virtual_key::DELETE => 57_426,
        virtual_key::CLEAR => KITTY_LEGACY_SENTINEL,
        virtual_key::MEDIA_PLAY_PAUSE => 57_430,
        virtual_key::MEDIA_STOP => 57_432,
        virtual_key::MEDIA_NEXT_TRACK => 57_435,
        virtual_key::MEDIA_PREV_TRACK => 57_436,
        virtual_key::VOLUME_DOWN => 57_438,
        virtual_key::VOLUME_UP => 57_439,
        virtual_key::VOLUME_MUTE => 57_440,
        virtual_key::LSHIFT => 57_441,
        virtual_key::LCONTROL => 57_442,
        virtual_key::LMENU => 57_443,
        virtual_key::LWIN => 57_444,
        virtual_key::RSHIFT => 57_447,
        virtual_key::RCONTROL => 57_448,
        virtual_key::RMENU => 57_449,
        virtual_key::RWIN => 57_450,
        _ => KITTY_TEXT_SENTINEL,
    }
}

fn modifier_bits(key: &SanitizedKeyEvent) -> u32 {
    u32::from(key.modifiers.shift())
        | (u32::from(key.modifiers.alt()) << 1)
        | (u32::from(key.modifiers.ctrl()) << 2)
}

fn is_kitty_functional(codepoint: u32) -> bool {
    (codepoint > KITTY_LEGACY_SENTINEL && codepoint <= 0x1f)
        || (0x7f..=0x9f).contains(&codepoint)
        || (0xe000..=0xf8ff).contains(&codepoint)
}

fn is_kitty_valid_text(codepoint: u32) -> bool {
    (codepoint > 0x1f && codepoint < 0x7f) || (codepoint > 0x9f && codepoint < INVALID_CODEPOINT)
}

fn is_modifier_key(key: u16) -> bool {
    (virtual_key::SHIFT..=virtual_key::MENU).contains(&key)
        || (virtual_key::LSHIFT..=virtual_key::RMENU).contains(&key)
}

fn flag(flags: u8, flag: u8) -> bool {
    flags & flag != 0
}

fn alt_prefix(sequence: String, enabled: bool) -> String {
    if enabled {
        format!("{ESC}{sequence}")
    } else {
        sequence
    }
}

fn valid_codepoint(codepoint: u32) -> Option<u32> {
    (codepoint < INVALID_CODEPOINT && char::from_u32(codepoint).is_some()).then_some(codepoint)
}

fn ascii_base_key(virtual_key: u16) -> Option<u32> {
    match virtual_key {
        value if (u16::from(b'A')..=u16::from(b'Z')).contains(&value) => {
            Some(u32::from(value + u16::from(b'a' - b'A')))
        }
        value if (0x20..=0x7e).contains(&value) => Some(u32::from(value)),
        _ => None,
    }
}

fn ascii_shifted_key(virtual_key: u16) -> Option<u32> {
    match virtual_key {
        value if (u16::from(b'A')..=u16::from(b'Z')).contains(&value) => Some(u32::from(value)),
        _ => ascii_base_key(virtual_key),
    }
}

fn lowercase_ascii(codepoint: u32) -> u32 {
    if (u32::from(b'A')..=u32::from(b'Z')).contains(&codepoint) {
        codepoint + u32::from(b'a' - b'A')
    } else {
        codepoint
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Debug, Default)]
    struct AzertyFixture;

    impl KeyboardMapper for AzertyFixture {
        fn unmodified_key(&self, event: &KeyEvent) -> Option<u32> {
            PortableKeyboardMapper.unmodified_key(event)
        }

        fn kitty_base_key(&self, event: &KeyEvent, alt_gr: bool) -> Option<u32> {
            if alt_gr {
                valid_codepoint(event.codepoint).map(lowercase_ascii)
            } else if event.virtual_key == u16::from(b'A') {
                Some(u32::from(b'a'))
            } else {
                PortableKeyboardMapper.kitty_base_key(event, alt_gr)
            }
        }

        fn kitty_shifted_key(&self, event: &KeyEvent, alt_gr: bool) -> Option<u32> {
            if event.virtual_key == u16::from(b'A') && !alt_gr {
                Some(u32::from(b'A'))
            } else {
                PortableKeyboardMapper.kitty_shifted_key(event, alt_gr)
            }
        }

        fn kitty_us_base_key(&self, event: &KeyEvent) -> Option<u32> {
            if event.virtual_key == u16::from(b'A') && event.scan_code == 0x10 {
                Some(u32::from(b'q'))
            } else {
                PortableKeyboardMapper.kitty_us_base_key(event)
            }
        }
    }

    fn event(vk: u16, sc: u16, cp: u32, state: u32) -> KeyEvent {
        KeyEvent {
            virtual_key: vk,
            scan_code: sc,
            codepoint: cp,
            control_key_state: state,
            key_down: true,
            repeat_count: 1,
        }
    }

    fn input(flags: u8) -> TerminalInput {
        let mut input = TerminalInput::new();
        input.set_kitty_keyboard_protocol(flags, super::super::KittyKeyboardProtocolMode::Replace);
        input
    }

    #[test]
    fn disambiguation_and_altgr_match_microsoft_cases() {
        let mut value = input(KittyKeyboardProtocolFlags::DISAMBIGUATE_ESCAPE_CODES);
        assert_eq!(
            value.handle_key_with_mapper(event(virtual_key::ESCAPE, 1, 0, 0), &AzertyFixture),
            "\u{1b}[27u"
        );

        let mut value = input(KittyKeyboardProtocolFlags::DISAMBIGUATE_ESCAPE_CODES);
        assert_eq!(
            value.handle_key_with_mapper(
                event(u16::from(b'A'), 0x10, 0, control_state::LEFT_CTRL_PRESSED),
                &AzertyFixture
            ),
            "\u{1b}[97;5u"
        );

        let mut value = input(KittyKeyboardProtocolFlags::DISAMBIGUATE_ESCAPE_CODES);
        assert_eq!(
            value.handle_key_with_mapper(
                event(
                    u16::from(b'A'),
                    0x10,
                    u32::from('æ'),
                    control_state::LEFT_CTRL_PRESSED | control_state::LEFT_ALT_PRESSED
                ),
                &AzertyFixture
            ),
            "æ"
        );
    }

    #[test]
    fn all_keys_modifiers_locks_and_associated_text_match_microsoft() {
        let k = KittyKeyboardProtocolFlags::REPORT_ALL_KEYS_AS_ESCAPE_CODES;
        let mut value = input(k);
        assert_eq!(
            value.handle_key_with_mapper(
                event(u16::from(b'A'), 0x10, u32::from(b'a'), 0),
                &AzertyFixture
            ),
            "\u{1b}[97u"
        );

        let mut value = input(k);
        assert_eq!(
            value.handle_key_with_mapper(
                event(
                    u16::from(b'A'),
                    0x10,
                    u32::from(b'A'),
                    control_state::SHIFT_PRESSED
                ),
                &AzertyFixture
            ),
            "\u{1b}[97;2u"
        );

        let mut value = input(k);
        assert_eq!(
            value.handle_key_with_mapper(
                event(
                    u16::from(b'A'),
                    0x10,
                    u32::from(b'A'),
                    control_state::CAPSLOCK_ON
                ),
                &AzertyFixture
            ),
            "\u{1b}[97;65u"
        );

        let mut value = input(k | KittyKeyboardProtocolFlags::REPORT_ASSOCIATED_TEXT);
        assert_eq!(
            value.handle_key_with_mapper(
                event(u16::from(b'A'), 0x10, u32::from(b'a'), 0),
                &AzertyFixture
            ),
            "\u{1b}[97;;97u"
        );
    }

    #[test]
    fn alternate_keys_use_layout_mapper() {
        let flags = KittyKeyboardProtocolFlags::REPORT_ALTERNATE_KEYS
            | KittyKeyboardProtocolFlags::REPORT_ALL_KEYS_AS_ESCAPE_CODES;
        let mut value = input(flags);
        assert_eq!(
            value.handle_key_with_mapper(
                event(
                    u16::from(b'A'),
                    0x10,
                    u32::from(b'A'),
                    control_state::SHIFT_PRESSED
                ),
                &AzertyFixture,
            ),
            "\u{1b}[97:65:113;2u"
        );
    }

    #[test]
    fn keypad_pua_and_modifier_keys_match_microsoft() {
        let d = KittyKeyboardProtocolFlags::DISAMBIGUATE_ESCAPE_CODES;
        let mut value = input(d);
        assert_eq!(
            value.handle_key_with_mapper(
                event(virtual_key::NUMPAD0, 0x52, u32::from(b'0'), 0),
                &AzertyFixture
            ),
            "\u{1b}[57399u"
        );

        let k = KittyKeyboardProtocolFlags::REPORT_ALL_KEYS_AS_ESCAPE_CODES;
        let mut value = input(k);
        assert_eq!(
            value.handle_key_with_mapper(
                event(virtual_key::SHIFT, 0x2a, 0, control_state::SHIFT_PRESSED),
                &AzertyFixture
            ),
            "\u{1b}[57441;2u"
        );
    }

    #[test]
    fn legacy_function_keys_use_kitty_csi_rules() {
        let d = KittyKeyboardProtocolFlags::DISAMBIGUATE_ESCAPE_CODES;
        let mut value = input(d);
        assert_eq!(
            value.handle_key_with_mapper(event(virtual_key::F1, 0x3b, 0, 0), &AzertyFixture),
            "\u{1b}[P"
        );
        let mut value = input(d);
        assert_eq!(
            value.handle_key_with_mapper(event(virtual_key::F1 + 2, 0x3d, 0, 0), &AzertyFixture),
            "\u{1b}[13~"
        );
    }

    #[test]
    fn event_types_report_repeat_and_release() {
        let flags = KittyKeyboardProtocolFlags::REPORT_EVENT_TYPES
            | KittyKeyboardProtocolFlags::REPORT_ALL_KEYS_AS_ESCAPE_CODES;
        let mut value = input(flags);
        let down = event(u16::from(b'A'), 0x10, u32::from(b'a'), 0);
        assert_eq!(
            value.handle_key_with_mapper(down, &AzertyFixture),
            "\u{1b}[97u"
        );
        assert_eq!(
            value.handle_key_with_mapper(down, &AzertyFixture),
            "\u{1b}[97;1:2u"
        );
        let mut up = down;
        up.key_down = false;
        assert_eq!(
            value.handle_key_with_mapper(up, &AzertyFixture),
            "\u{1b}[97;1:3u"
        );
    }

    #[test]
    fn navigation_release_uses_regular_csi_event_type() {
        let flags = KittyKeyboardProtocolFlags::REPORT_EVENT_TYPES
            | KittyKeyboardProtocolFlags::REPORT_ALL_KEYS_AS_ESCAPE_CODES;
        let mut value = input(flags);
        let mut up = event(virtual_key::UP, 0x48, 0, control_state::ENHANCED_KEY);
        up.key_down = false;
        assert_eq!(
            value.handle_key_with_mapper(up, &AzertyFixture),
            "\u{1b}[1;1:3A"
        );
    }

    #[test]
    fn microsoft_terminal_core_invalid_key_event_is_silent() {
        let mut value = TerminalInput::new();
        assert_eq!(
            value.handle_key_with_mapper(event(0, 123, 0, 0), &AzertyFixture),
            ""
        );
        assert_eq!(
            value.handle_key_with_mapper(event(255, 123, 0, 0), &AzertyFixture),
            ""
        );
    }

    #[test]
    fn surrogate_pairs_are_combined_before_encoding() {
        let mut value = TerminalInput::new();
        let first = event(virtual_key::PACKET, 0, 0xd83d, 0);
        let second = event(virtual_key::PACKET, 0, 0xdc4d, 0);
        assert_eq!(value.handle_key_with_mapper(first, &AzertyFixture), "");
        assert_eq!(value.handle_key_with_mapper(second, &AzertyFixture), "👍");
    }
}
