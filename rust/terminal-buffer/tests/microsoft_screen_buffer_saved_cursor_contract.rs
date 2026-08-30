use terminal_buffer::saved_cursor::{CharacterSet, SavedCursorPosition, SavedCursorState};
use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_color::Rgb;

#[test]
fn microsoft_screen_buffer_cursor_save_restore_contract() {
    let default_attrs = TextAttribute::default();
    let color_attrs = TextAttribute::from_rgb(Rgb::new(12, 34, 56), Rgb::new(78, 90, 12));
    let mut state = SavedCursorState::new(80, 30);

    // Restore after save: position, delayed wrap, attributes and G0 charset all
    // round-trip. Restoring again without another save reuses the same slot.
    state.set_cursor(20, 10);
    state.set_delayed_wrap(true);
    state.set_attributes(color_attrs);
    state.set_charset(CharacterSet::DecSpecialGraphics);
    state.save_cursor();

    state.set_cursor(0, 0);
    state.set_delayed_wrap(false);
    state.set_attributes(default_attrs);
    state.set_charset(CharacterSet::Ascii);
    state.restore_cursor();
    assert_eq!(state.cursor(), SavedCursorPosition { x: 20, y: 10 });
    assert!(state.delayed_wrap());
    assert_eq!(state.attributes(), color_attrs);
    assert_eq!(state.render_with_charset("lwkmvj"), "┌┬┐└┴┘");

    state.set_cursor(0, 0);
    state.set_delayed_wrap(false);
    state.set_attributes(default_attrs);
    state.set_charset(CharacterSet::Ascii);
    state.restore_cursor();
    assert_eq!(state.cursor(), SavedCursorPosition { x: 20, y: 10 });
    assert!(state.delayed_wrap());
    assert_eq!(state.attributes(), color_attrs);
    assert_eq!(state.render_with_charset("lwkmvj"), "┌┬┐└┴┘");

    // DECSTR resets the saved slot to the VT defaults.
    state.soft_reset();
    state.set_cursor(20, 10);
    state.set_delayed_wrap(true);
    state.set_attributes(color_attrs);
    state.set_charset(CharacterSet::DecSpecialGraphics);
    state.restore_cursor();
    assert_eq!(state.cursor(), SavedCursorPosition { x: 0, y: 0 });
    assert!(!state.delayed_wrap());
    assert_eq!(state.attributes(), default_attrs);
    assert_eq!(state.charset(), CharacterSet::Ascii);
    assert_eq!(state.render_with_charset("lwkmvj"), "lwkmvj");

    // DECOM is part of the saved slot. After restoring it, CUP home resolves
    // against the current vertical and horizontal margins.
    state.set_horizontal_margin_mode(true);
    state.set_vertical_margins(9, 19);
    state.set_horizontal_margins(30, 49);
    state.set_origin_mode(true);
    assert_eq!(state.cursor(), SavedCursorPosition { x: 30, y: 9 });
    state.save_cursor();
    state.set_origin_mode(false);
    assert_eq!(state.cursor(), SavedCursorPosition { x: 0, y: 0 });
    state.restore_cursor();
    state.home();
    assert!(state.origin_mode());
    assert_eq!(state.cursor(), SavedCursorPosition { x: 30, y: 9 });

    // A relative saved position is evaluated against the margins active at
    // restore time, rather than freezing the old margin origin.
    state.clear_vertical_margins();
    state.clear_horizontal_margins();
    state.set_origin_mode(true);
    state.set_cursor(5, 5);
    state.save_cursor();
    state.set_vertical_margins(14, 24);
    state.set_horizontal_margins(30, 49);
    state.restore_cursor();
    assert_eq!(state.cursor(), SavedCursorPosition { x: 35, y: 19 });

    // If the saved relative position exceeds the new region, DECRC clamps it
    // to the bottom-right margin.
    state.clear_vertical_margins();
    state.clear_horizontal_margins();
    state.set_origin_mode(true);
    state.set_cursor(15, 15);
    state.save_cursor();
    state.set_vertical_margins(0, 9);
    state.set_horizontal_margins(0, 9);
    state.restore_cursor();
    assert_eq!(state.cursor(), SavedCursorPosition { x: 9, y: 9 });
}
