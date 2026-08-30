//! Deterministic host formatting of legacy console attributes as VT SGR.

use terminal_buffer::text_attribute::TextAttribute;

const SGR_FOREGROUND: [u8; 16] = [
    30, 31, 32, 33, 34, 35, 36, 37, 90, 91, 92, 93, 94, 95, 96, 97,
];

/// Formats the subset of `TextAttribute` emitted by host `VtIo::FormatAttributes`.
///
/// The host always starts with SGR 0 because `SetConsoleTextAttribute` replaces
/// the full active rendition, including VT-only state that is not represented in
/// the legacy API. Reverse video and legacy foreground/background colors are then
/// appended using the same ANSI indices as the C++ implementation.
#[must_use]
pub fn format_attributes(attributes: TextAttribute) -> String {
    let mut output = String::from("\x1b[0");

    if attributes.is_reverse_video() {
        output.push_str(";7");
    }

    let foreground = attributes.foreground();
    if foreground.is_legacy() {
        let index = usize::from(foreground.index());
        output.push(';');
        output.push_str(&SGR_FOREGROUND[index].to_string());
    }

    let background = attributes.background();
    if background.is_legacy() {
        let index = usize::from(background.index());
        output.push(';');
        output.push_str(&(SGR_FOREGROUND[index] + 10).to_string());
    }

    output.push('m');
    output
}

#[cfg(test)]
mod tests {
    use super::*;
    use terminal_buffer::text_attribute::LegacyColorDefaults;
    use terminal_buffer::text_color::TextColor;

    #[test]
    fn default_attributes_only_reset_rendition() {
        assert_eq!(format_attributes(TextAttribute::default()), "\x1b[0m");
    }

    #[test]
    fn reverse_video_is_emitted_after_reset() {
        let mut attributes = TextAttribute::default();
        attributes.set_reverse_video(true);
        assert_eq!(format_attributes(attributes), "\x1b[0;7m");
    }

    #[test]
    fn ansi_legacy_colors_match_host_sgr_table() {
        let mut attributes = TextAttribute::default();
        attributes.set_foreground(TextColor::index16(TextColor::BRIGHT_RED));
        attributes.set_background(TextColor::index16(TextColor::DARK_BLUE));
        assert_eq!(format_attributes(attributes), "\x1b[0;91;44m");
    }

    #[test]
    fn legacy_windows_color_order_is_transposed_before_formatting() {
        let attributes = TextAttribute::from_legacy(0x0041, LegacyColorDefaults::default());
        assert_eq!(format_attributes(attributes), "\x1b[0;34;41m");
    }

    #[test]
    fn nonlegacy_colors_are_not_emitted_by_this_legacy_host_contract() {
        let mut attributes = TextAttribute::default();
        attributes.set_foreground(TextColor::rgb(1, 2, 3));
        attributes.set_background(TextColor::index256(42));
        assert_eq!(format_attributes(attributes), "\x1b[0m");
    }

    #[test]
    fn microsoft_vt_io_set_console_text_attribute_matches_exact_vectors() {
        let defaults = LegacyColorDefaults::default();
        let expected_foregrounds = [
            "\x1b[0;30;41m",
            "\x1b[0;34;41m",
            "\x1b[0;32;41m",
            "\x1b[0;36;41m",
            "\x1b[0;31;41m",
            "\x1b[0;35;41m",
            "\x1b[0;33;41m",
            "\x1b[0;41m",
            "\x1b[0;90;41m",
            "\x1b[0;94;41m",
            "\x1b[0;92;41m",
            "\x1b[0;96;41m",
            "\x1b[0;91;41m",
            "\x1b[0;95;41m",
            "\x1b[0;93;41m",
            "\x1b[0;97;41m",
        ];
        for (foreground, expected) in expected_foregrounds.into_iter().enumerate() {
            let legacy = u16::try_from(foreground).expect("foreground index fits") | 0x0040;
            assert_eq!(
                format_attributes(TextAttribute::from_legacy(legacy, defaults)),
                expected
            );
        }

        let expected_backgrounds = [
            "\x1b[0;31m",
            "\x1b[0;31;44m",
            "\x1b[0;31;42m",
            "\x1b[0;31;46m",
            "\x1b[0;31;41m",
            "\x1b[0;31;45m",
            "\x1b[0;31;43m",
            "\x1b[0;31;47m",
            "\x1b[0;31;100m",
            "\x1b[0;31;104m",
            "\x1b[0;31;102m",
            "\x1b[0;31;106m",
            "\x1b[0;31;101m",
            "\x1b[0;31;105m",
            "\x1b[0;31;103m",
            "\x1b[0;31;107m",
        ];
        for (background, expected) in expected_backgrounds.into_iter().enumerate() {
            let legacy = (u16::try_from(background).expect("background index fits") << 4) | 0x0004;
            assert_eq!(
                format_attributes(TextAttribute::from_legacy(legacy, defaults)),
                expected
            );
        }

        assert_eq!(
            format_attributes(TextAttribute::from_legacy(0x402d, defaults)),
            "\x1b[0;7;95;42m"
        );
        assert_eq!(
            format_attributes(TextAttribute::from_legacy(0x4007, defaults)),
            "\x1b[0;7m"
        );
    }
}
