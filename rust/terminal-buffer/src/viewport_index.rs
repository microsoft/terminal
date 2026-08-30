//! Safe viewport indexing for LF/VT/FF/IND/NEL, including DEC margins.
//!
//! The no-margin path owns viewport panning into scrollback. When DECSTBM is
//! active, line feed stays constrained by the visible viewport: inside the
//! margins it delegates rectangular scrolling to `vertical_scroll`; below the
//! bottom margin it cannot pan the viewport. NEL additionally returns to the
//! effective left margin.

use crate::rect_ops::{ScreenRect, erase_rect, scroll_rect};
use crate::row::RowError;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferPoint};
use crate::vertical_scroll::VerticalScrollState;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct LineFeedMargins {
    pub vertical: Option<(u16, u16)>,
    pub horizontal: Option<(u16, u16)>,
}

impl LineFeedMargins {
    #[must_use]
    pub const fn none() -> Self {
        Self {
            vertical: None,
            horizontal: None,
        }
    }

    #[must_use]
    pub const fn vertical(top: u16, bottom: u16) -> Self {
        Self {
            vertical: Some((top, bottom)),
            horizontal: None,
        }
    }

    #[must_use]
    pub const fn with_horizontal(mut self, left: u16, right: u16) -> Self {
        self.horizontal = Some((left, right));
        self
    }
}

/// Advances one row as LF/VT/FF/IND. If the cursor is on the viewport bottom,
/// the viewport pans into the next buffer row when possible; otherwise the
/// visible region scrolls in place at the physical buffer bottom.
pub fn index_down(
    buffer: &mut TextBuffer,
    viewport: &mut ScreenRect,
    cursor: &mut TextBufferPoint,
    erase_source_attribute: TextAttribute,
) -> Result<(), RowError> {
    let top = viewport.top.min(buffer.height());
    let bottom = viewport.bottom.min(buffer.height());
    if top >= bottom {
        return Ok(());
    }

    cursor.y = cursor.y.clamp(top, bottom - 1);
    if cursor.y < bottom - 1 {
        cursor.y += 1;
        return Ok(());
    }

    if bottom < buffer.height() {
        erase_rect(
            buffer,
            ScreenRect::new(0, bottom, buffer.width(), bottom + 1),
            erase_source_attribute,
        )?;
        viewport.top = top + 1;
        viewport.bottom = bottom + 1;
        cursor.y += 1;
        return Ok(());
    }

    let mut erase = erase_source_attribute;
    erase.set_standard_erase();
    if top + 1 < bottom {
        scroll_rect(
            buffer,
            ScreenRect::new(0, top + 1, buffer.width(), bottom),
            TextBufferPoint::new(0, top),
            erase,
        )?;
    }
    erase_rect(
        buffer,
        ScreenRect::new(0, bottom - 1, buffer.width(), bottom),
        erase_source_attribute,
    )?;
    Ok(())
}

/// NEL shares the same indexing behavior and additionally returns to column 0.
pub fn next_line(
    buffer: &mut TextBuffer,
    viewport: &mut ScreenRect,
    cursor: &mut TextBufferPoint,
    erase_source_attribute: TextAttribute,
) -> Result<(), RowError> {
    index_down(buffer, viewport, cursor, erase_source_attribute)?;
    cursor.x = 0;
    Ok(())
}

