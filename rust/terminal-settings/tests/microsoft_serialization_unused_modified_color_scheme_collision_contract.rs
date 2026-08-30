#![allow(clippy::too_many_lines)]

use terminal_settings::{serialization::SettingsDocument, settings_json::JsonValue};

#[test]
fn microsoft_serialization_roundtrip_user_modified_color_scheme_collision_unused_by_profiles_contract()
 {
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
                "background": "#111111",
                "black": "#111111",
                "blue": "#111111",
                "brightBlack": "#111111",
                "brightBlue": "#111111",
                "brightCyan": "#111111",
                "brightGreen": "#111111",
                "brightPurple": "#111111",
                "brightRed": "#111111",
                "brightWhite": "#111111",
                "brightYellow": "#111111",
                "cursorColor": "#111111",
                "cyan": "#111111",
                "foreground": "#111111",
                "green": "#111111",
                "name": "Tango Dark",
                "purple": "#111111",
                "red": "#111111",
                "selectionBackground": "#111111",
                "white": "#111111",
                "yellow": "#111111"
            }
        ]
    }
    "##;

    let new_settings = r##"
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
        "schemes": [
            {
                "background": "#111111",
                "black": "#111111",
                "blue": "#111111",
                "brightBlack": "#111111",
                "brightBlue": "#111111",
                "brightCyan": "#111111",
                "brightGreen": "#111111",
                "brightPurple": "#111111",
                "brightRed": "#111111",
                "brightWhite": "#111111",
                "brightYellow": "#111111",
                "cursorColor": "#111111",
                "cyan": "#111111",
                "foreground": "#111111",
                "green": "#111111",
                "name": "Tango Dark (modified)",
                "purple": "#111111",
                "red": "#111111",
                "selectionBackground": "#111111",
                "white": "#111111",
                "yellow": "#111111"
            }
        ]
    }
    "##;

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
        .expect("safe Rust SettingsLoader slice renames the unused modified collision");
    let expected = SettingsDocument::from_json(new_settings)
        .expect("Microsoft expected post-fixup vector is valid settings JSON");

    assert_eq!(fixed.to_json_value(), expected.to_json_value());

    let root = fixed
        .to_json_value()
        .as_object()
        .expect("fixed settings remain an object");
    let profiles = root
        .get("profiles")
        .and_then(JsonValue::as_object)
        .expect("profiles use the modern object shape");
    assert!(profiles.get("defaults").is_none());

    let schemes = root
        .get("schemes")
        .and_then(JsonValue::as_array)
        .expect("schemes remain an array");
    assert_eq!(schemes.len(), 1);
    let scheme = schemes[0]
        .as_object()
        .expect("the modified scheme remains an object");
    assert_eq!(
        scheme.get("name").and_then(JsonValue::as_str),
        Some("Tango Dark (modified)")
    );
    assert_eq!(
        scheme.get("foreground").and_then(JsonValue::as_str),
        Some("#111111")
    );

    let actions = root
        .get("actions")
        .and_then(JsonValue::as_array)
        .expect("Cascadia serialization materializes actions");
    assert!(actions.is_empty());
}
