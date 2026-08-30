use terminal_input::{KeyEvent, TerminalInput, control_state};

fn terminal_core_char_event(codepoint: char, virtual_key: u16, control_key_state: u32) -> KeyEvent {
    KeyEvent {
        virtual_key,
        scan_code: 0,
        codepoint: u32::from(codepoint),
        control_key_state,
        key_down: true,
        repeat_count: 1,
    }
}

#[test]
fn microsoft_terminal_core_alt_shift_key_contract() {
    // TerminalCore::SendCharEvent first resolves the character to a nonzero
    // virtual key and then synthesizes the KEY_EVENT consumed by TerminalInput.
    // The observable GH#637 contract is that Alt prefixes ESC while the already
    // translated WM_CHAR codepoint remains untouched by Shift handling.
    let mut input = TerminalInput::new();
    assert_eq!(
        input.handle_key(terminal_core_char_event(
            'a',
            u16::from(b'A'),
            control_state::LEFT_ALT_PRESSED,
        )),
        "\u{1b}a"
    );

    let mut input = TerminalInput::new();
    assert_eq!(
        input.handle_key(terminal_core_char_event(
            'A',
            u16::from(b'A'),
            control_state::LEFT_ALT_PRESSED | control_state::SHIFT_PRESSED,
        )),
        "\u{1b}A"
    );
}
