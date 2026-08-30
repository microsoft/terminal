use terminal_buffer::row::DbcsAttribute;
use terminal_buffer::text_attribute::TextAttribute;
use terminal_buffer::text_buffer::{TextBuffer, TextBufferPoint};
use terminal_buffer::text_buffer_iterator::{
    TextBufferCellIterator, TextBufferIteratorBounds, TextBufferIteratorError,
    TextBufferTextIterator,
};

fn fixture() -> TextBuffer {
    TextBuffer::new(10, 10, TextAttribute::default()).expect("valid iterator fixture")
}

fn cell_at(buffer: &TextBuffer, x: i32, y: i32) -> TextBufferCellIterator<'_> {
    TextBufferCellIterator::new(buffer, x, y).expect("point is inside fixture")
}

fn text_at(buffer: &TextBuffer, x: i32, y: i32) -> TextBufferTextIterator<'_> {
    TextBufferTextIterator::new(buffer, x, y).expect("point is inside fixture")
}

#[test]
fn microsoft_text_buffer_iterator_bool_operator_text_contract() {
    let buffer = fixture();
    let mut valid = text_at(&buffer, 0, 0);
    assert!(valid.is_valid());

    valid.advance_by(99);
    assert!(valid.is_valid());
    valid.increment();
    assert!(!valid.is_valid());
}

#[test]
fn microsoft_text_buffer_iterator_bool_operator_cell_contract() {
    let buffer = fixture();
    let mut valid = cell_at(&buffer, 0, 0);
    assert!(valid.is_valid());

    let mut last = cell_at(&buffer, 9, 9);
    assert!(last.is_valid());
    let _ = last.post_increment();
    assert!(!last.is_valid());

    valid.advance_by(100);
    assert!(!valid.is_valid());
}

#[test]
fn microsoft_text_buffer_iterator_equals_operator_text_contract() {
    let buffer = fixture();
    assert_eq!(text_at(&buffer, 0, 0), text_at(&buffer, 0, 0));
}

#[test]
fn microsoft_text_buffer_iterator_equals_operator_cell_contract() {
    let buffer = fixture();
    assert_eq!(cell_at(&buffer, 0, 0), cell_at(&buffer, 0, 0));
}

#[test]
fn microsoft_text_buffer_iterator_not_equals_operator_text_contract() {
    let buffer = fixture();
    assert_ne!(text_at(&buffer, 0, 0), text_at(&buffer, 1, 0));
}

#[test]
fn microsoft_text_buffer_iterator_not_equals_operator_cell_contract() {
    let buffer = fixture();
    assert_ne!(cell_at(&buffer, 0, 0), cell_at(&buffer, 1, 0));
}

#[test]
fn microsoft_text_buffer_iterator_plus_equals_operator_text_contract() {
    let buffer = fixture();
    let mut actual = text_at(&buffer, 0, 0);
    actual.advance_by(3);
    assert_eq!(actual, text_at(&buffer, 3, 0));
}

#[test]
fn microsoft_text_buffer_iterator_plus_equals_operator_cell_contract() {
    let buffer = fixture();
    let mut actual = cell_at(&buffer, 0, 0);
    actual.advance_by(3);
    assert_eq!(actual, cell_at(&buffer, 3, 0));
}

#[test]
fn microsoft_text_buffer_iterator_minus_equals_operator_text_contract() {
    let buffer = fixture();
    let expected = text_at(&buffer, 5, 5);
    let mut actual = text_at(&buffer, 8, 5);
    actual.retreat_by(3);
    assert_eq!(actual, expected);
}

#[test]
fn microsoft_text_buffer_iterator_minus_equals_operator_cell_contract() {
    let buffer = fixture();
    let expected = cell_at(&buffer, 5, 5);
    let mut actual = cell_at(&buffer, 8, 5);
    actual.retreat_by(3);
    assert_eq!(actual, expected);
}

#[test]
fn microsoft_text_buffer_iterator_prefix_plus_plus_operator_text_contract() {
    let buffer = fixture();
    let mut actual = text_at(&buffer, 0, 0);
    actual.increment();
    assert_eq!(actual, text_at(&buffer, 1, 0));
}

