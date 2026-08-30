#![allow(
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::too_many_lines
)]

use terminal_buffer::geometry::{InclusiveRect, Point};
use terminal_host::api_routines::{
    ConsoleCodePage, ConsoleScreenBuffer, ConsoleTitleState, ConsoleWriteStatus, ConsoleWriter,
    LegacyCell,
};

const DEFAULT_ATTR: u16 = 0x0007;
const FOREGROUND_BLUE: u16 = 0x0001;
const FOREGROUND_GREEN: u16 = 0x0002;
const FOREGROUND_RED: u16 = 0x0004;

fn ascii_encode(units: &[u16]) -> Vec<u8> {
    units
        .iter()
        .map(|unit| u8::try_from(*unit).expect("Microsoft title vector is ASCII"))
        .collect()
}

#[test]
fn microsoft_api_get_console_title_a_contract() {
    let expected = "Test window title.";
    let mut state = ConsoleTitleState::default();
    state.set_title(expected);

    let result = state.get_console_title_a(260);
    assert_ne!(result.written, 0);
    assert_eq!(result.written, expected.len() + 1);
    assert_eq!(result.needed, expected.encode_utf16().count());
    assert_eq!(&result.data[..expected.len()], expected.as_bytes());
    assert_eq!(result.data.last(), Some(&0));
}

#[test]
fn microsoft_api_get_console_title_w_contract() {
    let expected = "Test window title.";
    let expected_utf16: Vec<u16> = expected.encode_utf16().collect();
    let mut state = ConsoleTitleState::default();
    state.set_title(expected);

    let result = state.get_console_title_w(260);
    assert_ne!(result.written, 0);
    assert_eq!(result.written, expected_utf16.len());
    assert_eq!(result.needed, expected_utf16.len());
    assert_eq!(&result.data[..expected_utf16.len()], expected_utf16);
    assert_eq!(result.data.last(), Some(&0));
}

#[test]
fn microsoft_api_get_console_original_title_a_contract() {
    let expected = "Test original window title.";
    let mut state = ConsoleTitleState::default();
    state.set_original_title(expected);

    let result = state.get_console_original_title_a(260, ascii_encode);
    assert_ne!(result.written, 0);
    assert_eq!(result.written, expected.len() + 1);
    assert_eq!(result.needed, expected.encode_utf16().count());
    assert_eq!(&result.data[..expected.len()], expected.as_bytes());
    assert_eq!(result.data.last(), Some(&0));
}

#[test]
fn microsoft_api_get_console_original_title_w_contract() {
    let expected = "Test original window title.";
    let expected_utf16: Vec<u16> = expected.encode_utf16().collect();
    let mut state = ConsoleTitleState::default();
    state.set_original_title(expected);

    let result = state.get_console_original_title_w(260);
    assert_ne!(result.written, 0);
    assert_eq!(result.written, expected_utf16.len());
    assert_eq!(result.needed, expected_utf16.len());
    assert_eq!(&result.data[..expected_utf16.len()], expected_utf16);
    assert_eq!(result.data.last(), Some(&0));
}

#[test]
fn microsoft_api_write_console_a_contract() {
    let vectors: &[(ConsoleCodePage, &[u8], &str)] = &[
        (ConsoleCodePage::Usa437, b"Test Text", "Test Text"),
        (ConsoleCodePage::Japanese932, b"J\x82\xa0\x82\xa2", "Jあい"),
        (
            ConsoleCodePage::Utf8,
            b"Test \xe3\x82\xab Text",
            "Test カ Text",
        ),
    ];

    for wait in [false, true] {
        for &(code_page, bytes, expected) in vectors {
            for increment in [0_usize, 1, 2] {
                let mut writer = ConsoleWriter::new(code_page);
                writer.set_wait(wait);
                let step = if increment == 0 {
                    bytes.len()
                } else {
                    increment
                };
                let mut offset = 0;
                while offset < bytes.len() {
                    let end = (offset + step).min(bytes.len());
                    let chunk = &bytes[offset..end];
                    let result = writer.write_console_a(chunk);
                    if wait {
                        assert_eq!(result.status, ConsoleWriteStatus::Wait);
                    } else {
                        assert_eq!(result.status, ConsoleWriteStatus::Success);
                        assert_eq!(result.consumed, chunk.len());
                    }
                    offset = end;
                }

                if wait {
                    assert!(writer.output().is_empty());
                } else {
                    assert_eq!(writer.output(), expected.encode_utf16().collect::<Vec<_>>());
                    assert_eq!(writer.pending_byte_count(), 0);
                }
            }
        }
    }
}

