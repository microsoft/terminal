use terminal_buffer::alternate_buffer::{AlternateBufferState, CursorShape, ViewportSize};

#[test]
fn microsoft_screen_buffer_single_alternate_buffer_creation_contract() {
    let mut state = AlternateBufferState::new();
    assert!(!state.is_alternate_active());
    assert!(state.alternate().is_none());
    state.use_alternate();
    assert!(state.is_alternate_active());
    assert!(state.alternate().is_some());
    state.use_main();
    assert!(!state.is_alternate_active());
    assert!(state.alternate().is_none());
}

#[test]
fn microsoft_screen_buffer_multiple_alternate_buffer_creation_contract() {
    let mut state = AlternateBufferState::new();
    state.use_alternate();
    assert_eq!(state.generation(), 1);
    state.active_mut().cursor.x = 9;
    state.use_alternate();
    assert_eq!(state.generation(), 2);
    assert_eq!(state.alternate().unwrap().cursor.x, 9);
    state.use_main();
    assert!(!state.is_alternate_active());
}

#[test]
fn microsoft_screen_buffer_multiple_alternates_from_main_contract() {
    let mut state = AlternateBufferState::new();
    state.use_alternate();
    state.use_main();
    state.main_mut().cursor.x = 7;
    state.use_alternate();
    assert_eq!(state.generation(), 2);
    assert_eq!(state.alternate().unwrap().cursor.x, 7);
}

#[test]
fn microsoft_screen_buffer_alternate_cursor_inheritance_contract() {
    let mut state = AlternateBufferState::new();
    let main = &mut state.main_mut().cursor;
    main.x = 3;
    main.y = 5;
    main.visible = false;
    main.size = 33;
    main.shape = CursorShape::DoubleUnderscore;
    main.blinking = false;

    state.use_alternate();
    assert_eq!(state.alternate().unwrap().cursor, state.main().cursor);
    {
        let alt = &mut state.active_mut().cursor;
        alt.x = 5;
        alt.y = 3;
        alt.visible = true;
        alt.size = 66;
        alt.shape = CursorShape::EmptyBox;
        alt.blinking = true;
    }
    state.use_main();
    assert_eq!((state.main().cursor.x, state.main().cursor.y), (3, 5));
    assert!(state.main().cursor.visible);
    assert_eq!(state.main().cursor.size, 66);
    assert_eq!(state.main().cursor.shape, CursorShape::EmptyBox);
    assert!(state.main().cursor.blinking);
}

#[test]
fn microsoft_screen_buffer_alt_buffer_cursor_state_contract() {
    let mut state = AlternateBufferState::new();
    state.main_mut().cursor.size = 47;
    state.main_mut().cursor.shape = CursorShape::DoubleUnderscore;
    state.use_alternate();
    let alt = state.alternate().unwrap();
    assert_eq!(alt.cursor.size, 47);
    assert_eq!(alt.cursor.shape, CursorShape::DoubleUnderscore);
}

#[test]
fn microsoft_screen_buffer_alt_buffer_vt_dispatching_contract() {
    let mut state = AlternateBufferState::new();
    state.use_alternate();
    state.dispatch_vt("\u{1b}[5;6H");
    assert_eq!((state.main().cursor.x, state.main().cursor.y), (0, 0));
    assert_eq!(
        (
            state.alternate().unwrap().cursor.x,
            state.alternate().unwrap().cursor.y
        ),
        (5, 4)
    );
    state.dispatch_vt("\u{1b}[48;2;255;0;255m");
    assert!(!state.main().magenta_background);
    assert!(state.alternate().unwrap().magenta_background);
    state.dispatch_vt("X");
    assert!(state.main().text.is_empty());
    assert_eq!(state.alternate().unwrap().text, "X");
    assert_eq!(state.alternate().unwrap().cursor.x, 6);
}

#[test]
fn microsoft_screen_buffer_alt_buffer_ris_contract() {
    let mut state = AlternateBufferState::new();
    state.use_alternate();
    assert!(state.is_alternate_active());
    state.ris();
    assert!(!state.is_alternate_active());
}

