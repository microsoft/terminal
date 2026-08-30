use terminal_buffer::line_edit::{
    back_index, delete_cells, delete_columns, forward_index, insert_cells, insert_columns,
};
use terminal_buffer::rect_ops::{ScreenRect, erase_rect, fill_rect};
use terminal_buffer::screen_erase::{EraseType, erase_display, erase_line};
use terminal_buffer::terminal_modes::TerminalModeState;
use terminal_buffer::text_attribute::{TextAttribute, UnderlineStyle};
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};
use terminal_buffer::text_color::Rgb;
use terminal_buffer::vertical_scroll::VerticalScrollState;
use terminal_buffer::viewport_index::{index_down, next_line};

const WIDTH: u16 = 80;
const HEIGHT: u16 = 30;
const VIEWPORT_BOTTOM: u16 = 24;

fn active_attribute() -> TextAttribute {
    let mut attribute = TextAttribute::from_rgb(Rgb::new(12, 34, 56), Rgb::new(78, 90, 12));
    attribute.set_crossed_out(true);
    attribute.set_reverse_video(true);
    attribute.set_underline_style(UnderlineStyle::Single);
    attribute
}

fn fixture() -> TextBuffer {
    let buffer_attribute = TextAttribute::from_rgb(Rgb::new(255, 0, 0), Rgb::new(0, 0, 255));
    let mut buffer = TextBuffer::new(WIDTH, HEIGHT, buffer_attribute).unwrap();
    fill_rect(
        &mut buffer,
        ScreenRect::new(0, 0, WIDTH, HEIGHT),
        u16::from(b'X'),
        buffer_attribute,
    )
    .unwrap();
    buffer
}

fn assert_erased(buffer: &TextBuffer, x: u16, y: u16, expected: TextAttribute) {
    let row = buffer.row(i32::from(y));
    assert_eq!(row.glyph_at(i32::from(x)), &[u16::from(b' ')]);
    assert_eq!(row.attribute_at(i32::from(x)), expected);
}

