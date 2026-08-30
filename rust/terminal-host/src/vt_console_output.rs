//! Stateful console-output mutation and VT projection for `ConPTY` hosts.
//!
//! The native API owns handles, locking and Win32 argument marshalling. This
//! module owns the deterministic part shared by `WriteConsoleOutput*` and
//! `FillConsoleOutput*`: mutate the same cell state that the legacy API exposes,
//! then project the affected rows through `VtIo::Writer::WriteInfos` while
//! preserving the caller cursor.

use crate::vt_char_info::{HostCharInfo, write_infos};
use crate::vt_writer_sequences::{restore_cursor, save_cursor};
use terminal_buffer::width_detector::TextMeasurementEngine;

const COMMON_LVB_LEADING_BYTE: u16 = 0x0100;
const COMMON_LVB_TRAILING_BYTE: u16 = 0x0200;
const WIDE_FLAGS: u16 = COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE;
const CLEAR_SCREEN: &[u8] = b"\x1b[H\x1b[2J\x1b[3J";

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct VtWriteResult {
    pub written: usize,
    pub bytes: Vec<u8>,
}

/// Small platform-neutral owner for the screen cells consumed by the legacy
/// console-output APIs while `ConPTY` is active.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct VtScreenOutputState {
    width: usize,
    height: usize,
    cells: Vec<HostCharInfo>,
}

impl VtScreenOutputState {
    #[must_use]
    pub fn new(width: usize, height: usize, default_attributes: u16) -> Self {
        Self {
            width,
            height,
            cells: vec![HostCharInfo::new(u16::from(b' '), default_attributes); width * height],
        }
    }

    #[must_use]
    pub const fn width(&self) -> usize {
        self.width
    }

    #[must_use]
    pub const fn height(&self) -> usize {
        self.height
    }

    #[must_use]
    pub fn cells(&self) -> &[HostCharInfo] {
        &self.cells
    }

    pub fn replace_cells(&mut self, cells: &[HostCharInfo]) -> bool {
        if cells.len() != self.cells.len() {
            return false;
        }
        self.cells.copy_from_slice(cells);
        true
    }

    /// Replays `WriteConsoleOutputAttributeImpl`: attributes replace the legacy
    /// color/rendition bits while existing wide-cell bookkeeping survives.
    #[must_use]
    pub fn write_attributes(&mut self, x: i32, y: i32, attributes: &[u16]) -> VtWriteResult {
        let Some(start) = self.linear_index(x, y) else {
            return VtWriteResult {
                written: 0,
                bytes: Vec::new(),
            };
        };
        let written = attributes.len().min(self.cells.len().saturating_sub(start));
        for (cell, attributes) in self.cells[start..start + written]
            .iter_mut()
            .zip(attributes.iter().copied())
        {
            let wide = cell.attributes & WIDE_FLAGS;
            cell.attributes = (attributes & !WIDE_FLAGS) | wide;
        }
        VtWriteResult {
            written,
            bytes: self.emit_linear_range(start, written),
        }
    }

    /// Replays `WriteConsoleOutputCharacterWImpl`: characters replace glyphs,
    /// preserve the attributes already stored at each destination cell and obey
    /// the legacy wide-glyph rule that a glyph intersecting the last column
    /// clears that column before continuing on the next row.
    #[must_use]
    pub fn write_characters(&mut self, x: i32, y: i32, text: &[u16]) -> VtWriteResult {
        let Some(start) = self.linear_index(x, y) else {
            return VtWriteResult {
                written: 0,
                bytes: Vec::new(),
            };
        };
        let (written, end) = self.write_glyphs(start, text.iter().copied());
        VtWriteResult {
            written,
            bytes: self.emit_linear_range(start, end.saturating_sub(start)),
        }
    }

    /// Replays `FillConsoleOutputAttributeImpl` for the `ConPTY` writer path.
    /// The PowerShell clear-buffer compatibility pair deliberately suppresses
    /// the attribute write because the companion character fill emits ED.
    #[must_use]
    pub fn fill_attributes(
        &mut self,
        x: i32,
        y: i32,
        attributes: u16,
        count: usize,
        suppress_for_clear_shim: bool,
    ) -> VtWriteResult {
        let Some(start) = self.linear_index(x, y) else {
            return VtWriteResult {
                written: 0,
                bytes: Vec::new(),
            };
        };
        let written = count.min(self.cells.len().saturating_sub(start));
        for cell in &mut self.cells[start..start + written] {
            let wide = cell.attributes & WIDE_FLAGS;
            cell.attributes = (attributes & !WIDE_FLAGS) | wide;
        }
        let bytes = if suppress_for_clear_shim && start == 0 && count >= self.cells.len() {
            Vec::new()
        } else {
            self.emit_linear_range(start, written)
        };
        VtWriteResult { written, bytes }
    }

