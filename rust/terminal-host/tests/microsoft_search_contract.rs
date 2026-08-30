use terminal_buffer::row_writer::replace_text;
use terminal_buffer::search::TextSearchSpan;
use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};
use terminal_host::search::{SearchFlags, SearchSession};

fn fixture() -> TextBuffer {
    let mut buffer = TextBuffer::new(8, 4, TextAttribute::default()).expect("valid fixture");
    let row = "ABか".encode_utf16().collect::<Vec<_>>();
    for y in 0..4 {
        replace_text(buffer.row_mut(y), 0, &row).expect("source fixture fits");
    }
    buffer
}

fn expected_span(x: u16, y: u16) -> TextSearchSpan {
    TextSearchSpan::new(TextBufferPoint::new(x, y), TextBufferPoint::new(x + 2, y))
}

fn assert_source_sequence(needle: &str, flags: SearchFlags, reverse: bool, x: u16) {
    let buffer = fixture();
    let mut search = SearchSession::reset(&buffer, needle, flags, reverse);
    assert!(search.is_ok());
    assert_eq!(search.results().len(), 4);

    let rows: [u16; 4] = if reverse { [3, 2, 1, 0] } else { [0, 1, 2, 3] };
    for (index, y) in rows.into_iter().enumerate() {
        assert_eq!(search.current(), Some(expected_span(x, y)));
        if index + 1 != rows.len() {
            search.find_next(reverse);
        }
    }
}

#[test]
fn microsoft_search_forward_case_sensitive_contract() {
    assert_source_sequence("AB", SearchFlags::NONE, false, 0);
}

#[test]
fn microsoft_search_forward_case_sensitive_japanese_contract() {
    assert_source_sequence("か", SearchFlags::NONE, false, 2);
}

#[test]
fn microsoft_search_forward_case_insensitive_contract() {
    assert_source_sequence("ab", SearchFlags::CASE_INSENSITIVE, false, 0);
}

#[test]
fn microsoft_search_forward_case_insensitive_japanese_contract() {
    assert_source_sequence("か", SearchFlags::CASE_INSENSITIVE, false, 2);
}

#[test]
fn microsoft_search_backward_case_sensitive_contract() {
    assert_source_sequence("AB", SearchFlags::NONE, true, 0);
}

#[test]
fn microsoft_search_backward_case_sensitive_japanese_contract() {
    assert_source_sequence("か", SearchFlags::NONE, true, 2);
}

#[test]
fn microsoft_search_backward_case_insensitive_contract() {
    assert_source_sequence("ab", SearchFlags::CASE_INSENSITIVE, true, 0);
}

#[test]
fn microsoft_search_backward_case_insensitive_japanese_contract() {
    assert_source_sequence("か", SearchFlags::CASE_INSENSITIVE, true, 2);
}

#[test]
fn microsoft_search_forward_case_sensitive_regex_contract() {
    assert_source_sequence("[BA]{2}", SearchFlags::REGULAR_EXPRESSION, false, 0);
}

#[test]
fn microsoft_search_forward_case_sensitive_regex_japanese_contract() {
    assert_source_sequence(
        r"[\x{3041}-\x{304c}]",
        SearchFlags::REGULAR_EXPRESSION,
        false,
        2,
    );
}

#[test]
fn microsoft_search_forward_case_insensitive_regex_contract() {
    assert_source_sequence(
        "ab",
        SearchFlags::CASE_INSENSITIVE | SearchFlags::REGULAR_EXPRESSION,
        false,
        0,
    );
}

#[test]
fn microsoft_search_forward_case_insensitive_regex_japanese_contract() {
    assert_source_sequence(
        r"[\x{3041}-\x{304c}]",
        SearchFlags::CASE_INSENSITIVE | SearchFlags::REGULAR_EXPRESSION,
        false,
        2,
    );
}

#[test]
fn microsoft_search_forward_case_sensitive_regex_with_case_insensitive_flag_contract() {
    assert_source_sequence("(?i)ab", SearchFlags::REGULAR_EXPRESSION, false, 0);
}
