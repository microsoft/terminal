use terminal_settings::{cascadia_settings::CascadiaSettingsDocument, settings_json::JsonValue};

#[test]
fn microsoft_serialization_dont_roundtrip_no_reload_env_vars_contract() {
    let settings = r#"
    {
        "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
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

    let document = CascadiaSettingsDocument::from_json(settings)
        .expect("safe Rust CascadiaSettings owner accepts the Microsoft vector");
    let root = document
        .to_json_value()
        .as_object()
        .expect("Cascadia settings remains an object");

    assert!(
        root.get("compatibility.reloadEnvironmentVariables")
            .is_none(),
        "round-trip must not synthesize the legacy root setting"
    );

    let profiles = root
        .get("profiles")
        .and_then(JsonValue::as_object)
        .expect("legacy profiles array is serialized through the modern object shape");
    assert!(
        profiles.get("defaults").is_none(),
        "round-trip must not synthesize profile defaults when reloadEnvironmentVariables was absent"
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
        .expect("unrelated actions remain serialized");
    assert_eq!(actions.len(), 1);
}
