use terminal_buffer::width_detector::TextMeasurementEngine;

const MICROSOFT_SOURCE: &str = include_str!("data/CodepointWidthDetectorTests.fixture.cpp");

fn parse_active_grapheme_rows(source: &str) -> Vec<Vec<Vec<u16>>> {
    let mut rows = Vec::new();
    let mut in_grapheme_table = false;

    for line in source.lines() {
        let trimmed = line.trim();
        if trimmed.starts_with("static constexpr GraphemeBreakTest s_graphemeBreakTests[]")
            || trimmed.starts_with("static constexpr GraphemeBreakTest s_graphemeBreakTestsExtra[]")
        {
            in_grapheme_table = true;
            continue;
        }
        if in_grapheme_table && trimmed == "};" {
            in_grapheme_table = false;
            continue;
        }
        if !in_grapheme_table || trimmed.starts_with("//") || !trimmed.starts_with("{ L\"") {
            continue;
        }

        let literals = parse_wide_literals(trimmed);
        assert!(literals.len() >= 2, "unparsed Microsoft row: {trimmed}");
        rows.push(literals.into_iter().skip(1).collect());
    }

    rows
}

fn parse_wide_literals(line: &str) -> Vec<Vec<u16>> {
    let bytes = line.as_bytes();
    let mut literals = Vec::new();
    let mut index = 0;

    while index + 1 < bytes.len() {
        if bytes[index] != b'L' || bytes[index + 1] != b'\"' {
            index += 1;
            continue;
        }

        index += 2;
        let start = index;
        let mut escaped = false;
        while index < bytes.len() {
            let byte = bytes[index];
            if byte == b'\"' && !escaped {
                break;
            }
            if byte == b'\\' {
                escaped = !escaped;
            } else {
                escaped = false;
            }
            index += 1;
        }
        assert!(index < bytes.len(), "unterminated wide literal: {line}");
        literals.push(decode_cpp_wide_literal(&line[start..index]));
        index += 1;
    }

    literals
}

fn decode_cpp_wide_literal(body: &str) -> Vec<u16> {
    let chars = body.chars().collect::<Vec<_>>();
    let mut output = Vec::new();
    let mut index = 0;

    while index < chars.len() {
        if chars[index] != '\\' {
            push_scalar(&mut output, u32::from(chars[index]));
            index += 1;
            continue;
        }

        index += 1;
        assert!(index < chars.len(), "dangling C++ escape in {body}");
        match chars[index] {
            '\\' => push_scalar(&mut output, u32::from('\\')),
            '\"' => push_scalar(&mut output, u32::from('\"')),
            'n' => push_scalar(&mut output, u32::from('\n')),
            'r' => push_scalar(&mut output, u32::from('\r')),
            't' => push_scalar(&mut output, u32::from('\t')),
            'a' => push_scalar(&mut output, 0x07),
            'b' => push_scalar(&mut output, 0x08),
            'f' => push_scalar(&mut output, 0x0c),
            'v' => push_scalar(&mut output, 0x0b),
            'x' => {
                index += 1;
                let start = index;
                while index < chars.len() && chars[index].is_ascii_hexdigit() {
                    index += 1;
                }
                assert!(index > start, "empty C++ hex escape in {body}");
                let digits = chars[start..index].iter().collect::<String>();
                let value = u32::from_str_radix(&digits, 16).expect("valid C++ hex escape");
                push_scalar(&mut output, value);
                continue;
            }
            'u' => {
                let value = fixed_hex_escape(&chars, index + 1, 4, body);
                push_scalar(&mut output, value);
                index += 4;
            }
            'U' => {
                let value = fixed_hex_escape(&chars, index + 1, 8, body);
                push_scalar(&mut output, value);
                index += 8;
            }
            other => panic!("unsupported C++ escape \\{other} in {body}"),
        }
        index += 1;
    }

    output
}

fn fixed_hex_escape(chars: &[char], start: usize, width: usize, body: &str) -> u32 {
    let end = start + width;
    assert!(end <= chars.len(), "short fixed-width C++ escape in {body}");
    let digits = chars[start..end].iter().collect::<String>();
    assert!(
        digits.chars().all(|value| value.is_ascii_hexdigit()),
        "invalid fixed-width C++ escape in {body}"
    );
    u32::from_str_radix(&digits, 16).expect("valid fixed-width C++ escape")
}

fn push_scalar(output: &mut Vec<u16>, value: u32) {
    let scalar =
        char::from_u32(value).unwrap_or_else(|| panic!("invalid Unicode scalar U+{value:04X}"));
    let mut storage = [0_u16; 2];
    output.extend_from_slice(scalar.encode_utf16(&mut storage));
}

fn measured_lengths(text: &[u16], reverse: bool) -> Vec<usize> {
    let detector = TextMeasurementEngine::default();
    let measured = if reverse {
        detector.graphemes_backward(text)
    } else {
        detector.graphemes_forward(text)
    };
    measured.into_iter().map(|item| item.utf16_len).collect()
}

#[test]
fn microsoft_grapheme_break_test_replays_every_active_source_row() {
    let rows = parse_active_grapheme_rows(MICROSOFT_SOURCE);
    assert!(
        rows.len() > 500,
        "expected the full generated Microsoft corpus"
    );

    for (row_index, expected_graphemes) in rows.iter().enumerate() {
        let text = expected_graphemes
            .iter()
            .flat_map(|grapheme| grapheme.iter().copied())
            .collect::<Vec<_>>();
        let expected_lengths = expected_graphemes.iter().map(Vec::len).collect::<Vec<_>>();

        assert_eq!(
            measured_lengths(&text, false),
            expected_lengths,
            "forward Microsoft grapheme row {row_index}"
        );

        let mut reverse_lengths = expected_lengths;
        reverse_lengths.reverse();
        assert_eq!(
            measured_lengths(&text, true),
            reverse_lengths,
            "reverse Microsoft grapheme row {row_index}"
        );
    }
}
