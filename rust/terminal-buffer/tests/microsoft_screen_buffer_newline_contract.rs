use terminal_buffer::rect_ops::{ScreenRect, fill_rect};
use terminal_buffer::text_attribute::{TextAttribute, UnderlineStyle};
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};
use terminal_buffer::text_color::Rgb;
use terminal_buffer::viewport_index::{LineFeedMargins, line_feed};

const WIDTH: u16 = 80;
const HEIGHT: u16 = 40;
const VIEWPORT_HEIGHT: u16 = 25;

fn active_fill_attribute() -> TextAttribute {
    let mut attribute = TextAttribute::from_rgb(Rgb::new(12, 34, 56), Rgb::new(78, 90, 12));
    attribute.set_crossed_out(true);
    attribute.set_reverse_video(true);
    attribute.set_underline_style(UnderlineStyle::Curly);
    attribute
}

fn standard_erase(mut attribute: TextAttribute) -> TextAttribute {
    attribute.set_standard_erase();
    attribute
}

fn fill_line(buffer: &mut TextBuffer, y: u16, glyph: u8, attribute: TextAttribute) {
    fill_rect(
        buffer,
        ScreenRect::new(0, y, buffer.width(), y + 1),
        u16::from(glyph),
        attribute,
    )
    .unwrap();
}

fn write_cell(buffer: &mut TextBuffer, cursor: &mut TextBufferPoint, glyph: u8) {
    buffer
        .row_mut(i32::from(cursor.y))
        .replace_glyph(i32::from(cursor.x), 1, &[u16::from(glyph)])
        .unwrap();
    cursor.x = cursor.x.saturating_add(1).min(buffer.width() - 1);
}

fn assert_cell(buffer: &TextBuffer, x: u16, y: u16, glyph: u8, attribute: TextAttribute) {
    let row = buffer.row(i32::from(y));
    assert_eq!(row.glyph_at(i32::from(x)), &[u16::from(glyph)]);
    assert_eq!(row.attribute_at(i32::from(x)), attribute);
}

fn assert_row_attributes(buffer: &TextBuffer, y: u16, expected: TextAttribute) {
    let row = buffer.row(i32::from(y));
    for x in 0..buffer.width() {
        assert_eq!(row.attribute_at(i32::from(x)), expected);
    }
}

#[test]
fn microsoft_screen_buffer_vt_scroll_margins_newline_color_contract() {
    let default = TextAttribute::default();
    let mut buffer = TextBuffer::new(WIDTH, HEIGHT, default).unwrap();
    let mut viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_HEIGHT);
    let mut cursor = TextBufferPoint::new(0, 0);
    let margins = LineFeedMargins::vertical(1, 5);

    for _ in 0..10 {
        write_cell(&mut buffer, &mut cursor, b'X');
        line_feed(
            &mut buffer,
            &mut viewport,
            &mut cursor,
            margins,
            false,
            default,
        )
        .unwrap();

        for y in 0..10 {
            assert_row_attributes(&buffer, y, default);
        }
    }
}

#[test]
fn microsoft_screen_buffer_vt_newline_past_viewport_contract() {
    let default = TextAttribute::default();
    let active = active_fill_attribute();
    let expected_fill = standard_erase(active);
    let mut buffer = TextBuffer::new(WIDTH, HEIGHT, default).unwrap();
    let mut viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_HEIGHT);
    let mut cursor = TextBufferPoint::new(0, VIEWPORT_HEIGHT - 1);

    line_feed(
        &mut buffer,
        &mut viewport,
        &mut cursor,
        LineFeedMargins::none(),
        false,
        active,
    )
    .unwrap();

    assert_eq!(viewport, ScreenRect::new(0, 1, WIDTH, VIEWPORT_HEIGHT + 1));
    assert_eq!(cursor, TextBufferPoint::new(0, VIEWPORT_HEIGHT));
    for y in viewport.top..viewport.bottom - 1 {
        assert_row_attributes(&buffer, y, default);
    }
    assert_row_attributes(&buffer, viewport.bottom - 1, expected_fill);
}

#[test]
fn microsoft_screen_buffer_vt_newline_past_end_of_buffer_contract() {
    let default = TextAttribute::default();
    let active = active_fill_attribute();
    let expected_fill = standard_erase(active);
    let mut buffer = TextBuffer::new(WIDTH, HEIGHT, default).unwrap();
    let mut viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_HEIGHT);
    let mut cursor = TextBufferPoint::new(0, 0);

    for _ in 0..HEIGHT {
        line_feed(
            &mut buffer,
            &mut viewport,
            &mut cursor,
            LineFeedMargins::none(),
            false,
            default,
        )
        .unwrap();
    }

    cursor = TextBufferPoint::new(0, viewport.bottom - 1);
    line_feed(
        &mut buffer,
        &mut viewport,
        &mut cursor,
        LineFeedMargins::none(),
        false,
        active,
    )
    .unwrap();

    assert_eq!(viewport.bottom, HEIGHT);
    assert_eq!(cursor, TextBufferPoint::new(0, HEIGHT - 1));
    for y in viewport.top..viewport.bottom - 1 {
        assert_row_attributes(&buffer, y, default);
    }
    assert_row_attributes(&buffer, viewport.bottom - 1, expected_fill);
}

