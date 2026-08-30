//! Safe legacy Windows code-page encoding used by host input reads.
//!
//! The host buffer owns DBCS expansion and ordering. This module owns the
//! Unicode-to-byte adapter for legacy Windows code pages without calling Win32
//! or introducing unsafe Rust.

use encoding_rs::SHIFT_JIS;

/// Windows Japanese code page (CP932 / Windows-31J).
pub const CP_JAPANESE: u32 = 932;

/// Encodes one UTF-16 code unit with a supported Windows code page.
///
/// CP932 is represented by `encoding_rs::SHIFT_JIS`, whose encoder matches the
/// Windows Japanese code page for valid Unicode-to-byte mappings. Unsupported
/// code pages, surrogate code units and unmappable characters return `None`.
#[must_use]
pub fn encode_code_unit(code_page: u32, code_unit: u16) -> Option<Vec<u8>> {
    let encoding = match code_page {
        CP_JAPANESE => SHIFT_JIS,
        _ => return None,
    };

    let character = char::from_u32(u32::from(code_unit))?;
    let mut utf8 = [0_u8; 4];
    let text = character.encode_utf8(&mut utf8);
    let (encoded, actual_encoding, had_errors) = encoding.encode(text);

    if had_errors || actual_encoding != encoding {
        return None;
    }

    Some(encoded.into_owned())
}
