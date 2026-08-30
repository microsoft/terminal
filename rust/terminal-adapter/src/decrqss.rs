//! Portable DECRQSS response serialization for settings already owned by Rust.
//!
//! This module deliberately handles only settings whose complete state is
//! available in the portable adapter: scrolling margins and SGR attributes.
//! Cursor-style and renderer color-alias settings remain outside this owner
//! until those product surfaces are wired into Rust.

use terminal_buffer::{
    text_attribute::{TextAttribute, UnderlineStyle},
    text_color::{ColorType, TextColor},
};

use crate::adapt_dispatch::{PageGeometry, ScrollMargins};

const DCS_VALID_PREFIX: &str = "\u{1b}P1$r";
const DCS_INVALID_RESPONSE: &str = "\u{1b}P0$r\u{1b}\\";
const ST: &str = "\u{1b}\\";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DecrqssState {
    pub geometry: PageGeometry,
    pub margins: ScrollMargins,
    pub attributes: TextAttribute,
}

#[must_use]
pub fn serialize_request_setting(setting_id: &str, state: DecrqssState) -> String {
    match setting_id {
        "r" => serialize_vertical_margins(state.geometry, state.margins),
        "s" => serialize_horizontal_margins(state.geometry, state.margins),
        "m" => serialize_sgr(state.attributes),
        _ => DCS_INVALID_RESPONSE.to_owned(),
    }
}

fn serialize_vertical_margins(geometry: PageGeometry, margins: ScrollMargins) -> String {
    let (top, bottom) = margins.vertical().map_or((1, geometry.height), |range| {
        (range.start + 1, range.end + 1)
    });
    format!("{DCS_VALID_PREFIX}{top};{bottom}r{ST}")
}

fn serialize_horizontal_margins(geometry: PageGeometry, margins: ScrollMargins) -> String {
    let (left, right) = margins.horizontal().map_or((1, geometry.width), |range| {
        (range.start + 1, range.end + 1)
    });
    format!("{DCS_VALID_PREFIX}{left};{right}s{ST}")
}

fn serialize_sgr(attributes: TextAttribute) -> String {
    let mut params = vec!["0".to_owned()];

    if attributes.is_intense() {
        params.push("1".to_owned());
    }
    if attributes.is_faint() {
        params.push("2".to_owned());
    }
    if attributes.is_italic() {
        params.push("3".to_owned());
    }
    match attributes.underline_style() {
        UnderlineStyle::None => {}
        UnderlineStyle::Single => params.push("4".to_owned()),
        UnderlineStyle::Double => params.push("21".to_owned()),
        UnderlineStyle::Curly => params.push("4:3".to_owned()),
        UnderlineStyle::Dotted => params.push("4:4".to_owned()),
        UnderlineStyle::Dashed => params.push("4:5".to_owned()),
    }
    if attributes.is_blinking() {
        params.push("5".to_owned());
    }
    if attributes.is_reverse_video() {
        params.push("7".to_owned());
    }
    if attributes.is_invisible() {
        params.push("8".to_owned());
    }
    if attributes.is_crossed_out() {
        params.push("9".to_owned());
    }
    if attributes.is_overlined() {
        params.push("53".to_owned());
    }

    push_color(&mut params, attributes.foreground(), 30, 90, 38);
    push_color(&mut params, attributes.background(), 40, 100, 48);
    push_underline_color(&mut params, attributes.underline_color());

    format!("{DCS_VALID_PREFIX}{}m{ST}", params.join(";"))
}

fn push_color(params: &mut Vec<String>, color: TextColor, normal: u8, bright: u8, extended: u8) {
    match color.color_type() {
        ColorType::Default => {}
        ColorType::Index16 => {
            let index = color.index();
            let code = if index < 8 {
                normal + index
            } else {
                bright + (index - 8)
            };
            params.push(code.to_string());
        }
        ColorType::Index256 => params.push(format!("{extended}:5:{}", color.index())),
        ColorType::Rgb => {
            let rgb = color.rgb_value();
            params.push(format!("{extended}:2::{}:{}:{}", rgb.r, rgb.g, rgb.b));
        }
    }
}

