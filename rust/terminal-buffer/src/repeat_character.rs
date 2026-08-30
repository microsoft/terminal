//! Portable VT REP (Repeat Previous Graphic Character) state.
//!
//! REP is intentionally stateful: it repeats only the most recently written
//! graphic character, a non-graphic/VT action invalidates that eligibility, and
//! REP itself consumes the eligibility so a second REP without another graphic
//! character is a no-op. This mirrors the host behavior exercised by
//! `TextBufferTests::TestRepeatCharacter` without coupling the buffer to a parser.

use crate::row::RowError;
use crate::row_writer::replace_text_with_attribute;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::{TextBuffer, TextBufferPoint};

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct RepeatCharacterState {
    last_graphic: Vec<u16>,
    eligible: bool,
}

impl RepeatCharacterState {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            last_graphic: Vec::new(),
            eligible: false,
        }
    }

    /// Writes one graphic glyph and makes it eligible for a following REP.
    pub fn write_graphic(
        &mut self,
        buffer: &mut TextBuffer,
        cursor: &mut TextBufferPoint,
        glyph: &[u16],
        attribute: TextAttribute,
    ) -> Result<(), RowError> {
        if glyph.is_empty() {
            self.invalidate();
            return Ok(());
        }

        let y = cursor.y.min(buffer.height().saturating_sub(1));
        let end = replace_text_with_attribute(
            buffer.row_mut(i32::from(y)),
            i32::from(cursor.x),
            glyph,
            attribute,
        )?;
        cursor.x = end.min(buffer.width());
        cursor.y = y;
        self.last_graphic.clear();
        self.last_graphic.extend_from_slice(glyph);
        self.eligible = true;
        Ok(())
    }

    /// Invalidates REP after a cursor/control/VT action that is not a graphic
    /// write. The remembered glyph is kept only for diagnostics; it is no longer
    /// eligible to repeat.
    pub fn invalidate(&mut self) {
        self.eligible = false;
    }

    /// Repeats the eligible glyph `count` times. A zero count performs no write;
    /// callers that decode ECMA-48's omitted/zero parameter should normalize it
    /// to one before calling this buffer-side owner.
    pub fn repeat(
        &mut self,
        buffer: &mut TextBuffer,
        cursor: &mut TextBufferPoint,
        count: u16,
        attribute: TextAttribute,
    ) -> Result<(), RowError> {
        if !self.eligible || self.last_graphic.is_empty() || count == 0 {
            return Ok(());
        }

        let glyph = self.last_graphic.clone();
        self.eligible = false;
        for _ in 0..count {
            let y = cursor.y.min(buffer.height().saturating_sub(1));
            if cursor.x >= buffer.width() {
                break;
            }
            let end = replace_text_with_attribute(
                buffer.row_mut(i32::from(y)),
                i32::from(cursor.x),
                &glyph,
                attribute,
            )?;
            cursor.x = end.min(buffer.width());
            cursor.y = y;
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_text_buffer_repeat_character_contract() {
        let attribute = TextAttribute::default();
        let mut buffer = TextBuffer::new(16, 6, attribute).unwrap();
        let mut cursor = TextBufferPoint::new(0, 0);
        let mut repeat = RepeatCharacterState::new();

        repeat
            .write_graphic(&mut buffer, &mut cursor, &[u16::from(b'X')], attribute)
            .unwrap();
        repeat
            .repeat(&mut buffer, &mut cursor, 1, attribute)
            .unwrap();
        assert_eq!(cursor, TextBufferPoint::new(2, 0));
        assert_eq!(
            buffer.row(0).text_range(0, 3),
            &['X' as u16, 'X' as u16, ' ' as u16]
        );

        // A non-graphic action between the write and REP invalidates the repeat.
        cursor = TextBufferPoint::new(0, 1);
        repeat
            .write_graphic(&mut buffer, &mut cursor, &[u16::from(b'A')], attribute)
            .unwrap();
        repeat
            .write_graphic(&mut buffer, &mut cursor, &[u16::from(b'B')], attribute)
            .unwrap();
        repeat.invalidate();
        repeat
            .repeat(&mut buffer, &mut cursor, 1, attribute)
            .unwrap();
        assert_eq!(
            buffer.row(1).text_range(0, 3),
            &['A' as u16, 'B' as u16, ' ' as u16]
        );

        cursor = TextBufferPoint::new(0, 2);
        repeat
            .write_graphic(&mut buffer, &mut cursor, &[u16::from(b'C')], attribute)
            .unwrap();
        repeat
            .repeat(&mut buffer, &mut cursor, 5, attribute)
            .unwrap();
        assert_eq!(cursor, TextBufferPoint::new(6, 2));
        assert_eq!(
            buffer.row(2).text_range(0, 7),
            &[
                'C' as u16, 'C' as u16, 'C' as u16, 'C' as u16, 'C' as u16, 'C' as u16, ' ' as u16
            ]
        );

        // A line/control action invalidates the previous graphic.
        cursor = TextBufferPoint::new(0, 4);
        repeat
            .write_graphic(&mut buffer, &mut cursor, &[u16::from(b'D')], attribute)
            .unwrap();
        repeat.invalidate();
        repeat
            .repeat(&mut buffer, &mut cursor, 1, attribute)
            .unwrap();
        assert_eq!(cursor, TextBufferPoint::new(1, 4));

        // REP is single-use until another graphic character arrives.
        cursor = TextBufferPoint::new(0, 5);
        repeat
            .write_graphic(&mut buffer, &mut cursor, &[u16::from(b'E')], attribute)
            .unwrap();
        repeat
            .repeat(&mut buffer, &mut cursor, 1, attribute)
            .unwrap();
        assert_eq!(cursor, TextBufferPoint::new(2, 5));
        repeat
            .repeat(&mut buffer, &mut cursor, 1, attribute)
            .unwrap();
        assert_eq!(cursor, TextBufferPoint::new(2, 5));
        assert_eq!(
            buffer.row(5).text_range(0, 3),
            &['E' as u16, 'E' as u16, ' ' as u16]
        );
    }
}
