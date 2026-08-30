use terminal_settings::color_scheme::{Color, ColorSchemeSettings, OriginTag};

fn scheme(name: &str, foreground: &str, background: &str, table: &str) -> String {
    format!(
        r##"{{
            "name":"{name}",
            "foreground":"{foreground}",
            "background":"{background}",
            "selectionBackground":"{foreground}",
            "cursorColor":"#FFFFFF",
            "black":"{table}","red":"{table}","green":"{table}","yellow":"{table}",
            "blue":"{table}","purple":"{table}","cyan":"{table}","white":"{table}",
            "brightBlack":"{table}","brightRed":"{table}","brightGreen":"{table}","brightYellow":"{table}",
            "brightBlue":"{table}","brightPurple":"{table}","brightCyan":"{table}","brightWhite":"{table}"
        }}"##
    )
}

fn settings(profiles: &str, schemes: &[String]) -> String {
    format!(
        r#"{{ "profiles": {profiles}, "schemes": [{}] }}"#,
        schemes.join(",")
    )
}

fn rgb(hex: &str) -> Color {
    let digits = hex.trim_start_matches('#');
    Color::rgb(
        u8::from_str_radix(&digits[0..2], 16).unwrap(),
        u8::from_str_radix(&digits[2..4], 16).unwrap(),
        u8::from_str_radix(&digits[4..6], 16).unwrap(),
    )
}

#[test]
fn microsoft_color_scheme_update_scheme_references() {
    let user = r#"{
        "profiles": {
            "defaults": { "colorScheme": "Campbell" },
            "list": [
                { "name":"explicit", "colorScheme":"Campbell" },
                { "name":"hidden", "colorScheme":"Campbell", "hidden":true },
                { "name":"inherited" },
                { "name":"different", "colorScheme":"One Half Dark" },
                { "name":"neither", "colorScheme": { "dark":"One Half Dark", "light":"One Half Light" } },
                { "name":"light", "colorScheme": { "dark":"One Half Dark", "light":"Campbell" } },
                { "name":"dark", "colorScheme": { "dark":"Campbell", "light":"One Half Light" } }
            ]
        }
    }"#;
    let mut settings = ColorSchemeSettings::from_layers(user, "{}", &[]).unwrap();
    settings.update_scheme_references("Campbell", "Campbell (renamed)");

    let defaults = settings.profile_defaults();
    assert_eq!(defaults.dark_name(), "Campbell (renamed)");
    assert_eq!(defaults.light_name(), "Campbell (renamed)");
    assert!(defaults.has_dark_name());
    assert!(defaults.has_light_name());

    let profiles = settings.profiles();
    for profile in &profiles[0..2] {
        assert_eq!(
            profile.default_appearance().dark_name(),
            "Campbell (renamed)"
        );
        assert_eq!(
            profile.default_appearance().light_name(),
            "Campbell (renamed)"
        );
        assert!(profile.default_appearance().has_dark_name());
        assert!(profile.default_appearance().has_light_name());
    }
    assert_eq!(
        profiles[2].default_appearance().dark_name(),
        "Campbell (renamed)"
    );
    assert_eq!(
        profiles[2].default_appearance().light_name(),
        "Campbell (renamed)"
    );
    assert!(!profiles[2].default_appearance().has_dark_name());
    assert!(!profiles[2].default_appearance().has_light_name());
    assert_eq!(
        profiles[3].default_appearance().dark_name(),
        "One Half Dark"
    );
    assert_eq!(
        profiles[3].default_appearance().light_name(),
        "One Half Dark"
    );
    assert_eq!(
        profiles[4].default_appearance().dark_name(),
        "One Half Dark"
    );
    assert_eq!(
        profiles[4].default_appearance().light_name(),
        "One Half Light"
    );
    assert_eq!(
        profiles[5].default_appearance().dark_name(),
        "One Half Dark"
    );
    assert_eq!(
        profiles[5].default_appearance().light_name(),
        "Campbell (renamed)"
    );
    assert_eq!(
        profiles[6].default_appearance().dark_name(),
        "Campbell (renamed)"
    );
    assert_eq!(
        profiles[6].default_appearance().light_name(),
        "One Half Light"
    );
}

