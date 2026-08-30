use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::TextBuffer;
use terminal_core::selection::{BufferPoint, SelectionExpansion, SelectionState};

fn buffer(width: u16, height: u16) -> TextBuffer {
    TextBuffer::new(width, height, TextAttribute::default()).expect("valid test buffer")
}

fn write_ascii(buffer: &mut TextBuffer, y: i32, start_x: i32, text: &[u8]) {
    for (offset, byte) in text.iter().copied().enumerate() {
        buffer
            .row_mut(y)
            .replace_glyph(
                start_x + i32::try_from(offset).expect("small test offset"),
                1,
                &[u16::from(byte)],
            )
            .expect("test glyph fits");
    }
}

#[test]
fn microsoft_terminal_core_double_click_drag_right_preserves_word_expansion() {
    let mut buffer = buffer(100, 100);
    write_ascii(&mut buffer, 10, 4, b"doubleClickMe dragThroughHere");
    let mut state = SelectionState::default();

    state.multi_click(
        &buffer,
        BufferPoint::new(5, 10),
        SelectionExpansion::Word,
        &[],
    );
    state.set_end(BufferPoint::new(21, 10), Some(&buffer), None, &[]);

    assert_eq!(state.selection.start, BufferPoint::new(4, 10));
    assert_eq!(state.selection.end, BufferPoint::new(33, 10));
}

#[test]
fn microsoft_terminal_core_double_click_drag_left_preserves_word_expansion() {
    let mut buffer = buffer(100, 100);
    write_ascii(&mut buffer, 10, 4, b"doubleClickMe dragThroughHere");
    let mut state = SelectionState::default();

    state.multi_click(
        &buffer,
        BufferPoint::new(21, 10),
        SelectionExpansion::Word,
        &[],
    );
    state.set_end(BufferPoint::new(5, 10), Some(&buffer), None, &[]);

    assert_eq!(state.selection.start, BufferPoint::new(4, 10));
    assert_eq!(state.selection.end, BufferPoint::new(33, 10));
}

#[test]
fn microsoft_terminal_core_triple_click_general_case_selects_full_row() {
    let buffer = buffer(100, 100);
    let mut state = SelectionState::default();

    state.multi_click(
        &buffer,
        BufferPoint::new(5, 10),
        SelectionExpansion::Line,
        &[],
    );

    assert_eq!(state.selection.start, BufferPoint::new(0, 10));
    assert_eq!(state.selection.end, BufferPoint::new(100, 10));
}

#[test]
fn microsoft_terminal_core_triple_click_drag_horizontal_keeps_full_row() {
    let buffer = buffer(100, 100);
    let mut state = SelectionState::default();

    state.multi_click(
        &buffer,
        BufferPoint::new(5, 10),
        SelectionExpansion::Line,
        &[],
    );
    state.set_end(BufferPoint::new(7, 10), Some(&buffer), None, &[]);

    assert_eq!(state.selection.start, BufferPoint::new(0, 10));
    assert_eq!(state.selection.end, BufferPoint::new(100, 10));
}

#[test]
fn microsoft_terminal_core_triple_click_drag_vertical_expands_through_target_row() {
    let buffer = buffer(100, 100);
    let mut state = SelectionState::default();

    state.multi_click(
        &buffer,
        BufferPoint::new(5, 10),
        SelectionExpansion::Line,
        &[],
    );
    state.set_end(BufferPoint::new(5, 11), Some(&buffer), None, &[]);

    assert_eq!(state.selection.start, BufferPoint::new(0, 10));
    assert_eq!(state.selection.end, BufferPoint::new(100, 11));
}

#[test]
fn microsoft_terminal_core_shift_click_preserves_expansion_mode_across_full_sequence() {
    let mut buffer = buffer(100, 100);
    write_ascii(
        &mut buffer,
        10,
        4,
        b"doubleClickMe dragThroughHere anotherWord",
    );
    let mut state = SelectionState::default();

    state.multi_click(
        &buffer,
        BufferPoint::new(5, 10),
        SelectionExpansion::Word,
        &[],
    );
    assert_eq!(state.selection.start, BufferPoint::new(4, 10));
    assert_eq!(state.selection.end, BufferPoint::new(17, 10));

    state.set_end(
        BufferPoint::new(21, 10),
        Some(&buffer),
        Some(SelectionExpansion::Char),
        &[],
    );
    assert_eq!(state.selection.start, BufferPoint::new(4, 10));
    assert_eq!(state.selection.end, BufferPoint::new(22, 10));

    state.set_end(
        BufferPoint::new(21, 10),
        Some(&buffer),
        Some(SelectionExpansion::Word),
        &[],
    );
    assert_eq!(state.selection.end, BufferPoint::new(33, 10));

    state.set_end(
        BufferPoint::new(21, 10),
        Some(&buffer),
        Some(SelectionExpansion::Line),
        &[],
    );
    assert_eq!(state.selection.end, BufferPoint::new(100, 10));

    state.set_end(
        BufferPoint::new(21, 10),
        Some(&buffer),
        Some(SelectionExpansion::Word),
        &[],
    );
    assert_eq!(state.selection.end, BufferPoint::new(33, 10));

    state.set_end(BufferPoint::new(35, 10), Some(&buffer), None, &[]);
    assert_eq!(state.selection.end, BufferPoint::new(45, 10));

    state.set_end(BufferPoint::new(21, 10), Some(&buffer), None, &[]);
    assert_eq!(state.selection.end, BufferPoint::new(33, 10));

    state.set_end(BufferPoint::new(25, 10), Some(&buffer), None, &[]);
    assert_eq!(state.selection.start, BufferPoint::new(4, 10));
    assert_eq!(state.selection.end, BufferPoint::new(33, 10));
}
