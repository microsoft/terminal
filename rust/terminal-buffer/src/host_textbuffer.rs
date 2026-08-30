//! Portable orchestration for the remaining host `TextBufferTests.cpp` contracts.
//!
//! The lower-level Rust owners already store rows, attributes, shell marks and
//! ordinary host writes. This module closes deterministic behavior that crosses
//! those owners: right-edge VT cursor motion, multi-row word boundaries,
//! selection rectangles, the complete `CopyRequest` text matrix, and command
//! mark remapping while text reflows to a different width.

use crate::command_regions::CommandMark;
use crate::geometry::{InclusiveRect, Point};
use crate::row::{DbcsAttribute, DelimiterClass};
use crate::text_buffer::{TextBuffer, TextBufferPoint};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum WordBoundaryMode {
    Selection,
    Accessibility,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PlainTextRequest {
    pub start: TextBufferPoint,
    pub end: TextBufferPoint,
    pub block_selection: bool,
    pub include_crlf: bool,
    pub trim_trailing_whitespace: bool,
    pub format_wrapped_rows: bool,
}

#[must_use]
pub fn cursor_forward_clamped(
    cursor: TextBufferPoint,
    count: u16,
    buffer_width: u16,
) -> TextBufferPoint {
    TextBufferPoint::new(
        cursor
            .x
            .saturating_add(count)
            .min(buffer_width.saturating_sub(1)),
        cursor.y,
    )
}

#[must_use]
pub fn next_line_clamped(cursor: TextBufferPoint, buffer_height: u16) -> TextBufferPoint {
    TextBufferPoint::new(
        0,
        cursor
            .y
            .saturating_add(1)
            .min(buffer_height.saturating_sub(1)),
    )
}

#[must_use]
pub fn get_word_start(
    buffer: &TextBuffer,
    point: TextBufferPoint,
    word_delimiters: &[u16],
    mode: WordBoundaryMode,
) -> TextBufferPoint {
    let mut current = normalize_cell(buffer, point);
    match mode {
        WordBoundaryMode::Selection => {
            let class = class_at(buffer, current, word_delimiters);
            while let Some(previous) = previous_cell(buffer, current, false) {
                if class_at(buffer, previous, word_delimiters) != class {
                    break;
                }
                current = previous;
            }
            current
        }
        WordBoundaryMode::Accessibility => {
            if class_at(buffer, current, word_delimiters) != DelimiterClass::RegularChar {
                let mut earliest = current;
                let mut found_word = false;
                while let Some(previous) = previous_cell(buffer, current, true) {
                    earliest = previous;
                    current = previous;
                    if class_at(buffer, current, word_delimiters) == DelimiterClass::RegularChar {
                        found_word = true;
                        break;
                    }
                }
                if !found_word {
                    return earliest;
                }
            }
            while let Some(previous) = previous_cell(buffer, current, true) {
                if class_at(buffer, previous, word_delimiters) != DelimiterClass::RegularChar {
                    break;
                }
                current = previous;
            }
            current
        }
    }
}

#[must_use]
pub fn get_word_end(
    buffer: &TextBuffer,
    point: TextBufferPoint,
    word_delimiters: &[u16],
    mode: WordBoundaryMode,
) -> TextBufferPoint {
    let mut current = normalize_cell(buffer, point);
    match mode {
        WordBoundaryMode::Selection => {
            let class = class_at(buffer, current, word_delimiters);
            loop {
                let Some(next) = next_cell(buffer, current, false) else {
                    return row_end(buffer, current.y);
                };
                if class_at(buffer, next, word_delimiters) != class {
                    return next;
                }
                current = next;
            }
        }
        WordBoundaryMode::Accessibility => {
            if class_at(buffer, current, word_delimiters) == DelimiterClass::RegularChar {
                loop {
                    let Some(next) = next_cell(buffer, current, true) else {
                        return document_end(buffer);
                    };
                    if class_at(buffer, next, word_delimiters) != DelimiterClass::RegularChar {
                        current = next;
                        break;
                    }
                    current = next;
                }
            }
            loop {
                let Some(next) = next_cell(buffer, current, true) else {
                    return document_end(buffer);
                };
                if class_at(buffer, next, word_delimiters) == DelimiterClass::RegularChar {
                    return next;
                }
                current = next;
            }
        }
    }
}

#[must_use]
pub fn get_text_rects(
    buffer: &TextBuffer,
    start: TextBufferPoint,
    end: TextBufferPoint,
    block_selection: bool,
) -> Vec<InclusiveRect> {
    let (start, end) = normalize_span(buffer, start, end);
    let mut rects = Vec::new();
    if block_selection {
        let left = start.x.min(end.x).min(buffer.width().saturating_sub(1));
        let right = start.x.max(end.x).min(buffer.width().saturating_sub(1));
        for y in start.y..=end.y {
            let row = buffer.row(i32::from(y));
            let left = row.adjust_to_glyph_start(i32::from(left));
            let right = expand_inclusive_right(row, right, buffer.width());
            rects.push(InclusiveRect::new(
                i32::from(left),
                i32::from(y),
                i32::from(right),
                i32::from(y),
            ));
        }
    } else {
        for y in start.y..=end.y {
            let row = buffer.row(i32::from(y));
            let left = if y == start.y {
                row.adjust_to_glyph_start(i32::from(start.x))
            } else {
                0
            };
            let right = if y == end.y {
                expand_inclusive_right(row, end.x, buffer.width())
            } else {
                buffer.width().saturating_sub(1)
            };
            rects.push(InclusiveRect::new(
                i32::from(left),
                i32::from(y),
                i32::from(right),
                i32::from(y),
            ));
        }
    }
    rects
}

#[must_use]
pub fn get_plain_text(buffer: &TextBuffer, request: PlainTextRequest) -> Vec<u16> {
    let (start, end) = normalize_span(buffer, request.start, request.end);
    let block_left = start.x.min(end.x).min(buffer.width());
    let block_right = start.x.max(end.x).min(buffer.width());
    let physical_rows = request.block_selection || request.format_wrapped_rows;
    let mut output = Vec::new();

    for y in start.y..=end.y {
        let row = buffer.row(i32::from(y));
        let (begin, limit) = if request.block_selection {
            (block_left, block_right)
        } else {
            (
                if y == start.y {
                    start.x.min(buffer.width())
                } else {
                    0
                },
                if y == end.y {
                    end.x.min(buffer.width())
                } else {
                    buffer.width()
                },
            )
        };
        let mut text = row.text_range(i32::from(begin), i32::from(limit)).to_vec();
        if request.trim_trailing_whitespace && (physical_rows || !row.was_wrap_forced()) {
            trim_trailing_spaces(&mut text);
        }
        output.extend_from_slice(&text);
        if request.include_crlf && y != end.y && (physical_rows || !row.was_wrap_forced()) {
            output.extend_from_slice(&[u16::from(b'\r'), u16::from(b'\n')]);
        }
    }
    output
}

#[must_use]
pub fn reflow_command_marks(
    buffer: &TextBuffer,
    marks: &[CommandMark],
    new_width: u16,
) -> Vec<CommandMark> {
    assert!(new_width > 0, "reflow width must be positive");
    let layout = logical_layout(buffer);
    marks
        .iter()
        .map(|mark| CommandMark {
            start: remap_point(buffer, &layout, mark.start, new_width),
            end: remap_point(buffer, &layout, mark.end, new_width),
            command_end: mark
                .command_end
                .map(|point| remap_point(buffer, &layout, point, new_width)),
            output_end: mark
                .output_end
                .map(|point| remap_point(buffer, &layout, point, new_width)),
        })
        .collect()
}

#[derive(Debug, Clone, Copy)]
struct LogicalLayout {
    start_y: u16,
    end_y: u16,
    columns: u32,
}

fn normalize_cell(buffer: &TextBuffer, point: TextBufferPoint) -> TextBufferPoint {
    let y = point.y.min(buffer.height().saturating_sub(1));
    let row = buffer.row(i32::from(y));
    let x = point.x.min(buffer.width().saturating_sub(1));
    TextBufferPoint::new(row.adjust_to_glyph_start(i32::from(x)), y)
}

fn class_at(
    buffer: &TextBuffer,
    point: TextBufferPoint,
    word_delimiters: &[u16],
) -> DelimiterClass {
    buffer
        .row(i32::from(point.y))
        .delimiter_class_at(i32::from(point.x), word_delimiters)
}

fn previous_cell(
    buffer: &TextBuffer,
    point: TextBufferPoint,
    cross_unwrapped: bool,
) -> Option<TextBufferPoint> {
    let row = buffer.row(i32::from(point.y));
    if point.x > 0 {
        return Some(TextBufferPoint::new(
            row.navigate_to_previous(i32::from(point.x)),
            point.y,
        ));
    }
    if point.y == 0 {
        return None;
    }
    let previous_y = point.y - 1;
    let previous_row = buffer.row(i32::from(previous_y));
    if !cross_unwrapped && !previous_row.was_wrap_forced() {
        return None;
    }
    Some(TextBufferPoint::new(
        previous_row.adjust_to_glyph_start(i32::from(buffer.width().saturating_sub(1))),
        previous_y,
    ))
}

fn next_cell(
    buffer: &TextBuffer,
    point: TextBufferPoint,
    cross_unwrapped: bool,
) -> Option<TextBufferPoint> {
    let row = buffer.row(i32::from(point.y));
    let next = row.navigate_to_next(i32::from(point.x));
    if next < buffer.width() {
        return Some(TextBufferPoint::new(next, point.y));
    }
    if point.y.saturating_add(1) >= buffer.height() || (!cross_unwrapped && !row.was_wrap_forced())
    {
        return None;
    }
    Some(TextBufferPoint::new(0, point.y + 1))
}

fn row_end(buffer: &TextBuffer, y: u16) -> TextBufferPoint {
    TextBufferPoint::new(buffer.width(), y)
}

fn document_end(buffer: &TextBuffer) -> TextBufferPoint {
    TextBufferPoint::new(buffer.width(), buffer.height().saturating_sub(1))
}

fn normalize_span(
    buffer: &TextBuffer,
    start: TextBufferPoint,
    end: TextBufferPoint,
) -> (TextBufferPoint, TextBufferPoint) {
    let clamp = |point: TextBufferPoint| {
        TextBufferPoint::new(
            point.x.min(buffer.width()),
            point.y.min(buffer.height().saturating_sub(1)),
        )
    };
    let start = clamp(start);
    let end = clamp(end);
    if start <= end {
        (start, end)
    } else {
        (end, start)
    }
}

fn expand_inclusive_right(row: &crate::row::Row, x: u16, width: u16) -> u16 {
    let x = x.min(width.saturating_sub(1));
    if row.dbcs_attribute_at(i32::from(x)) == DbcsAttribute::Trailing {
        x.saturating_add(1).min(width.saturating_sub(1))
    } else {
        x
    }
}

fn trim_trailing_spaces(text: &mut Vec<u16>) {
    while text.last() == Some(&u16::from(b' ')) {
        text.pop();
    }
}

fn logical_layout(buffer: &TextBuffer) -> Vec<LogicalLayout> {
    let mut layout = Vec::new();
    let mut start_y = 0_u16;
    let mut columns = 0_u32;
    for y in 0..buffer.height() {
        let row = buffer.row(i32::from(y));
        let extent = if row.was_wrap_forced() {
            row.readable_column_count()
        } else {
            row.measure_right()
        };
        columns = columns.saturating_add(u32::from(extent));
        if !row.was_wrap_forced() {
            layout.push(LogicalLayout {
                start_y,
                end_y: y,
                columns,
            });
            start_y = y.saturating_add(1);
            columns = 0;
        }
    }
    if start_y < buffer.height() {
        layout.push(LogicalLayout {
            start_y,
            end_y: buffer.height().saturating_sub(1),
            columns,
        });
    }
    layout
}

fn remap_point(
    buffer: &TextBuffer,
    layout: &[LogicalLayout],
    point: Point,
    new_width: u16,
) -> Point {
    let y = point
        .y
        .clamp(0, i32::from(buffer.height()).saturating_sub(1));
    let x = point.x.clamp(0, i32::from(buffer.width()));
    let y_u16 = u16::try_from(y).unwrap_or_default();
    let line_index = layout
        .iter()
        .position(|line| y_u16 >= line.start_y && y_u16 <= line.end_y)
        .unwrap_or(layout.len().saturating_sub(1));

    let mut new_start_y = 0_u32;
    for line in &layout[..line_index] {
        new_start_y = new_start_y.saturating_add(rows_for_columns(line.columns, new_width));
    }
    let line = layout[line_index];
    let mut offset = 0_u32;
    for source_y in line.start_y..y_u16 {
        offset = offset.saturating_add(u32::from(
            buffer.row(i32::from(source_y)).readable_column_count(),
        ));
    }
    offset = offset.saturating_add(u32::try_from(x).unwrap_or_default());

    Point::new(
        i32::try_from(offset % u32::from(new_width)).unwrap_or(i32::MAX),
        i32::try_from(new_start_y.saturating_add(offset / u32::from(new_width)))
            .unwrap_or(i32::MAX),
    )
}

fn rows_for_columns(columns: u32, width: u16) -> u32 {
    if columns == 0 {
        1
    } else {
        columns.saturating_add(u32::from(width).saturating_sub(1)) / u32::from(width)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::command_regions::CommandRegionState;
    use crate::host_write::HostWriteState;
    use crate::text_attribute::TextAttribute;

    #[derive(Clone, Copy)]
    struct WordCase {
        point: (u16, u16),
        selection: (u16, u16),
        accessibility: (u16, u16),
    }

    fn p(value: (u16, u16)) -> TextBufferPoint {
        TextBufferPoint::new(value.0, value.1)
    }

    fn write_ascii_row(buffer: &mut TextBuffer, y: u16, text: &str, wrap: bool) {
        let row = buffer.row_mut(i32::from(y));
        for (x, byte) in text.bytes().enumerate() {
            row.replace_glyph(
                i32::try_from(x).expect("fixture column fits i32"),
                1,
                &[u16::from(byte)],
            )
            .expect("fixture text fits row");
        }
        row.set_wrap_forced(wrap);
    }

    fn assert_word_cases(buffer: &TextBuffer, cases: &[WordCase], start: bool) {
        let delimiters = [u16::from(b' ')];
        for case in cases {
            for (mode, expected) in [
                (WordBoundaryMode::Selection, case.selection),
                (WordBoundaryMode::Accessibility, case.accessibility),
            ] {
                let actual = if start {
                    get_word_start(buffer, p(case.point), &delimiters, mode)
                } else {
                    get_word_end(buffer, p(case.point), &delimiters, mode)
                };
                assert_eq!(
                    actual,
                    p(expected),
                    "word case {:?} mode {mode:?}",
                    case.point
                );
            }
        }
    }

    #[test]
    fn microsoft_host_text_buffer_backspace_right_side_vt_contract() {
        let attr = TextAttribute::default();
        let mut buffer = TextBuffer::new(6, 2, attr).unwrap();
        let cub = cursor_forward_clamped(TextBufferPoint::new(0, 0), 1000, buffer.width());
        let mut state = HostWriteState::new(cub, attr);
        let sequence = "x\u{8}y".encode_utf16().collect::<Vec<_>>();
        state.write_vt(&mut buffer, &sequence).unwrap();
        let post = next_line_clamped(state.cursor(), buffer.height());
        assert_eq!(post, TextBufferPoint::new(0, 1));
        assert_eq!(buffer.row(0).glyph_at(4), &[u16::from(b'y')]);
        assert_eq!(buffer.row(0).glyph_at(5), &[u16::from(b'x')]);
    }

    #[test]
    fn microsoft_host_text_buffer_get_word_boundaries_matches_all_source_vectors() {
        let attr = TextAttribute::default();
        let mut buffer = TextBuffer::new(80, 9001, attr).unwrap();
        write_ascii_row(&mut buffer, 0, "word other", false);
        write_ascii_row(&mut buffer, 1, "  more   words", false);

        let start_cases = [
            WordCase {
                point: (0, 0),
                selection: (0, 0),
                accessibility: (0, 0),
            },
            WordCase {
                point: (1, 0),
                selection: (0, 0),
                accessibility: (0, 0),
            },
            WordCase {
                point: (3, 0),
                selection: (0, 0),
                accessibility: (0, 0),
            },
            WordCase {
                point: (4, 0),
                selection: (4, 0),
                accessibility: (0, 0),
            },
            WordCase {
                point: (5, 0),
                selection: (5, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (6, 0),
                selection: (5, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (20, 0),
                selection: (10, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (79, 0),
                selection: (10, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (0, 1),
                selection: (0, 1),
                accessibility: (5, 0),
            },
            WordCase {
                point: (1, 1),
                selection: (0, 1),
                accessibility: (5, 0),
            },
            WordCase {
                point: (2, 1),
                selection: (2, 1),
                accessibility: (2, 1),
            },
            WordCase {
                point: (3, 1),
                selection: (2, 1),
                accessibility: (2, 1),
            },
            WordCase {
                point: (5, 1),
                selection: (2, 1),
                accessibility: (2, 1),
            },
            WordCase {
                point: (6, 1),
                selection: (6, 1),
                accessibility: (2, 1),
            },
            WordCase {
                point: (7, 1),
                selection: (6, 1),
                accessibility: (2, 1),
            },
            WordCase {
                point: (9, 1),
                selection: (9, 1),
                accessibility: (9, 1),
            },
            WordCase {
                point: (10, 1),
                selection: (9, 1),
                accessibility: (9, 1),
            },
            WordCase {
                point: (20, 1),
                selection: (14, 1),
                accessibility: (9, 1),
            },
            WordCase {
                point: (79, 1),
                selection: (14, 1),
                accessibility: (9, 1),
            },
        ];
        assert_word_cases(&buffer, &start_cases, true);

        let end_cases = [
            WordCase {
                point: (0, 0),
                selection: (4, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (1, 0),
                selection: (4, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (3, 0),
                selection: (4, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (4, 0),
                selection: (5, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (5, 0),
                selection: (10, 0),
                accessibility: (2, 1),
            },
            WordCase {
                point: (6, 0),
                selection: (10, 0),
                accessibility: (2, 1),
            },
            WordCase {
                point: (20, 0),
                selection: (80, 0),
                accessibility: (2, 1),
            },
            WordCase {
                point: (79, 0),
                selection: (80, 0),
                accessibility: (2, 1),
            },
            WordCase {
                point: (0, 1),
                selection: (2, 1),
                accessibility: (2, 1),
            },
            WordCase {
                point: (1, 1),
                selection: (2, 1),
                accessibility: (2, 1),
            },
            WordCase {
                point: (2, 1),
                selection: (6, 1),
                accessibility: (9, 1),
            },
            WordCase {
                point: (3, 1),
                selection: (6, 1),
                accessibility: (9, 1),
            },
            WordCase {
                point: (5, 1),
                selection: (6, 1),
                accessibility: (9, 1),
            },
            WordCase {
                point: (6, 1),
                selection: (9, 1),
                accessibility: (9, 1),
            },
            WordCase {
                point: (7, 1),
                selection: (9, 1),
                accessibility: (9, 1),
            },
            WordCase {
                point: (9, 1),
                selection: (14, 1),
                accessibility: (80, 9000),
            },
            WordCase {
                point: (10, 1),
                selection: (14, 1),
                accessibility: (80, 9000),
            },
            WordCase {
                point: (20, 1),
                selection: (80, 1),
                accessibility: (80, 9000),
            },
            WordCase {
                point: (79, 1),
                selection: (80, 1),
                accessibility: (80, 9000),
            },
        ];
        assert_word_cases(&buffer, &end_cases, false);

        let mut wrapped = TextBuffer::new(10, 6, attr).unwrap();
        for (y, text, wrap) in [
            (0, "this wordi", true),
            (1, "swrapped  ", false),
            (2, "notwrapped", false),
            (3, "spaces    ", true),
            (4, "    wrappe", true),
            (5, "d reachEOB", true),
        ] {
            write_ascii_row(&mut wrapped, y, text, wrap);
        }
        let wrapped_start = [
            WordCase {
                point: (0, 0),
                selection: (0, 0),
                accessibility: (0, 0),
            },
            WordCase {
                point: (1, 0),
                selection: (0, 0),
                accessibility: (0, 0),
            },
            WordCase {
                point: (4, 0),
                selection: (4, 0),
                accessibility: (0, 0),
            },
            WordCase {
                point: (5, 0),
                selection: (5, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (7, 0),
                selection: (5, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (4, 1),
                selection: (5, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (7, 1),
                selection: (5, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (9, 1),
                selection: (8, 1),
                accessibility: (5, 0),
            },
            WordCase {
                point: (0, 2),
                selection: (0, 2),
                accessibility: (0, 2),
            },
            WordCase {
                point: (9, 2),
                selection: (0, 2),
                accessibility: (0, 2),
            },
            WordCase {
                point: (0, 3),
                selection: (0, 3),
                accessibility: (0, 2),
            },
            WordCase {
                point: (7, 3),
                selection: (6, 3),
                accessibility: (0, 2),
            },
            WordCase {
                point: (1, 4),
                selection: (6, 3),
                accessibility: (0, 2),
            },
            WordCase {
                point: (4, 4),
                selection: (4, 4),
                accessibility: (4, 4),
            },
            WordCase {
                point: (8, 4),
                selection: (4, 4),
                accessibility: (4, 4),
            },
            WordCase {
                point: (0, 5),
                selection: (4, 4),
                accessibility: (4, 4),
            },
            WordCase {
                point: (1, 5),
                selection: (1, 5),
                accessibility: (4, 4),
            },
            WordCase {
                point: (9, 5),
                selection: (2, 5),
                accessibility: (2, 5),
            },
        ];
        assert_word_cases(&wrapped, &wrapped_start, true);
        let wrapped_end = [
            WordCase {
                point: (0, 0),
                selection: (4, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (1, 0),
                selection: (4, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (4, 0),
                selection: (5, 0),
                accessibility: (5, 0),
            },
            WordCase {
                point: (5, 0),
                selection: (8, 1),
                accessibility: (0, 2),
            },
            WordCase {
                point: (7, 0),
                selection: (8, 1),
                accessibility: (0, 2),
            },
            WordCase {
                point: (4, 1),
                selection: (8, 1),
                accessibility: (0, 2),
            },
            WordCase {
                point: (7, 1),
                selection: (8, 1),
                accessibility: (0, 2),
            },
            WordCase {
                point: (9, 1),
                selection: (10, 1),
                accessibility: (0, 2),
            },
            WordCase {
                point: (0, 2),
                selection: (10, 2),
                accessibility: (4, 4),
            },
            WordCase {
                point: (9, 2),
                selection: (10, 2),
                accessibility: (4, 4),
            },
            WordCase {
                point: (0, 3),
                selection: (6, 3),
                accessibility: (4, 4),
            },
            WordCase {
                point: (7, 3),
                selection: (4, 4),
                accessibility: (4, 4),
            },
            WordCase {
                point: (1, 4),
                selection: (4, 4),
                accessibility: (4, 4),
            },
            WordCase {
                point: (4, 4),
                selection: (1, 5),
                accessibility: (2, 5),
            },
            WordCase {
                point: (8, 4),
                selection: (1, 5),
                accessibility: (2, 5),
            },
            WordCase {
                point: (0, 5),
                selection: (1, 5),
                accessibility: (2, 5),
            },
            WordCase {
                point: (1, 5),
                selection: (2, 5),
                accessibility: (2, 5),
            },
            WordCase {
                point: (4, 5),
                selection: (10, 5),
                accessibility: (10, 5),
            },
            WordCase {
                point: (9, 5),
                selection: (10, 5),
                accessibility: (10, 5),
            },
        ];
        assert_word_cases(&wrapped, &wrapped_end, false);
    }

    #[test]
    fn microsoft_host_text_buffer_get_text_rects_matches_block_and_line_vectors() {
        let attr = TextAttribute::default();
        let mut buffer = TextBuffer::new(20, 50, attr).unwrap();
        write_ascii_row(&mut buffer, 0, "0123456789", false);
        write_ascii_row(&mut buffer, 4, "0123456789", false);
        let burrito = [0xd83c, 0xdf2f];
        {
            let row = buffer.row_mut(1);
            row.replace_glyph(0, 1, &[u16::from(b' ')]).unwrap();
            row.replace_glyph(1, 2, &burrito).unwrap();
            for (x, byte) in [(3, b'3'), (4, b'4'), (5, b'5'), (6, b'6')] {
                row.replace_glyph(x, 1, &[u16::from(byte)]).unwrap();
            }
            row.replace_glyph(7, 2, &burrito).unwrap();
        }
        {
            let row = buffer.row_mut(2);
            row.replace_glyph(0, 1, &[u16::from(b' ')]).unwrap();
            row.replace_glyph(1, 1, &[u16::from(b' ')]).unwrap();
            row.replace_glyph(2, 2, &burrito).unwrap();
            row.replace_glyph(4, 1, &[u16::from(b'4')]).unwrap();
            row.replace_glyph(5, 1, &[u16::from(b'5')]).unwrap();
            row.replace_glyph(6, 2, &burrito).unwrap();
        }
        {
            let row = buffer.row_mut(3);
            row.replace_glyph(0, 2, &burrito).unwrap();
            for (x, byte) in [
                (2, b'2'),
                (3, b'3'),
                (4, b'4'),
                (5, b'5'),
                (6, b'6'),
                (7, b'7'),
            ] {
                row.replace_glyph(x, 1, &[u16::from(byte)]).unwrap();
            }
            row.replace_glyph(8, 2, &burrito).unwrap();
        }
        let start = TextBufferPoint::new(1, 0);
        let end = TextBufferPoint::new(8, 4);
        assert_eq!(
            get_text_rects(&buffer, start, end, true),
            vec![
                InclusiveRect::new(1, 0, 8, 0),
                InclusiveRect::new(1, 1, 9, 1),
                InclusiveRect::new(1, 2, 8, 2),
                InclusiveRect::new(0, 3, 8, 3),
                InclusiveRect::new(1, 4, 8, 4),
            ]
        );
        assert_eq!(
            get_text_rects(&buffer, start, end, false),
            vec![
                InclusiveRect::new(1, 0, 19, 0),
                InclusiveRect::new(0, 1, 19, 1),
                InclusiveRect::new(0, 2, 19, 2),
                InclusiveRect::new(0, 3, 19, 3),
                InclusiveRect::new(0, 4, 8, 4),
            ]
        );
    }

    fn expected_unwrapped(block: bool, crlf: bool, trim: bool) -> &'static str {
        match (crlf, trim, block) {
            (true, true, _) => "12345\r\n  345\r\n123\r\n  3\r\n",
            (true, false, true) => "12345\r\n  345\r\n123  \r\n  3  \r\n     ",
            (true, false, false) => "12345     \r\n  345     \r\n123       \r\n  3       \r\n     ",
            (false, true, _) => "12345  345123  3",
            (false, false, true) => "12345  345123    3       ",
            (false, false, false) => "12345       345     123         3            ",
        }
    }

    fn expected_wrapped(block: bool, crlf: bool, trim: bool) -> &'static str {
        match (block, crlf, trim) {
            (true, true, true) => "12345\r\n67\r\n  345\r\n123\r\n\r\n",
            (true, true, false) => "12345\r\n67   \r\n  345\r\n123  \r\n     \r\n     ",
            (true, false, true) => "1234567  345123",
            (true, false, false) => "1234567     345123            ",
            (false, true, true) => "1234567\r\n  345123  \r\n",
            (false, true, false) => "1234567   \r\n  345123       \r\n     ",
            (false, false, true) => "1234567  345123  ",
            (false, false, false) => "1234567     345123            ",
        }
    }

    #[test]
    fn microsoft_host_text_buffer_get_plain_text_matches_complete_copy_request_matrix() {
        let attr = TextAttribute::default();
        let mut unwrapped = TextBuffer::new(10, 20, attr).unwrap();
        for (y, text) in [(0, "12345"), (1, "  345"), (2, "123  "), (3, "  3  ")] {
            write_ascii_row(&mut unwrapped, y, text, false);
        }
        let mut wrapped = TextBuffer::new(5, 20, attr).unwrap();
        for (y, text, wrap) in [
            (0, "12345", true),
            (1, "67   ", false),
            (2, "  345", true),
            (3, "123  ", true),
            (4, "     ", false),
            (5, "     ", false),
        ] {
            write_ascii_row(&mut wrapped, y, text, wrap);
        }
        for block in [false, true] {
            for crlf in [false, true] {
                for trim in [false, true] {
                    let request = PlainTextRequest {
                        start: TextBufferPoint::new(0, 0),
                        end: TextBufferPoint::new(5, 4),
                        block_selection: block,
                        include_crlf: crlf,
                        trim_trailing_whitespace: trim,
                        format_wrapped_rows: false,
                    };
                    assert_eq!(
                        String::from_utf16_lossy(&get_plain_text(&unwrapped, request)),
                        expected_unwrapped(block, crlf, trim)
                    );
                    let request = PlainTextRequest {
                        start: TextBufferPoint::new(0, 0),
                        end: TextBufferPoint::new(5, 5),
                        block_selection: block,
                        include_crlf: crlf,
                        trim_trailing_whitespace: trim,
                        format_wrapped_rows: block,
                    };
                    assert_eq!(
                        String::from_utf16_lossy(&get_plain_text(&wrapped, request)),
                        expected_wrapped(block, crlf, trim)
                    );
                }
            }
        }
    }

    fn write_prompt(state: &mut CommandRegionState) {
        state.command_finished();
        state.prompt_start();
        state.write_text("PWSH C:\\> ");
        state.command_start();
    }

    fn reflow_fixture() -> (TextBuffer, Vec<CommandMark>) {
        let attr = TextAttribute::default();
        let mut state = CommandRegionState::new(80);
        write_prompt(&mut state);
        state.write_text("Foo-bar");
        state.command_end();
        state.crlf();
        state.write_text("This is some text     ");
        state.crlf();
        state.write_text("with varying amounts  ");
        state.crlf();
        state.write_text("of whitespace");
        state.crlf();
        write_prompt(&mut state);
        state.write_text(&"F".repeat(80));
        state.command_end();
        state.crlf();
        state.write_text("This is more text     ");
        state.crlf();
        write_prompt(&mut state);
        state.write_text("yikes?");
        state.command_end();
        let mut buffer = TextBuffer::new(80, 10, attr).unwrap();
        for (y, len, wrap) in [
            (0, 17, false),
            (1, 22, false),
            (2, 22, false),
            (3, 13, false),
            (4, 80, true),
            (5, 10, false),
            (6, 22, false),
            (7, 16, false),
        ] {
            write_ascii_row(&mut buffer, y, &"X".repeat(len), wrap);
        }
        (buffer, state.marks().to_vec())
    }

    #[test]
    fn microsoft_host_text_buffer_reflow_prompt_regions_matches_all_dx_variants() {
        let (buffer, marks) = reflow_fixture();
        assert_eq!(marks[0].start, Point::new(0, 0));
        assert_eq!(marks[0].end, Point::new(10, 0));
        assert_eq!(marks[0].command_end, Some(Point::new(17, 0)));
        assert_eq!(marks[0].output_end, Some(Point::new(13, 3)));
        assert_eq!(marks[1].start, Point::new(0, 4));
        assert_eq!(marks[1].end, Point::new(10, 4));
        assert_eq!(marks[1].command_end, Some(Point::new(10, 5)));
        assert_eq!(marks[1].output_end, Some(Point::new(22, 6)));
        for (dx, command_end, output_y, third_y) in [
            (-15, Point::new(25, 5), 6, 7),
            (-1, Point::new(11, 5), 6, 7),
            (0, Point::new(10, 5), 6, 7),
            (1, Point::new(9, 5), 6, 7),
            (15, Point::new(90, 4), 5, 6),
        ] {
            let mapped = reflow_command_marks(&buffer, &marks, u16::try_from(80 + dx).unwrap());
            assert_eq!(mapped[0].start, Point::new(0, 0));
            assert_eq!(mapped[0].end, Point::new(10, 0));
            assert_eq!(mapped[0].command_end, Some(Point::new(17, 0)));
            assert_eq!(mapped[0].output_end, Some(Point::new(13, 3)));
            assert_eq!(mapped[1].start, Point::new(0, 4));
            assert_eq!(mapped[1].end, Point::new(10, 4));
            assert_eq!(mapped[1].command_end, Some(command_end));
            assert_eq!(mapped[1].output_end, Some(Point::new(22, output_y)));
            assert_eq!(mapped[2].start, Point::new(0, third_y));
            assert_eq!(mapped[2].end, Point::new(10, third_y));
            assert!(mapped[2].command_end.is_some());
            assert!(mapped[2].output_end.is_none());
        }
    }
}
