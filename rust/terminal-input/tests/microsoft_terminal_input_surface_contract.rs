use terminal_input::{KeyEvent, TerminalInput, control_state, virtual_key};

fn key_down(virtual_key: u16) -> KeyEvent {
    KeyEvent {
        virtual_key,
        scan_code: 0,
        // Microsoft TerminalInputTests populates UnicodeChar with
        // MapVirtualKeyW(..., MAPVK_VK_TO_CHAR) for key-down events. Escape is
        // the one fixed-key case where the portable Rust fallback consumes it.
        codepoint: if virtual_key == virtual_key::ESCAPE {
            u32::from(b'\x1b')
        } else {
            0
        },
        control_key_state: 0,
        key_down: true,
        repeat_count: 1,
    }
}

fn modified_key_down(virtual_key: u16, control_key_state: u32) -> KeyEvent {
    KeyEvent {
        virtual_key,
        scan_code: 0,
        // The fixed VT special keys in Microsoft's modifier table do not need
        // ToUnicodeEx output. Keeping this at zero deliberately excludes the
        // layout-dependent default/OEM branches from this portable contract.
        codepoint: 0,
        control_key_state,
        key_down: true,
        repeat_count: 1,
    }
}

fn key_up(virtual_key: u16) -> KeyEvent {
    KeyEvent {
        virtual_key,
        scan_code: 0,
        codepoint: 0,
        control_key_state: 0,
        key_down: false,
        repeat_count: 1,
    }
}

#[test]
fn microsoft_terminal_input_tests_fixed_special_key_table() {
    let cases = [
        (virtual_key::TAB, "\t"),
        (virtual_key::BACK, "\u{7f}"),
        (virtual_key::ESCAPE, "\u{1b}"),
        (virtual_key::PAUSE, "\u{1a}"),
        (virtual_key::UP, "\u{1b}[A"),
        (virtual_key::DOWN, "\u{1b}[B"),
        (virtual_key::RIGHT, "\u{1b}[C"),
        (virtual_key::LEFT, "\u{1b}[D"),
        (virtual_key::CLEAR, "\u{1b}[E"),
        (virtual_key::HOME, "\u{1b}[H"),
        (virtual_key::INSERT, "\u{1b}[2~"),
        (virtual_key::DELETE, "\u{1b}[3~"),
        (virtual_key::END, "\u{1b}[F"),
        (virtual_key::PRIOR, "\u{1b}[5~"),
        (virtual_key::NEXT, "\u{1b}[6~"),
        (0x70, "\u{1b}OP"),
        (0x71, "\u{1b}OQ"),
        (0x72, "\u{1b}OR"),
        (0x73, "\u{1b}OS"),
        (0x74, "\u{1b}[15~"),
        (0x75, "\u{1b}[17~"),
        (0x76, "\u{1b}[18~"),
        (0x77, "\u{1b}[19~"),
        (0x78, "\u{1b}[20~"),
        (0x79, "\u{1b}[21~"),
        (0x7a, "\u{1b}[23~"),
        (0x7b, "\u{1b}[24~"),
        (0x7c, "\u{1b}[25~"),
        (0x7d, "\u{1b}[26~"),
        (0x7e, "\u{1b}[28~"),
        (0x7f, "\u{1b}[29~"),
        (0x80, "\u{1b}[31~"),
        (0x81, "\u{1b}[32~"),
        (0x82, "\u{1b}[33~"),
        (0x83, "\u{1b}[34~"),
        (virtual_key::CANCEL, "\u{3}"),
    ];

    for (virtual_key, expected) in cases {
        let mut input = TerminalInput::new();
        assert_eq!(
            input.handle_key(key_down(virtual_key)),
            expected,
            "virtual_key={virtual_key:#x}"
        );
    }
}

