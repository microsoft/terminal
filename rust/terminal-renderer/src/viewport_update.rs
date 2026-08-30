#[derive(Clone, Copy, Debug, Eq, PartialEq)]
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
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ScrollDelta {
    pub x: i32,
    pub y: i32,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ViewportUpdate {
    pub viewport: InclusiveRect,
    pub scroll_delta: ScrollDelta,
}

#[must_use]
pub const fn plan_viewport_update(
    old_viewport: InclusiveRect,
    new_viewport: InclusiveRect,
    force_update: bool,
) -> Option<ViewportUpdate> {
    if !force_update
        && old_viewport.left == new_viewport.left
        && old_viewport.top == new_viewport.top
        && old_viewport.right == new_viewport.right
        && old_viewport.bottom == new_viewport.bottom
    {
        return None;
    }

    Some(ViewportUpdate {
        viewport: new_viewport,
        scroll_delta: ScrollDelta {
            x: old_viewport.left - new_viewport.left,
            y: old_viewport.top - new_viewport.top,
        },
    })
}

#[cfg(test)]
mod tests {
    use super::{InclusiveRect, ScrollDelta, ViewportUpdate, plan_viewport_update};

    #[test]
    fn unchanged_viewport_needs_no_update_without_force() {
        let viewport = InclusiveRect::new(0, 10, 79, 34);

        assert_eq!(plan_viewport_update(viewport, viewport, false), None);
    }

    #[test]
    fn forced_update_resynchronizes_even_when_viewport_is_unchanged() {
        let viewport = InclusiveRect::new(0, 10, 79, 34);

        assert_eq!(
            plan_viewport_update(viewport, viewport, true),
            Some(ViewportUpdate {
                viewport,
                scroll_delta: ScrollDelta { x: 0, y: 0 },
            })
        );
    }

    #[test]
    fn scroll_delta_is_old_origin_minus_new_origin() {
        let old_viewport = InclusiveRect::new(5, 20, 84, 44);
        let new_viewport = InclusiveRect::new(2, 27, 81, 51);

        assert_eq!(
            plan_viewport_update(old_viewport, new_viewport, false),
            Some(ViewportUpdate {
                viewport: new_viewport,
                scroll_delta: ScrollDelta { x: 3, y: -7 },
            })
        );
    }

    #[test]
    fn resize_without_origin_change_still_updates_the_engine() {
        let old_viewport = InclusiveRect::new(0, 0, 79, 24);
        let new_viewport = InclusiveRect::new(0, 0, 119, 39);

        assert_eq!(
            plan_viewport_update(old_viewport, new_viewport, false),
            Some(ViewportUpdate {
                viewport: new_viewport,
                scroll_delta: ScrollDelta { x: 0, y: 0 },
            })
        );
    }
}
