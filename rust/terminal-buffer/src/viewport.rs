//! Safe, platform-neutral port of the deterministic `Viewport` helper.
//!
//! The C++ type lives in `src/types/viewport.cpp` and is independent of Win32.
//! Keeping the row-major geometry here lets buffer, host, and selection code share
//! one owner without recreating native screen-buffer state.

use crate::geometry::{InclusiveRect, Point, Rect};

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct Size {
    pub width: i32,
    pub height: i32,
}

impl Size {
    #[must_use]
    pub const fn new(width: i32, height: i32) -> Self {
        Self { width, height }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct InvalidViewport;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Viewport {
    bounds: InclusiveRect,
}

impl Default for Viewport {
    fn default() -> Self {
        Self::empty()
    }
}

impl Viewport {
    #[must_use]
    pub const fn empty() -> Self {
        Self {
            bounds: InclusiveRect::new(0, 0, -1, -1),
        }
    }

    #[must_use]
    pub const fn from_inclusive(bounds: InclusiveRect) -> Self {
        Self { bounds }
    }

    #[must_use]
    pub const fn from_exclusive(bounds: Rect) -> Self {
        Self::from_inclusive(InclusiveRect::new(
            bounds.left,
            bounds.top,
            bounds.right.saturating_sub(1),
            bounds.bottom.saturating_sub(1),
        ))
    }

    #[must_use]
    pub const fn from_dimensions(origin: Point, dimensions: Size) -> Self {
        Self::from_inclusive(InclusiveRect::new(
            origin.x,
            origin.y,
            origin.x.saturating_add(dimensions.width).saturating_sub(1),
            origin.y.saturating_add(dimensions.height).saturating_sub(1),
        ))
    }

    #[must_use]
    pub const fn left(self) -> i32 {
        self.bounds.left
    }

    #[must_use]
    pub const fn right_inclusive(self) -> i32 {
        self.bounds.right
    }

    #[must_use]
    pub const fn right_exclusive(self) -> i32 {
        self.bounds.right.saturating_add(1)
    }

    #[must_use]
    pub const fn top(self) -> i32 {
        self.bounds.top
    }

    #[must_use]
    pub const fn bottom_inclusive(self) -> i32 {
        self.bounds.bottom
    }

    #[must_use]
    pub const fn bottom_exclusive(self) -> i32 {
        self.bounds.bottom.saturating_add(1)
    }

    #[must_use]
    pub const fn height(self) -> i32 {
        self.bottom_exclusive().saturating_sub(self.top())
    }

    #[must_use]
    pub const fn width(self) -> i32 {
        self.right_exclusive().saturating_sub(self.left())
    }

    #[must_use]
    pub const fn origin(self) -> Point {
        Point::new(self.left(), self.top())
    }

    #[must_use]
    pub const fn bottom_right_inclusive(self) -> Point {
        Point::new(self.right_inclusive(), self.bottom_inclusive())
    }

    #[must_use]
    pub const fn bottom_right_exclusive(self) -> Point {
        Point::new(self.right_exclusive(), self.bottom_exclusive())
    }

    #[must_use]
    pub const fn end_exclusive(self) -> Point {
        Point::new(self.left(), self.bottom_exclusive())
    }

    #[must_use]
    pub const fn dimensions(self) -> Size {
        Size::new(self.width(), self.height())
    }

    #[must_use]
    pub const fn to_inclusive(self) -> InclusiveRect {
        self.bounds
    }

    #[must_use]
    pub const fn to_exclusive(self) -> Rect {
        Rect::new(
            self.left(),
            self.top(),
            self.right_exclusive(),
            self.bottom_exclusive(),
        )
    }

    #[must_use]
    pub const fn is_valid(self) -> bool {
        self.left() <= self.right_inclusive() && self.top() <= self.bottom_inclusive()
    }

    #[must_use]
    pub const fn contains_point(self, point: Point) -> bool {
        point.x >= self.left()
            && point.x < self.right_exclusive()
            && point.y >= self.top()
            && point.y < self.bottom_exclusive()
    }

    #[must_use]
    pub const fn contains_viewport(self, other: Self) -> bool {
        other.left() >= self.left()
            && other.left() <= self.right_inclusive()
            && other.right_inclusive() >= self.left()
            && other.right_inclusive() <= self.right_inclusive()
            && other.top() >= self.top()
            && other.top() <= other.bottom_inclusive()
            && other.bottom_inclusive() >= self.top()
            && other.bottom_inclusive() <= self.bottom_inclusive()
    }

    /// Clamps a point to the nearest cell inside the viewport.
    ///
    /// # Errors
    /// Returns [`InvalidViewport`] when the viewport has no area.
    pub fn clamp_point(self, point: &mut Point) -> Result<(), InvalidViewport> {
        if !self.is_valid() {
            return Err(InvalidViewport);
        }

        point.x = point.x.clamp(self.left(), self.right_inclusive());
        point.y = point.y.clamp(self.top(), self.bottom_inclusive());
        Ok(())
    }

    #[must_use]
    pub fn clamp_viewport(self, other: Self) -> Self {
        Self::intersect(self, other)
    }

    pub fn increment_in_bounds(self, point: &mut Point) -> bool {
        self.walk_in_bounds(point, 1)
    }

    pub fn decrement_in_bounds(self, point: &mut Point) -> bool {
        self.walk_in_bounds(point, -1)
    }

    pub fn walk_in_bounds(self, point: &mut Point, delta: i32) -> bool {
        if !self.is_valid() {
            return false;
        }

        let left = i64::from(self.left());
        let top = i64::from(self.top());
        let width = i64::from(self.width());
        let height = i64::from(self.height());
        let max = width.saturating_mul(height).saturating_sub(1);
        let offset = width
            .saturating_mul(i64::from(point.y).saturating_sub(top))
            .saturating_add(i64::from(point.x).saturating_sub(left))
            .saturating_add(i64::from(delta));
        let clamped = offset.clamp(0, max);

        point.x = saturating_i32(clamped % width + left);
        point.y = saturating_i32(clamped / width + top);
        offset == clamped
    }

    #[must_use]
    pub const fn compare_in_bounds(self, first: Point, second: Point) -> i32 {
        (first.y - second.y) * self.width() + (first.x - second.x)
    }

    #[must_use]
    pub const fn offset(original: Self, delta: Point) -> Self {
        Self::from_inclusive(InclusiveRect::new(
            original.left().saturating_add(delta.x),
            original.top().saturating_add(delta.y),
            original.right_inclusive().saturating_add(delta.x),
            original.bottom_inclusive().saturating_add(delta.y),
        ))
    }

    #[must_use]
    pub const fn union(lhs: Self, rhs: Self) -> Self {
        if !lhs.is_valid() && !rhs.is_valid() {
            return Self::empty();
        }
        if !lhs.is_valid() {
            return rhs;
        }
        if !rhs.is_valid() {
            return lhs;
        }

        Self::from_inclusive(InclusiveRect::new(
            if lhs.left() < rhs.left() {
                lhs.left()
            } else {
                rhs.left()
            },
            if lhs.top() < rhs.top() {
                lhs.top()
            } else {
                rhs.top()
            },
            if lhs.right_inclusive() > rhs.right_inclusive() {
                lhs.right_inclusive()
            } else {
                rhs.right_inclusive()
            },
            if lhs.bottom_inclusive() > rhs.bottom_inclusive() {
                lhs.bottom_inclusive()
            } else {
                rhs.bottom_inclusive()
            },
        ))
    }

    #[must_use]
    pub const fn intersect(lhs: Self, rhs: Self) -> Self {
        let intersection = Self::from_inclusive(InclusiveRect::new(
            if lhs.left() > rhs.left() {
                lhs.left()
            } else {
                rhs.left()
            },
            if lhs.top() > rhs.top() {
                lhs.top()
            } else {
                rhs.top()
            },
            if lhs.right_inclusive() < rhs.right_inclusive() {
                lhs.right_inclusive()
            } else {
                rhs.right_inclusive()
            },
            if lhs.bottom_inclusive() < rhs.bottom_inclusive() {
                lhs.bottom_inclusive()
            } else {
                rhs.bottom_inclusive()
            },
        ));

        if intersection.is_valid() {
            intersection
        } else {
            Self::empty()
        }
    }

    #[must_use]
    pub fn subtract(original: Self, remove: Self) -> Vec<Self> {
        let mut result = Vec::with_capacity(4);
        let intersection = Self::intersect(original, remove);

        if !original.is_valid() {
            return result;
        }
        if !intersection.is_valid() {
            result.push(original);
            return result;
        }
        if original == intersection {
            return result;
        }

        let candidates = [
            Self::from_inclusive(InclusiveRect::new(
                original.left(),
                original.top(),
                original.right_inclusive(),
                intersection.top().saturating_sub(1),
            )),
            Self::from_inclusive(InclusiveRect::new(
                original.left(),
                intersection.bottom_exclusive(),
                original.right_inclusive(),
                original.bottom_inclusive(),
            )),
            Self::from_inclusive(InclusiveRect::new(
                original.left(),
                intersection.top(),
                intersection.left().saturating_sub(1),
                intersection.bottom_inclusive(),
            )),
            Self::from_inclusive(InclusiveRect::new(
                intersection.right_exclusive(),
                intersection.top(),
                original.right_inclusive(),
                intersection.bottom_inclusive(),
            )),
        ];

        result.extend(
            candidates
                .into_iter()
                .filter(|viewport| viewport.is_valid()),
        );
        result
    }
}

fn saturating_i32(value: i64) -> i32 {
    match i32::try_from(value) {
        Ok(value) => value,
        Err(_) if value < 0 => i32::MIN,
        Err(_) => i32::MAX,
    }
}
