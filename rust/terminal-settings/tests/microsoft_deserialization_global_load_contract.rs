use terminal_settings::{
    cascadia_settings::CascadiaSettingsDocument, deserialization_profiles::DeserializedProfiles,
};

#[test]
fn microsoft_deserialization_clamps_startup_columns_and_rows_contract() {
    let user = r#"{
        "initialCols": 1000000,
        "initialRows": -1000000,
        "profiles": [
            { "name": "profile0" }
        ]
    }"#;

    let settings = DeserializedProfiles::from_user_and_inbox(user, "{}").unwrap();
    assert_eq!(settings.global_i32("initialCols"), Some(999));
    assert_eq!(settings.global_i32("initialRows"), Some(1));
}

#[test]
fn microsoft_deserialization_accepts_trailing_commas_contract() {
    let user = r#"{
        "profiles": [
            { "name": "profile0" }
        ],
    }"#;

    let settings = DeserializedProfiles::from_user_and_inbox(user, "{}").unwrap();
    assert_eq!(settings.profile_count(), 1);
    assert_eq!(settings.profile_name(0), Some("profile0"));
}

#[test]
fn microsoft_deserialization_load_defaults_populates_active_profiles_contract() {
    // Exact built-in profile identities/names from TerminalSettingsModel/defaults.json.
    // The native TestValidDefaults contract observes only total/active profile counts.
    let defaults = r#"{
        "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
        "profiles": [
            {
                "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                "name": "Windows PowerShell",
                "hidden": false
            },
            {
                "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
                "name": "Command Prompt",
                "hidden": false
            }
        ]
    }"#;

    let settings = DeserializedProfiles::from_user_and_inbox("{}", defaults).unwrap();
    assert_eq!(settings.profile_count(), 2);
    assert_eq!(settings.active_profile_count(), settings.profile_count());
    assert_eq!(settings.active_profile_name(0), Some("Windows PowerShell"));
    assert_eq!(settings.active_profile_name(1), Some("Command Prompt"));
}

#[test]
fn microsoft_deserialization_migrates_reload_environment_variables_contract() {
    let legacy = r#"{
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
    }"#;

    let settings = CascadiaSettingsDocument::from_json(legacy).unwrap();
    assert!(settings.fixups_applied_during_load());
    assert_eq!(
        settings.profile_default_bool("compatibility.reloadEnvironmentVariables"),
        Some(false)
    );
    assert!(
        settings
            .to_json_value()
            .as_object()
            .unwrap()
            .get("compatibility.reloadEnvironmentVariables")
            .is_none()
    );
}
