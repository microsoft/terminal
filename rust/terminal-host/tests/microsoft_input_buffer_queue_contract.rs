#![allow(clippy::cast_possible_truncation, clippy::cast_possible_wrap)]

use terminal_host::input_buffer::{
    InputBuffer, InputEvent, KeyEvent, MOUSE_MOVED, MouseEvent, ReadOptions,
};

const RECORD_INSERT_COUNT: usize = 12;

fn key(ch: u16) -> InputEvent {
    InputEvent::Key(KeyEvent::new(true, 1, ch, 0, ch, 0))
}

fn menu() -> InputEvent {
    InputEvent::Menu
}

#[test]
fn microsoft_input_buffer_can_get_number_of_ready_events_contract() {
    let mut buffer = InputBuffer::new();
    assert_eq!(buffer.write(key(u16::from(b'a'))), 1);
    assert_eq!(buffer.ready_event_count(), 1);
    assert_eq!(buffer.write(menu()), 1);
    assert_eq!(buffer.ready_event_count(), 2);
}

#[test]
fn microsoft_input_buffer_can_insert_individually_contract() {
    let mut buffer = InputBuffer::new();
    for _ in 0..RECORD_INSERT_COUNT {
        assert_eq!(buffer.write(menu()), 1);
        assert_eq!(buffer.events().back(), Some(&menu()));
    }
    assert_eq!(buffer.ready_event_count(), RECORD_INSERT_COUNT);
}

#[test]
fn microsoft_input_buffer_can_bulk_insert_contract() {
    let mut buffer = InputBuffer::new();
    assert_eq!(
        buffer.write_bulk(vec![menu(); RECORD_INSERT_COUNT]),
        RECORD_INSERT_COUNT
    );
    assert_eq!(buffer.ready_event_count(), RECORD_INSERT_COUNT);
    assert!(buffer.events().iter().all(|event| event == &menu()));
}

#[test]
fn microsoft_input_buffer_coalesces_mouse_events_contract() {
    let mut buffer = InputBuffer::new();
    for index in 0..RECORD_INSERT_COUNT {
        assert_eq!(
            buffer.write(InputEvent::Mouse(MouseEvent {
                x: (index + 1) as i16,
                y: ((index + 1) * 2) as i16,
                event_flags: MOUSE_MOVED,
            })),
            1
        );
    }
    assert_eq!(buffer.ready_event_count(), 1);
    assert_eq!(
        buffer.events().front(),
        Some(&InputEvent::Mouse(MouseEvent {
            x: RECORD_INSERT_COUNT as i16,
            y: (RECORD_INSERT_COUNT * 2) as i16,
            event_flags: MOUSE_MOVED,
        }))
    );

    assert_eq!(buffer.write(key(0)), 1);
    assert_eq!(
        buffer.write(InputEvent::Mouse(MouseEvent {
            x: RECORD_INSERT_COUNT as i16,
            y: (RECORD_INSERT_COUNT * 2) as i16,
            event_flags: MOUSE_MOVED,
        })),
        1
    );
    assert_eq!(buffer.ready_event_count(), 3);
}

#[test]
fn microsoft_input_buffer_does_not_coalesce_bulk_mouse_events_contract() {
    let mut buffer = InputBuffer::new();
    let mouse = InputEvent::Mouse(MouseEvent {
        x: 0,
        y: 0,
        event_flags: MOUSE_MOVED,
    });
    assert_eq!(buffer.write(mouse.clone()), 1);
    assert_eq!(
        buffer.write_bulk(vec![mouse.clone(); RECORD_INSERT_COUNT]),
        RECORD_INSERT_COUNT
    );
    assert_eq!(buffer.ready_event_count(), RECORD_INSERT_COUNT + 1);
    assert!(buffer.events().iter().all(|event| event == &mouse));
}

