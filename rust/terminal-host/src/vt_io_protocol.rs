//! Deterministic protocol/configuration decisions from host `VtIo`.
//!
//! Writing bytes to Windows handles, waiting for DA1, and mutating global
//! console services remain platform-owned boundaries. This module preserves the
//! pure choices that precede those operations.

/// Text measurement modes selected by `VtIo::Initialize`.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum TextMeasurementMode {
    Graphemes,
    Wcswidth,
    Console,
}

/// Maps the optional conhost text-measurement argument to the mode selected by
/// `VtIo::Initialize`.
///
/// Empty input leaves the existing mode untouched. Any non-empty value not
/// explicitly recognized by the C++ implementation falls back to graphemes.
#[must_use]
pub fn text_measurement_mode(value: &str) -> Option<TextMeasurementMode> {
    if value.is_empty() {
        None
    } else {
        Some(match value {
            "wcswidth" => TextMeasurementMode::Wcswidth,
            "console" => TextMeasurementMode::Console,
            _ => TextMeasurementMode::Graphemes,
        })
    }
}

/// Width override applied to ambiguous codepoints by `VtIo::Initialize`.
#[must_use]
pub const fn ambiguous_width_override(ambiguous_is_wide: bool) -> Option<u8> {
    if ambiguous_is_wide { Some(2) } else { None }
}

/// Produces the startup negotiation written by `VtIo::StartIfNeeded`.
///
/// When cursor inheritance is requested, the cursor-position report request is
/// emitted before DA1 so the host can use the later DA1 response as the wait
/// boundary for both requests.
#[must_use]
pub fn startup_negotiation(inherit_cursor: bool) -> Vec<u8> {
    let mut bytes = Vec::with_capacity(if inherit_cursor { 26 } else { 22 });
    if inherit_cursor {
        bytes.extend_from_slice(b"\x1b[6n");
    }
    bytes.extend_from_slice(b"\x1b[c\x1b[?1004h\x1b[?9001h");
    bytes
}

/// Reset sequences written by `VtIo::Shutdown` while the lifecycle is running.
#[must_use]
pub const fn shutdown_negotiation() -> &'static [u8] {
    b"\x1b[?1004l\x1b[?9001l"
}

/// Returns true for C0 controls and single-character C1 controls.
///
/// This is the semantic equivalent of the local `IsControlCharacter` helper in
/// `VtIo.cpp`; the C++ bitwise expression is an optimization of these ranges.
#[must_use]
pub const fn is_control_character(value: u16) -> bool {
    value <= 0x1f || (value >= 0x7f && value <= 0x9f)
}

/// Sanitizes one UTF-16 code unit using the legacy host `SanitizeUCS2` contract.
///
/// C0 controls and DEL use the historical code page 437 display glyphs, C1
/// controls become `?`, and isolated surrogate code units become U+FFFD.
#[must_use]
pub fn sanitize_ucs2(value: u16) -> u16 {
    const C0_GLYPHS: [u16; 32] = [
        0x0020, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 0x25d8, 0x25cb, 0x25d9,
        0x2642, 0x2640, 0x266a, 0x266b, 0x263c, 0x25ba, 0x25c4, 0x2195, 0x203c, 0x00b6, 0x00a7,
        0x25ac, 0x21a8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
    ];

    if value < 0x20 {
        C0_GLYPHS[usize::from(value)]
    } else if value == 0x7f {
        0x2302
    } else if (0x80..0xa0).contains(&value) {
        0x003f
    } else if (0xd800..=0xdfff).contains(&value) {
        0xfffd
    } else {
        value
    }
}

/// Applies the legacy host LF-to-CRLF translation to UTF-16 code units.
///
/// A CR is inserted only before an LF that is not already preceded by CR. Runs
/// of CR/LF are copied as a unit, matching `Writer::WriteUTF16TranslateCRLF`.
#[must_use]
pub fn translate_crlf_utf16(input: &[u16]) -> Vec<u16> {
    let mut output = Vec::with_capacity(input.len());
    let mut index = 0usize;

    while index < input.len() {
        let Some(relative_lf) = input[index..].iter().position(|value| *value == 0x000a) else {
            output.extend_from_slice(&input[index..]);
            break;
        };
        let lf = index + relative_lf;
        output.extend_from_slice(&input[index..lf]);

        if lf == 0 || input[lf - 1] != 0x000d {
            output.push(0x000d);
        }

        let run_start = lf;
        index = lf + 1;
        while index < input.len() && matches!(input[index], 0x000a | 0x000d) {
            index += 1;
        }
        output.extend_from_slice(&input[run_start..index]);
    }

    output
}

/// Replaces actionable C0/C1 controls with their printable host equivalents.
///
/// Ordinary UTF-16 code units are copied unchanged. This preserves one display
/// cell per raw control as required by the legacy console write contract.
#[must_use]
pub fn strip_control_chars_utf16(input: &[u16]) -> Vec<u16> {
    input
        .iter()
        .copied()
        .map(|value| {
            if is_control_character(value) {
                sanitize_ucs2(value)
            } else {
                value
            }
        })
        .collect()
}

