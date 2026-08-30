//! Microsoft-derived integration witnesses for the portable Host TextBuffer slice.
//!
//! Each test corresponds to one `TextBufferTests.cpp` contract promoted from
//! Partial to Exact in R08. The five contracts that still cross unresolved
//! ownership boundaries intentionally do not appear here.

use crate::alternate_buffer::{CursorShape, CursorState};
use crate::cursor_properties::copy_cursor_properties;
use crate::host_write::HostWriteState;
use crate::hyperlink::HyperlinkStore;
use crate::line_edit::write_text;
use crate::repeat_character::RepeatCharacterState;
use crate::resize_integrity::resize_traditional;
use crate::row::Row;
use crate::row_writer::{replace_text, replace_text_with_attribute};
use crate::rtf_text::append_rtf_text;
use crate::screen_erase::{EraseType, erase_line};
use crate::sgr::apply_sgr;
use crate::text_attribute::{TextAttribute, UnderlineStyle};
use crate::text_buffer::{TextBuffer, TextBufferPoint};
use crate::text_buffer_queries::{glyph_end, glyph_start, last_non_space_character};
use crate::text_buffer_write::write_line;
use crate::text_color::{Rgb, TextColor};
use crate::vertical_scroll::VerticalScrollState;

fn default_attr() -> TextAttribute {
    TextAttribute::default()
}

fn utf16(text: &str) -> Vec<u16> {
    text.encode_utf16().collect()
}

fn boundary_measure(text: &str, width: u16) -> (u16, u16) {
    let mut row = Row::new(width, default_attr()).unwrap();
    replace_text(&mut row, 0, &utf16(text)).unwrap();
    (row.measure_left(), row.measure_right())
}

fn set_cell(buffer: &mut TextBuffer, x: u16, y: u16, glyph: &[u16], attr: TextAttribute) {
    let row = buffer.row_mut(i32::from(y));
    row.replace_glyph(i32::from(x), 1, glyph).unwrap();
    row.replace_attributes(i32::from(x), i32::from(x + 1), attr);
}

fn rgb_background(attribute: TextAttribute) -> Rgb {
    attribute.background().rgb_value()
}

#[test]
fn microsoft_text_buffer_test_buffer_create_contract() {
    let buffer = TextBuffer::new(80, 25, default_attr()).unwrap();
    assert_eq!((buffer.width(), buffer.height()), (80, 25));
    assert_eq!(buffer.first_row_index(), 0);
    assert_eq!(buffer.row(0).size(), 80);
}

#[test]
fn microsoft_text_buffer_test_wrap_flag_contract() {
    let mut row = Row::new(80, default_attr()).unwrap();
    assert!(!row.was_wrap_forced());
    row.set_wrap_forced(true);
    assert!(row.was_wrap_forced());
    row.set_wrap_forced(false);
    assert!(!row.was_wrap_forced());
}

#[test]
fn microsoft_text_buffer_test_wrap_through_write_line_contract() {
    let attr = default_attr();
    let mut buffer = TextBuffer::new(80, 2, attr).unwrap();
    let line = vec![u16::from(b'a'); 80];
    write_line(&mut buffer, 0, 0, &line, attr, None).unwrap();
    assert!(!buffer.row(0).was_wrap_forced());
    write_line(&mut buffer, 0, 0, &line, attr, Some(true)).unwrap();
    assert!(buffer.row(0).was_wrap_forced());
    write_line(&mut buffer, 0, 0, &line, attr, None).unwrap();
    assert!(buffer.row(0).was_wrap_forced());
    write_line(&mut buffer, 0, 0, &line, attr, Some(false)).unwrap();
    assert!(!buffer.row(0).was_wrap_forced());
    write_line(&mut buffer, 0, 0, &line, attr, None).unwrap();
    assert!(!buffer.row(0).was_wrap_forced());
}

