use terminal_settings::deserialization_copy::CloneableCascadiaSettings;

#[test]
fn microsoft_deserialization_copy_contract() {
    let json = r##"{
        "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
        "initialCols": 50,
        "profiles": [
            {
                "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                "name": "Custom Profile",
                "fontFace": "Cascadia Code"
            }
        ],
        "schemes": [
            {
                "name": "Campbell, but for a test",
                "foreground": "#CCCCCC",
                "background": "#0C0C0C",
                "cursorColor": "#FFFFFF",
                "black": "#0C0C0C",
                "red": "#C50F1F",
                "green": "#13A10E",
                "yellow": "#C19C00",
                "blue": "#0037DA",
                "purple": "#881798",
                "cyan": "#3A96DD",
                "white": "#CCCCCC",
                "brightBlack": "#767676",
                "brightRed": "#E74856",
                "brightGreen": "#16C60C",
                "brightYellow": "#F9F1A5",
                "brightBlue": "#3B78FF",
                "brightPurple": "#B4009E",
                "brightCyan": "#61D6D6",
                "brightWhite": "#F2F2F2"
            }
        ],
        "actions": [
            { "command": "openSettings", "keys": "ctrl+," },
            { "command": { "action": "openSettings", "target": "defaultsFile" }, "keys": "ctrl+alt+," },
            {
                "name": { "key": "SetColorSchemeParentCommandName" },
                "commands": [
                    {
                        "iterateOn": "schemes",
                        "name": "${scheme.name}",
                        "command": { "action": "setColorScheme", "colorScheme": "${scheme.name}" }
                    }
                ]
            }
        ]
    }"##;

    let settings = CloneableCascadiaSettings::from_json(json).unwrap();
    let mut copy = settings.deep_copy();

    assert_eq!(settings.default_profile(), copy.default_profile());
    assert_eq!(settings.profile_count(), copy.profile_count());
    assert_eq!(settings.profile_name(0), copy.profile_name(0));

    assert_eq!(settings.color_scheme_count(), 1);
    assert_eq!(settings.color_scheme_count(), copy.color_scheme_count());
    assert!(settings.has_color_scheme("Campbell, but for a test"));
    assert_eq!(
        settings.has_color_scheme("Campbell, but for a test"),
        copy.has_color_scheme("Campbell, but for a test")
    );

    assert_eq!(settings.keybinding_count(), 2);
    assert_eq!(settings.keybinding_count(), copy.keybinding_count());
    assert_eq!(settings.action_name_count(), copy.action_name_count());

    assert_eq!(settings.word_delimiters(), copy.word_delimiters());
    copy.set_word_delimiters("changed value");
    assert_ne!(settings.word_delimiters(), copy.word_delimiters());
}

#[test]
fn microsoft_deserialization_clone_inheritance_tree_contract() {
    let json = r#"{
        "defaultProfile": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
        "profiles": {
            "defaults": {
                "tabTitle": "PROFILE DEFAULTS TAB TITLE"
            },
            "list": [
                {
                    "guid": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
                    "name": "CMD",
                    "tabTitle": "CMD Tab Title"
                },
                {
                    "guid": "{61c54bbd-2222-5271-96e7-009a87ff44bf}",
                    "name": "PowerShell",
                    "tabTitle": "PowerShell Tab Title"
                },
                {
                    "guid": "{61c54bbd-3333-5271-96e7-009a87ff44bf}"
                }
            ]
        }
    }"#;

    let settings = CloneableCascadiaSettings::from_json(json).unwrap();
    let mut copy = settings.deep_copy();

    assert_eq!(settings.default_profile(), copy.default_profile());
    assert_eq!(settings.profile_count(), copy.profile_count());
    for index in 0..settings.profile_count() {
        assert_eq!(settings.profile_name(index), copy.profile_name(index));
        assert_eq!(
            settings.profile_tab_title(index),
            copy.profile_tab_title(index)
        );
    }
    assert_eq!(
        settings.profile_defaults_tab_title(),
        copy.profile_defaults_tab_title()
    );

    assert_eq!(
        settings.profile_defaults_has_tab_title(),
        copy.profile_defaults_has_tab_title()
    );
    copy.set_profile_defaults_tab_title("changed value");

    assert_eq!(settings.profile_name(0), copy.profile_name(0));
    assert_eq!(settings.profile_tab_title(0), copy.profile_tab_title(0));
    assert_eq!(settings.profile_name(1), copy.profile_name(1));
    assert_eq!(settings.profile_tab_title(1), copy.profile_tab_title(1));
    assert_ne!(settings.profile_tab_title(2), copy.profile_tab_title(2));
    assert_eq!(
        settings.profile_defaults_has_tab_title(),
        copy.profile_defaults_has_tab_title()
    );
    assert_ne!(
        settings.profile_defaults_tab_title(),
        copy.profile_defaults_tab_title()
    );

    assert!(!settings.profile_has_snap_on_input(0));
    assert!(!copy.profile_has_snap_on_input(0));
    assert_eq!(settings.profile_snap_on_input(0), Some(true));
    assert!(copy.set_profile_snap_on_input(0, false));
    assert!(!settings.profile_has_snap_on_input(0));
    assert!(copy.profile_has_snap_on_input(0));
    assert_eq!(copy.profile_snap_on_input(0), Some(false));

    let empty_defaults = r#"{
        "defaultProfile": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
        "profiles": {
            "defaults": {},
            "list": [
                {
                    "guid": "{61c54bbd-2222-5271-96e7-009a87ff44bf}",
                    "name": "PowerShell"
                }
            ]
        }
    }"#;
    let missing_defaults = r#"{
        "defaultProfile": "{61c54bbd-1111-5271-96e7-009a87ff44bf}",
        "profiles": [
            {
                "guid": "{61c54bbd-2222-5271-96e7-009a87ff44bf}",
                "name": "PowerShell"
            }
        ]
    }"#;

    for json in [empty_defaults, missing_defaults] {
        let settings = CloneableCascadiaSettings::from_json(json).unwrap();
        let copy = settings.deep_copy();
        assert!(settings.has_profile_defaults());
        assert!(copy.has_profile_defaults());
        assert_eq!(settings.active_profile_count(), 1);
        assert_eq!(settings.active_profile_count(), copy.active_profile_count());
        assert_eq!(settings.profile_parent_count(0), Some(1));
        assert_eq!(
            settings.profile_parent_count(0),
            copy.profile_parent_count(0)
        );
    }
}
