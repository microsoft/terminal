use terminal_buffer::output_cell::{GlyphWidthDetector, OutputCellIterator, TextAttributeBehavior};
use terminal_buffer::row::DbcsAttribute;
use terminal_buffer::text_attribute::{LegacyColorDefaults, TextAttribute};

struct MicrosoftWidthDetector;

impl GlyphWidthDetector for MicrosoftWidthDetector {
    fn is_full_width(&self, glyph: &[u16]) -> bool {
        matches!(glyph, [0x30a2..=0x30a6])
    }
}

fn utf16(value: &str) -> Vec<u16> {
    value.encode_utf16().collect()
}

#[test]
fn microsoft_host_output_cell_string_and_distance_standard_match() {
    let detector = MicrosoftWidthDetector;
    let text = utf16("The quick brown fox jumps over the lazy dog.");
    let mut iterator = OutputCellIterator::text_only(&text, &detector);

    for (input_position, expected) in text.iter().copied().enumerate() {
        let cell = iterator
            .next()
            .expect("one output cell per narrow code unit");
        assert_eq!(cell.chars(), &[expected]);
        assert_eq!(cell.dbcs_attribute(), DbcsAttribute::Single);
        assert_eq!(
            cell.text_attribute_behavior(),
            TextAttributeBehavior::Current
        );
        assert_eq!(iterator.position(), input_position + 1);
        assert_eq!(iterator.cell_distance(), input_position + 1);
    }

    assert!(iterator.next().is_none());
    assert_eq!(iterator.position(), text.len());
    assert_eq!(iterator.cell_distance(), text.len());
}

#[test]
fn microsoft_host_output_cell_full_width_string_and_distance_match() {
    let detector = MicrosoftWidthDetector;
    let text = [0x30a2, 0x30a3, 0x30a4, 0x30a5, 0x30a6];
    let mut iterator = OutputCellIterator::text_only(&text, &detector);

    for (index, expected) in text.iter().copied().enumerate() {
        let leading = iterator.next().expect("full-width leading cell");
        assert_eq!(leading.chars(), &[expected]);
        assert_eq!(leading.dbcs_attribute(), DbcsAttribute::Leading);
        assert_eq!(iterator.position(), index);

        let trailing = iterator.next().expect("full-width trailing cell");
        assert_eq!(trailing.chars(), &[expected]);
        assert_eq!(trailing.dbcs_attribute(), DbcsAttribute::Trailing);
        assert_eq!(iterator.position(), index + 1);
        assert_eq!(iterator.cell_distance(), (index + 1) * 2);
    }

    assert!(iterator.next().is_none());
    assert_eq!(iterator.position(), text.len());
    assert_eq!(iterator.cell_distance(), text.len() * 2);
}

#[test]
fn microsoft_host_output_cell_string_with_color_is_stored() {
    let detector = MicrosoftWidthDetector;
    let text = utf16("The quick brown fox jumps over the lazy dog.");
    let attribute = TextAttribute::from_legacy(0x000a, LegacyColorDefaults::default());
    let cells =
        OutputCellIterator::text_with_attribute(&text, attribute, &detector).collect::<Vec<_>>();

    assert_eq!(cells.len(), text.len());
    for (cell, expected) in cells.iter().zip(text.iter()) {
        assert_eq!(cell.chars(), &[*expected]);
        assert_eq!(cell.dbcs_attribute(), DbcsAttribute::Single);
        assert_eq!(cell.text_attribute(), attribute);
        assert_eq!(
            cell.text_attribute_behavior(),
            TextAttributeBehavior::Stored
        );
    }
}

#[test]
fn microsoft_host_output_cell_full_width_string_with_color_is_stored() {
    let detector = MicrosoftWidthDetector;
    let text = [0x30a2, 0x30a3, 0x30a4, 0x30a5, 0x30a6];
    let attribute = TextAttribute::from_legacy(0x000a, LegacyColorDefaults::default());
    let cells =
        OutputCellIterator::text_with_attribute(&text, attribute, &detector).collect::<Vec<_>>();

    assert_eq!(cells.len(), text.len() * 2);
    for (index, expected) in text.iter().copied().enumerate() {
        let leading = cells[index * 2];
        let trailing = cells[index * 2 + 1];
        assert_eq!(leading.chars(), &[expected]);
        assert_eq!(leading.dbcs_attribute(), DbcsAttribute::Leading);
        assert_eq!(leading.text_attribute(), attribute);
        assert_eq!(
            leading.text_attribute_behavior(),
            TextAttributeBehavior::Stored
        );
        assert_eq!(trailing.chars(), &[expected]);
        assert_eq!(trailing.dbcs_attribute(), DbcsAttribute::Trailing);
        assert_eq!(trailing.text_attribute(), attribute);
        assert_eq!(
            trailing.text_attribute_behavior(),
            TextAttributeBehavior::Stored
        );
    }
}

#[test]
fn microsoft_host_output_cell_mixed_full_width_distance_matches() {
    let detector = MicrosoftWidthDetector;
    let mut text = utf16("QWER");
    text.extend([0x30a2, 0x30a3, 0x30a4, 0x30a5, 0x30a6]);
    text.extend(utf16("TYUI"));
    let mut iterator = OutputCellIterator::text_only(&text, &detector);

    while iterator.next().is_some() {}

    assert_eq!(iterator.position(), text.len());
    assert_eq!(iterator.cell_distance(), text.len() + 5);
}