    /// Replays `FillConsoleOutputCharacterWImpl`, including the PowerShell
    /// full-buffer clear optimization.
    #[must_use]
    pub fn fill_character(
        &mut self,
        x: i32,
        y: i32,
        code_unit: u16,
        count: usize,
        enable_clear_shim: bool,
    ) -> VtWriteResult {
        let Some(start) = self.linear_index(x, y) else {
            return VtWriteResult {
                written: 0,
                bytes: Vec::new(),
            };
        };

        if enable_clear_shim
            && start == 0
            && count >= self.cells.len()
            && code_unit == u16::from(b' ')
        {
            for cell in &mut self.cells {
                cell.code_unit = code_unit;
                cell.attributes &= !WIDE_FLAGS;
            }
            return VtWriteResult {
                written: self.cells.len(),
                bytes: CLEAR_SCREEN.to_vec(),
            };
        }

        let (written, end) = self.write_glyphs(start, core::iter::repeat_n(code_unit, count));
        VtWriteResult {
            written,
            bytes: self.emit_linear_range(start, end.saturating_sub(start)),
        }
    }

    fn linear_index(&self, x: i32, y: i32) -> Option<usize> {
        let x = usize::try_from(x).ok()?;
        let y = usize::try_from(y).ok()?;
        if x >= self.width || y >= self.height {
            return None;
        }
        Some(y * self.width + x)
    }

    fn write_glyphs(
        &mut self,
        start: usize,
        values: impl IntoIterator<Item = u16>,
    ) -> (usize, usize) {
        let detector = TextMeasurementEngine::default();
        let mut index = start;
        let mut written = 0usize;

        for code_unit in values {
            if index >= self.cells.len() {
                break;
            }

            let wide = char::from_u32(u32::from(code_unit))
                .is_some_and(|value| detector.scalar_width(value) == 2);

            if wide {
                if index % self.width == self.width.saturating_sub(1) {
                    self.cells[index].code_unit = u16::from(b' ');
                    self.cells[index].attributes &= !WIDE_FLAGS;
                    index += 1;
                    if index >= self.cells.len() {
                        break;
                    }
                }
                if index + 1 >= self.cells.len()
                    || index % self.width == self.width.saturating_sub(1)
                {
                    break;
                }

                let leading_attributes = self.cells[index].attributes & !WIDE_FLAGS;
                let trailing_attributes = self.cells[index + 1].attributes & !WIDE_FLAGS;
                self.cells[index] =
                    HostCharInfo::new(code_unit, leading_attributes | COMMON_LVB_LEADING_BYTE);
                self.cells[index + 1] =
                    HostCharInfo::new(code_unit, trailing_attributes | COMMON_LVB_TRAILING_BYTE);
                index += 2;
            } else {
                self.cells[index].code_unit = code_unit;
                self.cells[index].attributes &= !WIDE_FLAGS;
                index += 1;
            }
            written += 1;
        }

        (written, index)
    }

    fn emit_linear_range(&self, start: usize, count: usize) -> Vec<u8> {
        if count == 0 {
            return Vec::new();
        }

        let end = start.saturating_add(count).min(self.cells.len());
        let mut output = Vec::new();
        output.extend_from_slice(save_cursor());
        let mut cursor = start;
        while cursor < end {
            let row = cursor / self.width;
            let x = cursor % self.width;
            let row_end = ((row + 1) * self.width).min(end);
            output.extend_from_slice(&write_infos(
                i32::try_from(x).unwrap_or(i32::MAX),
                i32::try_from(row).unwrap_or(i32::MAX),
                &self.cells[cursor..row_end],
            ));
            cursor = row_end;
        }
        output.extend_from_slice(restore_cursor());
        output
    }
}

/// Serializes a `CHAR_INFO` run while preserving the caller's cursor, matching
/// the corked writer transaction exercised by Microsoft's `WriteConsoleOutputW` test.
#[must_use]
pub fn write_infos_preserving_cursor(
    target_x: i32,
    target_y: i32,
    infos: &[HostCharInfo],
) -> Vec<u8> {
    let body = write_infos(target_x, target_y, infos);
    let mut output = Vec::with_capacity(save_cursor().len() + body.len() + restore_cursor().len());
    output.extend_from_slice(save_cursor());
    output.extend_from_slice(&body);
    output.extend_from_slice(restore_cursor());
    output
}

