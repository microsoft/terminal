use terminal_buffer::rect_ops::{ScreenRect, fill_rect};
use terminal_buffer::screen_erase::erase_all_with_scrollback;
use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};
use terminal_buffer::text_color::TextColor;

const WIDTH: u16 = 20;
const HEIGHT: u16 = 30;
const VIEWPORT_HEIGHT: u16 = 10;

fn fill_row(buffer: &mut TextBuffer, y: u16, ch: u8, attribute: TextAttribute) {
    fill_rect(
        buffer,
        ScreenRect::new(0, y, buffer.width(), y + 1),
        u16::from(ch),
        attribute,
    )
    .unwrap();
}

fn assert_row(buffer: &TextBuffer, y: u16, ch: u8, attribute: TextAttribute) {
    let row = buffer.row(i32::from(y));
    for x in 0..buffer.width() {
        assert_eq!(row.glyph_at(i32::from(x)), &[u16::from(ch)]);
        assert_eq!(row.attribute_at(i32::from(x)), attribute);
    }
}

fn assert_blank_viewport(buffer: &TextBuffer, viewport: ScreenRect, attribute: TextAttribute) {
    for y in viewport.top..viewport.bottom {
        assert_row(buffer, y, b' ', attribute);
    }
}

#[test]
fn microsoft_screen_buffer_erase_all_tests_contract() {
    let default = TextAttribute::default();
    let preserved = TextAttribute::from_legacy(
        0x0007,
        terminal_buffer::text_attribute::LegacyColorDefaults::default(),
    );
    let mut buffer = TextBuffer::new(WIDTH, HEIGHT, default).unwrap();
    let mut viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_HEIGHT);

    // Case 1: one written row at the top. ED 2 advances the viewport one row,
    // preserves the cursor's relative row, and leaves the old row in scrollback.
    fill_row(&mut buffer, 0, b'F', preserved);
    let mut cursor = TextBufferPoint::new(3, 0);
    erase_all_with_scrollback(&mut buffer, &mut viewport, &mut cursor, default).unwrap();

    assert_eq!(viewport, ScreenRect::new(0, 1, WIDTH, 11));
    assert_eq!(cursor, TextBufferPoint::new(3, 1));
    assert_row(&buffer, 0, b'F', preserved);
    assert_blank_viewport(&buffer, viewport, default);

    // Case 2: multiple lines below the buffer top. The pre-erase cursor is on
    // row 3 while the viewport starts at row 1, so the new top is row 4 and the
    // cursor remains two rows below that new top.
    for y in 1..=3 {
        fill_row(&mut buffer, y, b'B', preserved);
    }
    cursor = TextBufferPoint::new(3, 3);
    erase_all_with_scrollback(&mut buffer, &mut viewport, &mut cursor, default).unwrap();

    assert_eq!(viewport, ScreenRect::new(0, 4, WIDTH, 14));
    assert_eq!(cursor, TextBufferPoint::new(3, 6));
    for y in 1..=3 {
        assert_row(&buffer, y, b'B', preserved);
    }
    assert_blank_viewport(&buffer, viewport, default);

    // Case 3: the viewport is already anchored to the physical buffer bottom.
    // ED 2 cannot advance it any farther, but it still clears the visible rows
    // and preserves the cursor's viewport-relative position.
    viewport = ScreenRect::new(0, HEIGHT - VIEWPORT_HEIGHT, WIDTH, HEIGHT);
    cursor = TextBufferPoint::new(3, HEIGHT - 3);
    for y in viewport.top..viewport.bottom {
        fill_row(&mut buffer, y, b'C', preserved);
    }

    erase_all_with_scrollback(&mut buffer, &mut viewport, &mut cursor, default).unwrap();

    assert_eq!(
        viewport,
        ScreenRect::new(0, HEIGHT - VIEWPORT_HEIGHT, WIDTH, HEIGHT)
    );
    assert_eq!(cursor, TextBufferPoint::new(3, HEIGHT - 3));
    assert_blank_viewport(&buffer, viewport, default);
}

#[test]
fn microsoft_screen_buffer_vt_erase_all_persist_cursor_contract() {
    let attribute = TextAttribute::default();
    let mut buffer = TextBuffer::new(WIDTH, HEIGHT, attribute).unwrap();
    let mut viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_HEIGHT);
    let mut cursor = TextBufferPoint::new(1, 1);

    erase_all_with_scrollback(&mut buffer, &mut viewport, &mut cursor, attribute).unwrap();

    assert_eq!(viewport, ScreenRect::new(0, 2, WIDTH, 12));
    assert_eq!(cursor, TextBufferPoint::new(1, 3));
}

#[test]
fn microsoft_screen_buffer_vt_erase_all_persist_cursor_fill_color_contract() {
    let mut active = TextAttribute::default();
    active.set_foreground(TextColor::index16(TextColor::DARK_RED));
    active.set_background(TextColor::index16(TextColor::BRIGHT_BLUE));
    let active_before = active;

    let mut buffer = TextBuffer::new(WIDTH, HEIGHT, TextAttribute::default()).unwrap();
    let mut viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_HEIGHT);
    let mut cursor = TextBufferPoint::new(0, 0);

    erase_all_with_scrollback(&mut buffer, &mut viewport, &mut cursor, active).unwrap();

    // ED 2 does not mutate the active rendition, and every cell in the newly
    // visible viewport receives the active standard-erase colors.
    assert_eq!(active, active_before);
    assert_eq!(viewport, ScreenRect::new(0, 1, WIDTH, 11));
    assert_eq!(cursor, TextBufferPoint::new(0, 1));
    assert_blank_viewport(&buffer, viewport, active_before);
}
