//! Safe, platform-neutral foundation primitives shared across migrated Terminal layers.
//!
//! F01 moves deterministic TIL/value semantics into Rust without recreating
//! Win32/COM representation details that remain native boundaries.

#![forbid(unsafe_code)]

/// Coordinate type used by the platform-neutral Terminal geometry primitives.
pub type CoordType = i32;

/// Rounding policy used when converting floating-point geometry into terminal coordinates.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum SizeRound {
    Ceiling,
    Flooring,
    Rounding,
}

/// Failure modes for checked `Size` arithmetic and narrowing.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum SizeError {
    Overflow,
    DivideByZero,
    InvalidArgument,
    Narrowing,
}

/// Platform-neutral two-dimensional terminal size.
///
/// This owns the portable semantics of Microsoft's `til::size`. Win32 `COORD`/`SIZE`
/// and Direct2D conversions intentionally remain native interop boundaries.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Hash)]
pub struct Size {
    pub width: CoordType,
    pub height: CoordType,
}

impl Size {
    #[must_use]
    pub const fn new(width: CoordType, height: CoordType) -> Self {
        Self { width, height }
    }

    /// Converts floating-point dimensions using the requested rounding policy.
    ///
    /// # Errors
    ///
    /// Returns `SizeError::Narrowing` if either rounded dimension is non-finite or outside the
    /// `CoordType` range.
    pub fn from_f64(round: SizeRound, width: f64, height: f64) -> Result<Self, SizeError> {
        Ok(Self {
            width: round_coord(round, width)?,
            height: round_coord(round, height)?,
        })
    }

    #[must_use]
    pub const fn is_valid(self) -> bool {
        self.width > 0 && self.height > 0
    }

    /// Adds both dimensions using checked coordinate arithmetic.
    ///
    /// # Errors
    ///
    /// Returns `SizeError::Overflow` if either component exceeds the `CoordType` range.
    pub fn checked_add(self, other: Self) -> Result<Self, SizeError> {
        Ok(Self {
            width: self
                .width
                .checked_add(other.width)
                .ok_or(SizeError::Overflow)?,
            height: self
                .height
                .checked_add(other.height)
                .ok_or(SizeError::Overflow)?,
        })
    }

    /// Subtracts both dimensions using checked coordinate arithmetic.
    ///
    /// # Errors
    ///
    /// Returns `SizeError::Overflow` if either component exceeds the `CoordType` range.
    pub fn checked_sub(self, other: Self) -> Result<Self, SizeError> {
        Ok(Self {
            width: self
                .width
                .checked_sub(other.width)
                .ok_or(SizeError::Overflow)?,
            height: self
                .height
                .checked_sub(other.height)
                .ok_or(SizeError::Overflow)?,
        })
    }

    /// Multiplies both dimensions using checked coordinate arithmetic.
    ///
    /// # Errors
    ///
    /// Returns `SizeError::Overflow` if either component exceeds the `CoordType` range.
    pub fn checked_mul(self, other: Self) -> Result<Self, SizeError> {
        Ok(Self {
            width: self
                .width
                .checked_mul(other.width)
                .ok_or(SizeError::Overflow)?,
            height: self
                .height
                .checked_mul(other.height)
                .ok_or(SizeError::Overflow)?,
        })
    }

    /// Divides both dimensions using checked integer arithmetic.
    ///
    /// # Errors
    ///
    /// Returns `SizeError::DivideByZero` if either divisor is zero, or `SizeError::Overflow` for
    /// the signed minimum divided by negative one case.
    pub fn checked_div(self, other: Self) -> Result<Self, SizeError> {
        if other.width == 0 || other.height == 0 {
            return Err(SizeError::DivideByZero);
        }

        Ok(Self {
            width: self
                .width
                .checked_div(other.width)
                .ok_or(SizeError::Overflow)?,
            height: self
                .height
                .checked_div(other.height)
                .ok_or(SizeError::Overflow)?,
        })
    }

    /// Scales both dimensions and rounds each result toward positive infinity.
    ///
    /// # Errors
    ///
    /// Returns `SizeError::Narrowing` when a scaled dimension is non-finite or outside the
    /// `CoordType` range.
    pub fn scale_ceil(self, scale: f64) -> Result<Self, SizeError> {
        Self::from_f64(
            SizeRound::Ceiling,
            f64::from(self.width) * scale,
            f64::from(self.height) * scale,
        )
    }