#[test]
fn microsoft_text_buffer_iterator_prefix_plus_plus_operator_cell_contract() {
    let buffer = fixture();
    let mut actual = cell_at(&buffer, 0, 0);
    actual.increment();
    assert_eq!(actual, cell_at(&buffer, 1, 0));
}

#[test]
fn microsoft_text_buffer_iterator_prefix_minus_minus_operator_text_contract() {
    let buffer = fixture();
    let mut actual = text_at(&buffer, 6, 5);
    actual.decrement();
    assert_eq!(actual, text_at(&buffer, 5, 5));
}

#[test]
fn microsoft_text_buffer_iterator_prefix_minus_minus_operator_cell_contract() {
    let buffer = fixture();
    let mut actual = cell_at(&buffer, 6, 5);
    actual.decrement();
    assert_eq!(actual, cell_at(&buffer, 5, 5));
}

#[test]
fn microsoft_text_buffer_iterator_postfix_plus_plus_operator_text_contract() {
    let buffer = fixture();
    let mut actual = text_at(&buffer, 0, 0);
    let previous = actual.post_increment();
    assert_eq!(previous, text_at(&buffer, 0, 0));
    assert_eq!(actual, text_at(&buffer, 1, 0));
}

#[test]
fn microsoft_text_buffer_iterator_postfix_plus_plus_operator_cell_contract() {
    let buffer = fixture();
    let mut actual = cell_at(&buffer, 0, 0);
    let previous = actual.post_increment();
    assert_eq!(previous, cell_at(&buffer, 0, 0));
    assert_eq!(actual, cell_at(&buffer, 1, 0));
}

#[test]
fn microsoft_text_buffer_iterator_postfix_minus_minus_operator_text_contract() {
    let buffer = fixture();
    let mut actual = text_at(&buffer, 6, 5);
    let previous = actual.post_decrement();
    assert_eq!(previous, text_at(&buffer, 6, 5));
    assert_eq!(actual, text_at(&buffer, 5, 5));
}

#[test]
fn microsoft_text_buffer_iterator_postfix_minus_minus_operator_cell_contract() {
    let buffer = fixture();
    let mut actual = cell_at(&buffer, 6, 5);
    let previous = actual.post_decrement();
    assert_eq!(previous, cell_at(&buffer, 6, 5));
    assert_eq!(actual, cell_at(&buffer, 5, 5));
}

#[test]
fn microsoft_text_buffer_iterator_plus_operator_text_contract() {
    let buffer = fixture();
    let original = text_at(&buffer, 0, 0);
    let actual = original.offset(3);
    assert_eq!(original, text_at(&buffer, 0, 0));
    assert_eq!(actual, text_at(&buffer, 3, 0));
}

#[test]
fn microsoft_text_buffer_iterator_plus_operator_cell_contract() {
    let buffer = fixture();
    let original = cell_at(&buffer, 0, 0);
    let actual = original.offset(3);
    assert_eq!(original, cell_at(&buffer, 0, 0));
    assert_eq!(actual, cell_at(&buffer, 3, 0));
}

#[test]
fn microsoft_text_buffer_iterator_minus_operator_text_contract() {
    let buffer = fixture();
    let original = text_at(&buffer, 8, 5);
    let actual = original.offset(-3);
    assert_eq!(original, text_at(&buffer, 8, 5));
    assert_eq!(actual, text_at(&buffer, 5, 5));
}

#[test]
fn microsoft_text_buffer_iterator_minus_operator_cell_contract() {
    let buffer = fixture();
    let original = cell_at(&buffer, 8, 5);
    let actual = original.offset(-3);
    assert_eq!(original, cell_at(&buffer, 8, 5));
    assert_eq!(actual, cell_at(&buffer, 5, 5));
}

#[test]
fn microsoft_text_buffer_iterator_difference_operator_text_contract() {
    let buffer = fixture();
    let first = text_at(&buffer, 0, 0);
    let second = first.offset(3);
    assert_eq!(second.distance_from(first), Ok(3));
}

