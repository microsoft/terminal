#![allow(clippy::too_many_lines)]

use terminal_settings::{serialization::SettingsDocument, settings_json::JsonValue};

#[test]
fn microsoft_serialization_roundtrip_user_deleted_color_scheme_collision_contract() {
    let old_settings = r##"
    {
        "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
        "profiles": [
            {
                "name": "profile0",
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
            }
        ],
        "schemes": [
            {
                "name": "Tango Dark",
                "foreground": "#D3D7CF",
                "background": "#000000",
                "cursorColor": "#FFFFFF",
                "black": "#000000",
                "red": "#CC0000",
                "green": "#4E9A06",
                "yellow": "#C4A000",
                "blue": "#3465A4",
                "purple": "#75507B",
                "cyan": "#06989A",
                "white": "#D3D7CF",
                "brightBlack": "#555753",
                "brightRed": "#EF2929",
                "brightGreen": "#8AE234",
                "brightYellow": "#FCE94F",
                "brightBlue": "#729FCF",
                "brightPurple": "#AD7FA8",
                "brightCyan": "#34E2E2",
                "brightWhite": "#EEEEEC"
            }
        ]
    }
    "##;

    let new_settings = r#"
    {
        "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
        "profiles": {
            "list": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                }
            ]
        },
        "actions": [],
        "schemes": []
    }
    "#;

    // This is the inbox Tango Dark definition used by the collision policy.
    // The user vector omits selectionBackground, but Microsoft still treats it
    // as merge-equivalent and removes the user copy during FixupUserSettings.
    let inbox = r##"
    {
        "schemes": [
            {
                "background": "#000000",
                "black": "#000000",
                "blue": "#3465A4",
                "brightBlack": "#555753",
                "brightBlue": "#729FCF",
                "brightCyan": "#34E2E2",
                "brightGreen": "#8AE234",
                "brightPurple": "#AD7FA8",
                "brightRed": "#EF2929",
                "brightWhite": "#EEEEEC",
                "brightYellow": "#FCE94F",
                "cursorColor": "#FFFFFF",
                "cyan": "#06989A",
                "foreground": "#D3D7CF",
                "green": "#4E9A06",
                "name": "Tango Dark",
                "purple": "#75507B",
                "red": "#CC0000",
                "selectionBackground": "#FFFFFF",
                "white": "#D3D7CF",
                "yellow": "#C4A000"
            }
        ]
    }
    "##;

    let fixed = SettingsDocument::from_json_with_color_scheme_fixup(old_settings, inbox)
        .expect("safe Rust SettingsLoader slice removes the redundant Microsoft collision");
    let expected = SettingsDocument::from_json(new_settings)
        .expect("Microsoft expected post-fixup vector is valid settings JSON");

    assert_eq!(fixed.to_json_value(), expected.to_json_value());

    let root = fixed
        .to_json_value()
        .as_object()
        .expect("fixed settings remain an object");
    let schemes = root
        .get("schemes")
        .and_then(JsonValue::as_array)
        .expect("schemes remain an array");
    assert!(schemes.is_empty());

    let profiles = root
        .get("profiles")
        .and_then(JsonValue::as_object)
        .expect("profiles use the modern object shape");
    assert!(profiles.get("defaults").is_none());
    assert_eq!(
        profiles
            .get("list")
            .and_then(JsonValue::as_array)
            .map(<[terminal_settings::settings_json::JsonValue]>::len),
        Some(1)
    );

    let actions = root
        .get("actions")
        .and_then(JsonValue::as_array)
        .expect("Cascadia serialization materializes actions");
    assert!(actions.is_empty());
}