fn run_operation(operation: usize, erase_source: TextAttribute) -> (TextBuffer, u16, u16) {
    let mut buffer = fixture();

    match operation {
        // LF, VT, FF: identical downward indexing for the cell-grid observable.
        0..=2 => {
            let mut viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_BOTTOM);
            let mut cursor = TextBufferPoint::new(0, VIEWPORT_BOTTOM - 1);
            index_down(&mut buffer, &mut viewport, &mut cursor, erase_source).unwrap();
            (buffer, 0, VIEWPORT_BOTTOM)
        }
        // NEL: index down plus carriage return.
        3 => {
            let mut viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_BOTTOM);
            let mut cursor = TextBufferPoint::new(10, VIEWPORT_BOTTOM - 1);
            next_line(&mut buffer, &mut viewport, &mut cursor, erase_source).unwrap();
            assert_eq!(cursor.x, 0);
            (buffer, 0, VIEWPORT_BOTTOM)
        }
        // IND.
        4 => {
            let mut viewport = ScreenRect::new(0, 0, WIDTH, VIEWPORT_BOTTOM);
            let mut cursor = TextBufferPoint::new(0, VIEWPORT_BOTTOM - 1);
            index_down(&mut buffer, &mut viewport, &mut cursor, erase_source).unwrap();
            (buffer, 0, VIEWPORT_BOTTOM)
        }
        // RI at the top of the viewport scrolls down and reveals row zero.
        5 => {
            let mut state = VerticalScrollState::new(WIDTH, 0, VIEWPORT_BOTTOM);
            state.set_cursor(0, 0);
            state.reverse_index(&mut buffer, erase_source).unwrap();
            (buffer, 0, 0)
        }
        // DECBI at the left edge inserts a column and reveals the leftmost cell.
        6 => {
            back_index(
                &mut buffer,
                0..VIEWPORT_BOTTOM,
                0..WIDTH,
                0,
                1,
                erase_source,
            )
            .unwrap();
            (buffer, 0, 0)
        }
        // DECFI at the right edge deletes the left column and reveals the tail.
        7 => {
            forward_index(
                &mut buffer,
                0..VIEWPORT_BOTTOM,
                0..WIDTH,
                WIDTH - 1,
                1,
                erase_source,
            )
            .unwrap();
            (buffer, WIDTH - 1, 0)
        }
        // DECIC.
        8 => {
            insert_columns(
                &mut buffer,
                0..VIEWPORT_BOTTOM,
                0,
                1,
                0..WIDTH,
                erase_source,
            )
            .unwrap();
            (buffer, 0, 0)
        }
        // DECDC.
        9 => {
            delete_columns(
                &mut buffer,
                0..VIEWPORT_BOTTOM,
                0,
                1,
                0..WIDTH,
                erase_source,
            )
            .unwrap();
            (buffer, WIDTH - 1, 0)
        }
        // ICH.
        10 => {
            insert_cells(buffer.row_mut(0), 0, 1, 0..WIDTH, erase_source).unwrap();
            (buffer, 0, 0)
        }
        // DCH.
        11 => {
            delete_cells(buffer.row_mut(0), 0, 1, 0..WIDTH, erase_source).unwrap();
            (buffer, WIDTH - 1, 0)
        }
        // IL.
        12 => {
            let mut state = VerticalScrollState::new(WIDTH, 0, VIEWPORT_BOTTOM);
            state.set_cursor(0, 0);
            state.insert_lines(&mut buffer, 1, erase_source).unwrap();
            (buffer, 0, 0)
        }
        // DL.
        13 => {
            let mut state = VerticalScrollState::new(WIDTH, 0, VIEWPORT_BOTTOM);
            state.set_cursor(0, 0);
            state.delete_lines(&mut buffer, 1, erase_source).unwrap();
            (buffer, 0, VIEWPORT_BOTTOM - 1)
        }
        // ECH uses the same safe rectangular erase primitive for its one-row span.
        14 => {
            erase_rect(&mut buffer, ScreenRect::new(0, 0, 1, 1), erase_source).unwrap();
            (buffer, 0, 0)
        }
        // EL.
        15 => {
            erase_line(
                &mut buffer,
                TextBufferPoint::new(0, 0),
                EraseType::ToEnd,
                false,
                erase_source,
            )
            .unwrap();
            (buffer, 0, 0)
        }
        // ED.
        16 => {
            erase_display(
                &mut buffer,
                ScreenRect::new(0, 0, WIDTH, VIEWPORT_BOTTOM),
                TextBufferPoint::new(0, 0),
                EraseType::ToEnd,
                false,
                erase_source,
            )
            .unwrap();
            (buffer, 0, 0)
        }
        // DECERA.
        17 => {
            erase_rect(&mut buffer, ScreenRect::new(0, 0, 1, 1), erase_source).unwrap();
            (buffer, 0, 0)
        }
        // SU.
        18 => {
            let state = VerticalScrollState::new(WIDTH, 0, VIEWPORT_BOTTOM);
            state.scroll_up(&mut buffer, 1, erase_source).unwrap();
            (buffer, 0, VIEWPORT_BOTTOM - 1)
        }
        // SD.
        19 => {
            let state = VerticalScrollState::new(WIDTH, 0, VIEWPORT_BOTTOM);
            state.scroll_down(&mut buffer, 1, erase_source).unwrap();
            (buffer, 0, 0)
        }
        _ => unreachable!("Microsoft EraseColorMode has exactly 20 operation vectors"),
    }
}

#[test]
fn microsoft_screen_buffer_erase_color_mode_contract() {
    let active = active_attribute();

    for enabled in [false, true] {
        let mut modes = TerminalModeState::new();
        modes.set_erase_color_mode(enabled);
        assert_eq!(modes.erase_color_mode(), enabled);

        let erase_source = modes.erase_source_attribute(active);
        let mut expected = erase_source;
        expected.set_standard_erase();

        for operation in 0..20 {
            let (buffer, x, y) = run_operation(operation, erase_source);
            assert_erased(&buffer, x, y, expected);
        }
    }
}
