use terminal_settings::deserialization_fragments::FragmentSettings;

const INBOX: &str = r#"{
    "profiles": [
        {
            "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "name": "Windows PowerShell"
        },
        {
            "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
            "name": "Command Prompt"
        }
    ],
    "schemes": [
        { "name": "Campbell" },
        { "name": "Campbell Powershell" },
        { "name": "One Half Dark" }
    ]
}"#;

#[test]
fn microsoft_deserialization_load_fragments_with_multiple_updates_contract() {
    let fragment = r#"{
        "profiles": [
            {
                "updates": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                "name": "NewName"
            },
            {
                "updates": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
                "cursorShape": "filledBox"
            },
            {
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                "commandline": "cmd.exe"
            }
        ]
    }"#;

    let mut settings = FragmentSettings::from_user_and_inbox("{}", INBOX).unwrap();
    settings.apply_fragment(fragment).unwrap();

    assert!(!settings.duplicate_profile());
    assert_eq!(settings.profile_count(), 3);
    assert_eq!(settings.profile_name(0), Some("NewName"));
}

#[test]
fn microsoft_deserialization_fragment_action_simple_contract() {
    let fragment = r#"{
        "actions": [
            {
                "command": { "action": "addMark" },
                "name": "Test Action",
                "id": "Test.FragmentAction"
            }
        ]
    }"#;

    let mut settings = FragmentSettings::from_user_and_inbox("{}", INBOX).unwrap();
    settings.apply_fragment(fragment).unwrap();
    assert!(settings.runtime_has_action_named("Test Action"));
}

#[test]
fn microsoft_deserialization_fragment_action_no_keys_contract() {
    let fragment = r#"{
        "actions": [
            {
                "command": { "action": "addMark" },
                "keys": "ctrl+f",
                "id": "Test.FragmentAction",
                "name": "Test Action"
            }
        ]
    }"#;

    let mut settings = FragmentSettings::from_user_and_inbox("{}", INBOX).unwrap();
    settings.apply_fragment(fragment).unwrap();
    assert!(settings.runtime_has_action_named("Test Action"));
    assert!(!settings.runtime_has_keybinding("ctrl+f"));
}

#[test]
fn microsoft_deserialization_fragment_action_nested_contract() {
    let fragment = r#"{
        "actions": [
            {
                "name": "nested command",
                "commands": [
                    {
                        "name": "child1",
                        "command": { "action": "newTab", "commandline": "ssh me@first.com" }
                    },
                    {
                        "name": "child2",
                        "command": { "action": "newTab", "commandline": "ssh me@second.com" }
                    }
                ]
            }
        ]
    }"#;

    let mut settings = FragmentSettings::from_user_and_inbox("{}", INBOX).unwrap();
    settings.apply_fragment(fragment).unwrap();
    assert!(settings.runtime_has_action_named("nested command"));
    assert!(settings.runtime_action_has_nested_commands("nested command"));
}

#[test]
fn microsoft_deserialization_fragment_action_nested_no_name_contract() {
    let fragment = r#"{
        "actions": [
            {
                "commands": [
                    {
                        "name": "child1",
                        "command": { "action": "newTab", "commandline": "ssh me@first.com" }
                    },
                    {
                        "name": "child2",
                        "command": { "action": "newTab", "commandline": "ssh me@second.com" }
                    }
                ]
            }
        ]
    }"#;

    let mut settings = FragmentSettings::from_user_and_inbox("{}", INBOX).unwrap();
    settings.apply_fragment(fragment).unwrap();
    assert_eq!(settings.warning_count(), 0);
}

#[test]
fn microsoft_deserialization_fragment_action_iterable_contract() {
    let fragment = r#"{
        "actions": [
            {
                "name": "nested",
                "commands": [
                    {
                        "iterateOn": "schemes",
                        "name": "${scheme.name}",
                        "command": { "action": "setColorScheme", "colorScheme": "${scheme.name}" }
                    }
                ]
            }
        ]
    }"#;

    let mut settings = FragmentSettings::from_user_and_inbox("{}", INBOX).unwrap();
    settings.apply_fragment(fragment).unwrap();
    assert!(settings.runtime_has_action_named("nested"));
    assert!(settings.runtime_action_has_nested_commands("nested"));
    assert_eq!(
        settings.runtime_nested_command_count("nested"),
        Some(settings.color_scheme_count())
    );
}

#[test]
fn microsoft_deserialization_fragment_action_roundtrip_contract() {
    let fragment = r#"{
        "actions": [
            {
                "command": { "action": "addMark" },
                "name": "Test Action",
                "id": "Test.FragmentAction"
            }
        ]
    }"#;

    let mut settings = FragmentSettings::from_user_and_inbox("{}", INBOX).unwrap();
    settings.apply_fragment(fragment).unwrap();
    assert!(settings.runtime_has_action_named("Test Action"));

    // CascadiaSettings::ToJson serializes user-owned state, not fragment runtime
    // contributions. Reloading that serialization without the fragment therefore
    // must not resurrect the fragment action.
    assert!(!settings.roundtrip_has_action_named("Test Action"));
}
