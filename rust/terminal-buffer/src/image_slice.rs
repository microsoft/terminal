//! Safe per-row image storage compatible with Windows Terminal `ImageSlice`.
//!
//! The C++ implementation owns a rectangular pixel buffer spanning a half-open
//! column range. This port keeps that observable layout while replacing raw
//! pointer arithmetic and `memmove`/`memset` with checked slices and owned
//! vectors.

use core::sync::atomic::{AtomicU64, Ordering};

static NEXT_REVISION: AtomicU64 = AtomicU64::new(0);

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
#[repr(C)]
pub struct Pixel {
    pub blue: u8,
    pub green: u8,
    pub red: u8,
    pub reserved: u8,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct CellSize {
    pub width: u16,
    pub height: u16,
}

impl CellSize {
    #[must_use]
    pub const fn new(width: u16, height: u16) -> Self {
        Self { width, height }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ImageSliceError {
    EmptyCellSize,
    InvalidColumnRange,
    ArithmeticOverflow,
    SourceRangeOverflow,
    MismatchedCellSize,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ImageSlice {
    revision: u64,
    cell_size: CellSize,
    pixels: Vec<Pixel>,
    column_begin: i32,
    column_end: i32,
    pixel_width: usize,
}

impl ImageSlice {
    /// Creates an empty image slice for one text-buffer row.
    ///
    /// # Errors
    ///
    /// Returns an error if either cell dimension is zero.
    pub fn new(cell_size: CellSize) -> Result<Self, ImageSliceError> {
        if cell_size.width == 0 || cell_size.height == 0 {
            return Err(ImageSliceError::EmptyCellSize);
        }
        Ok(Self {
            revision: 0,
            cell_size,
            pixels: Vec::new(),
            column_begin: 0,
            column_end: 0,
            pixel_width: 0,
        })
    }

    pub fn bump_revision(&mut self) {
        loop {
            let revision = NEXT_REVISION.fetch_add(1, Ordering::Relaxed);
            if revision != 0 {
                self.revision = revision;
                return;
            }
        }
    }

    #[must_use]
    pub const fn revision(&self) -> u64 {
        self.revision
    }

    #[must_use]
    pub const fn cell_size(&self) -> CellSize {
        self.cell_size
    }

    #[must_use]
    pub const fn column_offset(&self) -> i32 {
        self.column_begin
    }

    #[must_use]
    pub fn pixel_width(&self) -> usize {
        self.pixel_width
    }

    #[must_use]
    pub fn pixels(&self) -> &[Pixel] {
        &self.pixels
    }

    /// Ensures that the requested half-open column range is backed by storage.
    /// Existing pixels retain their row-relative location if the range expands.
    ///
    /// # Errors
    ///
    /// Returns an error for reversed ranges or arithmetic that cannot be
    /// represented by the host address space.
    pub fn ensure_columns(
        &mut self,
        column_begin: i32,
        column_end: i32,
    ) -> Result<(), ImageSliceError> {
        if column_begin < 0 || column_end < column_begin {
            return Err(ImageSliceError::InvalidColumnRange);
        }
        if column_begin == column_end {
            return Ok(());
        }
        if !self.pixels.is_empty()
            && column_begin >= self.column_begin
            && column_end <= self.column_end
        {
            return Ok(());
        }

        let had_data = !self.pixels.is_empty();
        let old_begin = self.column_begin;
        let old_pixel_width = self.pixel_width;
        let new_begin = if had_data {
            self.column_begin.min(column_begin)
        } else {
            column_begin
        };
        let new_end = if had_data {
            self.column_end.max(column_end)
        } else {
            column_end
        };
        let new_pixel_width = self.columns_to_pixels(new_end - new_begin)?;
        let height = usize::from(self.cell_size.height);
        let new_len = new_pixel_width
            .checked_mul(height)
            .ok_or(ImageSliceError::ArithmeticOverflow)?;
        let mut new_pixels = vec![Pixel::default(); new_len];

        if had_data {
            let offset_columns = old_begin
                .checked_sub(new_begin)
                .ok_or(ImageSliceError::ArithmeticOverflow)?;
            let new_offset = self.columns_to_pixels(offset_columns)?;
            let copy_width = old_pixel_width.min(new_pixel_width.saturating_sub(new_offset));
            for row in 0..height {
                let old_start = row
                    .checked_mul(old_pixel_width)
                    .ok_or(ImageSliceError::ArithmeticOverflow)?;
                let new_start = row
                    .checked_mul(new_pixel_width)
                    .and_then(|value| value.checked_add(new_offset))
                    .ok_or(ImageSliceError::ArithmeticOverflow)?;
                let old_end = old_start
                    .checked_add(copy_width)
                    .ok_or(ImageSliceError::ArithmeticOverflow)?;
                let new_end = new_start
                    .checked_add(copy_width)
                    .ok_or(ImageSliceError::ArithmeticOverflow)?;
                new_pixels[new_start..new_end].copy_from_slice(&self.pixels[old_start..old_end]);
            }
        }

        self.column_begin = new_begin;
        self.column_end = new_end;
        self.pixel_width = new_pixel_width;
        self.pixels = new_pixels;
        Ok(())
    }

    /// Returns one mutable pixel row for a backed column range.
    ///
    /// # Errors
    ///
    /// Returns an error for an invalid row/range or if storage expansion fails.
    pub fn mutable_pixel_row(
        &mut self,
        row: u16,
        column_begin: i32,
        column_end: i32,
    ) -> Result<&mut [Pixel], ImageSliceError> {
        if row >= self.cell_size.height {
            return Err(ImageSliceError::InvalidColumnRange);
        }
        self.ensure_columns(column_begin, column_end)?;
        let start_in_row = self.columns_to_pixels(column_begin - self.column_begin)?;
        let width = self.columns_to_pixels(column_end - column_begin)?;
        let start = usize::from(row)
            .checked_mul(self.pixel_width)
            .and_then(|value| value.checked_add(start_in_row))
            .ok_or(ImageSliceError::ArithmeticOverflow)?;
        let end = start
            .checked_add(width)
            .ok_or(ImageSliceError::ArithmeticOverflow)?;
        Ok(&mut self.pixels[start..end])
    }

    /// Copies a cell range from another image slice. Missing source coverage is
    /// represented by transparent pixels, matching C++ `CopyCells` behavior.
    /// Returns `true` if the destination becomes completely empty.
    ///
    /// # Errors
    ///
    /// Returns an error for incompatible cell sizes, reversed ranges, or
    /// arithmetic overflow.
    pub fn copy_cells(
        &mut self,
        source: &Self,
        src_column: i32,
        dst_column_begin: i32,
        dst_column_end: i32,
    ) -> Result<bool, ImageSliceError> {
        if self.cell_size != source.cell_size {
            return Err(ImageSliceError::MismatchedCellSize);
        }
        if src_column < 0 || dst_column_begin < 0 || dst_column_end < dst_column_begin {
            return Err(ImageSliceError::InvalidColumnRange);
        }
        let distance = dst_column_end - dst_column_begin;
        let src_column_end = src_column
            .checked_add(distance)
            .ok_or(ImageSliceError::SourceRangeOverflow)?;

        let src_used_begin = src_column.max(source.column_begin);
        let src_used_end = src_column_end.min(source.column_end).max(src_used_begin);
        let dst_used_begin = dst_column_begin.max(self.column_begin);
        let dst_used_end = dst_column_end.min(self.column_end).max(dst_used_begin);
        let projected_offset = dst_column_begin
            .checked_sub(src_column)
            .ok_or(ImageSliceError::ArithmeticOverflow)?;
        let dst_write_begin = src_used_begin
            .checked_add(projected_offset)
            .ok_or(ImageSliceError::ArithmeticOverflow)?;
        let dst_write_end = src_used_end
            .checked_add(projected_offset)
            .ok_or(ImageSliceError::ArithmeticOverflow)?;

        if dst_write_begin < dst_write_end {
            self.ensure_columns(dst_write_begin, dst_write_end)?;
            let write_pixels = self.columns_to_pixels(dst_write_end - dst_write_begin)?;
            let src_offset = source.columns_to_pixels(src_used_begin - source.column_begin)?;
            let dst_offset = self.columns_to_pixels(dst_write_begin - self.column_begin)?;
            let height = usize::from(self.cell_size.height);
            for row in 0..height {
                let src_start = row
                    .checked_mul(source.pixel_width)
                    .and_then(|value| value.checked_add(src_offset))
                    .ok_or(ImageSliceError::ArithmeticOverflow)?;
                let dst_start = row
                    .checked_mul(self.pixel_width)
                    .and_then(|value| value.checked_add(dst_offset))
                    .ok_or(ImageSliceError::ArithmeticOverflow)?;
                let src_end = src_start
                    .checked_add(write_pixels)
                    .ok_or(ImageSliceError::ArithmeticOverflow)?;
                let dst_end = dst_start
                    .checked_add(write_pixels)
                    .ok_or(ImageSliceError::ArithmeticOverflow)?;
                self.pixels[dst_start..dst_end].copy_from_slice(&source.pixels[src_start..src_end]);
            }
        }

        if dst_used_begin < dst_write_begin && self.erase_cells(dst_used_begin, dst_write_begin)? {
            return Ok(true);
        }
        if dst_used_end > dst_write_end && self.erase_cells(dst_write_end, dst_used_end)? {
            return Ok(true);
        }
        Ok(self.column_begin >= self.column_end)
    }

    /// Erases a half-open column range. Returns `true` when the whole occupied
    /// range is covered so the owner can drop this slice entirely.
    ///
    /// # Errors
    ///
    /// Returns an error for a reversed/negative range or arithmetic overflow.
    pub fn erase_cells(
        &mut self,
        column_begin: i32,
        column_end: i32,
    ) -> Result<bool, ImageSliceError> {
        if column_begin < 0 || column_end < column_begin {
            return Err(ImageSliceError::InvalidColumnRange);
        }
        if self.pixels.is_empty() {
            return Ok(true);
        }
        if column_begin <= self.column_begin && column_end >= self.column_end {
            return Ok(true);
        }

        let erase_begin = column_begin.max(self.column_begin);
        let erase_end = column_end.min(self.column_end);
        if erase_begin < erase_end {
            let erase_offset = self.columns_to_pixels(erase_begin - self.column_begin)?;
            let erase_width = self.columns_to_pixels(erase_end - erase_begin)?;
            let height = usize::from(self.cell_size.height);
            for row in 0..height {
                let start = row
                    .checked_mul(self.pixel_width)
                    .and_then(|value| value.checked_add(erase_offset))
                    .ok_or(ImageSliceError::ArithmeticOverflow)?;
                let end = start
                    .checked_add(erase_width)
                    .ok_or(ImageSliceError::ArithmeticOverflow)?;
                self.pixels[start..end].fill(Pixel::default());
            }
        }
        Ok(false)
    }

    fn columns_to_pixels(&self, columns: i32) -> Result<usize, ImageSliceError> {
        let columns = usize::try_from(columns).map_err(|_| ImageSliceError::InvalidColumnRange)?;
        columns
            .checked_mul(usize::from(self.cell_size.width))
            .ok_or(ImageSliceError::ArithmeticOverflow)
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
    fn expansion_preserves_existing_pixels() {
        let mut slice = ImageSlice::new(CellSize::new(2, 2)).unwrap();
        slice.mutable_pixel_row(0, 2, 3).unwrap().fill(red(7));
        slice.mutable_pixel_row(1, 2, 3).unwrap().fill(red(9));
        slice.ensure_columns(1, 4).unwrap();

        assert_eq!(slice.column_offset(), 1);
        assert_eq!(slice.pixel_width(), 6);
        assert_eq!(&slice.pixels()[2..4], &[red(7), red(7)]);
        assert_eq!(&slice.pixels()[8..10], &[red(9), red(9)]);
    }

    #[test]
    fn copy_cells_projects_only_used_source_pixels() {
        let mut source = ImageSlice::new(CellSize::new(1, 1)).unwrap();
        source
            .mutable_pixel_row(0, 2, 4)
            .unwrap()
            .copy_from_slice(&[red(2), red(3)]);
        let mut destination = ImageSlice::new(CellSize::new(1, 1)).unwrap();
        destination.ensure_columns(5, 9).unwrap();
        destination.pixels.fill(red(8));

        assert!(!destination.copy_cells(&source, 1, 5, 9).unwrap());
        assert_eq!(
            destination.pixels(),
            &[Pixel::default(), red(2), red(3), Pixel::default()]
        );
    }

    #[test]
    fn erase_reports_complete_coverage_without_mutating_storage() {
        let mut slice = ImageSlice::new(CellSize::new(1, 1)).unwrap();
        slice.mutable_pixel_row(0, 3, 5).unwrap().fill(red(4));
        let before = slice.pixels().to_vec();

        assert!(slice.erase_cells(2, 6).unwrap());
        assert_eq!(slice.pixels(), before);
    }

    #[test]
    fn partial_erase_zeros_only_requested_pixels() {
        let mut slice = ImageSlice::new(CellSize::new(2, 1)).unwrap();
        slice.mutable_pixel_row(0, 1, 4).unwrap().fill(red(5));

        assert!(!slice.erase_cells(2, 3).unwrap());
        assert_eq!(
            slice.pixels(),
            &[
                red(5),
                red(5),
                Pixel::default(),
                Pixel::default(),
                red(5),
                red(5)
            ]
        );
    }

    #[test]
    fn revision_never_uses_zero_after_bump() {
        let mut slice = ImageSlice::new(CellSize::new(1, 1)).unwrap();
        slice.bump_revision();
        let first = slice.revision();
        slice.bump_revision();
        assert_ne!(first, 0);
        assert_ne!(slice.revision(), 0);
        assert_ne!(first, slice.revision());
    }
}
