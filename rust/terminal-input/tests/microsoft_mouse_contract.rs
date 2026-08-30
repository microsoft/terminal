use terminal_input::{Mode, MouseButtonState, MouseMessage, Point, TerminalInput, control_state};

const TEST_COORDS: [Point; 11] = [
    Point { x: 0, y: 0 },
    Point { x: 0, y: 1 },
    Point { x: 1, y: 1 },
    Point { x: 2, y: 2 },
    Point { x: 94, y: 94 },
    Point { x: 95, y: 95 },
    Point { x: 96, y: 96 },
    Point { x: 127, y: 127 },
    Point { x: 128, y: 128 },
    Point {
        x: i16::MAX as i32 - 33,
        y: i16::MAX as i32 - 33,
    },
    Point {
        x: i16::MAX as i32 - 32,
        y: i16::MAX as i32 - 32,
    },
];

const BUTTON_MESSAGES: [MouseMessage; 6] = [
    MouseMessage::LeftDown,
    MouseMessage::LeftUp,
    MouseMessage::MiddleDown,
    MouseMessage::MiddleUp,
    MouseMessage::RightDown,
    MouseMessage::RightUp,
];

const MODIFIERS: [u32; 5] = [
    0,
    control_state::SHIFT_PRESSED,
    control_state::LEFT_CTRL_PRESSED,
    control_state::RIGHT_ALT_PRESSED,
    control_state::RIGHT_ALT_PRESSED | control_state::LEFT_CTRL_PRESSED,
];

fn modifier_bits(state: u32) -> u32 {
    let mut result = 0;
    if state & control_state::SHIFT_PRESSED != 0 {
        result |= 0x04;
    }
    if state & control_state::ALT_PRESSED != 0 {
        result |= 0x08;
    }
    if state & control_state::CTRL_PRESSED != 0 {
        result |= 0x10;
    }
    result
}

fn x10_button(message: MouseMessage, modifiers: u32, delta: i16) -> u32 {
    let base = match message {
        MouseMessage::LeftDown | MouseMessage::LeftDoubleClick | MouseMessage::Move => 0,
        MouseMessage::MiddleDown | MouseMessage::MiddleDoubleClick => 1,
        MouseMessage::RightDown | MouseMessage::RightDoubleClick => 2,
        MouseMessage::LeftUp | MouseMessage::MiddleUp | MouseMessage::RightUp => 3,
        MouseMessage::Wheel => {
            if delta > 0 {
                64
            } else {
                65
            }
        }
        MouseMessage::HorizontalWheel => {
            if delta > 0 {
                67
            } else {
                66
            }
        }
    };
    base | modifier_bits(modifiers)
}

fn sgr_button(message: MouseMessage, modifiers: u32, delta: i16, hover: bool) -> u32 {
    let base = match message {
        MouseMessage::LeftDown | MouseMessage::LeftUp | MouseMessage::LeftDoubleClick => 0,
        MouseMessage::MiddleDown | MouseMessage::MiddleUp | MouseMessage::MiddleDoubleClick => 1,
        MouseMessage::RightDown | MouseMessage::RightUp | MouseMessage::RightDoubleClick => 2,
        MouseMessage::Move => 3,
        MouseMessage::Wheel => {
            if delta > 0 {
                64
            } else {
                65
            }
        }
        MouseMessage::HorizontalWheel => {
            if delta > 0 {
                67
            } else {
                66
            }
        }
    };
    base | modifier_bits(modifiers) | if hover { 0x20 } else { 0 }
}

fn x10_expected(message: MouseMessage, modifiers: u32, delta: i16, point: Point) -> String {
    let button = char::from_u32(0x20 + x10_button(message, modifiers, delta)).unwrap();
    let x = char::from_u32(u32::try_from(point.x + 33).unwrap()).unwrap();
    let y = char::from_u32(u32::try_from(point.y + 33).unwrap()).unwrap();
    format!("\u{1b}[M{button}{x}{y}")
}

fn sgr_expected(
    message: MouseMessage,
    modifiers: u32,
    delta: i16,
    point: Point,
    hover: bool,
) -> String {
    let button = sgr_button(message, modifiers, delta, hover);
    let final_character = if matches!(
        message,
        MouseMessage::LeftUp | MouseMessage::MiddleUp | MouseMessage::RightUp
    ) {
        'm'
    } else {
        'M'
    };
    format!(
        "\u{1b}[<{button};{};{}{final_character}",
        point.x + 1,
        point.y + 1
    )
}

