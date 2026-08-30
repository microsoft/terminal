pub const LEGACY_FACE_NAME_CAPACITY: usize = 32;
const TRUE_TYPE_FAMILY_BIT: u8 = 0x04;

#[must_use]
pub fn legacy_face_name_buffer(face_name: &[u16]) -> [u16; LEGACY_FACE_NAME_CAPACITY] {
    let mut buffer = [0; LEGACY_FACE_NAME_CAPACITY];
    let copied = face_name.len().min(LEGACY_FACE_NAME_CAPACITY - 1);
    buffer[..copied].copy_from_slice(&face_name[..copied]);
    buffer
}

#[must_use]
pub const fn is_default_raster_without_size(
    face_name_empty: bool,
    family: u8,
    weight: u32,
) -> bool {
    face_name_empty && family == 0 && weight == 0
}

#[must_use]
pub const fn is_true_type_family(family: u8) -> bool {
    family & TRUE_TYPE_FAMILY_BIT != 0
}

#[cfg(test)]
mod tests {
    use super::{
        LEGACY_FACE_NAME_CAPACITY, is_default_raster_without_size, is_true_type_family,
        legacy_face_name_buffer,
    };

    #[test]
    fn legacy_name_buffer_is_zero_terminated_and_zero_filled() {
        let name = "Cascadia Mono".encode_utf16().collect::<Vec<_>>();
        let buffer = legacy_face_name_buffer(&name);

        assert_eq!(&buffer[..name.len()], name.as_slice());
        assert_eq!(buffer[name.len()], 0);
        assert!(buffer[name.len() + 1..].iter().all(|value| *value == 0));
    }

    #[test]
    fn legacy_name_buffer_reserves_the_last_cell_for_the_terminator() {
        let name = vec![65_u16; LEGACY_FACE_NAME_CAPACITY + 8];
        let buffer = legacy_face_name_buffer(&name);
        let copied = LEGACY_FACE_NAME_CAPACITY - 1;

        assert_eq!(&buffer[..copied], &name[..copied]);
        assert_eq!(buffer[copied], 0);
    }

    #[test]
    fn default_raster_without_size_requires_blank_identity_fields() {
        assert!(is_default_raster_without_size(true, 0, 0));
        assert!(!is_default_raster_without_size(false, 0, 0));
        assert!(!is_default_raster_without_size(true, 1, 0));
        assert!(!is_default_raster_without_size(true, 0, 400));
    }

    #[test]
    fn true_type_family_uses_the_family_bit() {
        assert!(is_true_type_family(0x04));
        assert!(is_true_type_family(0x84));
        assert!(!is_true_type_family(0x00));
        assert!(!is_true_type_family(0x02));
    }
}
