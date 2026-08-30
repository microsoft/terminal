//! Safe output-cell views and UTF-16 text iteration.
//!
//! The C++ `OutputCellIterator` expands a full-width glyph into a leading and
//! trailing cell while advancing the underlying UTF-16 input only once. This
//! module preserves that contract without storing raw pointers or mutable
//! borrowed views.

use crate::row::DbcsAttribute;
use crate::text_attribute::TextAttribute;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum TextAttributeBehavior {
    /// Preserve the attribute already stored in the destination buffer.
    #[default]
    Current,
    /// Store both text and the supplied attribute.
    Stored,
    /// Store only the supplied attribute and preserve destination text.
    StoredOnly,
}

/// Width policy used while turning UTF-16 glyphs into output cells.
///
/// R04 keeps width detection behind an explicit dependency so the buffer core
/// remains platform-neutral. The terminal host can later connect this to the
/// migrated `CodepointWidthDetector` without introducing process-global state.
pub trait GlyphWidthDetector {
    fn is_full_width(&self, glyph: &[u16]) -> bool;
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OutputCellView<'a> {
    chars: &'a [u16],
    dbcs_attribute: DbcsAttribute,
    text_attribute: TextAttribute,
    behavior: TextAttributeBehavior,
}

impl<'a> OutputCellView<'a> {
    #[must_use]
    pub const fn new(
        chars: &'a [u16],
        dbcs_attribute: DbcsAttribute,
        text_attribute: TextAttribute,
        behavior: TextAttributeBehavior,
    ) -> Self {
        Self {
            chars,
            dbcs_attribute,
            text_attribute,
            behavior,
        }
    }

    #[must_use]
    pub const fn chars(self) -> &'a [u16] {
        self.chars
    }

    #[must_use]
    pub const fn columns(self) -> u16 {
        if matches!(self.dbcs_attribute, DbcsAttribute::Leading) {
            2
        } else {
            1
        }
    }

    #[must_use]
    pub const fn dbcs_attribute(self) -> DbcsAttribute {
        self.dbcs_attribute
    }

    #[must_use]
    pub const fn text_attribute(self) -> TextAttribute {
        self.text_attribute
    }

    #[must_use]
    pub const fn text_attribute_behavior(self) -> TextAttributeBehavior {
        self.behavior
    }
}

/// Safe text-backed subset of the C++ `OutputCellIterator` contract.
///
/// The iterator reports one item per destination cell. A full-width glyph is
/// therefore returned twice: first as `Leading`, then as `Trailing`. Its UTF-16
/// input position advances only after the trailing cell has been emitted.
pub struct OutputCellIterator<'a, W: GlyphWidthDetector> {
    text: &'a [u16],
    detector: &'a W,
    attribute: TextAttribute,
    behavior: TextAttributeBehavior,
    input_position: usize,
    cell_distance: usize,
    pending_trailing: bool,
    fill_limit: Option<usize>,
}

impl<'a, W: GlyphWidthDetector> OutputCellIterator<'a, W> {
    #[must_use]
    pub fn text_only(text: &'a [u16], detector: &'a W) -> Self {
        Self {
            text,
            detector,
            attribute: TextAttribute::default(),
            behavior: TextAttributeBehavior::Current,
            input_position: 0,
            cell_distance: 0,
            pending_trailing: false,
            fill_limit: None,
        }
    }

    #[must_use]
    pub const fn text_with_attribute(
        text: &'a [u16],
        attribute: TextAttribute,
        detector: &'a W,
    ) -> Self {
        Self {
            text,
            detector,
            attribute,
            behavior: TextAttributeBehavior::Stored,
            input_position: 0,
            cell_distance: 0,
            pending_trailing: false,
            fill_limit: None,
        }
    }

    /// Limits the number of destination cells emitted from the text run.
    #[must_use]
    pub const fn with_fill_limit(mut self, fill_limit: usize) -> Self {
        self.fill_limit = if fill_limit == 0 {
            None
        } else {
            Some(fill_limit)
        };
        self
    }

    #[must_use]
    pub const fn position(&self) -> usize {
        self.input_position
    }

    #[must_use]
    pub const fn cell_distance(&self) -> usize {
        self.cell_distance
    }

    fn current_glyph(&self) -> &'a [u16] {
        let remaining = &self.text[self.input_position..];
        let length = utf16_glyph_length(remaining);
        &remaining[..length]
    }
}

impl<'a, W: GlyphWidthDetector> Iterator for OutputCellIterator<'a, W> {
    type Item = OutputCellView<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self
            .fill_limit
            .is_some_and(|limit| self.cell_distance >= limit)
            || self.input_position >= self.text.len()
        {
            return None;
        }

