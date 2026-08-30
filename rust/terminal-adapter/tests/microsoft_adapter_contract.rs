use terminal_adapter::adapt_dispatch::{AdaptDispatchCore, PageGeometry, Point};
use terminal_parser::output_engine::{OutputAction, TermDispatch};

#[derive(Clone, Copy)]
enum Direction {
    Up,
    Down,
    Right,
    Left,
    Next,
    Previous,
}

fn microsoft_geometry() -> PageGeometry {
    // adapterTest.cpp uses a 100x600 buffer and a vertical viewport [20, 49).
    PageGeometry::new(20, 100, 29)
}

fn core_at(point: Point) -> AdaptDispatchCore {
    let mut core = AdaptDispatchCore::new(microsoft_geometry());
    core.set_cursor(point);
    core
}

fn apply(core: &mut AdaptDispatchCore, direction: Direction, distance: i32) {
    match direction {
        Direction::Up => core.cursor_up(distance),
        Direction::Down => core.cursor_down(distance),
        Direction::Right => core.cursor_forward(distance),
        Direction::Left => core.cursor_backward(distance),
        Direction::Next => core.cursor_next_line(distance),
        Direction::Previous => core.cursor_previous_line(distance),
    }
}

#[test]
fn microsoft_adapter_cursor_movement_matches_six_directions_and_bounds() {
    let edge_cases = [
        (Direction::Up, Point { x: 0, y: 20 }, Point { x: 0, y: 20 }),
        (
            Direction::Down,
            Point { x: 0, y: 48 },
            Point { x: 0, y: 48 },
        ),
        (
            Direction::Right,
            Point { x: 99, y: 20 },
            Point { x: 99, y: 20 },
        ),
        (
            Direction::Left,
            Point { x: 0, y: 20 },
            Point { x: 0, y: 20 },
        ),
        (
            Direction::Next,
            Point { x: 0, y: 48 },
            Point { x: 0, y: 48 },
        ),
        (
            Direction::Previous,
            Point { x: 0, y: 20 },
            Point { x: 0, y: 20 },
        ),
    ];
    for (direction, start, expected) in edge_cases {
        let mut core = core_at(start);
        apply(&mut core, direction, 1);
        assert_eq!(core.cursor(), expected);
    }

    for (direction, start, expected) in [
        (
            Direction::Next,
            Point { x: 99, y: 48 },
            Point { x: 0, y: 48 },
        ),
        (
            Direction::Previous,
            Point { x: 99, y: 20 },
            Point { x: 0, y: 20 },
        ),
    ] {
        let mut core = core_at(start);
        apply(&mut core, direction, 1);
        assert_eq!(core.cursor(), expected);
    }

    let center = Point { x: 50, y: 34 };
    for (direction, expected) in [
        (Direction::Up, Point { x: 50, y: 33 }),
        (Direction::Down, Point { x: 50, y: 35 }),
        (Direction::Right, Point { x: 51, y: 34 }),
        (Direction::Left, Point { x: 49, y: 34 }),
        (Direction::Next, Point { x: 0, y: 35 }),
        (Direction::Previous, Point { x: 0, y: 33 }),
    ] {
        let mut core = core_at(center);
        apply(&mut core, direction, 1);
        assert_eq!(core.cursor(), expected);
    }

    for (direction, expected) in [
        (Direction::Up, Point { x: 50, y: 20 }),
        (Direction::Down, Point { x: 50, y: 48 }),
        (Direction::Right, Point { x: 99, y: 34 }),
        (Direction::Left, Point { x: 0, y: 34 }),
        (Direction::Next, Point { x: 0, y: 48 }),
        (Direction::Previous, Point { x: 0, y: 20 }),
    ] {
        let mut core = core_at(center);
        apply(&mut core, direction, 100);
        assert_eq!(core.cursor(), expected);
    }
}

#[test]
fn microsoft_adapter_cursor_position_matches_viewport_relative_rows_and_buffer_columns() {
    let mut core = core_at(Point { x: 0, y: 20 });
    core.cursor_position(14, 14);
    assert_eq!(core.cursor(), Point { x: 13, y: 33 });

    core.set_cursor(Point { x: 99, y: 48 });
    core.cursor_position(1, 1);
    assert_eq!(core.cursor(), Point { x: 0, y: 20 });

    core.set_cursor(Point { x: 0, y: 20 });
    core.cursor_position(58, 200);
    assert_eq!(core.cursor(), Point { x: 99, y: 48 });
}

#[test]
fn microsoft_adapter_single_dimension_absolute_positioning_matches_reference_bounds() {
    let mut horizontal = core_at(Point { x: 0, y: 20 });
    horizontal.cursor_horizontal_absolute(50);
    assert_eq!(horizontal.cursor(), Point { x: 49, y: 20 });
    horizontal.set_cursor(Point { x: 99, y: 48 });
    horizontal.cursor_horizontal_absolute(1);
    assert_eq!(horizontal.cursor(), Point { x: 0, y: 48 });
    horizontal.cursor_horizontal_absolute(200);
    assert_eq!(horizontal.cursor(), Point { x: 99, y: 48 });

    let mut vertical = core_at(Point { x: 0, y: 20 });
    vertical.cursor_vertical_absolute(14);
    assert_eq!(vertical.cursor(), Point { x: 0, y: 33 });
    vertical.set_cursor(Point { x: 99, y: 48 });
    vertical.cursor_vertical_absolute(1);
    assert_eq!(vertical.cursor(), Point { x: 99, y: 20 });
    vertical.cursor_vertical_absolute(58);
    assert_eq!(vertical.cursor(), Point { x: 99, y: 48 });
}

#[test]
fn microsoft_adapter_cursor_save_restore_ported_subset_preserves_cursor_state() {
    let mut core = core_at(Point { x: 50, y: 34 });

    core.dispatch(OutputAction::CursorRestoreState);
    assert_eq!(core.cursor(), Point { x: 0, y: 20 });
    assert!(!core.delayed_eol_wrap());

    core.set_cursor(Point { x: 50, y: 34 });
    core.set_delayed_eol_wrap(true);
    core.dispatch(OutputAction::CursorSaveState);
    core.set_cursor(Point { x: 0, y: 48 });
    core.dispatch(OutputAction::CursorRestoreState);

    assert_eq!(core.cursor(), Point { x: 50, y: 34 });
    assert!(core.delayed_eol_wrap());
}
