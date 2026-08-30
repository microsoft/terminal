use terminal_settings::{
    deserialization_validation::{
        DeserializationValidation, DeserializationValidationWarning as Warning,
    },
    profile::ProfileGuid,
    profile_identity::ProfileIdentityGuid,
    profile_lookup::ProfileLookup,
};

const CAMPBELL_INBOX: &str = r#"{
    "schemes": [ { "name": "Campbell" } ]
}"#;

#[test]
fn microsoft_deserialization_invalid_color_scheme_name_contract() {
    let user = r#"{
        "profiles": [
            { "name": "profile0", "colorScheme": "Campbell" },
            { "name": "profile1", "colorScheme": "InvalidSchemeName" },
            { "name": "profile2" }
        ]
    }"#;

    let validation = DeserializationValidation::from_user_and_inbox(user, CAMPBELL_INBOX).unwrap();
    assert_eq!(
        validation.settings_warnings(),
        &[Warning::UnknownColorScheme]
    );
    assert_eq!(validation.profile_scheme(0), Some(("Campbell", "Campbell")));
    assert_eq!(validation.profile_scheme(1), Some(("Campbell", "Campbell")));
    assert_eq!(validation.profile_scheme(2), Some(("Campbell", "Campbell")));
}

#[test]
fn microsoft_deserialization_color_scheme_in_commands_contract() {
    let simple = r#"{
        "profiles": [ { "name": "profile0", "colorScheme": "Campbell" } ],
        "actions": [
            { "command": { "action": "setColorScheme", "colorScheme": "Campbell" } },
            { "command": { "action": "setColorScheme", "colorScheme": "invalidScheme" } }
        ]
    }"#;
    let nested = r#"{
        "profiles": [ { "name": "profile0", "colorScheme": "Campbell" } ],
        "actions": [
            { "command": { "action": "setColorScheme", "colorScheme": "Campbell" } },
            {
                "name": "parent",
                "commands": [
                    { "command": { "action": "setColorScheme", "colorScheme": "invalidScheme" } }
                ]
            }
        ]
    }"#;
    let nested_twice = r#"{
        "profiles": [ { "name": "profile0", "colorScheme": "Campbell" } ],
        "actions": [
            { "command": { "action": "setColorScheme", "colorScheme": "Campbell" } },
            {
                "name": "grandparent",
                "commands": [
                    {
                        "name": "parent",
                        "commands": [
                            { "command": { "action": "setColorScheme", "colorScheme": "invalidScheme" } }
                        ]
                    }
                ]
            }
        ]
    }"#;

    for settings in [simple, nested, nested_twice] {
        let validation =
            DeserializationValidation::from_user_and_inbox(settings, CAMPBELL_INBOX).unwrap();
        assert_eq!(
            validation.settings_warnings(),
            &[Warning::InvalidColorSchemeInCmd]
        );
    }
}

#[test]
fn microsoft_deserialization_helper_functions_contract() {
    let user = r#"{
        "defaultProfile": "{2C4DE342-38B7-51CF-B940-2309A097F518}",
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-5555-49a3-80bd-e8fdd045185c}" },
            { "name": "profile1", "guid": "{6239a42c-6666-49a3-80bd-e8fdd045185c}" },
            { "name": "ThisProfileShouldNotThrow" },
            { "name": "Ubuntu", "guid": "{2C4DE342-38B7-51CF-B940-2309A097F518}" }
        ]
    }"#;

    let lookup = ProfileLookup::from_legacy_user_json(user).unwrap();
    let guid0 = ProfileGuid::parse("{6239a42c-5555-49a3-80bd-e8fdd045185c}").unwrap();
    let guid1 = ProfileGuid::parse("{6239a42c-6666-49a3-80bd-e8fdd045185c}").unwrap();
    let guid2 = ProfileGuid::parse("{2C4DE342-38B7-51CF-B940-2309A097F518}").unwrap();
    let fake = ProfileGuid::parse("{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}").unwrap();

    assert_eq!(
        lookup.get_profile_by_name("profile0").unwrap().guid(),
        ProfileIdentityGuid::Explicit(guid0)
    );
    assert_eq!(
        lookup.get_profile_by_name("profile1").unwrap().guid(),
        ProfileIdentityGuid::Explicit(guid1)
    );
    assert_eq!(
        lookup.get_profile_by_name("Ubuntu").unwrap().guid(),
        ProfileIdentityGuid::Explicit(guid2)
    );
    assert_eq!(
        lookup
            .get_profile_by_name("ThisProfileShouldNotThrow")
            .unwrap()
            .guid(),
        ProfileIdentityGuid::Generated([
            0xbc, 0x44, 0x83, 0x9a, 0x2c, 0x70, 0x52, 0x13, 0x84, 0x1d, 0x37, 0xb7, 0x14, 0xda,
            0xbb, 0x4a,
        ])
    );
    assert!(lookup.get_profile_by_name("DoesNotExist").is_none());
    assert_eq!(
        lookup.find_explicit_profile(guid0).unwrap().name(),
        Some("profile0")
    );
    assert_eq!(
        lookup.find_explicit_profile(guid1).unwrap().name(),
        Some("profile1")
    );
    assert_eq!(
        lookup.find_explicit_profile(guid2).unwrap().name(),
        Some("Ubuntu")
    );
    assert!(lookup.find_explicit_profile(fake).is_none());
}