#[cfg(test)]
mod tests {
    use super::*;

    const FOREGROUND_BLUE: u16 = 0x0001;
    const FOREGROUND_RED: u16 = 0x0004;
    const BACKGROUND_GREEN: u16 = 0x0020;
    const DEFAULT_ATTRS: u16 = 0x0007;
    const RED: u16 = FOREGROUND_RED | BACKGROUND_GREEN;
    const BLUE: u16 = FOREGROUND_BLUE | BACKGROUND_GREEN;

    fn ci(ch: char, attributes: u16) -> HostCharInfo {
        HostCharInfo::new(u16::try_from(u32::from(ch)).unwrap(), attributes)
    }

    fn ascii_screen() -> VtScreenOutputState {
        let rows = [
            [
                ('A', RED),
                ('B', RED),
                ('a', BLUE),
                ('b', BLUE),
                ('C', RED),
                ('D', RED),
                ('c', BLUE),
                ('d', BLUE),
            ],
            [
                ('E', RED),
                ('F', RED),
                ('e', BLUE),
                ('f', BLUE),
                ('G', RED),
                ('H', RED),
                ('g', BLUE),
                ('h', BLUE),
            ],
            [
                ('i', BLUE),
                ('j', BLUE),
                ('I', RED),
                ('J', RED),
                ('k', BLUE),
                ('l', BLUE),
                ('K', RED),
                ('L', RED),
            ],
            [
                ('m', BLUE),
                ('n', BLUE),
                ('M', RED),
                ('N', RED),
                ('o', BLUE),
                ('p', BLUE),
                ('O', RED),
                ('P', RED),
            ],
        ];
        let mut state = VtScreenOutputState::new(8, 4, DEFAULT_ATTRS);
        let cells = rows
            .into_iter()
            .flatten()
            .map(|(ch, attributes)| ci(ch, attributes))
            .collect::<Vec<_>>();
        assert!(state.replace_cells(&cells));
        state
    }

    fn wide_screen() -> VtScreenOutputState {
        let rows = [
            [('〇', RED), ('一', BLUE), ('二', RED), ('三', BLUE)],
            [('四', RED), ('五', BLUE), ('六', RED), ('七', BLUE)],
            [('八', BLUE), ('九', RED), ('十', BLUE), ('百', RED)],
            [('千', BLUE), ('万', RED), ('億', BLUE), ('兆', RED)],
        ];
        let mut cells = Vec::new();
        for (ch, attributes) in rows.into_iter().flatten() {
            let unit = u16::try_from(u32::from(ch)).unwrap();
            cells.push(HostCharInfo::new(
                unit,
                attributes | COMMON_LVB_LEADING_BYTE,
            ));
            cells.push(HostCharInfo::new(
                unit,
                attributes | COMMON_LVB_TRAILING_BYTE,
            ));
        }
        let mut state = VtScreenOutputState::new(8, 4, DEFAULT_ATTRS);
        assert!(state.replace_cells(&cells));
        state
    }

    #[test]
    fn microsoft_vt_io_write_console_output_w_matches_exact_vector() {
        let infos = [ci('a', RED), ci('b', RED), ci('A', BLUE), ci('B', BLUE)];
        assert_eq!(
            write_infos_preserving_cursor(1, 1, &infos),
            b"\x1b\x37\x1b[2;2H\x1b[0;31;42mab\x1b[0;34;42mAB\x1b\x38"
        );
    }

    #[test]
    fn microsoft_vt_io_write_console_output_attribute_matches_exact_vector() {
        let mut state = ascii_screen();
        let result = state.write_attributes(6, 1, &[RED, BLUE, RED, BLUE]);
        assert_eq!(result.written, 4);
        assert_eq!(
            result.bytes,
            b"\x1b\x37\x1b[2;7H\x1b[0;31;42mg\x1b[0;34;42mh\x1b[3;1H\x1b[0;31;42mi\x1b[0;34;42mj\x1b\x38"
        );
    }