#[test]
fn microsoft_text_buffer_iterator_difference_operator_cell_contract() {
    let buffer = fixture();
    let first = cell_at(&buffer, 0, 0);
    let second = first.offset(3);
    assert_eq!(second.distance_from(first), Ok(3));
}

#[test]
fn microsoft_text_buffer_iterator_as_char_info_cell_contract() {
    let mut buffer = fixture();
    let mut attribute = TextAttribute::default();
    attribute.set_intense(true);
    buffer
        .row_mut(0)
        .replace_glyph(0, 1, &[u16::from(b'Q')])
        .expect("glyph fits");
    buffer.row_mut(0).replace_attributes(0, 1, attribute);

    let iterator = cell_at(&buffer, 0, 0);
    let info = iterator.char_info().expect("valid cell has char info");
    assert_eq!(info.unicode_char, u16::from(b'Q'));
    assert_eq!(info.text_attribute, attribute);
}

#[test]
fn microsoft_text_buffer_iterator_dereference_operator_text_contract() {
    let mut buffer = fixture();
    buffer
        .row_mut(0)
        .replace_glyph(0, 1, &[0xd83d, 0xde00])
        .expect("surrogate glyph fits");

    let iterator = text_at(&buffer, 0, 0);
    assert_eq!(iterator.text(), Some(&[0xd83d, 0xde00][..]));
}

#[test]
fn microsoft_text_buffer_iterator_dereference_operator_cell_contract() {
    let mut buffer = fixture();
    let mut attribute = TextAttribute::default();
    attribute.set_intense(true);
    buffer
        .row_mut(0)
        .replace_glyph(0, 2, &[0x4e00])
        .expect("wide glyph fits");
    buffer.row_mut(0).replace_attributes(0, 2, attribute);

    let cell = cell_at(&buffer, 0, 0)
        .cell()
        .expect("valid cell dereferences");
    assert_eq!(cell.chars(), &[0x4e00]);
    assert_eq!(cell.dbcs_attribute(), DbcsAttribute::Leading);
    assert_eq!(cell.text_attribute(), attribute);
}

#[test]
fn microsoft_text_buffer_iterator_constructed_no_limit_contract() {
    let buffer = fixture();
    let mut iterator = cell_at(&buffer, 0, 0);
    let bounds = iterator.bounds();

    assert!(iterator.is_valid());
    assert_eq!(bounds, TextBufferIteratorBounds::full(&buffer));
    assert_eq!(bounds.width(), 10);
    assert_eq!(bounds.height(), 10);

    iterator.advance_by(99);
    assert!(iterator.is_valid());
    assert_eq!(iterator.position(), TextBufferPoint::new(9, 9));

    iterator.increment();
    assert!(!iterator.is_valid());
    assert_eq!(
        TextBufferCellIterator::new(&buffer, -1, -1),
        Err(TextBufferIteratorError::InvalidPoint)
    );
}

#[test]
fn microsoft_text_buffer_iterator_constructed_limits_contract() {
    let buffer = fixture();
    let bounds = TextBufferIteratorBounds::inclusive(3, 1, 5, 1).expect("valid bounds");
    let mut iterator =
        TextBufferCellIterator::with_bounds(&buffer, 3, 1, bounds).expect("start is in bounds");

    assert!(iterator.is_valid());
    assert_eq!(iterator.bounds(), bounds);
    assert_eq!(bounds.width(), 3);
    assert_eq!(bounds.height(), 1);

    iterator.advance_by(2);
    assert!(iterator.is_valid());
    assert_eq!(iterator.position(), TextBufferPoint::new(5, 1));

    iterator.increment();
    assert!(!iterator.is_valid());
    assert_eq!(
        TextBufferCellIterator::with_bounds(&buffer, 0, 0, bounds),
        Err(TextBufferIteratorError::InvalidPoint)
    );

    let invalid_bounds = TextBufferIteratorBounds::inclusive(0, 0, buffer.width(), buffer.height())
        .expect("ordered but outside buffer");
    assert_eq!(
        TextBufferCellIterator::with_bounds(&buffer, 3, 1, invalid_bounds),
        Err(TextBufferIteratorError::InvalidBounds)
    );
}
