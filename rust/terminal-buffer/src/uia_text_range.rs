//! Platform-neutral state semantics shared by UI Automation text ranges.
//!
//! Windows COM/WRL provider plumbing remains native. This module owns only the
//! deterministic endpoint and block-range behavior implemented by
//! `UiaTextRangeBase`.

use core::cmp::Ordering;

use crate::geometry::Point;
use crate::viewport::Viewport;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TextRangeEndpoint {
    Start,
    End,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct UiaTextRangeCore {
    start: Point,
    end: Point,
    block_range: bool,
}

impl UiaTextRangeCore {
    #[must_use]
    pub fn new(start: Point, end: Point) -> Self {
        Self::with_block_range(start, end, false)
    }

    #[must_use]
    pub fn with_block_range(start: Point, end: Point, block_range: bool) -> Self {
        let (start, end) = if compare_points(start, end).is_gt() {
            (end, start)
        } else {
            (start, end)
        };

        Self {
            start,
            end,
            block_range,
        }
    }

    #[must_use]
    pub const fn start(self) -> Point {
        self.start
    }

    #[must_use]
    pub const fn end(self) -> Point {
        self.end
    }

    #[must_use]
    pub const fn block_range(self) -> bool {
        self.block_range
    }

    pub const fn set_block_range(&mut self, block_range: bool) {
        self.block_range = block_range;
    }

    #[must_use]
    pub const fn is_degenerate(self) -> bool {
        self.start.x == self.end.x && self.start.y == self.end.y
    }

    #[must_use]
    pub const fn endpoint(self, endpoint: TextRangeEndpoint) -> Point {
        match endpoint {
            TextRangeEndpoint::Start => self.start,
            TextRangeEndpoint::End => self.end,
        }
    }

    #[must_use]
    pub const fn same_range(self, other: Self) -> bool {
        self.start.x == other.start.x
            && self.start.y == other.start.y
            && self.end.x == other.end.x
            && self.end.y == other.end.y
    }

    /// Sets one endpoint and collapses the range if the new endpoint crosses
    /// the other endpoint, matching `UiaTextRangeBase::SetEndpoint`.
    pub fn set_endpoint(&mut self, endpoint: TextRangeEndpoint, value: Point) -> bool {
        match endpoint {
            TextRangeEndpoint::End => {
                self.end = value;
                if compare_points(self.start, self.end).is_gt() {
                    self.start = self.end;
                }
            }
            TextRangeEndpoint::Start => {
                self.start = value;
                if compare_points(self.end, self.start).is_lt() {
                    self.end = self.start;
                }
            }
        }

        self.is_degenerate()
    }

    /// Compares endpoints in terminal row-major order.
    ///
    /// The UIA exclusive document endpoint at `(left, bottom + 1)` is accepted,
    /// matching the C++ `Viewport::CompareInBounds(..., true)` call.
    #[must_use]
    pub fn compare_endpoints(
        self,
        endpoint: TextRangeEndpoint,
        other: Self,
        other_endpoint: TextRangeEndpoint,
        bounds: Viewport,
    ) -> Option<i32> {
        let mine = self.endpoint(endpoint);
        let theirs = other.endpoint(other_endpoint);
        if !is_uia_endpoint_in_bounds(bounds, mine) || !is_uia_endpoint_in_bounds(bounds, theirs) {
            return None;
        }

        Some(bounds.compare_in_bounds(mine, theirs))
    }

    pub fn move_endpoint_by_range(
        &mut self,
        endpoint: TextRangeEndpoint,
        target: Self,
        target_endpoint: TextRangeEndpoint,
    ) -> bool {
        self.set_endpoint(endpoint, target.endpoint(target_endpoint))
    }
}

fn compare_points(lhs: Point, rhs: Point) -> Ordering {
    lhs.y.cmp(&rhs.y).then_with(|| lhs.x.cmp(&rhs.x))
}

fn is_uia_endpoint_in_bounds(bounds: Viewport, point: Point) -> bool {
    bounds.contains_point(point) || point == bounds.end_exclusive()
}
