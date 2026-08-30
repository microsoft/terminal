use terminal_host::api_routines::{
    ConsoleInputModeState, ENABLE_AUTO_POSITION, ENABLE_ECHO_INPUT, ENABLE_EXTENDED_FLAGS,
    ENABLE_INSERT_MODE, ENABLE_LINE_INPUT, ENABLE_PROCESSED_INPUT, ENABLE_QUICK_EDIT_MODE,
    InputModeStatus,
};

#[test]
fn microsoft_api_set_console_input_mode_impl_valid_non_extended_contract() {
    let mut state = ConsoleInputModeState::from_mode(0);
    let requested = ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT;

    assert_eq!(
        state.set_console_input_mode(requested),
        InputModeStatus::Success
    );
    assert_eq!(state.input_mode(), requested);
    assert!(!state.quick_edit_mode());
    assert!(!state.auto_position());
    assert!(!state.insert_mode());
    assert!(state.cursor_double_mode());
}

#[test]
fn microsoft_api_set_console_input_mode_impl_valid_extended_contract() {
    let mut state = ConsoleInputModeState::from_mode(0);
    let requested = ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE | ENABLE_AUTO_POSITION;

    assert_eq!(
        state.set_console_input_mode(requested),
        InputModeStatus::Success
    );
    assert_eq!(state.input_mode(), 0);
    assert!(state.quick_edit_mode());
    assert!(state.auto_position());
    assert!(!state.insert_mode());
    assert!(state.cursor_double_mode());
}

#[test]
fn microsoft_api_set_console_input_mode_impl_extended_turn_off_contract() {
    let original = ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE | ENABLE_AUTO_POSITION;
    let mut state = ConsoleInputModeState::from_mode(original);

    assert_eq!(
        state.set_console_input_mode(ENABLE_EXTENDED_FLAGS),
        InputModeStatus::Success
    );
    assert_eq!(state.input_mode(), 0);
    assert!(!state.quick_edit_mode());
    assert!(!state.auto_position());
    assert!(!state.insert_mode());
    assert!(state.cursor_double_mode());
}

#[test]
fn microsoft_api_set_console_input_mode_impl_invalid_contract() {
    let mut state = ConsoleInputModeState::from_mode(0);
    let invalid_mode = 0x0800_0000;

    assert_eq!(
        state.set_console_input_mode(invalid_mode),
        InputModeStatus::InvalidArgument
    );
    assert_eq!(state.input_mode(), invalid_mode);
    assert!(!state.quick_edit_mode());
    assert!(!state.auto_position());
    assert!(!state.insert_mode());
}

#[test]
fn microsoft_api_set_console_input_mode_impl_insert_no_cooked_read_contract() {
    let mut enabled = ConsoleInputModeState::from_mode(0);
    assert_eq!(
        enabled.set_console_input_mode(ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE),
        InputModeStatus::Success
    );
    assert!(enabled.insert_mode());
    assert!(!enabled.cursor_double_mode());
    assert_eq!(enabled.cooked_read_insert_mode(), None);

    let mut disabled = ConsoleInputModeState::from_mode(0);
    assert_eq!(
        disabled.set_console_input_mode(ENABLE_EXTENDED_FLAGS),
        InputModeStatus::Success
    );
    assert!(!disabled.insert_mode());
    assert!(disabled.cursor_double_mode());
}

#[test]
fn microsoft_api_set_console_input_mode_impl_insert_cooked_read_contract() {
    let mut enabled = ConsoleInputModeState::from_mode(0);
    enabled.begin_cooked_read();
    assert_eq!(
        enabled.set_console_input_mode(ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE),
        InputModeStatus::Success
    );
    assert!(enabled.insert_mode());
    assert!(!enabled.cursor_double_mode());
    assert_eq!(enabled.cooked_read_insert_mode(), Some(true));

    let mut disabled = ConsoleInputModeState::from_mode(0);
    disabled.begin_cooked_read();
    assert_eq!(
        disabled.set_console_input_mode(ENABLE_EXTENDED_FLAGS),
        InputModeStatus::Success
    );
    assert!(!disabled.insert_mode());
    assert!(disabled.cursor_double_mode());
    assert_eq!(disabled.cooked_read_insert_mode(), Some(false));
}

#[test]
fn microsoft_api_set_console_input_mode_impl_echo_on_line_off_contract() {
    let mut state = ConsoleInputModeState::from_mode(0);

    assert_eq!(
        state.set_console_input_mode(ENABLE_ECHO_INPUT),
        InputModeStatus::InvalidArgument
    );
    assert_eq!(state.input_mode(), ENABLE_ECHO_INPUT);
}

#[test]
fn microsoft_api_set_console_input_mode_extended_flag_behaviors_contract() {
    for flag in [
        ENABLE_INSERT_MODE,
        ENABLE_QUICK_EDIT_MODE,
        ENABLE_AUTO_POSITION,
    ] {
        let mut state = ConsoleInputModeState::from_mode(0);
        assert_eq!(state.set_console_input_mode(flag), InputModeStatus::Success);
        assert_eq!(state.input_mode(), 0);
        assert_eq!(state.insert_mode(), flag == ENABLE_INSERT_MODE);
        assert_eq!(state.quick_edit_mode(), flag == ENABLE_QUICK_EDIT_MODE);
        assert_eq!(state.auto_position(), flag == ENABLE_AUTO_POSITION);
    }

    let original = ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE | ENABLE_AUTO_POSITION;
    let mut preserved = ConsoleInputModeState::from_mode(original);
    assert_eq!(
        preserved.set_console_input_mode(0),
        InputModeStatus::Success
    );
    assert!(preserved.insert_mode());
    assert!(preserved.quick_edit_mode());
    assert!(preserved.auto_position());
}

#[test]
fn microsoft_api_set_console_input_mode_impl_ps_readline_scenario_contract() {
    let mut state = ConsoleInputModeState::from_mode(0x01f7);

    assert_eq!(
        state.set_console_input_mode(0x01e4),
        InputModeStatus::InvalidArgument
    );
    assert_eq!(state.input_mode(), ENABLE_ECHO_INPUT);
    assert!(state.insert_mode());
    assert!(state.quick_edit_mode());
    assert!(state.auto_position());
    assert!(state.cursor_double_mode());
}
