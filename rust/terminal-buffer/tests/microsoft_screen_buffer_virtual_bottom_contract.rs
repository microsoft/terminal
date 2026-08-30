use terminal_buffer::virtual_bottom::{CursorPosition, ViewportState, VirtualBottomState};

#[test]
fn microsoft_screen_buffer_update_virtual_bottom_when_cursor_moves_below_it_contract() {
    let mut state = VirtualBottomState::new(80, 25);
    assert_eq!(state.virtual_bottom(), 24);
    state.set_cursor_direct(0, 24);
    state.set_viewport_origin(0, 12, false);
    let scrolled_viewport = state.viewport();

    state.advance_output_lines(10);
    assert_eq!(state.cursor(), CursorPosition { x: 0, y: 34 });
    assert_eq!(state.virtual_bottom(), 34);
    assert_eq!(state.viewport(), scrolled_viewport);

    state.move_to_virtual_bottom();
    assert_eq!(state.viewport().bottom(), 34);
}

#[test]
fn microsoft_screen_buffer_update_virtual_bottom_with_set_console_cursor_position_contract() {
    let mut state = VirtualBottomState::new(80, 25);
    state.set_viewport_origin(0, 50, true);
    assert_eq!(state.virtual_bottom(), 74);

    state.set_viewport_origin(0, 0, false);
    state.set_console_cursor_position(0, 50);
    assert_eq!(state.viewport().top, 50);
    assert_eq!(state.virtual_bottom(), 74);

    state.set_viewport_origin(0, 60, false);
    state.set_console_cursor_position(0, 51);
    assert_eq!(state.viewport().top, 50);
    assert_eq!(state.virtual_bottom(), 74);

    state.set_console_cursor_position(0, 0);
    assert_eq!(state.viewport().top, 0);
    assert_eq!(state.virtual_bottom(), state.viewport().bottom());
}

#[test]
fn microsoft_screen_buffer_update_virtual_bottom_after_internal_set_viewport_size_contract() {
    let mut state = VirtualBottomState::new(80, 25);
    state.set_viewport_origin(0, 50, true);
    assert_eq!(state.virtual_bottom(), 74);
    state.set_cursor_direct(0, 74);
    state.set_viewport_origin(0, 0, false);

    state.internal_set_viewport_height(23);
    assert_eq!(state.virtual_bottom(), 74);

    state.set_viewport_origin(0, 51, false);
    assert_eq!(state.viewport().bottom(), 73);
    state.internal_set_viewport_height(25);
    assert_eq!(state.virtual_bottom(), state.viewport().bottom());
    assert_eq!(state.virtual_bottom(), 75);

    state.set_viewport_origin(0, 52, false);
    assert_eq!(state.viewport().bottom(), 76);
    state.internal_set_viewport_height(23);
    assert_eq!(state.virtual_bottom(), state.viewport().bottom());
    assert_eq!(state.virtual_bottom(), 74);
}

#[test]
fn microsoft_screen_buffer_dont_change_virtual_bottom_with_offscreen_linefeed_contract() {
    let mut state = VirtualBottomState::new(80, 25);
    state.set_viewport_origin(0, 50, true);
    state.set_cursor_direct(0, 50);
    state.set_viewport_origin(0, 0, false);
    let virtual_bottom = state.virtual_bottom();

    state.offscreen_linefeed();
    assert_eq!(state.cursor().y, 51);
    assert_eq!(state.virtual_bottom(), virtual_bottom);
}

#[test]
fn microsoft_screen_buffer_dont_change_virtual_bottom_after_resize_window_contract() {
    let mut state = VirtualBottomState::new(80, 25);
    state.set_viewport_origin(0, 50, true);
    state.set_cursor_direct(0, state.virtual_bottom());
    state.set_viewport_origin(0, 0, false);
    let virtual_bottom = state.virtual_bottom();

    state.resize_window(80, 23);
    assert_eq!(state.viewport().height, 23);
    assert_eq!(state.virtual_bottom(), virtual_bottom);
}

#[test]
fn microsoft_screen_buffer_dont_change_virtual_bottom_with_make_cursor_visible_contract() {
    let mut state = VirtualBottomState::new(80, 25);
    state.set_viewport_origin(0, 50, true);
    state.set_cursor_direct(0, 50);
    state.set_viewport_origin(0, 0, false);
    let virtual_bottom = state.virtual_bottom();

    state.make_cursor_visible();
    assert_eq!(state.cursor().y, state.viewport().bottom());
    assert_eq!(state.virtual_bottom(), virtual_bottom);

    state.set_viewport_origin(0, 60, false);
    state.make_cursor_visible();
    assert_eq!(state.cursor().y, state.viewport().top);
    assert_eq!(state.virtual_bottom(), virtual_bottom);
}

#[test]
fn microsoft_screen_buffer_retain_horizontal_offset_when_moving_to_bottom_contract() {
    let mut state = VirtualBottomState::new(40, 25);
    state.set_viewport_origin(10, 20, true);
    assert_eq!(
        state.virtual_viewport(),
        ViewportState {
            left: 10,
            top: 20,
            width: 40,
            height: 25
        }
    );
    state.set_cursor_direct(10, 20);

    state.set_viewport_origin(10, 10, false);
    assert_eq!(state.viewport().left, 10);
    assert_eq!(state.viewport().top, 10);

    state.move_to_virtual_bottom();
    assert_eq!(state.viewport().left, 10);
    assert_eq!(state.viewport().top, 20);
}
