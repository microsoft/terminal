//! Checked numeric conversion semantics mirrored from `til::math`.

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum IntegralRound {
    Ceiling,
    Flooring,
    Rounding,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct MathNarrowingError;

/// Rounds an `f64` with the requested policy and narrows it to `i32`.
///
/// # Errors
///
/// Returns [`MathNarrowingError`] for non-finite values or results outside the
/// signed 32-bit range.
pub fn checked_round_i32(value: f64, mode: IntegralRound) -> Result<i32, MathNarrowingError> {
    if !value.is_finite() {
        return Err(MathNarrowingError);
    }

    let rounded = match mode {
        IntegralRound::Ceiling => value.ceil(),
        IntegralRound::Flooring => value.floor(),
        IntegralRound::Rounding => value.round(),
    };
    if rounded < f64::from(i32::MIN) || rounded > f64::from(i32::MAX) {
        return Err(MathNarrowingError);
    }

    #[allow(clippy::cast_possible_truncation)]
    Ok(rounded as i32)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn verify(mode: IntegralRound, cases: &[(f64, Result<i32, MathNarrowingError>)]) {
        for &(given, expected) in cases {
            assert_eq!(checked_round_i32(given, mode), expected, "given={given}");
        }
    }

    #[test]
    fn microsoft_til_math_ceiling_full_matrix() {
        verify(
            IntegralRound::Ceiling,
            &[
                (1.0, Ok(1)),
                (1.9, Ok(2)),
                (-7.1, Ok(-7)),
                (-8.5, Ok(-8)),
                (f64::from(i32::MAX) - 0.1, Ok(i32::MAX)),
                (f64::from(i32::MIN) - 0.1, Ok(i32::MIN)),
                (f64::from(i32::MAX) + 1.1, Err(MathNarrowingError)),
                (f64::from(i32::MIN) - 1.1, Err(MathNarrowingError)),
                (f64::INFINITY, Err(MathNarrowingError)),
                (f64::NEG_INFINITY, Err(MathNarrowingError)),
                (f64::NAN, Err(MathNarrowingError)),
            ],
        );
    }

    #[test]
    fn microsoft_til_math_flooring_full_matrix() {
        verify(
            IntegralRound::Flooring,
            &[
                (1.0, Ok(1)),
                (1.9, Ok(1)),
                (-7.1, Ok(-8)),
                (-8.5, Ok(-9)),
                (f64::from(i32::MAX) + 0.1, Ok(i32::MAX)),
                (f64::from(i32::MIN) + 0.1, Ok(i32::MIN)),
                (f64::from(i32::MAX) + 1.1, Err(MathNarrowingError)),
                (f64::from(i32::MIN) - 1.1, Err(MathNarrowingError)),
                (f64::INFINITY, Err(MathNarrowingError)),
                (f64::NEG_INFINITY, Err(MathNarrowingError)),
                (f64::NAN, Err(MathNarrowingError)),
            ],
        );
    }

    #[test]
    fn microsoft_til_math_rounding_full_matrix() {
        verify(
            IntegralRound::Rounding,
            &[
                (1.0, Ok(1)),
                (1.9, Ok(2)),
                (-7.1, Ok(-7)),
                (-8.5, Ok(-9)),
                (f64::from(i32::MAX) + 0.1, Ok(i32::MAX)),
                (f64::from(i32::MIN) - 0.1, Ok(i32::MIN)),
                (f64::from(i32::MAX) + 1.1, Err(MathNarrowingError)),
                (f64::from(i32::MIN) - 1.1, Err(MathNarrowingError)),
                (f64::INFINITY, Err(MathNarrowingError)),
                (f64::NEG_INFINITY, Err(MathNarrowingError)),
                (f64::NAN, Err(MathNarrowingError)),
            ],
        );
    }

    #[test]
    fn microsoft_til_math_normal_integers_full_matrix() {
        for value in [1, -1, i32::MAX, i32::MIN] {
            assert_eq!(
                checked_round_i32(f64::from(value), IntegralRound::Rounding),
                Ok(value)
            );
        }
    }
}
