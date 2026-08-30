use terminal_input::{KeyEvent, Mode, TerminalInput, control_state, virtual_key};

fn key(virtual_key: u16, control_key_state: u32, codepoint: u32) -> KeyEvent {
    KeyEvent {
        virtual_key,
        scan_code: 0,
        codepoint,
        control_key_state,
        key_down: true,
        repeat_count: 1,
    }
}

fn key_up(virtual_key: u16, codepoint: u32) -> KeyEvent {
    KeyEvent {
        key_down: false,
        ..key(virtual_key, 0, codepoint)
    }
}

fn assert_modifier_cases(cases: &[(u16, u32, u32, &str)]) {
    for &(virtual_key, state, codepoint, expected) in cases {
        let mut input = TerminalInput::new();
        assert_eq!(
            input.handle_key(key(virtual_key, state, codepoint)),
            expected,
            "virtual_key={virtual_key:#x}, state={state:#x}"
        );
    }
}

#[test]
fn microsoft_terminal_input_focus_events_match_disabled_and_enabled_contract() {
    let mut input = TerminalInput::new();
    assert_eq!(input.handle_focus(false), None);
    assert_eq!(input.handle_focus(true), None);

    input.set_input_mode(Mode::FocusEvent, true);
    assert_eq!(input.handle_focus(false).as_deref(), Some("\u{1b}[O"));
    assert_eq!(input.handle_focus(true).as_deref(), Some("\u{1b}[I"));
}

#[test]
fn microsoft_terminal_input_null_key_portable_subset_matches_ctrl_space_contract() {
    let mut input = TerminalInput::new();
    assert_eq!(
        input.handle_key(key(virtual_key::SPACE, control_state::LEFT_CTRL_PRESSED, 0,)),
        "\0"
    );

    let mut input = TerminalInput::new();
    assert_eq!(
        input.handle_key(key(
            virtual_key::SPACE,
            control_state::LEFT_CTRL_PRESSED | control_state::LEFT_ALT_PRESSED,
            0,
        )),
        "\u{1b}\0"
    );

    let mut input = TerminalInput::new();
    assert_eq!(
        input.handle_key(key(
            virtual_key::SPACE,
            control_state::RIGHT_CTRL_PRESSED | control_state::LEFT_ALT_PRESSED,
            0,
        )),
        "\u{1b}\0"
    );
}

#[test]
fn microsoft_terminal_input_different_modifiers_backspace_delete_and_tab() {
    assert_modifier_cases(&[
        (virtual_key::BACK, 0, 0, "\u{7f}"),
        (
            virtual_key::BACK,
            control_state::LEFT_CTRL_PRESSED,
            0x08,
            "\u{8}",
        ),
        (
            virtual_key::BACK,
            control_state::RIGHT_CTRL_PRESSED,
            0x08,
            "\u{8}",
        ),
        (
            virtual_key::BACK,
            control_state::LEFT_ALT_PRESSED,
            0x08,
            "\u{1b}\u{7f}",
        ),
        (
            virtual_key::BACK,
            control_state::RIGHT_ALT_PRESSED,
            0x08,
            "\u{1b}\u{7f}",
        ),
        (
            virtual_key::DELETE,
            control_state::LEFT_CTRL_PRESSED,
            0,
            "\u{1b}[3;5~",
        ),
        (
            virtual_key::DELETE,
            control_state::RIGHT_CTRL_PRESSED,
            0,
            "\u{1b}[3;5~",
        ),
        (
            virtual_key::DELETE,
            control_state::LEFT_ALT_PRESSED,
            0,
            "\u{1b}[3;3~",
        ),
        (
            virtual_key::DELETE,
            control_state::RIGHT_ALT_PRESSED,
            0,
            "\u{1b}[3;3~",
        ),
        (virtual_key::TAB, control_state::LEFT_CTRL_PRESSED, 0, "\t"),
        (virtual_key::TAB, control_state::RIGHT_CTRL_PRESSED, 0, "\t"),
        (
            virtual_key::TAB,
            control_state::SHIFT_PRESSED,
            0,
            "\u{1b}[Z",
        ),
    ]);
}

#[test]
fn microsoft_terminal_input_different_modifiers_slash_and_question() {
    assert_modifier_cases(&[
        (
            u16::from(b'/'),
            control_state::LEFT_CTRL_PRESSED,
            0,
            "\u{1f}",
        ),
        (
            u16::from(b'/'),
            control_state::RIGHT_CTRL_PRESSED,
            0,
            "\u{1f}",
        ),
        (
            u16::from(b'/'),
            control_state::LEFT_ALT_PRESSED,
            u32::from(b'/'),
            "\u{1b}/",
        ),
        (
            u16::from(b'/'),
            control_state::RIGHT_ALT_PRESSED,
            u32::from(b'/'),
            "\u{1b}/",
        ),
        (
            u16::from(b'?'),
            control_state::SHIFT_PRESSED | control_state::LEFT_CTRL_PRESSED,
            0,
            "\u{7f}",
        ),
        (
            u16::from(b'?'),
            control_state::SHIFT_PRESSED | control_state::RIGHT_CTRL_PRESSED,
            0,
            "\u{7f}",
        ),
        (
            u16::from(b'/'),
            control_state::LEFT_CTRL_PRESSED | control_state::LEFT_ALT_PRESSED,
            0,
            "\u{1b}\u{1f}",
        ),
        (
            u16::from(b'/'),
            control_state::RIGHT_CTRL_PRESSED | control_state::LEFT_ALT_PRESSED,
            0,
            "\u{1b}\u{1f}",
        ),
        (
            u16::from(b'/'),
            control_state::RIGHT_CTRL_PRESSED | control_state::RIGHT_ALT_PRESSED,
            0,
            "\u{1b}\u{1f}",
        ),
        (
            u16::from(b'?'),
            control_state::SHIFT_PRESSED
                | control_state::LEFT_CTRL_PRESSED
                | control_state::LEFT_ALT_PRESSED,
            0,
            "\u{1b}\u{7f}",
        ),
        (
            u16::from(b'?'),
            control_state::SHIFT_PRESSED
                | control_state::RIGHT_CTRL_PRESSED
                | control_state::LEFT_ALT_PRESSED,
            0,
            "\u{1b}\u{7f}",
        ),
        (
            u16::from(b'?'),
            control_state::SHIFT_PRESSED
                | control_state::RIGHT_CTRL_PRESSED
                | control_state::RIGHT_ALT_PRESSED,
            0,
            "\u{1b}\u{7f}",
        ),
    ]);
}

