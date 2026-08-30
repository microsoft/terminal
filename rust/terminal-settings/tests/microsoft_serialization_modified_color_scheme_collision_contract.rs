#![allow(clippy::too_many_lines)]

use terminal_settings::{serialization::SettingsDocument, settings_json::JsonValue};

#[test]
fn microsoft_serialization_roundtrip_user_modified_color_scheme_collision_contract() {
    let old_settings = r##"
    {
        "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
        "profiles": [
            {
                "name": "profile0",
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
            },
            {
                "name": "profile1",
                "colorScheme": "Tango Dark",
                "guid": "{d0a65a9d-8665-4128-97a4-a581aa747aa7}"
            }
        ],
        "schemes": [
            {
                "background": "#121314",
                "black": "#121314",
                "blue": "#121314",
                "brightBlack": "#121314",
                "brightBlue": "#121314",
                "brightCyan": "#121314",
                "brightGreen": "#121314",
                "brightPurple": "#121314",
                "brightRed": "#121314",
                "brightWhite": "#121314",
                "brightYellow": "#121314",
                "cursorColor": "#121314",
                "cyan": "#121314",
                "foreground": "#121314",
                "green": "#121314",
                "name": "Campbell",
                "purple": "#121314",
                "red": "#121314",
                "selectionBackground": "#121314",
                "white": "#121314",
                "yellow": "#121314"
            },
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

    let new_settings = r##"
    {
        "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
        "profiles": {
            "defaults": {
                "colorScheme": "Campbell (modified)"
            },
            "list": [
                {
                    "name": "profile0",
                    "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
                },
                {
                    "name": "profile1",
                    "colorScheme": "Tango Dark",
                    "guid": "{d0a65a9d-8665-4128-97a4-a581aa747aa7}"
                }
            ]
        },
        "actions": [],
        "schemes": [
            {
                "background": "#121314",
                "black": "#121314",
                "blue": "#121314",
                "brightBlack": "#121314",
                "brightBlue": "#121314",
                "brightCyan": "#121314",
                "brightGreen": "#121314",
                "brightPurple": "#121314",
                "brightRed": "#121314",
                "brightWhite": "#121314",
                "brightYellow": "#121314",
                "cursorColor": "#121314",
                "cyan": "#121314",
                "foreground": "#121314",
                "green": "#121314",
                "name": "Campbell",
                "purple": "#121314",
                "red": "#121314",
                "selectionBackground": "#121314",
                "white": "#121314",
                "yellow": "#121314"
            }
        ]
    }
    "##;

    // Minimal inbox slice needed by the Microsoft collision contract. Campbell
    // differs from the user's scheme; Tango Dark is byte-for-byte semantically
    // equivalent under SettingsModel's foreground/background/table merge key.
    let inbox = r##"
    {
        "schemes": [
            {
                "name": "Campbell",
                "cursorColor": "#FFFFFF",
                "selectionBackground": "#131313",
                "background": "#0C0C0C",
                "foreground": "#F2F2F2",
                "black": "#0C0C0C",
                "blue": "#0037DA",
                "cyan": "#3A96DD",
                "green": "#13A10E",
                "purple": "#881798",
                "red": "#C50F1F",
                "white": "#CCCCCC",
                "yellow": "#C19C00",
                "brightBlack": "#767676",
                "brightBlue": "#3B78FF",
                "brightCyan": "#61D6D6",
                "brightGreen": "#16C60C",
                "brightPurple": "#B4009E",
                "brightRed": "#E74856",
                "brightWhite": "#F2F2F2",
                "brightYellow": "#F9F1A5"
            },
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
        .expect("safe Rust SettingsLoader slice fixes the Microsoft collision vector");
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
        .expect("profiles are serialized through the modern object shape");
    let defaults = profiles
        .get("defaults")
        .and_then(JsonValue::as_object)
        .expect("collision materializes profile defaults");
    assert_eq!(
        defaults.get("colorScheme").and_then(JsonValue::as_str),
        Some("Campbell (modified)")
    );

    let schemes = root
        .get("schemes")
        .and_then(JsonValue::as_array)
        .expect("schemes remain an array");
    assert_eq!(schemes.len(), 1);
    assert_eq!(
        schemes[0]
            .as_object()
            .and_then(|scheme| scheme.get("name"))
            .and_then(JsonValue::as_str),
        Some("Campbell")
    );

    let actions = root
        .get("actions")
        .and_then(JsonValue::as_array)
        .expect("Cascadia serialization materializes actions");
    assert!(actions.is_empty());

    let list = profiles
        .get("list")
        .and_then(JsonValue::as_array)
        .expect("modern profiles contain list");
    assert_eq!(
        list[1]
            .as_object()
            .and_then(|profile| profile.get("colorScheme"))
            .and_then(JsonValue::as_str),
        Some("Tango Dark")
    );
}
