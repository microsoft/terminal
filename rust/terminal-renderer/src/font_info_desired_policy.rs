const TRUE_TYPE_FAMILY_BIT: u8 = 0x04;

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct CellSize {
    pub width: i32,
    pub height: i32,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct FontInfoDesiredPolicy {
    face_name_empty: bool,
    family: u8,
    desired_size: CellSize,
    default_raster_set_from_engine: bool,
}

impl FontInfoDesiredPolicy {
    #[must_use]
    pub const fn new(
        face_name_empty: bool,
        family: u8,
        desired_size: CellSize,
        default_raster_set_from_engine: bool,
    ) -> Self {
        Self {
            face_name_empty,
            family,
            desired_size,
            default_raster_set_from_engine,
        }
    }

    #[must_use]
    pub const fn is_true_type(self) -> bool {
        self.family & TRUE_TYPE_FAMILY_BIT != 0
    }

    #[must_use]
    pub const fn engine_size(self) -> CellSize {
        if self.is_true_type() {
            CellSize {
                width: 0,
                height: self.desired_size.height,
            }
        } else {
            self.desired_size
        }
    }

    #[must_use]
    pub const fn is_default_raster_font(self) -> bool {
        self.default_raster_set_from_engine
            || (self.face_name_empty
                && ((self.desired_size.width == 0 && self.desired_size.height == 0)
                    || (self.desired_size.width == 8 && self.desired_size.height == 12)))
    }
}

#[cfg(test)]
mod tests {
    use super::{CellSize, FontInfoDesiredPolicy, TRUE_TYPE_FAMILY_BIT};

    #[test]
    fn true_type_engine_size_discards_requested_width() {
        let policy = FontInfoDesiredPolicy::new(
            false,
            TRUE_TYPE_FAMILY_BIT,
            CellSize {
                width: 11,
                height: 19,
            },
            false,
        );

        assert!(policy.is_true_type());
        assert_eq!(
            policy.engine_size(),
            CellSize {
                width: 0,
                height: 19,
            }
        );
    }

    #[test]
    fn raster_engine_size_preserves_requested_dimensions() {
        let size = CellSize {
            width: 8,
            height: 16,
        };
        let policy = FontInfoDesiredPolicy::new(false, 0, size, false);

        assert!(!policy.is_true_type());
        assert_eq!(policy.engine_size(), size);
    }

    #[test]
    fn explicit_engine_default_is_always_default_raster() {
        let policy = FontInfoDesiredPolicy::new(
            false,
            TRUE_TYPE_FAMILY_BIT,
            CellSize {
                width: 20,
                height: 30,
            },
            true,
        );

        assert!(policy.is_default_raster_font());
    }

    #[test]
    fn blank_face_zero_size_and_eight_by_twelve_are_default_raster() {
        let zero = FontInfoDesiredPolicy::new(false, 0, CellSize::default(), false);
        let legacy = FontInfoDesiredPolicy::new(
            true,
            0,
            CellSize {
                width: 8,
                height: 12,
            },
            false,
        );

        assert!(!zero.is_default_raster_font());
        assert!(
            FontInfoDesiredPolicy::new(true, 0, CellSize::default(), false)
                .is_default_raster_font()
        );
        assert!(legacy.is_default_raster_font());
    }

    #[test]
    fn nonblank_face_is_not_default_raster_without_engine_marker() {
        let policy = FontInfoDesiredPolicy::new(
            false,
            0,
            CellSize {
                width: 8,
                height: 12,
            },
            false,
        );

        assert!(!policy.is_default_raster_font());
    }
}
