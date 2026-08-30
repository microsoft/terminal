use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::TextBuffer;
use terminal_buffer::vertical_scroll::{ScrollCursor, VerticalScrollState};

fn write_ascii(buffer: &mut TextBuffer, x: u16, y: u16, text: &[u8]) {
    let row = buffer.row_mut(i32::from(y));
    for (offset, byte) in text.iter().copied().enumerate() {
        let column = x + u16::try_from(offset).expect("Microsoft fixture fits in one row");
        row.replace_glyph(i32::from(column), 1, &[u16::from(byte)])
            .expect("fixture glyph fits");
    }
}

fn last_non_space_row(buffer: &TextBuffer) -> Option<u16> {
    buffer
        .logical_rows()
        .enumerate()
        .filter(|&(_row, content)| content.measure_right() != 0)
        .map(|(row, _content)| u16::try_from(row).expect("TextBuffer row index fits u16"))
        .last()
}

#[test]
fn microsoft_screen_buffer_test_reverse_line_feed_contract() {
    let attribute = TextAttribute::default();

    // Microsoft case 1: RI below the viewport top moves only Y upward. X and
    // the viewport remain unchanged.
    let mut buffer = TextBuffer::new(20, 12, attribute).expect("valid fixture");
    write_ascii(&mut buffer, 0, 0, b"foo");
    write_ascii(&mut buffer, 3, 1, b"foo");
    let mut state = VerticalScrollState::new(20, 0, 5);
    state.set_cursor(3, 1);
    state
        .reverse_index(&mut buffer, attribute)
        .expect("RI below viewport top succeeds");
    assert_eq!(state.cursor(), ScrollCursor { x: 3, y: 0 });
    assert_eq!(last_non_space_row(&buffer), Some(1));

    // Microsoft case 2: RI at the top pins the cursor and scrolls the viewport
    // contents down one row. The second foo therefore moves from row 1 to row 2.
    write_ascii(&mut buffer, 0, 0, b"123456789");
    state.set_cursor(9, 0);
    state
        .reverse_index(&mut buffer, attribute)
        .expect("RI at viewport top succeeds");
    assert_eq!(state.cursor(), ScrollCursor { x: 9, y: 0 });
    assert_eq!(last_non_space_row(&buffer), Some(2));

    // Microsoft case 3: the same top-of-viewport rule applies when the viewport
    // begins below the physical buffer origin. X stays at 8, Y stays at 5, and
    // the printable row is shifted to row 6.
    let mut buffer = TextBuffer::new(20, 12, attribute).expect("valid offset fixture");
    write_ascii(&mut buffer, 0, 5, b"ABCDEFGH");
    let mut state = VerticalScrollState::new(20, 5, 10);
    state.set_cursor(8, 5);
    state
        .reverse_index(&mut buffer, attribute)
        .expect("RI at offset viewport top succeeds");
    assert_eq!(state.cursor(), ScrollCursor { x: 8, y: 5 });
    assert_eq!(last_non_space_row(&buffer), Some(6));
}
