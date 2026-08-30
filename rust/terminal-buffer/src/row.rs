//! Safe owned storage for the deterministic core of Windows Terminal `ROW`.
//!
//! The C++ row packs a UTF-16 character buffer beside a column-to-character
//! offset table. The high bit marks columns that trail a wide glyph. This Rust
//! port keeps that observable representation while owning both buffers and
//! validating every offset update before it is committed.

use crate::line_rendition::LineRendition;
use crate::text_attribute::TextAttribute;

const CHAR_OFFSETS_TRAILER: u16 = 0x8000;
const CHAR_OFFSETS_MASK: u16 = 0x7fff;
const UNICODE_SPACE: u16 = 0x20;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DbcsAttribute {
    Single,
    Leading,
    Trailing,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DelimiterClass {
    ControlChar,
    DelimiterChar,
    RegularChar,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DirtyRange {
    pub begin: u16,
    pub end: u16,
}

impl DirtyRange {
    #[must_use]
    pub const fn empty(column: u16) -> Self {
        Self {
            begin: column,
            end: column,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RowError {
    EmptyRow,
    ColumnCountTooLarge,
    InvalidGlyphWidth,
    EmptyGlyph,
    GlyphDoesNotFit,
    CharacterStorageOverflow,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Row {
    chars: Vec<u16>,
    char_offsets: Vec<u16>,
    attributes: Vec<TextAttribute>,
    column_count: u16,
    line_rendition: LineRendition,
    wrap_forced: bool,
    double_byte_padded: bool,
}

impl Row {
    /// Creates one initialized row filled with spaces and one attribute.
    ///
    /// # Errors
    ///
    /// Returns an error for zero-width rows or widths that cannot fit in the
    /// 15-bit character-offset representation used by Windows Terminal.
    pub fn new(column_count: u16, fill_attribute: TextAttribute) -> Result<Self, RowError> {
        if column_count == 0 {
            return Err(RowError::EmptyRow);
        }
        if column_count > CHAR_OFFSETS_MASK {
            return Err(RowError::ColumnCountTooLarge);
        }

        let width = usize::from(column_count);
        let mut char_offsets = Vec::with_capacity(width + 1);
        for offset in 0..=column_count {
            char_offsets.push(offset);
        }

        Ok(Self {
            chars: vec![UNICODE_SPACE; width],
            char_offsets,
            attributes: vec![fill_attribute; width],
            column_count,
            line_rendition: LineRendition::SingleWidth,
            wrap_forced: false,
            double_byte_padded: false,
        })
    }

    pub fn reset(&mut self, fill_attribute: TextAttribute) {
        let width = usize::from(self.column_count);
        self.chars.clear();
        self.chars.resize(width, UNICODE_SPACE);
        self.char_offsets.clear();
        self.char_offsets.extend(0..=self.column_count);
        self.attributes.fill(fill_attribute);
        self.line_rendition = LineRendition::SingleWidth;
        self.wrap_forced = false;
        self.double_byte_padded = false;
    }

    #[must_use]
    pub const fn size(&self) -> u16 {
        self.column_count
    }

    pub const fn set_wrap_forced(&mut self, wrap: bool) {
        self.wrap_forced = wrap;
    }

    #[must_use]
    pub const fn was_wrap_forced(&self) -> bool {
        self.wrap_forced
    }

    pub const fn set_double_byte_padded(&mut self, padded: bool) {
        self.double_byte_padded = padded;
    }

    #[must_use]
    pub const fn was_double_byte_padded(&self) -> bool {
        self.double_byte_padded
    }

    pub const fn set_line_rendition(&mut self, rendition: LineRendition) {
        self.line_rendition = rendition;
    }

    #[must_use]
    pub const fn line_rendition(&self) -> LineRendition {
        self.line_rendition
    }

    #[must_use]
    pub const fn readable_column_count(&self) -> u16 {
        let padding = if self.double_byte_padded { 1 } else { 0 };
        if matches!(self.line_rendition, LineRendition::SingleWidth) {
            self.column_count.saturating_sub(padding)
        } else {
            self.column_count.saturating_sub(padding << 1) >> 1
        }
    }

    #[must_use]
    pub fn attributes(&self) -> &[TextAttribute] {
        &self.attributes
    }

    #[must_use]
    pub fn attribute_at(&self, column: i32) -> TextAttribute {
        self.attributes[usize::from(self.clamped_column(column))]
    }

    pub fn set_attr_to_end(&mut self, column_begin: i32, attribute: TextAttribute) {
        let begin = usize::from(self.clamped_column_inclusive(column_begin));
        self.attributes[begin..].fill(attribute);
    }

    pub fn replace_attributes(&mut self, begin: i32, end: i32, attribute: TextAttribute) {
        let begin = self.clamped_column_inclusive(begin);
        let end = self.clamped_column_inclusive(end).max(begin);
        self.attributes[usize::from(begin)..usize::from(end)].fill(attribute);
    }

    #[must_use]
    pub fn navigate_to_previous(&self, column: i32) -> u16 {
        self.adjust_backward(self.clamped_column(column.saturating_sub(1)))
    }

    #[must_use]
    pub fn navigate_to_next(&self, column: i32) -> u16 {
        self.adjust_forward(self.clamped_column_inclusive(column.saturating_add(1)))
    }

    #[must_use]
    pub fn adjust_to_glyph_start(&self, column: i32) -> u16 {
        self.adjust_backward(self.clamped_column(column))
    }

    #[must_use]
    pub fn adjust_to_glyph_end(&self, column: i32) -> u16 {
        self.adjust_forward(self.clamped_column_inclusive(column))
    }

    /// Clears one cell while repairing a clipped wide glyph when necessary.
    ///
    /// # Errors
    ///
    /// Returns an error only if the row's validated character-offset capacity
    /// would be exceeded.
    pub fn clear_cell(&mut self, column: i32) -> Result<DirtyRange, RowError> {
        self.replace_glyph(column, 1, &[UNICODE_SPACE])
    }

    /// Replaces one one- or two-column glyph while preserving the C++ row's
    /// behavior when the write intersects an existing wide glyph: any clipped
    /// leading/trailing portion is replaced with spaces.
    ///
    /// # Errors
    ///
    /// Returns an error for invalid/empty glyphs, a two-column glyph that does
    /// not fit, or a replacement whose UTF-16 storage exceeds the 15-bit offset
    /// representation.
    pub fn replace_glyph(
        &mut self,
        column_begin: i32,
        width: u16,
        glyph: &[u16],
    ) -> Result<DirtyRange, RowError> {
        if !(1..=2).contains(&width) {
            return Err(RowError::InvalidGlyphWidth);
        }
        if glyph.is_empty() {
            return Err(RowError::EmptyGlyph);
        }

        let column_begin = self.clamped_column_inclusive(column_begin);
        if column_begin == self.column_count {
            return Ok(DirtyRange::empty(column_begin));
        }
        let requested_end = column_begin
            .checked_add(width)
            .ok_or(RowError::GlyphDoesNotFit)?;
        if requested_end > self.column_count {
            return Err(RowError::GlyphDoesNotFit);
        }

        let dirty_begin = self.adjust_backward(column_begin);
        let dirty_end = self.adjust_forward(requested_end);
        let leading_spaces = column_begin - dirty_begin;
        let trailing_spaces = dirty_end - requested_end;

        let old_char_begin = usize::from(self.char_offset_raw(dirty_begin));
        let old_char_end = usize::from(self.char_offset_raw(dirty_end));
        let old_segment_len = old_char_end - old_char_begin;
        let new_segment_len = usize::from(leading_spaces)
            .saturating_add(glyph.len())
            .saturating_add(usize::from(trailing_spaces));
        let new_char_len = self
            .chars
            .len()
            .saturating_sub(old_segment_len)
            .saturating_add(new_segment_len);
        if new_char_len > usize::from(CHAR_OFFSETS_MASK) {
            return Err(RowError::CharacterStorageOverflow);
        }

        let difference = i32::try_from(new_segment_len)
            .map_err(|_| RowError::CharacterStorageOverflow)?
            - i32::try_from(old_segment_len).map_err(|_| RowError::CharacterStorageOverflow)?;
        let mut new_offsets = self.char_offsets.clone();
        let mut character = old_char_begin;

        for column in dirty_begin..column_begin {
            new_offsets[usize::from(column)] = to_offset(character)?;
            character += 1;
        }

        new_offsets[usize::from(column_begin)] = to_offset(character)?;
        for column in column_begin + 1..requested_end {
            new_offsets[usize::from(column)] = to_offset(character)? | CHAR_OFFSETS_TRAILER;
        }
        character += glyph.len();

        for column in requested_end..dirty_end {
            new_offsets[usize::from(column)] = to_offset(character)?;
            character += 1;
        }
        new_offsets[usize::from(dirty_end)] = to_offset(character)?;

        for column in dirty_end + 1..=self.column_count {
            let raw = self.char_offsets[usize::from(column)];
            let trailer = raw & CHAR_OFFSETS_TRAILER;
            let old_offset = i32::from(raw & CHAR_OFFSETS_MASK);
            let shifted = old_offset.saturating_add(difference);
            let shifted =
                usize::try_from(shifted).map_err(|_| RowError::CharacterStorageOverflow)?;
            new_offsets[usize::from(column)] = to_offset(shifted)? | trailer;
        }

        let mut segment = Vec::with_capacity(new_segment_len);
        segment.resize(usize::from(leading_spaces), UNICODE_SPACE);
        segment.extend_from_slice(glyph);
        segment.resize(new_segment_len, UNICODE_SPACE);

        self.chars.splice(old_char_begin..old_char_end, segment);
        self.char_offsets = new_offsets;

        debug_assert_eq!(
            self.char_offset_raw(self.column_count),
            u16::try_from(self.chars.len()).unwrap_or(u16::MAX)
        );
        debug_assert!(!self.is_trailer(0));
        debug_assert!(!self.is_trailer(self.column_count));

        Ok(DirtyRange {
            begin: dirty_begin,
            end: dirty_end,
        })
    }

    #[must_use]
    pub fn glyph_at(&self, column: i32) -> &[u16] {
        let column = self.clamped_column(column);
        let begin = usize::from(self.char_offset_raw(column));
        let mut end_column = column + 1;
        while self.is_trailer(end_column) {
            end_column += 1;
        }
        let end = usize::from(self.char_offset_raw(end_column));
        &self.chars[begin..end]
    }

    #[must_use]
    pub fn dbcs_attribute_at(&self, column: i32) -> DbcsAttribute {
        let column = self.clamped_column(column);
        if self.is_trailer(column) {
            DbcsAttribute::Trailing
        } else if self.is_trailer(column + 1) {
            DbcsAttribute::Leading
        } else {
            DbcsAttribute::Single
        }
    }

    #[must_use]
    pub fn text(&self) -> &[u16] {
        let end = usize::from(self.char_offset_raw(self.readable_column_count()));
        &self.chars[..end]
    }

    #[must_use]
    pub fn text_range(&self, column_begin: i32, column_end: i32) -> &[u16] {
        let columns = i32::from(self.readable_column_count());
        let begin = column_begin.clamp(0, columns);
        let end = column_end.clamp(begin, columns);
        let begin = u16::try_from(begin).unwrap_or_default();
        let end = u16::try_from(end).unwrap_or_default();
        let char_begin = usize::from(self.char_offset_raw(begin));
        let char_end = usize::from(self.char_offset_raw(end));
        &self.chars[char_begin..char_end]
    }

    #[must_use]
    pub fn leading_column_at_char_offset(&self, offset: isize) -> u16 {
        let target = usize::try_from(offset.max(0))
            .unwrap_or_default()
            .min(self.chars.len());
        let mut result = 0;
        for column in 0..=self.column_count {
            if self.is_trailer(column) {
                continue;
            }
            let current = usize::from(self.char_offset_raw(column));
            if current > target {
                break;
            }
            result = column;
        }
        result.min(self.column_count)
    }

    #[must_use]
    pub fn trailing_column_at_char_offset(&self, offset: isize) -> u16 {
        let mut column = self.leading_column_at_char_offset(offset);
        while column < self.column_count && self.is_trailer(column + 1) {
            column += 1;
        }
        column
    }

    #[must_use]
    pub fn char_offset(&self, column: i32) -> u16 {
        let column = column.clamp(0, i32::from(self.readable_column_count()));
        self.char_offset_raw(u16::try_from(column).unwrap_or_default())
    }

    #[must_use]
    pub fn get_last_non_space_column(&self) -> u16 {
        let text = self.text();
        let trailing_spaces = text
            .iter()
            .rev()
            .take_while(|&&unit| unit == UNICODE_SPACE)
            .count();
        self.readable_column_count()
            .saturating_sub(u16::try_from(trailing_spaces).unwrap_or(u16::MAX))
    }

    #[must_use]
    pub fn measure_left(&self) -> u16 {
        let leading_spaces = self
            .text()
            .iter()
            .take_while(|&&unit| unit == UNICODE_SPACE)
            .count();
        u16::try_from(leading_spaces).unwrap_or(u16::MAX)
    }

    #[must_use]
    pub fn measure_right(&self) -> u16 {
        if self.wrap_forced {
            self.column_count
                .saturating_sub(u16::from(self.double_byte_padded))
        } else {
            self.get_last_non_space_column()
        }
    }

    #[must_use]
    pub fn contains_text(&self) -> bool {
        self.text().iter().any(|&unit| unit != UNICODE_SPACE)
    }

    #[must_use]
    pub fn delimiter_class_at(&self, column: i32, word_delimiters: &[u16]) -> DelimiterClass {
        let glyph = self.glyph_at(column);
        let first = glyph.first().copied().unwrap_or_default();
        if first <= UNICODE_SPACE {
            DelimiterClass::ControlChar
        } else if word_delimiters.contains(&first) {
            DelimiterClass::DelimiterChar
        } else {
            DelimiterClass::RegularChar
        }
    }

    fn clamped_column(&self, column: i32) -> u16 {
        u16::try_from(column.clamp(0, i32::from(self.column_count) - 1)).unwrap_or_default()
    }

    fn clamped_column_inclusive(&self, column: i32) -> u16 {
        u16::try_from(column.clamp(0, i32::from(self.column_count))).unwrap_or_default()
    }

    fn char_offset_raw(&self, column: u16) -> u16 {
        self.char_offsets[usize::from(column)] & CHAR_OFFSETS_MASK
    }

    fn is_trailer(&self, column: u16) -> bool {
        self.char_offsets[usize::from(column)] & CHAR_OFFSETS_TRAILER != 0
    }

    fn adjust_backward(&self, mut column: u16) -> u16 {
        while self.is_trailer(column) {
            column -= 1;
        }
        column
    }

    fn adjust_forward(&self, mut column: u16) -> u16 {
        while self.is_trailer(column) {
            column += 1;
        }
        column
    }
}

fn to_offset(value: usize) -> Result<u16, RowError> {
    let offset = u16::try_from(value).map_err(|_| RowError::CharacterStorageOverflow)?;
    if offset > CHAR_OFFSETS_MASK {
        Err(RowError::CharacterStorageOverflow)
    } else {
        Ok(offset)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn row(width: u16) -> Row {
        Row::new(width, TextAttribute::default()).expect("valid test row")
    }

    #[test]
    fn reset_row_is_spaces_with_one_offset_per_column() {
        let row = row(6);
        assert_eq!(row.text(), &[UNICODE_SPACE; 6]);
        assert_eq!(row.readable_column_count(), 6);
        assert!(!row.contains_text());
        for column in 0..=6 {
            assert_eq!(row.char_offset(i32::from(column)), column);
        }
    }

    #[test]
    fn wide_glyph_uses_trailer_column_and_shared_character_offset() {
        let mut row = row(6);
        row.replace_glyph(2, 2, &[0x4e00]).expect("wide glyph fits");

        assert_eq!(row.glyph_at(2), &[0x4e00]);
        assert_eq!(row.glyph_at(3), &[0x4e00]);
        assert_eq!(row.dbcs_attribute_at(2), DbcsAttribute::Leading);
        assert_eq!(row.dbcs_attribute_at(3), DbcsAttribute::Trailing);
        assert_eq!(row.char_offset(2), 2);
        assert_eq!(row.char_offset(3), 2);
        assert_eq!(row.char_offset(4), 3);
    }

    #[test]
    fn navigation_never_stops_on_wide_glyph_trailer() {
        let mut row = row(8);
        row.replace_glyph(2, 2, &[0x4e00]).expect("wide glyph fits");
        row.replace_glyph(4, 2, &[0x4e01]).expect("wide glyph fits");

        assert_eq!(row.adjust_to_glyph_start(3), 2);
        assert_eq!(row.adjust_to_glyph_end(3), 4);
        assert_eq!(row.navigate_to_next(2), 4);
        assert_eq!(row.navigate_to_previous(4), 2);
    }

    #[test]
    fn overwriting_half_of_wide_glyph_pads_the_other_half_with_space() {
        let mut row = row(6);
        row.replace_glyph(2, 2, &[0x4e00]).expect("wide glyph fits");
        let dirty = row
            .replace_glyph(2, 1, &[u16::from(b'A')])
            .expect("narrow fits");
        assert_eq!(dirty, DirtyRange { begin: 2, end: 4 });
        assert_eq!(row.glyph_at(2), &[u16::from(b'A')]);
        assert_eq!(row.glyph_at(3), &[UNICODE_SPACE]);

        row.replace_glyph(2, 2, &[0x4e00]).expect("wide glyph fits");
        let dirty = row
            .replace_glyph(3, 1, &[u16::from(b'B')])
            .expect("narrow fits");
        assert_eq!(dirty, DirtyRange { begin: 2, end: 4 });
        assert_eq!(row.glyph_at(2), &[UNICODE_SPACE]);
        assert_eq!(row.glyph_at(3), &[u16::from(b'B')]);
    }

    #[test]
    fn surrogate_pair_offset_maps_back_to_leading_and_trailing_columns() {
        let mut row = row(6);
        row.replace_glyph(2, 2, &[0xd83d, 0xde00])
            .expect("surrogate pair fits");

        assert_eq!(row.leading_column_at_char_offset(2), 2);
        assert_eq!(row.leading_column_at_char_offset(3), 2);
        assert_eq!(row.trailing_column_at_char_offset(3), 3);
        assert_eq!(row.leading_column_at_char_offset(4), 4);
    }

    #[test]
    fn double_width_and_padding_reduce_readable_columns_like_cpp_row() {
        let mut row = row(10);
        assert_eq!(row.readable_column_count(), 10);
        row.set_double_byte_padded(true);
        assert_eq!(row.readable_column_count(), 9);
        row.set_line_rendition(LineRendition::DoubleWidth);
        assert_eq!(row.readable_column_count(), 4);
    }

    #[test]
    fn attributes_are_replaced_over_half_open_column_ranges() {
        let mut row = row(5);
        let mut highlighted = TextAttribute::default();
        highlighted.set_intense(true);
        row.replace_attributes(1, 4, highlighted);
        assert!(!row.attribute_at(0).is_intense());
        assert!(row.attribute_at(1).is_intense());
        assert!(row.attribute_at(3).is_intense());
        assert!(!row.attribute_at(4).is_intense());
    }

    #[test]
    fn wrap_measurement_honors_double_byte_padding() {
        let mut row = row(5);
        row.replace_glyph(0, 1, &[u16::from(b'X')])
            .expect("glyph fits");
        assert_eq!(row.measure_right(), 1);
        row.set_wrap_forced(true);
        assert_eq!(row.measure_right(), 5);
        row.set_double_byte_padded(true);
        assert_eq!(row.measure_right(), 4);
    }

    #[test]
    fn wide_glyph_cannot_start_in_last_column() {
        let mut row = row(4);
        assert_eq!(
            row.replace_glyph(3, 2, &[0x4e00]),
            Err(RowError::GlyphDoesNotFit)
        );
    }
}
