//! Windows Terminal-compatible Base64 decoding.
//!
//! This module intentionally preserves the observable behavior of
//! `Microsoft::Console::VirtualTerminal::Base64::Decode`, including its
//! permissive handling of padding in the final tail of the input.

use std::fmt;

/// Error returned by Windows Terminal-compatible Base64 decoding.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DecodeError {
    /// The input is not accepted by Windows Terminal's Base64 decoder.
    InvalidBase64,
    /// The decoded bytes are not valid UTF-8.
    InvalidUtf8,
}

impl fmt::Display for DecodeError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidBase64 => formatter.write_str("invalid Base64 input"),
            Self::InvalidUtf8 => formatter.write_str("decoded Base64 is not valid UTF-8"),
        }
    }
}

impl std::error::Error for DecodeError {}

/// Decodes text using the same Base64 rules as Windows Terminal.
///
/// Both RFC 4648 alphabets are accepted (`+/` and `-_`). Whitespace and
/// non-ASCII input are rejected. Padding is handled compatibly with the
/// existing C++ implementation rather than normalized to stricter RFC rules.
///
/// # Errors
///
/// Returns [`DecodeError::InvalidBase64`] for an input rejected by the C++
/// decoder, or [`DecodeError::InvalidUtf8`] when the decoded bytes cannot be
/// converted from UTF-8 text.
pub fn decode(input: &str) -> Result<String, DecodeError> {
    let utf16 = input.encode_utf16().collect::<Vec<_>>();
    decode_utf16(&utf16)
}

/// Decodes UTF-16 code units using Windows Terminal's Base64 rules.
///
/// This entry point mirrors the C++ decoder's `std::wstring_view` input more
/// closely than [`decode`] and will remain useful when the C ABI boundary is
/// introduced later in R01.
///
/// # Errors
///
/// Returns [`DecodeError::InvalidBase64`] for an input rejected by the C++
/// decoder, including any UTF-16 code unit outside the ASCII Base64 alphabet,
/// or [`DecodeError::InvalidUtf8`] when the decoded bytes are not valid UTF-8.
pub fn decode_utf16(input: &[u16]) -> Result<String, DecodeError> {
    let bytes = decode_bytes_utf16(input)?;
    String::from_utf8(bytes).map_err(|_| DecodeError::InvalidUtf8)
}

fn decode_bytes_utf16(input: &[u16]) -> Result<Vec<u8>, DecodeError> {
    let mut output = Vec::with_capacity(input.len().div_ceil(4) * 3);
    let mut index = 0usize;

    // The C++ implementation deliberately keeps the final five code units out
    // of the loop condition. That guarantees ordinary trailing padding reaches
    // the tail path while preserving some historical padding quirks. We mirror
    // that split because it is externally observable for malformed inputs.
    let batched_end = input.len().saturating_sub(5);

    while index < batched_end {
        let mut remainder = 0u32;

        for _ in 0..4 {
            let value = decode_value(input[index])?;
            index += 1;
            remainder = (remainder << 6) | u32::from(value);
        }

        output.push(shifted_byte(remainder, 16));
        output.push(shifted_byte(remainder, 8));
        output.push(shifted_byte(remainder, 0));
    }

    let mut remainder = 0u32;
    let mut tail_count = 0u8;

    while index < input.len() {
        let code_unit = input[index];
        index += 1;

        // This matches the existing C++ tail loop exactly: '=' is skipped no
        // matter where it occurs inside the final tail, not only at the end.
        if code_unit == u16::from(b'=') {
            continue;
        }

        let value = decode_value(code_unit)?;
        remainder = (remainder << 6) | u32::from(value);
        tail_count += 1;
    }

    match tail_count {
        0 => {}
        2 => output.push(shifted_byte(remainder, 4)),
        3 => {
            output.push(shifted_byte(remainder, 10));
            output.push(shifted_byte(remainder, 2));
        }
        4 => {
            output.push(shifted_byte(remainder, 16));
            output.push(shifted_byte(remainder, 8));
            output.push(shifted_byte(remainder, 0));
        }
        _ => return Err(DecodeError::InvalidBase64),
    }

    Ok(output)
}

fn shifted_byte(value: u32, shift: u32) -> u8 {
    (value >> shift).to_le_bytes()[0]
}

fn decode_value(code_unit: u16) -> Result<u8, DecodeError> {
    let byte = u8::try_from(code_unit).map_err(|_| DecodeError::InvalidBase64)?;

    let value = match byte {
        b'+' | b'-' => 62,
        b'/' | b'_' => 63,
        b'A'..=b'Z' => byte - b'A',
        b'a'..=b'z' => byte - b'a' + 26,
        b'0'..=b'9' => byte - b'0' + 52,
        _ => return Err(DecodeError::InvalidBase64),
    };

    Ok(value)
}

#[cfg(test)]
mod tests {
    use super::{DecodeError, decode, decode_utf16};