#[test]
fn microsoft_text_buffer_test_double_byte_pad_flag_contract() {
    let mut row = Row::new(80, default_attr()).unwrap();
    assert!(!row.was_double_byte_padded());
    row.set_double_byte_padded(true);
    assert!(row.was_double_byte_padded());
    row.set_double_byte_padded(false);
    assert!(!row.was_double_byte_padded());
}

#[test]
fn microsoft_text_buffer_boundary_empty_contract() {
    assert_eq!(boundary_measure("", 80), (80, 0));
}

#[test]
fn microsoft_text_buffer_boundary_full_contract() {
    assert_eq!(boundary_measure(&"X".repeat(80), 80), (0, 80));
}

#[test]
fn microsoft_text_buffer_boundary_regular_contract() {
    assert_eq!(
        boundary_measure("The quick brown fox jumps over the lazy dog.", 80),
        (0, 44)
    );
}

#[test]
fn microsoft_text_buffer_boundary_floating_contract() {
    assert_eq!(boundary_measure("     C:\\>     ", 80), (5, 9));
}

#[test]
fn microsoft_text_buffer_test_copy_properties_contract() {
    let mut target = CursorState {
        x: 7,
        y: 9,
        visible: false,
        size: 12,
        shape: CursorShape::Legacy,
        blinking: false,
    };
    let source = CursorState {
        x: 1,
        y: 2,
        visible: true,
        size: 50,
        shape: CursorShape::DoubleUnderscore,
        blinking: true,
    };
    copy_cursor_properties(&mut target, source);
    assert_eq!((target.x, target.y), (7, 9));
    assert!(target.visible);
    assert_eq!(target.shape, CursorShape::DoubleUnderscore);
}

#[test]
fn microsoft_text_buffer_test_get_last_non_space_character_contract() {
    let mut buffer = TextBuffer::new(80, 15, default_attr()).unwrap();
    for (y, text) in [(0_i32, "first"), (1, "second"), (2, "third"), (3, "fourth")] {
        replace_text(buffer.row_mut(y), 0, &utf16(text)).unwrap();
    }
    for y in [3_u16, 4, 14] {
        assert_eq!(
            last_non_space_character(&buffer, TextBufferPoint::new(0, y)),
            Some(TextBufferPoint::new(5, 3))
        );
    }
}

#[test]
fn microsoft_text_buffer_test_increment_circular_buffer_contract() {
    let attr = default_attr();
    let mut buffer = TextBuffer::new(8, 6, attr).unwrap();
    for expected in [1_u16, 2, 3, 4, 5, 0] {
        set_cell(&mut buffer, 0, 0, &[u16::from(b'X')], attr);
        buffer.rotate_up(1, attr);
        assert_eq!(buffer.first_row_index(), expected);
        assert!(!buffer.row(5).contains_text());
    }
}

#[test]
fn microsoft_text_buffer_mixed_rgb_legacy_foreground_contract() {
    let mut attr = default_attr();
    apply_sgr(&mut attr, &[38, 2, 64, 128, 255]);
    apply_sgr(&mut attr, &[49]);
    assert_eq!(attr.foreground().rgb_value(), Rgb::new(64, 128, 255));
    assert!(attr.background().is_default());
}

#[test]
fn microsoft_text_buffer_mixed_rgb_legacy_background_contract() {
    let mut attr = default_attr();
    apply_sgr(&mut attr, &[48, 2, 64, 128, 255]);
    apply_sgr(&mut attr, &[39]);
    assert!(attr.foreground().is_default());
    assert_eq!(rgb_background(attr), Rgb::new(64, 128, 255));
}

#[test]
fn microsoft_text_buffer_mixed_rgb_legacy_underline_contract() {
    let mut attr = default_attr();
    apply_sgr(&mut attr, &[48, 2, 64, 128, 255]);
    apply_sgr(&mut attr, &[4]);
    assert_eq!(attr.underline_style(), UnderlineStyle::Single);
    assert_eq!(rgb_background(attr), Rgb::new(64, 128, 255));
}

