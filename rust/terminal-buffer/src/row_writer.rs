//! Bulk row writes driven by safe `OutputCellView` values.

use crate::output_cell::{OutputCellIterator, OutputCellView, TextAttributeBehavior};
use crate::row::{DbcsAttribute, Row, RowError};
use crate::text_attribute::TextAttribute;
use crate::width_detector::CodepointWidthDetector;

/// Writes output-cell views into one row and returns the first untouched column.
///
/// Full-width text is written on its leading cell. The corresponding trailing
/// view consumes the second destination column without duplicating the glyph in
/// UTF-16 storage, matching the C++ iterator/ROW split of responsibilities.
///
/// # Errors
///
/// Propagates row storage errors, including a full-width glyph that cannot fit
/// in the final column.
pub fn write_cells<'a, I>(row: &mut Row, start_column: i32, cells: I) -> Result<u16, RowError>
where
    I: IntoIterator<Item = OutputCellView<'a>>,
{
    let start = start_column.clamp(0, i32::from(row.size()));
    let mut column = u16::try_from(start).unwrap_or_default();

    for cell in cells {
        if column >= row.size() {
            break;
        }

        let next_column = column.saturating_add(1);
        match cell.text_attribute_behavior() {
            TextAttributeBehavior::Current => {}
            TextAttributeBehavior::Stored | TextAttributeBehavior::StoredOnly => {
                row.replace_attributes(
                    i32::from(column),
                    i32::from(next_column),
                    cell.text_attribute(),
                );
            }
        }

        if !matches!(
            cell.text_attribute_behavior(),
            TextAttributeBehavior::StoredOnly
        ) {
            match cell.dbcs_attribute() {
                DbcsAttribute::Single => {
                    row.replace_glyph(i32::from(column), 1, cell.chars())?;
                }
                DbcsAttribute::Leading => {
                    row.replace_glyph(i32::from(column), 2, cell.chars())?;
                }
                DbcsAttribute::Trailing => {
                    // The leading view already stored the glyph across both columns.
                }
            }
        }

        column = next_column;
    }

    Ok(column)
}

/// Replaces UTF-16 text using the deterministic Unicode width policy while
/// preserving the row's existing attributes.
///
/// This is the safe Rust equivalent of the common C++ `ReplaceText` path: the
/// caller supplies UTF-16 text, scalar width is resolved centrally, wide glyphs
/// consume two cells, and the first untouched destination column is returned.
///
/// # Errors
///
/// Propagates row-storage errors if a glyph cannot be represented in the row.
pub fn replace_text(row: &mut Row, start_column: i32, text: &[u16]) -> Result<u16, RowError> {
    let detector = CodepointWidthDetector;
    write_cells(
        row,
        start_column,
        OutputCellIterator::text_only(text, &detector),
    )
}