#[test]
fn microsoft_screen_buffer_resize_alt_buffer_contract() {
    let mut state = AlternateBufferState::with_main_viewport(80, 25);
    let original_main = state.main().viewport;
    state.use_alternate();
    assert_eq!(state.alternate().unwrap().viewport, original_main);

    state.resize_alternate_viewport(82, 27);
    assert_eq!(
        state.alternate().unwrap().viewport,
        ViewportSize::new(82, 27)
    );
    assert_eq!(state.main().viewport, original_main);

    state.use_main();
    assert!(!state.is_alternate_active());
    assert!(state.alternate().is_none());
    assert_eq!(state.main().viewport, original_main);
}

#[test]
fn microsoft_screen_buffer_resize_alt_buffer_get_screen_buffer_info_contract() {
    const DELTAS: [i16; 4] = [-10, -1, 1, 10];

    for dx in DELTAS {
        for dy in DELTAS {
            let mut state = AlternateBufferState::with_main_viewport(80, 25);
            let original_main = state.main().viewport;
            state.use_alternate();
            let original_alt = state.alternate().unwrap().viewport;
            assert_eq!(original_alt, original_main);

            let width = u16::try_from(i32::from(original_main.width) + i32::from(dx)).unwrap();
            let height = u16::try_from(i32::from(original_main.height) + i32::from(dy)).unwrap();
            state.resize_alternate_viewport(width, height);

            let resized_alt = state.alternate().unwrap().viewport;
            assert_ne!(resized_alt.width, original_alt.width);
            assert_ne!(resized_alt.height, original_alt.height);
            assert_eq!(state.main().viewport, original_main);
            assert_eq!(state.api_viewport(), resized_alt);
        }
    }
}

#[test]
fn microsoft_screen_buffer_restore_down_alt_buffer_terminal_scrolling_contract() {
    let mut state = AlternateBufferState::with_main_viewport(80, 25);
    state.set_terminal_scrolling(true);
    state.use_alternate();
    assert_eq!(state.alternate().unwrap().viewport_top, 0);
    assert_eq!(state.alternate().unwrap().virtual_bottom, 24);

    state.process_alternate_window_resize(160, 50);
    assert_eq!(state.alternate().unwrap().viewport_top, 0);
    assert_eq!(state.alternate().unwrap().virtual_bottom, 49);

    state.process_alternate_window_resize(80, 25);
    assert_eq!(state.alternate().unwrap().viewport_top, 0);
    assert_eq!(state.alternate().unwrap().virtual_bottom, 24);
}

#[test]
fn microsoft_screen_buffer_snap_cursor_terminal_scrolling_contract() {
    let mut state = AlternateBufferState::with_main_viewport(80, 25);
    state.set_terminal_scrolling(true);
    state.set_active_viewport_top(10, true);
    assert_eq!(state.main().viewport_top, 10);
    assert_eq!(state.main().virtual_bottom, 34);

    state.set_active_viewport_top(2, false);
    assert_eq!(state.main().viewport_top, 2);
    assert_eq!(state.main().virtual_bottom, 34);

    state.set_console_cursor_position(0, 10);
    assert_eq!((state.main().cursor.x, state.main().cursor.y), (0, 10));
    assert_eq!(state.main().viewport_top, 10);
    assert_eq!(state.main().virtual_bottom, 34);
}

#[test]
fn microsoft_screen_buffer_clear_alternate_buffer_contract() {
    let mut state = AlternateBufferState::with_main_viewport(80, 25);
    state.write_active_text("foo\nfoo");
    assert_eq!(state.main().text, "foo\nfoo");
    assert_eq!((state.main().cursor.x, state.main().cursor.y), (3, 1));

    state.use_alternate();
    state.set_console_cursor_position(0, 0);
    state.write_active_text("foo\nfoo");
    assert_eq!(state.alternate().unwrap().text, "foo\nfoo");
    state.clear_active_text();
    assert!(state.alternate().unwrap().text.is_empty());
    state.set_console_cursor_position(0, 0);
    assert_eq!(
        (
            state.alternate().unwrap().cursor.x,
            state.alternate().unwrap().cursor.y
        ),
        (0, 0)
    );

    assert_eq!(state.main().text, "foo\nfoo");
    assert_eq!((state.main().cursor.x, state.main().cursor.y), (3, 1));
}
