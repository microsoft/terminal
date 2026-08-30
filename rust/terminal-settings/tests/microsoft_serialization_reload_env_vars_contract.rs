use terminal_settings::{cascadia_settings::CascadiaSettingsDocument, settings_json::JsonValue};

#[test]
fn microsoft_serialization_roundtrip_reload_env_vars_contract() {
    let old_settings = r#"
    {
        "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
        "compatibility.reloadEnvironmentVariables": false,
        "profiles": [
            {
                "name": "profile0",
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                "historySize": 1,
                "commandline": "cmd.exe"
            }
        ],
        "actions": [
            {
                "name": "foo",
                "command": "closePane",
                "keys": "ctrl+shift+w"
            }
        ]
    }
    "#;

    let new_settings = r#"
    {
        "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
        "profiles": {
            "defaults": {
                "compatibility.reloadEnvironmentVariables": false
            },
            "list": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                    "historySize": 1,
                    "commandline": "cmd.exe"
                }
            ]
        },
        "actions": [
            {
                "name": "foo",
                "command": "closePane",
                "keys": "ctrl+shift+w"
            }
        ]
    }
    "#;

    let old_document = CascadiaSettingsDocument::from_json(old_settings)
        .expect("safe Rust CascadiaSettings owner accepts the Microsoft legacy vector");
    let new_document = CascadiaSettingsDocument::from_json(new_settings)
        .expect("safe Rust CascadiaSettings owner accepts the Microsoft modern vector");

    assert_eq!(old_document.to_json_value(), new_document.to_json_value());

    let root = old_document
        .to_json_value()
        .as_object()
        .expect("Cascadia settings remains an object");
    assert!(
        root.get("compatibility.reloadEnvironmentVariables")
            .is_none()
    );

    let profiles = root
        .get("profiles")
        .and_then(JsonValue::as_object)
        .expect("legacy profile array is serialized through the modern object shape");
    let defaults = profiles
        .get("defaults")
        .and_then(JsonValue::as_object)
        .expect("reload environment setting is migrated into profile defaults");
    assert_eq!(
        defaults
            .get("compatibility.reloadEnvironmentVariables")
            .and_then(JsonValue::as_bool),
        Some(false)
    );

    let list = profiles
        .get("list")
        .and_then(JsonValue::as_array)
        .expect("modern profiles object contains list");
    let profile = list[0].as_object().expect("profile remains an object");
    assert_eq!(
        profile.get("commandline").and_then(JsonValue::as_str),
        Some("cmd.exe")
    );
    assert_eq!(
        profile.get("historySize").and_then(JsonValue::as_f64),
        Some(1.0)
    );

    let actions = root
        .get("actions")
        .and_then(JsonValue::as_array)
        .expect("unrelated action survives migration");
    assert_eq!(actions.len(), 1);
}
