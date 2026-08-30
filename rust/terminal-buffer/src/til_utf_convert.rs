//! Incremental UTF-8/UTF-16 conversion semantics from TIL's `u8u16` helpers.
//!
//! The original helpers preserve an incomplete UTF sequence between calls. This
//! module keeps that state explicitly while exposing Rust-owned `Vec` results.

/// Incremental UTF-8 to UTF-16 decoder.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct Utf8ToUtf16State {
    pending: Vec<u8>,
}

impl Utf8ToUtf16State {
    /// Converts one chunk, retaining a valid but incomplete trailing UTF-8
    /// sequence for the next call.
    #[must_use]
    pub fn push(&mut self, chunk: &[u8]) -> Vec<u16> {
        let mut input = Vec::with_capacity(self.pending.len() + chunk.len());
        input.append(&mut self.pending);
        input.extend_from_slice(chunk);

        let mut output = Vec::new();
        let mut offset = 0;
        while offset < input.len() {
            match std::str::from_utf8(&input[offset..]) {
                Ok(text) => {
                    output.extend(text.encode_utf16());
                    offset = input.len();
                }
                Err(error) => {
                    let valid = error.valid_up_to();
                    if valid != 0 {
                        let text = std::str::from_utf8(&input[offset..offset + valid])
                            .expect("valid_up_to must identify valid UTF-8");
                        output.extend(text.encode_utf16());
                        offset += valid;
                    }

                    if let Some(invalid_len) = error.error_len() {
                        output.push(0xfffd);
                        offset += invalid_len;
                    } else {
                        self.pending.extend_from_slice(&input[offset..]);
                        break;
                    }
                }
            }
        }
        output
    }

    /// Finishes the stream. An incomplete trailing sequence is represented by
    /// one replacement character, matching lossy Unicode conversion semantics.
    #[must_use]
    pub fn finish(&mut self) -> Vec<u16> {
        if self.pending.is_empty() {
            Vec::new()
        } else {
            self.pending.clear();
            vec![0xfffd]
        }
    }
}

/// Incremental UTF-16 to UTF-8 decoder.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct Utf16ToUtf8State {
    pending_high_surrogate: Option<u16>,
}

impl Utf16ToUtf8State {
    /// Converts one chunk, retaining a trailing high surrogate for the next
    /// call so a pair split across chunk boundaries remains lossless.
    #[must_use]
    pub fn push(&mut self, chunk: &[u16]) -> Vec<u8> {
        let mut input =
            Vec::with_capacity(chunk.len() + usize::from(self.pending_high_surrogate.is_some()));
        if let Some(high) = self.pending_high_surrogate.take() {
            input.push(high);
        }
        input.extend_from_slice(chunk);

        if input
            .last()
            .is_some_and(|unit| (0xd800..=0xdbff).contains(unit))
        {
            self.pending_high_surrogate = input.pop();
        }

        let text: String = char::decode_utf16(input)
            .map(|item| item.unwrap_or(char::REPLACEMENT_CHARACTER))
            .collect();
        text.into_bytes()
    }

    /// Finishes the stream, replacing a dangling high surrogate when present.
    #[must_use]
    pub fn finish(&mut self) -> Vec<u8> {
        if self.pending_high_surrogate.take().is_some() {
            char::REPLACEMENT_CHARACTER.to_string().into_bytes()
        } else {
            Vec::new()
        }
    }
}

/// Converts a complete UTF-8 buffer to UTF-16.
#[must_use]
pub fn utf8_to_utf16(input: &[u8]) -> Vec<u16> {
    let mut state = Utf8ToUtf16State::default();
    let mut output = state.push(input);
    output.extend(state.finish());
    output
}

/// Converts a complete UTF-16 buffer to UTF-8.
#[must_use]
pub fn utf16_to_utf8(input: &[u16]) -> Vec<u8> {
    let mut state = Utf16ToUtf8State::default();
    let mut output = state.push(input);
    output.extend(state.finish());
    output
}

#[cfg(test)]
mod tests {
    use super::*;

    const CJK_24F5C_UTF8: &[u8] = &[0xf0, 0xa4, 0xbd, 0x9c];
    const CJK_24F5C_UTF16: &[u16] = &[0xd853, 0xdf5c];

    #[test]
    fn microsoft_til_u8u16_test_u8_to_u16() {
        let input = [0x7e, 0xc3, 0xb6, 0xe2, 0x82, 0xac, 0xf0, 0xa4, 0xbd, 0x9c];
        assert_eq!(
            utf8_to_utf16(&input),
            [0x007e, 0x00f6, 0x20ac, 0xd853, 0xdf5c]
        );
    }

    #[test]
    fn microsoft_til_u8u16_test_u16_to_u8() {
        let input = [0x007e, 0x00f6, 0x20ac, 0xd853, 0xdf5c];
        assert_eq!(
            utf16_to_utf8(&input),
            [0x7e, 0xc3, 0xb6, 0xe2, 0x82, 0xac, 0xf0, 0xa4, 0xbd, 0x9c]
        );
    }

    #[test]
    fn microsoft_til_u8u16_test_u8_to_u16_partials() {
        let mut state = Utf8ToUtf16State::default();
        assert_eq!(
            state.push(&[0xf0, 0xa4, 0xbd, 0x9c, 0xf0, 0xa4, 0xbd]),
            CJK_24F5C_UTF16
        );
        assert_eq!(state.push(&[0x9c]), CJK_24F5C_UTF16);

        assert_eq!(state.push(&[0xe2]), []);
        assert_eq!(state.push(&[0x98, 0xba]), [0x263a]);
    }

    #[test]
    fn microsoft_til_u8u16_test_u16_to_u8_partials() {
        let mut state = Utf16ToUtf8State::default();
        assert_eq!(state.push(&[0xd853, 0xdf5c, 0xd853]), CJK_24F5C_UTF8);
        assert_eq!(state.push(&[0xdf5c]), CJK_24F5C_UTF8);
    }

    #[test]
    fn microsoft_til_u8u16_test_u8_to_u16_one_by_one() {
        let mut state = Utf8ToUtf16State::default();
        assert_eq!(state.push(&[0xf0]), []);
        assert_eq!(state.push(&[0x9f]), []);
        assert_eq!(state.push(&[0x93]), []);
        assert_eq!(state.push(&[0xb7]), [0xd83d, 0xdcf7]);
    }
}
