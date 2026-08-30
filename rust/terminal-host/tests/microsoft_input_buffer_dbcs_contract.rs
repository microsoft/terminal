use terminal_host::codepage::{CP_JAPANESE, encode_code_unit};
use terminal_host::input_buffer::{InputBuffer, InputEvent, KeyEvent, MouseEvent};

fn key(virtual_key: u16, unicode_char: u16) -> InputEvent {
    InputEvent::Key(KeyEvent::new(true, 1, virtual_key, 0, unicode_char, 0))
}

fn mouse() -> InputEvent {
    InputEvent::Mouse(MouseEvent {
        x: 0,
        y: 0,
        event_flags: 0,
    })
}

#[test]
fn microsoft_input_buffer_reading_dbcs_chars_pads_output_array_contract() {
    assert_eq!(
        encode_code_unit(CP_JAPANESE, 0x3042),
        Some(vec![0x82, 0xa0])
    );

    let mut input_buffer = InputBuffer::new();
    input_buffer.write_bulk([
        mouse(),
        key(u16::from(b'A'), u16::from(b'A')),
        key(0x3042, 0x3042),
        mouse(),
    ]);

    let actual =
        input_buffer.read_with_codepage(5, |code_unit| encode_code_unit(CP_JAPANESE, code_unit));

    let expected = vec![
        mouse(),
        key(u16::from(b'A'), u16::from(b'A')),
        key(0x3042, 0x82),
        key(0x3042, 0xa0),
        mouse(),
    ];

    assert_eq!(actual, expected);
    assert_eq!(input_buffer.ready_event_count(), 0);
}
