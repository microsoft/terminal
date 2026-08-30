use terminal_settings::deserialization_profiles::{
    DeserializationProfileError, DeserializationProfileWarning, DeserializedProfiles,
};

const EMPTY_INBOX: &str = r"{}";
const DEFAULT_PROFILES: &str = r#"{
    "profiles": [
        {
            "name": "Windows PowerShell",
            "guid": "{61c54bbd-c2c6-5271-96e7-009a87ff44bf}",
            "commandline": "powershell.exe"
        },
        {
            "name": "Command Prompt",
            "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
            "commandline": "cmd.exe"
        }
    ]
}"#;

#[test]
fn microsoft_deserialization_validate_profiles_exist_contract() {
    let good = r#"{
        "profiles": [ { "name": "profile0" } ]
    }"#;
    assert_eq!(
        DeserializedProfiles::from_user_and_inbox(good, EMPTY_INBOX)
            .expect("one profile is a valid settings collection")
            .profile_count(),
        1
    );

    let missing = r#"{
        "defaultProfile": "{6239a42c-1de4-49a3-80bd-e8fdd045185c}"
    }"#;
    assert_eq!(
        DeserializedProfiles::from_user_and_inbox(missing, EMPTY_INBOX),
        Err(DeserializationProfileError::NoProfiles)
    );

    let empty = r#"{ "profiles": [] }"#;
    assert_eq!(
        DeserializedProfiles::from_user_and_inbox(empty, EMPTY_INBOX),
        Err(DeserializationProfileError::NoProfiles)
    );
}

#[test]
fn microsoft_deserialization_validate_default_profile_exists_contract() {
    let good_guid = r#"{
        "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}" },
            { "name": "profile0", "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}" }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(good_guid, EMPTY_INBOX)
        .expect("Microsoft good default GUID vector parses");
    assert!(settings.warnings().is_empty());
    assert_eq!(settings.default_profile_index(), 0);

    let bad = r#"{
        "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}" },
            { "name": "profile1", "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}" }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(bad, EMPTY_INBOX)
        .expect("missing default is a warning, not a parse failure");
    assert_eq!(
        settings.warnings(),
        &[DeserializationProfileWarning::MissingDefaultProfile]
    );
    assert_eq!(settings.default_profile_index(), 0);

    let by_name = r#"{
        "defaultProfile": "profile1",
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}" },
            { "name": "profile1", "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}" }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(by_name, EMPTY_INBOX)
        .expect("defaultProfile may select by profile name");
    assert!(settings.warnings().is_empty());
    assert_eq!(settings.default_profile_index(), 1);
}

#[test]
fn microsoft_deserialization_validate_duplicate_profiles_contract() {
    let input = r#"{
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}" },
            { "name": "profile1", "guid": "{6239a42c-5555-49a3-80bd-e8fdd045185c}" },
            { "name": "profile2", "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}" },
            { "name": "profile3", "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}" },
            { "name": "profile4", "guid": "{6239a42c-6666-49a3-80bd-e8fdd045185c}" },
            { "name": "profile5", "guid": "{6239a42c-5555-49a3-80bd-e8fdd045185c}" },
            { "name": "profile6", "guid": "{6239a42c-7777-49a3-80bd-e8fdd045185c}" }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(input, EMPTY_INBOX)
        .expect("duplicates are recovered with a warning");
    assert_eq!(
        settings.warnings(),
        &[DeserializationProfileWarning::DuplicateProfile]
    );
    assert_eq!(settings.profile_count(), 4);
    assert_eq!(settings.profile_name(0), Some("profile0"));
    assert_eq!(settings.profile_name(1), Some("profile1"));
    assert_eq!(settings.profile_name(2), Some("profile4"));
    assert_eq!(settings.profile_name(3), Some("profile6"));
}

