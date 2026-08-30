use crate::CellSize;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct FontCellSizes {
    pub size: CellSize,
    pub unscaled_size: CellSize,
}

#[must_use]
pub const fn validate_font_cell_sizes(
    default_raster_without_size: bool,
    mut size: CellSize,
    mut unscaled_size: CellSize,
) -> FontCellSizes {
    if !default_raster_without_size {
        if size.width == 0 {
            size.width = 1;
        }

        if size.height == 0 {
            size = CellSize {
                width: 8,
                height: 12,
            };
            unscaled_size = size;
        }
    }

    FontCellSizes {
        size,
        unscaled_size,
    }
}

#[cfg(test)]
mod tests {
    use super::{FontCellSizes, validate_font_cell_sizes};
    use crate::CellSize;

    #[test]
    fn default_raster_zero_size_is_left_unchanged() {
        let zero = CellSize::default();

        assert_eq!(
            validate_font_cell_sizes(true, zero, zero),
            FontCellSizes {
                size: zero,
                unscaled_size: zero,
            }
        );
    }

    #[test]
    fn zero_width_is_promoted_to_one_when_height_exists() {
        let unscaled = CellSize {
            width: 0,
            height: 20,
        };

        assert_eq!(
            validate_font_cell_sizes(false, unscaled, unscaled),
            FontCellSizes {
                size: CellSize {
                    width: 1,
                    height: 20,
                },
                unscaled_size: unscaled,
            }
        );
    }

    #[test]
    fn zero_height_selects_eight_by_twelve_and_updates_unscaled_size() {
        assert_eq!(
            validate_font_cell_sizes(
                false,
                CellSize {
                    width: 5,
                    height: 0,
                },
                CellSize {
                    width: 5,
                    height: 0,
                },
            ),
            FontCellSizes {
                size: CellSize {
                    width: 8,
                    height: 12,
                },
                unscaled_size: CellSize {
                    width: 8,
                    height: 12,
                },
            }
        );
    }

    #[test]
    fn valid_size_and_distinct_unscaled_size_are_preserved() {
        let size = CellSize {
            width: 9,
            height: 18,
        };
        let unscaled = CellSize {
            width: 8,
            height: 16,
        };

        assert_eq!(
            validate_font_cell_sizes(false, size, unscaled),
            FontCellSizes {
                size,
                unscaled_size: unscaled,
            }
        );
    }
}
