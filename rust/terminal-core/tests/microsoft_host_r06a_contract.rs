use std::cmp::Ordering;
use terminal_core::selection::BufferPoint;

fn compare(a: BufferPoint, b: BufferPoint) -> Ordering {
    a.cmp(&b)
}

#[test]
fn microsoft_host_compare_coords_matches_all_row_major_cases() {
    let center = BufferPoint::new(20, 20);

    assert_eq!(compare(center, center), Ordering::Equal);

    assert_eq!(
        compare(BufferPoint::new(10, 20), center),
        Ordering::Less,
        "left on same row"
    );
    assert_eq!(
        compare(BufferPoint::new(20, 10), center),
        Ordering::Less,
        "above on same column"
    );
    assert_eq!(
        compare(BufferPoint::new(10, 10), center),
        Ordering::Less,
        "upper-left"
    );
    assert_eq!(
        compare(BufferPoint::new(30, 10), center),
        Ordering::Less,
        "upper-right still precedes by row"
    );

    assert_eq!(
        compare(BufferPoint::new(30, 20), center),
        Ordering::Greater,
        "right on same row"
    );
    assert_eq!(
        compare(BufferPoint::new(20, 30), center),
        Ordering::Greater,
        "below on same column"
    );
    assert_eq!(
        compare(BufferPoint::new(10, 30), center),
        Ordering::Greater,
        "lower-left still follows by row"
    );
    assert_eq!(
        compare(BufferPoint::new(30, 30), center),
        Ordering::Greater,
        "lower-right"
    );
}
