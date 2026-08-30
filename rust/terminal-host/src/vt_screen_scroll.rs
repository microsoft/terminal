//! Stateful `ScrollConsoleScreenBufferW` ownership for `ConPTY` output.
//!
//! This module ports the deterministic mutation and projection paths from
//! `ApiRoutines::ScrollConsoleScreenBufferWImpl`: the ordinary `CHAR_INFO`
//! backup/fill path and the DEC rectangular-area (`DECCRA`/`DECFRA`) path.

use crate::attribute_format::format_attributes;
use crate::vt_char_info::{HostCharInfo, write_infos};
use crate::vt_console_output::VtScreenOutputState;
use crate::vt_io_protocol::sanitize_ucs2;
use crate::vt_writer_sequences::{restore_cursor, save_cursor};
use terminal_buffer::geometry::{InclusiveRect, Point};
use terminal_buffer::text_attribute::{LegacyColorDefaults, TextAttribute};
use terminal_buffer::viewport::{Size, Viewport};

const CLEAR_SCREEN: &[u8] = b"\x1b[H\x1b[2J\x1b[3J";

/// Executes one legacy screen-buffer scroll against live cell state and returns
/// the exact VT projection selected by the `ConPTY` writer.
#[must_use]
pub fn scroll_console_screen_buffer(
    state: &mut VtScreenOutputState,
    source: InclusiveRect,
    target: Point,
    clip: Option<InclusiveRect>,
    fill_character: u16,
    fill_attribute: u16,
    enable_cmd_clear_shim: bool,
    rectangular_area_operations: bool,
) -> Vec<u8> {
    if (source.left == target.x && source.top == target.y)
        || source.left > source.right
        || source.top > source.bottom
    {
        return Vec::new();
    }

    let width = i32::try_from(state.width()).unwrap_or(i32::MAX);
    let height = i32::try_from(state.height()).unwrap_or(i32::MAX);
    let screen = Viewport::from_dimensions(Point::new(0, 0), Size::new(width, height));
    let source_viewport = Viewport::from_inclusive(source);
    let clip_viewport = clip
        .map(Viewport::from_inclusive)
        .map_or(screen, |value| Viewport::intersect(value, screen));

    let fill_character = if fill_character == 0 {
        u16::from(b' ')
    } else {
        sanitize_ucs2(fill_character)
    };

    if enable_cmd_clear_shim
        && source.left <= 0
        && source.top <= 0
        && source.right >= width.saturating_sub(1)
        && source.bottom >= height.saturating_sub(1)
        && target.x == 0
        && target.y <= -height
        && clip.is_none()
        && fill_character == u16::from(b' ')
    {
        let mut cells = state.cells().to_vec();
        for cell in &mut cells {
            cell.code_unit = u16::from(b' ');
        }
        let _ = state.replace_cells(&cells);
        return CLEAR_SCREEN.to_vec();
    }

    let mut cells = state.cells().to_vec();

    if rectangular_area_operations {
        let bytes = rectangular_protocol(
            screen,
            source_viewport,
            target,
            clip_viewport,
            fill_character,
            fill_attribute,
        );
        let _ = fallback_mutation(
            &mut cells,
            state.width(),
            state.height(),
            screen,
            source_viewport,
            target,
            clip_viewport,
            fill_character,
            fill_attribute,
            false,
        );
        let _ = state.replace_cells(&cells);
        bytes
    } else {
        let bytes = fallback_mutation(
            &mut cells,
            state.width(),
            state.height(),
            screen,
            source_viewport,
            target,
            clip_viewport,
            fill_character,
            fill_attribute,
            true,
        );
        let _ = state.replace_cells(&cells);
        bytes
    }
}