#[test]
fn microsoft_screen_buffer_vt_newline_outside_margins_contract() {
    let default = TextAttribute::default();
    let mut buffer = TextBuffer::new(WIDTH, HEIGHT, default).unwrap();
    let mut viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_HEIGHT);
    let mut cursor = TextBufferPoint::new(0, VIEWPORT_HEIGHT - 1);

    line_feed(
        &mut buffer,
        &mut viewport,
        &mut cursor,
        LineFeedMargins::none(),
        false,
        default,
    )
    .unwrap();
    assert_eq!(cursor, TextBufferPoint::new(0, VIEWPORT_HEIGHT));
    assert_eq!(viewport.top, 1);

    viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_HEIGHT);
    cursor = TextBufferPoint::new(0, VIEWPORT_HEIGHT - 1);
    line_feed(
        &mut buffer,
        &mut viewport,
        &mut cursor,
        LineFeedMargins::vertical(0, 5),
        false,
        default,
    )
    .unwrap();

    assert_eq!(cursor, TextBufferPoint::new(0, VIEWPORT_HEIGHT - 1));
    assert_eq!(viewport.top, 0);
}

#[test]
fn microsoft_screen_buffer_line_feed_escape_sequences_contract() {
    for with_return in [true, false] {
        let default = TextAttribute::default();
        let mut buffer = TextBuffer::new(WIDTH, HEIGHT, default).unwrap();
        let mut viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_HEIGHT);
        let initial_x = WIDTH / 2;
        let expected_x = if with_return { 0 } else { initial_x };

        let mut cursor = TextBufferPoint::new(initial_x, 0);
        line_feed(
            &mut buffer,
            &mut viewport,
            &mut cursor,
            LineFeedMargins::none(),
            with_return,
            default,
        )
        .unwrap();
        assert_eq!(cursor, TextBufferPoint::new(expected_x, 1));
        assert_eq!(viewport.top, 0);

        cursor = TextBufferPoint::new(initial_x, viewport.bottom - 1);
        line_feed(
            &mut buffer,
            &mut viewport,
            &mut cursor,
            LineFeedMargins::none(),
            with_return,
            default,
        )
        .unwrap();
        assert_eq!(cursor, TextBufferPoint::new(expected_x, VIEWPORT_HEIGHT));
        assert_eq!(viewport.top, 1);

        let margin_top = viewport.top + 4;
        let margin_bottom = viewport.top + 10;
        let initial_y = margin_bottom - 1;
        fill_line(&mut buffer, initial_y, b'Q', default);
        cursor = TextBufferPoint::new(initial_x, initial_y);
        line_feed(
            &mut buffer,
            &mut viewport,
            &mut cursor,
            LineFeedMargins::vertical(margin_top, margin_bottom),
            with_return,
            default,
        )
        .unwrap();
        assert_eq!(cursor, TextBufferPoint::new(expected_x, initial_y));
        assert_eq!(viewport.top, 1);
        assert_cell(&buffer, 0, initial_y - 1, b'Q', default);
        assert_cell(&buffer, 0, initial_y, b' ', default);

        fill_line(&mut buffer, initial_y, b'R', default);
        let initial_x_in_margins = 5;
        let expected_x_in_margins = if with_return { 2 } else { initial_x_in_margins };
        cursor = TextBufferPoint::new(initial_x_in_margins, initial_y);
        line_feed(
            &mut buffer,
            &mut viewport,
            &mut cursor,
            LineFeedMargins::vertical(margin_top, margin_bottom).with_horizontal(2, 6),
            with_return,
            default,
        )
        .unwrap();

        assert_eq!(
            cursor,
            TextBufferPoint::new(expected_x_in_margins, initial_y)
        );
        assert_eq!(viewport.top, 1);
        for x in 0..2 {
            assert_cell(&buffer, x, initial_y - 1, b'Q', default);
            assert_cell(&buffer, x, initial_y, b'R', default);
        }
        for x in 2..6 {
            assert_cell(&buffer, x, initial_y - 1, b'R', default);
            assert_cell(&buffer, x, initial_y, b' ', default);
        }
        assert_cell(&buffer, 6, initial_y - 1, b'Q', default);
        assert_cell(&buffer, 6, initial_y, b'R', default);
    }
}
