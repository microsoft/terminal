use terminal_settings::color_scheme::{Color, ColorSchemeCollection};

const INBOX: &str = r##"{
    "schemes": [{
        "background": "#0C0C0C", "black": "#0C0C0C", "blue": "#0037DA",
        "brightBlack": "#767676", "brightBlue": "#3B78FF", "brightCyan": "#61D6D6",
        "brightGreen": "#16C60C", "brightPurple": "#B4009E", "brightRed": "#E74856",
        "brightWhite": "#F2F2F2", "brightYellow": "#F9F1A5", "cursorColor": "#FFFFFF",
        "cyan": "#3A96DD", "foreground": "#CCCCCC", "green": "#13A10E",
        "name": "Campbell", "purple": "#881798", "red": "#C50F1F",
        "selectionBackground": "#FFFFFF", "white": "#CCCCCC", "yellow": "#C19C00"
    }]
}"##;

const USER: &str = r##"{
    "profiles": [{ "name": "profile0" }],
    "schemes": [{
        "background": "#012456", "black": "#0C0C0C", "blue": "#0037DA",
        "brightBlack": "#767676", "brightBlue": "#3B78FF", "brightCyan": "#61D6D6",
        "brightGreen": "#16C60C", "brightPurple": "#B4009E", "brightRed": "#E74856",
        "brightWhite": "#F2F2F2", "brightYellow": "#F9F1A5", "cursorColor": "#FFFFFF",
        "cyan": "#3A96DD", "foreground": "#CCCCCC", "green": "#13A10E",
        "name": "Campbell Powershell", "purple": "#881798", "red": "#C50F1F",
        "selectionBackground": "#FFFFFF", "white": "#CCCCCC", "yellow": "#C19C00"
    }]
}"##;

#[test]
fn microsoft_color_scheme_layer_color_schemes_on_array() {
    let schemes = ColorSchemeCollection::from_inbox_and_user_json(INBOX, USER)
        .expect("Microsoft non-colliding scheme layers should compose");

    assert_eq!(schemes.len(), 2);

    let campbell = schemes.get("Campbell").expect("Campbell should exist");
    assert_eq!(campbell.foreground(), Color::rgb(0xcc, 0xcc, 0xcc));
    assert_eq!(campbell.background(), Color::rgb(0x0c, 0x0c, 0x0c));

    let powershell = schemes
        .get("Campbell Powershell")
        .expect("Campbell Powershell should exist");
    assert_eq!(powershell.foreground(), Color::rgb(0xcc, 0xcc, 0xcc));
    assert_eq!(powershell.background(), Color::rgb(0x01, 0x24, 0x56));
}