#[allow(clippy::too_many_arguments)]
fn fallback_mutation(
    cells: &mut [HostCharInfo],
    width: usize,
    height: usize,
    screen: Viewport,
    source: Viewport,
    target: Point,
    clip: Viewport,
    fill_character: u16,
    fill_attribute: u16,
    emit: bool,
) -> Vec<u8> {
    let source_width = source.width().max(0) as usize;
    let source_height = source.height().max(0) as usize;
    if source_width == 0 || source_height == 0 {
        return Vec::new();
    }

    let fill_cell = HostCharInfo::new(fill_character, fill_attribute);
    let mut backup = vec![fill_cell; source_width.saturating_mul(source_height)];
    let read = Viewport::intersect(source, screen);
    if read.is_valid() {
        for y in read.top()..=read.bottom_inclusive() {
            for x in read.left()..=read.right_inclusive() {
                let src_index = usize::try_from(y)
                    .unwrap_or(usize::MAX)
                    .saturating_mul(width)
                    .saturating_add(usize::try_from(x).unwrap_or(usize::MAX));
                let backup_y =
                    usize::try_from(y.saturating_sub(source.top())).unwrap_or(usize::MAX);
                let backup_x =
                    usize::try_from(x.saturating_sub(source.left())).unwrap_or(usize::MAX);
                let backup_index = backup_y
                    .saturating_mul(source_width)
                    .saturating_add(backup_x);
                if let (Some(source_cell), Some(target_cell)) =
                    (cells.get(src_index).copied(), backup.get_mut(backup_index))
                {
                    *target_cell = source_cell;
                }
            }
        }
    }

    let fill = vec![fill_cell; source_width.saturating_mul(source_height)];
    let fill_viewport = Viewport::intersect(source, clip);
    let mut body = write_buffer(
        cells,
        width,
        height,
        &fill,
        source_width,
        fill_viewport,
        emit,
    );

    if read.is_valid() {
        let target_viewport = Viewport::from_dimensions(target, read.dimensions());
        let target_viewport = Viewport::intersect(target_viewport, clip);
        body.extend_from_slice(&write_buffer(
            cells,
            width,
            height,
            &backup,
            source_width,
            target_viewport,
            emit,
        ));
    }

    if !emit || body.is_empty() {
        return body;
    }
    let mut output = Vec::with_capacity(save_cursor().len() + body.len() + restore_cursor().len());
    output.extend_from_slice(save_cursor());
    output.extend_from_slice(&body);
    output.extend_from_slice(restore_cursor());
    output
}

fn write_buffer(
    cells: &mut [HostCharInfo],
    width: usize,
    height: usize,
    buffer: &[HostCharInfo],
    stride: usize,
    request: Viewport,
    emit: bool,
) -> Vec<u8> {
    if stride == 0 || !request.is_valid() {
        return Vec::new();
    }
    let screen = Viewport::from_dimensions(
        Point::new(0, 0),
        Size::new(
            i32::try_from(width).unwrap_or(i32::MAX),
            i32::try_from(height).unwrap_or(i32::MAX),
        ),
    );
    let clipped = Viewport::intersect(request, screen);
    if !clipped.is_valid() {
        return Vec::new();
    }

    let offset_y =
        usize::try_from(clipped.top().saturating_sub(request.top())).unwrap_or(usize::MAX);
    let offset_x =
        usize::try_from(clipped.left().saturating_sub(request.left())).unwrap_or(usize::MAX);
    let mut buffer_offset = offset_y.saturating_mul(stride).saturating_add(offset_x);
    let row_width = usize::try_from(clipped.width()).unwrap_or(0);
    let mut output = Vec::new();

    for y in clipped.top()..=clipped.bottom_inclusive() {
        let screen_start = usize::try_from(y)
            .unwrap_or(usize::MAX)
            .saturating_mul(width)
            .saturating_add(usize::try_from(clipped.left()).unwrap_or(usize::MAX));
        let screen_end = screen_start.saturating_add(row_width);
        let buffer_end = buffer_offset.saturating_add(row_width);
        if screen_end > cells.len() || buffer_end > buffer.len() {
            break;
        }
        cells[screen_start..screen_end].copy_from_slice(&buffer[buffer_offset..buffer_end]);
        if emit {
            output.extend_from_slice(&write_infos(
                clipped.left(),
                y,
                &cells[screen_start..screen_end],
            ));
        }
        buffer_offset = buffer_offset.saturating_add(stride);
    }
    output
}

