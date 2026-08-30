use terminal_buffer::geometry::{InclusiveRect, Point};
use terminal_buffer::uia_text_range::{TextRangeEndpoint, UiaTextRangeCore};
use terminal_buffer::viewport::Viewport;

#[test]
fn microsoft_interactivity_uia_degenerate_ranges_detected() {
    let origin = Point::new(0, 0);
    let degenerate = UiaTextRangeCore::new(origin, origin);
    assert!(degenerate.is_degenerate());
    assert_eq!(degenerate.start(), degenerate.end());

    let non_degenerate = UiaTextRangeCore::new(origin, Point::new(1, 0));
    assert!(!non_degenerate.is_degenerate());
    assert_ne!(non_degenerate.start(), non_degenerate.end());
}

#[test]
fn microsoft_interactivity_uia_compare_range() {
    let origin = Point::new(0, 0);
    let range = UiaTextRangeCore::new(origin, origin);
    let clone = range;
    assert!(range.same_range(clone));

    let different_end = UiaTextRangeCore::new(origin, Point::new(2, 0));
    assert!(!range.same_range(different_end));
}

#[test]
fn microsoft_interactivity_uia_compare_endpoints() {
    let origin = Point::new(0, 0);
    let bounds = Viewport::from_inclusive(InclusiveRect::new(0, 0, 9, 9));
    let range = UiaTextRangeCore::new(origin, origin);
    let clone = range;

    assert_eq!(
        range.compare_endpoints(
            TextRangeEndpoint::Start,
            range,
            TextRangeEndpoint::End,
            bounds
        ),
        Some(0)
    );
    assert_eq!(
        range.compare_endpoints(
            TextRangeEndpoint::Start,
            clone,
            TextRangeEndpoint::Start,
            bounds
        ),
        Some(0)
    );
    assert_eq!(
        range.compare_endpoints(
            TextRangeEndpoint::End,
            clone,
            TextRangeEndpoint::End,
            bounds
        ),
        Some(0)
    );

    let different_end = UiaTextRangeCore::new(origin, Point::new(2, 0));
    assert_eq!(
        range.compare_endpoints(
            TextRangeEndpoint::Start,
            different_end,
            TextRangeEndpoint::Start,
            bounds
        ),
        Some(0)
    );
    assert_eq!(
        range.compare_endpoints(
            TextRangeEndpoint::End,
            different_end,
            TextRangeEndpoint::End,
            bounds
        ),
        Some(-2)
    );
    assert_eq!(
        different_end.compare_endpoints(
            TextRangeEndpoint::End,
            range,
            TextRangeEndpoint::End,
            bounds
        ),
        Some(2)
    );
}

#[test]
fn microsoft_interactivity_uia_move_endpoint_by_range() {
    let origin = Point::new(0, 0);
    let source = UiaTextRangeCore::new(Point::new(0, 1), Point::new(1, 2));

    let mut target = UiaTextRangeCore::new(origin, origin);
    target.move_endpoint_by_range(TextRangeEndpoint::End, source, TextRangeEndpoint::Start);
    assert_eq!(target.start(), origin);
    assert_eq!(target.end(), source.start());

    target = UiaTextRangeCore::new(origin, origin);
    target.move_endpoint_by_range(TextRangeEndpoint::End, source, TextRangeEndpoint::End);
    assert_eq!(target.start(), origin);
    assert_eq!(target.end(), source.end());
    target.move_endpoint_by_range(TextRangeEndpoint::Start, source, TextRangeEndpoint::Start);
    assert_eq!(target.start(), source.start());
    assert_eq!(target.end(), source.end());

    target = source;
    target.move_endpoint_by_range(TextRangeEndpoint::Start, target, TextRangeEndpoint::End);
    assert_eq!(target.start(), source.end());
    assert_eq!(target.end(), source.end());
    assert!(target.is_degenerate());

    target = source;
    target.move_endpoint_by_range(TextRangeEndpoint::End, target, TextRangeEndpoint::Start);
    assert_eq!(target.start(), source.start());
    assert_eq!(target.end(), source.start());
    assert!(target.is_degenerate());

    target = UiaTextRangeCore::new(origin, origin);
    target.move_endpoint_by_range(TextRangeEndpoint::Start, source, TextRangeEndpoint::End);
    assert_eq!(target.start(), source.end());
    assert_eq!(target.end(), source.end());
    assert!(target.is_degenerate());

    target.move_endpoint_by_range(TextRangeEndpoint::End, source, TextRangeEndpoint::Start);
    assert_eq!(target.start(), source.start());
    assert_eq!(target.end(), source.start());
    assert!(target.is_degenerate());
}

#[test]
fn microsoft_interactivity_uia_block_range_clone_preserves_state() {
    let origin = Point::new(0, 0);
    let range = UiaTextRangeCore::new(origin, origin);
    assert!(!range.block_range());

    let mut clone = range;
    assert!(!clone.block_range());
    clone.set_block_range(true);

    let clone_again = clone;
    assert!(clone_again.block_range());
}