        let glyph = self.current_glyph();
        if self.pending_trailing {
            self.pending_trailing = false;
            self.input_position += glyph.len();
            self.cell_distance += 1;
            return Some(OutputCellView::new(
                glyph,
                DbcsAttribute::Trailing,
                self.attribute,
                self.behavior,
            ));
        }

        let full_width = self.detector.is_full_width(glyph);
        if full_width {
            self.pending_trailing = true;
        } else {
            self.input_position += glyph.len();
        }
        self.cell_distance += 1;

        Some(OutputCellView::new(
            glyph,
            if full_width {
                DbcsAttribute::Leading
            } else {
                DbcsAttribute::Single
            },
            self.attribute,
            self.behavior,
        ))
    }
}

const fn utf16_glyph_length(input: &[u16]) -> usize {
    if input.is_empty() {
        return 0;
    }
    if input.len() >= 2 && is_high_surrogate(input[0]) && is_low_surrogate(input[1]) {
        2
    } else {
        1
    }
}

const fn is_high_surrogate(unit: u16) -> bool {
    unit >= 0xd800 && unit <= 0xdbff
}

const fn is_low_surrogate(unit: u16) -> bool {
    unit >= 0xdc00 && unit <= 0xdfff
}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestWidthDetector;

    impl GlyphWidthDetector for TestWidthDetector {
        fn is_full_width(&self, glyph: &[u16]) -> bool {
            glyph == [0x4e00]
        }
    }

    #[test]
    fn narrow_utf16_text_consumes_one_input_glyph_per_cell() {
        let detector = TestWidthDetector;
        let text = [u16::from(b'A'), u16::from(b'B')];
        let mut iterator = OutputCellIterator::text_only(&text, &detector);

        let first = iterator.next().expect("first cell");
        assert_eq!(first.chars(), &[u16::from(b'A')]);
        assert_eq!(first.dbcs_attribute(), DbcsAttribute::Single);
        assert_eq!(iterator.position(), 1);
        assert_eq!(iterator.cell_distance(), 1);

        let second = iterator.next().expect("second cell");
        assert_eq!(second.chars(), &[u16::from(b'B')]);
        assert_eq!(iterator.position(), 2);
        assert_eq!(iterator.cell_distance(), 2);
        assert!(iterator.next().is_none());
    }

    #[test]
    fn full_width_glyph_emits_leading_then_trailing_without_double_consumption() {
        let detector = TestWidthDetector;
        let text = [0x4e00, u16::from(b'X')];
        let mut iterator = OutputCellIterator::text_only(&text, &detector);

        let leading = iterator.next().expect("leading cell");
        assert_eq!(leading.chars(), &[0x4e00]);
        assert_eq!(leading.dbcs_attribute(), DbcsAttribute::Leading);
        assert_eq!(leading.columns(), 2);
        assert_eq!(iterator.position(), 0);

        let trailing = iterator.next().expect("trailing cell");
        assert_eq!(trailing.chars(), &[0x4e00]);
        assert_eq!(trailing.dbcs_attribute(), DbcsAttribute::Trailing);
        assert_eq!(iterator.position(), 1);

        let next = iterator.next().expect("next glyph");
        assert_eq!(next.chars(), &[u16::from(b'X')]);
        assert_eq!(iterator.position(), 2);
    }

    #[test]
    fn surrogate_pair_is_one_input_glyph() {
        let detector = TestWidthDetector;
        let text = [0xd83d, 0xde00, u16::from(b'!')];
        let mut iterator = OutputCellIterator::text_only(&text, &detector);

        let smile = iterator.next().expect("surrogate glyph");
        assert_eq!(smile.chars(), &[0xd83d, 0xde00]);
        assert_eq!(iterator.position(), 2);
        assert_eq!(
            iterator.next().expect("punctuation").chars(),
            &[u16::from(b'!')]
        );
    }

    #[test]
    fn attributed_text_requests_stored_attribute_behavior() {
        let detector = TestWidthDetector;
        let text = [u16::from(b'A')];
        let mut attribute = TextAttribute::default();
        attribute.set_intense(true);
        let view = OutputCellIterator::text_with_attribute(&text, attribute, &detector)
            .next()
            .expect("cell");

        assert_eq!(
            view.text_attribute_behavior(),
            TextAttributeBehavior::Stored
        );
        assert_eq!(view.text_attribute(), attribute);
    }

    #[test]
    fn fill_limit_counts_destination_cells_including_wide_trailing_cell() {
        let detector = TestWidthDetector;
        let text = [0x4e00, u16::from(b'X')];
        let cells = OutputCellIterator::text_only(&text, &detector)
            .with_fill_limit(2)
            .collect::<Vec<_>>();

        assert_eq!(cells.len(), 2);
        assert_eq!(cells[0].dbcs_attribute(), DbcsAttribute::Leading);
        assert_eq!(cells[1].dbcs_attribute(), DbcsAttribute::Trailing);
    }
}
