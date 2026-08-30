#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ExclusiveRect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

impl ExclusiveRect {
    #[must_use]
    pub const fn new(left: i32, top: i32, right: i32, bottom: i32) -> Self {
        Self {
            left,
            top,
            right,
            bottom,
        }
    }
}

#[must_use]
pub fn plan_redraw_region(
    mut region: ExclusiveRect,
    viewport: ExclusiveRect,
    double_width_rows: &[bool],
) -> Option<ExclusiveRect> {
    if double_width_rows
        .iter()
        .any(|is_double_width| *is_double_width)
    {
        region.right *= 2;
    }

    let clipped = ExclusiveRect {
        left: region.left.max(viewport.left),
        top: region.top.max(viewport.top),
        right: region.right.min(viewport.right),
        bottom: region.bottom.min(viewport.bottom),
    };

    if clipped.left >= clipped.right || clipped.top >= clipped.bottom {
        return None;
    }

    Some(ExclusiveRect {
        left: clipped.left - viewport.left,
        top: clipped.top - viewport.top,
        right: clipped.right - viewport.left,
        bottom: clipped.bottom - viewport.top,
    })
}

#[cfg(test)]
mod tests {
    use super::{ExclusiveRect, plan_redraw_region};

    #[test]
    fn redraw_region_is_clipped_and_converted_to_viewport_origin() {
        let region = ExclusiveRect::new(12, 22, 18, 27);
        let viewport = ExclusiveRect::new(10, 20, 30, 40);

        assert_eq!(
            plan_redraw_region(region, viewport, &[false; 5]),
            Some(ExclusiveRect::new(2, 2, 8, 7))
        );
    }

    #[test]
    fn redraw_region_outside_viewport_produces_no_invalidation() {
        let region = ExclusiveRect::new(0, 0, 5, 5);
        let viewport = ExclusiveRect::new(10, 10, 20, 20);

        assert_eq!(plan_redraw_region(region, viewport, &[false; 5]), None);
    }

    #[test]
    fn double_width_row_doubles_right_margin_before_clipping() {
        let region = ExclusiveRect::new(4, 2, 8, 5);
        let viewport = ExclusiveRect::new(0, 0, 20, 10);

        assert_eq!(
            plan_redraw_region(region, viewport, &[false, true, false]),
            Some(ExclusiveRect::new(4, 2, 16, 5))
        );
    }

    #[test]
    fn expanded_double_width_region_is_still_trimmed_to_viewport() {
        let region = ExclusiveRect::new(8, 12, 14, 14);
        let viewport = ExclusiveRect::new(10, 10, 20, 20);

        assert_eq!(
            plan_redraw_region(region, viewport, &[true, false]),
            Some(ExclusiveRect::new(0, 2, 10, 4))
        );
    }
}
