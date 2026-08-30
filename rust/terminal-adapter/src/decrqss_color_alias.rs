//! Portable DECRQSS serialization for DECAC color-alias settings.
//!
//! DECAC reports color-table indices, not platform `COLORREF` values. Keeping
//! the four aliases as explicit value state lets the VT response layer remain
//! deterministic while renderer storage/invalidation stays outside the adapter.

const DCS_VALID_PREFIX: &str = "\u{1b}P1$r";
const DCS_INVALID_RESPONSE: &str = "\u{1b}P0$r\u{1b}\\";
const ST: &str = "\u{1b}\\";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ColorAliasIndices {
    pub default_foreground: usize,
    pub default_background: usize,
    pub frame_foreground: usize,
    pub frame_background: usize,
}

impl Default for ColorAliasIndices {
    fn default() -> Self {
        Self {
            default_foreground: 7,
            default_background: 0,
            frame_foreground: 263,
            frame_background: 264,
        }
    }
}

/// Serializes the DECAC branch of DECRQSS (`CSI Ps , |`).
///
/// An omitted item is equivalent to item 1 (normal text), item 2 reports the
/// window-frame aliases, and all other items use the DECRQSS invalid response.
#[must_use]
pub fn serialize_decac(item: Option<u16>, aliases: ColorAliasIndices) -> String {
    let (item, foreground, background) = match item.unwrap_or(1) {
        1 => (1, aliases.default_foreground, aliases.default_background),
        2 => (2, aliases.frame_foreground, aliases.frame_background),
        _ => return DCS_INVALID_RESPONSE.to_owned(),
    };

    format!("{DCS_VALID_PREFIX}{item};{foreground};{background},|{ST}")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn microsoft_request_settings_reports_decac_alias_indices() {
        let aliases = ColorAliasIndices {
            default_foreground: 3,
            default_background: 5,
            frame_foreground: 4,
            frame_background: 6,
        };

        assert_eq!(serialize_decac(None, aliases), "\u{1b}P1$r1;3;5,|\u{1b}\\");
        assert_eq!(
            serialize_decac(Some(1), aliases),
            "\u{1b}P1$r1;3;5,|\u{1b}\\"
        );
        assert_eq!(
            serialize_decac(Some(2), aliases),
            "\u{1b}P1$r2;4;6,|\u{1b}\\"
        );
        assert_eq!(serialize_decac(Some(3), aliases), "\u{1b}P0$r\u{1b}\\");
    }
}
