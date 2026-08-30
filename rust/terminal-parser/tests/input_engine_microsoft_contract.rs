use std::time::Duration;

use terminal_parser::input_engine::{
    DOUBLE_CLICK, FROM_LEFT_1ST_BUTTON_PRESSED, FROM_LEFT_2ND_BUTTON_PRESSED, InputAction,
    InputDispatch, InputRecord, InputStateMachineEngine, LEFT_ALT_PRESSED, LEFT_CTRL_PRESSED,
    MOUSE_HWHEELED, MOUSE_MOVED, MOUSE_WHEELED, MouseEvent, Point, RIGHTMOST_BUTTON_PRESSED,
    SCROLL_DELTA_BACKWARD, SCROLL_DELTA_FORWARD, SHIFT_PRESSED,
};
use terminal_parser::state_machine::{Parameters, StateMachine};

#[derive(Default)]
struct NoopDispatch;

impl InputDispatch for NoopDispatch {
    fn dispatch(&mut self, _action: InputAction) {}
}

#[derive(Default)]
struct RecordingDispatch {
    actions: Vec<InputAction>,
}

impl InputDispatch for RecordingDispatch {
    fn dispatch(&mut self, action: InputAction) {
        self.actions.push(action);
    }
}

fn assert_mouse_table(cases: &[(&str, MouseEvent)]) {
    let mut engine = InputStateMachineEngine::new(RecordingDispatch::default());
    engine.set_double_click_time(Duration::from_secs(1));
    let mut machine = StateMachine::new_input(engine);

    for (sequence, _) in cases {
        machine.process_str(sequence);
    }

    let actual = machine
        .engine()
        .dispatch()
        .actions
        .iter()
        .filter_map(|action| match action {
            InputAction::WriteInput(records) => records.iter().find_map(|record| match record {
                InputRecord::Mouse(mouse) => Some(*mouse),
                InputRecord::Key(_) => None,
            }),
            _ => None,
        })
        .collect::<Vec<_>>();
    let expected = cases.iter().map(|(_, event)| *event).collect::<Vec<_>>();

    assert_eq!(actual, expected);
}

const fn mouse(
    button_state: u32,
    control_key_state: u32,
    x: i32,
    y: i32,
    event_flags: u32,
) -> MouseEvent {
    MouseEvent {
        position: Point { x, y },
        button_state,
        control_key_state,
        event_flags,
    }
}

#[test]
fn microsoft_win32_input_optionals_matrix() {
    // InputEngineTest::TestWin32InputOptionals varies six independent booleans
    // and the number of supplied parameters from 0 through 6. Exercise the
    // complete 64 * 7 Cartesian product deterministically.
    for mask in 0u8..64 {
        let provide_virtual_key = mask & 0b00_0001 != 0;
        let provide_scan_code = mask & 0b00_0010 != 0;
        let provide_char_data = mask & 0b00_0100 != 0;
        let provide_key_down = mask & 0b00_1000 != 0;
        let provide_modifiers = mask & 0b01_0000 != 0;
        let provide_repeat_count = mask & 0b10_0000 != 0;

        let complete = [
            i32::from(provide_virtual_key),
            i32::from(provide_scan_code) * 2,
            i32::from(provide_char_data) * 3,
            i32::from(provide_key_down) * 4,
            i32::from(provide_modifiers) * 5,
            i32::from(provide_repeat_count) * 6,
        ];

        for parameter_count in 0usize..=6 {
            let parameters = Parameters::from_values(
                complete[..parameter_count]
                    .iter()
                    .copied()
                    .map(Some)
                    .collect(),
            );
            let key = InputStateMachineEngine::<NoopDispatch>::generate_win32_key(&parameters);

            assert_eq!(
                key.virtual_key,
                u16::from(provide_virtual_key && parameter_count > 0),
                "mask={mask:#08b}, parameter_count={parameter_count}: virtual key"
            );
            assert_eq!(
                key.scan_code,
                u16::from(provide_scan_code && parameter_count > 1) * 2,
                "mask={mask:#08b}, parameter_count={parameter_count}: scan code"
            );
            assert_eq!(
                key.unicode_char,
                u16::from(provide_char_data && parameter_count > 2) * 3,
                "mask={mask:#08b}, parameter_count={parameter_count}: character"
            );
            assert_eq!(
                key.key_down,
                provide_key_down && parameter_count > 3,
                "mask={mask:#08b}, parameter_count={parameter_count}: key-down"
            );
            assert_eq!(
                key.control_key_state,
                u32::from(provide_modifiers && parameter_count > 4) * 5,
                "mask={mask:#08b}, parameter_count={parameter_count}: modifiers"
            );

            let expected_repeat = if parameter_count == 6 {
                u16::from(provide_repeat_count) * 6
            } else {
                1
            };
            assert_eq!(
                key.repeat_count, expected_repeat,
                "mask={mask:#08b}, parameter_count={parameter_count}: repeat count"
            );
        }
    }
}