#[test]
fn microsoft_api_write_console_w_contract() {
    let text: Vec<u16> = "Test text".encode_utf16().collect();
    for wait in [false, true] {
        let mut writer = ConsoleWriter::new(ConsoleCodePage::Utf8);
        writer.set_wait(wait);
        let result = writer.write_console_w(&text);
        if wait {
            assert_eq!(result.status, ConsoleWriteStatus::Wait);
            assert!(writer.output().is_empty());
        } else {
            assert_eq!(result.status, ConsoleWriteStatus::Success);
            assert_eq!(result.consumed, text.len());
            assert_eq!(writer.output(), text);
        }
    }
}

fn cell(character: char, attributes: u16) -> LegacyCell {
    LegacyCell::new(character as u16, attributes)
}

fn fill_background(buffer: &mut ConsoleScreenBuffer) {
    buffer
        .fill_all(cell('Z', FOREGROUND_GREEN))
        .expect("background fill fits");
}

fn expected_attribute(character: char) -> u16 {
    match character {
        'Z' => FOREGROUND_GREEN,
        'A' => FOREGROUND_RED,
        'B' => FOREGROUND_BLUE,
        ' ' => DEFAULT_ATTR,
        other => panic!("unexpected expected-grid character {other}"),
    }
}

fn assert_pattern(buffer: &ConsoleScreenBuffer, rows: &[&str; 5]) {
    for (y, row) in rows.iter().enumerate() {
        assert_eq!(row.chars().count(), 5);
        for (x, character) in row.chars().enumerate() {
            let actual = buffer
                .cell(Point::new(x as i32, y as i32))
                .expect("expected point is inside 5x5 buffer");
            assert_eq!(
                actual,
                cell(character, expected_attribute(character)),
                "cell mismatch at ({x},{y})"
            );
        }
    }
}

fn full_buffer_rect() -> InclusiveRect {
    InclusiveRect::new(0, 0, 4, 4)
}