    #[test]
    fn decodes_rfc_4648_vectors_with_and_without_padding() {
        let vectors = [
            ("", ""),
            ("Zg==", "f"),
            ("Zm8=", "fo"),
            ("Zm9v", "foo"),
            ("Zm9vYg==", "foob"),
            ("Zm9vYmE=", "fooba"),
            ("Zm9vYmFy", "foobar"),
            ("Zg", "f"),
            ("Zm8", "fo"),
            ("Zm9vYg", "foob"),
            ("Zm9vYmE", "fooba"),
        ];

        for (encoded, expected) in vectors {
            assert_eq!(decode(encoded), Ok(expected.to_owned()), "{encoded}");
        }
    }

    #[test]
    fn decodes_base64url_alphabet() {
        assert_eq!(decode("8J-RjQ=="), Ok("👍".to_owned()));
        assert_eq!(decode("4KC_"), Ok("࠿".to_owned()));
    }

    #[test]
    fn matches_windows_terminal_unicode_vectors() {
        assert_eq!(
            decode("44Gr44G744KT44GU5rGJ6K+t7ZWc6rWt"),
            Ok("にほんご汉语한국".to_owned())
        );
        assert_eq!(
            decode("8J+RjfCfkY3wn4+78J+RjfCfj7zwn5GN8J+PvfCfkY3wn4++8J+RjfCfj78="),
            Ok("👍👍🏻👍🏼👍🏽👍🏾👍🏿".to_owned())
        );
    }

    #[test]
    fn preserves_tail_padding_quirks() {
        // The C++ decoder skips '=' anywhere in its final tail.
        assert_eq!(decode("Y=Q="), Ok("a".to_owned()));
        assert_eq!(decode("YQ==="), Ok("a".to_owned()));
        assert_eq!(decode("="), Ok(String::new()));
        assert_eq!(decode("====="), Ok(String::new()));

        // Six code units cross the C++ decoder's batched-loop boundary, so
        // padding is no longer ignored in the same way.
        assert_eq!(decode("YQ===="), Err(DecodeError::InvalidBase64));
        assert_eq!(decode("======"), Err(DecodeError::InvalidBase64));
    }

    #[test]
    fn rejects_invalid_base64_input() {
        for invalid in ["A", "abcde", "YW Jj", "YWJj\n", "YWJj?", "é"] {
            assert_eq!(
                decode(invalid),
                Err(DecodeError::InvalidBase64),
                "{invalid:?}"
            );
        }

        assert_eq!(decode_utf16(&[0xd800]), Err(DecodeError::InvalidBase64));
    }

    #[test]
    fn rejects_decoded_bytes_that_are_not_utf8() {
        assert_eq!(decode("/w=="), Err(DecodeError::InvalidUtf8));
    }

    #[test]
    fn deterministic_ascii_round_trips_match_reference_encoding() {
        let mut state = 0x9e37_79b9_7f4a_7c15u64;

        for length in 0..=128usize {
            let mut reference = vec![0u8; length];
            for byte in &mut reference {
                state ^= state << 13;
                state ^= state >> 7;
                state ^= state << 17;
                *byte = state.to_le_bytes()[0] & 0x7f;
            }

            let encoded = encode_reference(&reference);
            let expected = String::from_utf8(reference).expect("ASCII test data is valid UTF-8");
            assert_eq!(decode(&encoded), Ok(expected.clone()), "length={length}");

            let without_padding = encoded.trim_end_matches('=');
            assert_eq!(
                decode(without_padding),
                Ok(expected),
                "unpadded length={length}"
            );
        }
    }

    fn encode_reference(input: &[u8]) -> String {
        let mut encoded = String::with_capacity(input.len().div_ceil(3) * 4);
        let mut chunks = input.chunks_exact(3);

        for chunk in &mut chunks {
            let value =
                (u32::from(chunk[0]) << 16) | (u32::from(chunk[1]) << 8) | u32::from(chunk[2]);
            push_alphabet(&mut encoded, value >> 18);
            push_alphabet(&mut encoded, value >> 12);
            push_alphabet(&mut encoded, value >> 6);
            push_alphabet(&mut encoded, value);
        }

        match chunks.remainder() {
            [] => {}
            [first] => {
                let value = u32::from(*first) << 16;
                push_alphabet(&mut encoded, value >> 18);
                push_alphabet(&mut encoded, value >> 12);
                encoded.push('=');
                encoded.push('=');
            }
            [first, second] => {
                let value = (u32::from(*first) << 16) | (u32::from(*second) << 8);
                push_alphabet(&mut encoded, value >> 18);
                push_alphabet(&mut encoded, value >> 12);
                push_alphabet(&mut encoded, value >> 6);
                encoded.push('=');
            }
            _ => unreachable!("chunks_exact(3) remainder is shorter than three bytes"),
        }

        encoded
    }

    fn push_alphabet(encoded: &mut String, index: u32) {
        const ALPHABET: &[u8; 64] =
            b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        let index = usize::try_from(index & 0x3f).expect("six-bit Base64 index fits usize");
        encoded.push(char::from(ALPHABET[index]));
    }
}
