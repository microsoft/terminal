use terminal_buffer::rect_ops::{ScreenRect, fill_rect};
use terminal_buffer::screen_erase::{hard_reset, set_character_protection};
use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};
use terminal_buffer::text_color::Rgb;

fn assert_cell(buffer: &TextBuffer, x: u16, y: u16, ch: u8, attribute: TextAttribute) {
    let row = buffer.row(i32::from(y));
    assert_eq!(row.glyph_at(i32::from(x)), &[u16::from(ch)]);
    assert_eq!(row.attribute_at(i32::from(x)), attribute);
}

fn fill_buffer(buffer: &mut TextBuffer, ch: u8, attribute: TextAttribute) {
    fill_rect(
        buffer,
        ScreenRect::new(0, 0, buffer.width(), buffer.height()),
        u16::from(ch),
        attribute,
    )
    .unwrap();
}

#[test]
fn microsoft_screen_buffer_protected_attribute_tests_contract() {
    let cases: &[(&[u16], bool)] = &[
        (&[], false),
        (&[0], false),
        (&[1], true),
        (&[2], false),
        (&[2, 1], true),
        (&[1, 2], false),
    ];

    for (params, expected_protected) in cases {
        let mut active = TextAttribute::default();
        active.set_protected(!expected_protected);
        set_character_protection(&mut active, params);
        assert_eq!(active.is_protected(), *expected_protected);

        let mut buffer = TextBuffer::new(10, 1, TextAttribute::default()).unwrap();
        fill_rect(
            &mut buffer,
            ScreenRect::new(0, 0, 5, 1),
            u16::from(b'Z'),
            active,
        )
        .unwrap();
        for x in 0..5 {
            assert_cell(&buffer, x, 0, b'Z', active);
        }
    }
}

#[test]
fn microsoft_screen_buffer_hard_reset_buffer_contract() {
    let defaults = TextAttribute::default();
    let colored = TextAttribute::from_rgb(Rgb::new(12, 34, 56), Rgb::new(78, 90, 12));
    let mut buffer = TextBuffer::new(40, 60, defaults).unwrap();
    let mut viewport = ScreenRect::new(0, 0, 40, 25);
    let mut cursor = TextBufferPoint::new(0, 1);
    let mut active = defaults;

    fill_rect(
        &mut buffer,
        ScreenRect::new(0, 0, 12, 1),
        u16::from(b'H'),
        defaults,
    )
    .unwrap();
    hard_reset(&mut buffer, &mut viewport, &mut cursor, &mut active);
    assert_eq!(viewport, ScreenRect::new(0, 0, 40, 25));
    assert_eq!(cursor, TextBufferPoint::new(0, 0));
    assert_eq!(active, defaults);
    assert_cell(&buffer, 0, 0, b' ', defaults);

    fill_buffer(&mut buffer, b'X', colored);
    viewport = ScreenRect::new(0, 20, 40, 45);
    cursor = TextBufferPoint::new(0, 40);
    active = colored;
    hard_reset(&mut buffer, &mut viewport, &mut cursor, &mut active);

    assert_eq!(viewport, ScreenRect::new(0, 0, 40, 25));
    assert_eq!(cursor, TextBufferPoint::new(0, 0));
    assert_eq!(active, defaults);
    for y in 0..buffer.height() {
        assert_cell(&buffer, 0, y, b' ', defaults);
        assert_cell(&buffer, buffer.width() - 1, y, b' ', defaults);
    }
}
