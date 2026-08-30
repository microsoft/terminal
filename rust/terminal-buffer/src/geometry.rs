//! Minimal TIL-compatible geometry used by the Rust buffer core.

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Point {
    pub x: i32,
    pub y: i32,
}

impl Point {
    #[must_use]
    pub const fn new(x: i32, y: i32) -> Self {
        Self { x, y }
    }
}

/// Rectangle with an exclusive right/bottom edge, matching `til::rect`.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Rect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

impl Rect {
    #[must_use]
    pub const fn new(left: i32, top: i32, right: i32, bottom: i32) -> Self {
        Self {
            left,
            top,
            right,
            bottom,
        }
    }

    #[must_use]
    pub const fn width(self) -> i32 {
        self.right.saturating_sub(self.left)
    }

    #[must_use]
    pub const fn height(self) -> i32 {
        self.bottom.saturating_sub(self.top)
    }
}

/// Rectangle with inclusive right/bottom coordinates, matching
/// `til::inclusive_rect`.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct InclusiveRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

impl InclusiveRect {
    #[must_use]
    pub const fn new(left: i32, top: i32, right: i32, bottom: i32) -> Self {
        Self {
            left,
            top,
            right,
            bottom,
        }
    }

    #[must_use]
    pub const fn width(self) -> i32 {
        self.right.saturating_sub(self.left).saturating_add(1)
    }

    #[must_use]
    pub const fn height(self) -> i32 {
        self.bottom.saturating_sub(self.top).saturating_add(1)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rect_uses_exclusive_right_and_bottom_edges() {
        let rect = Rect::new(3, 4, 13, 9);
        assert_eq!(rect.width(), 10);
        assert_eq!(rect.height(), 5);
    }

    #[test]
    fn inclusive_rect_counts_both_edges() {
        let rect = InclusiveRect::new(3, 4, 12, 8);
        assert_eq!(rect.width(), 10);
        assert_eq!(rect.height(), 5);
    }
}
