//! VT line-rendition geometry compatible with `LineRendition.hpp`.

use crate::geometry::{InclusiveRect, Point, Rect};

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum LineRendition {
    #[default]
    SingleWidth,
    DoubleWidth,
    DoubleHeightTop,
    DoubleHeightBottom,
}

impl LineRendition {
    #[must_use]
    pub const fn is_double_width(self) -> bool {
        !matches!(self, Self::SingleWidth)
    }

    const fn scale_shift(self) -> u32 {
        if self.is_double_width() { 1 } else { 0 }
    }
}

#[must_use]
pub const fn screen_to_buffer_line(line: InclusiveRect, rendition: LineRendition) -> InclusiveRect {
    let shift = rendition.scale_shift();
    InclusiveRect::new(
        line.left >> shift,
        line.top,
        line.right >> shift,
        line.bottom,
    )
}

#[must_use]
pub const fn screen_to_buffer_line_inclusive(point: Point, rendition: LineRendition) -> Point {
    Point::new(point.x >> rendition.scale_shift(), point.y)
}

#[must_use]
pub const fn buffer_to_screen_line(line: Rect, rendition: LineRendition) -> Rect {
    let shift = rendition.scale_shift();
    Rect::new(
        line.left << shift,
        line.top,
        line.right << shift,
        line.bottom,
    )
}

#[must_use]
pub const fn buffer_to_screen_line_inclusive(
    line: InclusiveRect,
    rendition: LineRendition,
) -> InclusiveRect {
    let shift = rendition.scale_shift();
    let scale = if shift == 0 { 0 } else { 1 };
    InclusiveRect::new(
        line.left << shift,
        line.top,
        (line.right << shift).saturating_add(scale),
        line.bottom,
    )
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn single_width_geometry_is_unchanged() {
        let inclusive = InclusiveRect::new(2, 3, 9, 3);
        assert_eq!(
            screen_to_buffer_line(inclusive, LineRendition::SingleWidth),
            inclusive
        );
        assert_eq!(
            buffer_to_screen_line_inclusive(inclusive, LineRendition::SingleWidth),
            inclusive
        );
    }

    #[test]
    fn double_width_screen_coordinates_compress_by_two() {
        let screen = InclusiveRect::new(4, 7, 11, 7);
        assert_eq!(
            screen_to_buffer_line(screen, LineRendition::DoubleWidth),
            InclusiveRect::new(2, 7, 5, 7)
        );
        assert_eq!(
            screen_to_buffer_line_inclusive(Point::new(11, 7), LineRendition::DoubleHeightTop),
            Point::new(5, 7)
        );
    }

    #[test]
    fn inclusive_buffer_coordinate_expands_trailing_cell() {
        let buffer = InclusiveRect::new(2, 7, 5, 7);
        assert_eq!(
            buffer_to_screen_line_inclusive(buffer, LineRendition::DoubleHeightBottom),
            InclusiveRect::new(4, 7, 11, 7)
        );
    }

    #[test]
    fn exclusive_buffer_rect_does_not_add_trailing_cell() {
        let buffer = Rect::new(2, 7, 6, 8);
        assert_eq!(
            buffer_to_screen_line(buffer, LineRendition::DoubleWidth),
            Rect::new(4, 7, 12, 8)
        );
    }
}
