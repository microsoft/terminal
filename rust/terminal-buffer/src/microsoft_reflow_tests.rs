//! Direct replay of Microsoft's `ReflowTests.cpp::TestReflowCases` corpus.
//!
//! The fixture is a byte-for-byte copy of Microsoft's C++ source. This module
//! deliberately parses that source instead of maintaining a second hand-written
//! Rust expectation table, so the native corpus remains the authority.

use crate::output_cell::GlyphWidthDetector;
use crate::reflow_cursor::resize_with_reflow_and_cursor;
use crate::row::DbcsAttribute;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferPoint};
use crate::width_detector::CodepointWidthDetector;

const MICROSOFT_REFLOW_SOURCE: &str = include_str!("../tests/fixtures/ReflowTests.cpp");

#[derive(Debug, Clone, PartialEq, Eq)]
struct FixtureRow {
    text: String,
    wrap: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct FixtureBuffer {
    width: u16,
    height: u16,
    rows: Vec<FixtureRow>,
    cursor: TextBufferPoint,
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct FixtureCase {
    name: String,
    buffers: Vec<FixtureBuffer>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum Token {
    Ident(String),
    WideString(String),
    Integer(u16),
    LeftBrace,
    RightBrace,
    Comma,
}

struct Parser {
    tokens: Vec<Token>,
    offset: usize,
}

impl Parser {
    fn new(tokens: Vec<Token>) -> Self {
        Self { tokens, offset: 0 }
    }

    fn peek(&self) -> Option<&Token> {
        self.tokens.get(self.offset)
    }

    fn next(&mut self) -> Token {
        let token = self.tokens.get(self.offset).cloned().unwrap_or_else(|| {
            panic!(
                "unexpected end of Microsoft reflow fixture at token {}",
                self.offset
            )
        });
        self.offset += 1;
        token
    }

    fn expect_ident(&mut self, expected: &str) {
        match self.next() {
            Token::Ident(actual) if actual == expected => {}
            actual => panic!("expected identifier {expected:?}, got {actual:?}"),
        }
    }

    fn expect_left_brace(&mut self) {
        assert_eq!(self.next(), Token::LeftBrace);
    }

    fn expect_right_brace(&mut self) {
        assert_eq!(self.next(), Token::RightBrace);
    }

    fn expect_comma(&mut self) {
        assert_eq!(self.next(), Token::Comma);
    }

    fn take_string(&mut self) -> String {
        match self.next() {
            Token::WideString(value) => value,
            actual => panic!("expected wide string, got {actual:?}"),
        }
    }

    fn take_integer(&mut self) -> u16 {
        match self.next() {
            Token::Integer(value) => value,
            actual => panic!("expected integer, got {actual:?}"),
        }
    }

    fn take_bool(&mut self) -> bool {
        match self.next() {
            Token::Ident(value) if value == "true" => true,
            Token::Ident(value) if value == "false" => false,
            actual => panic!("expected bool, got {actual:?}"),
        }
    }

    fn consume_comma(&mut self) {
        if matches!(self.peek(), Some(Token::Comma)) {
            self.offset += 1;
        }
    }

    fn parse_point(&mut self) -> (u16, u16) {
        self.expect_left_brace();
        let x = self.take_integer();
        self.expect_comma();
        let y = self.take_integer();
        self.consume_comma();
        self.expect_right_brace();
        (x, y)
    }

    fn parse_row(&mut self) -> FixtureRow {
        self.expect_left_brace();
        let text = self.take_string();
        self.expect_comma();
        let wrap = self.take_bool();
        self.consume_comma();
        self.expect_right_brace();
        FixtureRow { text, wrap }
    }

    fn parse_buffer(&mut self) -> FixtureBuffer {
        self.expect_ident("TestBuffer");
        self.expect_left_brace();
        let (width, height) = self.parse_point();
        self.expect_comma();

        self.expect_left_brace();
        let mut rows = Vec::new();
        while matches!(self.peek(), Some(Token::LeftBrace)) {
            rows.push(self.parse_row());
            self.consume_comma();
        }
        self.expect_right_brace();
        self.expect_comma();

        let (cursor_x, cursor_y) = self.parse_point();
        self.consume_comma();
        self.expect_right_brace();

        FixtureBuffer {
            width,
            height,
            rows,
            cursor: TextBufferPoint::new(cursor_x, cursor_y),
        }
    }

    fn parse_case(&mut self) -> FixtureCase {
        self.expect_ident("TestCase");
        self.expect_left_brace();
        let name = self.take_string();
        self.expect_comma();
        self.expect_left_brace();

        let mut buffers = Vec::new();
        while matches!(self.peek(), Some(Token::Ident(value)) if value == "TestBuffer") {
            buffers.push(self.parse_buffer());
            self.consume_comma();
        }

        self.expect_right_brace();
        self.consume_comma();
        self.expect_right_brace();
        FixtureCase { name, buffers }
    }
}

fn parse_microsoft_cases(source: &str) -> Vec<FixtureCase> {
    const MARKER: &str = "static const TestCase testCases[] =";
    let marker = source
        .find(MARKER)
        .expect("Microsoft testCases[] marker exists");
    let after_marker = &source[marker + MARKER.len()..];
    let opening = after_marker
        .find('{')
        .expect("Microsoft testCases[] opening brace exists");
    let body = &after_marker[opening + 1..];

    let mut parser = Parser::new(tokenize(body));
    let mut cases = Vec::new();
    while matches!(parser.peek(), Some(Token::Ident(value)) if value == "TestCase") {
        cases.push(parser.parse_case());
        parser.consume_comma();
    }
    assert_eq!(parser.peek(), Some(&Token::RightBrace));
    cases
}

fn tokenize(source: &str) -> Vec<Token> {
    let bytes = source.as_bytes();
    let mut tokens = Vec::new();
    let mut index = 0;

    while index < bytes.len() {
        match bytes[index] {
            b' ' | b'\t' | b'\r' | b'\n' => index += 1,
            b'/' if bytes.get(index + 1) == Some(&b'/') => {
                index += 2;
                while index < bytes.len() && bytes[index] != b'\n' {
                    index += 1;
                }
            }
            b'/' if bytes.get(index + 1) == Some(&b'*') => {
                index += 2;
                while index + 1 < bytes.len() && !(bytes[index] == b'*' && bytes[index + 1] == b'/')
                {
                    index += 1;
                }
                index = (index + 2).min(bytes.len());
            }
            b'{' => {
                tokens.push(Token::LeftBrace);
                index += 1;
            }
            b'}' => {
                tokens.push(Token::RightBrace);
                index += 1;
            }
            b',' => {
                tokens.push(Token::Comma);
                index += 1;
            }
            b'L' if bytes.get(index + 1) == Some(&b'"') => {
                let start = index + 2;
                index = start;
                let mut escaped = false;
                while index < bytes.len() {
                    if bytes[index] == b'"' && !escaped {
                        break;
                    }
                    if bytes[index] == b'\\' {
                        escaped = !escaped;
                    } else {
                        escaped = false;
                    }
                    index += 1;
                }
                let raw = source
                    .get(start..index)
                    .expect("wide-string byte offsets are UTF-8 boundaries");
                tokens.push(Token::WideString(unescape_cpp_string(raw)));
                index += 1;
            }
            b'0'..=b'9' => {
                let start = index;
                while index < bytes.len() && bytes[index].is_ascii_digit() {
                    index += 1;
                }
                let value = source[start..index]
                    .parse::<u16>()
                    .expect("fixture integer fits u16");
                tokens.push(Token::Integer(value));
            }
            byte if byte.is_ascii_alphabetic() || byte == b'_' => {
                let start = index;
                index += 1;
                while index < bytes.len()
                    && (bytes[index].is_ascii_alphanumeric() || bytes[index] == b'_')
                {
                    index += 1;
                }
                tokens.push(Token::Ident(source[start..index].to_owned()));
            }
            b';' => index += 1,
            _ => index += 1,
        }
    }

    tokens
}

fn unescape_cpp_string(raw: &str) -> String {
    let mut output = String::new();
    let mut chars = raw.chars();
    while let Some(ch) = chars.next() {
        if ch != '\\' {
            output.push(ch);
            continue;
        }

        let escaped = chars.next().expect("fixture escape is complete");
        output.push(match escaped {
            'n' => '\n',
            'r' => '\r',
            't' => '\t',
            '\\' => '\\',
            '"' => '"',
            other => panic!("unsupported C++ fixture escape \\{other}"),
        });
    }
    output
}

fn glyph_width(detector: CodepointWidthDetector, glyph: &[u16]) -> u16 {
    if detector.is_full_width(glyph) { 2 } else { 1 }
}

fn buffer_from_fixture(fixture: &FixtureBuffer) -> TextBuffer {
    let attr = TextAttribute::default();
    let detector = CodepointWidthDetector;
    assert_eq!(fixture.rows.len(), usize::from(fixture.height));
    let mut buffer = TextBuffer::new(fixture.width, fixture.height, attr).unwrap();

    for (y, expected_row) in fixture.rows.iter().enumerate() {
        let row = buffer.row_mut(i32::try_from(y).expect("fixture row index fits i32"));
        let mut x = 0_u16;
        for ch in expected_row.text.chars() {
            let glyph = ch.encode_utf16(&mut [0_u16; 2]).to_vec();
            let width = glyph_width(detector, &glyph);
            row.replace_glyph(i32::from(x), width, &glyph).unwrap();
            x = x.saturating_add(width);
        }
        assert_eq!(x, fixture.width, "fixture row {y} width mismatch");
        row.set_wrap_forced(expected_row.wrap);
    }

    buffer
}

fn assert_matches_fixture(
    case_name: &str,
    step: usize,
    buffer: &TextBuffer,
    cursor: TextBufferPoint,
    expected: &FixtureBuffer,
) {
    assert_eq!(
        (buffer.width(), buffer.height()),
        (expected.width, expected.height),
        "{case_name} step {step}: dimensions"
    );
    assert_eq!(cursor, expected.cursor, "{case_name} step {step}: cursor");

    let detector = CodepointWidthDetector;
    for (y, expected_row) in expected.rows.iter().enumerate() {
        let row = buffer.row(i32::try_from(y).expect("fixture row index fits i32"));
        assert_eq!(
            row.was_wrap_forced(),
            expected_row.wrap,
            "{case_name} step {step}: row {y} wrap"
        );

        let mut x = 0_u16;
        for ch in expected_row.text.chars() {
            let glyph = ch.encode_utf16(&mut [0_u16; 2]).to_vec();
            let width = glyph_width(detector, &glyph);
            assert_eq!(
                row.glyph_at(i32::from(x)),
                glyph.as_slice(),
                "{case_name} step {step}: row {y} column {x} glyph"
            );
            if width == 2 {
                assert_eq!(
                    row.dbcs_attribute_at(i32::from(x)),
                    DbcsAttribute::Leading,
                    "{case_name} step {step}: row {y} column {x} leading"
                );
                assert_eq!(
                    row.dbcs_attribute_at(i32::from(x + 1)),
                    DbcsAttribute::Trailing,
                    "{case_name} step {step}: row {y} column {} trailing",
                    x + 1
                );
                assert_eq!(
                    row.glyph_at(i32::from(x + 1)),
                    glyph.as_slice(),
                    "{case_name} step {step}: row {y} column {} trailing glyph",
                    x + 1
                );
            } else {
                assert_eq!(
                    row.dbcs_attribute_at(i32::from(x)),
                    DbcsAttribute::Single,
                    "{case_name} step {step}: row {y} column {x} single"
                );
            }
            x = x.saturating_add(width);
        }
        assert_eq!(x, expected.width, "{case_name} step {step}: row {y} width");
    }
}

#[test]
fn microsoft_reflow_test_cases_contract() {
    let cases = parse_microsoft_cases(MICROSOFT_REFLOW_SOURCE);
    assert_eq!(
        cases.len(),
        15,
        "all 15 Microsoft ReflowTests.cpp scenarios are replayed"
    );

    let attr = TextAttribute::default();
    for case in cases {
        assert!(
            case.buffers.len() >= 2,
            "{} has a reflow transition",
            case.name
        );
        let mut buffer = buffer_from_fixture(&case.buffers[0]);
        let mut cursor = case.buffers[0].cursor;
        assert_matches_fixture(&case.name, 0, &buffer, cursor, &case.buffers[0]);

        for (step, expected) in case.buffers.iter().enumerate().skip(1) {
            resize_with_reflow_and_cursor(
                &mut buffer,
                &mut cursor,
                expected.width,
                expected.height,
                attr,
            )
            .unwrap_or_else(|error| panic!("{} step {step}: reflow failed: {error:?}", case.name));
            assert_matches_fixture(&case.name, step, &buffer, cursor, expected);
        }
    }
}
