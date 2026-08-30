//! Portable SGR application over `TextAttribute`.
//!
//! The parser owns CSI tokenization; this module owns the deterministic product
//! semantics of applying Select Graphic Rendition parameters to the active text
//! attribute. Keeping that state transition in `terminal-buffer` lets Host and
//! Terminal share the same color/rendition behavior without a parser dependency.

use crate::text_attribute::{TextAttribute, UnderlineStyle};
use crate::text_color::TextColor;

/// Applies one flattened SGR parameter list to `attribute`.
///
/// An omitted/empty parameter is represented as zero by the caller. Extended
/// colors support the semicolon form `38;2;r;g;b`, `48;2;r;g;b`, and
/// `58;2;r;g;b`; unsupported parameters are ignored, matching terminal SGR's
/// forward-compatible behavior.
pub fn apply_sgr(attribute: &mut TextAttribute, params: &[u16]) {
    if params.is_empty() {
        *attribute = TextAttribute::default();
        return;
    }

    let mut index = 0;
    while index < params.len() {
        let parameter = params[index];
        match parameter {
            0 => *attribute = TextAttribute::default(),
            1 => attribute.set_intense(true),
            2 => attribute.set_faint(true),
            4 => attribute.set_underline_style(UnderlineStyle::Single),
            7 => attribute.set_reverse_video(true),
            22 => {
                attribute.set_intense(false);
                attribute.set_faint(false);
            }
            24 => attribute.set_underline_style(UnderlineStyle::None),
            27 => attribute.set_reverse_video(false),
            30..=37 => attribute.set_foreground(TextColor::index16((parameter - 30) as u8)),
            38 => {
                if let Some((color, consumed)) = extended_rgb(params, index) {
                    attribute.set_foreground(color);
                    index += consumed;
                }
            }
            39 => attribute.set_default_foreground(),
            40..=47 => attribute.set_background(TextColor::index16((parameter - 40) as u8)),
            48 => {
                if let Some((color, consumed)) = extended_rgb(params, index) {
                    attribute.set_background(color);
                    index += consumed;
                }
            }
            49 => attribute.set_default_background(),
            58 => {
                if let Some((color, consumed)) = extended_rgb(params, index) {
                    attribute.set_underline_color(color);
                    index += consumed;
                }
            }
            59 => attribute.set_default_underline_color(),
            90..=97 => {
                attribute.set_foreground(TextColor::index16((parameter - 90 + 8) as u8));
            }
            100..=107 => {
                attribute.set_background(TextColor::index16((parameter - 100 + 8) as u8));
            }
            _ => {}
        }
        index += 1;
    }
}

