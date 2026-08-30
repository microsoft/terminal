//! Deterministic `CHAR_INFO` serialization from `VtIo::Writer::WriteInfos`.
//!
//! The Windows API remains responsible for producing `CHAR_INFO` records. This
//! module only ports the pure transformation from those wire-like values to VT
//! bytes.

use crate::attribute_format::format_attributes;
use crate::vt_io_protocol::{encode_ucs2_utf8, is_control_character, sanitize_ucs2};
use crate::vt_writer_sequences::cup;
use terminal_buffer::text_attribute::{LegacyColorDefaults, TextAttribute};

const COMMON_LVB_LEADING_BYTE: u16 = 0x0100;
const COMMON_LVB_TRAILING_BYTE: u16 = 0x0200;
const WIDE_FLAGS: u16 = COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE;

/// Platform-neutral representation of the fields consumed by `WriteInfos`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct HostCharInfo {
    pub code_unit: u16,
    pub attributes: u16,
}

impl HostCharInfo {
    #[must_use]
    pub const fn new(code_unit: u16, attributes: u16) -> Self {
        Self {
            code_unit,
            attributes,
        }
    }
}

/// Serializes one host `CHAR_INFO` run using the same edge handling and
/// attribute transitions as `VtIo::Writer::WriteInfos`.
#[must_use]
pub fn write_infos(target_x: i32, target_y: i32, infos: &[HostCharInfo]) -> Vec<u8> {
    let mut output = cup(target_x, target_y);
    if infos.is_empty() {
        return output;
    }

    let mut previous_attributes: Option<u16> = None;

    for (index, info) in infos.iter().copied().enumerate() {
        let first = index == 0;
        let last = index + 1 == infos.len();
        let mut code_unit = info.code_unit;
        let mut wide = info.attributes & WIDE_FLAGS != 0;

        if wide {
            if info.attributes & COMMON_LVB_LEADING_BYTE != 0 {
                if last {
                    code_unit = u16::from(b' ');
                    wide = false;
                }
            } else if first {
                code_unit = u16::from(b' ');
                wide = false;
            } else {
                // Interior trailing halves are represented by their leading half.
                continue;
            }
        }

        if previous_attributes != Some(info.attributes) {
            previous_attributes = Some(info.attributes);
            let attributes =
                TextAttribute::from_legacy(info.attributes, LegacyColorDefaults::default());
            output.extend_from_slice(format_attributes(attributes).as_bytes());
        }

        let repeat = if wide
            && ((0xd800..=0xdfff).contains(&code_unit) || is_control_character(code_unit))
        {
            2
        } else {
            1
        };
        let sanitized = sanitize_ucs2(code_unit);
        let encoded = encode_ucs2_utf8(sanitized);
        for _ in 0..repeat {
            output.extend_from_slice(&encoded);
        }
    }

    output
}

#[cfg(test)]
mod tests {
    use super::*;

    const DEFAULT_ATTRS: u16 = 0x0007;

    #[test]
    fn starts_with_cup_and_emits_attributes_only_on_change() {
        let infos = [
            HostCharInfo::new(u16::from(b'A'), DEFAULT_ATTRS),
            HostCharInfo::new(u16::from(b'B'), DEFAULT_ATTRS),
            HostCharInfo::new(u16::from(b'C'), 0x0004),
        ];

        assert_eq!(write_infos(4, 2, &infos), b"\x1b[3;5H\x1b[0mAB\x1b[0;31mC");
    }

    #[test]
    fn incomplete_wide_halves_at_run_edges_become_spaces() {
        let leading = [HostCharInfo::new(
            0x4e2d,
            DEFAULT_ATTRS | COMMON_LVB_LEADING_BYTE,
        )];
        let trailing = [HostCharInfo::new(
            0x4e2d,
            DEFAULT_ATTRS | COMMON_LVB_TRAILING_BYTE,
        )];

        assert_eq!(write_infos(0, 0, &leading), b"\x1b[1;1H\x1b[0m ");
        assert_eq!(write_infos(0, 0, &trailing), b"\x1b[1;1H\x1b[0m ");
    }

    #[test]
    fn interior_trailing_half_is_skipped() {
        let infos = [
            HostCharInfo::new(0x4e2d, DEFAULT_ATTRS | COMMON_LVB_LEADING_BYTE),
            HostCharInfo::new(0x4e2d, DEFAULT_ATTRS | COMMON_LVB_TRAILING_BYTE),
            HostCharInfo::new(u16::from(b'!'), DEFAULT_ATTRS),
        ];

        assert_eq!(
            write_infos(0, 0, &infos),
            b"\x1b[1;1H\x1b[0m\xe4\xb8\xad\x1b[0m!"
        );
    }

    #[test]
    fn wide_control_replacement_is_repeated_to_preserve_width() {
        let infos = [
            HostCharInfo::new(0x0001, DEFAULT_ATTRS | COMMON_LVB_LEADING_BYTE),
            HostCharInfo::new(0x0001, DEFAULT_ATTRS | COMMON_LVB_TRAILING_BYTE),
        ];

        assert_eq!(
            write_infos(0, 0, &infos),
            b"\x1b[1;1H\x1b[0m\xe2\x98\xba\xe2\x98\xba"
        );
    }

    #[test]
    fn empty_run_still_positions_cursor_like_cpp_writer() {
        assert_eq!(write_infos(9, 4, &[]), b"\x1b[5;10H");
    }
}