    #[test]
    fn microsoft_vt_io_write_console_output_character_w_matches_all_source_vectors() {
        let mut state = ascii_screen();

        let result = state.write_characters(5, 1, &"foobar".encode_utf16().collect::<Vec<_>>());
        assert_eq!(result.written, 6);
        assert_eq!(
            result.bytes,
            b"\x1b\x37\x1b[2;6H\x1b[0;31;42mf\x1b[0;34;42moo\x1b[3;1H\x1b[0;34;42mba\x1b[0;31;42mr\x1b\x38"
        );

        let result = state.write_characters(5, 3, &"foobar".encode_utf16().collect::<Vec<_>>());
        assert_eq!(result.written, 3);
        assert_eq!(
            result.bytes,
            b"\x1b\x37\x1b[4;6H\x1b[0;34;42mf\x1b[0;31;42moo\x1b\x38"
        );

        let result = state.write_characters(5, 1, &"✨✅❌".encode_utf16().collect::<Vec<_>>());
        assert_eq!(result.written, 3);
        assert_eq!(
            result.bytes,
            "\u{1b}\u{37}\u{1b}[2;6H\u{1b}[0;31;42m✨\u{1b}[0;34;42m \u{1b}[3;1H\u{1b}[0;34;42m✅\u{1b}[0;31;42m❌\u{1b}\u{38}".as_bytes()
        );
    }

    #[test]
    fn microsoft_vt_io_fill_console_output_attribute_matches_exact_vectors() {
        let mut blank = VtScreenOutputState::new(8, 4, DEFAULT_ATTRS);
        let result = blank.fill_attributes(0, 0, RED, 0, false);
        assert_eq!(result.written, 0);
        assert!(result.bytes.is_empty());
        let result = blank.fill_attributes(0, 0, DEFAULT_ATTRS, 32, true);
        assert_eq!(result.written, 32);
        assert!(result.bytes.is_empty());

        let mut state = ascii_screen();
        assert_eq!(
            state.fill_attributes(0, 0, RED, 3, false).bytes,
            b"\x1b\x37\x1b[1;1H\x1b[0;31;42mABa\x1b\x38"
        );
        assert_eq!(
            state.fill_attributes(5, 0, RED, 3, false).bytes,
            b"\x1b\x37\x1b[1;6H\x1b[0;31;42mDcd\x1b\x38"
        );
        let result = state.fill_attributes(4, 1, BLUE, 8, false);
        assert_eq!(result.written, 8);
        assert_eq!(
            result.bytes,
            b"\x1b\x37\x1b[2;5H\x1b[0;34;42mGHgh\x1b[3;1H\x1b[0;34;42mijIJ\x1b\x38"
        );
    }

    #[test]
    fn microsoft_vt_io_fill_console_output_attribute_wide_matches_exact_vector() {
        let mut state = wide_screen();
        let result = state.fill_attributes(2, 1, RED, 4, false);
        assert_eq!(result.written, 4);
        assert_eq!(
            result.bytes,
            "\u{1b}\u{37}\u{1b}[2;3H\u{1b}[0;31;42m五六\u{1b}\u{38}".as_bytes()
        );
    }

    #[test]
    fn microsoft_vt_io_fill_console_output_character_w_matches_all_source_vectors() {
        let mut blank = VtScreenOutputState::new(8, 4, DEFAULT_ATTRS);
        assert!(
            blank
                .fill_character(0, 0, u16::from(b'a'), 0, false)
                .bytes
                .is_empty()
        );
        assert_eq!(
            blank.fill_character(0, 0, u16::from(b' '), 32, true).bytes,
            CLEAR_SCREEN
        );

        let mut state = ascii_screen();
        assert_eq!(
            state.fill_character(0, 0, u16::from(b'a'), 3, false).bytes,
            b"\x1b\x37\x1b[1;1H\x1b[0;31;42maa\x1b[0;34;42ma\x1b\x38"
        );
        assert_eq!(
            state.fill_character(5, 0, u16::from(b'b'), 3, false).bytes,
            b"\x1b\x37\x1b[1;6H\x1b[0;31;42mb\x1b[0;34;42mbb\x1b\x38"
        );
        assert_eq!(
            state.fill_character(4, 1, u16::from(b'c'), 8, false).bytes,
            b"\x1b\x37\x1b[2;5H\x1b[0;31;42mcc\x1b[0;34;42mcc\x1b[3;1H\x1b[0;34;42mcc\x1b[0;31;42mcc\x1b\x38"
        );
        assert_eq!(
            state.fill_character(5, 1, 0x2728, 3, false).bytes,
            "\u{1b}\u{37}\u{1b}[2;6H\u{1b}[0;31;42m✨\u{1b}[0;34;42m \u{1b}[3;1H\u{1b}[0;34;42m✨\u{1b}[0;31;42m✨\u{1b}\u{38}".as_bytes()
        );
    }
}