fn tracking_modes() -> [Mode; 3] {
    [
        Mode::DefaultMouseTracking,
        Mode::ButtonEventMouseTracking,
        Mode::AnyEventMouseTracking,
    ]
}

#[test]
fn microsoft_mouse_default_mode_tests_match_full_button_modifier_coordinate_matrix() {
    for message in BUTTON_MESSAGES {
        for modifiers in MODIFIERS {
            for mode in tracking_modes() {
                let mut input = TerminalInput::new();
                assert_eq!(
                    input.handle_mouse(
                        Point::default(),
                        message,
                        modifiers,
                        0,
                        MouseButtonState::default(),
                    ),
                    None
                );
                input.set_input_mode(mode, true);

                for point in TEST_COORDS {
                    let expected = (point.x <= 94 && point.y <= 94)
                        .then(|| x10_expected(message, modifiers, 0, point));
                    assert_eq!(
                        input.handle_mouse(
                            point,
                            message,
                            modifiers,
                            0,
                            MouseButtonState::default(),
                        ),
                        expected,
                        "mode={mode:?}, message={message:?}, modifiers={modifiers:#x}, point={point:?}"
                    );
                }
            }
        }
    }
}

#[test]
fn microsoft_mouse_utf8_mode_tests_match_full_button_modifier_coordinate_matrix() {
    let max_coordinate = i32::from(i16::MAX) - 33;
    for message in BUTTON_MESSAGES {
        for modifiers in MODIFIERS {
            for mode in tracking_modes() {
                let mut input = TerminalInput::new();
                assert_eq!(
                    input.handle_mouse(
                        Point::default(),
                        message,
                        modifiers,
                        0,
                        MouseButtonState::default(),
                    ),
                    None
                );
                input.set_input_mode(Mode::Utf8MouseEncoding, true);
                input.set_input_mode(mode, true);

                for point in TEST_COORDS {
                    let expected = (point.x <= max_coordinate && point.y <= max_coordinate)
                        .then(|| x10_expected(message, modifiers, 0, point));
                    assert_eq!(
                        input.handle_mouse(
                            point,
                            message,
                            modifiers,
                            0,
                            MouseButtonState::default(),
                        ),
                        expected,
                        "mode={mode:?}, message={message:?}, modifiers={modifiers:#x}, point={point:?}"
                    );
                }
            }
        }
    }
}

#[test]
fn microsoft_mouse_sgr_mode_tests_match_tracking_button_modifier_coordinate_matrix() {
    let messages = [
        MouseMessage::LeftDown,
        MouseMessage::LeftUp,
        MouseMessage::MiddleDown,
        MouseMessage::MiddleUp,
        MouseMessage::RightDown,
        MouseMessage::RightUp,
        MouseMessage::Move,
    ];

    for message in messages {
        for modifiers in MODIFIERS {
            for mode in tracking_modes() {
                let mut input = TerminalInput::new();
                assert_eq!(
                    input.handle_mouse(
                        Point::default(),
                        message,
                        modifiers,
                        0,
                        MouseButtonState::default(),
                    ),
                    None
                );
                input.set_input_mode(Mode::SgrMouseEncoding, true);
                input.set_input_mode(mode, true);

                for point in TEST_COORDS {
                    let should_emit =
                        message != MouseMessage::Move || mode == Mode::AnyEventMouseTracking;
                    let expected = should_emit.then(|| {
                        sgr_expected(message, modifiers, 0, point, message == MouseMessage::Move)
                    });
                    assert_eq!(
                        input.handle_mouse(
                            point,
                            message,
                            modifiers,
                            0,
                            MouseButtonState::default(),
                        ),
                        expected,
                        "mode={mode:?}, message={message:?}, modifiers={modifiers:#x}, point={point:?}"
                    );
                }
            }
        }
    }
}

