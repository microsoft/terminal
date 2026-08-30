//! Portable owner for the Microsoft `til::rect` contract.

use core::cmp::Ordering;

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct TilPoint {
    pub x: i32,
    pub y: i32,
}
impl TilPoint {
    #[must_use]
    pub const fn new(x: i32, y: i32) -> Self {
        Self { x, y }
    }
}

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

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct TilRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RectError {
    Overflow,
    OutOfBounds,
    InvalidScale,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct SmallRect {
    pub left: i16,
    pub top: i16,
    pub right: i16,
    pub bottom: i16,
}
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Win32Rect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct D2dRectF {
    pub left: f32,
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
}
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct WinrtRect {
    pub x: f32,
    pub y: f32,
    pub width: f32,
    pub height: f32,
}
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FloatMath {
    Ceiling,
    Flooring,
    Rounding,
}

impl TilRect {
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
    pub const fn from_points(a: TilPoint, b: TilPoint) -> Self {
        Self::new(a.x, a.y, b.x, b.y)
    }
    #[must_use]
    pub const fn from_size(size: TilSize) -> Self {
        Self::new(0, 0, size.width, size.height)
    }
    pub fn from_point_and_size(origin: TilPoint, size: TilSize) -> Result<Self, RectError> {
        Ok(Self::new(
            origin.x,
            origin.y,
            origin
                .x
                .checked_add(size.width)
                .ok_or(RectError::Overflow)?,
            origin
                .y
                .checked_add(size.height)
                .ok_or(RectError::Overflow)?,
        ))
    }
    pub fn from_small_rect(r: SmallRect) -> Result<Self, RectError> {
        Ok(Self::new(
            i32::from(r.left),
            i32::from(r.top),
            i32::from(r.right)
                .checked_add(1)
                .ok_or(RectError::Overflow)?,
            i32::from(r.bottom)
                .checked_add(1)
                .ok_or(RectError::Overflow)?,
        ))
    }
    #[must_use]
    pub const fn from_win32_rect(r: Win32Rect) -> Self {
        Self::new(r.left, r.top, r.right, r.bottom)
    }
    #[must_use]
    pub const fn is_valid(self) -> bool {
        self.left >= 0 && self.top >= 0 && self.right > self.left && self.bottom > self.top
    }
    #[must_use]
    pub fn union(self, r: Self) -> Self {
        Self::new(
            self.left.min(r.left),
            self.top.min(r.top),
            self.right.max(r.right),
            self.bottom.max(r.bottom),
        )
    }
    pub fn union_in_place(&mut self, r: Self) {
        *self = self.union(r);
    }
    #[must_use]
    pub fn intersection(self, r: Self) -> Self {
        Self::new(
            self.left.max(r.left),
            self.top.max(r.top),
            self.right.min(r.right),
            self.bottom.min(r.bottom),
        )
    }
    pub fn intersection_in_place(&mut self, r: Self) {
        *self = self.intersection(r);
    }
    #[must_use]
    pub fn subtract(self, removal: Self) -> Vec<Self> {
        let overlap = self.intersection(removal);
        if overlap.right <= overlap.left || overlap.bottom <= overlap.top {
            return vec![self];
        }
        if overlap == self {
            return Vec::new();
        }
        let mut out = Vec::with_capacity(4);
        if self.top < overlap.top {
            out.push(Self::new(self.left, self.top, self.right, overlap.top));
        }
        if overlap.bottom < self.bottom {
            out.push(Self::new(
                self.left,
                overlap.bottom,
                self.right,
                self.bottom,
            ));
        }
        if self.left < overlap.left {
            out.push(Self::new(
                self.left,
                overlap.top,
                overlap.left,
                overlap.bottom,
            ));
        }
        if overlap.right < self.right {
            out.push(Self::new(
                overlap.right,
                overlap.top,
                self.right,
                overlap.bottom,
            ));
        }
        out
    }
    pub fn translate(self, p: TilPoint) -> Result<Self, RectError> {
        Ok(Self::new(
            self.left.checked_add(p.x).ok_or(RectError::Overflow)?,
            self.top.checked_add(p.y).ok_or(RectError::Overflow)?,
            self.right.checked_add(p.x).ok_or(RectError::Overflow)?,
            self.bottom.checked_add(p.y).ok_or(RectError::Overflow)?,
        ))
    }
    pub fn translate_in_place(&mut self, p: TilPoint) -> Result<(), RectError> {
        *self = self.translate(p)?;
        Ok(())
    }
    pub fn translate_negative(self, p: TilPoint) -> Result<Self, RectError> {
        Ok(Self::new(
            self.left.checked_sub(p.x).ok_or(RectError::Overflow)?,
            self.top.checked_sub(p.y).ok_or(RectError::Overflow)?,
            self.right.checked_sub(p.x).ok_or(RectError::Overflow)?,
            self.bottom.checked_sub(p.y).ok_or(RectError::Overflow)?,
        ))
    }
    pub fn translate_negative_in_place(&mut self, p: TilPoint) -> Result<(), RectError> {
        *self = self.translate_negative(p)?;
        Ok(())
    }
    pub fn scale_up(self, s: TilSize) -> Result<Self, RectError> {
        Ok(Self::new(
            self.left.checked_mul(s.width).ok_or(RectError::Overflow)?,
            self.top.checked_mul(s.height).ok_or(RectError::Overflow)?,
            self.right.checked_mul(s.width).ok_or(RectError::Overflow)?,
            self.bottom
                .checked_mul(s.height)
                .ok_or(RectError::Overflow)?,
        ))
    }
    pub fn scale_down(self, s: TilSize) -> Result<Self, RectError> {
        if s.width <= 0 || s.height <= 0 {
            return Err(RectError::InvalidScale);
        }
        Ok(Self::new(
            floor_div(self.left, s.width),
            floor_div(self.top, s.height),
            ceil_div(self.right, s.width),
            ceil_div(self.bottom, s.height),
        ))
    }
    pub fn width(self) -> Result<i32, RectError> {
        self.right.checked_sub(self.left).ok_or(RectError::Overflow)
    }
    pub fn height(self) -> Result<i32, RectError> {
        self.bottom.checked_sub(self.top).ok_or(RectError::Overflow)
    }
    pub fn narrow_left_i16(self) -> Result<i16, RectError> {
        i16::try_from(self.left).map_err(|_| RectError::Overflow)
    }
    pub fn narrow_top_i16(self) -> Result<i16, RectError> {
        i16::try_from(self.top).map_err(|_| RectError::Overflow)
    }
    pub fn narrow_right_i16(self) -> Result<i16, RectError> {
        i16::try_from(self.right).map_err(|_| RectError::Overflow)
    }
    pub fn narrow_bottom_i16(self) -> Result<i16, RectError> {
        i16::try_from(self.bottom).map_err(|_| RectError::Overflow)
    }
    pub fn narrow_width_i16(self) -> Result<i16, RectError> {
        i16::try_from(self.width()?).map_err(|_| RectError::Overflow)
    }
    pub fn narrow_height_i16(self) -> Result<i16, RectError> {
        i16::try_from(self.height()?).map_err(|_| RectError::Overflow)
    }
    #[must_use]
    pub const fn origin(self) -> TilPoint {
        TilPoint::new(self.left, self.top)
    }
    pub fn size(self) -> Result<TilSize, RectError> {
        Ok(TilSize::new(self.width()?, self.height()?))
    }
    #[must_use]
    pub const fn contains_point(self, p: TilPoint) -> bool {
        p.x >= self.left && p.x < self.right && p.y >= self.top && p.y < self.bottom
    }
    #[must_use]
    pub const fn contains_rect(self, r: Self) -> bool {
        r.left >= self.left && r.top >= self.top && r.right <= self.right && r.bottom <= self.bottom
    }
    pub fn index_of(self, p: TilPoint) -> Result<usize, RectError> {
        if !self.contains_point(p) {
            return Err(RectError::OutOfBounds);
        }
        let width = i64::from(self.width()?);
        let row = i64::from(p.y - self.top);
        let col = i64::from(p.x - self.left);
        usize::try_from(
            row.checked_mul(width)
                .and_then(|v| v.checked_add(col))
                .ok_or(RectError::Overflow)?,
        )
        .map_err(|_| RectError::Overflow)
    }
    pub fn point_at(self, index: usize) -> Result<TilPoint, RectError> {
        let width = usize::try_from(self.width()?).map_err(|_| RectError::Overflow)?;
        let height = usize::try_from(self.height()?).map_err(|_| RectError::Overflow)?;
        let area = width.checked_mul(height).ok_or(RectError::Overflow)?;
        if width == 0 || index >= area {
            return Err(RectError::OutOfBounds);
        }
        Ok(TilPoint::new(
            self.left
                .checked_add(i32::try_from(index % width).map_err(|_| RectError::Overflow)?)
                .ok_or(RectError::Overflow)?,
            self.top
                .checked_add(i32::try_from(index / width).map_err(|_| RectError::Overflow)?)
                .ok_or(RectError::Overflow)?,
        ))
    }
    pub fn to_small_rect(self) -> Result<SmallRect, RectError> {
        Ok(SmallRect {
            left: i16::try_from(self.left).map_err(|_| RectError::Overflow)?,
            top: i16::try_from(self.top).map_err(|_| RectError::Overflow)?,
            right: i16::try_from(self.right.checked_sub(1).ok_or(RectError::Overflow)?)
                .map_err(|_| RectError::Overflow)?,
            bottom: i16::try_from(self.bottom.checked_sub(1).ok_or(RectError::Overflow)?)
                .map_err(|_| RectError::Overflow)?,
        })
    }
    #[must_use]
    pub const fn to_win32_rect(self) -> Win32Rect {
        Win32Rect {
            left: self.left,
            top: self.top,
            right: self.right,
            bottom: self.bottom,
        }
    }
    #[must_use]
    pub fn to_d2d_rect(self) -> D2dRectF {
        D2dRectF {
            left: self.left as f32,
            top: self.top as f32,
            right: self.right as f32,
            bottom: self.bottom as f32,
        }
    }
    pub fn to_winrt_rect(self) -> Result<WinrtRect, RectError> {
        Ok(WinrtRect {
            x: self.left as f32,
            y: self.top as f32,
            width: self.width()? as f32,
            height: self.height()? as f32,
        })
    }
    pub fn from_float(
        math: FloatMath,
        left: f64,
        top: f64,
        right: f64,
        bottom: f64,
    ) -> Result<Self, RectError> {
        fn cvt(math: FloatMath, v: f64) -> Result<i32, RectError> {
            let v = match math {
                FloatMath::Ceiling => v.ceil(),
                FloatMath::Flooring => v.floor(),
                FloatMath::Rounding => v.round(),
            };
            if v < f64::from(i32::MIN) || v > f64::from(i32::MAX) {
                Err(RectError::Overflow)
            } else {
                Ok(v as i32)
            }
        }
        Ok(Self::new(
            cvt(math, left)?,
            cvt(math, top)?,
            cvt(math, right)?,
            cvt(math, bottom)?,
        ))
    }
    #[must_use]
    pub fn begin(self) -> RectPointIterator {
        RectPointIterator::new(self, 0)
    }
    #[must_use]
    pub fn end(self) -> RectPointIterator {
        let width = i64::from(self.width().unwrap_or(0).max(0));
        let height = i64::from(self.height().unwrap_or(0).max(0));
        RectPointIterator::new(self, width.saturating_mul(height))
    }
}

fn floor_div(v: i32, d: i32) -> i32 {
    let q = v / d;
    let r = v % d;
    if r != 0 && ((r > 0) != (d > 0)) {
        q - 1
    } else {
        q
    }
}
fn ceil_div(v: i32, d: i32) -> i32 {
    let q = v / d;
    let r = v % d;
    if r != 0 && ((r > 0) == (d > 0)) {
        q + 1
    } else {
        q
    }
}

#[derive(Clone, Copy, Debug)]
pub struct RectPointIterator {
    rect: TilRect,
    index: i64,
}
impl RectPointIterator {
    const fn new(rect: TilRect, index: i64) -> Self {
        Self { rect, index }
    }
    #[must_use]
    pub fn point(self) -> TilPoint {
        let width = i64::from(self.rect.width().unwrap_or(0).max(1));
        TilPoint::new(
            self.rect
                .left
                .saturating_add(i32::try_from(self.index.rem_euclid(width)).unwrap_or(i32::MAX)),
            self.rect
                .top
                .saturating_add(i32::try_from(self.index.div_euclid(width)).unwrap_or(i32::MAX)),
        )
    }
    pub fn increment(&mut self) {
        self.index = self.index.saturating_add(1);
    }
}
impl PartialEq for RectPointIterator {
    fn eq(&self, other: &Self) -> bool {
        self.rect == other.rect && self.index == other.index
    }
}
impl Eq for RectPointIterator {}
impl PartialOrd for RectPointIterator {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        if self.rect == other.rect {
            self.index.partial_cmp(&other.index)
        } else {
            None
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_rectangle_construction_value_contract() {
        assert_eq!(TilRect::default(), TilRect::new(0, 0, 0, 0));
        assert_eq!(
            TilRect::from_points(TilPoint::new(5, 10), TilPoint::new(15, 20)),
            TilRect::new(5, 10, 15, 20)
        );
        assert_eq!(
            TilRect::from_size(TilSize::new(5, 10)),
            TilRect::new(0, 0, 5, 10)
        );
        assert_eq!(
            TilRect::from_point_and_size(TilPoint::new(4, 8), TilSize::new(2, 10)),
            Ok(TilRect::new(4, 8, 6, 18))
        );
        assert_eq!(
            TilRect::from_point_and_size(TilPoint::new(4, 8), TilSize::new(i32::MAX, 0)),
            Err(RectError::Overflow)
        );
        assert_eq!(
            TilRect::from_small_rect(SmallRect {
                left: 5,
                top: 10,
                right: 14,
                bottom: 19
            }),
            Ok(TilRect::new(5, 10, 15, 20))
        );
        assert_eq!(
            TilRect::from_win32_rect(Win32Rect {
                left: 5,
                top: 10,
                right: 15,
                bottom: 20
            }),
            TilRect::new(5, 10, 15, 20)
        );
        let mut assigned = TilRect::new(1, 2, 3, 4);
        assert_eq!(assigned, TilRect::new(1, 2, 3, 4));
        assigned = TilRect::new(5, 6, 7, 8);
        assert_eq!(assigned, TilRect::new(5, 6, 7, 8));
        assert_ne!(assigned, TilRect::new(5, 6, 7, 9));
    }

    #[test]
    fn microsoft_rectangle_boolean_union_intersection_contract() {
        for l in [i32::MIN, -1, 0, 1, i32::MAX] {
            for t in [i32::MIN, -1, 0, 1, i32::MAX] {
                for r in [i32::MIN, -1, 0, 1, i32::MAX] {
                    for b in [i32::MIN, -1, 0, 1, i32::MAX] {
                        assert_eq!(
                            TilRect::new(l, t, r, b).is_valid(),
                            l >= 0 && t >= 0 && r > l && b > t
                        );
                    }
                }
            }
        }
        let one = TilRect::new(4, 6, 10, 14);
        let two = TilRect::new(5, 2, 13, 10);
        assert_eq!(one.union(two), TilRect::new(4, 2, 13, 14));
        assert_eq!(one.intersection(two), TilRect::new(5, 6, 10, 10));
        let mut r = one;
        r.union_in_place(two);
        assert_eq!(r, TilRect::new(4, 2, 13, 14));
        r = one;
        r.intersection_in_place(two);
        assert_eq!(r, TilRect::new(5, 6, 10, 10));
    }

    #[test]
    fn microsoft_rectangle_subtraction_contract() {
        let o = TilRect::new(0, 0, 10, 10);
        assert_eq!(o.subtract(o), vec![]);
        assert_eq!(o.subtract(TilRect::new(12, 12, 15, 15)), vec![o]);
        assert_eq!(
            o.subtract(TilRect::new(-12, 3, 15, 15)),
            vec![TilRect::new(0, 0, 10, 3)]
        );
        assert_eq!(
            o.subtract(TilRect::new(3, 3, 15, 15)),
            vec![TilRect::new(0, 0, 10, 3), TilRect::new(0, 3, 3, 10)]
        );
        assert_eq!(
            o.subtract(TilRect::new(3, 3, 15, 6)),
            vec![
                TilRect::new(0, 0, 10, 3),
                TilRect::new(0, 6, 10, 10),
                TilRect::new(0, 3, 3, 6)
            ]
        );
        assert_eq!(
            o.subtract(TilRect::new(3, 3, 6, 6)),
            vec![
                TilRect::new(0, 0, 10, 3),
                TilRect::new(0, 6, 10, 10),
                TilRect::new(0, 3, 3, 6),
                TilRect::new(6, 3, 10, 6)
            ]
        );
    }

    #[test]
    fn microsoft_rectangle_translation_scaling_contract() {
        let s = TilRect::new(10, 20, 30, 40);
        let p = TilPoint::new(3, 7);
        assert_eq!(s.translate(p), Ok(TilRect::new(13, 27, 33, 47)));
        assert_eq!(s.translate_negative(p), Ok(TilRect::new(7, 13, 27, 33)));
        let mut r = s;
        r.translate_in_place(p).unwrap();
        assert_eq!(r, TilRect::new(13, 27, 33, 47));
        r = s;
        r.translate_negative_in_place(p).unwrap();
        assert_eq!(r, TilRect::new(7, 13, 27, 33));
        assert_eq!(
            s.scale_up(TilSize::new(3, 7)),
            Ok(TilRect::new(30, 140, 90, 280))
        );
        assert_eq!(
            s.scale_up(TilSize::new(i32::MAX, 7)),
            Err(RectError::Overflow)
        );
        assert_eq!(
            TilRect::new(10, 20, 29, 40).scale_down(TilSize::new(3, 7)),
            Ok(TilRect::new(3, 2, 10, 6))
        );
    }

    #[test]
    fn microsoft_rectangle_accessors_contains_index_contract() {
        let r = TilRect::new(5, 10, 15, 20);
        assert_eq!((r.left, r.top, r.right, r.bottom), (5, 10, 15, 20));
        assert_eq!(r.width(), Ok(10));
        assert_eq!(r.height(), Ok(10));
        assert_eq!(
            (
                r.narrow_left_i16(),
                r.narrow_top_i16(),
                r.narrow_right_i16(),
                r.narrow_bottom_i16(),
                r.narrow_width_i16(),
                r.narrow_height_i16()
            ),
            (Ok(5), Ok(10), Ok(15), Ok(20), Ok(10), Ok(10))
        );
        assert_eq!(
            TilRect::new(5, 5, i32::MIN, 5).width(),
            Err(RectError::Overflow)
        );
        assert_eq!(
            TilRect::new(5, 5, 5, i32::MIN).height(),
            Err(RectError::Overflow)
        );
        assert_eq!(r.origin(), TilPoint::new(5, 10));
        assert_eq!(r.size(), Ok(TilSize::new(10, 10)));
        for x in [-1000, 0, 4, 5, 6, 14, 15, 16, 1000] {
            for y in [-1000, 0, 9, 10, 11, 19, 20, 21, 1000] {
                assert_eq!(
                    r.contains_point(TilPoint::new(x, y)),
                    x >= 5 && x < 15 && y >= 10 && y < 20
                );
            }
        }
        assert!(r.contains_rect(r));
        assert!(r.contains_rect(TilRect::new(8, 12, 10, 18)));
        assert!(!r.contains_rect(TilRect::new(0, 0, 50, 50)));
        assert!(!r.contains_rect(TilRect::new(14, 12, 30, 13)));
        assert_eq!(r.index_of(TilPoint::new(7, 17)), Ok(72));
        assert_eq!(r.index_of(TilPoint::new(1, 1)), Err(RectError::OutOfBounds));
        assert_eq!(r.point_at(72), Ok(TilPoint::new(7, 17)));
        assert_eq!(r.point_at(1000), Err(RectError::OutOfBounds));
    }

    #[test]
    fn microsoft_rectangle_projection_contract() {
        let r = TilRect::new(5, 10, 15, 20);
        assert_eq!(
            r.to_small_rect(),
            Ok(SmallRect {
                left: 5,
                top: 10,
                right: 14,
                bottom: 19
            })
        );
        assert_eq!(
            TilRect::new(i32::MAX, 10, 15, 20).to_small_rect(),
            Err(RectError::Overflow)
        );
        assert_eq!(
            r.to_win32_rect(),
            Win32Rect {
                left: 5,
                top: 10,
                right: 15,
                bottom: 20
            }
        );
        assert_eq!(
            r.to_d2d_rect(),
            D2dRectF {
                left: 5.0,
                top: 10.0,
                right: 15.0,
                bottom: 20.0
            }
        );
        assert_eq!(
            r.to_winrt_rect(),
            Ok(WinrtRect {
                x: 5.0,
                y: 10.0,
                width: 10.0,
                height: 10.0
            })
        );
    }

    #[test]
    fn microsoft_rectangle_iterator_contract() {
        let r = TilRect::new(5, 10, 15, 20);
        assert_eq!(r.begin().point(), TilPoint::new(5, 10));
        assert_eq!(r.end().point(), TilPoint::new(5, 20));
        let s = TilRect::from_size(TilSize::new(2, 2));
        let mut it = s.begin();
        for p in [
            TilPoint::new(0, 0),
            TilPoint::new(1, 0),
            TilPoint::new(0, 1),
            TilPoint::new(1, 1),
            TilPoint::new(0, 2),
            TilPoint::new(1, 2),
        ] {
            assert_eq!(it.point(), p);
            it.increment();
        }
        assert_eq!(s.begin(), s.begin());
        assert_ne!(s.begin(), s.end());
        assert!(s.begin() < s.end());
        assert!(s.end() > s.begin());
    }

    #[test]
    fn microsoft_rectangle_float_math_contract() {
        assert_eq!(
            TilRect::from_float(FloatMath::Ceiling, 1.6, 2.4, 3.2, 4.8),
            Ok(TilRect::new(2, 3, 4, 5))
        );
        assert_eq!(
            TilRect::from_float(FloatMath::Flooring, 1.6, 2.4, 3.2, 4.8),
            Ok(TilRect::new(1, 2, 3, 4))
        );
        assert_eq!(
            TilRect::from_float(FloatMath::Rounding, 1.6, 2.4, 3.2, 4.8),
            Ok(TilRect::new(2, 2, 3, 5))
        );
        assert_eq!(
            TilRect::from_float(FloatMath::Rounding, 3.6, 4.4, 5.7, 6.3),
            Ok(TilRect::new(4, 4, 6, 6))
        );
    }
}
