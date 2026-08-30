use terminal_input::{Mode, TerminalInput};

#[test]
fn microsoft_adapter_cursor_key_owner_supports_set_and_reset() {
    let mut input = TerminalInput::new();
    input.set_input_mode(Mode::CursorKey, true);
    assert!(input.get_input_mode(Mode::CursorKey));
    input.set_input_mode(Mode::CursorKey, false);
    assert!(!input.get_input_mode(Mode::CursorKey));
}

#[test]
fn microsoft_adapter_keypad_owner_supports_application_and_numeric_modes() {
    let mut input = TerminalInput::new();
    input.set_input_mode(Mode::Keypad, true);
    assert!(input.get_input_mode(Mode::Keypad));
    input.set_input_mode(Mode::Keypad, false);
    assert!(!input.get_input_mode(Mode::Keypad));
}

#[test]
fn microsoft_adapter_mouse_mode_owner_supports_all_six_source_modes() {
    let mut input = TerminalInput::new();
    for mode in [
        Mode::DefaultMouseTracking,
        Mode::Utf8MouseEncoding,
        Mode::SgrMouseEncoding,
        Mode::ButtonEventMouseTracking,
        Mode::AnyEventMouseTracking,
        Mode::AlternateScroll,
    ] {
        input.set_input_mode(mode, true);
        assert!(input.get_input_mode(mode), "mode {mode:?} should be set");
        input.set_input_mode(mode, false);
        assert!(!input.get_input_mode(mode), "mode {mode:?} should be reset");
    }
}

#[test]
fn microsoft_adapter_send_c1_owner_supports_7bit_and_8bit_state() {
    let mut input = TerminalInput::new();
    input.set_input_mode(Mode::SendC1, true);
    assert!(input.get_input_mode(Mode::SendC1));
    input.set_input_mode(Mode::SendC1, false);
    assert!(!input.get_input_mode(Mode::SendC1));
}
