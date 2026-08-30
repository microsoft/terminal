use terminal_buffer::geometry::{InclusiveRect, Point, Rect};
use terminal_buffer::viewport::{InvalidViewport, Size, Viewport};

fn assert_shape(view: Viewport, left: i32, top: i32, right: i32, bottom: i32) {
    assert_eq!(view.left(), left);
    assert_eq!(view.right_inclusive(), right);
    assert_eq!(view.right_exclusive(), right + 1);
    assert_eq!(view.top(), top);
    assert_eq!(view.bottom_inclusive(), bottom);
    assert_eq!(view.bottom_exclusive(), bottom + 1);
    assert_eq!(view.width(), right - left + 1);
    assert_eq!(view.height(), bottom - top + 1);
    assert_eq!(view.origin(), Point::new(left, top));
    assert_eq!(
        view.dimensions(),
        Size::new(right - left + 1, bottom - top + 1)
    );
}

#[test]
fn microsoft_host_viewport_create_empty() {
    let view = Viewport::empty();
    assert_shape(view, 0, 0, -1, -1);
    assert_eq!(view.origin(), Point::default());
    assert_eq!(view.dimensions(), Size::default());
}

#[test]
fn microsoft_host_viewport_create_from_inclusive() {
    let view = Viewport::from_inclusive(InclusiveRect::new(10, 3, 20, 5));
    assert_shape(view, 10, 3, 20, 5);
}

#[test]
fn microsoft_host_viewport_create_from_exclusive() {
    let view = Viewport::from_exclusive(Rect::new(10, 3, 20, 5));
    assert_shape(view, 10, 3, 19, 4);
}

#[test]
fn microsoft_host_viewport_create_from_dimensions_width_height() {
    let view = Viewport::from_dimensions(Point::new(10, 3), Size::new(11, 3));
    assert_shape(view, 10, 3, 20, 5);
}

#[test]
fn microsoft_host_viewport_create_from_dimensions() {
    let view = Viewport::from_dimensions(Point::new(10, 3), Size::new(11, 3));
    assert_shape(view, 10, 3, 20, 5);
}

#[test]
fn microsoft_host_viewport_create_from_dimensions_no_origin() {
    let view = Viewport::from_dimensions(Point::default(), Size::new(21, 6));
    assert_shape(view, 0, 0, 20, 5);
}

#[test]
fn microsoft_host_viewport_is_in_bounds_coord() {
    let view = Viewport::from_inclusive(InclusiveRect::new(10, 3, 20, 5));
    for point in [
        Point::new(10, 3),
        Point::new(10, 5),
        Point::new(20, 5),
        Point::new(20, 3),
    ] {
        assert!(view.contains_point(point));
    }
    for point in [
        Point::new(21, 3),
        Point::new(20, 2),
        Point::new(9, 3),
        Point::new(10, 2),
        Point::new(9, 5),
        Point::new(10, 6),
        Point::new(21, 5),
        Point::new(20, 6),
    ] {
        assert!(!view.contains_point(point));
    }
}

#[test]
fn microsoft_host_viewport_is_in_bounds_viewport() {
    let view = Viewport::from_inclusive(InclusiveRect::new(10, 3, 20, 5));
    assert!(view.contains_viewport(view));
    assert!(view.contains_viewport(Viewport::from_inclusive(InclusiveRect::new(11, 4, 19, 4))));

    for bounds in [
        InclusiveRect::new(10, 2, 20, 5),
        InclusiveRect::new(10, 3, 20, 6),
        InclusiveRect::new(9, 3, 20, 5),
        InclusiveRect::new(10, 3, 21, 5),
        InclusiveRect::new(11, 4, 21, 6),
    ] {
        assert!(!view.contains_viewport(Viewport::from_inclusive(bounds)));
    }
}

