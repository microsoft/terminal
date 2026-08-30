use terminal_settings::color_scheme::{Color, ColorScheme};
use terminal_settings::settings_json;

const CAMPBELL: &str = r##"{
    "background" : "#0C0C0C",
    "black" : "#0C0C0C",
    "blue" : "#0037DA",
    "brightBlack" : "#767676",
    "brightBlue" : "#3B78FF",
    "brightCyan" : "#61D6D6",
    "brightGreen" : "#16C60C",
    "brightPurple" : "#B4009E",
    "brightRed" : "#E74856",
    "brightWhite" : "#F2F2F2",
    "brightYellow" : "#F9F1A5",
    "cursorColor" : "#FFFFFF",
    "cyan" : "#3A96DD",
    "foreground" : "#F2F2F2",
    "green" : "#13A10E",
    "name" : "Campbell",
    "purple" : "#881798",
    "red" : "#C50F1F",
    "selectionBackground" : "#131313",
    "white" : "#CCCCCC",
    "yellow" : "#C19C00"
}"##;

#[test]
fn microsoft_color_scheme_parse_simple_color_scheme() {
    let scheme = ColorScheme::from_json(CAMPBELL).expect("Microsoft Campbell scheme should parse");

    assert_eq!(scheme.name(), "Campbell");
    assert_eq!(scheme.foreground(), Color::rgb(0xf2, 0xf2, 0xf2));
    assert_eq!(scheme.background(), Color::rgb(0x0c, 0x0c, 0x0c));
    assert_eq!(scheme.selection_background(), Color::rgb(0x13, 0x13, 0x13));
    assert_eq!(scheme.cursor_color(), Color::rgb(0xff, 0xff, 0xff));

    assert_eq!(
        scheme.table(),
        &[
            Color::rgb(0x0c, 0x0c, 0x0c),
            Color::rgb(0xc5, 0x0f, 0x1f),
            Color::rgb(0x13, 0xa1, 0x0e),
            Color::rgb(0xc1, 0x9c, 0x00),
            Color::rgb(0x00, 0x37, 0xda),
            Color::rgb(0x88, 0x17, 0x98),
            Color::rgb(0x3a, 0x96, 0xdd),
            Color::rgb(0xcc, 0xcc, 0xcc),
            Color::rgb(0x76, 0x76, 0x76),
            Color::rgb(0xe7, 0x48, 0x56),
            Color::rgb(0x16, 0xc6, 0x0c),
            Color::rgb(0xf9, 0xf1, 0xa5),
            Color::rgb(0x3b, 0x78, 0xff),
            Color::rgb(0xb4, 0x00, 0x9e),
            Color::rgb(0x61, 0xd6, 0xd6),
            Color::rgb(0xf2, 0xf2, 0xf2),
        ]
    );

    let input = settings_json::parse(CAMPBELL).expect("Microsoft Campbell JSON should parse");
    assert_eq!(scheme.to_json_value(), input);
}