/// Encodes one UCS-2 code unit to the UTF-8 bytes emitted by `VtIo::Writer`.
///
/// Surrogate code units are first replaced with U+FFFD. Because the input is a
/// single UCS-2 unit, the result is always one to three bytes.
#[must_use]
pub fn encode_ucs2_utf8(value: u16) -> Vec<u8> {
    let value = if (0xd800..=0xdfff).contains(&value) {
        0xfffd
    } else {
        value
    };

    let low_byte = |unit: u16| unit.to_le_bytes()[0];

    if value <= 0x7f {
        vec![low_byte(value)]
    } else if value <= 0x7ff {
        vec![0xc0 | low_byte(value >> 6), 0x80 | low_byte(value & 0x3f)]
    } else {
        vec![
            0xe0 | low_byte(value >> 12),
            0x80 | low_byte((value >> 6) & 0x3f),
            0x80 | low_byte(value & 0x3f),
        ]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn text_measurement_mapping_matches_vt_io() {
        assert_eq!(text_measurement_mode(""), None);
        assert_eq!(
            text_measurement_mode("wcswidth"),
            Some(TextMeasurementMode::Wcswidth)
        );
        assert_eq!(
            text_measurement_mode("console"),
            Some(TextMeasurementMode::Console)
        );
        assert_eq!(
            text_measurement_mode("graphemes"),
            Some(TextMeasurementMode::Graphemes)
        );
        assert_eq!(
            text_measurement_mode("future-value"),
            Some(TextMeasurementMode::Graphemes)
        );
    }

    #[test]
    fn ambiguous_width_override_only_applies_when_requested() {
        assert_eq!(ambiguous_width_override(false), None);
        assert_eq!(ambiguous_width_override(true), Some(2));
    }

    #[test]
    fn startup_negotiation_preserves_sequence_order() {
        assert_eq!(startup_negotiation(false), b"\x1b[c\x1b[?1004h\x1b[?9001h");
        assert_eq!(
            startup_negotiation(true),
            b"\x1b[6n\x1b[c\x1b[?1004h\x1b[?9001h"
        );
    }

    #[test]
    fn shutdown_negotiation_disables_focus_and_win32_input() {
        assert_eq!(shutdown_negotiation(), b"\x1b[?1004l\x1b[?9001l");
    }

    #[test]
    fn control_character_ranges_match_cpp_semantics() {
        assert!(is_control_character(0x00));
        assert!(is_control_character(0x1f));
        assert!(!is_control_character(0x20));
        assert!(!is_control_character(0x7e));
        assert!(is_control_character(0x7f));
        assert!(is_control_character(0x9f));
        assert!(!is_control_character(0xa0));
        assert!(!is_control_character(0xffff));
    }

    #[test]
    fn sanitize_ucs2_matches_legacy_display_contract() {
        assert_eq!(sanitize_ucs2(0x00), 0x0020);
        assert_eq!(sanitize_ucs2(0x01), 0x263a);
        assert_eq!(sanitize_ucs2(0x1f), 0x25bc);
        assert_eq!(sanitize_ucs2(0x20), 0x20);
        assert_eq!(sanitize_ucs2(0x7f), 0x2302);
        assert_eq!(sanitize_ucs2(0x80), 0x003f);
        assert_eq!(sanitize_ucs2(0x9f), 0x003f);
        assert_eq!(sanitize_ucs2(0xa0), 0xa0);
        assert_eq!(sanitize_ucs2(0xd800), 0xfffd);
        assert_eq!(sanitize_ucs2(0xdfff), 0xfffd);
        assert_eq!(sanitize_ucs2(0x2603), 0x2603);
    }

    #[test]
    fn crlf_translation_only_inserts_missing_carriage_returns() {
        assert_eq!(translate_crlf_utf16(&[]), Vec::<u16>::new());
        assert_eq!(
            translate_crlf_utf16(&[u16::from(b'a')]),
            vec![u16::from(b'a')]
        );
        assert_eq!(translate_crlf_utf16(&[0x000a]), vec![0x000d, 0x000a]);
        assert_eq!(
            translate_crlf_utf16(&[0x000d, 0x000a]),
            vec![0x000d, 0x000a]
        );
        assert_eq!(
            translate_crlf_utf16(&[
                u16::from(b'a'),
                0x000a,
                0x000a,
                0x000d,
                0x000a,
                u16::from(b'b'),
            ]),
            vec![
                u16::from(b'a'),
                0x000d,
                0x000a,
                0x000a,
                0x000d,
                0x000a,
                u16::from(b'b'),
            ]
        );
    }

    #[test]
    fn raw_control_stripping_preserves_cell_count() {
        let input = [u16::from(b'A'), 0x0001, 0x007f, 0x0080, u16::from(b'Z')];
        assert_eq!(
            strip_control_chars_utf16(&input),
            vec![u16::from(b'A'), 0x263a, 0x2302, 0x003f, u16::from(b'Z')]
        );
    }

    #[test]
    fn ucs2_encoding_matches_writer_contract() {
        assert_eq!(encode_ucs2_utf8(0x0041), vec![0x41]);
        assert_eq!(encode_ucs2_utf8(0x00a2), vec![0xc2, 0xa2]);
        assert_eq!(encode_ucs2_utf8(0x2603), vec![0xe2, 0x98, 0x83]);
        assert_eq!(encode_ucs2_utf8(0xd800), vec![0xef, 0xbf, 0xbd]);
    }
}
