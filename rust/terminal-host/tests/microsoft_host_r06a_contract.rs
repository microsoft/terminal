use terminal_buffer::text_attribute::{LegacyColorDefaults, TextAttribute};
use terminal_host::attribute_format::format_attributes;
use terminal_host::vt_io_protocol::{encode_ucs2_utf8, sanitize_ucs2};
use terminal_host::vt_writer_sequences::{cup, dectcem, window_title};

fn sanitized_title(value: &[u16]) -> Vec<u8> {
    let mut payload = Vec::new();
    for unit in value.iter().copied() {
        payload.extend(encode_ucs2_utf8(sanitize_ucs2(unit)));
    }
    window_title(&payload)
}

#[test]
fn microsoft_host_vt_io_set_console_cursor_position_matches_exact_vectors() {
    let positions = [(2, 3), (0, 0), (7, 3), (3, 2)];
    let mut actual = Vec::new();
    for (x, y) in positions {
        actual.extend(cup(x, y));
    }

    assert_eq!(actual, b"\x1b[4;3H\x1b[1;1H\x1b[4;8H\x1b[3;4H");
}

#[test]
fn microsoft_host_vt_io_set_console_title_matches_sanitized_vectors() {
    assert_eq!(
        sanitized_title(&"foobar".encode_utf16().collect::<Vec<_>>()),
        "\u{1b}]0;foobar\u{1b}\\".as_bytes()
    );

    let mut controls = "foo".encode_utf16().collect::<Vec<_>>();
    controls.extend([0x0001, 0x001f]);
    controls.extend("bar".encode_utf16());
    assert_eq!(
        sanitized_title(&controls),
        "\u{1b}]0;foo☺▼bar\u{1b}\\".as_bytes()
    );

    controls.extend([0x007f, 0x009f]);
    assert_eq!(
        sanitized_title(&controls),
        "\u{1b}]0;foo☺▼bar⌂?\u{1b}\\".as_bytes()
    );
}

#[test]
fn microsoft_host_vt_io_set_console_cursor_info_matches_exact_vectors() {
    let mut actual = Vec::new();
    actual.extend(dectcem(false));
    actual.extend(dectcem(true));
    assert_eq!(actual, b"\x1b[?25l\x1b[?25h");
}

#[test]
fn microsoft_host_vt_io_set_console_text_attribute_matches_full_legacy_matrix() {
    let defaults = LegacyColorDefaults::default();
    let mut actual = String::new();

    for foreground in 0u16..16 {
        actual.push_str(&format_attributes(TextAttribute::from_legacy(
            foreground | 0x0040,
            defaults,
        )));
    }

    for background in 0u16..16 {
        actual.push_str(&format_attributes(TextAttribute::from_legacy(
            (background << 4) | 0x0004,
            defaults,
        )));
    }

    actual.push_str(&format_attributes(TextAttribute::from_legacy(
        0x4000 | 0x0020 | 0x000d,
        defaults,
    )));
    actual.push_str(&format_attributes(TextAttribute::from_legacy(
        0x4000 | 0x0007,
        defaults,
    )));

    let expected = concat!(
        "\x1b[0;30;41m",
        "\x1b[0;34;41m",
        "\x1b[0;32;41m",
        "\x1b[0;36;41m",
        "\x1b[0;31;41m",
        "\x1b[0;35;41m",
        "\x1b[0;33;41m",
        "\x1b[0;41m",
        "\x1b[0;90;41m",
        "\x1b[0;94;41m",
        "\x1b[0;92;41m",
        "\x1b[0;96;41m",
        "\x1b[0;91;41m",
        "\x1b[0;95;41m",
        "\x1b[0;93;41m",
        "\x1b[0;97;41m",
        "\x1b[0;31m",
        "\x1b[0;31;44m",
        "\x1b[0;31;42m",
        "\x1b[0;31;46m",
        "\x1b[0;31;41m",
        "\x1b[0;31;45m",
        "\x1b[0;31;43m",
        "\x1b[0;31;47m",
        "\x1b[0;31;100m",
        "\x1b[0;31;104m",
        "\x1b[0;31;102m",
        "\x1b[0;31;106m",
        "\x1b[0;31;101m",
        "\x1b[0;31;105m",
        "\x1b[0;31;103m",
        "\x1b[0;31;107m",
        "\x1b[0;7;95;42m",
        "\x1b[0;7m"
    );

    assert_eq!(actual, expected);
}
