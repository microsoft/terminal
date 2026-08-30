use terminal_settings::{
    color_scheme::Color,
    profile::{Profile, ProfileGuid, ProfileParseError},
};

#[test]
fn microsoft_profile_generates_guid_accepts_only_braced_guid_format() {
    let without_guid = Profile::from_json(r#"{"name":"profile0"}"#).unwrap();
    let second_without_guid = Profile::from_json(r#"{"name":"profile1"}"#).unwrap();
    let null_guid = Profile::from_json(r#"{"name":"profile2","guid":null}"#).unwrap();
    assert!(!without_guid.has_guid());
    assert!(!second_without_guid.has_guid());
    assert!(!null_guid.has_guid());

    for invalid in [
        r#"{"name":"profile4","guid":"{6239A42C1DE449A380BDE8FDD045185C}"}"#,
        r#"{"name":"profile4","guid":"6239a42c-1de4-49a3-80bd-e8fdd045185c"}"#,
        r#"{"name":"profile4","guid":"(6239a42c-1de4-49a3-80bd-e8fdd045185c)\\"}"#,
    ] {
        assert_eq!(
            Profile::from_json(invalid),
            Err(ProfileParseError::InvalidGuid)
        );
    }

    let zero = Profile::from_json(
        r#"{"name":"profile3","guid":"{00000000-0000-0000-0000-000000000000}"}"#,
    )
    .unwrap();
    assert!(zero.has_guid());
    assert!(zero.guid().unwrap().is_zero());

    let lower = Profile::from_json(
        r#"{"name":"profile4","guid":"{6239a42c-1de4-49a3-80bd-e8fdd045185c}"}"#,
    )
    .unwrap();
    let upper = Profile::from_json(
        r#"{"name":"profile4","guid":"{6239A42C-1DE4-49A3-80BD-E8FDD045185C}"}"#,
    )
    .unwrap();
    let expected = ProfileGuid::parse("{6239a42c-1de4-49a3-80bd-e8fdd045185c}").unwrap();
    assert_eq!(lower.guid(), Some(expected));
    assert_eq!(upper.guid(), Some(expected));
    assert_eq!(
        expected.to_string(),
        "{6239a42c-1de4-49a3-80bd-e8fdd045185c}"
    );
}

#[test]
fn microsoft_layer_profile_properties_preserves_inherited_appearance_and_directory() {
    let profile0 = Profile::from_json(
        r##"{
            "name": "profile0",
            "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
            "foreground": "#000000",
            "background": "#010101",
            "selectionBackground": "#010101"
        }"##,
    )
    .unwrap();
    assert_eq!(profile0.foreground(), Some(Color::rgb(0, 0, 0)));
    assert_eq!(profile0.background(), Some(Color::rgb(1, 1, 1)));
    assert_eq!(profile0.selection_background(), Some(Color::rgb(1, 1, 1)));
    assert_eq!(profile0.name(), Some("profile0"));
    assert!(profile0.starting_directory().is_empty());

    let mut profile1 = profile0.create_child();
    profile1
        .layer_json(
            r##"{
                "name": "profile1",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                "foreground": "#020202",
                "startingDirectory": "C:/"
            }"##,
        )
        .unwrap();
    assert_eq!(profile1.foreground(), Some(Color::rgb(2, 2, 2)));
    assert_eq!(profile1.background(), Some(Color::rgb(1, 1, 1)));
    assert_eq!(profile1.selection_background(), Some(Color::rgb(1, 1, 1)));
    assert_eq!(profile1.name(), Some("profile1"));
    assert_eq!(profile1.starting_directory(), "C:/");

    let mut profile2 = profile1.create_child();
    profile2
        .layer_json(
            r##"{
                "name": "profile2",
                "guid": "{6239a42c-1111-49a3-80bd-e8fdd045185c}",
                "foreground": "#030303",
                "selectionBackground": "#020202"
            }"##,
        )
        .unwrap();
    assert_eq!(profile2.foreground(), Some(Color::rgb(3, 3, 3)));
    assert_eq!(profile2.background(), Some(Color::rgb(1, 1, 1)));
    assert_eq!(profile2.selection_background(), Some(Color::rgb(2, 2, 2)));
    assert_eq!(profile2.name(), Some("profile2"));
    assert_eq!(profile2.starting_directory(), "C:/");
}
