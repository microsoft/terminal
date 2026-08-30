use terminal_settings::{serialization::SettingsDocument, settings_json::JsonValue};

fn assert_roundtrip(input: &str) {
    let expected = terminal_settings::settings_json::parse(input)
        .expect("Microsoft GlobalSettings JSON parses");
    let settings = SettingsDocument::from_json(input)
        .expect("safe Rust settings document accepts the Microsoft vector");
    assert_eq!(settings.to_json_value(), &expected);
}

#[test]
fn microsoft_serialization_global_settings_contract() {
    // Microsoft: SerializationTests::GlobalSettings.
    let globals = r#"
    {
        "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
        "initialRows": 30,
        "initialCols": 120,
        "initialPosition": ",",
        "launchMode": "default",
        "alwaysOnTop": false,
        "copyOnSelect": false,
        "copyFormatting": "all",
        "wordDelimiters": " /\\()\"'-.,:;<>~!@#$%^&*|+=[]{}~?\u2502",
        "alwaysShowTabs": true,
        "showTabsInTitlebar": true,
        "showTerminalTitleInTitlebar": true,
        "tabWidthMode": "equal",
        "tabSwitcherMode": "mru",
        "theme": "system",
        "snapToGridOnResize": true,
        "disableAnimations": false,
        "trimPaste": true,
        "warning.confirmOnClose": "automatic",
        "warning.inputService": true,
        "warning.largePaste": true,
        "warning.multiLinePaste": "automatic",
        "actions": [],
        "keybindings": []
    }
    "#;

    assert_roundtrip(globals);

    let settings = SettingsDocument::from_json(globals).expect("full globals parse");
    let root = settings
        .to_json_value()
        .as_object()
        .expect("GlobalSettings remains an object");
    assert_eq!(
        root.get("initialRows").and_then(JsonValue::as_f64),
        Some(30.0)
    );
    assert_eq!(
        root.get("initialCols").and_then(JsonValue::as_f64),
        Some(120.0)
    );
    assert_eq!(
        root.get("alwaysOnTop").and_then(JsonValue::as_bool),
        Some(false)
    );
    assert_eq!(
        root.get("theme").and_then(JsonValue::as_str),
        Some("system")
    );
    assert_eq!(
        root.get("warning.confirmOnClose")
            .and_then(JsonValue::as_str),
        Some("automatic")
    );
    assert!(matches!(root.get("actions"), Some(JsonValue::Array(values)) if values.is_empty()));
    assert!(matches!(root.get("keybindings"), Some(JsonValue::Array(values)) if values.is_empty()));

    let small_globals = r#"
    {
        "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
        "actions": [],
        "keybindings": []
    }
    "#;
    assert_roundtrip(small_globals);
}