#[test]
fn microsoft_terminal_input_ctrl_num_contract_matches_one_through_nine() {
    let cases = [
        (b'1', "1"),
        (b'3', "\u{1b}"),
        (b'4', "\u{1c}"),
        (b'5', "\u{1d}"),
        (b'6', "\u{1e}"),
        (b'7', "\u{1f}"),
        (b'8', "\u{7f}"),
        (b'9', "9"),
    ];

    for (digit, expected) in cases {
        let mut input = TerminalInput::new();
        assert_eq!(
            input.handle_key(key(u16::from(digit), control_state::LEFT_CTRL_PRESSED, 0,)),
            expected,
            "digit={} ",
            char::from(digit)
        );
    }
}

#[test]
fn microsoft_terminal_input_backarrow_mode_matches_all_sixteen_combinations() {
    let combinations = [
        (0, "\u{8}", "\u{7f}"),
        (control_state::SHIFT_PRESSED, "\u{8}", "\u{7f}"),
        (control_state::LEFT_CTRL_PRESSED, "\u{7f}", "\u{8}"),
        (
            control_state::LEFT_CTRL_PRESSED | control_state::SHIFT_PRESSED,
            "\u{7f}",
            "\u{8}",
        ),
        (
            control_state::LEFT_ALT_PRESSED,
            "\u{1b}\u{8}",
            "\u{1b}\u{7f}",
        ),
        (
            control_state::LEFT_ALT_PRESSED | control_state::SHIFT_PRESSED,
            "\u{1b}\u{8}",
            "\u{1b}\u{7f}",
        ),
        (
            control_state::LEFT_ALT_PRESSED | control_state::LEFT_CTRL_PRESSED,
            "\u{1b}\u{7f}",
            "\u{1b}\u{8}",
        ),
        (
            control_state::LEFT_ALT_PRESSED
                | control_state::LEFT_CTRL_PRESSED
                | control_state::SHIFT_PRESSED,
            "\u{1b}\u{7f}",
            "\u{1b}\u{8}",
        ),
    ];

    for (state, enabled_expected, disabled_expected) in combinations {
        let mut enabled = TerminalInput::new();
        enabled.set_input_mode(Mode::BackarrowKey, true);
        assert_eq!(
            enabled.handle_key(key(virtual_key::BACK, state, 0)),
            enabled_expected,
            "enabled state={state:#x}"
        );

        let mut disabled = TerminalInput::new();
        disabled.set_input_mode(Mode::BackarrowKey, false);
        assert_eq!(
            disabled.handle_key(key(virtual_key::BACK, state, 0)),
            disabled_expected,
            "disabled state={state:#x}"
        );
    }
}

#[test]
fn microsoft_terminal_input_auto_repeat_mode_matches_three_downs_then_release() {
    let down = key(u16::from(b'A'), 0, u32::from(b'A'));
    let up = key_up(u16::from(b'A'), u32::from(b'A'));

    let mut input = TerminalInput::new();
    input.set_input_mode(Mode::AutoRepeat, false);
    assert_eq!(input.handle_key(down), "A");
    assert_eq!(input.handle_key(down), "");
    assert_eq!(input.handle_key(down), "");
    assert_eq!(input.handle_key(up), "");

    input.set_input_mode(Mode::AutoRepeat, true);
    assert_eq!(input.handle_key(down), "A");
    assert_eq!(input.handle_key(down), "A");
    assert_eq!(input.handle_key(down), "A");
    assert_eq!(input.handle_key(up), "");
}

#[test]
fn microsoft_terminal_input_send_c1_control_switches_home_and_f1_prefixes() {
    let mut input = TerminalInput::new();
    input.set_input_mode(Mode::SendC1, true);
    assert_eq!(input.handle_key(key(virtual_key::HOME, 0, 0)), "\u{009b}H");
    assert_eq!(input.handle_key(key(virtual_key::F1, 0, 0)), "\u{008f}P");

    input.set_input_mode(Mode::SendC1, false);
    assert_eq!(input.handle_key(key(virtual_key::HOME, 0, 0)), "\u{1b}[H");
    assert_eq!(input.handle_key(key(virtual_key::F1, 0, 0)), "\u{1b}OP");
}
