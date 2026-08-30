use terminal_settings::{
    deserialization_profile_properties::{CloseOnExitMode, DeserializedProfilePropertySet},
    profile::ProfileGuid,
};

const EMPTY_INBOX: &str = r"{}";

#[test]
fn microsoft_deserialization_close_on_exit_parsing_contract() {
    let user = r#"{
        "profiles": [
            { "name": "profile0", "closeOnExit": "graceful" },
            { "name": "profile1", "closeOnExit": "always" },
            { "name": "profile2", "closeOnExit": "never" },
            { "name": "profile3", "closeOnExit": "automatic" },
            { "name": "profile4", "closeOnExit": null }
        ]
    }"#;
    let settings = DeserializedProfilePropertySet::from_user_and_inbox(user, EMPTY_INBOX)
        .expect("Microsoft closeOnExit string vectors parse");
    let profiles = settings.profiles();
    assert_eq!(profiles[0].close_on_exit(), CloseOnExitMode::Graceful);
    assert_eq!(profiles[1].close_on_exit(), CloseOnExitMode::Always);
    assert_eq!(profiles[2].close_on_exit(), CloseOnExitMode::Never);
    assert_eq!(profiles[3].close_on_exit(), CloseOnExitMode::Automatic);
    assert_eq!(profiles[4].close_on_exit(), CloseOnExitMode::Automatic);
}

#[test]
fn microsoft_deserialization_close_on_exit_compatibility_shim_contract() {
    let user = r#"{
        "profiles": [
            { "name": "profile0", "closeOnExit": true },
            { "name": "profile1", "closeOnExit": false }
        ]
    }"#;
    let settings = DeserializedProfilePropertySet::from_user_and_inbox(user, EMPTY_INBOX)
        .expect("legacy boolean closeOnExit values remain compatible");
    assert_eq!(
        settings.profiles()[0].close_on_exit(),
        CloseOnExitMode::Graceful
    );
    assert_eq!(
        settings.profiles()[1].close_on_exit(),
        CloseOnExitMode::Never
    );
}

#[test]
fn microsoft_deserialization_layer_user_defaults_before_profiles_contract() {
    let user = r#"{
        "profiles": {
            "defaults": { "historySize": 1234 },
            "list": [
                {
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "name": "profile0",
                    "historySize": 2345
                },
                {
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "name": "profile1"
                }
            ]
        }
    }"#;
    let settings = DeserializedProfilePropertySet::from_user_and_inbox(user, EMPTY_INBOX)
        .expect("profiles.defaults layer before explicit profile properties");
    assert_eq!(settings.profiles()[0].history_size(), 2345);
    assert_eq!(settings.profiles()[1].history_size(), 1234);
}

#[test]
fn microsoft_deserialization_dont_layer_guid_from_user_defaults_contract() {
    let user = r#"{
        "profiles": {
            "defaults": {
                "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
            },
            "list": [
                {
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "name": "profile0"
                },
                { "name": "profile1" }
            ]
        }
    }"#;
    let settings = DeserializedProfilePropertySet::from_user_and_inbox(user, EMPTY_INBOX)
        .expect("identity fields are prohibited from profiles.defaults inheritance");
    let explicit = ProfileGuid::parse("{6239a42c-1111-49a3-80bd-e8fdd045185c}")
        .expect("Microsoft GUID vector is valid");
    assert_eq!(settings.profiles()[0].guid(), Some(explicit));
    assert_eq!(settings.profiles()[1].guid(), None);
}

#[test]
fn microsoft_deserialization_layer_user_defaults_on_dynamics_contract() {
    let inbox = r#"{
        "profiles": [
            {
                "name": "profile0",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                "source": "Terminal.App.UnitTest.0",
                "historySize": 1111
            },
            {
                "name": "profile1",
                "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                "source": "Terminal.App.UnitTest.1",
                "historySize": 2222
            },
            {
                "name": "profile2",
                "guid": "{6239a42c-4444-49a3-80bd-e8fdd045185c}",
                "source": "Terminal.App.UnitTest.1",
                "historySize": 4444
            }
        ]
    }"#;
    let user = r#"{
        "profiles": {
            "defaults": { "historySize": 1234 },
            "list": [
                {
                    "name": "profile0FromUserSettings",
                    "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                    "source": "Terminal.App.UnitTest.0"
                },
                {
                    "name": "profile1FromUserSettings",
                    "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}",
                    "source": "Terminal.App.UnitTest.1",
                    "historySize": 4444
                },
                {
                    "name": "profile2FromUserSettings",
                    "guid": "{6239a42c-3333-49a3-80bd-e8fdd045185c}",
                    "historySize": 5555
                }
            ]
        }
    }"#;
    let settings = DeserializedProfilePropertySet::from_user_and_inbox(user, inbox)
        .expect("user defaults layer above dynamic profiles and below explicit user values");
    let profiles = settings.profiles();
    assert_eq!(profiles.len(), 4);
    assert_eq!(profiles[0].name(), Some("profile0FromUserSettings"));
    assert_eq!(profiles[1].name(), Some("profile1FromUserSettings"));
    assert_eq!(profiles[2].name(), Some("profile2FromUserSettings"));
    assert_eq!(profiles[3].name(), Some("profile2"));
    assert_eq!(profiles[0].source(), Some("Terminal.App.UnitTest.0"));
    assert_eq!(profiles[1].source(), Some("Terminal.App.UnitTest.1"));
    assert_eq!(profiles[2].source(), None);
    assert_eq!(profiles[3].source(), Some("Terminal.App.UnitTest.1"));
    assert_eq!(profiles[0].history_size(), 1234);
    assert_eq!(profiles[1].history_size(), 4444);
    assert_eq!(profiles[2].history_size(), 5555);
    assert_eq!(profiles[3].history_size(), 1234);
}

#[test]
fn microsoft_deserialization_find_missing_profile_contract() {
    let user = r#"{
        "profiles": [
            {
                "name": "profile0",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}"
            },
            {
                "name": "profile1",
                "guid": "{6239a42c-2222-49a3-80bd-e8fdd045185c}"
            }
        ]
    }"#;
    let settings = DeserializedProfilePropertySet::from_user_and_inbox(user, EMPTY_INBOX)
        .expect("profile lookup vectors deserialize");
    let guid1 =
        ProfileGuid::parse("{6239a42c-1111-49a3-80bd-e8fdd045185c}").expect("GUID 1 is valid");
    let guid2 =
        ProfileGuid::parse("{6239a42c-2222-49a3-80bd-e8fdd045185c}").expect("GUID 2 is valid");
    let missing = ProfileGuid::parse("{6239a42c-3333-49a3-80bd-e8fdd045185c}")
        .expect("missing GUID vector is valid");
    assert_eq!(
        settings
            .profile_by_guid(guid1)
            .and_then(|profile| profile.name()),
        Some("profile0")
    );
    assert_eq!(
        settings
            .profile_by_guid(guid2)
            .and_then(|profile| profile.name()),
        Some("profile1")
    );
    assert!(settings.profile_by_guid(missing).is_none());
}