#[test]
fn microsoft_text_buffer_mixed_rgb_legacy_brightness_contract() {
    let mut attr = default_attr();
    apply_sgr(&mut attr, &[32]);
    assert_eq!(attr.foreground().index(), TextColor::DARK_GREEN);
    apply_sgr(&mut attr, &[1]);
    assert!(attr.is_intense());
    assert_eq!(attr.foreground().index(), TextColor::DARK_GREEN);
    assert!(attr.foreground().can_be_brightened());
}

#[test]
fn microsoft_text_buffer_rgb_erase_line_contract() {
    let mut active = default_attr();
    apply_sgr(&mut active, &[48, 2, 128, 128, 255]);
    let mut buffer = TextBuffer::new(8, 1, default_attr()).unwrap();
    set_cell(&mut buffer, 0, 0, &[u16::from(b'X')], default_attr());
    erase_line(
        &mut buffer,
        TextBufferPoint::new(1, 0),
        EraseType::ToEnd,
        false,
        active,
    )
    .unwrap();
    for x in 1..8 {
        assert_eq!(buffer.row(0).glyph_at(x), &[u16::from(b' ')]);
        assert_eq!(
            rgb_background(buffer.row(0).attribute_at(x)),
            Rgb::new(128, 128, 255)
        );
    }
}

#[test]
fn microsoft_text_buffer_unintense_contract() {
    let mut attr = default_attr();
    apply_sgr(&mut attr, &[1, 32]);
    assert!(attr.is_intense());
    apply_sgr(&mut attr, &[22]);
    assert!(!attr.is_intense());
    assert_eq!(attr.foreground().index(), TextColor::DARK_GREEN);
}

#[test]
fn microsoft_text_buffer_unintense_rgb_contract() {
    let mut attr = default_attr();
    apply_sgr(&mut attr, &[1, 32, 48, 2, 1, 2, 3]);
    apply_sgr(&mut attr, &[22]);
    assert!(!attr.is_intense());
    assert_eq!(attr.foreground().index(), TextColor::DARK_GREEN);
    assert_eq!(rgb_background(attr), Rgb::new(1, 2, 3));
}

#[test]
fn microsoft_text_buffer_complex_unintense_contract() {
    let mut attr = default_attr();
    apply_sgr(&mut attr, &[1, 32, 48, 2, 1, 2, 3]);
    apply_sgr(&mut attr, &[22, 38, 2, 32, 32, 32, 1]);
    assert!(attr.is_intense());
    assert_eq!(attr.foreground().rgb_value(), Rgb::new(32, 32, 32));
    apply_sgr(&mut attr, &[38, 2, 64, 64, 64, 22]);
    assert!(!attr.is_intense());
    assert_eq!(attr.foreground().rgb_value(), Rgb::new(64, 64, 64));
    assert_eq!(rgb_background(attr), Rgb::new(1, 2, 3));
}

#[test]
fn microsoft_text_buffer_copy_attrs_contract() {
    let mut green = default_attr();
    green.set_foreground(TextColor::index16(TextColor::DARK_GREEN));
    let mut blue = default_attr();
    blue.set_foreground(TextColor::index16(TextColor::DARK_BLUE));
    let mut magenta = default_attr();
    magenta.set_foreground(TextColor::index16(TextColor::DARK_MAGENTA));
    let mut buffer = TextBuffer::new(4, 3, default_attr()).unwrap();
    for x in 0..4 {
        set_cell(&mut buffer, x, 0, &[u16::from(b'A')], green);
        set_cell(&mut buffer, x, 1, &[u16::from(b'B')], blue);
        set_cell(&mut buffer, x, 2, &[u16::from(b'C')], magenta);
    }
    let mut scroll = VerticalScrollState::new(4, 0, 3);
    scroll.set_cursor(0, 0);
    scroll.insert_lines(&mut buffer, 1, default_attr()).unwrap();
    assert_eq!(buffer.row(1).attribute_at(0), green);
    assert_eq!(buffer.row(2).attribute_at(0), blue);
}