/// Applies LF/IND or NEL with optional DEC vertical and horizontal margins.
///
/// With no vertical margins this is the normal viewport-panning path. With
/// DECSTBM active, a cursor inside the margin region scrolls only that region at
/// its lower edge; a cursor below the region advances only while it remains
/// inside the viewport and never pans the viewport. When `with_return` is true,
/// NEL returns to column zero or to the active horizontal left margin.
pub fn line_feed(
    buffer: &mut TextBuffer,
    viewport: &mut ScreenRect,
    cursor: &mut TextBufferPoint,
    margins: LineFeedMargins,
    with_return: bool,
    erase_source_attribute: TextAttribute,
) -> Result<(), RowError> {
    let top = viewport.top.min(buffer.height());
    let bottom = viewport.bottom.min(buffer.height());
    if top >= bottom {
        return Ok(());
    }

    let horizontal = normalize_horizontal(buffer.width(), margins.horizontal);
    let return_column = horizontal.map_or(0, |(left, _)| left);

    if let Some((margin_top, margin_bottom)) = normalize_vertical(top, bottom, margins.vertical) {
        cursor.y = cursor.y.clamp(top, bottom - 1);
        if cursor.y >= margin_top && cursor.y < margin_bottom {
            if cursor.y == margin_bottom - 1 {
                let mut scrolling = VerticalScrollState::new(buffer.width(), top, bottom);
                scrolling.set_vertical_margins(margin_top, margin_bottom);
                if let Some((left, right)) = horizontal {
                    scrolling.set_horizontal_margins(left, right);
                }
                scrolling.set_cursor(cursor.x, cursor.y);
                scrolling.scroll_up(buffer, 1, erase_source_attribute)?;
            } else {
                cursor.y += 1;
            }
        } else if cursor.y < bottom - 1 {
            cursor.y += 1;
        }
    } else {
        index_down(buffer, viewport, cursor, erase_source_attribute)?;
    }

    if with_return {
        cursor.x = return_column;
    }
    Ok(())
}

fn normalize_vertical(
    viewport_top: u16,
    viewport_bottom: u16,
    margins: Option<(u16, u16)>,
) -> Option<(u16, u16)> {
    let (top, bottom) = margins?;
    let top = top.max(viewport_top).min(viewport_bottom);
    let bottom = bottom.max(viewport_top).min(viewport_bottom);
    (top < bottom).then_some((top, bottom))
}

fn normalize_horizontal(width: u16, margins: Option<(u16, u16)>) -> Option<(u16, u16)> {
    let (left, right) = margins?;
    let left = left.min(width);
    let right = right.min(width);
    (left < right).then_some((left, right))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::rect_ops::fill_rect;
    use crate::text_color::Rgb;

    #[test]
    fn index_pans_and_erases_the_newly_exposed_row() {
        let original = TextAttribute::default();
        let active = TextAttribute::from_rgb(Rgb::new(1, 2, 3), Rgb::new(4, 5, 6));
        let mut expected = active;
        expected.set_standard_erase();
        let mut buffer = TextBuffer::new(8, 6, original).unwrap();
        fill_rect(
            &mut buffer,
            ScreenRect::new(0, 0, 8, 6),
            u16::from(b'X'),
            original,
        )
        .unwrap();
        let mut viewport = ScreenRect::new(0, 0, 8, 4);
        let mut cursor = TextBufferPoint::new(3, 3);

        index_down(&mut buffer, &mut viewport, &mut cursor, active).unwrap();

        assert_eq!(viewport, ScreenRect::new(0, 1, 8, 5));
        assert_eq!(cursor, TextBufferPoint::new(3, 4));
        assert_eq!(buffer.row(4).glyph_at(0), &[u16::from(b' ')]);
        assert_eq!(buffer.row(4).attribute_at(0), expected);
    }

    #[test]
    fn decstbm_prevents_viewport_pan_below_the_bottom_margin() {
        let attr = TextAttribute::default();
        let mut buffer = TextBuffer::new(8, 8, attr).unwrap();
        let mut viewport = ScreenRect::new(0, 0, 8, 6);
        let mut cursor = TextBufferPoint::new(0, 5);

        line_feed(
            &mut buffer,
            &mut viewport,
            &mut cursor,
            LineFeedMargins::vertical(0, 3),
            false,
            attr,
        )
        .unwrap();

        assert_eq!(cursor, TextBufferPoint::new(0, 5));
        assert_eq!(viewport, ScreenRect::new(0, 0, 8, 6));
    }
}
