#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum ReferenceFrame {
    #[default]
    None,
    Absolute,
    FontSize,
    AdvanceWidth,
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CssLengthPercentage {
    value: f32,
    reference_frame: ReferenceFrame,
}

impl CssLengthPercentage {
    #[must_use]
    pub fn from_css(input: &str) -> Self {
        let input = input.trim_start();
        let Some((value, suffix)) = parse_number_prefix(input) else {
            return Self::default();
        };

        let (reference_frame, value) = match suffix {
            "" => (ReferenceFrame::FontSize, value),
            "%" => (ReferenceFrame::FontSize, value / 100.0),
            "px" => (ReferenceFrame::Absolute, value / 96.0),
            "pt" => (ReferenceFrame::Absolute, value / 72.0),
            "ch" => (ReferenceFrame::AdvanceWidth, value),
            _ => return Self::default(),
        };

        Self {
            value,
            reference_frame,
        }
    }

    #[must_use]
    pub const fn reference_frame(self) -> ReferenceFrame {
        self.reference_frame
    }

    #[must_use]
    pub const fn value(self) -> f32 {
        self.value
    }

    #[must_use]
    pub fn resolve(self, fallback: f32, dpi: f32, font_size: f32, advance_width: f32) -> f32 {
        match self.reference_frame {
            ReferenceFrame::Absolute => self.value * dpi,
            ReferenceFrame::FontSize => self.value * font_size,
            ReferenceFrame::AdvanceWidth => self.value * advance_width,
            ReferenceFrame::None => fallback,
        }
    }
}

fn parse_number_prefix(input: &str) -> Option<(f32, &str)> {
    let mut splits = input
        .char_indices()
        .map(|(index, _)| index)
        .collect::<Vec<_>>();
    splits.push(input.len());

    for split in splits.into_iter().rev() {
        if split == 0 {
            continue;
        }

        let candidate = &input[..split];
        if let Ok(value) = candidate.parse::<f32>() {
            if value.is_finite() {
                return Some((value, &input[split..]));
            }
            return None;
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::{CssLengthPercentage, ReferenceFrame};

    #[test]
    fn empty_and_invalid_inputs_are_unset() {
        for input in ["", " ", "bogus", "12em", "12 px", "1e1000"] {
            let value = CssLengthPercentage::from_css(input);
            assert_eq!(value.reference_frame(), ReferenceFrame::None, "{input}");
            assert_eq!(
                value.resolve(7.0, 96.0, 12.0, 8.0).to_bits(),
                7.0_f32.to_bits(),
                "{input}"
            );
        }
    }

    #[test]
    fn unitless_and_percent_values_are_relative_to_font_size() {
        let unitless = CssLengthPercentage::from_css("1.25");
        assert_eq!(unitless.reference_frame(), ReferenceFrame::FontSize);
        assert_eq!(
            unitless.resolve(0.0, 96.0, 16.0, 8.0).to_bits(),
            20.0_f32.to_bits()
        );

        let percent = CssLengthPercentage::from_css("125%");
        assert_eq!(percent.reference_frame(), ReferenceFrame::FontSize);
        assert_eq!(
            percent.resolve(0.0, 96.0, 16.0, 8.0).to_bits(),
            20.0_f32.to_bits()
        );
    }

    #[test]
    fn pixels_and_points_are_normalized_to_inches() {
        let pixels = CssLengthPercentage::from_css("96px");
        assert_eq!(pixels.reference_frame(), ReferenceFrame::Absolute);
        assert_eq!(pixels.value().to_bits(), 1.0_f32.to_bits());
        assert_eq!(
            pixels.resolve(0.0, 144.0, 0.0, 0.0).to_bits(),
            144.0_f32.to_bits()
        );

        let points = CssLengthPercentage::from_css("72pt");
        assert_eq!(points.reference_frame(), ReferenceFrame::Absolute);
        assert_eq!(points.value().to_bits(), 1.0_f32.to_bits());
        assert_eq!(
            points.resolve(0.0, 144.0, 0.0, 0.0).to_bits(),
            144.0_f32.to_bits()
        );
    }

    #[test]
    fn ch_values_are_relative_to_advance_width() {
        let value = CssLengthPercentage::from_css("1.5ch");
        assert_eq!(value.reference_frame(), ReferenceFrame::AdvanceWidth);
        assert_eq!(
            value.resolve(0.0, 96.0, 16.0, 8.0).to_bits(),
            12.0_f32.to_bits()
        );
    }

    #[test]
    fn leading_whitespace_matches_wcstof_but_trailing_whitespace_is_invalid() {
        assert_eq!(
            CssLengthPercentage::from_css(" 2px").reference_frame(),
            ReferenceFrame::Absolute
        );
        assert_eq!(
            CssLengthPercentage::from_css("2px ").reference_frame(),
            ReferenceFrame::None
        );
    }

    #[test]
    fn exponent_and_sign_forms_are_supported() {
        let value = CssLengthPercentage::from_css("-1.25e1%");
        assert_eq!(value.reference_frame(), ReferenceFrame::FontSize);
        assert_eq!(
            value.resolve(0.0, 96.0, 80.0, 8.0).to_bits(),
            (-10.0_f32).to_bits()
        );
    }
}
