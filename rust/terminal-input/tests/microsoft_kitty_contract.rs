use terminal_input::{
    KeyEvent, KeyboardMapper, KittyKeyboardProtocolFlags, KittyKeyboardProtocolMode, TerminalInput,
    control_state,
};

fn key(
    key_down: bool,
    virtual_key: u16,
    scan_code: u16,
    codepoint: u32,
    control_key_state: u32,
) -> KeyEvent {
    KeyEvent {
        key_down,
        virtual_key,
        scan_code,
        codepoint,
        control_key_state,
        repeat_count: 1,
    }
}

fn kitty_input(flags: u8) -> TerminalInput {
    let mut input = TerminalInput::new();
    input.set_kitty_keyboard_protocol(flags, KittyKeyboardProtocolMode::Replace);
    input
}

#[derive(Debug, Default, Clone, Copy)]
struct DeadKeyMapper;

impl KeyboardMapper for DeadKeyMapper {
    fn unmodified_key(&self, _event: &KeyEvent) -> Option<u32> {
        None
    }

    fn kitty_base_key(&self, _event: &KeyEvent, _alt_gr: bool) -> Option<u32> {
        None
    }

    fn kitty_shifted_key(&self, _event: &KeyEvent, _alt_gr: bool) -> Option<u32> {
        None
    }

    fn kitty_us_base_key(&self, _event: &KeyEvent) -> Option<u32> {
        None
    }
}

#[test]
fn microsoft_kitty_key_repeat_events_match_press_repeat_release_reset_contract() {
    let flags = KittyKeyboardProtocolFlags::REPORT_EVENT_TYPES
        | KittyKeyboardProtocolFlags::REPORT_ALL_KEYS_AS_ESCAPE_CODES;
    let mut input = kitty_input(flags);
    let down = key(true, u16::from(b'A'), 0x10, u32::from(b'a'), 0);
    let up = key(false, u16::from(b'A'), 0x10, u32::from(b'a'), 0);

    assert_eq!(input.handle_key(down), "\u{1b}[97u");
    assert_eq!(input.handle_key(down), "\u{1b}[97;1:2u");
    assert_eq!(input.handle_key(down), "\u{1b}[97;1:2u");
    assert_eq!(input.handle_key(up), "\u{1b}[97;1:3u");
    assert_eq!(input.handle_key(down), "\u{1b}[97u");
}

#[test]
fn microsoft_kitty_key_repeat_with_modifiers_preserves_modifier_contract() {
    let flags = KittyKeyboardProtocolFlags::REPORT_EVENT_TYPES
        | KittyKeyboardProtocolFlags::REPORT_ALL_KEYS_AS_ESCAPE_CODES;
    let mut input = kitty_input(flags);
    let down = key(
        true,
        u16::from(b'A'),
        0x10,
        u32::from(b'A'),
        control_state::SHIFT_PRESSED,
    );

    assert_eq!(input.handle_key(down), "\u{1b}[97;2u");
    assert_eq!(input.handle_key(down), "\u{1b}[97;2:2u");
}

#[test]
fn microsoft_kitty_key_repeat_resets_on_different_key_contract() {
    let flags = KittyKeyboardProtocolFlags::REPORT_EVENT_TYPES
        | KittyKeyboardProtocolFlags::REPORT_ALL_KEYS_AS_ESCAPE_CODES;
    let mut input = kitty_input(flags);

    assert_eq!(
        input.handle_key(key(true, u16::from(b'A'), 0x10, u32::from(b'a'), 0)),
        "\u{1b}[97u"
    );
    assert_eq!(
        input.handle_key(key(true, u16::from(b'B'), 0x30, u32::from(b'b'), 0)),
        "\u{1b}[98u"
    );
    assert_eq!(
        input.handle_key(key(true, u16::from(b'A'), 0x10, u32::from(b'a'), 0)),
        "\u{1b}[97u"
    );
}

#[test]
fn microsoft_kitty_ignore_dead_key_release_contract() {
    let mut input = kitty_input(KittyKeyboardProtocolFlags::REPORT_EVENT_TYPES);
    // Microsoft runs this under French (Standard, AZERTY). Its KeyboardHelper
    // calls ToUnicodeEx, whose dead-key result cannot be represented as one
    // codepoint and is therefore rejected. The Rust core models that platform
    // result through KeyboardMapper rather than hard-coding a layout-specific VK.
    let dead_key_release = key(
        false,
        0x00dd,
        0x0d,
        u32::from('¨'),
        control_state::SHIFT_PRESSED,
    );
    assert_eq!(
        input.handle_key_with_mapper(dead_key_release, &DeadKeyMapper),
        ""
    );
}