#[test]
fn microsoft_input_buffer_coalesces_key_events_contract() {
    let mut buffer = InputBuffer::new();
    let event = key(u16::from(b'a'));
    for _ in 0..RECORD_INSERT_COUNT {
        assert_eq!(buffer.write(event.clone()), 1);
    }
    assert_eq!(buffer.ready_event_count(), 1);
    let output = buffer.read(1, ReadOptions::PEEK);
    assert_eq!(output.len(), 1);
    let InputEvent::Key(key) = &output[0] else {
        panic!("expected key event")
    };
    assert_eq!(usize::from(key.repeat_count), RECORD_INSERT_COUNT);
}

#[test]
fn microsoft_input_buffer_does_not_coalesce_bulk_key_events_contract() {
    let mut buffer = InputBuffer::new();
    let event = key(u16::from(b'a'));
    assert_eq!(buffer.write(event.clone()), 1);
    assert_eq!(
        buffer.write_bulk(vec![event.clone(); RECORD_INSERT_COUNT]),
        RECORD_INSERT_COUNT
    );
    assert_eq!(buffer.ready_event_count(), RECORD_INSERT_COUNT + 1);
    assert!(buffer.events().iter().all(|stored| stored == &event));
}

#[test]
fn microsoft_input_buffer_does_not_coalesce_surrogate_pairs_contract() {
    let mut buffer = InputBuffer::new();
    assert_eq!(buffer.write(key(0xd83d)), 1);
    assert_eq!(buffer.write(key(0xdc4d)), 1);
    assert_eq!(buffer.write(key(0xdc4d)), 1);
    assert_eq!(buffer.ready_event_count(), 3);
}

#[test]
fn microsoft_input_buffer_can_flush_all_output_contract() {
    let mut buffer = InputBuffer::new();
    assert_eq!(
        buffer.write_bulk(vec![menu(); RECORD_INSERT_COUNT]),
        RECORD_INSERT_COUNT
    );
    buffer.flush();
    assert_eq!(buffer.ready_event_count(), 0);
}

#[test]
fn microsoft_input_buffer_can_flush_all_but_keys_contract() {
    let mut buffer = InputBuffer::new();
    let events = (0..RECORD_INSERT_COUNT)
        .map(|index| if index % 2 == 0 { menu() } else { key(0) })
        .collect::<Vec<_>>();
    assert_eq!(buffer.write_bulk(events), RECORD_INSERT_COUNT);
    buffer.flush_all_but_keys();
    assert_eq!(buffer.ready_event_count(), RECORD_INSERT_COUNT / 2);
    let output = buffer.read(RECORD_INSERT_COUNT / 2, ReadOptions::NORMAL);
    assert!(
        output
            .iter()
            .all(|event| matches!(event, InputEvent::Key(_)))
    );
}

#[test]
fn microsoft_input_buffer_can_read_input_contract() {
    let mut buffer = InputBuffer::new();
    let events = (0..RECORD_INSERT_COUNT)
        .map(|index| key(u16::from(b'A') + index as u16))
        .collect::<Vec<_>>();
    assert_eq!(buffer.write_bulk(events.clone()), RECORD_INSERT_COUNT);
    assert_eq!(
        buffer.read(RECORD_INSERT_COUNT, ReadOptions::NORMAL),
        events
    );
    assert_eq!(buffer.ready_event_count(), 0);
}

#[test]
fn microsoft_input_buffer_can_peek_at_events_contract() {
    let mut buffer = InputBuffer::new();
    let events = (0..RECORD_INSERT_COUNT)
        .map(|index| key(u16::from(b'A') + index as u16))
        .collect::<Vec<_>>();
    assert_eq!(buffer.write_bulk(events.clone()), RECORD_INSERT_COUNT);
    assert_eq!(buffer.read(RECORD_INSERT_COUNT, ReadOptions::PEEK), events);
    assert_eq!(buffer.ready_event_count(), RECORD_INSERT_COUNT);
}