#[test]
fn microsoft_host_viewport_clamp_coord() {
    let view = Viewport::from_inclusive(InclusiveRect::new(10, 3, 20, 5));
    for corner in [
        Point::new(10, 3),
        Point::new(10, 5),
        Point::new(20, 5),
        Point::new(20, 3),
    ] {
        let mut point = corner;
        view.clamp_point(&mut point).expect("valid viewport");
        assert_eq!(point, corner);
    }

    for (mut point, expected) in [
        (Point::new(21, 2), Point::new(20, 3)),
        (Point::new(9, 2), Point::new(10, 3)),
        (Point::new(9, 6), Point::new(10, 5)),
        (Point::new(21, 6), Point::new(20, 5)),
    ] {
        view.clamp_point(&mut point).expect("valid viewport");
        assert_eq!(point, expected);
    }

    let mut point = Point::new(21, 6);
    assert_eq!(
        Viewport::empty().clamp_point(&mut point),
        Err(InvalidViewport)
    );
}

#[test]
fn microsoft_host_viewport_clamp_viewport() {
    let view = Viewport::from_inclusive(InclusiveRect::new(10, 3, 20, 5));
    let larger = Viewport::from_inclusive(InclusiveRect::new(7, 0, 23, 8));
    assert_eq!(view.clamp_viewport(larger), view);

    let inside = Viewport::from_inclusive(InclusiveRect::new(11, 4, 19, 4));
    assert_eq!(view.clamp_viewport(inside), inside);

    let crossed = Viewport::from_inclusive(InclusiveRect::new(30, 15, 0, -7));
    assert_eq!(view.clamp_viewport(crossed), Viewport::empty());
}

#[test]
fn microsoft_host_viewport_increment_in_bounds() {
    let view = Viewport::from_inclusive(InclusiveRect::new(10, 20, 19, 29));

    let mut point = Point::new(15, 25);
    assert!(view.increment_in_bounds(&mut point));
    assert_eq!(point, Point::new(16, 25));

    let mut point = Point::new(19, 25);
    assert!(view.increment_in_bounds(&mut point));
    assert_eq!(point, Point::new(10, 26));

    let mut point = Point::new(19, 29);
    assert!(!view.increment_in_bounds(&mut point));
    assert_eq!(point, Point::new(19, 29));
}

#[test]
fn microsoft_host_viewport_decrement_in_bounds() {
    let view = Viewport::from_inclusive(InclusiveRect::new(10, 20, 19, 29));

    let mut point = Point::new(15, 25);
    assert!(view.decrement_in_bounds(&mut point));
    assert_eq!(point, Point::new(14, 25));

    let mut point = Point::new(10, 25);
    assert!(view.decrement_in_bounds(&mut point));
    assert_eq!(point, Point::new(19, 24));

    let mut point = Point::new(10, 20);
    assert!(!view.decrement_in_bounds(&mut point));
    assert_eq!(point, Point::new(10, 20));
}

#[test]
fn microsoft_host_viewport_move_in_bounds() {
    let view = Viewport::from_inclusive(InclusiveRect::new(0, 0, 19, 19));
    for y in 0..20 {
        for x in 0..20 {
            for delta in [0, 1, 7, 19, 20, 37, 199, 399] {
                let start_index = y * 20 + x;
                let target_index = (start_index + delta).min(399);
                let expected = Point::new(target_index % 20, target_index / 20);
                let mut point = Point::new(x, y);
                let success = view.walk_in_bounds(&mut point, delta);
                assert_eq!(success, start_index + delta <= 399);
                assert_eq!(point, expected);
            }
        }
    }
}

#[test]
fn microsoft_host_viewport_compare_in_bounds() {
    let view = Viewport::from_inclusive(InclusiveRect::new(10, 20, 19, 29));
    assert_eq!(
        view.compare_in_bounds(Point::new(12, 24), Point::new(14, 24)),
        -2
    );
    assert_eq!(
        view.compare_in_bounds(Point::new(14, 24), Point::new(12, 24)),
        2
    );
    assert_eq!(
        view.compare_in_bounds(Point::new(10, 24), Point::new(19, 23)),
        1
    );
    assert_eq!(
        view.compare_in_bounds(Point::new(19, 23), Point::new(10, 24)),
        -1
    );
}

