use terminal_buffer::cursor_movement::{CursorMovementState, CursorPosition};

#[test]
fn microsoft_screen_buffer_cursor_up_down_across_margins_contract() {
    let mut state = CursorMovementState::new(80, 30);
    state.set_vertical_margins(5, 18);

    state.set_cursor(0, 23);
    state.cursor_up(99);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 5 });

    state.set_cursor(0, 0);
    state.cursor_down(99);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 18 });
}

#[test]
fn microsoft_screen_buffer_cursor_up_down_outside_margins_contract() {
    let mut state = CursorMovementState::new(80, 30);
    state.set_vertical_margins(5, 18);

    state.set_cursor(0, 23);
    state.cursor_up(1);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 22 });

    state.set_cursor(0, 0);
    state.cursor_down(1);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 1 });
}

#[test]
fn microsoft_screen_buffer_cursor_up_down_exactly_at_margins_contract() {
    let mut state = CursorMovementState::new(80, 30);
    state.set_vertical_margins(5, 18);

    state.set_cursor(0, 18);
    state.cursor_down(1);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 18 });
    state.cursor_up(1);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 17 });

    state.set_cursor(0, 5);
    state.cursor_up(1);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 5 });
    state.cursor_down(1);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 6 });
}

#[test]
fn microsoft_screen_buffer_cursor_left_right_across_margins_contract() {
    let mut state = CursorMovementState::new(80, 30);
    state.set_horizontal_margin_mode(true);
    state.set_horizontal_margins(30, 49);

    state.set_cursor(39, 11);
    state.cursor_right(99);
    assert_eq!(state.cursor(), CursorPosition { x: 49, y: 11 });

    state.set_cursor(39, 11);
    state.cursor_left(99);
    assert_eq!(state.cursor(), CursorPosition { x: 30, y: 11 });
}

#[test]
fn microsoft_screen_buffer_cursor_left_right_outside_margins_contract() {
    let mut state = CursorMovementState::new(80, 30);
    state.set_horizontal_margin_mode(true);
    state.set_horizontal_margins(30, 49);

    state.set_cursor(0, 11);
    state.cursor_right(1);
    assert_eq!(state.cursor(), CursorPosition { x: 1, y: 11 });

    state.set_cursor(79, 11);
    state.cursor_left(1);
    assert_eq!(state.cursor(), CursorPosition { x: 78, y: 11 });
}

#[test]
fn microsoft_screen_buffer_cursor_left_right_exactly_at_margins_contract() {
    let mut state = CursorMovementState::new(80, 30);
    state.set_horizontal_margin_mode(true);
    state.set_horizontal_margins(30, 49);

    state.set_cursor(49, 11);
    state.cursor_right(1);
    assert_eq!(state.cursor(), CursorPosition { x: 49, y: 11 });
    state.cursor_left(1);
    assert_eq!(state.cursor(), CursorPosition { x: 48, y: 11 });

    state.set_cursor(30, 11);
    state.cursor_left(1);
    assert_eq!(state.cursor(), CursorPosition { x: 30, y: 11 });
    state.cursor_right(1);
    assert_eq!(state.cursor(), CursorPosition { x: 31, y: 11 });
}

#[test]
fn microsoft_screen_buffer_cursor_next_previous_line_contract() {
    let mut state = CursorMovementState::new(80, 30);

    state.set_cursor(20, 10);
    state.cursor_next_line(5);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 15 });

    state.set_cursor(20, 10);
    state.cursor_previous_line(5);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 5 });

    state.set_horizontal_margin_mode(true);
    state.set_horizontal_margins(10, 29);
    state.set_vertical_margins(8, 12);

    state.set_cursor(20, 10);
    state.cursor_next_line(5);
    assert_eq!(state.cursor(), CursorPosition { x: 10, y: 12 });

    state.set_cursor(20, 10);
    state.cursor_previous_line(5);
    assert_eq!(state.cursor(), CursorPosition { x: 10, y: 8 });

    state.set_cursor(20, 13);
    state.cursor_next_line(5);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 18 });

    state.set_cursor(20, 7);
    state.cursor_previous_line(5);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 2 });
}

#[test]
fn microsoft_screen_buffer_cursor_position_relative_contract() {
    let mut state = CursorMovementState::new(80, 30);

    state.set_cursor(20, 10);
    state.horizontal_position_relative(5);
    assert_eq!(state.cursor(), CursorPosition { x: 25, y: 10 });

    state.set_cursor(20, 10);
    state.vertical_position_relative(5);
    assert_eq!(state.cursor(), CursorPosition { x: 20, y: 15 });

    state.set_horizontal_margin_mode(true);
    state.set_horizontal_margins(18, 22);
    state.set_vertical_margins(8, 12);

    state.set_cursor(20, 10);
    state.horizontal_position_relative(5);
    assert_eq!(state.cursor(), CursorPosition { x: 25, y: 10 });

    state.set_cursor(20, 10);
    state.vertical_position_relative(5);
    assert_eq!(state.cursor(), CursorPosition { x: 20, y: 15 });

    state.set_cursor(20, 10);
    state.horizontal_position_relative(9_999);
    assert_eq!(state.cursor(), CursorPosition { x: 79, y: 10 });

    state.set_cursor(20, 10);
    state.vertical_position_relative(9_999);
    assert_eq!(state.cursor(), CursorPosition { x: 20, y: 29 });
}
