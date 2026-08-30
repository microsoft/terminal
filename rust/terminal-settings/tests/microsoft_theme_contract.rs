use terminal_settings::theme::{
    Color, ElementTheme, SettingsLoadWarning, Theme, ThemeColorType, ThemeSettings,
};

#[test]
fn microsoft_theme_parse_simple_theme() {
    let theme = Theme::from_json(
        r##"{
            "name": "orange",
            "tabRow": {
                "background": "#FFFF8800",
                "unfocusedBackground": "#FF8844",
                "iconStyle": "default"
            },
            "window": {
                "applicationTheme": "light",
                "useMica": true
            }
        }"##,
    )
    .expect("Microsoft simple theme should parse");

    assert_eq!(theme.name(), "orange");
    let tab_row = theme.tab_row().expect("tab row should exist");
    let background = tab_row.background().expect("background should exist");
    assert_eq!(background.color_type(), ThemeColorType::Color);
    assert_eq!(
        background.color(),
        Some(Color::rgba(0xff, 0xff, 0x88, 0x00))
    );
    assert_eq!(
        tab_row
            .unfocused_background()
            .expect("unfocused background should exist")
            .color(),
        Some(Color::rgb(0xff, 0x88, 0x44))
    );
    let window = theme.window().expect("window theme should exist");
    assert_eq!(window.requested_theme(), ElementTheme::Light);
    assert!(window.use_mica());
}

#[test]
fn microsoft_theme_parse_empty_theme() {
    let theme =
        Theme::from_json(r#"{ "name": "empty" }"#).expect("Microsoft empty theme should parse");

    assert_eq!(theme.name(), "empty");
    assert!(theme.tab_row().is_none());
    assert!(theme.window().is_none());
    assert_eq!(theme.requested_theme(), ElementTheme::Default);
}

#[test]
fn microsoft_theme_parse_no_window_theme() {
    let theme = Theme::from_json(
        r##"{
            "name": "noWindow",
            "tabRow": {
                "background": "#112233",
                "unfocusedBackground": "#FF884400"
            },
        }"##,
    )
    .expect("Microsoft theme with no window should parse");

    assert_eq!(theme.name(), "noWindow");
    let tab_row = theme.tab_row().expect("tab row should exist");
    assert_eq!(
        tab_row
            .background()
            .expect("background should exist")
            .color(),
        Some(Color::rgb(0x11, 0x22, 0x33))
    );
    assert!(theme.window().is_none());
    assert_eq!(theme.requested_theme(), ElementTheme::Default);
}

#[test]
fn microsoft_theme_parse_null_window_theme() {
    let theme = Theme::from_json(
        r##"{
            "name": "nullWindow",
            "tabRow": {
                "background": "#112233",
                "unfocusedBackground": "#FF884400"
            },
            "window": null
        }"##,
    )
    .expect("Microsoft theme with null window should parse");

    assert_eq!(theme.name(), "nullWindow");
    assert!(theme.tab_row().is_some());
    assert!(theme.window().is_none());
    assert_eq!(theme.requested_theme(), ElementTheme::Default);
}

#[test]
fn microsoft_theme_parse_theme_with_null_theme_color() {
    let settings = ThemeSettings::from_user_settings_json(
        r#"{
            "themes": [
                {
                    "name": "backgroundEmpty",
                    "tabRow": {},
                    "window": { "applicationTheme": "light", "useMica": true }
                },
                {
                    "name": "backgroundNull",
                    "tabRow": { "background": null },
                    "window": { "applicationTheme": "light", "useMica": true }
                },
                {
                    "name": "backgroundOmittedEntirely",
                    "window": { "applicationTheme": "light", "useMica": true }
                }
            ]
        }"#,
    )
    .expect("Microsoft null theme-color settings should parse");

    let empty = settings
        .theme("backgroundEmpty")
        .expect("backgroundEmpty should exist");
    assert!(empty.tab_row().is_some());
    assert!(
        empty
            .tab_row()
            .expect("tab row should exist")
            .background()
            .is_none()
    );

    let null = settings
        .theme("backgroundNull")
        .expect("backgroundNull should exist");
    assert!(null.tab_row().is_some());
    assert!(
        null.tab_row()
            .expect("tab row should exist")
            .background()
            .is_none()
    );

    let omitted = settings
        .theme("backgroundOmittedEntirely")
        .expect("backgroundOmittedEntirely should exist");
    assert!(omitted.tab_row().is_none());
}

#[test]
fn microsoft_theme_invalid_current_theme() {
    let settings = ThemeSettings::from_user_settings_json(
        r#"{
            "theme": "foo",
            "themes": [
                {
                    "name": "bar",
                    "tabRow": {},
                    "window": { "applicationTheme": "light", "useMica": true }
                }
            ]
        }"#,
    )
    .expect("Microsoft invalid current-theme settings should parse");

    assert_eq!(settings.warnings(), &[SettingsLoadWarning::UnknownTheme]);
    let bar = settings.theme("bar").expect("bar theme should exist");
    assert!(bar.tab_row().is_some());
    assert!(
        bar.tab_row()
            .expect("tab row should exist")
            .background()
            .is_none()
    );
    assert_eq!(settings.current_theme().name(), "system");
}
