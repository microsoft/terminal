use terminal_input::{KeyEvent, TerminalInput, control_state, virtual_key};

const VK_OEM_2: u16 = 0xbf;

fn event(virtual_key: u16, codepoint: u32, control_key_state: u32, key_down: bool) -> KeyEvent {
    KeyEvent {
        virtual_key,
        scan_code: 0,
        codepoint,
        control_key_state,
        key_down,
        repeat_count: 1,
    }
}

fn translate(input: &mut TerminalInput, events: &[KeyEvent]) -> String {
    events
        .iter()
        .copied()
        .map(|event| input.handle_key(event))
        .collect()
}

#[test]
fn microsoft_alt_intermediate_roundtrip_preserves_alt_slash_then_ctrl_e() {
    let mut input = TerminalInput::new();

    let alt_slash = [
        event(virtual_key::MENU, 0, control_state::LEFT_ALT_PRESSED, true),
        event(
            VK_OEM_2,
            u32::from(b'/'),
            control_state::LEFT_ALT_PRESSED,
            true,
        ),
        event(
            VK_OEM_2,
            u32::from(b'/'),
            control_state::LEFT_ALT_PRESSED,
            false,
        ),
        event(virtual_key::MENU, 0, 0, false),
    ];
    assert_eq!(translate(&mut input, &alt_slash), "\u{1b}/");

    let ctrl_e = [
        event(
            virtual_key::CONTROL,
            0,
            control_state::LEFT_CTRL_PRESSED,
            true,
        ),
        event(
            u16::from(b'E'),
            0x05,
            control_state::LEFT_CTRL_PRESSED,
            true,
        ),
        event(
            u16::from(b'E'),
            0x05,
            control_state::LEFT_CTRL_PRESSED,
            false,
        ),
        event(virtual_key::CONTROL, 0, 0, false),
    ];
    assert_eq!(translate(&mut input, &ctrl_e), "\u{5}");
}