#[test]
fn microsoft_text_buffer_empty_sgr_contract() {
    let mut attr = TextAttribute::from_rgb(Rgb::new(1, 2, 3), Rgb::new(4, 5, 6));
    apply_sgr(&mut attr, &[]);
    assert_eq!(attr, default_attr());
    apply_sgr(&mut attr, &[31, 0]);
    assert_eq!(attr, default_attr());
}

#[test]
fn microsoft_text_buffer_reverse_reset_contract() {
    let mut attr = default_attr();
    apply_sgr(&mut attr, &[42, 38, 2, 128, 5, 255]);
    let fg = attr.foreground();
    let bg = attr.background();
    apply_sgr(&mut attr, &[7]);
    assert!(attr.is_reverse_video());
    apply_sgr(&mut attr, &[27]);
    assert!(!attr.is_reverse_video());
    assert_eq!(attr.foreground(), fg);
    assert_eq!(attr.background(), bg);
}

#[test]
fn microsoft_text_buffer_copy_last_attr_contract() {
    let mut final_attr = default_attr();
    final_attr.set_foreground(TextColor::index16(TextColor::DARK_CYAN));
    let mut buffer = TextBuffer::new(4, 2, default_attr()).unwrap();
    buffer.row_mut(0).replace_attributes(0, 4, final_attr);
    let mut scroll = VerticalScrollState::new(4, 0, 2);
    scroll.set_cursor(0, 0);
    scroll.insert_lines(&mut buffer, 1, default_attr()).unwrap();
    assert_eq!(buffer.row(1).attribute_at(3), final_attr);
}

#[test]
fn microsoft_text_buffer_rgb_then_intense_contract() {
    let mut attr = default_attr();
    apply_sgr(&mut attr, &[38, 2, 40, 40, 40, 48, 2, 168, 153, 132]);
    let fg = attr.foreground();
    let bg = attr.background();
    apply_sgr(&mut attr, &[1]);
    assert!(attr.is_intense());
    assert_eq!(attr.foreground(), fg);
    assert_eq!(attr.background(), bg);
}

#[test]
fn microsoft_text_buffer_reset_clears_intensity_contract() {
    let mut attr = default_attr();
    apply_sgr(&mut attr, &[32, 1]);
    assert!(attr.is_intense());
    apply_sgr(&mut attr, &[0, 32]);
    assert!(!attr.is_intense());
    assert_eq!(attr.foreground().index(), TextColor::DARK_GREEN);
}

#[test]
fn microsoft_text_buffer_backspace_strings_contract() {
    let attr = default_attr();
    let mut buffer = TextBuffer::new(16, 1, attr).unwrap();
    let mut state = HostWriteState::new(TextBufferPoint::new(0, 0), attr);
    state.write_vt(&mut buffer, &utf16("a\u{8} \u{8}")).unwrap();
    assert_eq!(state.cursor(), TextBufferPoint::new(0, 0));
    assert_eq!(buffer.row(0).glyph_at(0), &[u16::from(b' ')]);
}

#[test]
fn microsoft_text_buffer_backspace_strings_api_contract() {
    let attr = default_attr();
    let mut buffer = TextBuffer::new(16, 1, attr).unwrap();
    let mut state = HostWriteState::new(TextBufferPoint::new(0, 0), attr);
    for part in ["a", "\u{8}", " ", "\u{8}"] {
        state.write_chars_legacy(&mut buffer, &utf16(part)).unwrap();
    }
    assert_eq!(state.cursor(), TextBufferPoint::new(0, 0));
    assert_eq!(buffer.row(0).glyph_at(0), &[u16::from(b' ')]);
}

