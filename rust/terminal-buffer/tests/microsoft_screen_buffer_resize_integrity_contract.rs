use terminal_buffer::alternate_buffer::{CursorShape, CursorState};
use terminal_buffer::resize_integrity::ResizeIntegrityState;
use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::TextBuffer;
use terminal_buffer::text_color::TextColor;

const WIDTH: u16 = 80;
const HEIGHT: u16 = 40;

fn fixture() -> ResizeIntegrityState {
    let fill = TextAttribute::default();
    let mut tagged = fill;
    tagged.set_background(TextColor::index16(TextColor::DARK_BLUE));

    let mut buffer = TextBuffer::new(WIDTH, HEIGHT, fill).expect("valid Microsoft resize fixture");
    buffer
        .row_mut(0)
        .replace_attributes(0, i32::from(WIDTH), tagged);

    let cursor = CursorState {
        x: 7,
        y: 5,
        visible: false,
        size: 66,
        shape: CursorShape::EmptyBox,
        blinking: false,
    };

    ResizeIntegrityState::new(buffer, cursor, fill)
}

#[test]
fn microsoft_screen_buffer_resize_traditional_does_not_double_free_attr_rows_contract() {
    let mut state = fixture();
    let expected_cursor = state.cursor();
    let expected_attribute = state.buffer().row(0).attribute_at(0);

    state
        .resize_screen_buffer(WIDTH, HEIGHT - 1, false)
        .expect("traditional one-row shrink completes without ownership failure");

    assert_eq!(state.buffer().width(), WIDTH);
    assert_eq!(state.buffer().height(), HEIGHT - 1);
    assert_eq!(state.buffer().row(0).attribute_at(0), expected_attribute);
    assert_eq!(state.cursor(), expected_cursor);
}

#[test]
fn microsoft_screen_buffer_resize_cursor_unchanged_contract() {
    for use_reflow in [false, true] {
        for dx in [-10_i32, -1, 0, 1, 10] {
            for dy in [-10_i32, -1, 0, 1, 10] {
                let mut state = fixture();
                let initial_cursor = state.cursor();
                let expected_width =
                    u16::try_from(i32::from(WIDTH) + dx).expect("Microsoft width stays positive");
                let expected_height =
                    u16::try_from(i32::from(HEIGHT) + dy).expect("Microsoft height stays positive");

                state
                    .resize_screen_buffer(expected_width, expected_height, use_reflow)
                    .expect("Microsoft resize matrix vector succeeds");

                assert_eq!(state.buffer().width(), expected_width);
                assert_eq!(state.buffer().height(), expected_height);
                assert_eq!(state.cursor().shape, initial_cursor.shape);
                assert_eq!(state.cursor().size, initial_cursor.size);
                assert_eq!(state.cursor(), initial_cursor);
            }
        }
    }
}