#[test]
fn microsoft_api_scroll_console_screen_buffer_w_contract() {
    let fill = cell('A', FOREGROUND_RED);
    let scroll_rect = InclusiveRect::new(1, 1, 2, 2);

    for set_margins in [false, true] {
        for check_clipped in [false, true] {
            let mut buffer =
                ConsoleScreenBuffer::new(5, 5, DEFAULT_ATTR).expect("valid 5x5 screen buffer");
            buffer.set_vertical_margins(set_margins.then_some((1, 3)));
            assert_eq!(
                buffer.vertical_margins(),
                set_margins.then_some((1, 3)),
                "the VT margin state is present but must not affect the Win32 scroll API"
            );

            let vertical_clip = check_clipped.then_some(InclusiveRect::new(0, 0, 1, 4));
            let horizontal_clip = check_clipped.then_some(InclusiveRect::new(0, 0, 4, 1));

            fill_background(&mut buffer);
            buffer
                .scroll_console_screen_buffer(
                    full_buffer_rect(),
                    Point::new(0, -2),
                    vertical_clip,
                    fill,
                )
                .unwrap();
            if check_clipped {
                assert_pattern(&buffer, &["ZZZZZ", "ZZZZZ", "ZZZZZ", "AAZZZ", "AAZZZ"]);
            } else {
                assert_pattern(&buffer, &["ZZZZZ", "ZZZZZ", "ZZZZZ", "AAAAA", "AAAAA"]);
            }

            fill_background(&mut buffer);
            buffer
                .scroll_console_screen_buffer(
                    full_buffer_rect(),
                    Point::new(0, 2),
                    vertical_clip,
                    fill,
                )
                .unwrap();
            if check_clipped {
                assert_pattern(&buffer, &["AAZZZ", "AAZZZ", "ZZZZZ", "ZZZZZ", "ZZZZZ"]);
            } else {
                assert_pattern(&buffer, &["AAAAA", "AAAAA", "ZZZZZ", "ZZZZZ", "ZZZZZ"]);
            }

            fill_background(&mut buffer);
            buffer
                .scroll_console_screen_buffer(
                    full_buffer_rect(),
                    Point::new(-2, 0),
                    horizontal_clip,
                    fill,
                )
                .unwrap();
            if check_clipped {
                assert_pattern(&buffer, &["ZZZAA", "ZZZAA", "ZZZZZ", "ZZZZZ", "ZZZZZ"]);
            } else {
                assert_pattern(&buffer, &["ZZZAA", "ZZZAA", "ZZZAA", "ZZZAA", "ZZZAA"]);
            }

            fill_background(&mut buffer);
            buffer
                .scroll_console_screen_buffer(
                    full_buffer_rect(),
                    Point::new(2, 0),
                    horizontal_clip,
                    fill,
                )
                .unwrap();
            if check_clipped {
                assert_pattern(&buffer, &["AAZZZ", "AAZZZ", "ZZZZZ", "ZZZZZ", "ZZZZZ"]);
            } else {
                assert_pattern(&buffer, &["AAZZZ", "AAZZZ", "AAZZZ", "AAZZZ", "AAZZZ"]);
            }

            fill_background(&mut buffer);
            let down_right_clip = check_clipped.then_some(InclusiveRect::new(1, 1, 4, 4));
            buffer
                .scroll_console_screen_buffer(
                    full_buffer_rect(),
                    Point::new(2, 2),
                    down_right_clip,
                    fill,
                )
                .unwrap();
            if check_clipped {
                assert_pattern(&buffer, &["ZZZZZ", "ZAAAA", "ZAZZZ", "ZAZZZ", "ZAZZZ"]);
            } else {
                assert_pattern(&buffer, &["AAAAA", "AAAAA", "AAZZZ", "AAZZZ", "AAZZZ"]);
            }

            fill_background(&mut buffer);
            let up_left_clip = check_clipped.then_some(InclusiveRect::new(0, 0, 3, 3));
            buffer
                .scroll_console_screen_buffer(
                    full_buffer_rect(),
                    Point::new(-2, -2),
                    up_left_clip,
                    fill,
                )
                .unwrap();
            if check_clipped {
                assert_pattern(&buffer, &["ZZZAZ", "ZZZAZ", "ZZZAZ", "AAAAZ", "ZZZZZ"]);
            } else {
                assert_pattern(&buffer, &["ZZZAA", "ZZZAA", "ZZZAA", "AAAAA", "AAAAA"]);
            }

            fill_background(&mut buffer);
            buffer
                .scroll_console_screen_buffer(
                    full_buffer_rect(),
                    Point::new(0, -10),
                    vertical_clip,
                    fill,
                )
                .unwrap();
            if check_clipped {
                assert_pattern(&buffer, &["AAZZZ", "AAZZZ", "AAZZZ", "AAZZZ", "AAZZZ"]);
            } else {
                assert_pattern(&buffer, &["AAAAA", "AAAAA", "AAAAA", "AAAAA", "AAAAA"]);
            }

            fill_background(&mut buffer);
            buffer
                .scroll_console_screen_buffer(
                    full_buffer_rect(),
                    Point::new(-10, -10),
                    vertical_clip,
                    LegacyCell::new(0, 0),
                )
                .unwrap();
            if check_clipped {
                assert_pattern(&buffer, &["  ZZZ", "  ZZZ", "  ZZZ", "  ZZZ", "  ZZZ"]);
            } else {
                assert_pattern(&buffer, &["     ", "     ", "     ", "     ", "     "]);
            }

            let small_clip = check_clipped.then_some(InclusiveRect::new(2, 0, 3, 4));
            fill_background(&mut buffer);
            buffer
                .fill_rect(scroll_rect, cell('B', FOREGROUND_BLUE))
                .unwrap();
            buffer
                .scroll_console_screen_buffer(scroll_rect, Point::new(2, 2), small_clip, fill)
                .unwrap();
            if check_clipped {
                assert_pattern(&buffer, &["ZZZZZ", "ZBAZZ", "ZBBBZ", "ZZBBZ", "ZZZZZ"]);
            } else {
                assert_pattern(&buffer, &["ZZZZZ", "ZAAZZ", "ZABBZ", "ZZBBZ", "ZZZZZ"]);
            }

            fill_background(&mut buffer);
            buffer
                .fill_rect(scroll_rect, cell('B', FOREGROUND_BLUE))
                .unwrap();
            buffer
                .scroll_console_screen_buffer(scroll_rect, Point::new(3, 3), small_clip, fill)
                .unwrap();
            if check_clipped {
                assert_pattern(&buffer, &["ZZZZZ", "ZBAZZ", "ZBAZZ", "ZZZBZ", "ZZZBZ"]);
            } else {
                assert_pattern(&buffer, &["ZZZZZ", "ZAAZZ", "ZAAZZ", "ZZZBB", "ZZZBB"]);
            }
        }
    }
}