fn rectangular_protocol(
    screen: Viewport,
    source: Viewport,
    target: Point,
    clip: Viewport,
    fill_character: u16,
    fill_attribute: u16,
) -> Vec<u8> {
    let fill_viewport = Viewport::intersect(source, clip);
    let target_source = Point::new(
        target.x.saturating_sub(source.left()),
        target.y.saturating_sub(source.top()),
    );
    let source_target = Point::new(-target_source.x, -target_source.y);
    let clip_at_source = Viewport::offset(clip, source_target);
    let copy_source = Viewport::intersect(Viewport::intersect(source, screen), clip_at_source);
    let copy_target = if copy_source.is_valid() {
        Viewport::offset(copy_source, target_source)
    } else {
        Viewport::empty()
    };
    let fills = Viewport::subtract(fill_viewport, copy_target);

    if !copy_target.is_valid() && fills.is_empty() {
        return Vec::new();
    }

    let mut output = Vec::new();
    output.extend_from_slice(save_cursor());
    if !fills.is_empty() {
        output.extend_from_slice(
            format_attributes(TextAttribute::from_legacy(
                fill_attribute,
                LegacyColorDefaults::default(),
            ))
            .as_bytes(),
        );
    }
    if copy_target.is_valid() {
        output.extend_from_slice(
            format!(
                "\x1b[{};{};{};{};;{};{}$v",
                copy_source.top() + 1,
                copy_source.left() + 1,
                copy_source.bottom_exclusive(),
                copy_source.right_exclusive(),
                copy_target.top() + 1,
                copy_target.left() + 1,
            )
            .as_bytes(),
        );
    }
    for fill in fills {
        output.extend_from_slice(
            format!(
                "\x1b[{};{};{};{};{}$x",
                fill_character,
                fill.top() + 1,
                fill.left() + 1,
                fill.bottom_exclusive(),
                fill.right_exclusive(),
            )
            .as_bytes(),
        );
    }
    output.extend_from_slice(restore_cursor());
    output
}

#[cfg(test)]
mod tests {
    use super::*;

    const RED: u16 = 0x0004 | 0x0020;
    const BLUE: u16 = 0x0001 | 0x0020;

    fn ci(ch: char, attributes: u16) -> HostCharInfo {
        HostCharInfo::new(u16::try_from(u32::from(ch)).unwrap(), attributes)
    }

    fn initial_state() -> VtScreenOutputState {
        let rows = [
            [
                ('A', RED),
                ('B', RED),
                ('a', BLUE),
                ('b', BLUE),
                ('C', RED),
                ('D', RED),
                ('c', BLUE),
                ('d', BLUE),
            ],
            [
                ('E', RED),
                ('F', RED),
                ('e', BLUE),
                ('f', BLUE),
                ('G', RED),
                ('H', RED),
                ('g', BLUE),
                ('h', BLUE),
            ],
            [
                ('i', BLUE),
                ('j', BLUE),
                ('I', RED),
                ('J', RED),
                ('k', BLUE),
                ('l', BLUE),
                ('K', RED),
                ('L', RED),
            ],
            [
                ('m', BLUE),
                ('n', BLUE),
                ('M', RED),
                ('N', RED),
                ('o', BLUE),
                ('p', BLUE),
                ('O', RED),
                ('P', RED),
            ],
        ];
        let cells = rows
            .into_iter()
            .flatten()
            .map(|(ch, attr)| ci(ch, attr))
            .collect::<Vec<_>>();
        let mut state = VtScreenOutputState::new(8, 4, 0x0007);
        assert!(state.replace_cells(&cells));
        state
    }

    fn execute_reference_scroll_sequence(rectangular: bool) -> (VtScreenOutputState, Vec<Vec<u8>>) {
        let mut state = initial_state();
        let calls = [
            (
                InclusiveRect::new(1, 0, 2, 1),
                Point::new(5, 2),
                None,
                u16::from(b'Z'),
                RED,
            ),
            (
                InclusiveRect::new(0, 1, 2, 2),
                Point::new(6, 2),
                Some(InclusiveRect::new(1, 1, 6, 3)),
                u16::from(b'z'),
                BLUE,
            ),
            (
                InclusiveRect::new(7, 0, 8, 1),
                Point::new(4, 2),
                None,
                u16::from(b'Y'),
                RED,
            ),
            (
                InclusiveRect::new(-1, 0, 4, 3),
                Point::new(3, 1),
                Some(InclusiveRect::new(3, -1, 7, 9)),
                u16::from(b'y'),
                BLUE,
            ),
        ];
        let outputs = calls
            .into_iter()
            .map(|(source, target, clip, fill, attr)| {
                scroll_console_screen_buffer(
                    &mut state,
                    source,
                    target,
                    clip,
                    fill,
                    attr,
                    false,
                    rectangular,
                )
            })
            .collect();
        (state, outputs)
    }

