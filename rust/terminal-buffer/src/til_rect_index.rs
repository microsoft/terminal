//! Checked row-major indexing for the portable Microsoft `til::rect` owner.

use crate::til_rect::{RectError, TilPoint, TilRect};

impl TilRect {
    /// Returns the row-major cell index using the same bounded coordinate
    /// arithmetic as Microsoft's `til::rect::index_of` contract.
    pub fn checked_index_of(self, point: TilPoint) -> Result<i32, RectError> {
        if !self.contains_point(point) {
            return Err(RectError::OutOfBounds);
        }

        let width = self.width()?;
        let row = point.y.checked_sub(self.top).ok_or(RectError::Overflow)?;
        let column = point.x.checked_sub(self.left).ok_or(RectError::Overflow)?;
        row.checked_mul(width)
            .and_then(|value| value.checked_add(column))
            .ok_or(RectError::Overflow)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_rectangle_index_overflow_contract() {
        let rect = TilRect::new(5, 10, 15, 20);
        assert_eq!(rect.checked_index_of(TilPoint::new(7, 17)), Ok(72));
        assert_eq!(
            rect.checked_index_of(TilPoint::new(1, 1)),
            Err(RectError::OutOfBounds)
        );

        let max = i32::MAX;
        let huge = TilRect::new(0, 0, max, max);
        assert_eq!(
            huge.checked_index_of(TilPoint::new(max - 1, max - 1)),
            Err(RectError::Overflow)
        );
    }
}
