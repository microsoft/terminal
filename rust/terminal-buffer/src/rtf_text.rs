//! RTF text escaping for clipboard serialization.
//!
//! Windows Terminal serializes UTF-16 code units directly into RTF. Printable
//! ASCII is emitted as-is except for the three RTF metacharacters, while every
//! non-ASCII code unit uses the signed 16-bit `\uN?` representation. Surrogate
//! pairs therefore become two RTF Unicode escapes, matching the native host.

/// Appends UTF-16 text using Windows Terminal's RTF escaping contract.
pub fn append_rtf_text(output: &mut String, text: &[u16]) {
    for &unit in text {
        match unit {
            value if value == u16::from(b'\\') => output.push_str("\\\\"),
            value if value == u16::from(b'{') => output.push_str("\\{"),
            value if value == u16::from(b'}') => output.push_str("\\}"),
            0x20..=0x7e => output.push(char::from(u8::try_from(unit).expect("ASCII unit fits"))),
            _ => {
                let signed = i16::from_ne_bytes(unit.to_ne_bytes());
                use core::fmt::Write;
                write!(output, "\\u{signed}?").expect("writing to String is infallible");
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_text_buffer_append_rtf_text_contract() {
        let mut output = String::new();
        append_rtf_text(
            &mut output,
            &"This is some Ascii \\ {}"
                .encode_utf16()
                .collect::<Vec<_>>(),
        );
        assert_eq!(output, "This is some Ascii \\\\ \\{\\}");

        output.clear();
        append_rtf_text(
            &mut output,
            &[
                0x004c, 0x006f, 0x0077, 0x0020, 0x0063, 0x006f, 0x0064, 0x0065, 0x0020, 0x0075,
                0x006e, 0x0069, 0x0074, 0x0073, 0x003a, 0x0020, 0x00e1, 0x0020, 0x00e9, 0x0020,
                0x00ed, 0x0020, 0x00f3, 0x0020, 0x00fa, 0x0020, 0x2b81, 0x0020, 0x2b82,
            ],
        );
        assert_eq!(
            output,
            "Low code units: \\u225? \\u233? \\u237? \\u243? \\u250? \\u11137? \\u11138?"
        );

        output.clear();
        append_rtf_text(
            &mut output,
            &"High code units: "
                .encode_utf16()
                .chain([0xa7b5, 0x0020, 0xa7b7])
                .collect::<Vec<_>>(),
        );
        assert_eq!(output, "High code units: \\u-22603? \\u-22601?");

        output.clear();
        append_rtf_text(
            &mut output,
            &"Surrogates: "
                .encode_utf16()
                .chain([
                    0xd83c, 0xdf66, 0x0020, 0xd83d, 0xdc7e, 0x0020, 0xd83d, 0xdc40,
                ])
                .collect::<Vec<_>>(),
        );
        assert_eq!(
            output,
            "Surrogates: \\u-10180?\\u-8346? \\u-10179?\\u-9090? \\u-10179?\\u-9152?"
        );
    }
}
