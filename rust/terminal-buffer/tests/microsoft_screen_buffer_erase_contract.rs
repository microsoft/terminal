#![allow(clippy::cast_possible_truncation)]

use terminal_buffer::rect_ops::{ScreenRect, fill_rect};
use terminal_buffer::screen_erase::{EraseType, erase_display, erase_line, erase_scrollback};
use terminal_buffer::text_attribute::{TextAttribute, UnderlineStyle};
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};
use terminal_buffer::text_color::Rgb;

fn fill_buffer(buffer: &mut TextBuffer, ch: u8, attribute: TextAttribute) {
    fill_rect(
        buffer,
        ScreenRect::new(0, 0, buffer.width(), buffer.height()),
        u16::from(ch),
        attribute,
    )
    .unwrap();
}

fn fill_row(buffer: &mut TextBuffer, y: u16, ch: u8, attribute: TextAttribute) {
    fill_rect(
        buffer,
        ScreenRect::new(0, y, buffer.width(), y + 1),
        u16::from(ch),
        attribute,
    )
    .unwrap();
}

fn assert_cell(buffer: &TextBuffer, x: u16, y: u16, ch: u8, attribute: TextAttribute) {
    let row = buffer.row(i32::from(y));
    assert_eq!(row.glyph_at(i32::from(x)), &[u16::from(ch)]);
    assert_eq!(row.attribute_at(i32::from(x)), attribute);
}

fn should_erase(
    erase_type: EraseType,
    erase_screen: bool,
    cursor: TextBufferPoint,
    x: u16,
    y: u16,
) -> bool {
    if !erase_screen {
        return y == cursor.y
            && match erase_type {
                EraseType::ToEnd => x >= cursor.x,
                EraseType::FromBeginning => x <= cursor.x,
                EraseType::All => true,
            };
    }

    match erase_type {
        EraseType::ToEnd => y > cursor.y || (y == cursor.y && x >= cursor.x),
        EraseType::FromBeginning => y < cursor.y || (y == cursor.y && x <= cursor.x),
        EraseType::All => true,
    }
}

#[test]
fn microsoft_screen_buffer_erase_tests_contract() {
    let width = 40;
    let height = 30;
    let viewport = ScreenRect::new(5, 10, 35, 20);
    let cursor = TextBufferPoint::new(width / 2, 15);
    let buffer_attr = TextAttribute::from_rgb(Rgb::new(1, 2, 3), Rgb::new(4, 5, 6));
    let mut active_attr = TextAttribute::from_rgb(Rgb::new(12, 34, 56), Rgb::new(78, 90, 12));
    active_attr.set_crossed_out(true);
    active_attr.set_reverse_video(true);
    active_attr.set_underline_style(UnderlineStyle::Curly);
    let mut erase_attr = active_attr;
    erase_attr.set_standard_erase();

    for erase_type in [EraseType::ToEnd, EraseType::FromBeginning, EraseType::All] {
        for erase_screen in [false, true] {
            for selective in [false, true] {
                let mut buffer = TextBuffer::new(width, height, TextAttribute::default()).unwrap();
                fill_buffer(&mut buffer, b'Z', buffer_attr);
                let mut protected_attr = buffer_attr;
                protected_attr.set_protected(true);
                if selective {
                    for y in viewport.top..viewport.bottom {
                        fill_rect(
                            &mut buffer,
                            ScreenRect::new(0, y, 5, y + 1),
                            u16::from(b'Z'),
                            protected_attr,
                        )
                        .unwrap();
                    }
                }

                if erase_screen {
                    erase_display(
                        &mut buffer,
                        viewport,
                        cursor,
                        erase_type,
                        selective,
                        active_attr,
                    )
                    .unwrap();
                } else {
                    erase_line(&mut buffer, cursor, erase_type, selective, active_attr).unwrap();
                }

                for y in 0..height {
                    for x in 0..width {
                        let in_view = y >= viewport.top && y < viewport.bottom;
                        let protected = selective && in_view && x < 5;
                        let erased = in_view
                            && !protected
                            && should_erase(erase_type, erase_screen, cursor, x, y);
                        let expected_char = if erased { b' ' } else { b'Z' };
                        let expected_attr = if protected {
                            protected_attr
                        } else if erased {
                            if selective { buffer_attr } else { erase_attr }
                        } else {
                            buffer_attr
                        };
                        assert_cell(&buffer, x, y, expected_char, expected_attr);
                    }
                }
            }
        }
    }
}

#[test]
fn microsoft_screen_buffer_erase_scrollback_tests_contract() {
    let width = 40;
    let height = 40;
    let initial = TextAttribute::default();
    let viewport_attr = TextAttribute::from_rgb(Rgb::new(7, 8, 9), Rgb::new(10, 11, 12));
    let buffer_attr = TextAttribute::from_rgb(Rgb::new(1, 2, 3), Rgb::new(4, 5, 6));
    let mut buffer = TextBuffer::new(width, height, initial).unwrap();
    fill_buffer(&mut buffer, b'Z', buffer_attr);

    for (offset, ch) in (b'A'..=b'J').enumerate() {
        fill_row(&mut buffer, 10 + offset as u16, ch, viewport_attr);
    }

    let mut viewport = ScreenRect::new(5, 10, 35, 20);
    let mut cursor = TextBufferPoint::new(20, 15);
    erase_scrollback(&mut buffer, &mut viewport, &mut cursor, initial).unwrap();

    assert_eq!(viewport, ScreenRect::new(5, 0, 35, 10));
    assert_eq!(cursor, TextBufferPoint::new(20, 5));
    for (offset, ch) in (b'A'..=b'J').enumerate() {
        assert_cell(&buffer, 0, offset as u16, ch, viewport_attr);
        assert_cell(&buffer, width - 1, offset as u16, ch, viewport_attr);
    }
    for y in 10..height {
        assert_cell(&buffer, 0, y, b' ', initial);
        assert_cell(&buffer, width - 1, y, b' ', initial);
    }
}