    /// Divides positive dimensions and rounds each quotient upward.
    ///
    /// # Errors
    ///
    /// Returns `SizeError::InvalidArgument` when the dividend has a negative dimension or the
    /// divisor has a non-positive dimension.
    pub fn divide_ceil(self, other: Self) -> Result<Self, SizeError> {
        if self.width < 0 || self.height < 0 || other.width <= 0 || other.height <= 0 {
            return Err(SizeError::InvalidArgument);
        }

        Ok(Self {
            width: if self.width == 0 {
                0
            } else {
                (self.width - 1) / other.width + 1
            },
            height: if self.height == 0 {
                0
            } else {
                (self.height - 1) / other.height + 1
            },
        })
    }

    /// Narrows the width to the Win32-compatible signed 16-bit coordinate width.
    ///
    /// # Errors
    ///
    /// Returns `SizeError::Narrowing` if the width does not fit in `i16`.
    pub fn narrow_width_i16(self) -> Result<i16, SizeError> {
        i16::try_from(self.width).map_err(|_| SizeError::Narrowing)
    }

    /// Narrows the height to the Win32-compatible signed 16-bit coordinate width.
    ///
    /// # Errors
    ///
    /// Returns `SizeError::Narrowing` if the height does not fit in `i16`.
    pub fn narrow_height_i16(self) -> Result<i16, SizeError> {
        i16::try_from(self.height).map_err(|_| SizeError::Narrowing)
    }

    /// Computes the area and narrows it back to `CoordType`.
    ///
    /// # Errors
    ///
    /// Returns `SizeError::Narrowing` if the product does not fit in `CoordType`.
    pub fn area(self) -> Result<CoordType, SizeError> {
        let area = i64::from(self.width) * i64::from(self.height);
        CoordType::try_from(area).map_err(|_| SizeError::Narrowing)
    }

    /// Computes the area and narrows it to a signed 16-bit value.
    ///
    /// # Errors
    ///
    /// Returns `SizeError::Narrowing` if the product does not fit in `i16`.
    pub fn area_i16(self) -> Result<i16, SizeError> {
        let area = i64::from(self.width) * i64::from(self.height);
        i16::try_from(area).map_err(|_| SizeError::Narrowing)
    }
}

#[allow(clippy::cast_possible_truncation)]
fn round_coord(round: SizeRound, value: f64) -> Result<CoordType, SizeError> {
    if !value.is_finite() {
        return Err(SizeError::Narrowing);
    }

    let rounded = match round {
        SizeRound::Ceiling => value.ceil(),
        SizeRound::Flooring => value.floor(),
        SizeRound::Rounding => value.round(),
    };

    if rounded < f64::from(CoordType::MIN) || rounded > f64::from(CoordType::MAX) {
        return Err(SizeError::Narrowing);
    }

    Ok(rounded as CoordType)
}

/// A monotonically increasing state generation.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd, Hash)]
pub struct Generation(u32);

impl Generation {
    #[must_use]
    pub const fn value(self) -> u32 {
        self.0
    }

    fn bump(&mut self) {
        self.0 = self.0.wrapping_add(1);
    }
}

/// Value wrapper whose equality intentionally tracks mutation generation, not `T` equality.
///
/// This mirrors `til::generational`: reads are cheap, while every mutable access bumps the
/// generation so downstream caches can cheaply detect any state change.
#[derive(Clone, Debug)]
pub struct Generational<T> {
    generation: Generation,
    value: T,
}

impl<T: Default> Default for Generational<T> {
    fn default() -> Self {
        Self {
            generation: Generation::default(),
            value: T::default(),
        }
    }
}

impl<T> Generational<T> {
    #[must_use]
    pub const fn new(value: T) -> Self {
        Self {
            generation: Generation(0),
            value,
        }
    }

    #[must_use]
    pub const fn with_generation(generation: Generation, value: T) -> Self {
        Self { generation, value }
    }

    #[must_use]
    pub const fn generation(&self) -> Generation {
        self.generation
    }

    #[must_use]
    pub const fn get(&self) -> &T {
        &self.value
    }