#[test]
fn microsoft_deserialization_validate_many_warnings_contract() {
    let input = r#"{
        "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}" },
            { "name": "profile1", "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}" },
            { "name": "profile2", "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}" },
            { "name": "profile3", "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}" },
            { "name": "profile4", "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}" }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(input, EMPTY_INBOX)
        .expect("multiple recoverable validation warnings are accumulated");
    assert_eq!(
        settings.warnings(),
        &[
            DeserializationProfileWarning::DuplicateProfile,
            DeserializationProfileWarning::MissingDefaultProfile,
        ]
    );
    assert_eq!(settings.profile_count(), 3);
    assert_eq!(settings.default_profile_index(), 0);
}

#[test]
fn microsoft_deserialization_layer_global_properties_contract() {
    let inbox = r#"{
        "alwaysShowTabs": true,
        "initialCols": 120,
        "initialRows": 30
    }"#;
    let user = r#"{
        "showTabsInTitlebar": false,
        "initialCols": 240,
        "initialRows": 60,
        "profiles": [
            { "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}" }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(user, inbox)
        .expect("global properties layer with user precedence");
    assert_eq!(settings.global_bool("alwaysShowTabs"), Some(true));
    assert_eq!(settings.global_i32("initialCols"), Some(240));
    assert_eq!(settings.global_i32("initialRows"), Some(60));
    assert_eq!(settings.global_bool("showTabsInTitlebar"), Some(false));
}

#[test]
fn microsoft_deserialization_validate_profile_ordering_contract() {
    let inbox = r#"{
        "profiles": [
            { "name": "profile2", "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}" },
            { "name": "profile3", "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}" }
        ]
    }"#;
    let reversed = r#"{
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}" },
            { "name": "profile1", "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}" }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(reversed, inbox)
        .expect("user ordering wins for matched inbox profiles");
    assert_eq!(settings.profile_count(), 2);
    assert_eq!(settings.profile_name(0), Some("profile0"));
    assert_eq!(settings.profile_name(1), Some("profile1"));

    let user_first = r#"{
        "profiles": [
            { "name": "profile4", "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}" },
            { "name": "profile5", "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}" }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(user_first, inbox)
        .expect("unmatched inbox profiles append after user ordering");
    assert_eq!(settings.profile_count(), 3);
    assert_eq!(settings.profile_name(0), Some("profile4"));
    assert_eq!(settings.profile_name(1), Some("profile5"));
    assert_eq!(settings.profile_name(2), Some("profile2"));
}

#[test]
fn microsoft_deserialization_validate_hide_profiles_contract() {
    let inbox = r#"{
        "profiles": [
            { "name": "profile2", "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}" },
            { "name": "profile3", "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}" }
        ]
    }"#;
    let first = r#"{
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}", "hidden": true },
            { "name": "profile1", "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}" }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(first, inbox)
        .expect("one hidden matched profile remains in AllProfiles only");
    assert_eq!(settings.profile_count(), 2);
    assert_eq!(settings.active_profile_count(), 1);
    assert_eq!(settings.active_profile_name(0), Some("profile1"));

    let second = r#"{
        "profiles": [
            { "name": "profile4", "guid": "{6239a42c-0000-49a3-80bd-e8fdd045185c}", "hidden": true },
            { "name": "profile5", "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}" },
            { "name": "profile6", "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}", "hidden": true }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(second, inbox)
        .expect("active profile projection filters hidden entries after ordering");
    assert_eq!(settings.profile_count(), 4);
    assert_eq!(settings.active_profile_count(), 2);
    assert_eq!(settings.active_profile_name(0), Some("profile5"));
    assert_eq!(settings.active_profile_name(1), Some("profile2"));
}

