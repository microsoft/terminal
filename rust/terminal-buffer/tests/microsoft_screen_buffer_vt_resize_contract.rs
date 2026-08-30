use terminal_buffer::alternate_buffer::ViewportSize;
use terminal_buffer::text_attribute::{TextAttribute, UnderlineStyle};
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};
use terminal_buffer::text_color::{Rgb, TextColor};
use terminal_buffer::vt_resize::VtResizeState;

const BUFFER_HEIGHT: u16 = 300;
const INITIAL_WIDTH: u16 = 80;
const INITIAL_VIEW_HEIGHT: u16 = 25;

fn state() -> VtResizeState {
    let attribute = TextAttribute::default();
    let buffer =
        TextBuffer::new(INITIAL_WIDTH, BUFFER_HEIGHT, attribute).expect("valid host fixture");
    VtResizeState::new(
        buffer,
        ViewportSize::new(INITIAL_WIDTH, INITIAL_VIEW_HEIGHT),
        attribute,
    )
}

fn microsoft_extended_attribute() -> TextAttribute {
    let mut attribute = TextAttribute::from_rgb(Rgb::new(12, 34, 56), Rgb::new(78, 90, 12));
    attribute.set_underline_color(TextColor::rgb(188, 20, 24));
    attribute.set_crossed_out(true);
    attribute.set_underline_style(UnderlineStyle::Curly);
    attribute.set_italic(true);
    attribute
}

#[test]
fn microsoft_screen_buffer_vt_resize_contract() {
    let mut state = state();
    let initial_buffer_height = state.buffer().height();

    for (rows, columns) in [(30, 80), (40, 80), (40, 90), (12, 12)] {
        assert!(
            state
                .resize_window(rows, columns)
                .expect("Microsoft CSI 8 t vector succeeds")
        );
        assert_eq!(state.buffer().height(), initial_buffer_height);
        assert_eq!(state.buffer().width(), columns);
        assert_eq!(state.viewport(), ViewportSize::new(columns, rows));
    }

    let before_zero_resize = state.clone();
    assert!(
        !state
            .resize_window(0, 0)
            .expect("zero-dimension CSI 8 t is a no-op")
    );
    assert_eq!(state, before_zero_resize);
}

#[test]
fn microsoft_screen_buffer_vt_resize_comprehensive_contract() {
    for dx in [-10_i32, -1, 0, 1, 10] {
        for dy in [-10_i32, -1, 0, 1, 10] {
            let mut state = state();
            let initial = state.viewport();
            let expected_width = u16::try_from(i32::from(initial.width) + dx)
                .expect("Microsoft width stays positive");
            let expected_height = u16::try_from(i32::from(initial.height) + dy)
                .expect("Microsoft height stays positive");

            assert!(
                state
                    .resize_window(expected_height, expected_width)
                    .expect("Microsoft comprehensive CSI 8 t vector succeeds")
            );
            assert_eq!(
                state.viewport(),
                ViewportSize::new(expected_width, expected_height)
            );
            assert_eq!(state.buffer().width(), expected_width);
            assert_eq!(state.buffer().height(), BUFFER_HEIGHT);
        }
    }
}

#[test]
fn microsoft_screen_buffer_vt_resize_deccolm_contract() {
    let mut state = state();
    state.set_vertical_margins(4, 14);
    state.set_cursor_relative(45, 9);

    let initial_viewport = state.viewport();
    let initial_cursor = state.cursor();
    let initial_margins = state.margins();
    let initial_buffer_width = state.buffer().width();

    assert!(!state.allow_deccolm());
    assert!(
        !state
            .set_deccolm(true)
            .expect("disabled DECCOLM is a no-op")
    );
    assert_eq!(state.viewport(), initial_viewport);
    assert_eq!(state.cursor(), initial_cursor);
    assert_eq!(state.margins(), initial_margins);
    assert_eq!(state.buffer().width(), initial_buffer_width);

    state.set_vertical_margins(4, 14);
    state.set_cursor_relative(45, 9);
    state.set_allow_deccolm(true);
    assert!(
        state
            .set_deccolm(true)
            .expect("enabled DECCOLM selects 132 columns")
    );
    assert_eq!(state.buffer().height(), BUFFER_HEIGHT);
    assert_eq!(state.buffer().width(), 132);
    assert_eq!(
        state.viewport(),
        ViewportSize::new(132, INITIAL_VIEW_HEIGHT)
    );
    assert_eq!(state.margins(), None);
    assert_eq!(state.cursor(), TextBufferPoint::new(0, 0));

    state.set_vertical_margins(4, 14);
    state.set_cursor_relative(45, 9);
    let disabled_cursor = state.cursor();
    let disabled_margins = state.margins();
    let disabled_viewport = state.viewport();
    state.set_allow_deccolm(false);
    assert!(
        !state
            .set_deccolm(false)
            .expect("disallowed DECCOLM reset is a no-op")
    );
    assert_eq!(state.cursor(), disabled_cursor);
    assert_eq!(state.margins(), disabled_margins);
    assert_eq!(state.viewport(), disabled_viewport);
    assert_eq!(state.buffer().width(), 132);

    state.set_vertical_margins(4, 14);
    state.set_cursor_relative(45, 9);
    state.set_allow_deccolm(true);
    assert!(
        state
            .set_deccolm(false)
            .expect("enabled DECCOLM reset selects 80 columns")
    );
    assert_eq!(state.buffer().height(), BUFFER_HEIGHT);
    assert_eq!(state.buffer().width(), 80);
    assert_eq!(state.viewport(), ViewportSize::new(80, INITIAL_VIEW_HEIGHT));
    assert_eq!(state.margins(), None);
    assert_eq!(state.cursor(), TextBufferPoint::new(0, 0));
}

#[test]
fn microsoft_screen_buffer_vt_resize_preserving_attributes_contract() {
    let expected = microsoft_extended_attribute();

    let mut csi_resize = state();
    csi_resize.set_current_attribute(expected);
    csi_resize
        .resize_window(24, 132)
        .expect("CSI 8 t grows to 132 columns");
    assert_eq!(csi_resize.buffer().width(), 132);
    csi_resize
        .resize_window(24, 80)
        .expect("CSI 8 t returns to 80 columns");
    assert_eq!(csi_resize.buffer().width(), 80);
    assert_eq!(csi_resize.current_attribute(), expected);

    let mut deccolm_resize = state();
    deccolm_resize.set_current_attribute(expected);
    deccolm_resize.set_allow_deccolm(true);
    deccolm_resize
        .set_deccolm(true)
        .expect("DECCOLM grows to 132 columns");
    assert_eq!(deccolm_resize.buffer().width(), 132);
    deccolm_resize
        .set_deccolm(false)
        .expect("DECCOLM returns to 80 columns");
    assert_eq!(deccolm_resize.buffer().width(), 80);
    assert_eq!(deccolm_resize.current_attribute(), expected);
}
