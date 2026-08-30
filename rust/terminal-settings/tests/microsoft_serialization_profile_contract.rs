use terminal_settings::{serialization::SettingsDocument, settings_json::JsonValue};

fn assert_profile_roundtrip(input: &str) {
    let expected =
        terminal_settings::settings_json::parse(input).expect("Microsoft Profile JSON parses");
    let document = SettingsDocument::from_profile_json(input)
        .expect("safe Rust Profile owner accepts the Microsoft vector");
    assert_eq!(document.to_json_value(), &expected);
}

#[test]
fn microsoft_serialization_profile_contract() {
    // Microsoft: SerializationTests::Profile.
    let profile = r##"
    {
        "name": "Windows PowerShell",
        "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
        "commandline": "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
        "startingDirectory": "%USERPROFILE%",
        "icon": "ms-appx:///ProfileIcons/{61c54bbd-c2c6-5271-96e7-009a87ff44bf}.png",
        "hidden": false,
        "tabTitle": "Cool Tab",
        "suppressApplicationTitle": false,
        "font": {
            "face": "Cascadia Mono",
            "size": 12,
            "weight": "normal"
        },
        "padding": "8, 8, 8, 8",
        "antialiasingMode": "grayscale",
        "cursorShape": "bar",
        "cursorColor": "#CCBBAA",
        "cursorHeight": 10,
        "altGrAliasing": true,
        "colorScheme": "Campbell",
        "tabColor": "#0C0C0C",
        "foreground": "#AABBCC",
        "background": "#BBCCAA",
        "selectionBackground": "#CCAABB",
        "useAcrylic": false,
        "opacity": 50,
        "backgroundImage": "made_you_look.jpeg",
        "backgroundImageStretchMode": "uniformToFill",
        "backgroundImageAlignment": "center",
        "backgroundImageOpacity": 1,
        "scrollbarState": "visible",
        "snapOnInput": true,
        "historySize": 9001,
        "closeOnExit": "graceful",
        "experimental.retroTerminalEffect": false,
        "environment": {
            "KEY_1": "VALUE_1",
            "KEY_2": "%KEY_1%",
            "KEY_3": "%PATH%"
        }
    }
    "##;
    assert_profile_roundtrip(profile);

    let document = SettingsDocument::from_profile_json(profile).expect("full profile parse");
    let root = document
        .to_json_value()
        .as_object()
        .expect("Profile remains an object");
    assert_eq!(
        root.get("historySize").and_then(JsonValue::as_f64),
        Some(9001.0)
    );
    assert_eq!(
        root.get("snapOnInput").and_then(JsonValue::as_bool),
        Some(true)
    );
    assert_eq!(
        root.get("foreground").and_then(JsonValue::as_str),
        Some("#AABBCC")
    );
    assert!(
        matches!(root.get("environment"), Some(JsonValue::Object(values)) if values.len() == 3)
    );

    assert_profile_roundtrip(r#"{ "name": "Custom Profile" }"#);

    assert_profile_roundtrip(
        r#"{
            "guid": "{8b039d4d-77ca-5a83-88e1-dfc8e895a127}",
            "name": "Weird Profile",
            "hidden": false,
            "tabColor": null,
            "foreground": null,
            "source": "local"
        }"#,
    );

    assert_profile_roundtrip(
        r#"{
            "guid": "{8b039d4d-77ca-5a83-88e1-dfc8e895a127}",
            "name": "profileWithIcon",
            "hidden": false,
            "icon": "foo.png"
        }"#,
    );

    assert_profile_roundtrip(
        r#"{
            "guid": "{8b039d4d-77ca-5a83-88e1-dfc8e895a127}",
            "name": "profileWithNullIcon",
            "hidden": false,
            "icon": null
        }"#,
    );

    assert_profile_roundtrip(
        r#"{
            "guid": "{8b039d4d-77ca-5a83-88e1-dfc8e895a127}",
            "name": "profileWithNoIcon",
            "hidden": false,
            "icon": "none"
        }"#,
    );
}

#[test]
fn microsoft_serialization_legacy_font_settings_contract() {
    // Microsoft: SerializationTests::LegacyFontSettings.
    let legacy = r#"
    {
        "name": "Profile with legacy font settings",
        "fontFace": "Cascadia Mono",
        "fontSize": 12,
        "fontWeight": "normal"
    }
    "#;
    let expected = terminal_settings::settings_json::parse(
        r#"
        {
            "name": "Profile with legacy font settings",
            "font": {
                "face": "Cascadia Mono",
                "size": 12,
                "weight": "normal"
            }
        }
        "#,
    )
    .expect("Microsoft canonical font JSON parses");

    let document = SettingsDocument::from_profile_json(legacy)
        .expect("safe Rust canonicalizes legacy profile font aliases");
    assert_eq!(document.to_json_value(), &expected);

    let root = document
        .to_json_value()
        .as_object()
        .expect("canonical profile remains an object");
    assert!(!root.contains_key("fontFace"));
    assert!(!root.contains_key("fontSize"));
    assert!(!root.contains_key("fontWeight"));
    let font = root
        .get("font")
        .and_then(JsonValue::as_object)
        .expect("modern font object is emitted");
    assert_eq!(
        font.get("face").and_then(JsonValue::as_str),
        Some("Cascadia Mono")
    );
    assert_eq!(font.get("size").and_then(JsonValue::as_f64), Some(12.0));
    assert_eq!(
        font.get("weight").and_then(JsonValue::as_str),
        Some("normal")
    );
}