#[test]
fn microsoft_deserialization_reorder_with_null_guids_contract() {
    let user = r#"{
        "defaultProfile": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
        "profiles": [
            { "name": "profile0", "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}" },
            { "name": "profile1" },
            { "name": "cmdFromUserSettings", "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}" }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(user, DEFAULT_PROFILES)
        .expect("name-only user profiles receive effective identity without breaking reorder");
    assert!(settings.warnings().is_empty());
    assert_eq!(settings.profile_count(), 4);
    for index in 0..4 {
        assert!(settings.profile_has_effective_guid(index));
    }
    assert_eq!(settings.profile_name(0), Some("profile0"));
    assert_eq!(settings.profile_name(1), Some("profile1"));
    assert_eq!(settings.profile_name(2), Some("cmdFromUserSettings"));
    assert_eq!(settings.profile_name(3), Some("Windows PowerShell"));
}

#[test]
fn microsoft_deserialization_reordering_without_guid_contract() {
    let user = r##"{
        "defaultProfile": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
        "profiles": [
            {
                "guid": "{0caa0dad-35be-5f56-a8ff-afceeeaa6101}",
                "commandline": "cmd.exe",
                "background": "#8A00FF"
            },
            {
                "name": "ThisProfileShouldNotCrash",
                "tabTitle": "Ubuntu",
                "commandline": "wsl.exe",
                "background": "#2C001E"
            },
            {
                "name": "Ubuntu",
                "guid": "{2C4DE342-38B7-51CF-B940-2309A097F518}",
                "background": "#2C001E"
            }
        ]
    }"##;
    let settings = DeserializedProfiles::from_user_and_inbox(user, DEFAULT_PROFILES)
        .expect("GUID-less profile ordering remains stable and non-crashing");
    assert!(settings.warnings().is_empty());
    assert_eq!(settings.profile_count(), 4);
    for index in 0..4 {
        assert!(settings.profile_has_effective_guid(index));
    }
    assert_eq!(settings.profile_name(0), Some("Command Prompt"));
    assert_eq!(settings.profile_name(1), Some("ThisProfileShouldNotCrash"));
    assert_eq!(settings.profile_name(2), Some("Ubuntu"));
    assert_eq!(settings.profile_name(3), Some("Windows PowerShell"));
}

#[test]
fn microsoft_deserialization_layering_name_only_profiles_contract() {
    let user = r#"{
        "defaultProfile": "{00000000-0000-5f56-a8ff-afceeeaa6101}",
        "profiles": [
            { "guid": "{00000000-0000-5f56-a8ff-afceeeaa6101}", "name": "ThisProfileIsGood" },
            { "name": "ThisProfileShouldNotLayer" },
            { "name": "NeitherShouldThisOne" }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(user, DEFAULT_PROFILES)
        .expect("distinct name-only profiles preserve independent generated identities");
    assert_eq!(settings.profile_count(), 5);
    assert_eq!(settings.profile_name(0), Some("ThisProfileIsGood"));
    assert_eq!(settings.profile_name(1), Some("ThisProfileShouldNotLayer"));
    assert_eq!(settings.profile_name(2), Some("NeitherShouldThisOne"));
    assert_eq!(settings.profile_name(3), Some("Windows PowerShell"));
    assert_eq!(settings.profile_name(4), Some("Command Prompt"));
}

#[test]
fn microsoft_deserialization_hide_all_profiles_contract() {
    let good = r#"{
        "profiles": [
            { "name": "profile0", "hidden": false },
            { "name": "profile1", "hidden": true }
        ]
    }"#;
    let settings = DeserializedProfiles::from_user_and_inbox(good, EMPTY_INBOX)
        .expect("at least one active profile is valid");
    assert_eq!(settings.profile_count(), 2);
    assert_eq!(settings.active_profile_count(), 1);

    let all_hidden = r#"{
        "profiles": [
            { "name": "profile0", "hidden": true },
            { "name": "profile1", "hidden": true }
        ]
    }"#;
    assert_eq!(
        DeserializedProfiles::from_user_and_inbox(all_hidden, EMPTY_INBOX),
        Err(DeserializationProfileError::AllProfilesHidden)
    );
}
