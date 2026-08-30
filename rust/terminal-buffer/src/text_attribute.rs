//! Safe Rust representation of Windows Terminal text attributes.

use crate::text_color::{Rgb, TextColor};

const FG_ATTRS: u16 = 0x000f;
const BG_ATTRS: u16 = 0x00f0;
const USED_META_ATTRS: u16 = 0xdc00;
const FOREGROUND_INTENSITY: u16 = 0x0008;
const UNDERLINE_STYLE_MASK: u16 = 0x01c0;
const UNDERLINE_STYLE_SHIFT: u32 = 6;

const ATTR_INTENSE: u16 = 0x0001;
const ATTR_ITALICS: u16 = 0x0002;
const ATTR_BLINKING: u16 = 0x0004;
const ATTR_INVISIBLE: u16 = 0x0008;
const ATTR_CROSSED_OUT: u16 = 0x0010;
const ATTR_FAINT: u16 = 0x0020;
const ATTR_TOP_GRIDLINE: u16 = 0x0400;
const ATTR_LEFT_GRIDLINE: u16 = 0x0800;
const ATTR_RIGHT_GRIDLINE: u16 = 0x1000;
const ATTR_PROTECTED: u16 = 0x2000;
const ATTR_REVERSE_VIDEO: u16 = 0x4000;
const ATTR_BOTTOM_GRIDLINE: u16 = 0x8000;
const ATTR_RENDITION: u16 = 0xdfff;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
#[repr(u16)]
pub enum UnderlineStyle {
    #[default]
    None = 0,
    Single = 1,
    Double = 2,
    Curly = 3,
    Dotted = 4,
    Dashed = 5,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
#[repr(u16)]
pub enum MarkKind {
    #[default]
    None = 0,
    Prompt = 1,
    Command = 2,
    Output = 3,
}

/// Explicit replacement for the mutable process-global legacy color mapping in
/// the C++ implementation. Keeping the mapping as a value preserves conversion
/// semantics without introducing shared mutable state in product Rust.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct LegacyColorDefaults {
    foreground: u8,
    background: u8,
}

impl Default for LegacyColorDefaults {
    fn default() -> Self {
        Self {
            foreground: 7,
            background: 0,
        }
    }
}

impl LegacyColorDefaults {
    #[must_use]
    pub fn from_legacy_attribute(attribute: u16) -> Self {
        Self {
            foreground: u8::try_from(attribute & FG_ATTRS).unwrap_or_default(),
            background: u8::try_from((attribute & BG_ATTRS) >> 4).unwrap_or_default(),
        }
    }

    #[must_use]
    pub const fn foreground(self) -> u8 {
        self.foreground
    }

    #[must_use]
    pub const fn background(self) -> u8 {
        self.background
    }

    fn foreground_color(self, legacy_index: u8) -> TextColor {
        if legacy_index == self.foreground {
            TextColor::default()
        } else {
            TextColor::index256(TextColor::transpose_legacy_index(legacy_index))
        }
    }