#[test]
fn microsoft_sgr_mouse_button_click_table() {
    assert_mouse_table(&[
        (
            "\u{1b}[<0;1;1M",
            mouse(FROM_LEFT_1ST_BUTTON_PRESSED, 0, 0, 0, 0),
        ),
        ("\u{1b}[<0;1;1m", mouse(0, 0, 0, 0, 0)),
        (
            "\u{1b}[<1;1;1M",
            mouse(FROM_LEFT_2ND_BUTTON_PRESSED, 0, 0, 0, 0),
        ),
        ("\u{1b}[<1;1;1m", mouse(0, 0, 0, 0, 0)),
        (
            "\u{1b}[<2;1;1M",
            mouse(RIGHTMOST_BUTTON_PRESSED, 0, 0, 0, 0),
        ),
        ("\u{1b}[<2;1;1m", mouse(0, 0, 0, 0, 0)),
    ]);
}

#[test]
fn microsoft_sgr_mouse_modifier_table() {
    assert_mouse_table(&[
        (
            "\u{1b}[<4;1;1M",
            mouse(FROM_LEFT_1ST_BUTTON_PRESSED, SHIFT_PRESSED, 0, 0, 0),
        ),
        ("\u{1b}[<4;1;1m", mouse(0, SHIFT_PRESSED, 0, 0, 0)),
        (
            "\u{1b}[<9;1;1M",
            mouse(FROM_LEFT_2ND_BUTTON_PRESSED, LEFT_ALT_PRESSED, 0, 0, 0),
        ),
        ("\u{1b}[<9;1;1m", mouse(0, LEFT_ALT_PRESSED, 0, 0, 0)),
        (
            "\u{1b}[<18;1;1M",
            mouse(RIGHTMOST_BUTTON_PRESSED, LEFT_CTRL_PRESSED, 0, 0, 0),
        ),
        ("\u{1b}[<18;1;1m", mouse(0, LEFT_CTRL_PRESSED, 0, 0, 0)),
    ]);
}

#[test]
fn microsoft_sgr_mouse_movement_table() {
    let both = FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED;
    assert_mouse_table(&[
        (
            "\u{1b}[<2;1;1M",
            mouse(RIGHTMOST_BUTTON_PRESSED, 0, 0, 0, 0),
        ),
        (
            "\u{1b}[<34;1;2M",
            mouse(RIGHTMOST_BUTTON_PRESSED, 0, 0, 1, MOUSE_MOVED),
        ),
        (
            "\u{1b}[<34;2;2M",
            mouse(RIGHTMOST_BUTTON_PRESSED, 0, 1, 1, MOUSE_MOVED),
        ),
        ("\u{1b}[<2;2;2m", mouse(0, 0, 1, 1, 0)),
        (
            "\u{1b}[<0;2;2M",
            mouse(FROM_LEFT_1ST_BUTTON_PRESSED, 0, 1, 1, 0),
        ),
        ("\u{1b}[<2;2;2M", mouse(both, 0, 1, 1, 0)),
        ("\u{1b}[<32;2;3M", mouse(both, 0, 1, 2, MOUSE_MOVED)),
        ("\u{1b}[<32;3;3M", mouse(both, 0, 2, 2, MOUSE_MOVED)),
        (
            "\u{1b}[<0;3;3m",
            mouse(RIGHTMOST_BUTTON_PRESSED, 0, 2, 2, 0),
        ),
        ("\u{1b}[<2;3;3m", mouse(0, 0, 2, 2, 0)),
    ]);
}