    fn expected_final_cells() -> Vec<HostCharInfo> {
        [
            [
                ('A', RED),
                ('Z', RED),
                ('Z', RED),
                ('y', BLUE),
                ('y', BLUE),
                ('D', RED),
                ('c', BLUE),
                ('Y', RED),
            ],
            [
                ('E', RED),
                ('z', BLUE),
                ('z', BLUE),
                ('y', BLUE),
                ('A', RED),
                ('Z', RED),
                ('Z', RED),
                ('b', BLUE),
            ],
            [
                ('i', BLUE),
                ('z', BLUE),
                ('z', BLUE),
                ('y', BLUE),
                ('E', RED),
                ('z', BLUE),
                ('z', BLUE),
                ('f', BLUE),
            ],
            [
                ('m', BLUE),
                ('n', BLUE),
                ('M', RED),
                ('y', BLUE),
                ('i', BLUE),
                ('z', BLUE),
                ('z', BLUE),
                ('J', RED),
            ],
        ]
        .into_iter()
        .flatten()
        .map(|(ch, attr)| ci(ch, attr))
        .collect()
    }

    #[test]
    fn microsoft_vt_io_scroll_console_screen_buffer_w_matches_all_source_vectors_and_final_grid() {
        let mut no_op = initial_state();
        assert!(
            scroll_console_screen_buffer(
                &mut no_op,
                InclusiveRect::new(0, 0, -1, -1),
                Point::new(0, 0),
                None,
                u16::from(b' '),
                0,
                false,
                false,
            )
            .is_empty()
        );
        assert!(
            scroll_console_screen_buffer(
                &mut no_op,
                InclusiveRect::new(-10, -10, -9, -9),
                Point::new(0, 0),
                None,
                u16::from(b' '),
                0,
                false,
                false,
            )
            .is_empty()
        );

        let clear = scroll_console_screen_buffer(
            &mut no_op,
            InclusiveRect::new(0, 0, 7, 3),
            Point::new(0, -4),
            None,
            0,
            0,
            true,
            false,
        );
        assert_eq!(clear, CLEAR_SCREEN);

        let (state, outputs) = execute_reference_scroll_sequence(false);
        assert_eq!(
            outputs[0],
            b"\x1b\x37\x1b[1;2H\x1b[0;31;42mZZ\x1b[2;2H\x1b[0;31;42mZZ\x1b[3;6H\x1b[0;31;42mB\x1b[0;34;42ma\x1b[4;6H\x1b[0;31;42mF\x1b[0;34;42me\x1b\x38"
        );
        assert_eq!(
            outputs[1],
            b"\x1b\x37\x1b[2;2H\x1b[0;34;42mzz\x1b[3;2H\x1b[0;34;42mzz\x1b[3;7H\x1b[0;31;42mE\x1b[4;7H\x1b[0;34;42mi\x1b\x38"
        );
        assert_eq!(
            outputs[2],
            b"\x1b\x37\x1b[1;8H\x1b[0;31;42mY\x1b[2;8H\x1b[0;31;42mY\x1b[3;5H\x1b[0;34;42md\x1b[4;5H\x1b[0;34;42mh\x1b\x38"
        );
        assert_eq!(
            outputs[3],
            b"\x1b\x37\x1b[1;4H\x1b[0;34;42myy\x1b[2;4H\x1b[0;34;42myy\x1b[3;4H\x1b[0;34;42myy\x1b[4;4H\x1b[0;34;42myy\x1b[2;4H\x1b[0;34;42my\x1b[0;31;42mAZZ\x1b[0;34;42mb\x1b[3;4H\x1b[0;34;42my\x1b[0;31;42mE\x1b[0;34;42mzzf\x1b[4;4H\x1b[0;34;42myizz\x1b[0;31;42mJ\x1b\x38"
        );
        assert_eq!(state.cells(), expected_final_cells());
    }

    #[test]
    fn microsoft_vt_io_scroll_console_screen_buffer_w_deccra_matches_all_source_vectors_and_final_grid()
     {
        let (state, outputs) = execute_reference_scroll_sequence(true);
        assert_eq!(
            outputs[0],
            b"\x1b\x37\x1b[0;31;42m\x1b[1;2;2;3;;3;6$v\x1b[90;1;2;2;3$x\x1b\x38"
        );
        assert_eq!(
            outputs[1],
            b"\x1b\x37\x1b[0;34;42m\x1b[2;1;3;1;;3;7$v\x1b[122;2;2;3;3$x\x1b\x38"
        );
        assert_eq!(
            outputs[2],
            b"\x1b\x37\x1b[0;31;42m\x1b[1;8;2;8;;3;5$v\x1b[89;1;8;2;8$x\x1b\x38"
        );
        assert_eq!(
            outputs[3],
            b"\x1b\x37\x1b[0;34;42m\x1b[1;1;3;4;;2;5$v\x1b[121;1;4;1;5$x\x1b[121;2;4;4;4$x\x1b\x38"
        );
        assert_eq!(state.cells(), expected_final_cells());
    }
}
