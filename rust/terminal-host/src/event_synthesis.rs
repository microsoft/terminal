//! Pure legacy character-width classification from `EventSynthesis.cpp`.
//!
//! Keyboard-layout probing, virtual-key mapping, code-page conversion, and
//! actual input-event construction remain Win32-owned boundaries. This module
//! preserves only the deterministic UCS-2 width heuristic used to decide when
//! the legacy numpad fallback is eligible.

/// Returns whether the UCS-2 code unit falls in one of the historical
/// full-width ranges used by `EventSynthesis.cpp`.
#[must_use]
pub const fn is_legacy_full_width(code_unit: u16) -> bool {
    matches!(
        code_unit,
        0x1100..=0x115f
            | 0x2e80..=0x303e
            | 0x3041..=0x3094
            | 0x30a1..=0x30f6
            | 0x3105..=0x312c
            | 0x3131..=0x318e
            | 0x3190..=0x3247
            | 0x3251..=0x4dbf
            | 0x4e00..=0xa4c6
            | 0xa960..=0xa97c
            | 0xac00..=0xd7a3
            | 0xf900..=0xfaff
            | 0xfe10..=0xfe1f
            | 0xfe30..=0xfe6b
            | 0xff01..=0xff5e
            | 0xffe0..=0xffe6
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_cpp_range_accepts_its_boundaries() {
        for (start, end) in [
            (0x1100, 0x115f),
            (0x2e80, 0x303e),
            (0x3041, 0x3094),
            (0x30a1, 0x30f6),
            (0x3105, 0x312c),
            (0x3131, 0x318e),
            (0x3190, 0x3247),
            (0x3251, 0x4dbf),
            (0x4e00, 0xa4c6),
            (0xa960, 0xa97c),
            (0xac00, 0xd7a3),
            (0xf900, 0xfaff),
            (0xfe10, 0xfe1f),
            (0xfe30, 0xfe6b),
            (0xff01, 0xff5e),
            (0xffe0, 0xffe6),
        ] {
            assert!(is_legacy_full_width(start));
            assert!(is_legacy_full_width(end));
        }
    }

    #[test]
    fn deliberate_gaps_remain_narrow() {
        for value in [
            0x115f + 1,
            0x303e + 1,
            0x3094 + 1,
            0x30f6 + 1,
            0x312c + 1,
            0x318e + 1,
            0x3247 + 1,
            0x4dbf + 1,
            0xa4c6 + 1,
            0xa97c + 1,
            0xd7a3 + 1,
            0xfaff + 1,
            0xfe1f + 1,
            0xfe6b + 1,
            0xff5e + 1,
            0xffe6 + 1,
        ] {
            assert!(!is_legacy_full_width(value));
        }
    }

    #[test]
    fn representative_ascii_and_surrogates_are_not_full_width() {
        assert!(!is_legacy_full_width(u16::from(b'A')));
        assert!(!is_legacy_full_width(0xd800));
        assert!(!is_legacy_full_width(0xdfff));
    }

    #[test]
    fn representative_cjk_hiragana_and_fullwidth_ascii_are_wide() {
        assert!(is_legacy_full_width(0x4e2d));
        assert!(is_legacy_full_width(0x3042));
        assert!(is_legacy_full_width(0xff21));
    }
}