#[test]
fn microsoft_color_scheme_layer_user_owned_collision() {
    let inbox = settings(
        "[]",
        &[
            scheme("Campbell", "#CCCCCC", "#0C0C0C", "#0C0C0C"),
            scheme("Antique", "#C0C0C0", "#000000", "#808080"),
        ],
    );
    let user = settings(
        r#"[{"name":"profile0"}]"#,
        &[
            scheme("Campbell", "#121314", "#121314", "#121314"),
            scheme("Antique", "#C0C0C0", "#000000", "#808080"),
        ],
    );

    let settings = ColorSchemeSettings::from_layers(&user, &inbox, &[]).unwrap();
    assert_eq!(settings.scheme_count(), 3);

    let modified = settings.scheme("Campbell (modified)").unwrap();
    assert_eq!(modified.scheme().foreground(), rgb("#121314"));
    assert_eq!(modified.scheme().background(), rgb("#121314"));
    assert_eq!(modified.origin(), OriginTag::User);

    let stock = settings.scheme("Campbell").unwrap();
    assert_eq!(stock.scheme().foreground(), rgb("#CCCCCC"));
    assert_eq!(stock.scheme().background(), rgb("#0C0C0C"));
    assert_eq!(stock.origin(), OriginTag::InBox);

    let antique = settings.scheme("Antique").unwrap();
    assert_eq!(antique.scheme().foreground(), rgb("#C0C0C0"));
    assert_eq!(antique.origin(), OriginTag::InBox);
}

#[test]
fn microsoft_color_scheme_collision_retargets_all_profiles() {
    let inbox = settings(
        "[]",
        &[
            scheme("Campbell", "#CCCCCC", "#0C0C0C", "#0C0C0C"),
            scheme("Antique", "#C0C0C0", "#000000", "#808080"),
        ],
    );
    let profiles = r#"{
        "defaults": {},
        "list": [
            {"name":"profile0"},
            {"name":"profile1","colorScheme":"Antique"},
            {"name":"profile2","colorScheme":"Campbell"},
            {"name":"profile3","unfocusedAppearance":{"colorScheme":"Campbell"}},
            {"name":"profile4","unfocusedAppearance":{"colorScheme":{"dark":"Campbell"}}}
        ]
    }"#;
    let user = settings(
        profiles,
        &[scheme("Campbell", "#121314", "#121314", "#121314")],
    );
    let settings = ColorSchemeSettings::from_layers(&user, &inbox, &[]).unwrap();

    let defaults = settings.profile_defaults();
    assert!(defaults.has_light_name());
    assert!(defaults.has_dark_name());
    assert_eq!(defaults.light_name(), "Campbell (modified)");
    assert_eq!(defaults.dark_name(), "Campbell (modified)");

    let profiles = settings.profiles();
    assert!(!profiles[0].default_appearance().has_light_name());
    assert!(!profiles[0].default_appearance().has_dark_name());
    assert_eq!(
        profiles[0].default_appearance().light_name(),
        "Campbell (modified)"
    );
    assert_eq!(profiles[1].default_appearance().light_name(), "Antique");
    assert_eq!(
        profiles[2].default_appearance().light_name(),
        "Campbell (modified)"
    );
    assert!(profiles[2].default_appearance().has_light_name());

    assert!(!profiles[3].default_appearance().has_light_name());
    assert!(profiles[3].unfocused_appearance().has_light_name());
    assert_eq!(
        profiles[3].unfocused_appearance().light_name(),
        "Campbell (modified)"
    );
    assert!(profiles[3].unfocused_appearance().has_dark_name());
    assert_eq!(
        profiles[3].unfocused_appearance().dark_name(),
        "Campbell (modified)"
    );

    assert!(!profiles[4].unfocused_appearance().has_light_name());
    assert_eq!(
        profiles[4].unfocused_appearance().light_name(),
        "Campbell (modified)"
    );
    assert!(profiles[4].unfocused_appearance().has_dark_name());
    assert_eq!(
        profiles[4].unfocused_appearance().dark_name(),
        "Campbell (modified)"
    );
}

