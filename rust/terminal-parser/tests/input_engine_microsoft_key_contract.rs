use terminal_parser::input_engine::{
    ENHANCED_KEY, InputAction, InputDispatch, InputRecord, InputStateMachineEngine, KeyEvent,
    LEFT_ALT_PRESSED, LEFT_CTRL_PRESSED, SHIFT_PRESSED,
};
use terminal_parser::state_machine::{State, StateMachine};

const VK_BACK: u16 = 0x08;
const VK_TAB: u16 = 0x09;
const VK_RETURN: u16 = 0x0d;
const VK_SHIFT: u16 = 0x10;
const VK_CONTROL: u16 = 0x11;
const VK_MENU: u16 = 0x12;
const VK_PRIOR: u16 = 0x21;
const VK_NEXT: u16 = 0x22;
const VK_END: u16 = 0x23;
const VK_HOME: u16 = 0x24;
const VK_LEFT: u16 = 0x25;
const VK_UP: u16 = 0x26;
const VK_RIGHT: u16 = 0x27;
const VK_DOWN: u16 = 0x28;
const VK_INSERT: u16 = 0x2d;
const VK_DELETE: u16 = 0x2e;
const VK_F3: u16 = 0x72;
const VK_OEM_2: u16 = 0xbf;

#[derive(Default)]
struct RecordingDispatch {
    actions: Vec<InputAction>,
}

impl InputDispatch for RecordingDispatch {
    fn dispatch(&mut self, action: InputAction) {
        self.actions.push(action);
    }
}

fn machine() -> StateMachine<InputStateMachineEngine<RecordingDispatch>> {
    StateMachine::new_input(InputStateMachineEngine::new(RecordingDispatch::default()))
}

fn primary_key_downs(
    machine: &StateMachine<InputStateMachineEngine<RecordingDispatch>>,
) -> Vec<KeyEvent> {
    let mut keys = Vec::new();
    for action in &machine.engine().dispatch().actions {
        if let InputAction::WriteInput(records) = action {
            for record in records {
                if let InputRecord::Key(key) = record
                    && key.key_down
                    && !matches!(key.virtual_key, VK_SHIFT | VK_CONTROL | VK_MENU)
                {
                    keys.push(*key);
                }
            }
        }
    }
    keys
}

fn parse_single_key(sequence: &str) -> KeyEvent {
    let mut machine = machine();
    machine.process_str(sequence);
    let keys = primary_key_downs(&machine);
    assert_eq!(keys.len(), 1, "sequence {sequence:?}");
    keys[0]
}

fn assert_key(key: KeyEvent, virtual_key: u16, unicode_char: u16, control_key_state: u32) {
    assert!(key.key_down);
    assert_eq!(key.repeat_count, 1);
    assert_eq!(key.virtual_key, virtual_key);
    assert_eq!(key.unicode_char, unicode_char);
    assert_eq!(key.control_key_state, control_key_state);
}

#[test]
fn microsoft_cursor_positioning_consumes_once_then_reverts_to_f3() {
    let engine = InputStateMachineEngine::new(RecordingDispatch::default());
    engine.capture_next_cursor_position_report();
    let mut machine = StateMachine::new_input(engine);

    machine.process_str("\u{1b}[1;4R");
    assert!(matches!(
        machine.engine().dispatch().actions.as_slice(),
        [InputAction::MoveCursor { row: 1, column: 4 }]
    ));

    machine.process_str("\u{1b}[1;4R");
    let keys = primary_key_downs(&machine);
    assert_eq!(keys.len(), 1);
    assert_key(keys[0], VK_F3, 0, LEFT_ALT_PRESSED | SHIFT_PRESSED);
}

#[test]
fn microsoft_csi_cursor_backtab_matches_shift_tab() {
    assert_key(
        parse_single_key("\u{1b}[Z"),
        VK_TAB,
        u16::from(b'\t'),
        SHIFT_PRESSED,
    );
}

#[test]
fn microsoft_enhanced_keys_table_matches_all_ten_sequences() {
    for (virtual_key, sequence) in [
        (VK_PRIOR, "\u{1b}[5~"),
        (VK_NEXT, "\u{1b}[6~"),
        (VK_END, "\u{1b}[F"),
        (VK_HOME, "\u{1b}[H"),
        (VK_LEFT, "\u{1b}[D"),
        (VK_UP, "\u{1b}[A"),
        (VK_RIGHT, "\u{1b}[C"),
        (VK_DOWN, "\u{1b}[B"),
        (VK_INSERT, "\u{1b}[2~"),
        (VK_DELETE, "\u{1b}[3~"),
    ] {
        assert_key(parse_single_key(sequence), virtual_key, 0, ENHANCED_KEY);
    }
}

#[test]
fn microsoft_ss3_cursor_key_table_matches_all_six_sequences() {
    for (virtual_key, sequence) in [
        (VK_UP, "\u{1b}OA"),
        (VK_DOWN, "\u{1b}OB"),
        (VK_RIGHT, "\u{1b}OC"),
        (VK_LEFT, "\u{1b}OD"),
        (VK_HOME, "\u{1b}OH"),
        (VK_END, "\u{1b}OF"),
    ] {
        assert_key(parse_single_key(sequence), virtual_key, 0, 0);
    }
}

#[test]
fn microsoft_alt_backspace_matches_escape_delete() {
    assert_key(
        parse_single_key("\u{1b}\u{7f}"),
        VK_BACK,
        0x08,
        LEFT_ALT_PRESSED,
    );
}

#[test]
fn microsoft_alt_ctrl_d_matches_escape_eot() {
    assert_key(
        parse_single_key("\u{1b}\u{4}"),
        u16::from(b'D'),
        0x04,
        LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED,
    );
}

#[test]
fn microsoft_ctrl_alt_z_and_x_execute_from_escape_in_input_mode() {
    for (sequence, virtual_key, unicode_char) in [
        ("\u{1b}\u{1a}", u16::from(b'Z'), 0x1a),
        ("\u{1b}\u{18}", u16::from(b'X'), 0x18),
    ] {
        assert_key(
            parse_single_key(sequence),
            virtual_key,
            unicode_char,
            LEFT_ALT_PRESSED | LEFT_CTRL_PRESSED,
        );
    }
}

#[test]
fn microsoft_alt_backspace_then_enter_returns_to_ground_between_keys() {
    let mut machine = machine();

    machine.process_str("\u{1b}\u{7f}");
    assert_eq!(machine.state(), State::Ground);
    machine.process_str("\r");
    assert_eq!(machine.state(), State::Ground);

    let keys = primary_key_downs(&machine);
    assert_eq!(keys.len(), 2);
    assert_key(keys[0], VK_BACK, 0x08, LEFT_ALT_PRESSED);
    assert_key(keys[1], VK_RETURN, u16::from(b'\r'), 0);
}

#[test]
fn microsoft_alt_intermediate_parser_half_preserves_alt_slash_then_ctrl_e() {
    let mut machine = machine();

    machine.process_str("\u{1b}/");
    machine.process_str("\u{5}");

    let keys = primary_key_downs(&machine);
    assert_eq!(keys.len(), 2);
    assert_key(keys[0], VK_OEM_2, u16::from(b'/'), LEFT_ALT_PRESSED);
    assert_key(keys[1], u16::from(b'E'), 0x05, LEFT_CTRL_PRESSED);
}
