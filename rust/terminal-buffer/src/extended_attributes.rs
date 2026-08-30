//! Portable SGR extended-text-attribute state for Host `ScreenBufferTests`.
//!
//! This owner keeps rendition bits and foreground/background color encoding in
//! the same `TextAttribute` value that is persisted to written cells. It covers
//! the regression surface from microsoft/terminal#2554 without host globals.

use crate::text_attribute::{TextAttribute, UnderlineStyle};
use crate::text_color::TextColor;

#[derive(Debug, Clone, PartialEq, Eq, Default)]
pub struct ExtendedAttributesState {
    current: TextAttribute,
    written: Vec<TextAttribute>,
}

impl ExtendedAttributesState {
    #[must_use]
    pub const fn current(&self) -> TextAttribute {
        self.current
    }

    #[must_use]
    pub fn written(&self) -> &[TextAttribute] {
        &self.written
    }

    pub fn write_cell(&mut self) {
        self.written.push(self.current);
    }

    pub fn apply_sgr(&mut self, params: &[u16]) {
        let params = if params.is_empty() { &[0][..] } else { params };
        let mut i = 0;
        while i < params.len() {
            match params[i] {
                0 => self.current = TextAttribute::default(),
                1 => self.current.set_intense(true),
                2 => self.current.set_faint(true),
                3 => self.current.set_italic(true),
                4 => self.current.set_underline_style(UnderlineStyle::Single),
                5 => self.current.set_blinking(true),
                8 => self.current.set_invisible(true),
                9 => self.current.set_crossed_out(true),
                21 => self.current.set_underline_style(UnderlineStyle::Double),
                22 => {
                    self.current.set_intense(false);
                    self.current.set_faint(false);
                }
                23 => self.current.set_italic(false),
                24 => self.current.set_underline_style(UnderlineStyle::None),
                25 => self.current.set_blinking(false),
                28 => self.current.set_invisible(false),
                29 => self.current.set_crossed_out(false),
                30..=37 => self
                    .current
                    .set_foreground(TextColor::index16((params[i] - 30) as u8)),
                39 => self.current.set_default_foreground(),
                40..=47 => self
                    .current
                    .set_background(TextColor::index16((params[i] - 40) as u8)),
                49 => self.current.set_default_background(),
                38 | 48 => {
                    let foreground = params[i] == 38;
                    if params.get(i + 1) == Some(&5) {
                        if let Some(index) = params.get(i + 2).and_then(|v| u8::try_from(*v).ok()) {
                            if foreground {
                                self.current.set_foreground(TextColor::index256(index));
                            } else {
                                self.current.set_background(TextColor::index256(index));
                            }
                            i += 2;
                        }
                    } else if params.get(i + 1) == Some(&2) && i + 4 < params.len() {
                        let rgb = (
                            u8::try_from(params[i + 2]).ok(),
                            u8::try_from(params[i + 3]).ok(),
                            u8::try_from(params[i + 4]).ok(),
                        );
                        if let (Some(r), Some(g), Some(b)) = rgb {
                            if foreground {
                                self.current.set_foreground(TextColor::rgb(r, g, b));
                            } else {
                                self.current.set_background(TextColor::rgb(r, g, b));
                            }
                            i += 4;
                        }
                    }
                }
                _ => {}
            }
            i += 1;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn set_extended(state: &mut ExtendedAttributesState, mask: u8) {
        if mask & 0x01 != 0 {
            state.apply_sgr(&[1]);
        }
        if mask & 0x02 != 0 {
            state.apply_sgr(&[2]);
        }
        if mask & 0x04 != 0 {
            state.apply_sgr(&[3]);
        }
        if mask & 0x08 != 0 {
            state.apply_sgr(&[4]);
        } else if mask & 0x10 != 0 {
            state.apply_sgr(&[21]);
        }
        if mask & 0x20 != 0 {
            state.apply_sgr(&[5]);
        }
        if mask & 0x40 != 0 {
            state.apply_sgr(&[8]);
        }
        if mask & 0x80 != 0 {
            state.apply_sgr(&[9]);
        }
    }

    fn reset_extended_one_by_one(state: &mut ExtendedAttributesState, mask: u8) {
        if mask & 0x03 != 0 {
            state.apply_sgr(&[22]);
            state.write_cell();
        }
        if mask & 0x04 != 0 {
            state.apply_sgr(&[23]);
            state.write_cell();
        }
        if mask & 0x18 != 0 {
            state.apply_sgr(&[24]);
            state.write_cell();
        }
        if mask & 0x20 != 0 {
            state.apply_sgr(&[25]);
            state.write_cell();
        }
        if mask & 0x40 != 0 {
            state.apply_sgr(&[28]);
            state.write_cell();
        }
        if mask & 0x80 != 0 {
            state.apply_sgr(&[29]);
            state.write_cell();
        }
    }

    #[test]
    fn microsoft_extended_text_attributes_contract() {
        for mask in 0u8..=u8::MAX {
            let mut state = ExtendedAttributesState::default();
            set_extended(&mut state, mask);
            let expected = state.current();
            state.write_cell();
            assert_eq!(
                state.written()[0].character_attributes(),
                expected.character_attributes()
            );
            reset_extended_one_by_one(&mut state, mask);
            state.apply_sgr(&[0]);
            assert_eq!(state.current(), TextAttribute::default());
        }
    }

    #[test]
    fn microsoft_extended_text_attributes_with_colors_contract() {
        let foregrounds: &[&[u16]] = &[&[39], &[32], &[38, 5, 20], &[38, 2, 1, 2, 3]];
        let backgrounds: &[&[u16]] = &[&[49], &[42], &[48, 5, 20], &[48, 2, 1, 2, 3]];

        for mask in 0u8..=u8::MAX {
            for foreground in foregrounds {
                for background in backgrounds {
                    let mut state = ExtendedAttributesState::default();
                    set_extended(&mut state, mask);
                    state.apply_sgr(foreground);
                    state.apply_sgr(background);
                    let colored = state.current();
                    state.write_cell();
                    assert_eq!(state.written()[0], colored);

                    reset_extended_one_by_one(&mut state, mask);
                    assert_eq!(state.current().foreground(), colored.foreground());
                    assert_eq!(state.current().background(), colored.background());

                    state.apply_sgr(&[39]);
                    assert!(state.current().foreground().is_default());
                    assert_eq!(state.current().background(), colored.background());
                    state.apply_sgr(&[49]);
                    assert!(state.current().background().is_default());
                }
            }
        }
    }
}