#[test]
fn microsoft_color_scheme_collision_with_fragments() {
    let inbox = settings(
        "[]",
        &[
            scheme("Campbell", "#CCCCCC", "#0C0C0C", "#0C0C0C"),
            scheme("Antique", "#C0C0C0", "#000000", "#808080"),
        ],
    );
    let fragment = settings(
        r#"[{"name":"fragment profile 0","colorScheme":{"light":"Mango Light","dark":"Mango Dark"}}]"#,
        &[
            scheme("Campbell", "#444444", "#444444", "#444444"),
            scheme("Mango Dark", "#D3D7CF", "#000000", "#555753"),
            scheme("Mango Light", "#555753", "#FFFFFF", "#555753"),
        ],
    );
    let user = settings(
        r#"{"defaults":{},"list":[
            {"name":"profile0"},
            {"name":"profile1","colorScheme":"Antique"},
            {"name":"profile2","colorScheme":"Mango Light"}
        ]}"#,
        &[scheme("Mango Light", "#121314", "#121314", "#121314")],
    );

    let settings = ColorSchemeSettings::from_layers(&user, &inbox, &[&fragment]).unwrap();
    assert_eq!(
        settings.scheme("Campbell").unwrap().origin(),
        OriginTag::Fragment
    );
    assert_eq!(
        settings.scheme("Campbell").unwrap().scheme().foreground(),
        rgb("#444444")
    );
    assert_eq!(
        settings.scheme("Antique").unwrap().origin(),
        OriginTag::InBox
    );
    assert_eq!(
        settings.scheme("Mango Light").unwrap().origin(),
        OriginTag::Fragment
    );
    assert_eq!(
        settings.scheme("Mango Light (modified)").unwrap().origin(),
        OriginTag::User
    );
    assert_eq!(
        settings
            .scheme("Mango Light (modified)")
            .unwrap()
            .scheme()
            .foreground(),
        rgb("#121314")
    );

    let defaults = settings.profile_defaults();
    assert!(!defaults.has_light_name());
    assert!(!defaults.has_dark_name());
    assert_eq!(defaults.light_name(), "Campbell");
    assert_eq!(defaults.dark_name(), "Campbell");

    let profiles = settings.profiles();
    assert_eq!(profiles[0].default_appearance().light_name(), "Campbell");
    assert_eq!(profiles[1].default_appearance().light_name(), "Antique");
    assert_eq!(
        profiles[2].default_appearance().light_name(),
        "Mango Light (modified)"
    );
    assert!(profiles[2].default_appearance().has_light_name());
    assert!(profiles[2].default_appearance().has_dark_name());

    let fragment_profile = &profiles[3];
    assert!(fragment_profile.default_appearance().has_light_name());
    assert_eq!(
        fragment_profile.default_appearance().light_name(),
        "Mango Light (modified)"
    );
    assert!(!fragment_profile.default_appearance().has_dark_name());
    assert_eq!(
        fragment_profile.default_appearance().dark_name(),
        "Mango Dark"
    );
}

#[test]
fn microsoft_color_scheme_layer_multiple_user_collisions() {
    let inbox = settings("[]", &[scheme("Campbell", "#111111", "#111111", "#111111")]);
    let user = settings(
        r#"[{"name":"profile0"}]"#,
        &[
            scheme("Campbell", "#222222", "#222222", "#222222"),
            scheme("Campbell (modified)", "#333333", "#333333", "#333333"),
        ],
    );

    let settings = ColorSchemeSettings::from_layers(&user, &inbox, &[]).unwrap();
    assert_eq!(settings.scheme_count(), 3);
    let modified2 = settings.scheme("Campbell (modified 2)").unwrap();
    assert_eq!(modified2.scheme().foreground(), rgb("#222222"));
    assert_eq!(modified2.origin(), OriginTag::User);
    let modified = settings.scheme("Campbell (modified)").unwrap();
    assert_eq!(modified.scheme().foreground(), rgb("#333333"));
    assert_eq!(modified.origin(), OriginTag::User);
    let stock = settings.scheme("Campbell").unwrap();
    assert_eq!(stock.scheme().foreground(), rgb("#111111"));
    assert_eq!(stock.origin(), OriginTag::InBox);
}
