//! Materializes [`PageEvent`](crate::page_manager::PageEvent) values onto safe Rust buffers.
//!
//! R03 intentionally stopped at a typed paging control plane. R04 connects that
//! control plane to concrete [`TextBuffer`] storage while keeping renderer and
//! C++ integration outside this crate.

use crate::page_manager::{MAX_PAGES, PageBufferRef, PageEvent, PageSize};
use terminal_buffer::{
    text_attribute::TextAttribute,
    text_buffer::{TextBuffer, TextBufferError},
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PageStorageError {
    InvalidSize,
    Buffer(TextBufferError),
    MissingBackground(i32),
}

impl From<TextBufferError> for PageStorageError {
    fn from(value: TextBufferError) -> Self {
        Self::Buffer(value)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct PageProperties {
    pub cursor_visible: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PageStorage {
    visible: TextBuffer,
    visible_properties: PageProperties,
    backgrounds: [Option<TextBuffer>; MAX_PAGES as usize],
    background_properties: [PageProperties; MAX_PAGES as usize],
    redraw_requested: bool,
}

impl PageStorage {
    #[must_use]
    pub fn new(visible: TextBuffer) -> Self {
        Self {
            visible,
            visible_properties: PageProperties {
                cursor_visible: true,
            },
            backgrounds: core::array::from_fn(|_| None),
            background_properties: [PageProperties::default(); MAX_PAGES as usize],
            redraw_requested: false,
        }
    }

    #[must_use]
    pub const fn visible(&self) -> &TextBuffer {
        &self.visible
    }

    #[must_use]
    pub fn visible_mut(&mut self) -> &mut TextBuffer {
        &mut self.visible
    }

    #[must_use]
    pub const fn redraw_requested(&self) -> bool {
        self.redraw_requested
    }

    pub const fn clear_redraw_request(&mut self) {
        self.redraw_requested = false;
    }

    #[must_use]
    pub const fn visible_properties(&self) -> PageProperties {
        self.visible_properties
    }

    #[must_use]
    pub fn background(&self, page: i32) -> Option<&TextBuffer> {
        Self::background_index(page).and_then(|index| self.backgrounds[index].as_ref())
    }

    /// Applies a sequence emitted by `PageManager` in-order.
    ///
    /// # Errors
    ///
    /// Returns an error if an event contains dimensions that cannot be represented
    /// by the Rust buffer, references a missing background buffer, or a buffer
    /// resize/allocation fails.
    pub fn apply_events(
        &mut self,
        events: &[PageEvent],
        fill_attribute: TextAttribute,
    ) -> Result<(), PageStorageError> {
        for event in events {
            self.apply_event(*event, fill_attribute)?;
        }
        Ok(())
    }

    fn apply_event(
        &mut self,
        event: PageEvent,
        fill_attribute: TextAttribute,
    ) -> Result<(), PageStorageError> {
        match event {
            PageEvent::CreateBackgroundBuffer { page, size } => {
                let index = Self::checked_background_index(page)?;
                self.backgrounds[index] = Some(Self::new_buffer(size, fill_attribute)?);
            }
            PageEvent::ResizeBackgroundBuffer { page, new_size, .. } => {
                let index = Self::checked_background_index(page)?;
                let buffer = self.backgrounds[index]
                    .as_mut()
                    .ok_or(PageStorageError::MissingBackground(page))?;
                let (width, height) = Self::dimensions(new_size)?;
                buffer.resize_width_reflow(width, fill_attribute)?;
                buffer.resize_height(height, fill_attribute)?;
            }
            PageEvent::SaveVisibleRows {
                page,
                visible_top,
                size,
            } => {
                self.copy_visible_to_background(page, visible_top, size, fill_attribute)?;
            }
            PageEvent::LoadVisibleRows {
                page,
                visible_top,
                size,
            } => {
                self.copy_background_to_visible(page, visible_top, size)?;
            }
            PageEvent::CopyProperties { from, to, .. } => {
                let properties = self.properties(from)?;
                self.set_properties(to, properties)?;
            }
            PageEvent::SetVisibleCursorVisible(visible) => {
                self.visible_properties.cursor_visible = visible;
            }
            PageEvent::RedrawAll => self.redraw_requested = true,
        }
        Ok(())
    }

    fn copy_visible_to_background(
        &mut self,
        page: i32,
        visible_top: i32,
        size: PageSize,
        fill_attribute: TextAttribute,
    ) -> Result<(), PageStorageError> {
        let index = Self::checked_background_index(page)?;
        if self.backgrounds[index].is_none() {
            self.backgrounds[index] = Some(Self::new_buffer(size, fill_attribute)?);
        }
        let (_, height) = Self::dimensions(size)?;
        let background = self.backgrounds[index]
            .as_mut()
            .ok_or(PageStorageError::MissingBackground(page))?;
        for y in 0..height {
            *background.row_mut(i32::from(y)) =
                self.visible.row(visible_top + i32::from(y)).clone();
        }
        Ok(())
    }

    fn copy_background_to_visible(
        &mut self,
        page: i32,
        visible_top: i32,
        size: PageSize,
    ) -> Result<(), PageStorageError> {
        let index = Self::checked_background_index(page)?;
        let background = self.backgrounds[index]
            .as_ref()
            .ok_or(PageStorageError::MissingBackground(page))?;
        let (_, height) = Self::dimensions(size)?;
        for y in 0..height {
            *self.visible.row_mut(visible_top + i32::from(y)) =
                background.row(i32::from(y)).clone();
        }
        Ok(())
    }

    fn properties(&self, reference: PageBufferRef) -> Result<PageProperties, PageStorageError> {
        match reference {
            PageBufferRef::Visible => Ok(self.visible_properties),
            PageBufferRef::Background(page) => {
                let index = Self::checked_background_index(page)?;
                Ok(self.background_properties[index])
            }
        }
    }

    fn set_properties(
        &mut self,
        reference: PageBufferRef,
        properties: PageProperties,
    ) -> Result<(), PageStorageError> {
        match reference {
            PageBufferRef::Visible => self.visible_properties = properties,
            PageBufferRef::Background(page) => {
                let index = Self::checked_background_index(page)?;
                self.background_properties[index] = properties;
            }
        }
        Ok(())
    }

    fn new_buffer(
        size: PageSize,
        fill_attribute: TextAttribute,
    ) -> Result<TextBuffer, PageStorageError> {
        let (width, height) = Self::dimensions(size)?;
        TextBuffer::new(width, height, fill_attribute).map_err(Into::into)
    }

    fn dimensions(size: PageSize) -> Result<(u16, u16), PageStorageError> {
        let width = u16::try_from(size.width).map_err(|_| PageStorageError::InvalidSize)?;
        let height = u16::try_from(size.height).map_err(|_| PageStorageError::InvalidSize)?;
        if width == 0 || height == 0 {
            return Err(PageStorageError::InvalidSize);
        }
        Ok((width, height))
    }

    fn checked_background_index(page: i32) -> Result<usize, PageStorageError> {
        Self::background_index(page).ok_or(PageStorageError::MissingBackground(page))
    }

    fn background_index(page: i32) -> Option<usize> {
        (1..=MAX_PAGES)
            .contains(&page)
            .then(|| usize::try_from(page - 1).expect("positive page index fits usize"))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use terminal_buffer::text_buffer::TextBuffer;

    fn attribute() -> TextAttribute {
        TextAttribute::default()
    }

    #[test]
    fn save_and_load_materialize_page_rows() {
        let mut visible = TextBuffer::new(4, 4, attribute()).unwrap();
        visible
            .row_mut(2)
            .replace_glyph(0, 1, &[u16::from(b'A')])
            .unwrap();
        visible
            .row_mut(3)
            .replace_glyph(0, 1, &[u16::from(b'B')])
            .unwrap();
        let mut storage = PageStorage::new(visible);
        let size = PageSize::new(4, 2);

        storage
            .apply_events(
                &[
                    PageEvent::CreateBackgroundBuffer { page: 2, size },
                    PageEvent::SaveVisibleRows {
                        page: 2,
                        visible_top: 2,
                        size,
                    },
                ],
                attribute(),
            )
            .unwrap();

        storage.visible_mut().row_mut(2).reset(attribute());
        storage.visible_mut().row_mut(3).reset(attribute());
        storage
            .apply_events(
                &[PageEvent::LoadVisibleRows {
                    page: 2,
                    visible_top: 2,
                    size,
                }],
                attribute(),
            )
            .unwrap();

        assert_eq!(storage.visible().row(2).glyph_at(0), &[u16::from(b'A')]);
        assert_eq!(storage.visible().row(3).glyph_at(0), &[u16::from(b'B')]);
    }

    #[test]
    fn property_copy_and_redraw_are_materialized() {
        let visible = TextBuffer::new(4, 2, attribute()).unwrap();
        let mut storage = PageStorage::new(visible);
        let size = PageSize::new(4, 2);
        storage
            .apply_events(
                &[
                    PageEvent::CreateBackgroundBuffer { page: 2, size },
                    PageEvent::CopyProperties {
                        from: PageBufferRef::Visible,
                        to: PageBufferRef::Background(2),
                        old_top: 0,
                        new_top: 0,
                    },
                    PageEvent::SetVisibleCursorVisible(false),
                    PageEvent::RedrawAll,
                ],
                attribute(),
            )
            .unwrap();

        assert!(!storage.visible_properties().cursor_visible);
        assert!(storage.redraw_requested());
        storage.clear_redraw_request();
        assert!(!storage.redraw_requested());
    }
}
