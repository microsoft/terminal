//! Mouse input translation for Windows Terminal-compatible VT sequences.
//!
//! This module ports the semantics of `mouseInput.cpp` without depending on
//! Win32 message constants or platform APIs. The public enum names preserve the
//! meaning of those messages while the encoder stays portable and safe.

use super::{Mode, TerminalInput, control_state};

const WHEEL_DELTA: i32 = 120;
const MAX_DEFAULT_COORDINATE: i32 = 94;
const MAX_UTF8_COORDINATE: i32 = 32_734;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Point {
    pub x: i32,
    pub y: i32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MouseMessage {
    Move,
    LeftDown,
    LeftUp,
    LeftDoubleClick,
    RightDown,
    RightUp,
    RightDoubleClick,
    MiddleDown,
    MiddleUp,
    MiddleDoubleClick,
    Wheel,
    HorizontalWheel,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct MouseButtonState {
    pub left_down: bool,
    pub middle_down: bool,
    pub right_down: bool,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) struct MouseInputState {
    last_position: Point,
    last_message: Option<MouseMessage>,
    accumulated_delta: i32,
}

impl Default for MouseInputState {
    fn default() -> Self {
        Self {
            last_position: Point { x: -1, y: -1 },
            last_message: None,
            accumulated_delta: 0,
        }
    }
}

impl TerminalInput {
    #[must_use]
    pub fn is_tracking_mouse_input(&self) -> bool {
        self.get_input_mode(Mode::DefaultMouseTracking)
            || self.get_input_mode(Mode::ButtonEventMouseTracking)
            || self.get_input_mode(Mode::AnyEventMouseTracking)
    }

    #[must_use]
    pub fn handle_mouse(
        &mut self,
        position: Point,
        message: MouseMessage,
        modifier_key_state: u32,
        delta: i16,
        buttons: MouseButtonState,
    ) -> Option<String> {
        let delta = i32::from(delta);

        if delta.signum() != self.mouse_input_state.accumulated_delta.signum() {
            self.mouse_input_state.accumulated_delta = 0;
        }

        if is_wheel(message) {
            self.mouse_input_state.accumulated_delta += delta;
            if self.mouse_input_state.accumulated_delta.abs() < WHEEL_DELTA {
                return if self.is_tracking_mouse_input()
                    || self.should_send_alternate_scroll(message, delta)
                {
                    Some(String::new())
                } else {
                    None
                };
            }
            self.mouse_input_state.accumulated_delta = 0;
        }

        if self.is_tracking_mouse_input() {
            let hover = message == MouseMessage::Move;
            let button_event = is_button_event(message);
            let same_event = position == self.mouse_input_state.last_position
                && self.mouse_input_state.last_message == Some(message);
            let real_message = if hover {
                pressed_button(buttons)
            } else {
                message
            };
            let physical_button_pressed = real_message != MouseMessage::LeftUp;

            let should_emit = button_event
                || (hover
                    && self.get_input_mode(Mode::ButtonEventMouseTracking)
                    && !same_event
                    && physical_button_pressed)
                || (hover && self.get_input_mode(Mode::AnyEventMouseTracking) && !same_event);

            if should_emit {
                if self.get_input_mode(Mode::ButtonEventMouseTracking)
                    || self.get_input_mode(Mode::AnyEventMouseTracking)
                {
                    self.mouse_input_state.last_position = position;
                    self.mouse_input_state.last_message = Some(message);
                }

                if self.get_input_mode(Mode::Utf8MouseEncoding) {
                    return generate_utf8_sequence(
                        self,
                        position,
                        real_message,
                        hover,
                        modifier_key_state,
                        delta,
                    );
                }
                if self.get_input_mode(Mode::SgrMouseEncoding) {
                    let encoded_message = if physical_button_pressed {
                        real_message
                    } else {
                        message
                    };
                    return Some(generate_sgr_sequence(
                        self,
                        position,
                        encoded_message,
                        is_button_up(message),
                        hover,
                        modifier_key_state,
                        delta,
                    ));
                }
                return generate_default_sequence(
                    self,
                    position,
                    real_message,
                    hover,
                    modifier_key_state,
                    delta,
                );
            }
        }

        if self.should_send_alternate_scroll(message, delta) {
            return alternate_scroll_output(self, message, delta);
        }

        None
    }

    #[must_use]
    pub fn should_send_alternate_scroll(&self, message: MouseMessage, delta: i32) -> bool {
        self.in_alternate_buffer
            && self.get_input_mode(Mode::AlternateScroll)
            && is_wheel(message)
            && delta != 0
    }
}

fn is_button_event(message: MouseMessage) -> bool {
    matches!(
        message,
        MouseMessage::LeftDoubleClick
            | MouseMessage::LeftDown
            | MouseMessage::LeftUp
            | MouseMessage::MiddleUp
            | MouseMessage::RightUp
            | MouseMessage::RightDown
            | MouseMessage::RightDoubleClick
            | MouseMessage::MiddleDown
            | MouseMessage::MiddleDoubleClick
            | MouseMessage::Wheel
            | MouseMessage::HorizontalWheel
    )
}

fn is_wheel(message: MouseMessage) -> bool {
    matches!(message, MouseMessage::Wheel | MouseMessage::HorizontalWheel)
}

fn is_button_up(message: MouseMessage) -> bool {
    matches!(
        message,
        MouseMessage::LeftUp | MouseMessage::RightUp | MouseMessage::MiddleUp
    )
}

fn pressed_button(state: MouseButtonState) -> MouseMessage {
    if state.left_down {
        MouseMessage::LeftDown
    } else if state.middle_down {
        MouseMessage::MiddleDown
    } else if state.right_down {
        MouseMessage::RightDown
    } else {
        MouseMessage::LeftUp
    }
}

fn modifier_bits(modifier_key_state: u32) -> i32 {
    let mut value = 0;
    if modifier_key_state & control_state::SHIFT_PRESSED != 0 {
        value |= 0x04;
    }
    if modifier_key_state & control_state::ALT_PRESSED != 0 {
        value |= 0x08;
    }
    if modifier_key_state & control_state::CTRL_PRESSED != 0 {
        value |= 0x10;
    }
    value
}

fn x_button_encoding(
    message: MouseMessage,
    hover: bool,
    modifier_key_state: u32,
    delta: i32,
) -> u32 {
    let mut value = match message {
        MouseMessage::LeftDoubleClick | MouseMessage::LeftDown | MouseMessage::Move => 0,
        MouseMessage::LeftUp | MouseMessage::MiddleUp | MouseMessage::RightUp => 3,
        MouseMessage::RightDown | MouseMessage::RightDoubleClick => 2,
        MouseMessage::MiddleDown | MouseMessage::MiddleDoubleClick => 1,
        MouseMessage::Wheel => {
            if delta > 0 {
                0x40
            } else {
                0x41
            }
        }
        MouseMessage::HorizontalWheel => {
            if delta > 0 {
                0x43
            } else {
                0x42
            }
        }
    };
    if hover {
        value += 0x20;
    }
    value | u32::try_from(modifier_bits(modifier_key_state)).unwrap_or_default()
}

fn sgr_button_encoding(
    message: MouseMessage,
    hover: bool,
    modifier_key_state: u32,
    delta: i32,
) -> i32 {
    let mut value = match message {
        MouseMessage::LeftDoubleClick | MouseMessage::LeftDown | MouseMessage::LeftUp => 0,
        MouseMessage::RightUp | MouseMessage::RightDown | MouseMessage::RightDoubleClick => 2,
        MouseMessage::MiddleUp | MouseMessage::MiddleDown | MouseMessage::MiddleDoubleClick => 1,
        MouseMessage::Move => 3,
        MouseMessage::Wheel => {
            if delta > 0 {
                0x40
            } else {
                0x41
            }
        }
        MouseMessage::HorizontalWheel => {
            if delta > 0 {
                0x43
            } else {
                0x42
            }
        }
    };
    if hover {
        value += 0x20;
    }
    value | modifier_bits(modifier_key_state)
}

fn generate_default_sequence(
    input: &TerminalInput,
    position: Point,
    message: MouseMessage,
    hover: bool,
    modifier_key_state: u32,
    delta: i32,
) -> Option<String> {
    if position.x > MAX_DEFAULT_COORDINATE || position.y > MAX_DEFAULT_COORDINATE {
        return None;
    }
    generate_x10_like_sequence(input, position, message, hover, modifier_key_state, delta)
}

fn generate_utf8_sequence(
    input: &TerminalInput,
    position: Point,
    message: MouseMessage,
    hover: bool,
    modifier_key_state: u32,
    delta: i32,
) -> Option<String> {
    if position.x > MAX_UTF8_COORDINATE || position.y > MAX_UTF8_COORDINATE {
        return None;
    }
    generate_x10_like_sequence(input, position, message, hover, modifier_key_state, delta)
}

fn generate_x10_like_sequence(
    input: &TerminalInput,
    position: Point,
    message: MouseMessage,
    hover: bool,
    modifier_key_state: u32,
    delta: i32,
) -> Option<String> {
    let encoded_button =
        char::from_u32(0x20 + x_button_encoding(message, hover, modifier_key_state, delta))?;
    let encoded_x = encode_default_coordinate(position.x.checked_add(1)?)?;
    let encoded_y = encode_default_coordinate(position.y.checked_add(1)?)?;
    Some(format!(
        "{}M{encoded_button}{encoded_x}{encoded_y}",
        input.csi_prefix()
    ))
}

fn encode_default_coordinate(vt_coordinate: i32) -> Option<char> {
    let encoded = vt_coordinate.checked_add(32)?;
    char::from_u32(u32::try_from(encoded).ok()?)
}

fn generate_sgr_sequence(
    input: &TerminalInput,
    position: Point,
    message: MouseMessage,
    release: bool,
    hover: bool,
    modifier_key_state: u32,
    delta: i32,
) -> String {
    let button = sgr_button_encoding(message, hover, modifier_key_state, delta);
    let x = position.x.saturating_add(1);
    let y = position.y.saturating_add(1);
    let final_character = if release { 'm' } else { 'M' };
    format!("{}<{button};{x};{y}{final_character}", input.csi_prefix())
}

fn alternate_scroll_output(
    input: &TerminalInput,
    message: MouseMessage,
    delta: i32,
) -> Option<String> {
    let final_character = match message {
        MouseMessage::Wheel if delta > 0 => 'A',
        MouseMessage::Wheel => 'B',
        MouseMessage::HorizontalWheel if delta > 0 => 'C',
        MouseMessage::HorizontalWheel => 'D',
        _ => return None,
    };

    let prefix = if !input.get_input_mode(Mode::Ansi) {
        super::ESC.to_string()
    } else if input.get_input_mode(Mode::CursorKey) {
        input.ss3_prefix()
    } else {
        input.csi_prefix()
    };
    Some(format!("{prefix}{final_character}"))
}

#[cfg(test)]
mod tests {
    use super::{MouseButtonState, MouseMessage, Point, WHEEL_DELTA};
    use crate::{Mode, TerminalInput, control_state};

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
            x: 32_734,
            y: 32_734,
        },
        Point {
            x: 32_735,
            y: 32_735,
        },
    ];

    fn sgr_expected(message: MouseMessage, modifiers: u32, delta: i16, p: Point) -> String {
        let button = super::sgr_button_encoding(
            message,
            message == MouseMessage::Move,
            modifiers,
            i32::from(delta),
        );
        let final_character = if super::is_button_up(message) {
            'm'
        } else {
            'M'
        };
        format!("\u{1b}[<{button};{};{}{final_character}", p.x + 1, p.y + 1)
    }

    #[test]
    fn mouse_is_unhandled_until_tracking_is_enabled() {
        let mut input = TerminalInput::new();
        assert_eq!(
            input.handle_mouse(
                Point::default(),
                MouseMessage::LeftDown,
                0,
                0,
                MouseButtonState::default()
            ),
            None
        );
    }

    #[test]
    fn default_encoding_matches_microsoft_coordinate_boundary() {
        let messages = [
            MouseMessage::LeftDown,
            MouseMessage::LeftUp,
            MouseMessage::MiddleDown,
            MouseMessage::MiddleUp,
            MouseMessage::RightDown,
            MouseMessage::RightUp,
        ];
        let modifiers = [
            0,
            control_state::SHIFT_PRESSED,
            control_state::LEFT_CTRL_PRESSED,
            control_state::RIGHT_ALT_PRESSED,
            control_state::RIGHT_ALT_PRESSED | control_state::LEFT_CTRL_PRESSED,
        ];
        for message in messages {
            for modifier in modifiers {
                let mut input = TerminalInput::new();
                input.set_input_mode(Mode::DefaultMouseTracking, true);
                for point in TEST_COORDS {
                    let actual = input.handle_mouse(
                        point,
                        message,
                        modifier,
                        0,
                        MouseButtonState::default(),
                    );
                    assert_eq!(actual.is_some(), point.x <= 94 && point.y <= 94);
                    if let Some(sequence) = actual {
                        assert!(sequence.starts_with("\u{1b}[M"));
                        assert_eq!(sequence.chars().count(), 6);
                    }
                }
            }
        }
    }

    #[test]
    fn utf8_encoding_extends_coordinate_range_like_microsoft() {
        let mut input = TerminalInput::new();
        input.set_input_mode(Mode::Utf8MouseEncoding, true);
        input.set_input_mode(Mode::DefaultMouseTracking, true);
        for point in TEST_COORDS {
            let output = input.handle_mouse(
                point,
                MouseMessage::LeftDown,
                0,
                0,
                MouseButtonState::default(),
            );
            assert_eq!(output.is_some(), point.x <= 32_734 && point.y <= 32_734);
        }
    }

    #[test]
    fn sgr_encoding_matches_buttons_modifiers_and_coordinates() {
        let messages = [
            MouseMessage::LeftDown,
            MouseMessage::LeftUp,
            MouseMessage::MiddleDown,
            MouseMessage::MiddleUp,
            MouseMessage::RightDown,
            MouseMessage::RightUp,
        ];
        let modifiers = [
            0,
            control_state::SHIFT_PRESSED,
            control_state::LEFT_CTRL_PRESSED,
            control_state::RIGHT_ALT_PRESSED,
            control_state::RIGHT_ALT_PRESSED | control_state::LEFT_CTRL_PRESSED,
        ];
        for message in messages {
            for modifier in modifiers {
                let mut input = TerminalInput::new();
                input.set_input_mode(Mode::SgrMouseEncoding, true);
                input.set_input_mode(Mode::DefaultMouseTracking, true);
                for point in TEST_COORDS {
                    assert_eq!(
                        input.handle_mouse(
                            point,
                            message,
                            modifier,
                            0,
                            MouseButtonState::default()
                        ),
                        Some(sgr_expected(message, modifier, 0, point))
                    );
                }
            }
        }
    }

    #[test]
    fn movement_tracking_distinguishes_default_button_and_any_event_modes() {
        let point = Point { x: 4, y: 7 };
        let mut input = TerminalInput::new();
        input.set_input_mode(Mode::SgrMouseEncoding, true);
        input.set_input_mode(Mode::DefaultMouseTracking, true);
        assert_eq!(
            input.handle_mouse(point, MouseMessage::Move, 0, 0, MouseButtonState::default()),
            None
        );
        input.set_input_mode(Mode::ButtonEventMouseTracking, true);
        assert_eq!(
            input.handle_mouse(point, MouseMessage::Move, 0, 0, MouseButtonState::default()),
            None
        );
        let drag = input.handle_mouse(
            point,
            MouseMessage::Move,
            0,
            0,
            MouseButtonState {
                left_down: true,
                ..MouseButtonState::default()
            },
        );
        assert_eq!(drag.as_deref(), Some("\u{1b}[<32;5;8M"));
        assert_eq!(
            input.handle_mouse(
                point,
                MouseMessage::Move,
                0,
                0,
                MouseButtonState {
                    left_down: true,
                    ..MouseButtonState::default()
                }
            ),
            None
        );
        input.set_input_mode(Mode::AnyEventMouseTracking, true);
        let next = Point { x: 5, y: 7 };
        assert_eq!(
            input
                .handle_mouse(next, MouseMessage::Move, 0, 0, MouseButtonState::default())
                .as_deref(),
            Some("\u{1b}[<35;6;8M")
        );
    }

    #[test]
    fn wheel_delta_is_accumulated_and_direction_changes_reset_it() {
        let mut input = TerminalInput::new();
        input.set_input_mode(Mode::SgrMouseEncoding, true);
        input.set_input_mode(Mode::DefaultMouseTracking, true);
        let point = Point::default();
        assert_eq!(
            input.handle_mouse(
                point,
                MouseMessage::Wheel,
                0,
                40,
                MouseButtonState::default()
            ),
            Some(String::new())
        );
        assert_eq!(
            input.handle_mouse(
                point,
                MouseMessage::Wheel,
                0,
                40,
                MouseButtonState::default()
            ),
            Some(String::new())
        );
        assert_eq!(
            input.handle_mouse(
                point,
                MouseMessage::Wheel,
                0,
                -40,
                MouseButtonState::default()
            ),
            Some(String::new())
        );
        assert_eq!(
            input.handle_mouse(
                point,
                MouseMessage::Wheel,
                0,
                -80,
                MouseButtonState::default()
            ),
            Some("\u{1b}[<65;1;1M".to_string())
        );
        assert_eq!(WHEEL_DELTA, 120);
    }

    #[test]
    fn vertical_and_horizontal_wheels_match_microsoft_sgr_codes() {
        let mut input = TerminalInput::new();
        input.set_input_mode(Mode::SgrMouseEncoding, true);
        input.set_input_mode(Mode::DefaultMouseTracking, true);
        let p = Point { x: 2, y: 3 };
        for (message, delta, expected_button) in [
            (MouseMessage::Wheel, 120, 64),
            (MouseMessage::Wheel, -120, 65),
            (MouseMessage::HorizontalWheel, 120, 67),
            (MouseMessage::HorizontalWheel, -120, 66),
        ] {
            assert_eq!(
                input.handle_mouse(p, message, 0, delta, MouseButtonState::default()),
                Some(format!("\u{1b}[<{expected_button};3;4M"))
            );
        }
    }

    #[test]
    fn alternate_scroll_generates_cursor_sequences_and_honors_cursor_mode() {
        let mut input = TerminalInput::new();
        input.use_alternate_screen_buffer();
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
                    MouseButtonState::default()
                ),
                Some(expected.to_string())
            );
        }
        input.set_input_mode(Mode::CursorKey, true);
        assert_eq!(
            input.handle_mouse(
                Point::default(),
                MouseMessage::Wheel,
                0,
                120,
                MouseButtonState::default()
            ),
            Some("\u{1b}OA".to_string())
        );
        input.set_input_mode(Mode::AlternateScroll, false);
        assert_eq!(
            input.handle_mouse(
                Point::default(),
                MouseMessage::Wheel,
                0,
                120,
                MouseButtonState::default()
            ),
            None
        );
    }

    #[test]
    fn sgr_release_uses_lowercase_final_and_hover_uses_motion_bit() {
        let mut input = TerminalInput::new();
        input.set_input_mode(Mode::SgrMouseEncoding, true);
        input.set_input_mode(Mode::AnyEventMouseTracking, true);
        assert_eq!(
            input.handle_mouse(
                Point { x: 1, y: 2 },
                MouseMessage::LeftUp,
                0,
                0,
                MouseButtonState::default()
            ),
            Some("\u{1b}[<0;2;3m".to_string())
        );
        assert_eq!(
            input.handle_mouse(
                Point { x: 2, y: 2 },
                MouseMessage::Move,
                control_state::LEFT_CTRL_PRESSED,
                0,
                MouseButtonState::default()
            ),
            Some("\u{1b}[<51;3;3M".to_string())
        );
    }
}