    /// Marks the wrapped state as changed before returning mutable access.
    pub fn write(&mut self) -> &mut T {
        self.generation.bump();
        &mut self.value
    }
}

impl<T> PartialEq for Generational<T> {
    fn eq(&self, other: &Self) -> bool {
        self.generation == other.generation
    }
}

impl<T> Eq for Generational<T> {}

#[cfg(test)]
mod tests {
    use super::{CoordType, Generation, Generational, Size, SizeError, SizeRound};

    #[derive(Clone, Debug, Default)]
    struct Data {
        value: i32,
    }

    #[test]
    fn microsoft_til_size_default_construct() {
        assert_eq!(Size::new(0, 0), Size::default());
    }

    #[test]
    fn microsoft_til_size_raw_construct() {
        let size = Size::new(5, 10);
        assert_eq!(5, size.width);
        assert_eq!(10, size.height);
    }

    #[test]
    fn microsoft_til_size_raw_floating_construct() {
        assert_eq!(
            Size::new(3, 8),
            Size::from_f64(SizeRound::Rounding, 3.2, 7.8).unwrap()
        );
    }

    #[test]
    fn microsoft_til_size_signed_construct() {
        assert_eq!(Size::new(-5, -10), Size::new(-5, -10));
    }

    #[test]
    fn microsoft_til_size_equality() {
        assert_eq!(Size::new(5, 10), Size::new(5, 10));
        assert_ne!(Size::new(4, 10), Size::new(5, 10));
        assert_ne!(Size::new(5, 10), Size::new(6, 10));
        assert_ne!(Size::new(5, 9), Size::new(5, 10));
        assert_ne!(Size::new(5, 10), Size::new(5, 11));
    }

    #[test]
    fn microsoft_til_size_inequality() {
        assert!(!(Size::new(5, 10) != Size::new(5, 10)));
        assert_ne!(Size::new(4, 10), Size::new(5, 10));
        assert_ne!(Size::new(5, 10), Size::new(6, 10));
        assert_ne!(Size::new(5, 9), Size::new(5, 10));
        assert_ne!(Size::new(5, 10), Size::new(5, 11));
    }

    #[test]
    fn microsoft_til_size_boolean() {
        let values = [CoordType::MIN, -1, 0, 1, CoordType::MAX];
        for width in values {
            for height in values {
                assert_eq!(width > 0 && height > 0, Size::new(width, height).is_valid());
            }
        }
    }

    #[test]
    fn microsoft_til_size_addition() {
        assert_eq!(
            Ok(Size::new(28, 57)),
            Size::new(5, 10).checked_add(Size::new(23, 47))
        );
        assert_eq!(
            Err(SizeError::Overflow),
            Size::new(CoordType::MAX, 0).checked_add(Size::new(1, 1))
        );
        assert_eq!(
            Err(SizeError::Overflow),
            Size::new(0, CoordType::MAX).checked_add(Size::new(1, 1))
        );
    }

    #[test]
    fn microsoft_til_size_subtraction() {
        assert_eq!(
            Ok(Size::new(-18, -37)),
            Size::new(5, 10).checked_sub(Size::new(23, 47))
        );
        assert_eq!(
            Err(SizeError::Overflow),
            Size::new(-2, -2).checked_sub(Size::new(CoordType::MAX, 0))
        );
        assert_eq!(
            Err(SizeError::Overflow),
            Size::new(-2, -2).checked_sub(Size::new(0, CoordType::MAX))
        );
    }

    #[test]
    fn microsoft_til_size_multiplication() {
        assert_eq!(
            Ok(Size::new(115, 470)),
            Size::new(5, 10).checked_mul(Size::new(23, 47))
        );
        assert_eq!(
            Err(SizeError::Overflow),
            Size::new(CoordType::MAX, 0).checked_mul(Size::new(10, 10))
        );
        assert_eq!(
            Err(SizeError::Overflow),
            Size::new(0, CoordType::MAX).checked_mul(Size::new(10, 10))
        );
    }

    #[test]
    fn microsoft_til_size_scale_by_float() {
        assert_eq!(Ok(Size::new(9, 18)), Size::new(5, 10).scale_ceil(1.783));
        assert_eq!(Err(SizeError::Narrowing), Size::new(5, 10).scale_ceil(1e12));
    }

