//! Row-oriented text-buffer writes that preserve the native optional wrap flag.

use crate::row::RowError;
use crate::row_writer::replace_text_with_attribute;
use crate::text_attribute::TextAttribute;
use crate::text_buffer::TextBuffer;

/// Writes UTF-16 text into one row and optionally updates the row's forced-wrap
/// marker. `None` preserves the existing marker, matching `TextBuffer::WriteLine`.
pub fn write_line(
    buffer: &mut TextBuffer,
    x: u16,
    y: u16,
    text: &[u16],
    attribute: TextAttribute,
    wrap: Option<bool>,
) -> Result<u16, RowError> {
    let row = buffer.row_mut(i32::from(y.min(buffer.height().saturating_sub(1))));
    let end = replace_text_with_attribute(row, i32::from(x), text, attribute)?;
    if let Some(wrap) = wrap {
        row.set_wrap_forced(wrap);
    }
    Ok(end)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_text_buffer_write_line_wrap_contract() {
        let attribute = TextAttribute::default();
        let mut buffer = TextBuffer::new(80, 2, attribute).unwrap();
        let text = "hello world".encode_utf16().collect::<Vec<_>>();

        write_line(&mut buffer, 0, 0, &text, attribute, None).unwrap();
        assert!(!buffer.row(0).was_wrap_forced());

        write_line(&mut buffer, 0, 0, &text, attribute, Some(true)).unwrap();
        assert!(buffer.row(0).was_wrap_forced());

        write_line(&mut buffer, 0, 0, &text, attribute, None).unwrap();
        assert!(buffer.row(0).was_wrap_forced());

        write_line(&mut buffer, 0, 0, &text, attribute, Some(false)).unwrap();
        assert!(!buffer.row(0).was_wrap_forced());

        write_line(&mut buffer, 0, 0, &text, attribute, None).unwrap();
        assert!(!buffer.row(0).was_wrap_forced());
    }
}
