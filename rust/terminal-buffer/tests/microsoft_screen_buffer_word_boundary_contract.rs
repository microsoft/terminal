use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};
use terminal_buffer::word_boundary::{WordBoundary, screen_word_boundary};

fn buffer_with_ascii(text: &str) -> TextBuffer {
    let width = u16::try_from(text.len()).expect("Microsoft test text fits in a terminal row");
    let mut buffer =
        TextBuffer::new(width, 10, TextAttribute::default()).expect("valid test buffer");

    for (x, byte) in text.bytes().enumerate() {
        buffer
            .row_mut(0)
            .replace_glyph(
                i32::try_from(x).expect("small Microsoft test column"),
                1,
                &[u16::from(byte)],
            )
            .expect("ASCII glyph fits in the source-sized row");
    }

    buffer
}

fn assert_boundary(buffer: &TextBuffer, query_x: u16, start_x: u16, end_x: u16, trim: bool) {
    assert_eq!(
        screen_word_boundary(buffer, TextBufferPoint::new(query_x, 0), trim, &[]),
        WordBoundary {
            start: TextBufferPoint::new(start_x, 0),
            end: TextBufferPoint::new(end_x, 0),
        }
    );
}

#[test]
fn microsoft_screen_buffer_get_word_boundary_contract() {
    let buffer = buffer_with_ascii("This is some test text for word boundaries.");

    // First word: front, middle and final glyph.
    for query_x in [0, 1, 3] {
        assert_boundary(&buffer, query_x, 0, 4, false);
    }

    // Middle word: front, middle and final glyph.
    for query_x in [13, 15, 16] {
        assert_boundary(&buffer, query_x, 13, 17, false);
    }

    // Final word: front, middle and the source's one-past-row query.
    for query_x in [32, 39, 43] {
        assert_boundary(&buffer, query_x, 32, 43, false);
    }

    // Microsoft's separator case asks on the space after "some" and expects
    // the word immediately to its left, with an exclusive end at that space.
    assert_boundary(&buffer, 12, 8, 12, false);
}

fn assert_trim_leading_zeros_contract(trim: bool) {
    let buffer = buffer_with_ascii("000fe12 0xfe12 0Xfe12 0nfe12 0Nfe12");

    assert_boundary(&buffer, 0, if trim { 3 } else { 0 }, 7, trim);
    assert_boundary(&buffer, 8, 8, 14, trim);
    assert_boundary(&buffer, 15, 15, 21, trim);
    assert_boundary(&buffer, 22, 22, 28, trim);
    assert_boundary(&buffer, 29, if trim { 30 } else { 29 }, 35, trim);
}

#[test]
fn microsoft_screen_buffer_get_word_boundary_trim_zeros_on_contract() {
    assert_trim_leading_zeros_contract(true);
}

#[test]
fn microsoft_screen_buffer_get_word_boundary_trim_zeros_off_contract() {
    assert_trim_leading_zeros_contract(false);
}