/// Replaces UTF-16 text and stores one attribute across the written cells.
///
/// # Errors
///
/// Propagates row-storage errors if a glyph cannot be represented in the row.
pub fn replace_text_with_attribute(
    row: &mut Row,
    start_column: i32,
    text: &[u16],
    attribute: TextAttribute,
) -> Result<u16, RowError> {
    let detector = CodepointWidthDetector;
    write_cells(
        row,
        start_column,
        OutputCellIterator::text_with_attribute(text, attribute, &detector),
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::output_cell::GlyphWidthDetector;

    struct TestWidthDetector;

    impl GlyphWidthDetector for TestWidthDetector {
        fn is_full_width(&self, glyph: &[u16]) -> bool {
            glyph == [0x4e00]
        }
    }

    fn row(width: u16) -> Row {
        Row::new(width, TextAttribute::default()).expect("valid test row")
    }

    #[test]
    fn bulk_write_preserves_current_attributes_for_text_only_input() {
        let detector = TestWidthDetector;
        let mut row = row(5);
        let mut highlighted = TextAttribute::default();
        highlighted.set_intense(true);
        row.replace_attributes(0, 5, highlighted);

        let text = [u16::from(b'A'), u16::from(b'B')];
        let end = write_cells(&mut row, 1, OutputCellIterator::text_only(&text, &detector))
            .expect("bulk write succeeds");

        assert_eq!(end, 3);
        assert_eq!(row.glyph_at(1), &[u16::from(b'A')]);
        assert_eq!(row.glyph_at(2), &[u16::from(b'B')]);
        assert!(row.attribute_at(1).is_intense());
        assert!(row.attribute_at(2).is_intense());
    }

    #[test]
    fn bulk_write_stores_attribute_with_text_when_requested() {
        let detector = TestWidthDetector;
        let mut row = row(4);
        let mut highlighted = TextAttribute::default();
        highlighted.set_intense(true);
        let text = [u16::from(b'X')];

        write_cells(
            &mut row,
            2,
            OutputCellIterator::text_with_attribute(&text, highlighted, &detector),
        )
        .expect("bulk write succeeds");

        assert_eq!(row.glyph_at(2), &[u16::from(b'X')]);
        assert!(row.attribute_at(2).is_intense());
    }

    #[test]
    fn full_width_iterator_and_row_writer_consume_exactly_two_columns() {
        let detector = TestWidthDetector;
        let mut row = row(5);
        let text = [0x4e00, u16::from(b'Z')];

        let end = write_cells(&mut row, 1, OutputCellIterator::text_only(&text, &detector))
            .expect("bulk write succeeds");

        assert_eq!(end, 4);
        assert_eq!(row.glyph_at(1), &[0x4e00]);
        assert_eq!(row.dbcs_attribute_at(1), DbcsAttribute::Leading);
        assert_eq!(row.dbcs_attribute_at(2), DbcsAttribute::Trailing);
        assert_eq!(row.glyph_at(3), &[u16::from(b'Z')]);
    }

    #[test]
    fn bulk_write_stops_at_row_boundary_without_touching_later_input() {
        let detector = TestWidthDetector;
        let mut row = row(3);
        let text = [u16::from(b'A'), u16::from(b'B')];

        let end = write_cells(&mut row, 2, OutputCellIterator::text_only(&text, &detector))
            .expect("bulk write succeeds");

        assert_eq!(end, 3);
        assert_eq!(row.glyph_at(2), &[u16::from(b'A')]);
    }

    #[test]
    fn replace_text_uses_concrete_unicode_width_detection() {
        let mut row = row(6);
        let text = [u16::from(b'A'), 0x754c, u16::from(b'B')];

        let end = replace_text(&mut row, 1, &text).expect("replace text succeeds");

        assert_eq!(end, 5);
        assert_eq!(row.glyph_at(1), &[u16::from(b'A')]);
        assert_eq!(row.glyph_at(2), &[0x754c]);
        assert_eq!(row.dbcs_attribute_at(2), DbcsAttribute::Leading);
        assert_eq!(row.dbcs_attribute_at(3), DbcsAttribute::Trailing);
        assert_eq!(row.glyph_at(4), &[u16::from(b'B')]);
    }

    #[test]
    fn replace_text_handles_supplementary_emoji_as_one_wide_glyph() {
        let mut row = row(5);
        let rocket = [0xd83d, 0xde80];

        let end = replace_text(&mut row, 1, &rocket).expect("replace text succeeds");

        assert_eq!(end, 3);
        assert_eq!(row.glyph_at(1), &rocket);
        assert_eq!(row.dbcs_attribute_at(1), DbcsAttribute::Leading);
        assert_eq!(row.dbcs_attribute_at(2), DbcsAttribute::Trailing);
    }

    #[test]
    fn replace_text_with_attribute_updates_both_cells_of_wide_glyph() {
        let mut row = row(4);
        let mut highlighted = TextAttribute::default();
        highlighted.set_intense(true);

        replace_text_with_attribute(&mut row, 1, &[0x754c], highlighted)
            .expect("replace text succeeds");

        assert!(row.attribute_at(1).is_intense());
        assert!(row.attribute_at(2).is_intense());
    }
}
