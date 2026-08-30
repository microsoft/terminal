//! Safe fill/run forms of the Windows Terminal `OutputCellIterator` contract.
//!
//! The text-backed iterator in `output_cell` owns UTF-16 width expansion. This
//! companion owner covers the remaining constructor families that repeat one
//! logical cell/run or project already-materialized legacy cells without raw
//! pointers or Win32 `CHAR_INFO` storage.

use crate::output_cell::{GlyphWidthDetector, TextAttributeBehavior};
use crate::row::DbcsAttribute;
use crate::text_attribute::{LegacyColorDefaults, TextAttribute};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct LegacyCharInfo {
    pub unicode_char: u16,
    pub attributes: u16,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct OwnedOutputCell {
    chars: Vec<u16>,
    dbcs_attribute: DbcsAttribute,
    text_attribute: TextAttribute,
    behavior: TextAttributeBehavior,
}

impl OwnedOutputCell {
    #[must_use]
    pub fn new(
        chars: Vec<u16>,
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
    pub fn chars(&self) -> &[u16] {
        &self.chars
    }

    #[must_use]
    pub const fn dbcs_attribute(&self) -> DbcsAttribute {
        self.dbcs_attribute
    }

    #[must_use]
    pub const fn text_attribute(&self) -> TextAttribute {
        self.text_attribute
    }

    #[must_use]
    pub const fn text_attribute_behavior(&self) -> TextAttributeBehavior {
        self.behavior
    }
}

/// Owned iterator for repeated fills and materialized runs.
///
/// A finite fill counts logical source items, matching the C++ constructors:
/// one full-width character therefore emits a leading/trailing pair per repeat.
/// `None` means an unlimited fill. Materialized runs use one finite cycle.
pub struct OutputCellRunIterator {
    pattern: Vec<OwnedOutputCell>,
    pattern_position: usize,
    cycles_remaining: Option<usize>,
}

impl OutputCellRunIterator {
    fn repeating(pattern: Vec<OwnedOutputCell>, cycles_remaining: Option<usize>) -> Self {
        Self {
            pattern,
            pattern_position: 0,
            cycles_remaining,
        }
    }

    #[must_use]
    pub fn character_fill<W: GlyphWidthDetector>(
        character: u16,
        detector: &W,
        repeat_limit: Option<usize>,
    ) -> Self {
        let glyph = [character];
        let full_width = detector.is_full_width(&glyph);
        let mut pattern = vec![OwnedOutputCell::new(
            glyph.to_vec(),
            if full_width {
                DbcsAttribute::Leading
            } else {
                DbcsAttribute::Single
            },
            TextAttribute::default(),
            TextAttributeBehavior::Current,
        )];

        if full_width {
            pattern.push(OwnedOutputCell::new(
                glyph.to_vec(),
                DbcsAttribute::Trailing,
                TextAttribute::default(),
                TextAttributeBehavior::Current,
            ));
        }

        Self::repeating(pattern, repeat_limit)
    }

    #[must_use]
    pub fn attribute_fill(attribute: TextAttribute, repeat_limit: Option<usize>) -> Self {
        Self::repeating(
            vec![OwnedOutputCell::new(
                Vec::new(),
                DbcsAttribute::Single,
                attribute,
                TextAttributeBehavior::StoredOnly,
            )],
            repeat_limit,
        )
    }

    #[must_use]
    pub fn text_and_attribute_fill(
        character: u16,
        attribute: TextAttribute,
        repeat_limit: Option<usize>,
    ) -> Self {
        Self::repeating(
            vec![OwnedOutputCell::new(
                vec![character],
                DbcsAttribute::Single,
                attribute,
                TextAttributeBehavior::Stored,
            )],
            repeat_limit,
        )
    }

    #[must_use]
    pub fn char_info_fill(
        char_info: LegacyCharInfo,
        defaults: LegacyColorDefaults,
        repeat_limit: Option<usize>,
    ) -> Self {
        Self::text_and_attribute_fill(
            char_info.unicode_char,
            TextAttribute::from_legacy(char_info.attributes, defaults),
            repeat_limit,
        )
    }

    #[must_use]
    pub fn legacy_color_run(colors: &[u16], defaults: LegacyColorDefaults) -> Self {
        let pattern = colors
            .iter()
            .map(|color| {
                OwnedOutputCell::new(
                    Vec::new(),
                    DbcsAttribute::Single,
                    TextAttribute::from_legacy(*color, defaults),
                    TextAttributeBehavior::StoredOnly,
                )
            })
            .collect();
        Self::repeating(pattern, Some(1))
    }

    #[must_use]
    pub fn legacy_char_info_run(
        char_infos: &[LegacyCharInfo],
        defaults: LegacyColorDefaults,
    ) -> Self {
        let pattern = char_infos
            .iter()
            .map(|char_info| {
                OwnedOutputCell::new(
                    vec![char_info.unicode_char],
                    DbcsAttribute::Single,
                    TextAttribute::from_legacy(char_info.attributes, defaults),
                    TextAttributeBehavior::Stored,
                )
            })
            .collect();
        Self::repeating(pattern, Some(1))
    }

    #[must_use]
    pub fn output_cell_run(cells: &[OwnedOutputCell]) -> Self {
        Self::repeating(cells.to_vec(), Some(1))
    }
}

impl Iterator for OutputCellRunIterator {
    type Item = OwnedOutputCell;

    fn next(&mut self) -> Option<Self::Item> {
        if self.pattern.is_empty() || self.cycles_remaining == Some(0) {
            return None;
        }

        let cell = self.pattern[self.pattern_position].clone();
        self.pattern_position += 1;

        if self.pattern_position == self.pattern.len() {
            self.pattern_position = 0;
            if let Some(remaining) = self.cycles_remaining {
                self.cycles_remaining = Some(remaining - 1);
            }
        }

        Some(cell)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const SHORT_MAX: usize = i16::MAX as usize;
    const FOREGROUND_BLUE: u16 = 0x0001;
    const FOREGROUND_GREEN: u16 = 0x0002;
    const FOREGROUND_RED: u16 = 0x0004;
    const FOREGROUND_INTENSITY: u16 = 0x0008;
    const BACKGROUND_BLUE: u16 = 0x0010;
    const BACKGROUND_GREEN: u16 = 0x0020;

    struct MicrosoftWidthDetector;

    impl GlyphWidthDetector for MicrosoftWidthDetector {
        fn is_full_width(&self, glyph: &[u16]) -> bool {
            glyph == [0x30a2]
        }
    }

    fn legacy_attribute(value: u16) -> TextAttribute {
        TextAttribute::from_legacy(value, LegacyColorDefaults::default())
    }

    fn assert_character_cell(cell: &OwnedOutputCell, character: u16, dbcs: DbcsAttribute) {
        assert_eq!(cell.chars(), &[character]);
        assert_eq!(cell.dbcs_attribute(), dbcs);
        assert_eq!(
            cell.text_attribute_behavior(),
            TextAttributeBehavior::Current
        );
    }

    #[test]
    fn microsoft_output_cell_character_fill_double_width_contract() {
        let detector = MicrosoftWidthDetector;
        let mut iterator = OutputCellRunIterator::character_fill(0x30a2, &detector, Some(5));

        for _ in 0..5 {
            let leading = iterator.next().expect("leading cell");
            assert_character_cell(&leading, 0x30a2, DbcsAttribute::Leading);
            let trailing = iterator.next().expect("trailing cell");
            assert_character_cell(&trailing, 0x30a2, DbcsAttribute::Trailing);
        }
        assert!(iterator.next().is_none());
    }

    #[test]
    fn microsoft_output_cell_character_fill_limited_contract() {
        let detector = MicrosoftWidthDetector;
        let cells = OutputCellRunIterator::character_fill(u16::from(b'Q'), &detector, Some(5))
            .collect::<Vec<_>>();
        assert_eq!(cells.len(), 5);
        for cell in &cells {
            assert_character_cell(cell, u16::from(b'Q'), DbcsAttribute::Single);
        }
    }

    #[test]
    fn microsoft_output_cell_character_fill_unlimited_contract() {
        let detector = MicrosoftWidthDetector;
        let mut iterator = OutputCellRunIterator::character_fill(u16::from(b'Q'), &detector, None);
        for _ in 0..SHORT_MAX {
            assert_character_cell(
                &iterator.next().expect("unlimited character fill"),
                u16::from(b'Q'),
                DbcsAttribute::Single,
            );
        }
        assert!(iterator.next().is_some());
    }

    #[test]
    fn microsoft_output_cell_attribute_fill_limited_contract() {
        let attribute = legacy_attribute(FOREGROUND_RED | BACKGROUND_BLUE);
        let cells = OutputCellRunIterator::attribute_fill(attribute, Some(5)).collect::<Vec<_>>();
        assert_eq!(cells.len(), 5);
        for cell in &cells {
            assert!(cell.chars().is_empty());
            assert_eq!(cell.text_attribute(), attribute);
            assert_eq!(
                cell.text_attribute_behavior(),
                TextAttributeBehavior::StoredOnly
            );
        }
    }

    #[test]
    fn microsoft_output_cell_attribute_fill_unlimited_contract() {
        let attribute = legacy_attribute(FOREGROUND_RED | BACKGROUND_BLUE);
        let mut iterator = OutputCellRunIterator::attribute_fill(attribute, None);
        for _ in 0..SHORT_MAX {
            let cell = iterator.next().expect("unlimited attribute fill");
            assert!(cell.chars().is_empty());
            assert_eq!(cell.text_attribute(), attribute);
            assert_eq!(
                cell.text_attribute_behavior(),
                TextAttributeBehavior::StoredOnly
            );
        }
        assert!(iterator.next().is_some());
    }

    #[test]
    fn microsoft_output_cell_text_and_attribute_fill_limited_contract() {
        let attribute = legacy_attribute(FOREGROUND_RED | BACKGROUND_BLUE);
        let cells =
            OutputCellRunIterator::text_and_attribute_fill(u16::from(b'Q'), attribute, Some(5))
                .collect::<Vec<_>>();
        assert_eq!(cells.len(), 5);
        for cell in &cells {
            assert_eq!(cell.chars(), &[u16::from(b'Q')]);
            assert_eq!(cell.text_attribute(), attribute);
            assert_eq!(
                cell.text_attribute_behavior(),
                TextAttributeBehavior::Stored
            );
        }
    }

    #[test]
    fn microsoft_output_cell_text_and_attribute_fill_unlimited_contract() {
        let attribute = legacy_attribute(FOREGROUND_RED | BACKGROUND_BLUE);
        let mut iterator =
            OutputCellRunIterator::text_and_attribute_fill(u16::from(b'Q'), attribute, None);
        for _ in 0..SHORT_MAX {
            let cell = iterator.next().expect("unlimited text/attribute fill");
            assert_eq!(cell.chars(), &[u16::from(b'Q')]);
            assert_eq!(cell.text_attribute(), attribute);
            assert_eq!(
                cell.text_attribute_behavior(),
                TextAttributeBehavior::Stored
            );
        }
        assert!(iterator.next().is_some());
    }

    #[test]
    fn microsoft_output_cell_char_info_fill_limited_contract() {
        let defaults = LegacyColorDefaults::default();
        let char_info = LegacyCharInfo {
            unicode_char: u16::from(b'Q'),
            attributes: FOREGROUND_RED | BACKGROUND_BLUE,
        };
        let cells =
            OutputCellRunIterator::char_info_fill(char_info, defaults, Some(5)).collect::<Vec<_>>();
        assert_eq!(cells.len(), 5);
        for cell in &cells {
            assert_eq!(cell.chars(), &[u16::from(b'Q')]);
            assert_eq!(
                cell.text_attribute(),
                legacy_attribute(char_info.attributes)
            );
            assert_eq!(
                cell.text_attribute_behavior(),
                TextAttributeBehavior::Stored
            );
        }
    }

    #[test]
    fn microsoft_output_cell_char_info_fill_unlimited_contract() {
        let defaults = LegacyColorDefaults::default();
        let char_info = LegacyCharInfo {
            unicode_char: u16::from(b'Q'),
            attributes: FOREGROUND_RED | BACKGROUND_BLUE,
        };
        let mut iterator = OutputCellRunIterator::char_info_fill(char_info, defaults, None);
        for _ in 0..SHORT_MAX {
            let cell = iterator.next().expect("unlimited CHAR_INFO fill");
            assert_eq!(cell.chars(), &[u16::from(b'Q')]);
            assert_eq!(
                cell.text_attribute(),
                legacy_attribute(char_info.attributes)
            );
            assert_eq!(
                cell.text_attribute_behavior(),
                TextAttributeBehavior::Stored
            );
        }
        assert!(iterator.next().is_some());
    }

    #[test]
    fn microsoft_output_cell_legacy_color_data_run_contract() {
        let colors = [
            FOREGROUND_GREEN,
            FOREGROUND_RED | BACKGROUND_BLUE,
            FOREGROUND_BLUE | FOREGROUND_INTENSITY,
            BACKGROUND_GREEN,
        ];
        let cells =
            OutputCellRunIterator::legacy_color_run(&colors, LegacyColorDefaults::default())
                .collect::<Vec<_>>();
        assert_eq!(cells.len(), colors.len());
        for (cell, color) in cells.iter().zip(colors) {
            assert!(cell.chars().is_empty());
            assert_eq!(cell.text_attribute(), legacy_attribute(color));
            assert_eq!(
                cell.text_attribute_behavior(),
                TextAttributeBehavior::StoredOnly
            );
        }
    }

    #[test]
    fn microsoft_output_cell_legacy_char_info_run_contract() {
        let infos = (0u16..5)
            .map(|index| LegacyCharInfo {
                unicode_char: u16::from(b'A') + index,
                attributes: index,
            })
            .collect::<Vec<_>>();
        let cells =
            OutputCellRunIterator::legacy_char_info_run(&infos, LegacyColorDefaults::default())
                .collect::<Vec<_>>();
        assert_eq!(cells.len(), infos.len());
        for (cell, info) in cells.iter().zip(&infos) {
            assert_eq!(cell.chars(), &[info.unicode_char]);
            assert_eq!(cell.text_attribute(), legacy_attribute(info.attributes));
            assert_eq!(
                cell.text_attribute_behavior(),
                TextAttributeBehavior::Stored
            );
        }
    }

    #[test]
    fn microsoft_output_cell_run_contract() {
        let pair = vec![0xd834, 0xdd1e];
        let source = (0u16..5)
            .map(|index| {
                OwnedOutputCell::new(
                    pair.clone(),
                    DbcsAttribute::Single,
                    legacy_attribute(index),
                    TextAttributeBehavior::Stored,
                )
            })
            .collect::<Vec<_>>();
        let cells = OutputCellRunIterator::output_cell_run(&source).collect::<Vec<_>>();
        assert_eq!(cells, source);
    }
}