#[test]
fn microsoft_text_buffer_repeat_character_contract() {
    let attr = default_attr();
    let mut buffer = TextBuffer::new(16, 2, attr).unwrap();
    let mut cursor = TextBufferPoint::new(0, 0);
    let mut repeat = RepeatCharacterState::new();
    repeat
        .write_graphic(&mut buffer, &mut cursor, &[u16::from(b'X')], attr)
        .unwrap();
    repeat.repeat(&mut buffer, &mut cursor, 5, attr).unwrap();
    assert_eq!(cursor, TextBufferPoint::new(6, 0));
    assert_eq!(buffer.row(0).text_range(0, 6), &vec![u16::from(b'X'); 6]);
    repeat.invalidate();
    repeat.repeat(&mut buffer, &mut cursor, 1, attr).unwrap();
    assert_eq!(cursor, TextBufferPoint::new(6, 0));
}

#[test]
fn microsoft_text_buffer_resize_traditional_contract() {
    let mut last = default_attr();
    last.set_foreground(TextColor::index16(TextColor::DARK_RED));
    let mut buffer = TextBuffer::new(4, 2, default_attr()).unwrap();
    set_cell(&mut buffer, 3, 0, &[u16::from(b'Z')], last);
    resize_traditional(&mut buffer, 6, 2, default_attr()).unwrap();
    assert_eq!((buffer.width(), buffer.height()), (6, 2));
    assert_eq!(buffer.row(0).glyph_at(3), &[u16::from(b'Z')]);
    assert_eq!(buffer.row(0).attribute_at(4), last);
    assert_eq!(buffer.row(0).attribute_at(5), last);
}

#[test]
fn microsoft_text_buffer_resize_rotation_preserves_high_unicode_contract() {
    let attr = default_attr();
    let glyph = [0xd83d, 0xde00];
    let mut buffer = TextBuffer::new(8, 3, attr).unwrap();
    buffer.row_mut(1).replace_glyph(2, 2, &glyph).unwrap();
    buffer.rotate_up(1, attr);
    resize_traditional(&mut buffer, 10, 3, attr).unwrap();
    assert_eq!(buffer.row(0).glyph_at(2), &glyph);
}

#[test]
fn microsoft_text_buffer_scroll_rotation_preserves_high_unicode_contract() {
    let attr = default_attr();
    let glyph = [0xd83d, 0xde00];
    let mut buffer = TextBuffer::new(8, 3, attr).unwrap();
    buffer.row_mut(1).replace_glyph(2, 2, &glyph).unwrap();
    buffer.rotate_up(1, attr);
    assert_eq!(buffer.row(0).glyph_at(2), &glyph);
    assert!(!buffer.row(2).contains_text());
}

#[test]
fn microsoft_text_buffer_resize_high_unicode_row_removal_contract() {
    let attr = default_attr();
    let mut buffer = TextBuffer::new(8, 3, attr).unwrap();
    buffer
        .row_mut(2)
        .replace_glyph(2, 2, &[0xd83d, 0xde00])
        .unwrap();
    resize_traditional(&mut buffer, 8, 2, attr).unwrap();
    assert_eq!(buffer.height(), 2);
}

#[test]
fn microsoft_text_buffer_resize_high_unicode_column_removal_contract() {
    let attr = default_attr();
    let mut buffer = TextBuffer::new(8, 2, attr).unwrap();
    buffer
        .row_mut(0)
        .replace_glyph(6, 2, &[0xd83d, 0xde00])
        .unwrap();
    resize_traditional(&mut buffer, 6, 2, attr).unwrap();
    assert_eq!(buffer.width(), 6);
    assert!(!buffer.row(0).contains_text());
}

#[test]
fn microsoft_text_buffer_overwrite_chars_contract() {
    let attr = default_attr();
    let mut row = Row::new(10, attr).unwrap();
    replace_text(&mut row, 0, &utf16("abcdefghij")).unwrap();
    let end = write_text(&mut row, 3, &utf16("XYZ"), attr, false, 0..10).unwrap();
    assert_eq!(end, 6);
    assert_eq!(row.text_range(0, 10), &utf16("abcXYZghij"));
}