fn push_underline_color(params: &mut Vec<String>, color: TextColor) {
    match color.color_type() {
        ColorType::Default => {}
        ColorType::Index16 | ColorType::Index256 => params.push(format!("58:5:{}", color.index())),
        ColorType::Rgb => {
            let rgb = color.rgb_value();
            params.push(format!("58:2::{}:{}:{}", rgb.r, rgb.g, rgb.b));
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::adapt_dispatch::AdaptDispatchCore;

    fn state(core: &AdaptDispatchCore, attributes: TextAttribute) -> DecrqssState {
        DecrqssState {
            geometry: core.geometry(),
            margins: core.margins(),
            attributes,
        }
    }

    #[test]
    fn microsoft_request_settings_reports_vertical_and_horizontal_margins() {
        let geometry = PageGeometry::new(20, 100, 25);
        let mut core = AdaptDispatchCore::new(geometry);
        assert!(core.set_top_bottom_margins(5, 10));
        assert_eq!(
            serialize_request_setting("r", state(&core, TextAttribute::default())),
            "\u{1b}P1$r5;10r\u{1b}\\"
        );

        assert!(core.set_top_bottom_margins(0, 0));
        assert_eq!(
            serialize_request_setting("r", state(&core, TextAttribute::default())),
            "\u{1b}P1$r1;25r\u{1b}\\"
        );

        assert!(core.set_mode(true, 69, true));
        assert!(core.set_left_right_margins(5, 10));
        assert_eq!(
            serialize_request_setting("s", state(&core, TextAttribute::default())),
            "\u{1b}P1$r5;10s\u{1b}\\"
        );
    }

    #[test]
    fn microsoft_request_settings_reports_rendition_flags_and_underlines() {
        let core = AdaptDispatchCore::new(PageGeometry::new(20, 100, 25));
        let mut attributes = TextAttribute::default();
        assert_eq!(
            serialize_request_setting("m", state(&core, attributes)),
            "\u{1b}P1$r0m\u{1b}\\"
        );

        attributes.set_intense(true);
        attributes.set_underline_style(UnderlineStyle::Single);
        attributes.set_reverse_video(true);
        assert_eq!(
            serialize_request_setting("m", state(&core, attributes)),
            "\u{1b}P1$r0;1;4;7m\u{1b}\\"
        );

        attributes = TextAttribute::default();
        attributes.set_underline_style(UnderlineStyle::Curly);
        assert_eq!(
            serialize_request_setting("m", state(&core, attributes)),
            "\u{1b}P1$r0;4:3m\u{1b}\\"
        );

        attributes = TextAttribute::default();
        attributes.set_faint(true);
        attributes.set_blinking(true);
        attributes.set_invisible(true);
        assert_eq!(
            serialize_request_setting("m", state(&core, attributes)),
            "\u{1b}P1$r0;2;5;8m\u{1b}\\"
        );

        attributes = TextAttribute::default();
        attributes.set_italic(true);
        attributes.set_crossed_out(true);
        assert_eq!(
            serialize_request_setting("m", state(&core, attributes)),
            "\u{1b}P1$r0;3;9m\u{1b}\\"
        );

        attributes = TextAttribute::default();
        attributes.set_underline_style(UnderlineStyle::Double);
        attributes.set_overlined(true);
        assert_eq!(
            serialize_request_setting("m", state(&core, attributes)),
            "\u{1b}P1$r0;21;53m\u{1b}\\"
        );
    }

    #[test]
    fn microsoft_request_settings_reports_indexed_and_rgb_colors() {
        let core = AdaptDispatchCore::new(PageGeometry::new(20, 100, 25));
        let mut attributes = TextAttribute::default();
        attributes.set_foreground(TextColor::index16(TextColor::DARK_YELLOW));
        attributes.set_background(TextColor::index16(TextColor::DARK_CYAN));
        assert_eq!(
            serialize_request_setting("m", state(&core, attributes)),
            "\u{1b}P1$r0;33;46m\u{1b}\\"
        );

        attributes = TextAttribute::default();
        attributes.set_foreground(TextColor::index16(TextColor::BRIGHT_CYAN));
        attributes.set_background(TextColor::index16(TextColor::BRIGHT_YELLOW));
        assert_eq!(
            serialize_request_setting("m", state(&core, attributes)),
            "\u{1b}P1$r0;96;103m\u{1b}\\"
        );

        attributes = TextAttribute::default();
        attributes.set_foreground(TextColor::index256(123));
        attributes.set_background(TextColor::index256(45));
        attributes.set_underline_color(TextColor::index256(128));
        assert_eq!(
            serialize_request_setting("m", state(&core, attributes)),
            "\u{1b}P1$r0;38:5:123;48:5:45;58:5:128m\u{1b}\\"
        );

        attributes = TextAttribute::default();
        attributes.set_foreground(TextColor::rgb(12, 34, 56));
        attributes.set_background(TextColor::rgb(65, 43, 21));
        attributes.set_underline_color(TextColor::rgb(128, 222, 45));
        assert_eq!(
            serialize_request_setting("m", state(&core, attributes)),
            "\u{1b}P1$r0;38:2::12:34:56;48:2::65:43:21;58:2::128:222:45m\u{1b}\\"
        );
    }

    #[test]
    fn unsupported_setting_uses_invalid_decrqss_response() {
        let core = AdaptDispatchCore::new(PageGeometry::new(20, 100, 25));
        assert_eq!(
            serialize_request_setting("x", state(&core, TextAttribute::default())),
            "\u{1b}P0$r\u{1b}\\"
        );
    }
}