#[test]
fn microsoft_deserialization_keybindings_warnings_contract() {
    let bad_settings = r#"{
        "defaultProfile": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}" },
            { "name": "profile1", "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}" }
        ],
        "keybindings": [
            { "command": { "action": "splitPane", "split":"auto" }, "keys": [ "ctrl+alt+t", "ctrl+a" ] },
            { "command": { "action": "moveFocus" }, "keys": [ "ctrl+a" ] },
            { "command": { "action": "resizePane" }, "keys": [ "ctrl+b" ] },
            { "name": "invalid nested", "commands":[ { "name" : "hello" }, { "name" : "world" } ] }
        ]
    }"#;

    let validation =
        DeserializationValidation::from_user_and_inbox(bad_settings, CAMPBELL_INBOX).unwrap();
    assert_eq!(validation.key_map_count(), 2);
    assert_eq!(validation.action_map_count(), 1);
    assert_eq!(validation.name_map_count(), 1);
    assert!(!validation.action_is_bound_for_key("ctrl+a"));
    assert!(!validation.action_is_bound_for_key("ctrl+b"));
    assert_eq!(
        validation.keybinding_warnings(),
        &[
            Warning::TooManyKeysForChord,
            Warning::MissingRequiredParameter,
            Warning::MissingRequiredParameter,
            Warning::FailedToParseSubCommands,
        ]
    );
    assert_eq!(
        validation.settings_warnings(),
        &[
            Warning::AtLeastOneKeybindingWarning,
            Warning::TooManyKeysForChord,
            Warning::MissingRequiredParameter,
            Warning::MissingRequiredParameter,
            Warning::FailedToParseSubCommands,
        ]
    );
}

#[test]
fn microsoft_deserialization_execute_commandline_warning_contract() {
    let bad_settings = r#"{
        "defaultProfile": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}" },
            { "name": "profile1", "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}" }
        ],
        "keybindings": [
            { "name":null, "command": { "action": "wt" }, "keys": [ "ctrl+a" ] },
            { "name":null, "command": { "action": "wt", "commandline":"" }, "keys": [ "ctrl+b" ] },
            { "name":null, "command": { "action": "wt", "commandline":null }, "keys": [ "ctrl+c" ] }
        ]
    }"#;

    let validation =
        DeserializationValidation::from_user_and_inbox(bad_settings, CAMPBELL_INBOX).unwrap();
    assert_eq!(validation.key_map_count(), 3);
    assert!(!validation.action_is_bound_for_key("ctrl+a"));
    assert!(!validation.action_is_bound_for_key("ctrl+b"));
    assert!(!validation.action_is_bound_for_key("ctrl+c"));
    assert_eq!(
        validation.keybinding_warnings(),
        &[
            Warning::MissingRequiredParameter,
            Warning::MissingRequiredParameter,
            Warning::MissingRequiredParameter,
        ]
    );
    assert_eq!(
        validation.settings_warnings(),
        &[
            Warning::AtLeastOneKeybindingWarning,
            Warning::MissingRequiredParameter,
            Warning::MissingRequiredParameter,
            Warning::MissingRequiredParameter,
        ]
    );
}
