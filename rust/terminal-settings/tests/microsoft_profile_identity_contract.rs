use terminal_settings::profile_identity::{ProfileIdentityGuid, ProfileIdentitySettings};

#[test]
fn microsoft_test_gen_guids_for_profiles_distinguishes_source_identity() {
    let inbox = r#"{
        "profiles": [
            {
                "name": "profile0",
                "source": "Terminal.App.UnitTest.0"
            },
            {
                "name": "profile1"
            }
        ]
    }"#;
    let user = r#"{
        "profiles": [
            {
                "name": "profile0",
                "source": "Terminal.App.UnitTest.0"
            },
            {
                "name": "profile0"
            }
        ]
    }"#;

    let settings = ProfileIdentitySettings::from_layered_legacy_arrays(user, inbox).unwrap();
    let profiles = settings.profiles();

    assert_eq!(profiles.len(), 3);
    assert_eq!(profiles[0].name(), Some("profile0"));
    assert_eq!(
        profiles[0].guid(),
        ProfileIdentityGuid::Generated([
            0x52, 0xb9, 0x37, 0x2a, 0xc1, 0xb1, 0x57, 0xeb, 0xa9, 0xce, 0xe2, 0xcd, 0xc8, 0xd5,
            0x2c, 0xeb,
        ])
    );
    assert_eq!(profiles[0].source(), Some("Terminal.App.UnitTest.0"));

    assert_eq!(profiles[1].name(), Some("profile1"));
    assert_eq!(
        profiles[1].guid(),
        ProfileIdentityGuid::Generated([
            0x87, 0x62, 0xbf, 0x49, 0x5b, 0x86, 0x58, 0x79, 0x94, 0x28, 0xae, 0xb4, 0xed, 0x7b,
            0xa5, 0x45,
        ])
    );
    assert_eq!(profiles[1].source(), None);

    assert_eq!(profiles[2].name(), Some("profile0"));
    assert_eq!(
        profiles[2].guid(),
        ProfileIdentityGuid::Generated([
            0x69, 0x09, 0x55, 0xe2, 0xdb, 0xc2, 0x50, 0x9c, 0xb3, 0x6d, 0xac, 0x46, 0x6f, 0xdd,
            0xa6, 0x56,
        ])
    );
    assert_eq!(profiles[2].source(), None);
    assert_ne!(profiles[0].guid(), profiles[2].guid());
}

#[test]
fn microsoft_profile_defaults_prohibited_settings_do_not_inherit_identity_or_commandline() {
    let user = r#"{
        "profiles": {
            "defaults": {
                "guid": "{00000000-0000-0000-0000-000000000000}",
                "name": "Default Profile Name",
                "source": "Default Profile Source",
                "commandline": "foo.exe"
            },
            "list": [
                {
                    "name": "PowerShell",
                    "commandline": "powershell.exe",
                    "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}"
                },
                {
                    "name": "Profile with just a name"
                },
                {
                    "guid": "{a0776706-1fa6-4439-b46c-287a65c084d5}"
                }
            ]
        }
    }"#;

    let settings =
        ProfileIdentitySettings::from_modern_json_with_prohibited_defaults(user).unwrap();
    assert!(!settings.defaults_has_guid());
    assert!(!settings.defaults_has_name());
    assert!(!settings.defaults_has_source());
    assert!(!settings.defaults_has_commandline());

    let profiles = settings.profiles();
    assert_eq!(profiles.len(), 3);

    assert_eq!(profiles[0].name(), Some("PowerShell"));
    assert_eq!(
        profiles[0].commandline(),
        Some("%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe")
    );
    assert_eq!(profiles[0].source(), None);
    assert!(!profiles[0].guid().is_zero());

    assert_eq!(profiles[1].name(), Some("Profile with just a name"));
    assert_eq!(
        profiles[1].guid(),
        ProfileIdentityGuid::Generated([
            0x21, 0x6a, 0x97, 0x2d, 0x23, 0x13, 0x53, 0xd3, 0x9e, 0x56, 0x79, 0x53, 0xdc, 0x73,
            0xbc, 0x61,
        ])
    );
    assert_eq!(profiles[1].source(), None);
    assert_ne!(profiles[1].commandline(), Some("foo.exe"));

    assert_ne!(profiles[2].name(), Some("Default Profile Name"));
    assert!(!profiles[2].guid().is_zero());
    assert_eq!(profiles[2].source(), None);
    assert_ne!(profiles[2].commandline(), Some("foo.exe"));
}
