//! Portable RGBA color semantics matching `til::color`.

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct TilColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl TilColor {
    #[must_use]
    pub const fn rgb(r: u8, g: u8, b: u8) -> Self {
        Self { r, g, b, a: 255 }
    }

    #[must_use]
    pub const fn rgba(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self { r, g, b, a }
    }

    #[must_use]
    pub const fn from_colorref(colorref: u32) -> Self {
        Self {
            r: (colorref & 0xff) as u8,
            g: ((colorref >> 8) & 0xff) as u8,
            b: ((colorref >> 16) & 0xff) as u8,
            a: 255,
        }
    }

    #[must_use]
    pub const fn to_colorref(self) -> u32 {
        (self.r as u32) | ((self.g as u32) << 8) | ((self.b as u32) << 16)
    }

    #[must_use]
    pub const fn from_integral_rgba(r: i32, g: i32, b: i32, a: i32) -> Self {
        Self::rgba(r as u8, g as u8, b as u8, a as u8)
    }

    #[must_use]
    pub fn from_float_rgba(r: f32, g: f32, b: f32, a: f32) -> Self {
        Self::rgba(
            (r * 255.0) as u8,
            (g * 255.0) as u8,
            (b * 255.0) as u8,
            (a * 255.0) as u8,
        )
    }

    #[must_use]
    pub const fn with_alpha(self, alpha: u8) -> Self {
        Self { a: alpha, ..self }
    }

    #[must_use]
    pub fn layer_over(self, destination: Self) -> Self {
        let inverse_alpha = f32::from(255 - self.a) / 255.0;
        let result_a = f32::from(self.a) + f32::from(destination.a) * inverse_alpha;
        let result_r = (f32::from(self.r) * f32::from(self.a)
            + f32::from(destination.r) * f32::from(destination.a) * inverse_alpha)
            / result_a;
        let result_g = (f32::from(self.g) * f32::from(self.a)
            + f32::from(destination.g) * f32::from(destination.a) * inverse_alpha)
            / result_a;
        let result_b = (f32::from(self.b) * f32::from(self.a)
            + f32::from(destination.b) * f32::from(destination.a) * inverse_alpha)
            / result_a;

        Self::rgba(
            (result_r + 0.5) as u8,
            (result_g + 0.5) as u8,
            (result_b + 0.5) as u8,
            (result_a + 0.5) as u8,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_til_color_construct_contract() {
        let rgb = TilColor::rgb(0xde, 0xad, 0xbe);
        assert_eq!(rgb, TilColor::rgba(0xde, 0xad, 0xbe, 0xff));
        let rgba = TilColor::rgba(0xde, 0xad, 0xbe, 0xef);
        assert_ne!(rgb, rgba);
    }

    #[test]
    fn microsoft_til_color_from_colorref_contract() {
        assert_eq!(
            TilColor::from_colorref(0x00fe_edfa),
            TilColor::rgba(0xfa, 0xed, 0xfe, 0xff)
        );
    }

    #[test]
    fn microsoft_til_color_to_colorref_contract() {
        assert_eq!(
            TilColor::rgba(0xf0, 0x0d, 0xca, 0xfe).to_colorref(),
            0x00ca_0df0
        );
    }

    #[test]
    fn microsoft_til_color_from_integral_struct_contract() {
        assert_eq!(
            TilColor::from_integral_rgba(0xca, 0xfe, 0xf0, 0x0d),
            TilColor::rgba(0xca, 0xfe, 0xf0, 0x0d)
        );
        assert_eq!(
            TilColor::from_integral_rgba(0xfa, 0xce, 0xb0, 0x17),
            TilColor::rgba(0xfa, 0xce, 0xb0, 0x17)
        );
    }

    #[test]
    fn microsoft_til_color_from_float_struct_contract() {
        assert_eq!(
            TilColor::from_float_rgba(0.730, 0.867, 0.793, 0.997),
            TilColor::rgba(0xba, 0xdd, 0xca, 0xfe)
        );
        assert_eq!(
            TilColor::from_float_rgba(0.871, 0.679, 0.981, 0.067),
            TilColor::rgba(0xde, 0xad, 0xfa, 0x11)
        );
    }

    #[test]
    fn microsoft_til_color_with_alpha_contract() {
        let color = TilColor::from_colorref(0x00fe_edfa);
        assert_eq!(
            color.with_alpha(0x7f),
            TilColor::rgba(0xfa, 0xed, 0xfe, 0x7f)
        );
        assert_ne!(color.with_alpha(0x7f), color);
    }

    #[test]
    fn microsoft_til_color_layer_over_contract() {
        let orange = TilColor::rgba(255, 165, 0, 255);
        let blue = TilColor::rgba(0, 205, 255, 255);
        let orange_alpha = TilColor::rgba(255, 165, 0, 165);
        let blue_alpha = TilColor::rgba(0, 205, 255, 205);

        assert_eq!(orange.layer_over(blue), orange);
        assert_eq!(blue.layer_over(orange), blue);
        assert_eq!(
            orange_alpha.layer_over(blue),
            TilColor::rgba(165, 179, 90, 255)
        );
        assert_eq!(
            orange_alpha.layer_over(blue_alpha),
            TilColor::rgba(177, 177, 78, 237)
        );
        assert_eq!(
            blue_alpha.layer_over(orange),
            TilColor::rgba(50, 197, 205, 255)
        );
        assert_eq!(
            blue_alpha.layer_over(orange_alpha),
            TilColor::rgba(35, 200, 220, 237)
        );
    }
}