    #[test]
    fn microsoft_til_size_division() {
        assert_eq!(
            Ok(Size::new(24, 10)),
            Size::new(555, 510).checked_div(Size::new(23, 47))
        );
        assert_eq!(
            Err(SizeError::DivideByZero),
            Size::new(1, 1).checked_div(Size::new(CoordType::MAX, 0))
        );
    }

    #[test]
    fn microsoft_til_size_division_rounding_up() {
        assert_eq!(
            Ok(Size::new(4, 3)),
            Size::new(10, 5).divide_ceil(Size::new(3, 2))
        );
        assert_eq!(
            Err(SizeError::InvalidArgument),
            Size::new(-10, -5).divide_ceil(Size::new(3, 2))
        );
    }

    #[test]
    fn microsoft_til_size_width_cast() {
        assert_eq!(Ok(5_i16), Size::new(5, 10).narrow_width_i16());
    }

    #[test]
    fn microsoft_til_size_height_cast() {
        assert_eq!(Ok(10_i16), Size::new(5, 10).narrow_height_i16());
    }

    #[test]
    fn microsoft_til_size_area() {
        assert_eq!(Ok(50), Size::new(5, 10).area());
        assert_eq!(
            Err(SizeError::Narrowing),
            Size::new(CoordType::MAX, CoordType::MAX).area()
        );
    }

    #[test]
    fn microsoft_til_size_area_cast() {
        assert_eq!(Ok(50_i16), Size::new(5, 10).area_i16());
        let max_short = CoordType::from(i16::MAX);
        assert_eq!(
            Err(SizeError::Narrowing),
            Size::new(max_short, max_short).area_i16()
        );
    }

    #[test]
    fn microsoft_til_size_cast_from_float_with_math_types() {
        for (width, height, expected) in [
            (1.0, 2.0, Size::new(1, 2)),
            (1.6, 2.4, Size::new(2, 3)),
            (3.0, 4.0, Size::new(3, 4)),
            (3.6, 4.4, Size::new(4, 5)),
            (5.0, 6.0, Size::new(5, 6)),
            (5.6, 6.4, Size::new(6, 7)),
        ] {
            assert_eq!(
                expected,
                Size::from_f64(SizeRound::Ceiling, width, height).unwrap()
            );
        }

        for (width, height, expected) in [
            (1.0, 2.0, Size::new(1, 2)),
            (1.6, 2.4, Size::new(1, 2)),
            (3.0, 4.0, Size::new(3, 4)),
            (3.6, 4.4, Size::new(3, 4)),
            (5.0, 6.0, Size::new(5, 6)),
            (5.6, 6.4, Size::new(5, 6)),
        ] {
            assert_eq!(
                expected,
                Size::from_f64(SizeRound::Flooring, width, height).unwrap()
            );
        }

        for (width, height, expected) in [
            (1.0, 2.0, Size::new(1, 2)),
            (1.6, 2.4, Size::new(2, 2)),
            (3.0, 4.0, Size::new(3, 4)),
            (3.6, 4.4, Size::new(4, 4)),
            (5.0, 6.0, Size::new(5, 6)),
            (5.6, 6.4, Size::new(6, 6)),
        ] {
            assert_eq!(
                expected,
                Size::from_f64(SizeRound::Rounding, width, height).unwrap()
            );
        }
    }

    #[test]
    fn microsoft_til_generational_basic_matches_source_contract() {
        let mut src = Generational::<Data>::default();
        let mut dst = Generational::<Data>::default();

        assert_eq!(0, src.get().value);

        src.write().value = 123;
        assert_ne!(dst, src);

        dst = src.clone();
        assert_eq!(dst, src);
        assert_eq!(123, dst.get().value);
    }

    #[test]
    fn generational_equality_is_generation_based_not_value_based() {
        let mut left = Generational::new(10_u32);
        let mut right = Generational::new(99_u32);
        assert_eq!(left, right);

        *left.write() = 99;
        assert_ne!(left, right);
        right.write();
        assert_eq!(left, right);
    }

    #[test]
    fn explicit_generation_is_preserved_and_wraps_like_uint32() {
        let max = Generation(u32::MAX);
        let mut value = Generational::with_generation(max, 1_u8);
        value.write();
        assert_eq!(0, value.generation().value());
    }
}
