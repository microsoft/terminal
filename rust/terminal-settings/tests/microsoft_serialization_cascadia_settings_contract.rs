use terminal_settings::{
    cascadia_settings::CascadiaSettingsDocument,
    settings_json::{self, JsonValue},
};

#[test]
fn microsoft_serialization_cascadia_settings_contract() {
    // Microsoft: SerializationTests::CascadiaSettings exercises a complete
    // settings-model document rather than one leaf owner. Keep representative
    // globals, profiles, schemes, actions and keybindings in the same typed tree.
    let input = r##"
    {
        "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
        "initialRows": 30,
        "alwaysOnTop": false,
        "profiles": {
            "defaults": {},
            "list": [
                {
                    "name": "Windows PowerShell",
                    "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                    "commandline": "%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"
                }
            ]
        },
        "schemes": [
            {
                "name": "Campbell",
                "foreground": "#F2F2F2",
                "background": "#0C0C0C"
            }
        ],
        "actions": [
            { "command": "paste", "id": "Test.Paste" }
        ],
        "keybindings": [
            { "keys": "ctrl+v", "id": "Test.Paste" }
        ]
    }
    "##;

    let expected = settings_json::parse(input).expect("Microsoft CascadiaSettings vector parses");
    let settings = CascadiaSettingsDocument::from_json(input)
        .expect("safe Rust CascadiaSettings aggregate accepts the Microsoft-derived vector");
    assert_eq!(settings.to_json_value(), &expected);

    let root = settings
        .to_json_value()
        .as_object()
        .expect("CascadiaSettings remains an object");
    assert_eq!(
        root.get("initialRows").and_then(JsonValue::as_f64),
        Some(30.0)
    );
    assert_eq!(
        root.get("alwaysOnTop").and_then(JsonValue::as_bool),
        Some(false)
    );
    assert!(matches!(root.get("schemes"), Some(JsonValue::Array(values)) if values.len() == 1));
    assert!(matches!(root.get("actions"), Some(JsonValue::Array(values)) if values.len() == 1));
    assert!(matches!(root.get("keybindings"), Some(JsonValue::Array(values)) if values.len() == 1));
}

#[test]
fn cascadia_settings_rejects_invalid_aggregate_shapes() {
    assert!(CascadiaSettingsDocument::from_json(r#"{"profiles":42}"#).is_err());
    assert!(CascadiaSettingsDocument::from_json(r#"{"schemes":{}}"#).is_err());
    assert!(CascadiaSettingsDocument::from_json(r#"{"actions":{}}"#).is_err());
    assert!(CascadiaSettingsDocument::from_json(r#"{"keybindings":{}}"#).is_err());
}
