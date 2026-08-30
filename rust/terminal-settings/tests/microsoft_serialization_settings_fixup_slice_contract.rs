use terminal_settings::{
    serialization::SettingsDocument,
    settings_fixup::{effective_profile_commandline, fixup_user_settings},
};

#[test]
fn microsoft_serialization_fixup_user_settings_detects_changes_contract() {
    let clean_settings = r#"
    {
        "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
        "profiles": [
            {
                "name": "profile0",
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                "commandline": "cmd.exe"
            }
        ]
    }
    "#;

    let mut first = SettingsDocument::from_json(clean_settings).expect("Microsoft vector is valid");
    assert!(
        !fixup_user_settings(&mut first).expect("custom profile needs no commandline patch"),
        "a non-builtin profile with cmd.exe must not request a product fixup"
    );

    // CascadiaSettings::ToJson projects legacy profiles to the modern list
    // shape. Once serialized into that stable shape, reloading must not request
    // another product fixup.
    first
        .canonicalize_legacy_profiles()
        .expect("serialization projection succeeds");
    let stable = first.to_json_value().clone();
    assert!(
        !fixup_user_settings(&mut first).expect("stable settings remain clean"),
        "a clean round-trip must not require another fixup"
    );
    assert_eq!(first.to_json_value(), &stable);
}

#[test]
fn microsoft_serialization_fixup_commandline_patching_contract() {
    let cmd_settings = r#"
    {
        "defaultProfile": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
        "profiles": [
            {
                "name": "Command Prompt",
                "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
                "commandline": "cmd.exe"
            }
        ]
    }
    "#;
    let mut cmd = SettingsDocument::from_json(cmd_settings).expect("CMD vector is valid");
    assert!(fixup_user_settings(&mut cmd).expect("CMD patch succeeds"));
    assert_eq!(
        effective_profile_commandline(&cmd, 0)
            .expect("CMD profile is valid")
            .as_deref(),
        Some("%SystemRoot%\\System32\\cmd.exe")
    );

    let powershell_settings = r#"
    {
        "defaultProfile": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
        "profiles": [
            {
                "name": "Windows PowerShell",
                "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
                "commandline": "powershell.exe"
            }
        ]
    }
    "#;
    let mut powershell =
        SettingsDocument::from_json(powershell_settings).expect("PowerShell vector is valid");
    assert!(fixup_user_settings(&mut powershell).expect("PowerShell patch succeeds"));
    assert_eq!(
        effective_profile_commandline(&powershell, 0)
            .expect("PowerShell profile is valid")
            .as_deref(),
        Some("%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe")
    );

    let inherited_cmd_settings = r#"
    {
        "defaultProfile": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
        "profiles": [
            {
                "name": "Command Prompt",
                "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}"
            }
        ]
    }
    "#;
    let mut inherited_cmd =
        SettingsDocument::from_json(inherited_cmd_settings).expect("inherited CMD vector is valid");
    assert!(
        !fixup_user_settings(&mut inherited_cmd).expect("no materialization is required"),
        "an inherited inbox commandline must not be written into user settings"
    );
    assert_eq!(
        effective_profile_commandline(&inherited_cmd, 0)
            .expect("inherited CMD profile is valid")
            .as_deref(),
        Some("%SystemRoot%\\System32\\cmd.exe")
    );

    let custom_cmd_settings = r#"
    {
        "defaultProfile": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
        "profiles": [
            {
                "name": "My Custom CMD",
                "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}",
                "commandline": "cmd.exe"
            }
        ]
    }
    "#;
    let mut custom =
        SettingsDocument::from_json(custom_cmd_settings).expect("custom CMD vector is valid");
    assert!(
        !fixup_user_settings(&mut custom).expect("custom profile remains untouched"),
        "custom profiles must not receive built-in commandline patches"
    );
    assert_eq!(
        effective_profile_commandline(&custom, 0)
            .expect("custom profile is valid")
            .as_deref(),
        Some("cmd.exe")
    );
}
