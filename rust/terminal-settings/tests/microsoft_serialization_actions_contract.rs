#![allow(clippy::too_many_lines)]

use terminal_settings::{action_map::ActionMapDocument, settings_json};

#[test]
fn microsoft_serialization_actions_contract() {
    let vectors = [
        r#"[
            { "command": "paste", "id": "Test.Paste" }
        ]"#,
        r#"[
            { "command": { "action": "setTabColor" }, "id": "Test.SetTabColor" }
        ]"#,
        r##"[
            { "command": { "action": "setTabColor", "color": "#112233" }, "id": "Test.SetTabColor112233" }
        ]"##,
        r#"[
            { "command": { "action": "copy" }, "id": "Test.Copy" },
            { "command": { "action": "copy", "singleLine": true, "copyFormatting": "html" }, "id": "Test.CopyWithArgs" }
        ]"#,
        r#"{
            "actions": [
                { "command": "toggleAlwaysOnTop", "id": "Test.ToggleAlwaysOnTop" }
            ],
            "keybindings": [
                { "keys": "ctrl+a", "id": "Test.ToggleAlwaysOnTop" },
                { "keys": "ctrl+b", "id": "Test.ToggleAlwaysOnTop" }
            ]
        }"#,
        r#"{
            "actions": [
                { "command": { "action": "adjustFontSize", "delta": 1 }, "id": "Test.EnlargeFont" }
            ],
            "keybindings": [
                { "keys": "ctrl+c", "id": "Test.EnlargeFont" },
                { "keys": "ctrl+d", "id": "Test.EnlargeFont" }
            ]
        }"#,
        r#"{
            "actions": [
                { "icon": "image.png", "name": "Scroll To Top Name", "command": "scrollToTop", "id": "Test.ScrollToTop" }
            ],
            "keybindings": [
                { "id": "Test.ScrollToTop", "keys": "ctrl+f" },
                { "id": "Test.ScrollToTop", "keys": "ctrl+e" }
            ]
        }"#,
        r#"[
            { "command": { "action": "newTab", "index": 0 }, "id": "Test.NewTerminal" },
        ]"#,
        r#"[
            { "command": { "action": "renameWindow", "name": null }, "id": "Test.MeaningfulNull" }
        ]"#,
        r#"[
            {
                "name": "Change font size...",
                "commands": [
                    { "command": { "action": "adjustFontSize", "delta": 1 } },
                    { "command": { "action": "adjustFontSize", "delta": -1 } },
                    { "command": "resetFontSize" },
                ]
            }
        ]"#,
        r#"[
            {
                "name": "New tab",
                "commands": [
                    {
                        "iterateOn": "profiles",
                        "icon": "${profile.icon}",
                        "name": "${profile.name}",
                        "command": { "action": "newTab", "profile": "${profile.name}" }
                    }
                ]
            }
        ]"#,
        r#"[
            {
                "commands": [
                    {
                        "command": {
                            "action": "sendInput",
                            "input": "${profile.name}"
                        },
                        "iterateOn": "profiles"
                    }
                ],
                "name": "Send Input ..."
            }
        ]"#,
        r#"[
            {
                "commands": [
                    {
                        "commands": [
                            {
                                "command": {
                                    "action": "sendInput",
                                    "input": "${profile.name} ${scheme.name}"
                                },
                                "iterateOn": "schemes"
                            }
                        ],
                        "iterateOn": "profiles",
                        "name": "nest level (${profile.name})"
                    }
                ],
                "name": "Send Input (Evil) ..."
            }
        ]"#,
        r#"[
            {
                "command": {
                    "action": "newTab",
                    "profile": "${profile.name}"
                },
                "icon": "${profile.icon}",
                "iterateOn": "profiles",
                "name": "${profile.name}: New tab"
            }
        ]"#,
        r#"{
            "actions": [],
            "keybindings": [
                { "id": null, "keys": "ctrl+c" }
            ]
        }"#,
    ];

    for (index, vector) in vectors.iter().enumerate() {
        let expected = settings_json::parse(vector)
            .unwrap_or_else(|error| panic!("Microsoft vector {index} must parse: {error:?}"));
        let document = ActionMapDocument::from_json(vector)
            .unwrap_or_else(|error| panic!("Rust ActionMap must accept vector {index}: {error:?}"));
        assert_eq!(
            document.to_json_value(),
            &expected,
            "Microsoft action serialization vector {index} must round-trip structure-identically"
        );
    }
}