#[test]
fn microsoft_text_buffer_replace_contract() {
    let mut attr = default_attr();
    attr.set_intense(true);
    let mut row = Row::new(10, default_attr()).unwrap();
    replace_text(&mut row, 0, &utf16("abcdefghij")).unwrap();
    let end = replace_text_with_attribute(&mut row, 2, &utf16("XYZ"), attr).unwrap();
    assert_eq!(end, 5);
    assert_eq!(row.text_range(0, 10), &utf16("abXYZfghij"));
    assert_eq!(row.attribute_at(2), attr);
}

#[test]
fn microsoft_text_buffer_insert_contract() {
    let attr = default_attr();
    let mut row = Row::new(10, attr).unwrap();
    replace_text(&mut row, 0, &utf16("abcdef")).unwrap();
    let end = write_text(&mut row, 2, &utf16("XY"), attr, true, 0..10).unwrap();
    assert_eq!(end, 4);
    assert_eq!(row.text_range(0, 8), &utf16("abXYcdef"));
}

#[test]
fn microsoft_text_buffer_append_rtf_text_contract() {
    let mut output = String::new();
    append_rtf_text(&mut output, &utf16("This is some Ascii \\ {}"));
    assert_eq!(output, "This is some Ascii \\\\ \\{\\}");
    output.clear();
    append_rtf_text(&mut output, &[0xa7b5, 0x0020, 0xd83d, 0xdc7e]);
    assert_eq!(output, "\\u-22603? \\u-10179?\\u-9090?");
}

#[test]
fn microsoft_text_buffer_get_glyph_boundaries_contract() {
    let attr = default_attr();
    let mut buffer = TextBuffer::new(10, 2, attr).unwrap();
    buffer
        .row_mut(0)
        .replace_glyph(1, 2, &[0xd83c, 0xdf2f])
        .unwrap();
    assert_eq!(
        glyph_start(&buffer, TextBufferPoint::new(2, 0)),
        TextBufferPoint::new(1, 0)
    );
    assert_eq!(
        glyph_end(&buffer, TextBufferPoint::new(1, 0)),
        TextBufferPoint::new(3, 0)
    );
    buffer
        .row_mut(0)
        .replace_glyph(9, 1, &[u16::from(b'X')])
        .unwrap();
    assert_eq!(
        glyph_end(&buffer, TextBufferPoint::new(9, 0)),
        TextBufferPoint::new(0, 1)
    );
}

#[test]
fn microsoft_text_buffer_hyperlink_trim_contract() {
    let fill = default_attr();
    let mut buffer = TextBuffer::new(80, 10, fill).unwrap();
    let mut store = HyperlinkStore::new();
    let obsolete = store.add("test.url", Some("CustomId"));
    let live = store.add("other.url", Some("OtherCustomId"));
    let mut obsolete_attr = fill;
    obsolete_attr.set_hyperlink_id(obsolete);
    buffer.row_mut(0).set_attr_to_end(70, obsolete_attr);
    let mut live_attr = fill;
    live_attr.set_hyperlink_id(live);
    buffer.row_mut(5).set_attr_to_end(70, live_attr);
    buffer.rotate_up(1, fill);
    store.trim_to_buffer(&buffer);
    assert_eq!(store.uri(obsolete), None);
    assert_eq!(store.uri(live), Some("other.url"));
}

#[test]
fn microsoft_text_buffer_no_hyperlink_trim_contract() {
    let fill = default_attr();
    let mut buffer = TextBuffer::new(80, 10, fill).unwrap();
    let mut store = HyperlinkStore::new();
    let id = store.add("test.url", Some("CustomId"));
    let mut attr = fill;
    attr.set_hyperlink_id(id);
    buffer.row_mut(0).set_attr_to_end(70, attr);
    buffer.row_mut(5).set_attr_to_end(70, attr);
    buffer.rotate_up(1, fill);
    store.trim_to_buffer(&buffer);
    assert_eq!(store.uri(id), Some("test.url"));
    assert_eq!(store.len(), 1);
}
