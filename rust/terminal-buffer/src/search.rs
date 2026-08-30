//! Unicode text search over terminal-buffer rows with cell-coordinate results.
//!
//! Windows Terminal stores row text as UTF-16 while exposing search results in
//! terminal cell coordinates. A Unicode scalar may therefore consume two UTF-16
//! code units but one cell, while an East Asian wide glyph consumes one scalar
//! and two cells. This module searches the stored UTF-16 representation and maps
//! each match back through the row's validated character-to-cell offsets.

use crate::text_buffer::{TextBuffer, TextBufferPoint};

/// An end-exclusive match span in logical terminal-buffer coordinates.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TextSearchSpan {
    pub start: TextBufferPoint,
    pub end: TextBufferPoint,
}

impl TextSearchSpan {
    #[must_use]
    pub const fn new(start: TextBufferPoint, end: TextBufferPoint) -> Self {
        Self { start, end }
    }
}

/// Portable flags for the host-level search contract.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct SearchTextOptions {
    pub case_insensitive: bool,
    pub regular_expression: bool,
}

/// Errors from the portable regular-expression subset exercised by the native
/// host search contracts.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SearchPatternError {
    UnsupportedSyntax,
    UnterminatedClass,
    InvalidEscape,
    InvalidRange,
    InvalidRepeat,
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum ClassItem {
    Single(char),
    Range(char, char),
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum PatternAtom {
    Literal(char),
    Class(Vec<ClassItem>),
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct CompiledPattern {
    atoms: Vec<PatternAtom>,
    case_insensitive: bool,
}

impl TextBuffer {
    /// Finds exact UTF-16 substring matches and returns terminal-cell spans.
    ///
    /// Matches are row-local. Host-level direction/cursor orchestration remains
    /// a separate integration responsibility.
    #[must_use]
    pub fn search_text(&self, needle: &[u16]) -> Vec<TextSearchSpan> {
        if needle.is_empty() {
            return Vec::new();
        }

        let mut matches = Vec::new();
        for y in 0..self.height() {
            let row = self.row(i32::from(y));
            let haystack = row.text();
            if needle.len() > haystack.len() {
                continue;
            }

            for char_begin in 0..=haystack.len() - needle.len() {
                let char_end = char_begin + needle.len();
                if haystack[char_begin..char_end] != *needle {
                    continue;
                }

                let start_x = row.leading_column_at_char_offset(
                    isize::try_from(char_begin).unwrap_or(isize::MAX),
                );
                let end_x = row
                    .leading_column_at_char_offset(isize::try_from(char_end).unwrap_or(isize::MAX));
                matches.push(TextSearchSpan::new(
                    TextBufferPoint::new(start_x, y),
                    TextBufferPoint::new(end_x, y),
                ));
            }
        }
        matches
    }

    /// Searches row text with the case-folding and regular-expression options
    /// exercised by Microsoft's `SearchTests.cpp` family.
    ///
    /// The regex parser deliberately owns the deterministic subset required by
    /// the console contract: literals, bracket classes/ranges, `\\x{...}` scalar
    /// escapes, exact `{n}` repetition, and a leading `(?i)` modifier. Unsupported
    /// syntax is rejected instead of being silently reinterpreted.
    pub fn search_text_with_options(
        &self,
        needle: &str,
        options: SearchTextOptions,
    ) -> Result<Vec<TextSearchSpan>, SearchPatternError> {
        if needle.is_empty() {
            return Ok(Vec::new());
        }

        let compiled = compile_pattern(needle, options)?;
        if compiled.atoms.is_empty() {
            return Ok(Vec::new());
        }

        let mut matches = Vec::new();
        for y in 0..self.height() {
            let row = self.row(i32::from(y));
            let decoded = std::char::decode_utf16(row.text().iter().copied())
                .map(|scalar| scalar.unwrap_or(char::REPLACEMENT_CHARACTER))
                .collect::<Vec<_>>();
            if compiled.atoms.len() > decoded.len() {
                continue;
            }

            let mut utf16_offsets = Vec::with_capacity(decoded.len() + 1);
            utf16_offsets.push(0_usize);
            let mut offset = 0_usize;
            for scalar in &decoded {
                offset = offset.saturating_add(scalar.len_utf16());
                utf16_offsets.push(offset);
            }

            for scalar_begin in 0..=decoded.len() - compiled.atoms.len() {
                let scalar_end = scalar_begin + compiled.atoms.len();
                if !compiled.matches(&decoded[scalar_begin..scalar_end]) {
                    continue;
                }

                let char_begin = utf16_offsets[scalar_begin];
                let char_end = utf16_offsets[scalar_end];
                let start_x = row.leading_column_at_char_offset(
                    isize::try_from(char_begin).unwrap_or(isize::MAX),
                );
                let end_x = row
                    .leading_column_at_char_offset(isize::try_from(char_end).unwrap_or(isize::MAX));
                matches.push(TextSearchSpan::new(
                    TextBufferPoint::new(start_x, y),
                    TextBufferPoint::new(end_x, y),
                ));
            }
        }

        Ok(matches)
    }
}

impl CompiledPattern {
    fn matches(&self, candidate: &[char]) -> bool {
        self.atoms
            .iter()
            .zip(candidate)
            .all(|(atom, scalar)| atom.matches(*scalar, self.case_insensitive))
    }
}

impl PatternAtom {
    fn matches(&self, scalar: char, case_insensitive: bool) -> bool {
        match self {
            Self::Literal(expected) => char_equals(*expected, scalar, case_insensitive),
            Self::Class(items) => items.iter().any(|item| match item {
                ClassItem::Single(expected) => char_equals(*expected, scalar, case_insensitive),
                ClassItem::Range(begin, end) => {
                    if (*begin..=*end).contains(&scalar) {
                        true
                    } else if case_insensitive {
                        let folded_scalar = simple_lower(scalar);
                        let folded_begin = simple_lower(*begin);
                        let folded_end = simple_lower(*end);
                        match (folded_scalar, folded_begin, folded_end) {
                            (Some(value), Some(begin), Some(end)) => (begin..=end).contains(&value),
                            _ => false,
                        }
                    } else {
                        false
                    }
                }
            }),
        }
    }
}

fn char_equals(left: char, right: char, case_insensitive: bool) -> bool {
    left == right
        || (case_insensitive
            && (left.to_lowercase().eq(right.to_lowercase())
                || left.to_uppercase().eq(right.to_uppercase())))
}

fn simple_lower(value: char) -> Option<char> {
    let mut folded = value.to_lowercase();
    let first = folded.next()?;
    folded.next().is_none().then_some(first)
}

fn compile_pattern(
    needle: &str,
    mut options: SearchTextOptions,
) -> Result<CompiledPattern, SearchPatternError> {
    let mut pattern = needle;
    if options.regular_expression && pattern.starts_with("(?i)") {
        options.case_insensitive = true;
        pattern = &pattern[4..];
    }

    if !options.regular_expression {
        return Ok(CompiledPattern {
            atoms: pattern.chars().map(PatternAtom::Literal).collect(),
            case_insensitive: options.case_insensitive,
        });
    }

    let chars = pattern.chars().collect::<Vec<_>>();
    let mut atoms = Vec::new();
    let mut index = 0_usize;
    while index < chars.len() {
        let atom = match chars[index] {
            '[' => parse_class(&chars, &mut index)?,
            '\\' => PatternAtom::Literal(parse_escape(&chars, &mut index)?),
            '(' | ')' | '.' | '*' | '+' | '?' | '|' | '^' | '$' => {
                return Err(SearchPatternError::UnsupportedSyntax);
            }
            scalar => {
                index += 1;
                PatternAtom::Literal(scalar)
            }
        };

        let repeat = if index < chars.len() && chars[index] == '{' {
            parse_repeat(&chars, &mut index)?
        } else {
            1
        };
        if repeat > 1024 {
            return Err(SearchPatternError::InvalidRepeat);
        }
        atoms.extend(std::iter::repeat_n(atom, repeat));
    }

    Ok(CompiledPattern {
        atoms,
        case_insensitive: options.case_insensitive,
    })
}

fn parse_class(chars: &[char], index: &mut usize) -> Result<PatternAtom, SearchPatternError> {
    *index += 1;
    let mut items = Vec::new();
    while *index < chars.len() && chars[*index] != ']' {
        let begin = parse_class_scalar(chars, index)?;
        if *index < chars.len()
            && chars[*index] == '-'
            && chars.get(*index + 1).is_some_and(|next| *next != ']')
        {
            *index += 1;
            let end = parse_class_scalar(chars, index)?;
            if begin > end {
                return Err(SearchPatternError::InvalidRange);
            }
            items.push(ClassItem::Range(begin, end));
        } else {
            items.push(ClassItem::Single(begin));
        }
    }

    if chars.get(*index) != Some(&']') || items.is_empty() {
        return Err(SearchPatternError::UnterminatedClass);
    }
    *index += 1;
    Ok(PatternAtom::Class(items))
}

fn parse_class_scalar(chars: &[char], index: &mut usize) -> Result<char, SearchPatternError> {
    match chars.get(*index).copied() {
        Some('\\') => parse_escape(chars, index),
        Some(']') | None => Err(SearchPatternError::UnterminatedClass),
        Some(value) => {
            *index += 1;
            Ok(value)
        }
    }
}

fn parse_escape(chars: &[char], index: &mut usize) -> Result<char, SearchPatternError> {
    if chars.get(*index) != Some(&'\\') {
        return Err(SearchPatternError::InvalidEscape);
    }
    *index += 1;

    if chars.get(*index) == Some(&'x') && chars.get(*index + 1) == Some(&'{') {
        *index += 2;
        let begin = *index;
        while *index < chars.len() && chars[*index] != '}' {
            *index += 1;
        }
        if chars.get(*index) != Some(&'}') || begin == *index {
            return Err(SearchPatternError::InvalidEscape);
        }
        let digits = chars[begin..*index].iter().collect::<String>();
        *index += 1;
        let scalar = u32::from_str_radix(&digits, 16)
            .ok()
            .and_then(char::from_u32)
            .ok_or(SearchPatternError::InvalidEscape)?;
        Ok(scalar)
    } else {
        let value = chars
            .get(*index)
            .copied()
            .ok_or(SearchPatternError::InvalidEscape)?;
        *index += 1;
        Ok(value)
    }
}

fn parse_repeat(chars: &[char], index: &mut usize) -> Result<usize, SearchPatternError> {
    *index += 1;
    let begin = *index;
    while *index < chars.len() && chars[*index].is_ascii_digit() {
        *index += 1;
    }
    if begin == *index || chars.get(*index) != Some(&'}') {
        return Err(SearchPatternError::InvalidRepeat);
    }
    let digits = chars[begin..*index].iter().collect::<String>();
    *index += 1;
    digits
        .parse::<usize>()
        .map_err(|_| SearchPatternError::InvalidRepeat)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::row_writer::replace_text;
    use crate::text_attribute::TextAttribute;

    fn utf16(text: &str) -> Vec<u16> {
        text.encode_utf16().collect()
    }

    fn span(begin: u16, end: u16) -> TextSearchSpan {
        TextSearchSpan::new(TextBufferPoint::new(begin, 0), TextBufferPoint::new(end, 0))
    }

    #[test]
    fn microsoft_utext_adapter_unicode_matches_source_contract() {
        let mut buffer = TextBuffer::new(24, 1, TextAttribute::default()).expect("valid buffer");
        replace_text(buffer.row_mut(0), 0, &utf16("abc 𝒶𝒷𝒸 abc ネコちゃん"))
            .expect("Microsoft source text fits the 24-cell row");

        assert_eq!(
            buffer.search_text(&utf16("abc")),
            vec![span(0, 3), span(8, 11)]
        );
        assert_eq!(buffer.search_text(&utf16("𝒷")), vec![span(5, 6)]);
        assert_eq!(buffer.search_text(&utf16("ネコ")), vec![span(12, 16)]);
    }

    #[test]
    fn empty_needle_does_not_create_synthetic_matches() {
        let buffer = TextBuffer::new(4, 1, TextAttribute::default()).expect("valid buffer");
        assert!(buffer.search_text(&[]).is_empty());
    }

    #[test]
    fn microsoft_host_search_regex_subset_maps_wide_cells() {
        let mut buffer = TextBuffer::new(8, 1, TextAttribute::default()).expect("valid buffer");
        replace_text(buffer.row_mut(0), 0, &utf16("ABか")).expect("fixture fits");

        assert_eq!(
            buffer
                .search_text_with_options(
                    r"[\x{3041}-\x{304c}]",
                    SearchTextOptions {
                        case_insensitive: false,
                        regular_expression: true,
                    },
                )
                .expect("supported source regex"),
            vec![span(2, 4)]
        );
    }
}