#[test]
fn microsoft_host_viewport_offset() {
    let original = Viewport::from_inclusive(InclusiveRect::new(0, 0, 10, 10));
    assert_eq!(
        Viewport::offset(original, Point::new(7, 2)),
        Viewport::from_inclusive(InclusiveRect::new(7, 2, 17, 12))
    );
    assert_eq!(
        Viewport::offset(original, Point::new(-3, -5)),
        Viewport::from_inclusive(InclusiveRect::new(-3, -5, 7, 5))
    );
}

#[test]
fn microsoft_host_viewport_union() {
    let one = Viewport::from_inclusive(InclusiveRect::new(4, 6, 10, 14));
    let two = Viewport::from_inclusive(InclusiveRect::new(5, 2, 13, 10));
    assert_eq!(
        Viewport::union(one, two),
        Viewport::from_inclusive(InclusiveRect::new(4, 2, 13, 14))
    );
}

#[test]
fn microsoft_host_viewport_intersect() {
    let one = Viewport::from_inclusive(InclusiveRect::new(4, 6, 10, 14));
    let two = Viewport::from_inclusive(InclusiveRect::new(5, 2, 13, 10));
    assert_eq!(
        Viewport::intersect(one, two),
        Viewport::from_inclusive(InclusiveRect::new(5, 6, 10, 10))
    );
}

fn subtraction_fixture(remove: InclusiveRect) -> Vec<Viewport> {
    Viewport::subtract(
        Viewport::from_inclusive(InclusiveRect::new(0, 0, 10, 10)),
        Viewport::from_inclusive(remove),
    )
}

#[test]
fn microsoft_host_viewport_subtract_four() {
    assert_eq!(
        subtraction_fixture(InclusiveRect::new(3, 3, 6, 6)),
        vec![
            Viewport::from_inclusive(InclusiveRect::new(0, 0, 10, 2)),
            Viewport::from_inclusive(InclusiveRect::new(0, 7, 10, 10)),
            Viewport::from_inclusive(InclusiveRect::new(0, 3, 2, 6)),
            Viewport::from_inclusive(InclusiveRect::new(7, 3, 10, 6)),
        ]
    );
}

#[test]
fn microsoft_host_viewport_subtract_three() {
    assert_eq!(
        subtraction_fixture(InclusiveRect::new(3, 3, 15, 6)),
        vec![
            Viewport::from_inclusive(InclusiveRect::new(0, 0, 10, 2)),
            Viewport::from_inclusive(InclusiveRect::new(0, 7, 10, 10)),
            Viewport::from_inclusive(InclusiveRect::new(0, 3, 2, 6)),
        ]
    );
}

#[test]
fn microsoft_host_viewport_subtract_two() {
    assert_eq!(
        subtraction_fixture(InclusiveRect::new(3, 3, 15, 15)),
        vec![
            Viewport::from_inclusive(InclusiveRect::new(0, 0, 10, 2)),
            Viewport::from_inclusive(InclusiveRect::new(0, 3, 2, 10)),
        ]
    );
}

#[test]
fn microsoft_host_viewport_subtract_one() {
    assert_eq!(
        subtraction_fixture(InclusiveRect::new(-12, 3, 15, 15)),
        vec![Viewport::from_inclusive(InclusiveRect::new(0, 0, 10, 2))]
    );
}

#[test]
fn microsoft_host_viewport_subtract_zero() {
    let original = Viewport::from_inclusive(InclusiveRect::new(0, 0, 10, 10));
    assert_eq!(
        Viewport::subtract(
            original,
            Viewport::from_inclusive(InclusiveRect::new(12, 12, 15, 15))
        ),
        vec![original]
    );
}

#[test]
fn microsoft_host_viewport_subtract_same() {
    let original = Viewport::from_inclusive(InclusiveRect::new(0, 0, 10, 10));
    assert!(Viewport::subtract(original, original).is_empty());
}
