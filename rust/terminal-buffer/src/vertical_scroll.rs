//! Safe vertical screen-buffer scrolling over resolved viewport and DEC margins.
//!
//! This owner composes the existing overlap-safe rectangular mover with the
//! cursor/margin rules needed by SU, SD, IL, DL and RI. VT parsing and renderer
//! notification remain outside this module.

use crate::rect_ops::{ScreenRect, fill_rect, scroll_rect};
use crate::row::RowError;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferPoint};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ScrollCursor {
    pub x: u16,
    pub y: u16,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct VerticalScrollState {
    width: u16,
    viewport_top: u16,
    viewport_bottom: u16,
    vertical_margins: Option<(u16, u16)>,
    horizontal_margins: Option<(u16, u16)>,
    cursor: ScrollCursor,
}

impl VerticalScrollState {
    #[must_use]
    pub fn new(width: u16, viewport_top: u16, viewport_bottom: u16) -> Self {
        assert!(width > 0);
        assert!(viewport_top < viewport_bottom);
        Self {
            width,
            viewport_top,
            viewport_bottom,
            vertical_margins: None,
            horizontal_margins: None,
            cursor: ScrollCursor {
                x: 0,
                y: viewport_top,
            },
        }
    }

    pub fn set_vertical_margins(&mut self, top: u16, bottom: u16) {
        assert!(self.viewport_top <= top && top < bottom && bottom <= self.viewport_bottom);
        self.vertical_margins = Some((top, bottom));
    }

    pub fn clear_vertical_margins(&mut self) {
        self.vertical_margins = None;
    }

    pub fn set_horizontal_margins(&mut self, left: u16, right: u16) {
        assert!(left < right && right <= self.width);
        self.horizontal_margins = Some((left, right));
    }

    pub fn clear_horizontal_margins(&mut self) {
        self.horizontal_margins = None;
    }

    pub fn set_cursor(&mut self, x: u16, y: u16) {
        self.cursor = ScrollCursor { x, y };
    }

    #[must_use]
    pub const fn cursor(&self) -> ScrollCursor {
        self.cursor
    }

    pub fn scroll_up(
        &self,
        buffer: &mut TextBuffer,
        count: u16,
        active_attribute: TextAttribute,
    ) -> Result<(), RowError> {
        scroll_up_rect(buffer, self.scroll_rect(), count, active_attribute)
    }

    pub fn scroll_down(
        &self,
        buffer: &mut TextBuffer,
        count: u16,
        active_attribute: TextAttribute,
    ) -> Result<(), RowError> {
        scroll_down_rect(buffer, self.scroll_rect(), count, active_attribute)
    }

    pub fn insert_lines(
        &mut self,
        buffer: &mut TextBuffer,
        count: u16,
        active_attribute: TextAttribute,
    ) -> Result<(), RowError> {
        let region = self.scroll_rect();
        if self.cursor.y < region.top || self.cursor.y >= region.bottom {
            return Ok(());
        }
        let insertion = ScreenRect::new(region.left, self.cursor.y, region.right, region.bottom);
        scroll_down_rect(buffer, insertion, count, active_attribute)?;
        self.cursor.x = region.left;
        Ok(())
    }

    pub fn delete_lines(
        &mut self,
        buffer: &mut TextBuffer,
        count: u16,
        active_attribute: TextAttribute,
    ) -> Result<(), RowError> {
        let region = self.scroll_rect();
        if self.cursor.y < region.top || self.cursor.y >= region.bottom {
            return Ok(());
        }
        let deletion = ScreenRect::new(region.left, self.cursor.y, region.right, region.bottom);
        scroll_up_rect(buffer, deletion, count, active_attribute)?;
        self.cursor.x = region.left;
        Ok(())
    }

    pub fn reverse_index(
        &mut self,
        buffer: &mut TextBuffer,
        active_attribute: TextAttribute,
    ) -> Result<(), RowError> {
        let region = self.scroll_rect();
        if self.cursor.y == region.top {
            scroll_down_rect(buffer, region, 1, active_attribute)
        } else {
            self.cursor.y = self.cursor.y.saturating_sub(1);
            Ok(())
        }
    }

    fn scroll_rect(self) -> ScreenRect {
        let (top, bottom) = self
            .vertical_margins
            .unwrap_or((self.viewport_top, self.viewport_bottom));
        let (left, right) = self.horizontal_margins.unwrap_or((0, self.width));
        ScreenRect::new(left, top, right, bottom)
    }
}

fn standard_erase(mut attribute: TextAttribute) -> TextAttribute {
    attribute.set_standard_erase();
    attribute
}

fn scroll_up_rect(
    buffer: &mut TextBuffer,
    region: ScreenRect,
    count: u16,
    active_attribute: TextAttribute,
) -> Result<(), RowError> {
    let height = region.height();
    let count = count.max(1).min(height);
    let erase = standard_erase(active_attribute);
    if count == height {
        return fill_rect(buffer, region, u16::from(b' '), erase);
    }
    scroll_rect(
        buffer,
        ScreenRect::new(region.left, region.top + count, region.right, region.bottom),
        TextBufferPoint::new(region.left, region.top),
        erase,
    )?;
    fill_rect(
        buffer,
        ScreenRect::new(
            region.left,
            region.bottom - count,
            region.right,
            region.bottom,
        ),
        u16::from(b' '),
        erase,
    )
}

fn scroll_down_rect(
    buffer: &mut TextBuffer,
    region: ScreenRect,
    count: u16,
    active_attribute: TextAttribute,
) -> Result<(), RowError> {
    let height = region.height();
    let count = count.max(1).min(height);
    let erase = standard_erase(active_attribute);
    if count == height {
        return fill_rect(buffer, region, u16::from(b' '), erase);
    }
    scroll_rect(
        buffer,
        ScreenRect::new(region.left, region.top, region.right, region.bottom - count),
        TextBufferPoint::new(region.left, region.top + count),
        erase,
    )?;
    fill_rect(
        buffer,
        ScreenRect::new(region.left, region.top, region.right, region.top + count),
        u16::from(b' '),
        erase,
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::text_color::{Rgb, TextColor};

    fn fill_line(buffer: &mut TextBuffer, y: u16, ch: u8, attr: TextAttribute) {
        fill_rect(
            buffer,
            ScreenRect::new(0, y, buffer.width(), y + 1),
            u16::from(ch),
            attr,
        )
        .unwrap();
    }

    fn assert_cell(buffer: &TextBuffer, x: u16, y: u16, ch: u8, attr: TextAttribute) {
        let row = buffer.row(i32::from(y));
        assert_eq!(row.glyph_at(i32::from(x)), &[u16::from(ch)]);
        assert_eq!(row.attribute_at(i32::from(x)), attr);
    }

    fn common_fixture() -> (TextBuffer, VerticalScrollState, TextAttribute) {
        let attr = TextAttribute::default();
        let mut buffer = TextBuffer::new(8, 6, attr).unwrap();
        for (y, ch) in [b'A', b'5', b'6', b'7', b' ', b'B'].into_iter().enumerate() {
            fill_line(&mut buffer, y as u16, ch, attr);
        }
        let mut state = VerticalScrollState::new(8, 0, 6);
        state.set_vertical_margins(1, 5);
        state.set_cursor(0, 4);
        (buffer, state, attr)
    }

    #[test]
    fn microsoft_screen_buffer_scroll_operations_contract() {
        let attr = TextAttribute::default();
        for count in [1, 2, 5] {
            let mut buffer = TextBuffer::new(10, 20, attr).unwrap();
            for y in 0..20 {
                fill_line(&mut buffer, y, b'Z', attr);
            }
            let state = VerticalScrollState::new(10, 5, 15);
            state.scroll_up(&mut buffer, count, attr).unwrap();
            for y in 0..5 {
                assert_cell(&buffer, 0, y, b'Z', attr);
            }
            for y in 15..20 {
                assert_cell(&buffer, 0, y, b'Z', attr);
            }
        }
    }

    #[test]
    fn microsoft_screen_buffer_scroll_up_in_margins_contract() {
        let (mut buffer, state, attr) = common_fixture();
        state.scroll_up(&mut buffer, 1, attr).unwrap();
        for (y, ch) in [
            (0, b'A'),
            (1, b'6'),
            (2, b'7'),
            (3, b' '),
            (4, b' '),
            (5, b'B'),
        ] {
            assert_cell(&buffer, 0, y, ch, attr);
        }

        let (mut buffer, mut state, attr) = common_fixture();
        state.set_horizontal_margins(2, 6);
        state.scroll_up(&mut buffer, 1, attr).unwrap();
        assert_cell(&buffer, 0, 1, b'5', attr);
        assert_cell(&buffer, 2, 1, b'6', attr);
        assert_cell(&buffer, 6, 1, b'5', attr);
    }

    #[test]
    fn microsoft_screen_buffer_scroll_down_in_margins_contract() {
        let (mut buffer, state, attr) = common_fixture();
        state.scroll_down(&mut buffer, 1, attr).unwrap();
        for (y, ch) in [
            (0, b'A'),
            (1, b' '),
            (2, b'5'),
            (3, b'6'),
            (4, b'7'),
            (5, b'B'),
        ] {
            assert_cell(&buffer, 0, y, ch, attr);
        }

        let (mut buffer, mut state, attr) = common_fixture();
        state.set_horizontal_margins(2, 6);
        state.scroll_down(&mut buffer, 1, attr).unwrap();
        assert_cell(&buffer, 0, 1, b'5', attr);
        assert_cell(&buffer, 2, 1, b' ', attr);
        assert_cell(&buffer, 6, 1, b'5', attr);
    }

    #[test]
    fn microsoft_screen_buffer_insert_lines_in_margins_contract() {
        let (mut buffer, mut state, attr) = common_fixture();
        state.set_cursor(4, 2);
        state.insert_lines(&mut buffer, 2, attr).unwrap();
        assert_eq!(state.cursor(), ScrollCursor { x: 0, y: 2 });
        assert_cell(&buffer, 0, 2, b' ', attr);
        assert_cell(&buffer, 0, 3, b' ', attr);
        assert_cell(&buffer, 0, 4, b'6', attr);

        let (mut buffer, mut state, attr) = common_fixture();
        state.set_horizontal_margins(2, 6);
        state.set_cursor(4, 2);
        state.insert_lines(&mut buffer, 2, attr).unwrap();
        assert_eq!(state.cursor(), ScrollCursor { x: 2, y: 2 });
        assert_cell(&buffer, 1, 2, b'6', attr);
        assert_cell(&buffer, 2, 2, b' ', attr);
        assert_cell(&buffer, 6, 2, b'6', attr);
    }

    #[test]
    fn microsoft_screen_buffer_delete_lines_in_margins_contract() {
        let (mut buffer, mut state, attr) = common_fixture();
        state.set_cursor(4, 2);
        state.delete_lines(&mut buffer, 2, attr).unwrap();
        assert_eq!(state.cursor(), ScrollCursor { x: 0, y: 2 });
        assert_cell(&buffer, 0, 2, b' ', attr);
        assert_cell(&buffer, 0, 3, b' ', attr);
        assert_cell(&buffer, 0, 4, b' ', attr);

        let (mut buffer, mut state, attr) = common_fixture();
        state.set_horizontal_margins(2, 6);
        state.set_cursor(4, 2);
        state.delete_lines(&mut buffer, 2, attr).unwrap();
        assert_eq!(state.cursor(), ScrollCursor { x: 2, y: 2 });
        assert_cell(&buffer, 1, 2, b'6', attr);
        assert_cell(&buffer, 2, 2, b' ', attr);
        assert_cell(&buffer, 6, 2, b'6', attr);
    }

    #[test]
    fn microsoft_screen_buffer_reverse_line_feed_in_margins_contract() {
        let (mut buffer, mut state, attr) = common_fixture();
        state.set_cursor(4, 1);
        state.reverse_index(&mut buffer, attr).unwrap();
        assert_eq!(state.cursor(), ScrollCursor { x: 4, y: 1 });
        assert_cell(&buffer, 0, 1, b' ', attr);
        assert_cell(&buffer, 0, 2, b'5', attr);

        state.set_cursor(4, 3);
        state.reverse_index(&mut buffer, attr).unwrap();
        assert_eq!(state.cursor(), ScrollCursor { x: 4, y: 2 });
    }

    #[test]
    fn microsoft_screen_buffer_scroll_lines_256_colors_contract() {
        for color_style in 0..3 {
            let mut attr = TextAttribute::default();
            match color_style {
                0 => attr.set_background(TextColor::index16(TextColor::DARK_GREEN)),
                1 => attr.set_background(TextColor::index256(20)),
                _ => attr.set_background(TextColor::rgb(1, 2, 3)),
            }
            let mut erase = attr;
            erase.set_standard_erase();
            let buffer = TextBuffer::new(8, 6, TextAttribute::default()).unwrap();
            let mut state = VerticalScrollState::new(8, 0, 6);
            state.set_vertical_margins(0, 3);
            state.set_cursor(0, 0);

            for operation in 0..3 {
                let mut candidate = buffer.clone();
                match operation {
                    0 => state.insert_lines(&mut candidate, 10, attr).unwrap(),
                    1 => state.delete_lines(&mut candidate, 10, attr).unwrap(),
                    _ => {
                        for _ in 0..10 {
                            state.reverse_index(&mut candidate, attr).unwrap();
                        }
                    }
                }
                for y in 0..3 {
                    assert_cell(&candidate, 0, y, b' ', erase);
                }
            }
        }

        let rgb = Rgb::new(1, 2, 3);
        assert_eq!(TextColor::rgb(rgb.r, rgb.g, rgb.b), TextColor::rgb(1, 2, 3));
    }
}
