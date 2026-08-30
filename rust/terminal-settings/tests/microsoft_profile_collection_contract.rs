use terminal_settings::profile_collection::{ProfileCollection, SettingsLoadWarning};

#[test]
fn microsoft_layer_profiles_on_array_merges_by_guid_and_preserves_inbox_order() {
    let inbox = r#"{
        "profiles": [
            {
                "name": "profile0",
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
            },
            {
                "name": "profile1",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
            },
            {
                "name": "profile2",
                "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
            }
        ]
    }"#;
    let user = r#"{
        "profiles": [
            {
                "name": "profile3",
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}"
            },
            {
                "name": "profile4",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
            }
        ]
    }"#;

    let collection = ProfileCollection::from_layered_legacy_arrays(user, inbox).unwrap();
    let profiles = collection.profiles();

    assert_eq!(profiles.len(), 3);
    assert_eq!(profiles[0].name(), Some("profile3"));
    assert_eq!(profiles[1].name(), Some("profile4"));
    assert_eq!(profiles[2].name(), Some("profile2"));
}

#[test]
fn microsoft_correct_old_default_shell_paths_only_for_canonical_guids() {
    let user = r#"{
        "profiles": {
            "defaults": {
                "commandline": "pwsh.exe"
            },
            "list": [
                {
                    "name": "powershell 1",
                    "commandline": "powershell.exe",
                    "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}"
                },
                {
                    "name": "powershell 2",
                    "commandline": "powershell.exe",
                    "guid": "{61c54bbd-0000-5271-96e7-009a87ff44bf}"
                },
                {
                    "name": "cmd 1",
                    "commandline": "cmd.exe",
                    "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}"
                },
                {
                    "name": "cmd 2",
                    "commandline": "cmd.exe",
                    "guid": "{0caa0dad-0000-5f56-a8ff-afceeeaa6101}"
                }
            ]
        }
    }"#;

    let collection = ProfileCollection::from_user_json_with_legacy_shell_path_fixups(user).unwrap();
    let profiles = collection.profiles();

    assert_eq!(profiles.len(), 4);
    assert_eq!(profiles[0].name(), Some("powershell 1"));
    assert_eq!(profiles[1].name(), Some("powershell 2"));
    assert_eq!(profiles[2].name(), Some("cmd 1"));
    assert_eq!(profiles[3].name(), Some("cmd 2"));
    assert_eq!(
        profiles[0].commandline(),
        Some("%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe")
    );
    assert_eq!(profiles[1].commandline(), Some("powershell.exe"));
    assert_eq!(
        profiles[2].commandline(),
        Some("%SystemRoot%\\System32\\cmd.exe")
    );
    assert_eq!(profiles[3].commandline(), Some("cmd.exe"));
}

#[test]
fn microsoft_profile_environment_case_collision_emits_two_warnings() {
    let user = r#"{
        "profiles": [
            {
                "name": "profile0",
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                "environment": {
                    "FOO": "VALUE",
                    "Foo": "Value"
                }
            }
        ]
    }"#;

    let collection = ProfileCollection::from_user_json_with_profile_validation(user).unwrap();

    assert_eq!(collection.profiles().len(), 1);
    assert_eq!(
        collection.warnings(),
        [
            SettingsLoadWarning::UnknownColorScheme,
            SettingsLoadWarning::InvalidProfileEnvironmentVariables,
        ]
    );
}
