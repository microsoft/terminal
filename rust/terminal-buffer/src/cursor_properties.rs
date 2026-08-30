//! Cursor presentation copying shared by text-buffer transitions.
//!
//! Screen buffers own cursor position independently, but presentation properties
//! follow the source cursor when buffer properties are copied. Keeping this
//! operation explicit prevents position from being accidentally overwritten.

use crate::alternate_buffer::CursorState;

/// Copies cursor presentation while preserving the target buffer's position.
pub fn copy_cursor_properties(target: &mut CursorState, source: CursorState) {
    target.visible = source.visible;
    target.size = source.size;
    target.shape = source.shape;
    target.blinking = source.blinking;
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::alternate_buffer::CursorShape;

    #[test]
    fn microsoft_text_buffer_copy_properties_contract() {
        let mut target = CursorState {
            x: 7,
            y: 9,
            visible: false,
            size: 10,
            shape: CursorShape::Legacy,
            blinking: false,
        };
        let source = CursorState {
            x: 1,
            y: 2,
            visible: true,
            size: 50,
            shape: CursorShape::DoubleUnderscore,
            blinking: true,
        };

        copy_cursor_properties(&mut target, source);

        assert_eq!((target.x, target.y), (7, 9));
        assert!(target.visible);
        assert_eq!(target.size, 50);
        assert_eq!(target.shape, CursorShape::DoubleUnderscore);
        assert!(target.blinking);
    }
}
