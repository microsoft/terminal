use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::TextBuffer;
use terminal_core::selection::{BufferPoint, SelectionExpansion, SelectionInfo, SelectionState};
use terminal_core::selection_rendering::{SelectionSpan, selection_spans};

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
fn microsoft_terminal_core_select_unit_matches_single_cell_anchor_contract() {
    let buffer = buffer(100, 100);
    let mut state = SelectionState::default();

    state.set_anchor(BufferPoint::new(5, 10));

    assert_eq!(state.selection.start, BufferPoint::new(5, 10));
    assert_eq!(state.selection.end, BufferPoint::new(5, 10));
    assert_eq!(
        selection_spans(&buffer, &state.selection),
        [SelectionSpan::new(
            BufferPoint::new(5, 10),
            BufferPoint::new(5, 10),
        )]
    );
}

#[test]
fn microsoft_terminal_core_select_area_matches_linear_selection_contract() {
    let buffer = buffer(100, 100);
    let mut state = SelectionState::default();
    state.set_anchor(BufferPoint::new(5, 10));

    state.set_end(BufferPoint::new(15, 20), Some(&buffer), None, &[]);

    assert_eq!(
        selection_spans(&buffer, &state.selection),
        [SelectionSpan::new(
            BufferPoint::new(5, 10),
            BufferPoint::new(15, 20),
        )]
    );
}

#[test]
fn microsoft_terminal_core_select_box_area_matches_one_span_per_row_contract() {
    let buffer = buffer(100, 100);
    let mut state = SelectionState::default();
    state.set_anchor(BufferPoint::new(5, 10));
    state.selection.set_block_selection(true);
    state.set_end(BufferPoint::new(15, 20), Some(&buffer), None, &[]);

    let spans = selection_spans(&buffer, &state.selection);
    assert_eq!(spans.len(), 11);
    for (offset, span) in spans.iter().enumerate() {
        let y = 10 + i32::try_from(offset).expect("small row offset");
        assert_eq!(
            *span,
            SelectionSpan::new(BufferPoint::new(5, y), BufferPoint::new(15, y))
        );
    }
}

#[test]
fn microsoft_terminal_core_wide_glyph_leading_anchor_stays_degenerate() {
    let mut buffer = buffer(100, 100);
    buffer
        .row_mut(10)
        .replace_glyph(4, 2, &[0xd83c, 0xdf2f])
        .expect("wide burrito glyph fits");
    let mut state = SelectionState::default();

    state.set_anchor(BufferPoint::new(4, 10));

    assert_eq!(
        selection_spans(&buffer, &state.selection),
        [SelectionSpan::new(
            BufferPoint::new(4, 10),
            BufferPoint::new(4, 10),
        )]
    );
}

#[test]
fn microsoft_terminal_core_double_click_general_case_selects_complete_word() {
    let mut buffer = buffer(100, 100);
    write_ascii(&mut buffer, 10, 4, b"doubleClickMe");
    let mut state = SelectionState::default();

    state.multi_click(
        &buffer,
        BufferPoint::new(5, 10),
        SelectionExpansion::Word,
        &[u16::from(b':'), u16::from(b'>')],
    );

    assert_eq!(state.selection.start, BufferPoint::new(4, 10));
    assert_eq!(state.selection.end, BufferPoint::new(17, 10));
}

#[test]
fn microsoft_terminal_core_double_click_delimiter_selects_empty_row_class() {
    let buffer = buffer(100, 100);
    let mut state = SelectionState::default();

    state.multi_click(
        &buffer,
        BufferPoint::new(5, 10),
        SelectionExpansion::Word,
        &[u16::from(b':'), u16::from(b'>')],
    );

    assert_eq!(state.selection.start, BufferPoint::new(0, 10));
    assert_eq!(state.selection.end, BufferPoint::new(100, 10));
}

#[test]
fn microsoft_terminal_core_double_click_delimiter_class_isolated_cell_contract() {
    let mut buffer = buffer(100, 100);
    write_ascii(&mut buffer, 10, 4, b"C:\\Terminal>");
    let mut state = SelectionState::default();

    state.multi_click(
        &buffer,
        BufferPoint::new(15, 10),
        SelectionExpansion::Word,
        &[u16::from(b':'), u16::from(b'>')],
    );

    assert_eq!(state.selection.start, BufferPoint::new(15, 10));
    assert_eq!(state.selection.end, BufferPoint::new(16, 10));
}

#[test]
fn microsoft_terminal_core_triple_click_wrapped_line_expands_full_logical_line() {
    let mut buffer = buffer(10, 5);
    write_ascii(&mut buffer, 0, 0, b"ABCDEFGHIJ");
    write_ascii(&mut buffer, 1, 0, b"KLMNOPQRST");
    write_ascii(&mut buffer, 2, 0, b"UVWXYZ");
    buffer.row_mut(0).set_wrap_forced(true);
    buffer.row_mut(1).set_wrap_forced(true);
    let mut state = SelectionState::default();

    state.multi_click(
        &buffer,
        BufferPoint::new(5, 1),
        SelectionExpansion::Line,
        &[],
    );

    assert_eq!(state.selection.start, BufferPoint::new(0, 0));
    assert_eq!(state.selection.end, BufferPoint::new(10, 2));
}

#[test]
fn microsoft_terminal_core_pivot_contract_preserves_anchor_across_drag_and_shift_click() {
    let buffer = buffer(100, 100);
    let mut state = SelectionState {
        selection: SelectionInfo {
            start: BufferPoint::new(10, 10),
            end: BufferPoint::new(21, 10),
            pivot: BufferPoint::new(10, 10),
            block_selection: false,
            active: true,
        },
        expansion: SelectionExpansion::Char,
        ..SelectionState::default()
    };

    state.set_end(BufferPoint::new(5, 10), None, None, &[]);
    assert_eq!(state.selection.start, BufferPoint::new(5, 10));
    assert_eq!(state.selection.end, BufferPoint::new(10, 10));

    state.set_end(BufferPoint::new(20, 10), None, None, &[]);
    assert_eq!(state.selection.start, BufferPoint::new(10, 10));
    assert_eq!(state.selection.end, BufferPoint::new(20, 10));

    state.set_end(
        BufferPoint::new(5, 10),
        Some(&buffer),
        Some(SelectionExpansion::Char),
        &[],
    );
    assert_eq!(state.selection.start, BufferPoint::new(5, 10));
    assert_eq!(state.selection.end, BufferPoint::new(10, 10));

    state.set_end(
        BufferPoint::new(20, 10),
        Some(&buffer),
        Some(SelectionExpansion::Char),
        &[],
    );
    assert_eq!(state.selection.start, BufferPoint::new(10, 10));
    assert_eq!(state.selection.end, BufferPoint::new(21, 10));
}
