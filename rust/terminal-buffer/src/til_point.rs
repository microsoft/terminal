//! Portable owner for the Microsoft `til::point` contract.

use crate::til_rect::TilPoint;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PointError {
    Overflow,
    DivisionByZero,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Coord {
    pub x: i16,
    pub y: i16,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Win32Point {
    pub x: i32,
    pub y: i32,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct D2dPointF {
    pub x: f32,
    pub y: f32,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PointMath {
    Ceiling,
    Flooring,
    Rounding,
}

impl TilPoint {
    pub fn from_rounded(x: f64, y: f64) -> Result<Self, PointError> {
        Self::from_float(PointMath::Rounding, x, y)
    }

    #[must_use]
    pub const fn from_coord(coord: Coord) -> Self {
        Self::new(coord.x as i32, coord.y as i32)
    }

    #[must_use]
    pub const fn from_win32_point(point: Win32Point) -> Self {
        Self::new(point.x, point.y)
    }

    #[must_use]
    pub const fn is_valid(self) -> bool {
        self.x >= 0 && self.y >= 0
    }

    #[must_use]
    pub const fn component_le(self, other: Self) -> bool {
        self.x <= other.x && self.y <= other.y
    }

    #[must_use]
    pub const fn component_ge(self, other: Self) -> bool {
        self.x >= other.x && self.y >= other.y
    }

    pub fn checked_add(self, other: Self) -> Result<Self, PointError> {
        Ok(Self::new(
            self.x.checked_add(other.x).ok_or(PointError::Overflow)?,
            self.y.checked_add(other.y).ok_or(PointError::Overflow)?,
        ))
    }

    pub fn checked_sub(self, other: Self) -> Result<Self, PointError> {
        Ok(Self::new(
            self.x.checked_sub(other.x).ok_or(PointError::Overflow)?,
            self.y.checked_sub(other.y).ok_or(PointError::Overflow)?,
        ))
    }

    pub fn checked_mul(self, other: Self) -> Result<Self, PointError> {
        Ok(Self::new(
            self.x.checked_mul(other.x).ok_or(PointError::Overflow)?,
            self.y.checked_mul(other.y).ok_or(PointError::Overflow)?,
        ))
    }

    pub fn checked_div(self, other: Self) -> Result<Self, PointError> {
        if other.x == 0 || other.y == 0 {
            return Err(PointError::DivisionByZero);
        }
        Ok(Self::new(
            self.x.checked_div(other.x).ok_or(PointError::Overflow)?,
            self.y.checked_div(other.y).ok_or(PointError::Overflow)?,
        ))
    }

    pub fn checked_scale_mul(self, scale: i32) -> Result<Self, PointError> {
        Ok(Self::new(
            self.x.checked_mul(scale).ok_or(PointError::Overflow)?,
            self.y.checked_mul(scale).ok_or(PointError::Overflow)?,
        ))
    }

    pub fn checked_scale_div(self, scale: i32) -> Result<Self, PointError> {
        if scale == 0 {
            return Err(PointError::DivisionByZero);
        }
        Ok(Self::new(
            self.x.checked_div(scale).ok_or(PointError::Overflow)?,
            self.y.checked_div(scale).ok_or(PointError::Overflow)?,
        ))
    }

    pub fn narrow_x_i16(self) -> Result<i16, PointError> {
        i16::try_from(self.x).map_err(|_| PointError::Overflow)
    }

    pub fn narrow_y_i16(self) -> Result<i16, PointError> {
        i16::try_from(self.y).map_err(|_| PointError::Overflow)
    }

    #[must_use]
    pub const fn to_win32_point(self) -> Win32Point {
        Win32Point {
            x: self.x,
            y: self.y,
        }
    }

    #[must_use]
    pub fn to_d2d_point(self) -> D2dPointF {
        D2dPointF {
            x: self.x as f32,
            y: self.y as f32,
        }
    }

    pub fn from_float(math: PointMath, x: f64, y: f64) -> Result<Self, PointError> {
        fn convert(math: PointMath, value: f64) -> Result<i32, PointError> {
            let value = match math {
                PointMath::Ceiling => value.ceil(),
                PointMath::Flooring => value.floor(),
                PointMath::Rounding => value.round(),
            };
            if value < f64::from(i32::MIN) || value > f64::from(i32::MAX) {
                Err(PointError::Overflow)
            } else {
                Ok(value as i32)
            }
        }
        Ok(Self::new(convert(math, x)?, convert(math, y)?))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_point_construction_value_contract() {
        assert_eq!(TilPoint::default(), TilPoint::new(0, 0));
        assert_eq!(TilPoint::new(5, 10), TilPoint { x: 5, y: 10 });
        assert_eq!(TilPoint::new(-5, -10), TilPoint { x: -5, y: -10 });
        assert_eq!(TilPoint::from_rounded(3.2, 7.6), Ok(TilPoint::new(3, 8)));
        assert_eq!(
            TilPoint::from_coord(Coord { x: -5, y: 10 }),
            TilPoint::new(-5, 10)
        );
        assert_eq!(
            TilPoint::from_win32_point(Win32Point { x: 5, y: -10 }),
            TilPoint::new(5, -10)
        );
        assert_eq!(TilPoint::new(5, 10), TilPoint::new(5, 10));
        assert_ne!(TilPoint::new(4, 10), TilPoint::new(5, 10));
    }

    #[test]
    fn microsoft_point_ordering_boolean_contract() {
        let equal = TilPoint::new(5, 10);
        assert!(equal.component_le(equal));
        assert!(equal.component_ge(equal));
        assert!(TilPoint::new(4, 10).component_le(equal));
        assert!(TilPoint::new(5, 9).component_le(equal));
        assert!(!TilPoint::new(4, 10).component_ge(equal));
        assert!(!TilPoint::new(5, 9).component_ge(equal));
        for x in [i32::MIN, -1, 0, 1, i32::MAX] {
            for y in [i32::MIN, -1, 0, 1, i32::MAX] {
                assert_eq!(TilPoint::new(x, y).is_valid(), x >= 0 && y >= 0);
            }
        }
    }

    #[test]
    fn microsoft_point_add_subtract_contract() {
        let one = TilPoint::new(5, 10);
        let two = TilPoint::new(23, 47);
        assert_eq!(one.checked_add(two), Ok(TilPoint::new(28, 57)));
        assert_eq!(one.checked_sub(two), Ok(TilPoint::new(-18, -37)));
        assert_eq!(
            TilPoint::new(i32::MAX, 0).checked_add(TilPoint::new(1, 1)),
            Err(PointError::Overflow)
        );
        assert_eq!(
            TilPoint::new(-2, -2).checked_sub(TilPoint::new(i32::MAX, 0)),
            Err(PointError::Overflow)
        );
    }

    #[test]
    fn microsoft_point_multiply_divide_scale_contract() {
        let one = TilPoint::new(5, 10);
        let two = TilPoint::new(23, 47);
        assert_eq!(one.checked_mul(two), Ok(TilPoint::new(115, 470)));
        assert_eq!(
            TilPoint::new(555, 510).checked_div(two),
            Ok(TilPoint::new(24, 10))
        );
        assert_eq!(one.checked_scale_mul(23), Ok(TilPoint::new(115, 230)));
        assert_eq!(
            TilPoint::new(555, 510).checked_scale_div(23),
            Ok(TilPoint::new(24, 22))
        );
        assert_eq!(
            TilPoint::new(i32::MAX, 0).checked_mul(TilPoint::new(10, 10)),
            Err(PointError::Overflow)
        );
        assert_eq!(
            TilPoint::new(1, 1).checked_div(TilPoint::new(i32::MAX, 0)),
            Err(PointError::DivisionByZero)
        );
        assert_eq!(
            TilPoint::new(1, 1).checked_scale_div(0),
            Err(PointError::DivisionByZero)
        );
    }

    #[test]
    fn microsoft_point_narrow_and_projection_contract() {
        let point = TilPoint::new(5, 10);
        assert_eq!(point.narrow_x_i16(), Ok(5));
        assert_eq!(point.narrow_y_i16(), Ok(10));
        assert_eq!(point.to_win32_point(), Win32Point { x: 5, y: 10 });
        assert_eq!(point.to_d2d_point(), D2dPointF { x: 5.0, y: 10.0 });
    }

    #[test]
    fn microsoft_point_float_math_contract() {
        assert_eq!(
            TilPoint::from_float(PointMath::Ceiling, 1.6, 2.4),
            Ok(TilPoint::new(2, 3))
        );
        assert_eq!(
            TilPoint::from_float(PointMath::Flooring, 1.6, 2.4),
            Ok(TilPoint::new(1, 2))
        );
        assert_eq!(
            TilPoint::from_float(PointMath::Rounding, 1.6, 2.4),
            Ok(TilPoint::new(2, 2))
        );
        assert_eq!(
            TilPoint::from_float(PointMath::Rounding, 3.6, 4.4),
            Ok(TilPoint::new(4, 4))
        );
    }
}