#[test]
fn microsoft_terminal_input_modifier_fixed_vt_table_matches_all_fifteen_states() {
    // Exact Data:uiModifierKeystate values from Microsoft's
    // TerminalInputModifierKeyTests. The test intentionally covers only the
    // fModifySequence=true special-key branch; ToUnicodeEx/default/OEM behavior
    // remains Windows/platform evidence.
    let modifier_states = [
        control_state::RIGHT_ALT_PRESSED,
        control_state::LEFT_ALT_PRESSED,
        control_state::RIGHT_ALT_PRESSED | control_state::LEFT_ALT_PRESSED,
        control_state::RIGHT_CTRL_PRESSED,
        control_state::RIGHT_ALT_PRESSED | control_state::RIGHT_CTRL_PRESSED,
        control_state::LEFT_ALT_PRESSED | control_state::RIGHT_CTRL_PRESSED,
        control_state::RIGHT_ALT_PRESSED
            | control_state::LEFT_ALT_PRESSED
            | control_state::RIGHT_CTRL_PRESSED,
        control_state::LEFT_CTRL_PRESSED,
        control_state::LEFT_ALT_PRESSED | control_state::LEFT_CTRL_PRESSED,
        control_state::RIGHT_CTRL_PRESSED | control_state::LEFT_CTRL_PRESSED,
        control_state::LEFT_ALT_PRESSED
            | control_state::RIGHT_CTRL_PRESSED
            | control_state::LEFT_CTRL_PRESSED,
        control_state::SHIFT_PRESSED,
        control_state::SHIFT_PRESSED | control_state::RIGHT_ALT_PRESSED,
        control_state::SHIFT_PRESSED | control_state::LEFT_ALT_PRESSED,
        control_state::SHIFT_PRESSED
            | control_state::RIGHT_ALT_PRESSED
            | control_state::LEFT_ALT_PRESSED,
    ];

    let fixed_sequences = [
        (virtual_key::UP, None, 'A'),
        (virtual_key::DOWN, None, 'B'),
        (virtual_key::RIGHT, None, 'C'),
        (virtual_key::LEFT, None, 'D'),
        (virtual_key::CLEAR, None, 'E'),
        (virtual_key::HOME, None, 'H'),
        (virtual_key::INSERT, Some(2_u16), '~'),
        (virtual_key::DELETE, Some(3_u16), '~'),
        (virtual_key::END, None, 'F'),
        (virtual_key::PRIOR, Some(5_u16), '~'),
        (virtual_key::NEXT, Some(6_u16), '~'),
        (virtual_key::F1, None, 'P'),
        (virtual_key::F1 + 1, None, 'Q'),
        (virtual_key::F1 + 2, None, 'R'),
        (virtual_key::F1 + 3, None, 'S'),
        (virtual_key::F5, Some(15_u16), '~'),
        (virtual_key::F5 + 1, Some(17_u16), '~'),
        (virtual_key::F5 + 2, Some(18_u16), '~'),
        (virtual_key::F5 + 3, Some(19_u16), '~'),
        (virtual_key::F5 + 4, Some(20_u16), '~'),
        (virtual_key::F5 + 5, Some(21_u16), '~'),
        (virtual_key::F11, Some(23_u16), '~'),
        (virtual_key::F12, Some(24_u16), '~'),
        (virtual_key::F13, Some(25_u16), '~'),
        (virtual_key::F13 + 1, Some(26_u16), '~'),
        (virtual_key::F13 + 2, Some(28_u16), '~'),
        (virtual_key::F13 + 3, Some(29_u16), '~'),
        (virtual_key::F13 + 4, Some(31_u16), '~'),
        (virtual_key::F13 + 5, Some(32_u16), '~'),
        (virtual_key::F13 + 6, Some(33_u16), '~'),
        (virtual_key::F20, Some(34_u16), '~'),
    ];

    for control_key_state in modifier_states {
        let shift = u8::from(control_key_state & control_state::SHIFT_PRESSED != 0);
        let alt = u8::from(control_key_state & control_state::ALT_PRESSED != 0);
        let ctrl = u8::from(control_key_state & control_state::CTRL_PRESSED != 0);
        let microsoft_modifier = 1 + shift + (2 * alt) + (4 * ctrl);

        for (virtual_key, number, final_character) in fixed_sequences {
            let expected = format!(
                "\u{1b}[{};{microsoft_modifier}{final_character}",
                number.unwrap_or(1)
            );
            let mut input = TerminalInput::new();
            assert_eq!(
                input.handle_key(modified_key_down(virtual_key, control_key_state)),
                expected,
                "state={control_key_state:#06x}, virtual_key={virtual_key:#04x}"
            );
        }
    }
}

#[test]
fn microsoft_terminal_input_tests_all_key_up_events_are_silent() {
    for virtual_key in 0_u16..u16::from(u8::MAX) {
        let mut input = TerminalInput::new();
        assert_eq!(
            input.handle_key(key_up(virtual_key)),
            "",
            "virtual_key={virtual_key:#x}"
        );
    }
}

#[test]
fn microsoft_test_focus_events_matches_focus_mode_contract() {
    let mut input = TerminalInput::new();

    assert_eq!(input.handle_focus(false), None);
    assert_eq!(input.handle_focus(true), None);

    input.set_input_mode(terminal_input::Mode::FocusEvent, true);

    assert_eq!(input.handle_focus(false), Some("\u{1b}[O".to_string()));
    assert_eq!(input.handle_focus(true), Some("\u{1b}[I".to_string()));
}

#[test]
fn microsoft_send_c1_control_test_matches_eight_and_seven_bit_modes() {
    let mut input = TerminalInput::new();

    input.set_input_mode(terminal_input::Mode::SendC1, true);
    assert_eq!(input.handle_key(key_down(virtual_key::HOME)), "\u{009b}H");
    assert_eq!(input.handle_key(key_down(virtual_key::F1)), "\u{008f}P");

    input.set_input_mode(terminal_input::Mode::SendC1, false);
    assert_eq!(input.handle_key(key_down(virtual_key::HOME)), "\u{1b}[H");
    assert_eq!(input.handle_key(key_down(virtual_key::F1)), "\u{1b}OP");
}