    fn background_color(self, legacy_index: u8) -> TextColor {
        if legacy_index == self.background {
            TextColor::default()
        } else {
            TextColor::index256(TextColor::transpose_legacy_index(legacy_index))
        }
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct TextAttribute {
    attrs: u16,
    hyperlink_id: u16,
    foreground: TextColor,
    background: TextColor,
    underline_color: TextColor,
    mark_kind: MarkKind,
}

impl TextAttribute {
    #[must_use]
    pub fn from_legacy(attribute: u16, defaults: LegacyColorDefaults) -> Self {
        let fg = u8::try_from(attribute & FG_ATTRS).unwrap_or_default();
        let bg = u8::try_from((attribute & BG_ATTRS) >> 4).unwrap_or_default();
        Self {
            attrs: attribute & USED_META_ATTRS,
            foreground: defaults.foreground_color(fg),
            background: defaults.background_color(bg),
            ..Self::default()
        }
    }

    #[must_use]
    pub fn from_rgb(foreground: Rgb, background: Rgb) -> Self {
        Self {
            attrs: 0,
            hyperlink_id: 0,
            foreground: TextColor::rgb(foreground.r, foreground.g, foreground.b),
            background: TextColor::rgb(background.r, background.g, background.b),
            underline_color: TextColor::default(),
            mark_kind: MarkKind::None,
        }
    }

    #[must_use]
    pub fn legacy_attributes(self, defaults: LegacyColorDefaults) -> u16 {
        let foreground = u16::from(self.foreground.legacy_index(defaults.foreground));
        let background = u16::from(self.background.legacy_index(defaults.background));
        let brighten = self.is_intense() && self.foreground.can_be_brightened();
        foreground
            | (background << 4)
            | (self.attrs & USED_META_ATTRS)
            | if brighten { FOREGROUND_INTENSITY } else { 0 }
    }

    #[must_use]
    pub const fn foreground(self) -> TextColor {
        self.foreground
    }

    #[must_use]
    pub const fn background(self) -> TextColor {
        self.background
    }

    #[must_use]
    pub const fn underline_color(self) -> TextColor {
        self.underline_color
    }

    #[must_use]
    pub const fn hyperlink_id(self) -> u16 {
        self.hyperlink_id
    }

    #[must_use]
    pub const fn mark_kind(self) -> MarkKind {
        self.mark_kind
    }

    #[must_use]
    pub const fn character_attributes(self) -> u16 {
        self.attrs
    }

    pub const fn set_foreground(&mut self, color: TextColor) {
        self.foreground = color;
    }

    pub const fn set_background(&mut self, color: TextColor) {
        self.background = color;
    }

    pub const fn set_underline_color(&mut self, color: TextColor) {
        self.underline_color = color;
    }

    pub const fn set_hyperlink_id(&mut self, id: u16) {
        self.hyperlink_id = id;
    }

    pub const fn set_mark_kind(&mut self, kind: MarkKind) {
        self.mark_kind = kind;
    }

    pub fn set_default_foreground(&mut self) {
        self.foreground.set_default();
    }

    pub fn set_default_background(&mut self) {
        self.background.set_default();
    }

    pub fn set_default_underline_color(&mut self) {
        self.underline_color.set_default();
    }

    #[must_use]
    pub const fn is_legacy(self) -> bool {
        self.foreground.is_legacy() && self.background.is_legacy()
    }

    #[must_use]
    pub const fn is_hyperlink(self) -> bool {
        self.hyperlink_id != 0
    }

    #[must_use]
    pub const fn is_intense(self) -> bool {
        self.has(ATTR_INTENSE)
    }

    #[must_use]
    pub const fn is_faint(self) -> bool {
        self.has(ATTR_FAINT)
    }

    #[must_use]
    pub const fn is_italic(self) -> bool {
        self.has(ATTR_ITALICS)
    }

    #[must_use]
    pub const fn is_blinking(self) -> bool {
        self.has(ATTR_BLINKING)
    }

    #[must_use]
    pub const fn is_invisible(self) -> bool {
        self.has(ATTR_INVISIBLE)
    }

    #[must_use]
    pub const fn is_crossed_out(self) -> bool {
        self.has(ATTR_CROSSED_OUT)
    }

    #[must_use]
    pub const fn is_overlined(self) -> bool {
        self.has(ATTR_TOP_GRIDLINE)
    }

    #[must_use]
    pub const fn is_reverse_video(self) -> bool {
        self.has(ATTR_REVERSE_VIDEO)
    }

    #[must_use]
    pub const fn is_protected(self) -> bool {
        self.has(ATTR_PROTECTED)
    }

    #[must_use]
    pub const fn is_top_gridline(self) -> bool {
        self.has(ATTR_TOP_GRIDLINE)
    }

    #[must_use]
    pub const fn is_bottom_gridline(self) -> bool {
        self.has(ATTR_BOTTOM_GRIDLINE)
    }

    #[must_use]
    pub const fn is_left_gridline(self) -> bool {
        self.has(ATTR_LEFT_GRIDLINE)
    }

    #[must_use]
    pub const fn is_right_gridline(self) -> bool {
        self.has(ATTR_RIGHT_GRIDLINE)
    }

    #[must_use]
    pub const fn is_any_gridline_enabled(self) -> bool {
        self.attrs
            & (ATTR_TOP_GRIDLINE | ATTR_BOTTOM_GRIDLINE | ATTR_LEFT_GRIDLINE | ATTR_RIGHT_GRIDLINE)
            != 0
    }

    #[must_use]
    pub const fn is_bold(self, intense_is_bold: bool) -> bool {
        self.is_intense() && (intense_is_bold || !self.foreground.can_be_brightened())
    }

    pub fn set_intense(&mut self, enabled: bool) {
        self.set_flag(ATTR_INTENSE, enabled);
    }

    pub fn set_faint(&mut self, enabled: bool) {
        self.set_flag(ATTR_FAINT, enabled);
    }

    pub fn set_italic(&mut self, enabled: bool) {
        self.set_flag(ATTR_ITALICS, enabled);
    }

    pub fn set_blinking(&mut self, enabled: bool) {
        self.set_flag(ATTR_BLINKING, enabled);
    }

    pub fn set_invisible(&mut self, enabled: bool) {
        self.set_flag(ATTR_INVISIBLE, enabled);
    }

    pub fn set_crossed_out(&mut self, enabled: bool) {
        self.set_flag(ATTR_CROSSED_OUT, enabled);
    }

    pub fn set_overlined(&mut self, enabled: bool) {
        self.set_flag(ATTR_TOP_GRIDLINE, enabled);
    }

    pub fn set_reverse_video(&mut self, enabled: bool) {
        self.set_flag(ATTR_REVERSE_VIDEO, enabled);
    }

    pub fn set_protected(&mut self, enabled: bool) {
        self.set_flag(ATTR_PROTECTED, enabled);
    }

    pub fn set_left_gridline(&mut self, enabled: bool) {
        self.set_flag(ATTR_LEFT_GRIDLINE, enabled);
    }

    pub fn set_right_gridline(&mut self, enabled: bool) {
        self.set_flag(ATTR_RIGHT_GRIDLINE, enabled);
    }

    pub fn invert(&mut self) {
        self.attrs ^= ATTR_REVERSE_VIDEO;
    }

    #[must_use]
    pub fn underline_style(self) -> UnderlineStyle {
        match (self.attrs & UNDERLINE_STYLE_MASK) >> UNDERLINE_STYLE_SHIFT {
            0 => UnderlineStyle::None,
            2 => UnderlineStyle::Double,
            3 => UnderlineStyle::Curly,
            4 => UnderlineStyle::Dotted,
            5 => UnderlineStyle::Dashed,
            _ => UnderlineStyle::Single,
        }
    }

    #[must_use]
    pub fn is_underlined(self) -> bool {
        self.underline_style() != UnderlineStyle::None
    }

    pub fn set_underline_style(&mut self, style: UnderlineStyle) {
        self.attrs =
            (self.attrs & !UNDERLINE_STYLE_MASK) | ((style as u16) << UNDERLINE_STYLE_SHIFT);
    }

    pub fn set_default_rendition_attributes(&mut self) {
        self.attrs &= !ATTR_RENDITION;
    }

    pub fn set_standard_erase(&mut self) {
        self.attrs = 0;
        self.hyperlink_id = 0;
        self.mark_kind = MarkKind::None;
    }

    #[must_use]
    pub const fn background_is_default(self) -> bool {
        self.background.is_default()
    }

    #[must_use]
    pub fn has_identical_visual_representation_for_blank_space(
        self,
        other: Self,
        inverted: bool,
    ) -> bool {
        let check_foreground = inverted != self.is_reverse_video();
        !self.is_any_gridline_enabled()
            && !self.is_underlined()
            && !self.is_crossed_out()
            && !self.is_hyperlink()
            && self.attrs == other.attrs
            && if check_foreground {
                self.foreground == other.foreground
            } else {
                self.background == other.background
            }
            && self.is_hyperlink() == other.is_hyperlink()
    }

    const fn has(self, flag: u16) -> bool {
        self.attrs & flag != 0
    }

    fn set_flag(&mut self, flag: u16, enabled: bool) {
        if enabled {
            self.attrs |= flag;
        } else {
            self.attrs &= !flag;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn legacy_attributes_round_trip_with_windows_color_order() {
        let defaults = LegacyColorDefaults::default();
        let legacy = 0x0041; // red background, blue foreground in legacy BGRI ordering
        let attribute = TextAttribute::from_legacy(legacy, defaults);
        assert!(attribute.is_legacy());
        assert_eq!(attribute.legacy_attributes(defaults), legacy);
    }

    #[test]
    fn legacy_meta_bits_round_trip() {
        let defaults = LegacyColorDefaults::default();
        for flag in [0x0400, 0x0800, 0x1000, 0x4000, 0x8000] {
            let legacy = 0x0041 | flag;
            let attribute = TextAttribute::from_legacy(legacy, defaults);
            assert_eq!(attribute.legacy_attributes(defaults), legacy);
        }
    }

    #[test]
    fn custom_legacy_defaults_map_to_default_text_colors() {
        let defaults = LegacyColorDefaults::from_legacy_attribute(0x0014);
        let attribute = TextAttribute::from_legacy(0x0024, defaults);
        assert!(attribute.foreground().is_default());
        assert!(!attribute.background().is_default());
        assert_eq!(attribute.legacy_attributes(defaults), 0x0024);
    }

    #[test]
    fn rendition_flags_and_underline_style_are_independent() {
        let mut attribute = TextAttribute::default();
        attribute.set_intense(true);
        attribute.set_italic(true);
        attribute.set_underline_style(UnderlineStyle::Curly);
        attribute.set_protected(true);

        assert!(attribute.is_intense());
        assert!(attribute.is_italic());
        assert!(attribute.is_underlined());
        assert_eq!(attribute.underline_style(), UnderlineStyle::Curly);
        assert!(attribute.is_protected());

        attribute.set_default_rendition_attributes();
        assert!(!attribute.is_intense());
        assert!(!attribute.is_italic());
        assert!(!attribute.is_underlined());
        assert!(attribute.is_protected());
    }

    #[test]
    fn standard_erase_preserves_colors_but_clears_metadata() {
        let mut attribute = TextAttribute::from_rgb(Rgb::new(1, 2, 3), Rgb::new(4, 5, 6));
        attribute.set_reverse_video(true);
        attribute.set_hyperlink_id(42);
        attribute.set_mark_kind(MarkKind::Output);
        let foreground = attribute.foreground();
        let background = attribute.background();

        attribute.set_standard_erase();
        assert_eq!(attribute.character_attributes(), 0);
        assert_eq!(attribute.hyperlink_id(), 0);
        assert_eq!(attribute.mark_kind(), MarkKind::None);
        assert_eq!(attribute.foreground(), foreground);
        assert_eq!(attribute.background(), background);
    }

    #[test]
    fn invert_only_toggles_reverse_video() {
        let mut attribute = TextAttribute::default();
        assert!(!attribute.is_reverse_video());
        attribute.invert();
        assert!(attribute.is_reverse_video());
        attribute.invert();
        assert!(!attribute.is_reverse_video());
    }

    #[test]
    fn blank_space_visual_equivalence_uses_visible_color_side() {
        let mut left = TextAttribute::default();
        let mut right = TextAttribute::default();
        left.set_foreground(TextColor::rgb(1, 2, 3));
        right.set_foreground(TextColor::rgb(1, 2, 3));
        left.set_background(TextColor::rgb(9, 9, 9));
        right.set_background(TextColor::rgb(8, 8, 8));

        assert!(!left.has_identical_visual_representation_for_blank_space(right, false));
        left.set_reverse_video(true);
        right.set_reverse_video(true);
        assert!(left.has_identical_visual_representation_for_blank_space(right, false));
    }
}
