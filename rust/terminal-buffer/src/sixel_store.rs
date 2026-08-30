//! Safe row-aligned storage for decoded Sixel image placement.
//!
//! Windows Terminal's C++ `SixelParser` projects decoded image pixels into an
//! `ImageSlice` owned by each covered text row. This module preserves that
//! deterministic storage contract without introducing parser state, renderer
//! state, C++, FFI, or unsafe Rust.

use crate::image_slice::{CellSize, ImageSlice, ImageSliceError, Pixel};

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct SixelPixel {
    pub color: Pixel,
    pub transparent: bool,
}

impl SixelPixel {
    #[must_use]
    pub const fn opaque(color: Pixel) -> Self {
        Self {
            color,
            transparent: false,
        }
    }

    #[must_use]
    pub const fn transparent() -> Self {
        Self {
            color: Pixel {
                blue: 0,
                green: 0,
                red: 0,
                reserved: 0,
            },
            transparent: true,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SixelStoreError {
    EmptyCellSize,
    InvalidOrigin,
    InvalidImageDimensions,
    SourceLengthMismatch,
    ArithmeticOverflow,
    ImageSlice(ImageSliceError),
}

impl From<ImageSliceError> for SixelStoreError {
    fn from(value: ImageSliceError) -> Self {
        Self::ImageSlice(value)
    }
}

/// Row-indexed image ownership mirroring the C++ `ROW::_imageSlice` contract.
///
/// A slot is `None` until a Sixel write reaches that text row. Mutating an
/// existing slice bumps its revision before pixels are changed, matching
/// `ROW::GetMutableImageSlice()`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SixelRowStore {
    cell_size: CellSize,
    rows: Vec<Option<ImageSlice>>,
}

impl SixelRowStore {
    /// Creates storage aligned with a concrete number of text rows.
    ///
    /// # Errors
    ///
    /// Returns an error if either cell dimension is zero.
    pub fn new(row_count: usize, cell_size: CellSize) -> Result<Self, SixelStoreError> {
        if cell_size.width == 0 || cell_size.height == 0 {
            return Err(SixelStoreError::EmptyCellSize);
        }
        Ok(Self {
            cell_size,
            rows: vec![None; row_count],
        })
    }

    #[must_use]
    pub const fn cell_size(&self) -> CellSize {
        self.cell_size
    }

    #[must_use]
    pub fn row_count(&self) -> usize {
        self.rows.len()
    }

    #[must_use]
    pub fn image_slice(&self, row: usize) -> Option<&ImageSlice> {
        self.rows.get(row).and_then(Option::as_ref)
    }

    pub fn clear_row(&mut self, row: usize) {
        if let Some(slot) = self.rows.get_mut(row) {
            *slot = None;
        }
    }

    pub fn reset(&mut self) {
        self.rows.fill(None);
    }

    pub fn resize_rows(&mut self, row_count: usize) {
        self.rows.resize(row_count, None);
    }

    /// Places one decoded Sixel raster into row-aligned `ImageSlice` storage.
    ///
    /// `origin_column` and `origin_row` are text-cell coordinates. `image_width`
    /// and `image_height` are device-pixel dimensions. Transparent source pixels
    /// leave existing destination pixels untouched, matching the C++ parser.
    /// Pixels extending past the bottom of the concrete row store are clipped.
    ///
    /// # Errors
    ///
    /// Returns an error for negative origins, empty dimensions, mismatched source
    /// length, or checked-arithmetic/storage failures.
    pub fn place_raster(
        &mut self,
        origin_column: i32,
        origin_row: i32,
        image_width: usize,
        image_height: usize,
        pixels: &[SixelPixel],
    ) -> Result<(), SixelStoreError> {
        if origin_column < 0 || origin_row < 0 {
            return Err(SixelStoreError::InvalidOrigin);
        }
        if image_width == 0 || image_height == 0 {
            return Err(SixelStoreError::InvalidImageDimensions);
        }
        let expected_len = image_width
            .checked_mul(image_height)
            .ok_or(SixelStoreError::ArithmeticOverflow)?;
        if pixels.len() != expected_len {
            return Err(SixelStoreError::SourceLengthMismatch);
        }

        let cell_width = usize::from(self.cell_size.width);
        let cell_height = usize::from(self.cell_size.height);
        let covered_columns = image_width
            .checked_add(cell_width - 1)
            .ok_or(SixelStoreError::ArithmeticOverflow)?
            / cell_width;
        let covered_columns =
            i32::try_from(covered_columns).map_err(|_| SixelStoreError::ArithmeticOverflow)?;
        let column_end = origin_column
            .checked_add(covered_columns)
            .ok_or(SixelStoreError::ArithmeticOverflow)?;
        let origin_row = usize::try_from(origin_row).map_err(|_| SixelStoreError::InvalidOrigin)?;

        for source_y in 0..image_height {
            let row_delta = source_y / cell_height;
            let destination_row = origin_row
                .checked_add(row_delta)
                .ok_or(SixelStoreError::ArithmeticOverflow)?;
            if destination_row >= self.rows.len() {
                break;
            }
            let pixel_row = u16::try_from(source_y % cell_height)
                .map_err(|_| SixelStoreError::ArithmeticOverflow)?;
            let source_begin = source_y
                .checked_mul(image_width)
                .ok_or(SixelStoreError::ArithmeticOverflow)?;
            let source_end = source_begin
                .checked_add(image_width)
                .ok_or(SixelStoreError::ArithmeticOverflow)?;

            let slice = self.mutable_or_create_slice(destination_row)?;
            let destination = slice.mutable_pixel_row(pixel_row, origin_column, column_end)?;
            for (source, destination) in pixels[source_begin..source_end]
                .iter()
                .zip(destination.iter_mut())
            {
                if !source.transparent {
                    *destination = source.color;
                }
            }
        }

        Ok(())
    }

    fn mutable_or_create_slice(&mut self, row: usize) -> Result<&mut ImageSlice, SixelStoreError> {
        let slot = self
            .rows
            .get_mut(row)
            .ok_or(SixelStoreError::ArithmeticOverflow)?;
        if slot.is_none() {
            *slot = Some(ImageSlice::new(self.cell_size)?);
        }
        let slice = slot.as_mut().ok_or(SixelStoreError::ArithmeticOverflow)?;
        slice.bump_revision();
        Ok(slice)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn red(value: u8) -> Pixel {
        Pixel {
            red: value,
            ..Pixel::default()
        }
    }

    #[test]
    fn raster_is_split_across_concrete_text_rows() {
        let mut store = SixelRowStore::new(3, CellSize::new(2, 2)).unwrap();
        let pixels = (1..=12)
            .map(|value| SixelPixel::opaque(red(value)))
            .collect::<Vec<_>>();

        store.place_raster(1, 0, 3, 4, &pixels).unwrap();

        let first = store.image_slice(0).unwrap();
        let second = store.image_slice(1).unwrap();
        assert_eq!(first.column_offset(), 1);
        assert_eq!(first.pixel_width(), 4);
        assert_eq!(&first.pixels()[..3], &[red(1), red(2), red(3)]);
        assert_eq!(&first.pixels()[4..7], &[red(4), red(5), red(6)]);
        assert_eq!(&second.pixels()[..3], &[red(7), red(8), red(9)]);
        assert_eq!(&second.pixels()[4..7], &[red(10), red(11), red(12)]);
    }

    #[test]
    fn transparent_pixels_preserve_existing_destination_content() {
        let mut store = SixelRowStore::new(1, CellSize::new(1, 1)).unwrap();
        store
            .place_raster(
                2,
                0,
                2,
                1,
                &[SixelPixel::opaque(red(4)), SixelPixel::opaque(red(5))],
            )
            .unwrap();
        let revision = store.image_slice(0).unwrap().revision();

        store
            .place_raster(
                2,
                0,
                2,
                1,
                &[SixelPixel::transparent(), SixelPixel::opaque(red(9))],
            )
            .unwrap();

        let slice = store.image_slice(0).unwrap();
        assert_eq!(slice.pixels(), &[red(4), red(9)]);
        assert_ne!(slice.revision(), revision);
    }

    #[test]
    fn raster_is_clipped_at_bottom_of_row_store() {
        let mut store = SixelRowStore::new(2, CellSize::new(1, 2)).unwrap();
        let pixels = vec![SixelPixel::opaque(red(7)); 5];
        store.place_raster(0, 1, 1, 5, &pixels).unwrap();

        assert!(store.image_slice(0).is_none());
        assert!(store.image_slice(1).is_some());
    }

    #[test]
    fn clearing_and_reset_drop_row_owned_image_slices() {
        let mut store = SixelRowStore::new(2, CellSize::new(1, 1)).unwrap();
        store
            .place_raster(0, 0, 1, 1, &[SixelPixel::opaque(red(1))])
            .unwrap();
        store
            .place_raster(0, 1, 1, 1, &[SixelPixel::opaque(red(2))])
            .unwrap();

        store.clear_row(0);
        assert!(store.image_slice(0).is_none());
        assert!(store.image_slice(1).is_some());

        store.reset();
        assert!(store.image_slice(1).is_none());
    }
}
