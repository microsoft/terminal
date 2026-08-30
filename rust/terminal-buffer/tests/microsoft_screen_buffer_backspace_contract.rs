use terminal_buffer::color_table::ColorTableState;
use terminal_buffer::host_write::HostWriteState;
use terminal_buffer::line_edit::delete_cells;
use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};
use terminal_buffer::text_color::Rgb;

const WIDTH: u16 = 80;
const HEIGHT: u16 = 25;
const MAGENTA: Rgb = Rgb::new(255, 0, 255);

fn default_symbolic_attribute() -> TextAttribute {
    let mut attribute = TextAttribute::default();
    attribute.set_default_background();
    attribute
}

fn fixture() -> (TextBuffer, ColorTableState, TextAttribute) {
    let expected = default_symbolic_attribute();
    let buffer = TextBuffer::new(WIDTH, HEIGHT, expected).expect("valid Microsoft-sized buffer");
    let mut colors = ColorTableState::default();
    assert!(colors.apply_osc(11, "rgb:ff/00/ff"));
    (buffer, colors, expected)
}

fn assert_default_cell(
    buffer: &TextBuffer,
    colors: &ColorTableState,
    x: u16,
    expected: TextAttribute,
) {
    let attribute = buffer.row(0).attribute_at(i32::from(x));
    assert!(!attribute.is_legacy());
    assert_eq!(attribute, expected);
    assert_eq!(colors.attribute_colors(attribute).1, MAGENTA);
}

#[test]
fn microsoft_screen_buffer_backspace_default_attrs_contract() {
    let (mut buffer, colors, expected) = fixture();
    let mut writer = HostWriteState::new(TextBufferPoint::new(0, 0), expected);

    writer
        .write_vt(&mut buffer, &"XX\u{8}".encode_utf16().collect::<Vec<_>>())
        .expect("VT write plus backspace succeeds");

    assert_eq!(writer.cursor(), TextBufferPoint::new(1, 0));
    assert_eq!(buffer.row(0).glyph_at(0), &[u16::from(b'X')]);
    assert_eq!(buffer.row(0).glyph_at(1), &[u16::from(b'X')]);
    assert_default_cell(&buffer, &colors, 0, expected);
    assert_default_cell(&buffer, &colors, 1, expected);
}

#[test]
fn microsoft_screen_buffer_backspace_default_attrs_write_chars_legacy_contract() {
    for write_singly in [false, true] {
        let (mut buffer, colors, expected) = fixture();
        let mut writer = HostWriteState::new(TextBufferPoint::new(0, 0), expected);

        if write_singly {
            writer
                .write_chars_legacy(&mut buffer, &[u16::from(b'X')])
                .expect("first legacy write succeeds");
            writer
                .write_chars_legacy(&mut buffer, &[u16::from(b'X')])
                .expect("second legacy write succeeds");
            writer
                .write_chars_legacy(&mut buffer, &[0x0008])
                .expect("legacy backspace succeeds");
        } else {
            writer
                .write_chars_legacy(&mut buffer, &[u16::from(b'X'), u16::from(b'X'), 0x0008])
                .expect("batched legacy write succeeds");
        }

        assert_eq!(writer.cursor(), TextBufferPoint::new(1, 0));
        assert_eq!(buffer.row(0).glyph_at(0), &[u16::from(b'X')]);
        assert_eq!(buffer.row(0).glyph_at(1), &[u16::from(b'X')]);
        assert_default_cell(&buffer, &colors, 0, expected);
        assert_default_cell(&buffer, &colors, 1, expected);
    }
}

#[test]
fn microsoft_screen_buffer_backspace_default_attrs_in_prompt_contract() {
    let (mut buffer, _colors, expected) = fixture();
    let mut writer = HostWriteState::new(TextBufferPoint::new(0, 0), expected);

    // Microsoft's source first clears the row with the current attributes. The
    // fixture is constructed with that same symbolic default attribute, so the
    // complete row starts in the equivalent post-ED state.
    writer
        .write_vt(&mut buffer, &"XXX".encode_utf16().collect::<Vec<_>>())
        .expect("prompt text write succeeds");
    writer.cursor_left(2);
    assert_eq!(writer.cursor(), TextBufferPoint::new(1, 0));

    let cursor = writer.cursor();
    delete_cells(
        buffer.row_mut(i32::from(cursor.y)),
        cursor.x,
        1,
        0..WIDTH,
        writer.current_attribute(),
    )
    .expect("prompt DCH succeeds");

    assert_eq!(writer.cursor(), TextBufferPoint::new(1, 0));
    for x in 0..WIDTH {
        assert_eq!(buffer.row(0).attribute_at(i32::from(x)), expected);
    }
}
