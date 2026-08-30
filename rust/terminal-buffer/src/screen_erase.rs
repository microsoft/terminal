//! Safe erase, selective-protection and RIS reset semantics for screen buffers.
//!
//! This owner composes the existing rectangular cell primitives with the
//! viewport/cursor rules exercised by Host `ScreenBufferTests`. VT parsing and
//! renderer notification remain outside this module.

use crate::rect_ops::{ScreenRect, erase_rect, fill_rect, scroll_rect, selective_erase_rect};
use crate::row::RowError;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferPoint};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EraseType {
    ToEnd,
    FromBeginning,
    All,
}

/// Applies DECSCA parameters to the active attributes. Microsoft accepts 0 and
/// 2 as unprotected, 1 as protected, and applies multiple parameters in order.
pub fn set_character_protection(attribute: &mut TextAttribute, params: &[u16]) {
    if params.is_empty() {
        attribute.set_protected(false);
        return;
    }

    for parameter in params {
        match *parameter {
            0 | 2 => attribute.set_protected(false),
            1 => attribute.set_protected(true),
            _ => {}
        }
    }
}

/// Erases the cursor row using EL/DECSEL semantics.
pub fn erase_line(
    buffer: &mut TextBuffer,
    cursor: TextBufferPoint,
    erase_type: EraseType,
    selective: bool,
    active_attribute: TextAttribute,
) -> Result<(), RowError> {
    if cursor.y >= buffer.height() {
        return Ok(());
    }

    let width = buffer.width();
    let x = cursor.x.min(width.saturating_sub(1));
    let rect = match erase_type {
        EraseType::ToEnd => ScreenRect::new(x, cursor.y, width, cursor.y + 1),
        EraseType::FromBeginning => ScreenRect::new(0, cursor.y, x.saturating_add(1), cursor.y + 1),
        EraseType::All => ScreenRect::new(0, cursor.y, width, cursor.y + 1),
    };
    erase_region(buffer, rect, selective, active_attribute)
}

/// Erases the resolved viewport using ED/DECSED semantics. Horizontal viewport
/// offsets do not limit ED: complete buffer rows are affected, while the
/// viewport contributes only the vertical bounds.
pub fn erase_display(
    buffer: &mut TextBuffer,
    viewport: ScreenRect,
    cursor: TextBufferPoint,
    erase_type: EraseType,
    selective: bool,
    active_attribute: TextAttribute,
) -> Result<(), RowError> {
    let top = viewport.top.min(buffer.height());
    let bottom = viewport.bottom.min(buffer.height());
    if top >= bottom || cursor.y < top || cursor.y >= bottom {
        return Ok(());
    }

    let width = buffer.width();
    let x = cursor.x.min(width.saturating_sub(1));
    match erase_type {
        EraseType::ToEnd => {
            erase_region(
                buffer,
                ScreenRect::new(x, cursor.y, width, cursor.y + 1),
                selective,
                active_attribute,
            )?;
            if cursor.y + 1 < bottom {
                erase_region(
                    buffer,
                    ScreenRect::new(0, cursor.y + 1, width, bottom),
                    selective,
                    active_attribute,
                )?;
            }
        }
        EraseType::FromBeginning => {
            if top < cursor.y {
                erase_region(
                    buffer,
                    ScreenRect::new(0, top, width, cursor.y),
                    selective,
                    active_attribute,
                )?;
            }
            erase_region(
                buffer,
                ScreenRect::new(0, cursor.y, x.saturating_add(1), cursor.y + 1),
                selective,
                active_attribute,
            )?;
        }
        EraseType::All => {
            erase_region(
                buffer,
                ScreenRect::new(0, top, width, bottom),
                selective,
                active_attribute,
            )?;
        }
    }
    Ok(())
}

/// Applies Windows Terminal's scrollback-aware ED 2 policy.
///
/// Erase All advances the viewport until its top is immediately below the
/// pre-erase cursor row, unless the physical buffer bottom prevents that move.
/// The cursor keeps the same viewport-relative position, prior rows remain in
/// scrollback, and the newly visible viewport is erased with the active standard
/// erase attributes. Horizontal viewport coordinates are preserved.
pub fn erase_all_with_scrollback(
    buffer: &mut TextBuffer,
    viewport: &mut ScreenRect,
    cursor: &mut TextBufferPoint,
    active_attribute: TextAttribute,
) -> Result<(), RowError> {
    let buffer_height = buffer.height();
    let top = viewport.top.min(buffer_height);
    let bottom = viewport.bottom.min(buffer_height);
    if top >= bottom {
        return Ok(());
    }

    let viewport_height = bottom - top;
    let relative_x = cursor.x.saturating_sub(viewport.left);
    let relative_y = cursor.y.saturating_sub(top);
    let max_top = buffer_height.saturating_sub(viewport_height);
    let new_top = cursor.y.saturating_add(1).min(max_top);
    let new_bottom = new_top.saturating_add(viewport_height).min(buffer_height);

    *viewport = ScreenRect::new(viewport.left, new_top, viewport.right, new_bottom);
    cursor.x = viewport
        .left
        .saturating_add(relative_x)
        .min(buffer.width().saturating_sub(1));
    cursor.y = new_top
        .saturating_add(relative_y)
        .min(buffer_height.saturating_sub(1));

    erase_rect(
        buffer,
        ScreenRect::new(0, new_top, buffer.width(), new_bottom),
        active_attribute,
    )
}

/// Implements ED 3 (erase scrollback): viewport rows move to logical row zero,
/// the cursor keeps its viewport-relative position, and all rows below the
/// moved viewport are reset to the initial attributes.
pub fn erase_scrollback(
    buffer: &mut TextBuffer,
    viewport: &mut ScreenRect,
    cursor: &mut TextBufferPoint,
    initial_attribute: TextAttribute,
) -> Result<(), RowError> {
    let top = viewport.top.min(buffer.height());
    let bottom = viewport.bottom.min(buffer.height());
    if top >= bottom {
        return Ok(());
    }

    let height = bottom - top;
    scroll_rect(
        buffer,
        ScreenRect::new(0, top, buffer.width(), bottom),
        TextBufferPoint::new(0, 0),
        initial_attribute,
    )?;
    if height < buffer.height() {
        fill_rect(
            buffer,
            ScreenRect::new(0, height, buffer.width(), buffer.height()),
            u16::from(b' '),
            initial_attribute,
        )?;
    }

    cursor.y = cursor.y.saturating_sub(top);
    *viewport = ScreenRect::new(viewport.left, 0, viewport.right, height);
    Ok(())
}

/// Resets the deterministic buffer state asserted by RIS in Host tests: clear
/// cells/default attributes, viewport origin, cursor position and active attrs.
pub fn hard_reset(
    buffer: &mut TextBuffer,
    viewport: &mut ScreenRect,
    cursor: &mut TextBufferPoint,
    active_attribute: &mut TextAttribute,
) {
    let viewport_width = viewport.width().min(buffer.width());
    let viewport_height = viewport.height().min(buffer.height());
    let defaults = TextAttribute::default();

    buffer.reset(defaults);
    *viewport = ScreenRect::new(0, 0, viewport_width, viewport_height);
    *cursor = TextBufferPoint::new(0, 0);
    *active_attribute = defaults;
}

fn erase_region(
    buffer: &mut TextBuffer,
    rect: ScreenRect,
    selective: bool,
    active_attribute: TextAttribute,
) -> Result<(), RowError> {
    if selective {
        selective_erase_rect(buffer, rect)
    } else {
        erase_rect(buffer, rect, active_attribute)
    }
}
