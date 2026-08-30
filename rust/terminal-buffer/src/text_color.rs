//! Safe Rust representation of Windows Terminal `TextColor` semantics.

pub const TABLE_SIZE: usize = 267;
pub const DEFAULT_FOREGROUND: usize = 261;
pub const DEFAULT_BACKGROUND: usize = 262;
pub const FRAME_FOREGROUND: usize = 263;
pub const FRAME_BACKGROUND: usize = 264;
pub const CURSOR_COLOR: usize = 265;
pub const SELECTION_BACKGROUND: usize = 266;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Rgb {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

impl Rgb {
    #[must_use]
    pub const fn new(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b }
    }
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum ColorType {
    #[default]
    Default,
    Index16,
    Index256,
    Rgb,
}

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct TextColor {
    kind: ColorType,
    value: Rgb,
}

impl TextColor {
    pub const DARK_BLACK: u8 = 0;
    pub const DARK_RED: u8 = 1;
    pub const DARK_GREEN: u8 = 2;
    pub const DARK_YELLOW: u8 = 3;
    pub const DARK_BLUE: u8 = 4;
    pub const DARK_MAGENTA: u8 = 5;
    pub const DARK_CYAN: u8 = 6;
    pub const DARK_WHITE: u8 = 7;
    pub const BRIGHT_BLACK: u8 = 8;
    pub const BRIGHT_RED: u8 = 9;
    pub const BRIGHT_GREEN: u8 = 10;
    pub const BRIGHT_YELLOW: u8 = 11;
    pub const BRIGHT_BLUE: u8 = 12;
    pub const BRIGHT_MAGENTA: u8 = 13;
    pub const BRIGHT_CYAN: u8 = 14;
    pub const BRIGHT_WHITE: u8 = 15;

    #[must_use]
    pub const fn index16(index: u8) -> Self {
        Self {
            kind: ColorType::Index16,
            value: Rgb::new(index, 0, 0),
        }
    }

    #[must_use]
    pub const fn index256(index: u8) -> Self {
        Self {
            kind: ColorType::Index256,
            value: Rgb::new(index, 0, 0),
        }
    }

    #[must_use]
    pub const fn rgb(r: u8, g: u8, b: u8) -> Self {
        Self {
            kind: ColorType::Rgb,
            value: Rgb::new(r, g, b),
        }
    }

    #[must_use]
    pub const fn color_type(self) -> ColorType {
        self.kind
    }

    #[must_use]
    pub const fn index(self) -> u8 {
        self.value.r
    }

    #[must_use]
    pub const fn rgb_value(self) -> Rgb {
        self.value
    }

    #[must_use]
    pub const fn can_be_brightened(self) -> bool {
        matches!(self.kind, ColorType::Index16 | ColorType::Default)
    }

    #[must_use]
    pub const fn is_default(self) -> bool {
        matches!(self.kind, ColorType::Default)
    }

    #[must_use]
    pub const fn is_index16(self) -> bool {
        matches!(self.kind, ColorType::Index16)
    }

    #[must_use]
    pub const fn is_index256(self) -> bool {
        matches!(self.kind, ColorType::Index256)
    }

    #[must_use]
    pub const fn is_rgb(self) -> bool {
        matches!(self.kind, ColorType::Rgb)
    }

    #[must_use]
    pub const fn is_legacy(self) -> bool {
        matches!(self.kind, ColorType::Index16 | ColorType::Index256) && self.index() < 16
    }

    #[must_use]
    pub const fn is_default_or_legacy(self) -> bool {
        !matches!(self.kind, ColorType::Rgb) && self.index() < 16
    }

    pub fn set_default(&mut self) {
        *self = Self::default();
    }

    pub fn set_index(&mut self, index: u8, is_index256: bool) {
        *self = if is_index256 {
            Self::index256(index)
        } else {
            Self::index16(index)
        };
    }

    pub fn set_rgb(&mut self, color: Rgb) {
        *self = Self::rgb(color.r, color.g, color.b);
    }

    /// Resolves this color through a Windows Terminal color table.
    #[must_use]
    pub fn resolve(self, table: &[Rgb; TABLE_SIZE], default_index: usize, brighten: bool) -> Rgb {
        match self.kind {
            ColorType::Default => {
                let default = table[default_index];
                if brighten {
                    table[..8]
                        .iter()
                        .position(|candidate| *candidate == default)
                        .map_or(default, |index| table[index + 8])
                } else {
                    default
                }
            }
            ColorType::Rgb => self.value,
            ColorType::Index16 if brighten => table[usize::from(self.index() | 8)],
            ColorType::Index16 | ColorType::Index256 => table[usize::from(self.index())],
        }
    }

    #[must_use]
    pub fn legacy_index(self, default_index: u8) -> u8 {
        match self.kind {
            ColorType::Default => default_index,
            ColorType::Index16 | ColorType::Index256 => {
                INDEX256_TO_INDEX16[usize::from(self.index())]
            }
            ColorType::Rgb => {
                let compressed = (self.value.r & 0b1110_0000)
                    + ((self.value.g >> 3) & 0b0001_1100)
                    + ((self.value.b >> 6) & 0b0000_0011);
                COMPRESSED_RGB_TO_INDEX16[usize::from(compressed)]
            }
        }
    }

    /// Converts between legacy Windows BGRI nibble ordering and ANSI RGBI.
    #[must_use]
    pub const fn transpose_legacy_index(index: u8) -> u8 {
        let one_bit_set = (index ^ (index >> 2)) & 1;
        index ^ one_bit_set ^ (one_bit_set << 2)
    }
}

const COMPRESSED_RGB_TO_INDEX16: [u8; 256] = [
    0, 1, 1, 9, 0, 0, 1, 1, 2, 1, 1, 1, 2, 8, 1, 9, 2, 2, 3, 3, 2, 2, 11, 3, 10, 10, 11, 11, 10,
    10, 10, 11, 0, 5, 1, 1, 0, 0, 1, 1, 8, 1, 1, 1, 2, 8, 1, 9, 2, 2, 3, 3, 2, 2, 11, 3, 10, 10,
    10, 11, 10, 10, 10, 11, 5, 5, 5, 1, 4, 5, 1, 1, 8, 8, 1, 9, 2, 8, 9, 9, 2, 2, 3, 3, 2, 2, 11,
    3, 10, 10, 11, 11, 10, 10, 10, 11, 4, 5, 5, 1, 4, 5, 5, 1, 8, 5, 5, 1, 8, 8, 9, 9, 2, 2, 8, 9,
    10, 2, 11, 3, 10, 10, 11, 11, 10, 10, 10, 11, 4, 13, 5, 5, 4, 13, 5, 5, 4, 13, 13, 13, 6, 8,
    13, 9, 6, 8, 8, 9, 10, 10, 11, 3, 10, 10, 11, 11, 10, 10, 10, 11, 4, 13, 13, 13, 4, 13, 13, 13,
    4, 12, 13, 13, 6, 12, 13, 13, 6, 6, 8, 9, 6, 6, 7, 7, 10, 14, 14, 7, 10, 10, 14, 11, 4, 12, 13,
    13, 4, 12, 13, 13, 4, 12, 13, 13, 6, 12, 12, 13, 6, 6, 12, 7, 6, 6, 7, 7, 6, 14, 14, 7, 14, 14,
    14, 15, 12, 12, 13, 13, 12, 12, 13, 13, 12, 12, 12, 13, 12, 12, 12, 13, 6, 12, 12, 7, 6, 6, 7,
    7, 6, 14, 14, 7, 14, 14, 14, 15,
];

const INDEX256_TO_INDEX16: [u8; 256] = [
    0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15, 0, 1, 1, 1, 9, 9, 2, 1, 1, 1, 1, 1, 2, 2,
    3, 3, 3, 3, 2, 2, 11, 11, 3, 3, 10, 10, 11, 11, 11, 11, 10, 10, 10, 10, 11, 11, 5, 5, 5, 5, 1,
    1, 8, 8, 1, 1, 9, 9, 2, 2, 3, 3, 3, 3, 2, 2, 11, 11, 3, 3, 10, 10, 11, 11, 11, 11, 10, 10, 10,
    10, 11, 11, 4, 13, 5, 5, 5, 5, 4, 13, 13, 13, 13, 13, 6, 8, 8, 8, 9, 9, 10, 10, 11, 11, 3, 3,
    10, 10, 11, 11, 11, 11, 10, 10, 10, 10, 11, 11, 4, 13, 13, 13, 13, 13, 4, 12, 13, 13, 13, 13,
    6, 6, 8, 8, 9, 9, 6, 6, 7, 7, 7, 7, 10, 14, 14, 14, 7, 7, 10, 10, 14, 14, 11, 11, 4, 12, 13,
    13, 13, 13, 4, 12, 13, 13, 13, 13, 6, 6, 12, 12, 7, 7, 6, 6, 7, 7, 7, 7, 6, 14, 14, 14, 7, 7,
    14, 14, 14, 14, 15, 15, 12, 12, 13, 13, 13, 13, 12, 12, 12, 12, 13, 13, 6, 12, 12, 12, 7, 7, 6,
    6, 7, 7, 7, 7, 6, 14, 14, 14, 7, 7, 14, 14, 14, 14, 15, 15, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 15, 15,
];

#[cfg(test)]
mod tests {
    use super::*;

    fn table() -> [Rgb; TABLE_SIZE] {
        let mut table = [Rgb::default(); TABLE_SIZE];
        table[0] = Rgb::new(12, 12, 12);
        table[7] = Rgb::new(204, 204, 204);
        table[8] = Rgb::new(118, 118, 118);
        table[15] = Rgb::new(242, 242, 242);
        table[DEFAULT_FOREGROUND] = Rgb::new(1, 2, 3);
        table[DEFAULT_BACKGROUND] = Rgb::new(4, 5, 6);
        table
    }

    #[test]
    fn default_color_resolves_to_alias() {
        let color = TextColor::default();
        assert!(color.is_default());
        assert_eq!(
            color.resolve(&table(), DEFAULT_FOREGROUND, false),
            Rgb::new(1, 2, 3)
        );
    }

    #[test]
    fn dark_index_brightens_only_for_index16() {
        let table = table();
        let indexed = TextColor::index16(7);
        assert_eq!(indexed.resolve(&table, DEFAULT_FOREGROUND, false), table[7]);
        assert_eq!(indexed.resolve(&table, DEFAULT_FOREGROUND, true), table[15]);

        let indexed256 = TextColor::index256(7);
        assert_eq!(
            indexed256.resolve(&table, DEFAULT_FOREGROUND, true),
            table[7]
        );
    }

    #[test]
    fn rgb_color_is_independent_of_color_table() {
        let color = TextColor::rgb(7, 8, 9);
        assert!(color.is_rgb());
        assert_eq!(
            color.resolve(&table(), DEFAULT_BACKGROUND, true),
            Rgb::new(7, 8, 9)
        );
    }

    #[test]
    fn mutation_transitions_match_cpp_states() {
        let mut color = TextColor::rgb(7, 8, 9);
        color.set_default();
        assert!(color.is_default());
        color.set_index(7, false);
        assert!(color.is_index16());
        color.set_index(42, true);
        assert!(color.is_index256());
        color.set_rgb(Rgb::new(1, 2, 3));
        assert_eq!(color.rgb_value(), Rgb::new(1, 2, 3));
    }

    #[test]
    fn legacy_transpose_swaps_windows_red_and_blue_bits() {
        assert_eq!(TextColor::transpose_legacy_index(1), 4);
        assert_eq!(TextColor::transpose_legacy_index(4), 1);
        assert_eq!(TextColor::transpose_legacy_index(2), 2);
        assert_eq!(TextColor::transpose_legacy_index(7), 7);
        assert_eq!(TextColor::transpose_legacy_index(9), 12);
    }

    #[test]
    fn lossy_legacy_conversion_matches_terminal_tables() {
        assert_eq!(TextColor::index256(1).legacy_index(7), 4);
        assert_eq!(TextColor::index256(9).legacy_index(7), 12);
        assert_eq!(TextColor::rgb(255, 0, 0).legacy_index(7), 12);
    }
}