#[test]
fn microsoft_mouse_scroll_wheel_tests_match_all_recorded_deltas_modifiers_and_encodings() {
    let deltas = [-120_i16, 120_i16, -10_000_i16, 32_736_i16];
    for delta in deltas {
        for modifiers in MODIFIERS {
            let mut default_input = TerminalInput::new();
            assert_eq!(
                default_input.handle_mouse(
                    Point::default(),
                    MouseMessage::Wheel,
                    modifiers,
                    delta,
                    MouseButtonState::default(),
                ),
                None
            );
            default_input.set_input_mode(Mode::DefaultMouseTracking, true);
            for point in TEST_COORDS {
                let expected = (point.x <= 94 && point.y <= 94)
                    .then(|| x10_expected(MouseMessage::Wheel, modifiers, delta, point));
                assert_eq!(
                    default_input.handle_mouse(
                        point,
                        MouseMessage::Wheel,
                        modifiers,
                        delta,
                        MouseButtonState::default(),
                    ),
                    expected,
                    "default delta={delta}, modifiers={modifiers:#x}, point={point:?}"
                );
            }

            let mut utf8_input = TerminalInput::new();
            utf8_input.set_input_mode(Mode::Utf8MouseEncoding, true);
            utf8_input.set_input_mode(Mode::DefaultMouseTracking, true);
            let max_coordinate = i32::from(i16::MAX) - 33;
            for point in TEST_COORDS {
                let expected = (point.x <= max_coordinate && point.y <= max_coordinate)
                    .then(|| x10_expected(MouseMessage::Wheel, modifiers, delta, point));
                assert_eq!(
                    utf8_input.handle_mouse(
                        point,
                        MouseMessage::Wheel,
                        modifiers,
                        delta,
                        MouseButtonState::default(),
                    ),
                    expected,
                    "utf8 delta={delta}, modifiers={modifiers:#x}, point={point:?}"
                );
            }

            let mut sgr_input = TerminalInput::new();
            sgr_input.set_input_mode(Mode::SgrMouseEncoding, true);
            sgr_input.set_input_mode(Mode::DefaultMouseTracking, true);
            for point in TEST_COORDS {
                assert_eq!(
                    sgr_input.handle_mouse(
                        point,
                        MouseMessage::Wheel,
                        modifiers,
                        delta,
                        MouseButtonState::default(),
                    ),
                    Some(sgr_expected(
                        MouseMessage::Wheel,
                        modifiers,
                        delta,
                        point,
                        false,
                    )),
                    "sgr delta={delta}, modifiers={modifiers:#x}, point={point:?}"
                );
            }
        }
    }
}

#[test]
fn microsoft_mouse_alternate_scroll_mode_tests_match_buffer_mode_and_direction_contract() {
    let mut input = TerminalInput::new();
    input.use_alternate_screen_buffer();
    input.set_input_mode(Mode::AlternateScroll, true);

    for (message, delta, expected) in [
        (MouseMessage::Wheel, 120, "\u{1b}[A"),
        (MouseMessage::Wheel, -120, "\u{1b}[B"),
        (MouseMessage::HorizontalWheel, 120, "\u{1b}[C"),
        (MouseMessage::HorizontalWheel, -120, "\u{1b}[D"),
    ] {
        assert_eq!(
            input.handle_mouse(
                Point::default(),
                message,
                0,
                delta,
                MouseButtonState::default(),
            ),
            Some(expected.to_string())
        );
    }

    input.set_input_mode(Mode::CursorKey, true);
    for (message, delta, expected) in [
        (MouseMessage::Wheel, 120, "\u{1b}OA"),
        (MouseMessage::Wheel, -120, "\u{1b}OB"),
        (MouseMessage::HorizontalWheel, 120, "\u{1b}OC"),
        (MouseMessage::HorizontalWheel, -120, "\u{1b}OD"),
    ] {
        assert_eq!(
            input.handle_mouse(
                Point::default(),
                message,
                0,
                delta,
                MouseButtonState::default(),
            ),
            Some(expected.to_string())
        );
    }

    input.set_input_mode(Mode::AlternateScroll, false);
    assert_eq!(
        input.handle_mouse(
            Point::default(),
            MouseMessage::Wheel,
            0,
            120,
            MouseButtonState::default(),
        ),
        None
    );

    input.use_main_screen_buffer();
    input.set_input_mode(Mode::AlternateScroll, true);
    assert_eq!(
        input.handle_mouse(
            Point::default(),
            MouseMessage::Wheel,
            0,
            120,
            MouseButtonState::default(),
        ),
        None
    );
}
