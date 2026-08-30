//! Portable owner for Microsoft TIL point/size cross-operator semantics.

use std::ops::{Add, Div, Mul, Sub};

use crate::til_rect::TilPoint;

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct TilSize {
    pub width: i32,
    pub height: i32,
}

impl TilSize {
    #[must_use]
    pub const fn new(width: i32, height: i32) -> Self {
        Self { width, height }
    }
}

impl Add<TilSize> for TilPoint {
    type Output = Self;

    fn add(self, rhs: TilSize) -> Self::Output {
        Self::new(self.x + rhs.width, self.y + rhs.height)
    }
}

impl Sub<TilSize> for TilPoint {
    type Output = Self;

    fn sub(self, rhs: TilSize) -> Self::Output {
        Self::new(self.x - rhs.width, self.y - rhs.height)
    }
}

impl Mul<TilSize> for TilPoint {
    type Output = Self;

    fn mul(self, rhs: TilSize) -> Self::Output {
        Self::new(self.x * rhs.width, self.y * rhs.height)
    }
}

impl Div<TilSize> for TilPoint {
    type Output = Self;

    fn div(self, rhs: TilSize) -> Self::Output {
        Self::new(self.x / rhs.width, self.y / rhs.height)
    }
}

impl Add<TilPoint> for TilSize {
    type Output = Self;

    fn add(self, rhs: TilPoint) -> Self::Output {
        Self::new(self.width + rhs.x, self.height + rhs.y)
    }
}

impl Sub<TilPoint> for TilSize {
    type Output = Self;

    fn sub(self, rhs: TilPoint) -> Self::Output {
        Self::new(self.width - rhs.x, self.height - rhs.y)
    }
}

impl Mul<TilPoint> for TilSize {
    type Output = Self;

    fn mul(self, rhs: TilPoint) -> Self::Output {
        Self::new(self.width * rhs.x, self.height * rhs.y)
    }
}

impl Div<TilPoint> for TilSize {
    type Output = Self;

    fn div(self, rhs: TilPoint) -> Self::Output {
        Self::new(self.width / rhs.x, self.height / rhs.y)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_til_point_size_operator_contract() {
        let point = TilPoint::new(5, 10);
        let size = TilSize::new(2, 4);

        assert_eq!(point + size, TilPoint::new(7, 14));
        assert_eq!(point - size, TilPoint::new(3, 6));
        assert_eq!(point * size, TilPoint::new(10, 40));
        assert_eq!(point / size, TilPoint::new(2, 2));
    }

    #[test]
    fn microsoft_til_size_point_operator_contract() {
        let size = TilSize::new(5, 10);
        let point = TilPoint::new(2, 4);

        assert_eq!(size + point, TilSize::new(7, 14));
        assert_eq!(size - point, TilSize::new(3, 6));
        assert_eq!(size * point, TilSize::new(10, 40));
        assert_eq!(size / point, TilSize::new(2, 2));
    }
}
