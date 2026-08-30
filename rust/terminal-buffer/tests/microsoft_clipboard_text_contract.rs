use terminal_buffer::clipboard_text::{
    ClipboardCopyRequest, ClipboardSelectionMode, get_plain_text,
};
use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};

fn write_common_row(buffer: &mut TextBuffer, y: i32) {
    let row = buffer.row_mut(y);
    for (x, width, glyph) in [
        (0, 1, vec![u16::from(b'A')]),
        (1, 1, vec![u16::from(b'B')]),
        (2, 2, vec![0x304b]),
        (4, 1, vec![u16::from(b'C')]),
        (5, 2, vec![0x304d]),
        (7, 1, vec![u16::from(b'D')]),
        (8, 1, vec![u16::from(b'E')]),
    ] {
        row.replace_glyph(x, width, &glyph)
            .expect("Microsoft clipboard fixture glyph fits");
    }
}

fn microsoft_common_state_fixture() -> TextBuffer {
    let mut buffer = TextBuffer::new(80, 4, TextAttribute::default()).unwrap();
    for y in 0..4 {
        write_common_row(&mut buffer, y);
    }
    buffer.row_mut(1).set_wrap_forced(true);
    buffer.row_mut(3).set_wrap_forced(true);
    buffer
}

#[test]
fn microsoft_clipboard_block_selection_contract() {
    let buffer = microsoft_common_state_fixture();
    let actual = get_plain_text(
        &buffer,
        ClipboardCopyRequest::new(
            TextBufferPoint::new(0, 0),
            TextBufferPoint::new(15, 3),
            ClipboardSelectionMode::Block,
        ),
    );
    let expected = [
        "ABかCきDE      ",
        "ABかCきDE      ",
        "ABかCきDE      ",
        "ABかCきDE      ",
    ]
    .join("\r\n")
    .encode_utf16()
    .collect::<Vec<_>>();
    assert_eq!(actual, expected);
}

#[test]
fn microsoft_clipboard_line_selection_contract() {
    let buffer = microsoft_common_state_fixture();
    let actual = get_plain_text(
        &buffer,
        ClipboardCopyRequest::new(
            TextBufferPoint::new(0, 0),
            TextBufferPoint::new(15, 3),
            ClipboardSelectionMode::Line,
        ),
    );

    let mut expected = "ABかCきDE\r\n".encode_utf16().collect::<Vec<_>>();
    expected.extend("ABかCきDE".encode_utf16());
    expected.extend(std::iter::repeat_n(u16::from(b' '), 71));
    expected.extend("ABかCきDE\r\nABかCきDE      ".encode_utf16());
    assert_eq!(actual, expected);
}