#[test]
fn microsoft_sgr_mouse_scroll_table() {
    assert_mouse_table(&[
        (
            "\u{1b}[<64;1;1M",
            mouse(SCROLL_DELTA_FORWARD, 0, 0, 0, MOUSE_WHEELED),
        ),
        (
            "\u{1b}[<65;1;1M",
            mouse(SCROLL_DELTA_BACKWARD, 0, 0, 0, MOUSE_WHEELED),
        ),
        (
            "\u{1b}[<66;1;1M",
            mouse(SCROLL_DELTA_BACKWARD, 0, 0, 0, MOUSE_HWHEELED),
        ),
        (
            "\u{1b}[<67;1;1M",
            mouse(SCROLL_DELTA_FORWARD, 0, 0, 0, MOUSE_HWHEELED),
        ),
    ]);
}

#[test]
fn microsoft_sgr_mouse_double_click_table() {
    assert_mouse_table(&[
        (
            "\u{1b}[<0;1;1M",
            mouse(FROM_LEFT_1ST_BUTTON_PRESSED, 0, 0, 0, 0),
        ),
        ("\u{1b}[<0;1;1m", mouse(0, 0, 0, 0, 0)),
        (
            "\u{1b}[<0;1;1M",
            mouse(FROM_LEFT_1ST_BUTTON_PRESSED, 0, 0, 0, DOUBLE_CLICK),
        ),
        ("\u{1b}[<0;1;1m", mouse(0, 0, 0, 0, 0)),
        (
            "\u{1b}[<0;1;1M",
            mouse(FROM_LEFT_1ST_BUTTON_PRESSED, 0, 0, 0, 0),
        ),
        ("\u{1b}[<0;1;1m", mouse(0, 0, 0, 0, 0)),
        (
            "\u{1b}[<1;1;1M",
            mouse(FROM_LEFT_2ND_BUTTON_PRESSED, 0, 0, 0, 0),
        ),
        ("\u{1b}[<1;1;1m", mouse(0, 0, 0, 0, 0)),
        (
            "\u{1b}[<1;1;1M",
            mouse(FROM_LEFT_2ND_BUTTON_PRESSED, 0, 0, 0, DOUBLE_CLICK),
        ),
        ("\u{1b}[<1;1;1m", mouse(0, 0, 0, 0, 0)),
        (
            "\u{1b}[<1;1;1M",
            mouse(FROM_LEFT_2ND_BUTTON_PRESSED, 0, 0, 0, 0),
        ),
        ("\u{1b}[<1;1;1m", mouse(0, 0, 0, 0, 0)),
        (
            "\u{1b}[<2;1;1M",
            mouse(RIGHTMOST_BUTTON_PRESSED, 0, 0, 0, 0),
        ),
        ("\u{1b}[<2;1;1m", mouse(0, 0, 0, 0, 0)),
        (
            "\u{1b}[<2;1;1M",
            mouse(RIGHTMOST_BUTTON_PRESSED, 0, 0, 0, DOUBLE_CLICK),
        ),
        ("\u{1b}[<2;1;1m", mouse(0, 0, 0, 0, 0)),
        (
            "\u{1b}[<2;1;1M",
            mouse(RIGHTMOST_BUTTON_PRESSED, 0, 0, 0, 0),
        ),
        ("\u{1b}[<2;1;1m", mouse(0, 0, 0, 0, 0)),
    ]);
}

#[test]
fn microsoft_sgr_mouse_hover_table() {
    assert_mouse_table(&[
        ("\u{1b}[<35;1;1m", mouse(0, 0, 0, 0, MOUSE_MOVED)),
        ("\u{1b}[<35;2;2m", mouse(0, 0, 1, 1, MOUSE_MOVED)),
    ]);
}
