#![allow(clippy::cast_possible_truncation)]

use terminal_host::input_buffer::{
    DEFAULT_INPUT_MODE, InputBuffer, InputEvent, KeyEvent, MouseEvent, ReadOptions, VK_CONTROL,
    VK_PAUSE,
};

const RECORD_INSERT_COUNT: usize = 12;

fn key(ch: u16) -> InputEvent {
    InputEvent::Key(KeyEvent::new(true, 1, ch, 0, ch, 0))
}

fn key_with_repeat(ch: u16, repeat_count: u16) -> InputEvent {
    InputEvent::Key(KeyEvent::new(true, repeat_count, ch, 0, ch, 0))
}

#[test]
fn microsoft_input_buffer_emptying_read_resets_wait_event_contract() {
    let mut buffer = InputBuffer::new();
    let events = (0..RECORD_INSERT_COUNT)
        .map(|index| key(u16::from(b'A') + index as u16))
        .collect::<Vec<_>>();
    assert_eq!(buffer.write_bulk(events), RECORD_INSERT_COUNT);
    buffer.set_wait_signaled(true);
    assert_eq!(buffer.read(1, ReadOptions::NORMAL).len(), 1);
    assert!(buffer.wait_signaled());

    buffer.set_wait_signaled(true);
    assert_eq!(
        buffer
            .read(RECORD_INSERT_COUNT - 1, ReadOptions::NORMAL)
            .len(),
        RECORD_INSERT_COUNT - 1
    );
    assert!(!buffer.wait_signaled());
}

#[test]
fn microsoft_input_buffer_dbcs_padding_portable_contract() {
    let mut buffer = InputBuffer::new();
    let input = vec![
        InputEvent::Mouse(MouseEvent {
            x: 0,
            y: 0,
            event_flags: 0,
        }),
        key(u16::from(b'A')),
        InputEvent::Key(KeyEvent::new(true, 1, 0x3042, 0, 0x3042, 0)),
        InputEvent::Mouse(MouseEvent {
            x: 0,
            y: 0,
            event_flags: 0,
        }),
    ];
    assert_eq!(buffer.write_bulk(input), 4);
    let output =
        buffer.read_with_codepage(5, |value| (value == 0x3042).then_some(vec![0x82, 0xa0]));
    assert_eq!(output.len(), 5);
    assert_eq!(
        output[0],
        InputEvent::Mouse(MouseEvent {
            x: 0,
            y: 0,
            event_flags: 0
        })
    );
    assert_eq!(output[1], key(u16::from(b'A')));
    assert_eq!(
        output[2],
        InputEvent::Key(KeyEvent::new(true, 1, 0x3042, 0, 0x82, 0))
    );
    assert_eq!(
        output[3],
        InputEvent::Key(KeyEvent::new(true, 1, 0x3042, 0, 0xa0, 0))
    );
    assert_eq!(
        output[4],
        InputEvent::Mouse(MouseEvent {
            x: 0,
            y: 0,
            event_flags: 0
        })
    );
}

#[test]
fn microsoft_input_buffer_can_prepend_events_contract() {
    let mut buffer = InputBuffer::new();
    let original = (0..RECORD_INSERT_COUNT)
        .map(|index| key(u16::from(b'A') + index as u16))
        .collect::<Vec<_>>();
    let prepended = (0..RECORD_INSERT_COUNT)
        .map(|index| key(u16::from(b'a') + index as u16))
        .collect::<Vec<_>>();
    assert_eq!(buffer.write_bulk(original.clone()), RECORD_INSERT_COUNT);
    assert_eq!(buffer.prepend(prepended.clone()), RECORD_INSERT_COUNT);
    assert_eq!(
        buffer.read(RECORD_INSERT_COUNT, ReadOptions::NORMAL),
        prepended
    );
    assert_eq!(buffer.ready_event_count(), RECORD_INSERT_COUNT);
    assert_eq!(
        buffer.read(RECORD_INSERT_COUNT, ReadOptions::NORMAL),
        original
    );
    assert_eq!(buffer.ready_event_count(), 0);
}

#[test]
fn microsoft_input_buffer_can_reinitialize_contract() {
    let mut buffer = InputBuffer::new();
    let original_mode = buffer.input_mode();
    assert_eq!(original_mode, DEFAULT_INPUT_MODE);
    assert_eq!(buffer.write(InputEvent::Menu), 1);
    buffer.set_input_mode(0);
    buffer.reinitialize();
    assert_eq!(buffer.input_mode(), original_mode);
    assert_eq!(buffer.ready_event_count(), 0);
}

#[test]
fn microsoft_input_buffer_suspension_removes_pause_keys_contract() {
    let mut buffer = InputBuffer::new();
    assert!(!buffer.output_suspended());
    assert_eq!(
        buffer.write(InputEvent::Key(KeyEvent::new(true, 1, VK_PAUSE, 0, 0, 0))),
        0
    );
    assert!(buffer.output_suspended());
    assert_eq!(buffer.ready_event_count(), 0);

    assert_eq!(buffer.write(key(u16::from(b'a'))), 0);
    assert!(!buffer.output_suspended());
    assert_eq!(buffer.ready_event_count(), 0);
}

#[test]
fn microsoft_input_buffer_system_keys_do_not_unpause_contract() {
    let mut buffer = InputBuffer::new();
    assert_eq!(
        buffer.write(InputEvent::Key(KeyEvent::new(true, 1, VK_PAUSE, 0, 0, 0))),
        0
    );
    assert!(buffer.output_suspended());
    assert_eq!(
        buffer.write(InputEvent::Key(KeyEvent::new(true, 1, VK_CONTROL, 0, 0, 0))),
        1
    );
    assert!(buffer.output_suspended());
    assert_eq!(buffer.ready_event_count(), 1);
    assert_eq!(buffer.read(2, ReadOptions::PEEK).len(), 1);
}

#[test]
fn microsoft_input_buffer_stream_reading_decoalesces_contract() {
    let mut buffer = InputBuffer::new();
    assert_eq!(buffer.write(key_with_repeat(u16::from(b'a'), 5)), 1);
    let output = buffer.read(1, ReadOptions::STREAM);
    assert_eq!(output, vec![key_with_repeat(u16::from(b'a'), 1)]);
    assert_eq!(buffer.ready_event_count(), 1);
    let InputEvent::Key(stored) = buffer.events().front().expect("stored key") else {
        panic!("expected key")
    };
    assert_eq!(stored.repeat_count, 4);
}

#[test]
fn microsoft_input_buffer_stream_peeking_decoalesces_contract() {
    let mut buffer = InputBuffer::new();
    assert_eq!(buffer.write(key_with_repeat(u16::from(b'a'), 5)), 1);
    let output = buffer.read(1, ReadOptions::STREAM_PEEK);
    assert_eq!(output, vec![key_with_repeat(u16::from(b'a'), 1)]);
    assert_eq!(buffer.ready_event_count(), 1);
    let InputEvent::Key(stored) = buffer.events().front().expect("stored key") else {
        panic!("expected key")
    };
    assert_eq!(stored.repeat_count, 5);
}