fn extended_rgb(params: &[u16], index: usize) -> Option<(TextColor, usize)> {
    if params.get(index + 1) != Some(&2) {
        return None;
    }
    let r = u8::try_from(*params.get(index + 2)?).ok()?;
    let g = u8::try_from(*params.get(index + 3)?).ok()?;
    let b = u8::try_from(*params.get(index + 4)?).ok()?;
    Some((TextColor::rgb(r, g, b), 4))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::text_color::{ColorType, Rgb};

    #[test]
    fn reset_and_empty_sgr_restore_defaults() {
        let mut attribute = TextAttribute::from_rgb(Rgb::new(1, 2, 3), Rgb::new(4, 5, 6));
        attribute.set_intense(true);
        apply_sgr(&mut attribute, &[]);
        assert_eq!(attribute, TextAttribute::default());

        apply_sgr(&mut attribute, &[31, 0]);
        assert_eq!(attribute, TextAttribute::default());
    }

    #[test]
    fn rgb_and_legacy_channels_remain_independent() {
        let mut attribute = TextAttribute::default();
        apply_sgr(&mut attribute, &[38, 2, 64, 128, 255]);
        apply_sgr(&mut attribute, &[49]);
        assert_eq!(attribute.foreground().rgb_value(), Rgb::new(64, 128, 255));
        assert!(attribute.background().is_default());

        apply_sgr(&mut attribute, &[48, 2, 64, 128, 255]);
        apply_sgr(&mut attribute, &[39]);
        assert!(attribute.foreground().is_default());
        assert_eq!(attribute.background().rgb_value(), Rgb::new(64, 128, 255));
    }

    #[test]
    fn underline_and_reverse_are_orthogonal_to_rgb_colors() {
        let mut attribute = TextAttribute::default();
        apply_sgr(&mut attribute, &[48, 2, 64, 128, 255, 4]);
        assert!(attribute.is_underlined());
        assert_eq!(attribute.background().rgb_value(), Rgb::new(64, 128, 255));

        apply_sgr(&mut attribute, &[42, 38, 2, 128, 5, 255, 7]);
        assert!(attribute.is_reverse_video());
        assert_eq!(attribute.foreground().rgb_value(), Rgb::new(128, 5, 255));
        assert_eq!(attribute.background().color_type(), ColorType::Index16);
        assert_eq!(attribute.background().index(), TextColor::DARK_GREEN);
        apply_sgr(&mut attribute, &[27]);
        assert!(!attribute.is_reverse_video());
    }

    #[test]
    fn intensity_only_brightens_legacy_foreground_semantics() {
        let mut legacy = TextAttribute::default();
        apply_sgr(&mut legacy, &[32]);
        assert_eq!(legacy.foreground().index(), TextColor::DARK_GREEN);
        assert!(!legacy.is_intense());
        apply_sgr(&mut legacy, &[1]);
        assert!(legacy.is_intense());
        assert!(legacy.foreground().can_be_brightened());

        let mut rgb = TextAttribute::default();
        apply_sgr(&mut rgb, &[38, 2, 40, 40, 40, 48, 2, 168, 153, 132]);
        let foreground = rgb.foreground();
        let background = rgb.background();
        apply_sgr(&mut rgb, &[1]);
        assert_eq!(rgb.foreground(), foreground);
        assert_eq!(rgb.background(), background);
        assert!(rgb.is_intense());
    }

    #[test]
    fn sgr_22_clears_intensity_without_retyping_colors() {
        let mut attribute = TextAttribute::default();
        apply_sgr(&mut attribute, &[1, 32, 48, 2, 1, 2, 3]);
        assert!(attribute.is_intense());
        assert_eq!(attribute.foreground().index(), TextColor::DARK_GREEN);
        assert_eq!(attribute.background().rgb_value(), Rgb::new(1, 2, 3));

        apply_sgr(&mut attribute, &[22]);
        assert!(!attribute.is_intense());
        assert_eq!(attribute.foreground().index(), TextColor::DARK_GREEN);
        assert_eq!(attribute.background().rgb_value(), Rgb::new(1, 2, 3));
    }

    #[test]
    fn complex_unintense_preserves_rgb_foreground_values() {
        let mut attribute = TextAttribute::default();
        apply_sgr(&mut attribute, &[1, 32, 48, 2, 1, 2, 3]);
        apply_sgr(&mut attribute, &[22]);
        apply_sgr(&mut attribute, &[38, 2, 32, 32, 32]);
        apply_sgr(&mut attribute, &[1]);
        assert!(attribute.is_intense());
        assert_eq!(attribute.foreground().rgb_value(), Rgb::new(32, 32, 32));
        apply_sgr(&mut attribute, &[38, 2, 64, 64, 64]);
        apply_sgr(&mut attribute, &[22]);
        assert!(!attribute.is_intense());
        assert_eq!(attribute.foreground().rgb_value(), Rgb::new(64, 64, 64));
        assert_eq!(attribute.background().rgb_value(), Rgb::new(1, 2, 3));
    }

    #[test]
    fn reset_clears_intensity_before_following_legacy_color() {
        let mut attribute = TextAttribute::default();
        apply_sgr(&mut attribute, &[32, 1]);
        assert!(attribute.is_intense());
        apply_sgr(&mut attribute, &[0]);
        assert!(!attribute.is_intense());
        apply_sgr(&mut attribute, &[32]);
        assert!(!attribute.is_intense());
        assert_eq!(attribute.foreground().index(), TextColor::DARK_GREEN);
    }

    #[test]
    fn standard_erase_keeps_active_rgb_colors() {
        let mut attribute = TextAttribute::default();
        apply_sgr(&mut attribute, &[48, 2, 128, 128, 255]);
        let background = attribute.background();
        attribute.set_standard_erase();
        assert_eq!(attribute.background(), background);
        assert_eq!(attribute.background().rgb_value(), Rgb::new(128, 128, 255));
    }
}
